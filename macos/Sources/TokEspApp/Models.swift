import Foundation

enum ProviderID: String, CaseIterable, Codable, CodingKey, Identifiable {
    case claude
    case codex

    var id: String { rawValue }

    init?(stringValue: String) {
        self.init(rawValue: stringValue)
    }

    var stringValue: String { rawValue }

    init?(intValue: Int) {
        nil
    }

    var intValue: Int? { nil }

    var displayName: String {
        switch self {
        case .claude: "Claude"
        case .codex: "Codex"
        }
    }
}

enum Fidelity: String, Codable {
    case official
    case derived
    case manual
    case legacy
}

enum ProviderStatus: Equatable {
    case ok
    case stale(ageSeconds: Int)
    case needsAuth
    case accessDenied
    case unsupported(reason: String)
    case error(reason: String)

    var label: String {
        switch self {
        case .ok: "Atualizado"
        case let .stale(ageSeconds): "Dados de \(ageSeconds.formatted()) s atrás"
        case .needsAuth: "Autenticação necessária"
        case .accessDenied: "Acesso negado"
        case let .unsupported(reason), let .error(reason): reason
        }
    }
}

struct UsageWindow: Equatable, Identifiable {
    let id: String
    let label: String
    let usedFraction: Double?
    let resetsAt: Date?
}

struct ProviderSnapshot: Equatable, Identifiable {
    let id: ProviderID
    let displayName: String
    let fidelity: Fidelity
    let status: ProviderStatus
    let windows: [UsageWindow]
    let headlineID: String?
    let hasData: Bool

    var headline: UsageWindow? {
        if let headlineID, let match = windows.first(where: { $0.id == headlineID }) {
            return match
        }
        return windows.first
    }
}

struct LegacyProvidersView: Decodable {
    let providers: [ProviderID: LegacyUsageView]

    init(providers: [ProviderID: LegacyUsageView]) {
        self.providers = providers
    }

    init(from decoder: Decoder) throws {
        let root = try decoder.container(keyedBy: RootKey.self)
        let providerContainer = try root.nestedContainer(keyedBy: ProviderID.self, forKey: .providers)
        var providers: [ProviderID: LegacyUsageView] = [:]
        for provider in ProviderID.allCases where providerContainer.contains(provider) {
            providers[provider] = try providerContainer.decode(LegacyUsageView.self, forKey: provider)
        }
        self.providers = providers
    }
}

private enum RootKey: String, CodingKey {
    case providers
}

struct LegacyUsageView: Decodable {
    let windows: [LegacyWindow]
    let ageSeconds: Int
    let stale: Bool
    let hasData: Bool
}

struct LegacyWindow: Decodable {
    let id: String
    let usedPercentage: Double?
    let resetsAt: TimeInterval
}

extension LegacyProvidersView {
    func canonicalSnapshots() -> [ProviderSnapshot] {
        ProviderID.allCases.map { provider in
            let usage = providers[provider] ?? LegacyUsageView(windows: [], ageSeconds: 0, stale: false, hasData: false)
            let status: ProviderStatus = usage.stale ? .stale(ageSeconds: usage.ageSeconds) : .ok
            let windows = usage.windows.map { window in
                UsageWindow(
                    id: window.id,
                    label: window.label,
                    usedFraction: window.usedPercentage.map { min(max($0 / 100, 0), 1) },
                    resetsAt: Date(timeIntervalSince1970: window.resetsAt)
                )
            }
            return ProviderSnapshot(
                id: provider,
                displayName: provider.displayName,
                fidelity: .legacy,
                status: status,
                windows: windows,
                headlineID: windows.first?.id,
                hasData: usage.hasData
            )
        }
    }
}

private extension LegacyWindow {
    var label: String {
        switch id {
        case "five_hour": "5 horas"
        case "seven_day": "7 dias"
        default: id
        }
    }
}
