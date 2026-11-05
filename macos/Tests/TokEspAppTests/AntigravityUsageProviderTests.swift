import Foundation
import Testing
@testable import TokEspApp

private let now = Date(timeIntervalSince1970: 1_793_660_000)

/// Shape of `agy -p /usage --output-format json`, values synthetic.
private func report(
    status: String = "SUCCESS",
    geminiFiveHour: String = #""remaining_fraction": 0.75"#,
    extraGroups: String = ""
) -> Data {
    Data("""
    {
      "status": "\(status)",
      "num_turns": 0,
      "command": {
        "name": "usage",
        "data": {
          "groups": [
            {
              "name": "Gemini Models",
              "buckets": [
                {"id": "gemini-weekly", "name": "Weekly Limit Remaining", "window": "weekly", "remaining_fraction": 0.9, "reset_time": "2026-11-09T21:59:11Z"},
                {"id": "gemini-5h", "name": "Five Hour Limit Remaining", "window": "5h", \(geminiFiveHour), "reset_time": "2026-11-03T02:59:11Z"}
              ]
            },
            {
              "name": "Claude and GPT models",
              "buckets": [
                {"id": "3p-weekly", "name": "Weekly Limit Remaining", "window": "weekly", "remaining_fraction": 0.4, "reset_time": "2026-11-09T21:59:11Z"},
                {"id": "3p-5h", "name": "Five Hour Limit Remaining", "window": "5h", "remaining_fraction": 1, "reset_time": "2026-11-03T02:59:11Z"}
              ]
            }\(extraGroups)
          ]
        }
      }
    }
    """.utf8)
}

/// Serves scripted `agy` output and counts how often the CLI would run.
private final class FakeAgy: @unchecked Sendable {
    private let lock = NSLock()
    private var _runs = 0
    var output: Result<Data, Error>

    init(_ output: Data) {
        self.output = .success(output)
    }

    var runs: Int { lock.withLock { _runs } }

    func run() throws -> Data {
        lock.withLock { _runs += 1 }
        return try output.get()
    }
}

private func provider(_ group: AntigravityQuotaGroup, _ fake: FakeAgy, reader: AntigravityUsageReader? = nil) -> AntigravityUsageProvider {
    AntigravityUsageProvider(group: group, reader: reader ?? AntigravityUsageReader(run: { try fake.run() }))
}

@Test func geminiGroupBecomesFiveHourAndSevenDayWindows() async throws {
    let snapshot = try await provider(.gemini, FakeAgy(report())).fetch(now: now)

    #expect(snapshot.id == .antigravityGemini)
    #expect(snapshot.fidelity == .official)
    #expect(snapshot.windows.map(\.id) == ["five_hour", "seven_day"])
    #expect(snapshot.windows[0].usedFraction == 0.25)
    #expect(abs((snapshot.windows[1].usedFraction ?? 0) - 0.1) < 0.000_1)
    #expect(snapshot.windows[0].resetsAt == ISO8601Date.parse("2026-11-03T02:59:11Z"))
    #expect(snapshot.headlineID == "five_hour")
    #expect(snapshot.observedAt == now)
}

@Test func claudeGroupReadsTheClaudeAndGptBuckets() async throws {
    let snapshot = try await provider(.claude, FakeAgy(report())).fetch(now: now)

    #expect(snapshot.id == .antigravityClaude)
    #expect(snapshot.windows.map(\.usedFraction) == [0, 0.6])
}

@Test func missingOrDisabledBucketIsUnknownNotZero() async throws {
    let missing = try await provider(.gemini, FakeAgy(report(geminiFiveHour: #""disabled": false"#))).fetch(now: now)
    let disabled = try await provider(.gemini, FakeAgy(report(geminiFiveHour: #""disabled": true, "remaining_fraction": 1"#))).fetch(now: now)

    #expect(missing.windows[0].usedFraction == nil)
    #expect(disabled.windows[0].usedFraction == nil)
}

@Test func modelSpecificGroupsDoNotReplaceTheSharedGeminiQuota() async throws {
    let argon = #"""
    ,{"name": "Gemini 4 Argon", "buckets": [{"id": "gemini-argon-wtu-weekly", "window": "weekly", "remaining_fraction": 0.01, "reset_time": "2026-11-09T21:59:11Z"}]}
    """#
    let fake = FakeAgy(report(extraGroups: argon))
    let reader = AntigravityUsageReader(run: { try fake.run() })
    let gemini = try await provider(.gemini, fake, reader: reader).fetch(now: now)
    let argonSnapshot = try await provider(.argon, fake, reader: reader).fetch(now: now)

    #expect(abs((gemini.windows[1].usedFraction ?? 0) - 0.1) < 0.000_1)
    #expect(argonSnapshot.id == .antigravityArgon)
    #expect(argonSnapshot.windows.map(\.id) == ["seven_day"])
    #expect(argonSnapshot.headlineID == "seven_day")
    #expect(abs((argonSnapshot.windows[0].usedFraction ?? 0) - 0.99) < 0.000_1)
    #expect(fake.runs == 1)
}

@Test func bothGroupsShareOneCliRun() async throws {
    let fake = FakeAgy(report())
    let reader = AntigravityUsageReader(run: { try fake.run() })

    _ = try await provider(.gemini, fake, reader: reader).fetch(now: now)
    _ = try await provider(.claude, fake, reader: reader).fetch(now: now)

    #expect(fake.runs == 1)
}

@Test func failedReportAsksToSignInToTheCli() async {
    await #expect(throws: ProviderFetchError.self) {
        try await provider(.gemini, FakeAgy(report(status: "ERROR"))).fetch(now: now)
    }
}

@Test func reportWithoutTheGroupIsUnsupported() async {
    let onlyGemini = Data(#"{"status": "SUCCESS", "command": {"name": "usage", "data": {"groups": [{"name": "Gemini Models", "buckets": []}]}}}"#.utf8)

    await #expect(throws: ProviderFetchError.self) {
        try await provider(.claude, FakeAgy(onlyGemini)).fetch(now: now)
    }
}

@Test func failedRunIsRetriedOnTheNextFetch() async throws {
    let fake = FakeAgy(report())
    fake.output = .failure(ProviderFetchError.unavailable("agy falhou"))
    let reader = AntigravityUsageReader(run: { try fake.run() })

    await #expect(throws: ProviderFetchError.self) {
        try await provider(.gemini, fake, reader: reader).fetch(now: now)
    }
    fake.output = .success(report())
    _ = try await provider(.gemini, fake, reader: reader).fetch(now: now)

    #expect(fake.runs == 2)
}
