import Foundation
import os

/// Backend address and collector token, read from the file the shell
/// collectors use (`~/.config/tokesp/config.sh`), so there is one place to set them.
struct BackendConfig: Equatable, Sendable {
    let url: URL
    let token: String

    static var defaultFile: URL {
        if let override = ProcessInfo.processInfo.environment["TOKESP_CONFIG"] {
            return URL(fileURLWithPath: override)
        }
        return FileManager.default.homeDirectoryForCurrentUser.appending(path: ".config/tokesp/config.sh")
    }

    static func load(from file: URL = defaultFile) -> BackendConfig? {
        guard let text = try? String(contentsOf: file, encoding: .utf8) else { return nil }
        return parse(text)
    }

    /// Reads plain `export KEY="value"` assignments; the file is never executed.
    static func parse(_ text: String) -> BackendConfig? {
        var values: [String: String] = [:]
        for line in text.split(whereSeparator: \.isNewline) {
            guard let (key, value) = assignment(String(line)) else { continue }
            values[key] = value
        }
        guard let rawURL = values["TOKESP_URL"], let url = URL(string: rawURL), url.scheme != nil,
              let token = values["TOKESP_TOKEN"], !token.isEmpty else { return nil }
        return BackendConfig(url: url, token: token)
    }

    private static func assignment(_ line: String) -> (String, String)? {
        var statement = line.trimmingCharacters(in: .whitespaces)
        guard !statement.hasPrefix("#") else { return nil }
        if statement.hasPrefix("export ") { statement.removeFirst("export ".count) }
        guard let equals = statement.firstIndex(of: "=") else { return nil }
        let key = statement[..<equals].trimmingCharacters(in: .whitespaces)
        var value = statement[statement.index(after: equals)...].trimmingCharacters(in: .whitespaces)
        if value.count >= 2, let quote = value.first, quote == "\"" || quote == "'", value.last == quote {
            value = String(value.dropFirst().dropLast())
        }
        return (key, value)
    }
}

/// The `POST /ingest` body, in the shape the shell collectors send.
struct IngestPayload: Encodable, Equatable, Sendable {
    let provider: String
    let source: String
    let observedAt: TimeInterval
    let windows: [Window]

    struct Window: Encodable, Equatable, Sendable {
        let id: String
        let usedPercentage: Double
        let resetsAt: TimeInterval
    }

    /// Nil when there is nothing worth sending: no data, data the app itself
    /// read back from the backend, or no window with both values. An empty
    /// snapshot would erase the backend's last known data.
    init?(snapshot: ProviderSnapshot, source: String) {
        guard snapshot.hasData, snapshot.fidelity != .legacy, let observedAt = snapshot.observedAt else { return nil }
        let windows = snapshot.windows.compactMap(Window.init(window:))
        guard !windows.isEmpty else { return nil }
        provider = snapshot.id.collectorName
        self.source = source
        self.observedAt = observedAt.timeIntervalSince1970.rounded(.down)
        self.windows = windows
    }
}

private extension IngestPayload.Window {
    init?(window: UsageWindow) {
        guard let fraction = window.usedFraction, let resetsAt = window.resetsAt else { return nil }
        self.init(id: window.id, usedPercentage: fraction * 100, resetsAt: resetsAt.timeIntervalSince1970.rounded(.down))
    }
}

private extension ProviderID {
    /// The backend names providers after the collector that reports them.
    var collectorName: String {
        switch self {
        case .claude: "claude-code"
        case .codex: "codex"
        case .cursor: "cursor"
        }
    }
}

/// Forwards what the app collects to the backend, which feeds the ESP32
/// display. Each provider's reading is sent once; a failed send is retried on
/// the next refresh.
actor BackendPublisher {
    private let loadConfig: @Sendable () -> BackendConfig?
    private let send: @Sendable (URLRequest) async throws -> (Data, URLResponse)
    private let source: String
    private var lastSent: [String: IngestPayload] = [:]
    private let logger = Logger(subsystem: "com.tokesp.app", category: "backend")

    init(
        loadConfig: @escaping @Sendable () -> BackendConfig? = { BackendConfig.load() },
        send: @escaping @Sendable (URLRequest) async throws -> (Data, URLResponse) = { try await URLSession.shared.data(for: $0) },
        source: String = BackendPublisher.hostName
    ) {
        self.loadConfig = loadConfig
        self.send = send
        self.source = source
    }

    /// Short host name, as `hostname -s` gives the shell collectors.
    static var hostName: String {
        ProcessInfo.processInfo.hostName.split(separator: ".").first.map(String.init) ?? "mac"
    }

    func publish(_ snapshot: ProviderSnapshot) async {
        guard let payload = IngestPayload(snapshot: snapshot, source: source),
              lastSent[payload.provider] != payload,
              let config = loadConfig() else { return }
        do {
            try await post(payload, to: config)
            lastSent[payload.provider] = payload
        } catch {
            logger.error("ingest failed for \(payload.provider, privacy: .public): \(error.localizedDescription, privacy: .public)")
        }
    }

    private func post(_ payload: IngestPayload, to config: BackendConfig) async throws {
        var request = URLRequest(url: config.url.appending(path: "ingest"))
        request.httpMethod = "POST"
        request.timeoutInterval = 5
        request.setValue("Bearer \(config.token)", forHTTPHeaderField: "Authorization")
        request.setValue("application/json", forHTTPHeaderField: "Content-Type")
        request.httpBody = try JSONEncoder().encode(payload)
        let (_, response) = try await send(request)
        let status = (response as? HTTPURLResponse)?.statusCode ?? 0
        guard (200..<300).contains(status) else { throw BackendPublishError.rejected(status: status) }
    }
}

enum BackendPublishError: LocalizedError {
    case rejected(status: Int)

    var errorDescription: String? {
        switch self {
        case let .rejected(status): "backend answered HTTP \(status)"
        }
    }
}
