import CryptoKit
import Foundation

/// Claude usage, from the two sources that exist.
///
/// `collector/statusline.sh` writes a file after every Claude Code reply, which
/// costs no request and is the documented data. The account usage route is a
/// fallback: it is undocumented, rate limited per token and its token is only
/// refreshed by Claude Code, so it is polled at most every 15 minutes.
struct ClaudeUsageProvider: NativeUsageProvider {
    let statusline: ClaudeReadingFile
    let pollCache: ClaudeReadingFile
    let gate: ClaudePollGate
    let endpoint: URL
    let readToken: @Sendable () throws -> String?
    let send: @Sendable (URLRequest) async throws -> (Data, URLResponse)

    init(
        statusline: ClaudeReadingFile = .statusline,
        pollCache: ClaudeReadingFile = .pollCache,
        gate: ClaudePollGate = ClaudePollGate(),
        endpoint: URL = URL(string: "https://api.anthropic.com/api/oauth/usage")!,
        readToken: @escaping @Sendable () throws -> String? = KeychainReader.claudeAccessToken,
        send: @escaping @Sendable (URLRequest) async throws -> (Data, URLResponse) = { try await URLSession.shared.data(for: $0) }
    ) {
        self.statusline = statusline
        self.pollCache = pollCache
        self.gate = gate
        self.endpoint = endpoint
        self.readToken = readToken
        self.send = send
    }

    var id: ProviderID { .claude }

    func fetch(now: Date) async throws -> ProviderSnapshot {
        let known = newestReading()
        if let known, known.isFresh(at: now) { return snapshot(known, now: now) }
        do {
            return snapshot(try await poll(now: now), now: now)
        } catch {
            guard let known else { throw error }
            return snapshot(known, now: now)
        }
    }

    private func newestReading() -> ClaudeReading? {
        [statusline.read(), pollCache.read()]
            .compactMap { $0 }
            .max { $0.observedAt < $1.observedAt }
    }

    private func poll(now: Date) async throws -> ClaudeReading {
        guard gate.isOpen(at: now) else {
            throw ProviderFetchError.unavailable("Aguardando dados do Claude Code")
        }
        guard let token = try readToken(), !token.isEmpty else {
            throw ProviderFetchError.needsAuth("Entre novamente no Claude Code")
        }
        let tokenID = ClaudePollGate.tokenID(token)
        guard !gate.rejects(tokenID) else {
            throw ProviderFetchError.needsAuth("Entre novamente no Claude Code")
        }
        let windows = try await requestWindows(token: token, tokenID: tokenID, now: now)
        gate.recordSuccess(at: now)
        let reading = ClaudeReading(windows: windows, observedAt: now, fidelity: .derived)
        try? pollCache.write(reading)
        return reading
    }

    private func requestWindows(token: String, tokenID: String, now: Date) async throws -> [UsageWindow] {
        var request = URLRequest(url: endpoint)
        request.timeoutInterval = 10
        request.setValue("Bearer \(token)", forHTTPHeaderField: "Authorization")
        request.setValue("oauth-2025-04-20", forHTTPHeaderField: "anthropic-beta")
        let (data, response) = try await send(request)
        guard let http = response as? HTTPURLResponse else {
            throw ProviderFetchError.unavailable("Claude não respondeu")
        }
        switch http.statusCode {
        case 200: break
        case 401, 403:
            gate.reject(tokenID)
            throw ProviderFetchError.needsAuth("Entre novamente no Claude Code")
        case 429:
            let header = TimeInterval(http.value(forHTTPHeaderField: "Retry-After") ?? "")
            throw ProviderFetchError.rateLimited(retryAfter: gate.recordRateLimit(at: now, retryAfter: header))
        default:
            throw ProviderFetchError.unavailable("Claude indisponível (HTTP \(http.statusCode))")
        }
        let windows = try JSONDecoder().decode(ClaudeUsage.self, from: data).windows
        guard !windows.isEmpty else {
            throw ProviderFetchError.unsupported("O Claude não informou limites para esta conta")
        }
        return windows
    }

    private func snapshot(_ reading: ClaudeReading, now: Date) -> ProviderSnapshot {
        let age = Int(max(0, now.timeIntervalSince(reading.observedAt)))
        return ProviderSnapshot(
            id: id,
            displayName: id.displayName,
            fidelity: reading.fidelity,
            status: reading.isFresh(at: now) ? .ok : .stale(ageSeconds: age),
            windows: reading.windows,
            headlineID: reading.windows.first?.id,
            hasData: true,
            observedAt: reading.observedAt
        )
    }
}

struct ClaudeReading: Equatable {
    let windows: [UsageWindow]
    let observedAt: Date
    let fidelity: Fidelity

    func isFresh(at now: Date) -> Bool {
        now.timeIntervalSince(observedAt) < ClaudePollGate.interval
    }
}

/// The snapshot file shared with `collector/statusline.sh`, in the same shape
/// the collector posts to the backend.
struct ClaudeReadingFile: Sendable {
    let url: URL
    let fidelity: Fidelity

    static let statusline = ClaudeReadingFile(
        url: stateDirectory.appending(path: "claude-statusline.json"),
        fidelity: .official
    )
    static let pollCache = ClaudeReadingFile(
        url: stateDirectory.appending(path: "claude-poll.json"),
        fidelity: .derived
    )

    static var stateDirectory: URL {
        if let override = ProcessInfo.processInfo.environment["TOKESP_STATE_DIR"] {
            return URL(fileURLWithPath: override)
        }
        return FileManager.default.homeDirectoryForCurrentUser.appending(path: ".cache/tokesp")
    }

    init(url: URL, fidelity: Fidelity = .official) {
        self.url = url
        self.fidelity = fidelity
    }

    func read() -> ClaudeReading? {
        guard let data = try? Data(contentsOf: url),
              let stored = try? JSONDecoder().decode(Stored.self, from: data) else { return nil }
        let windows = stored.windows.map(UsageWindow.init(stored:))
        guard !windows.isEmpty else { return nil }
        return ClaudeReading(windows: windows, observedAt: Date(timeIntervalSince1970: stored.observedAt), fidelity: fidelity)
    }

    func write(_ reading: ClaudeReading) throws {
        let stored = Stored(
            observedAt: reading.observedAt.timeIntervalSince1970,
            windows: reading.windows.map(Stored.Window.init(window:))
        )
        try FileManager.default.createDirectory(at: url.deletingLastPathComponent(), withIntermediateDirectories: true)
        try JSONEncoder().encode(stored).write(to: url, options: .atomic)
    }

    fileprivate struct Stored: Codable {
        let observedAt: TimeInterval
        let windows: [Window]

        struct Window: Codable {
            let id: String
            let usedPercentage: Double?
            let resetsAt: TimeInterval?

            init(window: UsageWindow) {
                id = window.id
                usedPercentage = window.usedFraction.map { $0 * 100 }
                resetsAt = window.resetsAt?.timeIntervalSince1970
            }
        }
    }
}

private extension UsageWindow {
    init(stored: ClaudeReadingFile.Stored.Window) {
        self.init(
            id: stored.id,
            label: UsageWindow.defaultLabel(for: stored.id),
            usedFraction: stored.usedPercentage.map { min(max($0 / 100, 0), 1) },
            resetsAt: stored.resetsAt.map(Date.init(timeIntervalSince1970:))
        )
    }
}

/// Keeps the undocumented usage route from being hammered: one poll per
/// interval, a wait after a rate limit, and no retry with a token the server
/// already rejected. Persisted, so restarting the app does not reset it.
struct ClaudePollGate: Sendable {
    static let interval: TimeInterval = 15 * 60
    static let minimumBackoff: TimeInterval = 300
    static let maximumBackoff: TimeInterval = 3_600

    let suiteName: String?

    init(suiteName: String? = nil) {
        self.suiteName = suiteName
    }

    private var defaults: UserDefaults {
        suiteName.flatMap(UserDefaults.init(suiteName:)) ?? .standard
    }

    func isOpen(at now: Date) -> Bool {
        now.timeIntervalSince1970 >= defaults.double(forKey: Key.nextPoll)
    }

    func rejects(_ tokenID: String) -> Bool {
        defaults.string(forKey: Key.rejectedToken) == tokenID
    }

    func recordSuccess(at now: Date) {
        defaults.set(now.addingTimeInterval(Self.interval).timeIntervalSince1970, forKey: Key.nextPoll)
        defaults.removeObject(forKey: Key.rejectedToken)
    }

    /// Anthropic answers with `Retry-After: 0` and keeps refusing, so a short
    /// wait is never honored as is. Returns the wait actually applied.
    func recordRateLimit(at now: Date, retryAfter: TimeInterval?) -> TimeInterval {
        let wait = min(max(retryAfter ?? 0, Self.minimumBackoff), Self.maximumBackoff)
        defaults.set(now.addingTimeInterval(wait).timeIntervalSince1970, forKey: Key.nextPoll)
        return wait
    }

    func reject(_ tokenID: String) {
        defaults.set(tokenID, forKey: Key.rejectedToken)
    }

    /// Identifies a token without keeping it around.
    static func tokenID(_ token: String) -> String {
        SHA256.hash(data: Data(token.utf8)).map { String(format: "%02x", $0) }.joined()
    }

    private enum Key {
        static let nextPoll = "claude.nextPollAt"
        static let rejectedToken = "claude.rejectedToken"
    }
}

struct ClaudeUsage: Decodable {
    let fiveHour: Window?
    let sevenDay: Window?

    var windows: [UsageWindow] {
        [("five_hour", fiveHour), ("seven_day", sevenDay)].compactMap(UsageWindow.init(claude:))
    }

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
    init?(claude source: (String, ClaudeUsage.Window?)) {
        guard let window = source.1,
              let utilization = window.utilization,
              let value = window.resetsAt,
              let reset = ISO8601Date.parse(value) else { return nil }
        self.init(
            id: source.0,
            label: UsageWindow.defaultLabel(for: source.0),
            usedFraction: min(max(utilization / 100, 0), 1),
            resetsAt: reset
        )
    }
}

struct ClaudeCredentials: Decodable {
    let claudeAiOauth: OAuth?

    struct OAuth: Decodable {
        let accessToken: String?
    }
}

enum KeychainReader {
    @Sendable
    static func claudeAccessToken() throws -> String? {
        try readClaudeCredentials().claudeAiOauth?.accessToken
    }

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
