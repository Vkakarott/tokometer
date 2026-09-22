import Foundation

protocol NativeUsageProvider: Sendable {
    var id: ProviderID { get }
    var refreshInterval: TimeInterval { get }
    func fetch(now: Date) async throws -> ProviderSnapshot
}

extension NativeUsageProvider {
    var refreshInterval: TimeInterval { 60 }
}

enum ProviderFetchError: LocalizedError {
    case needsAuth(String)
    case unsupported(String)
    case unavailable(String)
    case rateLimited(retryAfter: TimeInterval)

    var errorDescription: String? {
        switch self {
        case let .needsAuth(message), let .unsupported(message), let .unavailable(message): message
        case let .rateLimited(retryAfter): "Atualização limitada; nova tentativa em \(Int(retryAfter / 60).formatted()) min"
        }
    }
}

struct CodexUsageProvider: NativeUsageProvider {
    let authFile: URL
    let endpoint: URL

    init(
        authFile: URL = FileManager.default.homeDirectoryForCurrentUser.appending(path: ".codex/auth.json"),
        endpoint: URL = URL(string: "https://chatgpt.com/backend-api/wham/usage")!
    ) {
        self.authFile = authFile
        self.endpoint = endpoint
    }

    var id: ProviderID { .codex }

    func fetch(now: Date) async throws -> ProviderSnapshot {
        let auth = try JSONDecoder().decode(CodexAuth.self, from: Data(contentsOf: authFile))
        guard let token = auth.tokens.accessToken, let account = auth.tokens.accountID else {
            throw ProviderFetchError.needsAuth("Entre novamente no Codex")
        }
        var request = URLRequest(url: endpoint)
        request.timeoutInterval = 10
        request.setValue("Bearer \(token)", forHTTPHeaderField: "Authorization")
        request.setValue(account, forHTTPHeaderField: "ChatGPT-Account-Id")
        request.setValue("codex_cli_rs", forHTTPHeaderField: "User-Agent")
        let (data, response) = try await URLSession.shared.data(for: request)
        guard let http = response as? HTTPURLResponse else {
            throw ProviderFetchError.unavailable("Codex não respondeu")
        }
        guard http.statusCode != 401, http.statusCode != 403 else {
            throw ProviderFetchError.needsAuth("Entre novamente no Codex")
        }
        guard http.statusCode == 200 else {
            throw ProviderFetchError.unavailable("Codex indisponível (HTTP \(http.statusCode))")
        }
        let usage = try JSONDecoder().decode(CodexUsage.self, from: data)
        let windows = [usage.rateLimit?.primaryWindow, usage.rateLimit?.secondaryWindow]
            .compactMap { $0 }
            .compactMap(UsageWindow.init(codex:))
        guard !windows.isEmpty else {
            throw ProviderFetchError.unsupported("O Codex não informou limites para esta conta")
        }
        return ProviderSnapshot(
            id: id,
            displayName: id.displayName,
            fidelity: .official,
            status: .ok,
            windows: windows,
            headlineID: windows.first?.id,
            hasData: true,
            observedAt: now
        )
    }
}

struct CursorUsageProvider: NativeUsageProvider {
    let stateFile: URL
    let endpoint: URL

    init(
        stateFile: URL = FileManager.default.homeDirectoryForCurrentUser.appending(path: "Library/Application Support/Cursor/User/globalStorage/state.vscdb"),
        endpoint: URL = URL(string: "https://cursor.com/api/usage-summary")!
    ) {
        self.stateFile = stateFile
        self.endpoint = endpoint
    }

    var id: ProviderID { .cursor }
    var refreshInterval: TimeInterval { 300 }

    func fetch(now: Date) async throws -> ProviderSnapshot {
        let token = try CursorStateReader.accessToken(from: stateFile)
        var request = URLRequest(url: endpoint)
        request.timeoutInterval = 10
        request.setValue("WorkosCursorSessionToken=::\(token)", forHTTPHeaderField: "Cookie")
        let (data, response) = try await URLSession.shared.data(for: request)
        guard let http = response as? HTTPURLResponse else { throw ProviderFetchError.unavailable("Cursor não respondeu") }
        guard http.statusCode != 401, http.statusCode != 403 else { throw ProviderFetchError.needsAuth("Entre novamente no Cursor") }
        guard http.statusCode == 200 else { throw ProviderFetchError.unavailable("Cursor indisponível (HTTP \(http.statusCode))") }
        let usage = try JSONDecoder().decode(CursorUsage.self, from: data)
        let windows = usage.windows
        guard !windows.isEmpty else {
            throw ProviderFetchError.unsupported((usage.isUnlimited ?? false) ? "O plano Cursor é ilimitado" : "O Cursor não informou consumo")
        }
        return ProviderSnapshot(id: id, displayName: id.displayName, fidelity: .official, status: .ok, windows: windows, headlineID: windows.first?.id, hasData: true, observedAt: now)
    }
}

@MainActor
final class NativePollingController {
    private let providers: [any NativeUsageProvider]
    private let store: UsageStore
    private let legacyFallback: LegacySnapshotProvider
    private var task: Task<Void, Never>?
    private var retryAfter: [ProviderID: Date] = [:]
    private var lastAttempt: [ProviderID: Date] = [:]

    init(providers: [any NativeUsageProvider], store: UsageStore, legacyFallback: LegacySnapshotProvider = LegacySnapshotProvider()) {
        self.providers = providers
        self.store = store
        self.legacyFallback = legacyFallback
    }

    func start() {
        guard task == nil else { return }
        task = Task { [weak self] in
            while !Task.isCancelled {
                await self?.refresh()
                try? await Task.sleep(for: .seconds(60))
            }
        }
    }

    func stop() {
        task?.cancel()
        task = nil
    }

    func refresh() async {
        guard !store.isDemoMode else { return }
        for provider in providers {
            let now = Date.now
            if let next = retryAfter[provider.id], next > now { continue }
            if let previous = lastAttempt[provider.id], now.timeIntervalSince(previous) < provider.refreshInterval { continue }
            lastAttempt[provider.id] = now
            do {
                store.apply(try await provider.fetch(now: .now))
            } catch {
                if case let ProviderFetchError.rateLimited(delay) = error {
                    retryAfter[provider.id] = .now.addingTimeInterval(delay)
                }
                if let fallback = try? await legacyFallback.load().canonicalSnapshots().first(where: { $0.id == provider.id && $0.hasData }) {
                    store.apply(fallback)
                    continue
                }
                store.preserveLastGood(for: provider.id, error: error)
            }
        }
    }
}

private struct CodexAuth: Decodable {
    let tokens: Tokens

    struct Tokens: Decodable {
        let accessToken: String?
        let accountID: String?

        enum CodingKeys: String, CodingKey {
            case accessToken = "access_token"
            case accountID = "account_id"
        }
    }
}

private struct CodexUsage: Decodable {
    let rateLimit: RateLimit?

    enum CodingKeys: String, CodingKey {
        case rateLimit = "rate_limit"
    }

    struct RateLimit: Decodable {
        let primaryWindow: Window?
        let secondaryWindow: Window?

        enum CodingKeys: String, CodingKey {
            case primaryWindow = "primary_window"
            case secondaryWindow = "secondary_window"
        }
    }

    struct Window: Decodable {
        let usedPercent: Double?
        let resetAt: TimeInterval?
        let limitWindowSeconds: Int?

        enum CodingKeys: String, CodingKey {
            case usedPercent = "used_percent"
            case resetAt = "reset_at"
            case limitWindowSeconds = "limit_window_seconds"
        }
    }
}

private extension UsageWindow {
    init?(codex window: CodexUsage.Window) {
        guard let percentage = window.usedPercent, let resetAt = window.resetAt else { return nil }
        let details: (String, String)
        switch window.limitWindowSeconds {
        case 18_000: details = ("five_hour", "5 horas")
        case 604_800: details = ("seven_day", "7 dias")
        default: return nil
        }
        self.init(id: details.0, label: details.1, usedFraction: min(max(percentage / 100, 0), 1), resetsAt: Date(timeIntervalSince1970: resetAt))
    }
}

private enum CursorStateReader {
    static func accessToken(from stateFile: URL) throws -> String {
        guard FileManager.default.fileExists(atPath: stateFile.path) else {
            throw ProviderFetchError.needsAuth("Abra o Cursor e entre na sua conta")
        }
        let process = Process()
        let output = Pipe()
        process.executableURL = URL(fileURLWithPath: "/usr/bin/sqlite3")
        process.arguments = ["-readonly", stateFile.path, "SELECT value FROM ItemTable WHERE key='cursorAuth/accessToken';"]
        process.standardOutput = output
        try process.run()
        process.waitUntilExit()
        guard process.terminationStatus == 0 else { throw ProviderFetchError.unavailable("Não foi possível ler a sessão do Cursor") }
        let raw = String(decoding: output.fileHandleForReading.readDataToEndOfFile(), as: UTF8.self).trimmingCharacters(in: .whitespacesAndNewlines)
        let token = (try? JSONDecoder().decode(String.self, from: Data(raw.utf8))) ?? raw
        guard token.split(separator: ".").count == 3 else { throw ProviderFetchError.needsAuth("Entre novamente no Cursor") }
        return token
    }
}

struct CursorUsage: Decodable {
    let billingCycleEnd: String?
    let isUnlimited: Bool?
    let individualUsage: IndividualUsage?

    enum CodingKeys: String, CodingKey {
        case billingCycleEnd
        case isUnlimited
        case individualUsage
    }

    struct IndividualUsage: Decodable {
        let plan: Plan?
        let onDemand: Spend?
    }

    struct Plan: Decodable {
        let totalPercentUsed: Double?
        let apiPercentUsed: Double?
    }

    struct Spend: Decodable {
        let enabled: Bool?
        let used: Double?
        let limit: Double?
    }

    var windows: [UsageWindow] {
        let reset = billingCycleEnd.flatMap(ISO8601Date.parse)
        let plan = individualUsage?.plan
        var result: [UsageWindow] = []
        if let total = plan?.totalPercentUsed { result.append(UsageWindow(id: "included", label: "Uso incluído", usedFraction: min(max(total / 100, 0), 1), resetsAt: reset)) }
        if let api = plan?.apiPercentUsed, api > 0 { result.append(UsageWindow(id: "api", label: "Uso de API", usedFraction: min(max(api / 100, 0), 1), resetsAt: reset)) }
        if individualUsage?.onDemand?.enabled == true, let used = individualUsage?.onDemand?.used, let limit = individualUsage?.onDemand?.limit, limit > 0 { result.append(UsageWindow(id: "on_demand", label: "Sob demanda", usedFraction: min(max(used / limit, 0), 1), resetsAt: reset)) }
        return result
    }
}

/// Parses ISO 8601 timestamps with or without fractional seconds; the default
/// `ISO8601DateFormatter` rejects values like `2026-09-22T21:30:00.010643+00:00`.
enum ISO8601Date {
    static func parse(_ value: String) -> Date? {
        let fractional = ISO8601DateFormatter()
        fractional.formatOptions = [.withInternetDateTime, .withFractionalSeconds]
        return fractional.date(from: value) ?? ISO8601DateFormatter().date(from: value)
    }
}
