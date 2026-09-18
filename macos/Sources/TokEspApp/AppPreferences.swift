import Combine
import Foundation

enum NotchEdge: String, CaseIterable, Identifiable {
    case top
    case bottom

    var id: String { rawValue }

    var label: String {
        switch self {
        case .top: "Superior"
        case .bottom: "Inferior"
        }
    }
}

@MainActor
final class AppPreferences: ObservableObject {
    @Published var edge: NotchEdge {
        didSet { defaults.set(edge.rawValue, forKey: Keys.edge) }
    }

    @Published var visibleProviders: Set<ProviderID> {
        didSet { defaults.set(visibleProviders.map(\.rawValue), forKey: Keys.visibleProviders) }
    }

    @Published var demoMode: Bool {
        didSet { defaults.set(demoMode, forKey: Keys.demoMode) }
    }

    private let defaults: UserDefaults

    init(defaults: UserDefaults = .standard) {
        self.defaults = defaults
        edge = NotchEdge(rawValue: defaults.string(forKey: Keys.edge) ?? "") ?? .top
        let storedProviders = defaults.stringArray(forKey: Keys.visibleProviders) ?? ProviderID.allCases.map(\.rawValue)
        visibleProviders = Set(storedProviders.compactMap(ProviderID.init(rawValue:)))
        demoMode = defaults.bool(forKey: Keys.demoMode)
    }

    func isVisible(_ provider: ProviderID) -> Bool {
        visibleProviders.contains(provider)
    }

    func setVisible(_ visible: Bool, for provider: ProviderID) {
        if visible {
            visibleProviders.insert(provider)
        } else {
            visibleProviders.remove(provider)
        }
    }
}

private enum Keys {
    static let edge = "notch.edge"
    static let visibleProviders = "notch.visibleProviders"
    static let demoMode = "app.demoMode"
}
