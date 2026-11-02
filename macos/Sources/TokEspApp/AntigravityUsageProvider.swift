import Foundation

/// Antigravity shares one quota among the Gemini models and another among the
/// Claude and GPT models; each group is shown as its own provider.
enum AntigravityQuotaGroup: Sendable {
    case gemini
    case claude

    var providerID: ProviderID {
        switch self {
        case .gemini: .antigravityGemini
        case .claude: .antigravityClaude
        }
    }

    /// Model-specific groups such as "Gemini 4 Argon" carry a number and are a
    /// separate allowance, so only the unnumbered Gemini group matches.
    func matches(groupName: String) -> Bool {
        let name = groupName.lowercased()
        switch self {
        case .gemini: return name.contains("gemini") && !name.contains(where: \.isNumber)
        case .claude: return name.contains("claude") || name.contains("gpt")
        }
    }
}

struct AntigravityUsageProvider: NativeUsageProvider {
    let group: AntigravityQuotaGroup
    let reader: AntigravityUsageReader

    var id: ProviderID { group.providerID }
    var refreshInterval: TimeInterval { 300 }

    func fetch(now: Date) async throws -> ProviderSnapshot {
        let report = try await reader.report()
        guard let windows = report.windows(for: group), !windows.isEmpty else {
            throw ProviderFetchError.unsupported("O Antigravity não informou a cota de \(id.displayName)")
        }
        return ProviderSnapshot(id: id, displayName: id.displayName, fidelity: .official, status: .ok, windows: windows, headlineID: windows.first?.id, hasData: true, observedAt: now)
    }
}

/// Runs the CLI once for both quota groups: a report younger than `maxAge`
/// is reused, and a failed run is never cached.
actor AntigravityUsageReader {
    static let shared = AntigravityUsageReader()

    private let run: @Sendable () throws -> Data
    private let maxAge: TimeInterval
    private var cached: (report: AntigravityUsageReport, readAt: Date)?
    private var inFlight: Task<AntigravityUsageReport, Error>?

    init(maxAge: TimeInterval = 60, run: @escaping @Sendable () throws -> Data = { try AgyUsageCommand.run() }) {
        self.maxAge = maxAge
        self.run = run
    }

    func report() async throws -> AntigravityUsageReport {
        if let cached, Date.now.timeIntervalSince(cached.readAt) < maxAge { return cached.report }
        if let inFlight { return try await inFlight.value }
        let task = Task { [run] in try AntigravityUsageReport.decode(try await Self.offMainThread(run)) }
        inFlight = task
        defer { inFlight = nil }
        let report = try await task.value
        cached = (report, .now)
        return report
    }

    /// The CLI blocks for several seconds; keep it off Swift's cooperative pool.
    private static func offMainThread(_ run: @escaping @Sendable () throws -> Data) async throws -> Data {
        try await withCheckedThrowingContinuation { continuation in
            DispatchQueue.global(qos: .utility).async {
                continuation.resume(with: Result { try run() })
            }
        }
    }
}

/// `agy -p /usage` prints the account's quota without starting a model turn.
/// The CLI keeps its own Google login; the app never reads it.
enum AgyUsageCommand {
    static func run(timeout: TimeInterval = 30) throws -> Data {
        guard let binary = locate() else {
            throw ProviderFetchError.unavailable("Instale a CLI do Antigravity (agy)")
        }
        let process = Process()
        let output = Pipe()
        process.executableURL = binary
        process.arguments = ["-p", "/usage", "--output-format", "json", "--print-timeout", "\(Int(timeout))s"]
        process.currentDirectoryURL = FileManager.default.temporaryDirectory
        process.standardInput = FileHandle.nullDevice
        process.standardError = FileHandle.nullDevice
        process.standardOutput = output
        try process.run()
        let watchdog = DispatchWorkItem { process.terminate() }
        DispatchQueue.global().asyncAfter(deadline: .now() + timeout + 5, execute: watchdog)
        let data = output.fileHandleForReading.readDataToEndOfFile()
        process.waitUntilExit()
        watchdog.cancel()
        guard process.terminationReason == .exit, process.terminationStatus == 0 else {
            throw ProviderFetchError.unavailable("A CLI do Antigravity não respondeu")
        }
        return data
    }

    /// Apps launched by launchd do not inherit the shell PATH, so check where
    /// the installer and package managers put the binary.
    private static func locate() -> URL? {
        let home = FileManager.default.homeDirectoryForCurrentUser
        let candidates = [
            home.appending(path: ".local/bin/agy"),
            URL(fileURLWithPath: "/opt/homebrew/bin/agy"),
            URL(fileURLWithPath: "/usr/local/bin/agy"),
        ]
        return candidates.first { FileManager.default.isExecutableFile(atPath: $0.path) }
    }
}

struct AntigravityUsageReport: Decodable, Sendable {
    let groups: [Group]

    struct Group: Decodable, Sendable {
        let name: String?
        let buckets: [Bucket]?
    }

    struct Bucket: Decodable, Sendable {
        let id: String?
        let window: String?
        let disabled: Bool?
        let remainingFraction: Double?
        let resetTime: String?
    }

    static func decode(_ data: Data) throws -> AntigravityUsageReport {
        let decoder = JSONDecoder()
        decoder.keyDecodingStrategy = .convertFromSnakeCase
        guard let envelope = try? decoder.decode(Envelope.self, from: data),
              envelope.status == "SUCCESS", envelope.command?.name == "usage",
              let groups = envelope.command?.data?.groups else {
            throw ProviderFetchError.needsAuth("Entre na CLI do Antigravity (agy)")
        }
        return AntigravityUsageReport(groups: groups)
    }

    /// Nil when the report has no group for `quota`.
    func windows(for quota: AntigravityQuotaGroup) -> [UsageWindow]? {
        guard let group = groups.first(where: { quota.matches(groupName: $0.name ?? "") }) else { return nil }
        return (group.buckets ?? [])
            .compactMap(UsageWindow.init(antigravity:))
            .sorted { $0.id == "five_hour" && $1.id != "five_hour" }
    }

    private struct Envelope: Decodable {
        let status: String?
        let command: Command?

        struct Command: Decodable {
            let name: String?
            let data: Payload?
        }

        struct Payload: Decodable {
            let groups: [Group]?
        }
    }
}

private extension UsageWindow {
    /// A missing fraction may be proto3 dropping an exhausted 0; a disabled
    /// bucket is paused. Both are unknown rather than a guessed percentage.
    init?(antigravity bucket: AntigravityUsageReport.Bucket) {
        guard let kind = Self.antigravityKind(of: bucket) else { return nil }
        let used = bucket.disabled == true ? nil : bucket.remainingFraction.map { 1 - min(max($0, 0), 1) }
        self.init(id: kind, label: Self.defaultLabel(for: kind), usedFraction: used, resetsAt: bucket.resetTime.flatMap(ISO8601Date.parse))
    }

    static func antigravityKind(of bucket: AntigravityUsageReport.Bucket) -> String? {
        let cadence = (bucket.window ?? bucket.id ?? "").lowercased()
        if cadence == "5h" || cadence.hasSuffix("-5h") { return "five_hour" }
        if cadence == "weekly" || cadence.hasSuffix("-weekly") { return "seven_day" }
        return nil
    }
}
