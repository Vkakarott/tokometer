import Foundation
import Testing
@testable import TokEspApp

@Test("converts the legacy view into canonical snapshots")
func convertsLegacyView() {
    let view = LegacyProvidersView(
        providers: [
            .claude: LegacyUsageView(
                windows: [LegacyWindow(id: "five_hour", usedPercentage: 38, resetsAt: 1_800_003_600)],
                ageSeconds: 12,
                stale: false,
                hasData: true
            )
        ]
    )

    let claude = view.canonicalSnapshots().first(where: { $0.id == .claude })

    #expect(claude?.headline?.usedFraction == 0.38)
    #expect(claude?.status == .ok)
    #expect(claude?.fidelity == .legacy)
}

@Test("keeps expired legacy values as unknown")
func preservesUnknownWindow() {
    let view = LegacyProvidersView(
        providers: [
            .codex: LegacyUsageView(
                windows: [LegacyWindow(id: "five_hour", usedPercentage: nil, resetsAt: 1_800_003_600)],
                ageSeconds: 20,
                stale: true,
                hasData: true
            )
        ]
    )

    let codex = view.canonicalSnapshots().first(where: { $0.id == .codex })

    #expect(codex?.headline?.usedFraction == nil)
    #expect(codex?.status == .stale(ageSeconds: 20))
}

@Test("decodes the object keyed by provider identifiers")
func decodesBackendResponse() throws {
    let data = Data("""
    {
      "providers": {
        "claude": {
          "windows": [{"id":"five_hour","usedPercentage":55,"resetsAt":1789708200}],
          "ageSeconds":36,
          "stale":false,
          "hasData":true
        },
        "codex": {
          "windows": [{"id":"seven_day","usedPercentage":71,"resetsAt":1789989136}],
          "ageSeconds":106,
          "stale":false,
          "hasData":true
        }
      }
    }
    """.utf8)

    let view = try JSONDecoder().decode(LegacyProvidersView.self, from: data)

    #expect(view.providers[.claude]?.windows.first?.usedPercentage == 55)
    #expect(view.providers[.codex]?.windows.first?.id == "seven_day")
}

@Test("uses Cursor total percent for the included-usage headline")
func decodesCursorUsage() throws {
    let data = Data("""
    {
      "billingCycleEnd": "2026-09-24T03:32:15.933Z",
      "isUnlimited": false,
      "individualUsage": {
        "plan": {"totalPercentUsed": 9.5, "apiPercentUsed": 19.0},
        "onDemand": {"enabled": false}
      }
    }
    """.utf8)

    let usage = try JSONDecoder().decode(CursorUsage.self, from: data)

    #expect(usage.windows.first?.id == "included")
    #expect(usage.windows.first?.usedFraction == 0.095)
    #expect(usage.windows.last?.id == "api")
    #expect(usage.windows.last?.usedFraction == 0.19)
}

@Test("reads both Claude windows when resets_at has fractional seconds")
func decodesClaudeUsageWithFractionalSeconds() throws {
    let data = Data("""
    {
      "five_hour": {"utilization": 21.0, "resets_at": "2026-09-22T21:30:00.010643+00:00"},
      "seven_day": {"utilization": 92.0, "resets_at": "2026-09-23T03:00:00.010662+00:00"},
      "seven_day_opus": null
    }
    """.utf8)

    let usage = try JSONDecoder().decode(ClaudeUsage.self, from: data)

    #expect(usage.windows.map(\.id) == ["five_hour", "seven_day"])
    #expect(usage.windows.first?.usedFraction == 0.21)
    let reset = try #require(usage.windows.first?.resetsAt)
    #expect(abs(reset.timeIntervalSince1970 - 1_790_112_600.010643) < 0.001)
    #expect(usage.windows.last?.usedFraction == 0.92)
}

@Test("skips a Claude window without a reset time")
func skipsClaudeWindowWithoutReset() throws {
    let data = Data("""
    {
      "five_hour": {"utilization": 0.0, "resets_at": null},
      "seven_day": {"utilization": 89.0, "resets_at": "2026-09-23T03:00:00+00:00"}
    }
    """.utf8)

    let usage = try JSONDecoder().decode(ClaudeUsage.self, from: data)

    #expect(usage.windows.map(\.id) == ["seven_day"])
}
