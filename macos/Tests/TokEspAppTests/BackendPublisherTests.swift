import Foundation
import Testing
@testable import TokEspApp

private let observed = Date(timeIntervalSince1970: 1_790_000_000)
private let config = BackendConfig(url: URL(string: "http://localhost:43110")!, token: "collector-secret")

/// Records every request the publisher sends and answers with `status`.
private final class FakeBackend: @unchecked Sendable {
    private let lock = NSLock()
    private var _requests: [URLRequest] = []
    var status = 204

    var requests: [URLRequest] { lock.withLock { _requests } }

    func send(_ request: URLRequest) -> (Data, URLResponse) {
        lock.withLock { _requests.append(request) }
        return (Data(), HTTPURLResponse(url: request.url!, statusCode: status, httpVersion: nil, headerFields: nil)!)
    }
}

private func snapshot(
    _ provider: ProviderID,
    windows: [UsageWindow],
    fidelity: Fidelity = .official,
    hasData: Bool = true
) -> ProviderSnapshot {
    ProviderSnapshot(
        id: provider,
        displayName: provider.displayName,
        fidelity: fidelity,
        status: .ok,
        windows: windows,
        headlineID: windows.first?.id,
        hasData: hasData,
        observedAt: observed
    )
}

private func window(_ id: String, _ fraction: Double?, resetsIn seconds: TimeInterval? = 3_600) -> UsageWindow {
    UsageWindow(id: id, label: id, usedFraction: fraction, resetsAt: seconds.map { observed.addingTimeInterval($0) })
}

private func publisher(_ fake: FakeBackend, config: BackendConfig? = config) -> BackendPublisher {
    BackendPublisher(loadConfig: { config }, send: { fake.send($0) }, source: "mac")
}

private func body(_ request: URLRequest) throws -> [String: Any] {
    let data = try #require(request.httpBody)
    return try #require(JSONSerialization.jsonObject(with: data) as? [String: Any])
}

@Test func sendsClaudeUnderItsCollectorName() async throws {
    let fake = FakeBackend()
    await publisher(fake).publish(snapshot(.claude, windows: [window("five_hour", 0.86), window("seven_day", 0.19)]))

    let request = try #require(fake.requests.first)
    #expect(request.url?.absoluteString == "http://localhost:43110/ingest")
    #expect(request.httpMethod == "POST")
    #expect(request.value(forHTTPHeaderField: "Authorization") == "Bearer collector-secret")
    let payload = try body(request)
    #expect(payload["provider"] as? String == "claude-code")
    #expect(payload["source"] as? String == "mac")
    #expect(payload["observedAt"] as? Double == 1_790_000_000)
    let windows = try #require(payload["windows"] as? [[String: Any]])
    #expect(windows.map { $0["id"] as? String } == ["five_hour", "seven_day"])
    #expect(windows.first?["usedPercentage"] as? Double == 86)
    #expect(windows.first?["resetsAt"] as? Double == 1_790_003_600)
}

@Test func sendsCursorBillingWindows() async throws {
    let fake = FakeBackend()
    await publisher(fake).publish(snapshot(.cursor, windows: [window("included", 0.21), window("on_demand", 0.125)]))

    let payload = try body(try #require(fake.requests.first))
    #expect(payload["provider"] as? String == "cursor")
    #expect((payload["windows"] as? [[String: Any]])?.map { $0["id"] as? String } == ["included", "on_demand"])
}

@Test func dropsIncompleteWindowsAndSkipsWhenNoneRemain() async throws {
    let fake = FakeBackend()
    let sender = publisher(fake)
    await sender.publish(snapshot(.codex, windows: [window("five_hour", nil), window("seven_day", 0.16, resetsIn: nil)]))
    #expect(fake.requests.isEmpty, "an empty snapshot would erase the backend's data")

    await sender.publish(snapshot(.codex, windows: [window("five_hour", nil), window("seven_day", 0.16)]))
    let windows = try body(try #require(fake.requests.first))["windows"] as? [[String: Any]]
    #expect(windows?.map { $0["id"] as? String } == ["seven_day"])
}

@Test func neverEchoesBackendDataOrEmptySnapshots() async {
    let fake = FakeBackend()
    let sender = publisher(fake)
    await sender.publish(snapshot(.claude, windows: [window("five_hour", 0.5)], fidelity: .legacy))
    await sender.publish(snapshot(.claude, windows: [window("five_hour", 0.5)], hasData: false))
    #expect(fake.requests.isEmpty)
}

@Test func sendsEachReadingOnceAndRetriesAfterFailure() async {
    let fake = FakeBackend()
    let sender = publisher(fake)
    let reading = snapshot(.codex, windows: [window("five_hour", 0.5)])

    fake.status = 500
    await sender.publish(reading)
    fake.status = 204
    await sender.publish(reading)
    await sender.publish(reading)
    #expect(fake.requests.count == 2, "a rejected reading is retried, an accepted one is not re-sent")

    await sender.publish(snapshot(.codex, windows: [window("five_hour", 0.6)]))
    #expect(fake.requests.count == 3)
}

@Test func doesNothingWithoutBackendConfig() async {
    let fake = FakeBackend()
    await publisher(fake, config: nil).publish(snapshot(.codex, windows: [window("five_hour", 0.5)]))
    #expect(fake.requests.isEmpty)
}

@Test func parsesTheCollectorConfigFile() {
    let text = """
    # tokEsp collector config (see collector/README.md).
    export TOKESP_URL="http://localhost:43110"
    export TOKESP_TOKEN='secret value'
    """
    #expect(BackendConfig.parse(text) == BackendConfig(url: URL(string: "http://localhost:43110")!, token: "secret value"))
    #expect(BackendConfig.parse("export TOKESP_URL=\"http://localhost:43110\"") == nil)
    #expect(BackendConfig.parse("# export TOKESP_URL=x\nexport TOKESP_TOKEN=t") == nil)
}
