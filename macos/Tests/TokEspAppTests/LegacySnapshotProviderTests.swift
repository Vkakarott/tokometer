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
