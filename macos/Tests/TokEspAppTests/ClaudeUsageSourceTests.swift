import Foundation
import Testing
@testable import TokEspApp

private let start = Date(timeIntervalSince1970: 1_790_000_000)

private let usageBody = Data("""
{
  "five_hour": {"utilization": 21.0, "resets_at": "2026-09-22T21:30:00.010643+00:00"},
  "seven_day": {"utilization": 92.0, "resets_at": "2026-09-23T03:00:00.010662+00:00"}
}
""".utf8)

private let nearLimitUsageBody = Data("""
{
  "five_hour": {"utilization": 95.0, "resets_at": "2026-09-22T21:30:00.010643+00:00"},
  "seven_day": {"utilization": 92.0, "resets_at": "2026-09-23T03:00:00.010662+00:00"}
}
""".utf8)

/// Counts calls and serves scripted HTTP answers to the provider.
private final class FakeClaude: @unchecked Sendable {
    private let lock = NSLock()
    private var _requests = 0
    private var _tokenReads = 0
    var token = "token-a"
    var status = 200
    var retryAfter: String?
    var body = usageBody

    var requests: Int { lock.withLock { _requests } }
    var tokenReads: Int { lock.withLock { _tokenReads } }

    func readToken() -> String? {
        lock.withLock { _tokenReads += 1 }
        return token
    }

    func send(_ request: URLRequest) -> (Data, URLResponse) {
        lock.withLock { _requests += 1 }
        let headers = retryAfter.map { ["Retry-After": $0] }
        let response = HTTPURLResponse(url: request.url!, statusCode: status, httpVersion: nil, headerFields: headers)!
        return (status == 200 ? body : Data(), response)
    }
}

private struct Fixture {
    let fake = FakeClaude()
    let directory: URL
    let statusline: ClaudeReadingFile
    let provider: ClaudeUsageProvider

    init() throws {
        directory = FileManager.default.temporaryDirectory.appending(path: UUID().uuidString)
        try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
        statusline = ClaudeReadingFile(url: directory.appending(path: "claude-statusline.json"))
        let fake = fake
        provider = ClaudeUsageProvider(
            statusline: statusline,
            pollCache: ClaudeReadingFile(url: directory.appending(path: "claude-poll.json")),
            gate: ClaudePollGate(suiteName: "tokesp-tests-\(UUID().uuidString)"),
            readToken: { fake.readToken() },
            send: { fake.send($0) }
        )
    }

    /// Writes the file exactly as collector/statusline.sh does.
    func writeStatusline(observedAt: Date, fiveHour: Int = 30, includeFiveHour: Bool = true) throws {
        let fiveHourWindow = includeFiveHour
            ? #"{"id":"five_hour","usedPercentage":\#(fiveHour),"resetsAt":1790112600},"#
            : ""
        let json = """
        {"provider":"claude-code","source":"mac","observedAt":\(Int(observedAt.timeIntervalSince1970)),\
        "windows":[\(fiveHourWindow)\
        {"id":"seven_day","usedPercentage":92,"resetsAt":1790132400}],"fingerprint":"x"}
        """
        try Data(json.utf8).write(to: statusline.url)
    }
}

@Test("uses a fresh statusline reading without touching the network or the Keychain")
func usesFreshStatusline() async throws {
    let fixture = try Fixture()
    try fixture.writeStatusline(observedAt: start.addingTimeInterval(-60))

    let snapshot = try await fixture.provider.fetch(now: start)

    #expect(snapshot.windows.first?.usedFraction == 0.30)
    #expect(snapshot.status == .ok)
    #expect(fixture.fake.requests == 0)
    #expect(fixture.fake.tokenReads == 0)
}

@Test("polls when a fresh statusline reading omits a subscription window")
func pollsWhenFreshStatuslineIsIncomplete() async throws {
    let fixture = try Fixture()
    try fixture.writeStatusline(observedAt: start.addingTimeInterval(-60), includeFiveHour: false)

    let snapshot = try await fixture.provider.fetch(now: start)

    #expect(snapshot.windows.count == 2)
    #expect(snapshot.windows.first?.id == "five_hour")
    #expect(snapshot.windows.first?.usedFraction == 0.21)
    #expect(snapshot.windows.last?.id == "seven_day")
    #expect(snapshot.windows.last?.usedFraction == 0.92)
    #expect(fixture.fake.requests == 1)
}

@Test("polls once when the statusline is old, then waits 15 minutes")
func pollsWhenStatuslineIsOld() async throws {
    let fixture = try Fixture()
    try fixture.writeStatusline(observedAt: start.addingTimeInterval(-3_600))

    let polled = try await fixture.provider.fetch(now: start)
    let again = try await fixture.provider.fetch(now: start.addingTimeInterval(60))

    #expect(polled.windows.first?.usedFraction == 0.21)
    #expect(again.windows.first?.usedFraction == 0.30)
    #expect(again.status == .ok)
    #expect(fixture.fake.requests == 1)
}

@Test("refreshes near-limit Claude usage after one minute")
func refreshesNearLimitUsage() async throws {
    let fixture = try Fixture()
    fixture.fake.body = nearLimitUsageBody

    _ = try await fixture.provider.fetch(now: start)
    let refreshed = try await fixture.provider.fetch(now: start.addingTimeInterval(61))

    #expect(refreshed.windows.first?.usedFraction == 0.95)
    #expect(fixture.fake.requests == 2)
}

@Test("keeps the last reading and backs off after a rate limit, even across restarts")
func backsOffAfterRateLimit() async throws {
    let fixture = try Fixture()
    try fixture.writeStatusline(observedAt: start.addingTimeInterval(-3_600))
    fixture.fake.status = 429
    fixture.fake.retryAfter = "0"

    let limited = try await fixture.provider.fetch(now: start)
    let waiting = try await fixture.provider.fetch(now: start.addingTimeInterval(120))
    fixture.fake.status = 200
    let retried = try await fixture.provider.fetch(now: start.addingTimeInterval(301))

    #expect(limited.windows.first?.usedFraction == 0.30)
    #expect(limited.status == .stale(ageSeconds: 3_600))
    #expect(waiting.status == .stale(ageSeconds: 3_720))
    #expect(retried.windows.first?.usedFraction == 0.21)
    #expect(fixture.fake.requests == 2)
}

@Test("does not retry a rejected token until Claude Code refreshes it")
func waitsForNewTokenAfterUnauthorized() async throws {
    let fixture = try Fixture()
    fixture.fake.status = 401

    await #expect(throws: ProviderFetchError.self) { try await fixture.provider.fetch(now: start) }
    fixture.fake.status = 200
    await #expect(throws: ProviderFetchError.self) { try await fixture.provider.fetch(now: start.addingTimeInterval(1_000)) }
    fixture.fake.token = "token-b"
    let refreshed = try await fixture.provider.fetch(now: start.addingTimeInterval(1_060))

    #expect(refreshed.windows.first?.usedFraction == 0.21)
    #expect(fixture.fake.requests == 2)
}

@Test("prefers a newer statusline reading over an older poll")
func prefersNewestReading() async throws {
    let fixture = try Fixture()
    _ = try await fixture.provider.fetch(now: start)
    try fixture.writeStatusline(observedAt: start.addingTimeInterval(120), fiveHour: 40)

    let snapshot = try await fixture.provider.fetch(now: start.addingTimeInterval(180))

    #expect(snapshot.windows.first?.usedFraction == 0.40)
    #expect(fixture.fake.requests == 1)
}

@Test("keeps the higher usage when sources describe the same reset window")
func keepsHigherUsageWithinSameWindow() async throws {
    let fixture = try Fixture()
    _ = try await fixture.provider.fetch(now: start)
    try fixture.writeStatusline(observedAt: start.addingTimeInterval(60), fiveHour: 11)

    let snapshot = try await fixture.provider.fetch(now: start.addingTimeInterval(120))

    #expect(snapshot.windows.first?.id == "five_hour")
    #expect(snapshot.windows.first?.usedFraction == 0.21)
    #expect(fixture.fake.requests == 1)
}
