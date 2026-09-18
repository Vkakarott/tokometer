import Foundation

protocol NativeUsageProvider: Sendable {
    var id: ProviderID { get }
    func fetch(now: Date) async throws -> ProviderSnapshot
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

struct ClaudeUsageProvider: NativeUsageProvider {
    let endpoint: URL

    init(endpoint: URL = URL(string: "https://api.anthropic.com/api/oauth/usage")!) {
        self.endpoint = endpoint
    }

    var id: ProviderID { .claude }

    func fetch(now: Date) async throws -> ProviderSnapshot {
        let credentials = try KeychainReader.readClaudeCredentials()
        guard let token = credentials.claudeAiOauth?.accessToken else {
            throw ProviderFetchError.needsAuth("Entre novamente no Claude Code")
        }
        var request = URLRequest(url: endpoint)
        request.timeoutInterval = 10
        request.setValue("Bearer \(token)", forHTTPHeaderField: "Authorization")
        request.setValue("oauth-2025-04-20", forHTTPHeaderField: "anthropic-beta")
        let (data, response) = try await URLSession.shared.data(for: request)
        guard let http = response as? HTTPURLResponse else {
            throw ProviderFetchError.unavailable("Claude não respondeu")
        }
        guard http.statusCode != 401, http.statusCode != 403 else {
            throw ProviderFetchError.needsAuth("Entre novamente no Claude Code")
        }
        guard http.statusCode != 429 else {
            let retryAfter = TimeInterval(http.value(forHTTPHeaderField: "Retry-After") ?? "") ?? 300
            throw ProviderFetchError.rateLimited(retryAfter: max(300, retryAfter))
        }
        guard http.statusCode == 200 else {
            throw ProviderFetchError.unavailable("Claude indisponível (HTTP \(http.statusCode))")
        }
        let usage = try JSONDecoder().decode(ClaudeUsage.self, from: data)
        let windows = [("five_hour", "5 horas", usage.fiveHour), ("seven_day", "7 dias", usage.sevenDay)]
            .compactMap(UsageWindow.init(claude:))
        guard !windows.isEmpty else {
            throw ProviderFetchError.unsupported("O Claude não informou limites para esta conta")
        }
        return ProviderSnapshot(
            id: id,
            displayName: id.displayName,
            fidelity: .derived,
            status: .ok,
            windows: windows,
            headlineID: windows.first?.id,
            hasData: true,
            observedAt: now
        )
    }
}

@MainActor
final class NativePollingController {
    private let providers: [any NativeUsageProvider]
    private let store: UsageStore
    private let legacyFallback: LegacySnapshotProvider
    private var task: Task<Void, Never>?
    private var retryAfter: [ProviderID: Date] = [:]

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
            if let next = retryAfter[provider.id], next > .now { continue }
            do {
                store.apply(try await provider.fetch(now: .now))
            } catch {
                if case let ProviderFetchError.rateLimited(delay) = error {
                    retryAfter[provider.id] = .now.addingTimeInterval(delay)
                }
                if let fallback = try? await legacyFallback.load().canonicalSnapshots().first(where: { $0.id == provider.id }) {
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

private struct ClaudeCredentials: Decodable {
    let claudeAiOauth: OAuth?

    struct OAuth: Decodable {
        let accessToken: String?
    }
}

private struct ClaudeUsage: Decodable {
    let fiveHour: Window?
    let sevenDay: Window?

    enum CodingKeys: String, CodingKey {
        case fiveHour = "five_hour"
        case sevenDay = "seven_day"
    }

    struct Window: Decodable {
        let utilization: Double?
        let resetsAt: String?

        enum CodingKeys: String, CodingKey {
            case utilization
            case resetsAt = "resets_at"
        }
    }
}

private extension UsageWindow {
    init?(claude source: (String, String, ClaudeUsage.Window?)) {
        guard let window = source.2, let utilization = window.utilization, let value = window.resetsAt, let reset = ISO8601DateFormatter().date(from: value) else { return nil }
        self.init(id: source.0, label: source.1, usedFraction: min(max(utilization / 100, 0), 1), resetsAt: reset)
    }
}

private enum KeychainReader {
    static func readClaudeCredentials() throws -> ClaudeCredentials {
        let process = Process()
        let output = Pipe()
        process.executableURL = URL(fileURLWithPath: "/usr/bin/security")
        process.arguments = ["find-generic-password", "-s", "Claude Code-credentials", "-w"]
        process.standardOutput = output
        try process.run()
        process.waitUntilExit()
        guard process.terminationStatus == 0 else {
            throw ProviderFetchError.needsAuth("Autorize o acesso ao login do Claude Code")
        }
        return try JSONDecoder().decode(ClaudeCredentials.self, from: output.fileHandleForReading.readDataToEndOfFile())
    }
}
