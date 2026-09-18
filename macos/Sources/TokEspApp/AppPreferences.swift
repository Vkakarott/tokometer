import AppKit
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

    @Published var preferredScreenID: UInt32? {
        didSet {
            if let preferredScreenID {
                defaults.set(Int(preferredScreenID), forKey: Keys.preferredScreenID)
            } else {
                defaults.removeObject(forKey: Keys.preferredScreenID)
            }
        }
    }

    @Published private(set) var hasCompletedOnboarding: Bool {
        didSet { defaults.set(hasCompletedOnboarding, forKey: Keys.hasCompletedOnboarding) }
    }

    private let defaults: UserDefaults

    init(defaults: UserDefaults = .standard) {
        self.defaults = defaults
        edge = NotchEdge(rawValue: defaults.string(forKey: Keys.edge) ?? "") ?? .top
        let storedProviders = defaults.stringArray(forKey: Keys.visibleProviders) ?? ProviderID.allCases.map(\.rawValue)
        visibleProviders = Set(storedProviders.compactMap(ProviderID.init(rawValue:)))
        demoMode = defaults.bool(forKey: Keys.demoMode)
        preferredScreenID = (defaults.object(forKey: Keys.preferredScreenID) as? NSNumber)?.uint32Value
        hasCompletedOnboarding = defaults.bool(forKey: Keys.hasCompletedOnboarding)
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

    func completeOnboarding(with providers: Set<ProviderID>) {
        visibleProviders = providers
        hasCompletedOnboarding = true
    }
}

private enum Keys {
    static let edge = "notch.edge"
    static let visibleProviders = "notch.visibleProviders"
    static let demoMode = "app.demoMode"
    static let preferredScreenID = "tokonotch.preferredScreenID"
    static let hasCompletedOnboarding = "app.hasCompletedOnboarding"
}

extension NSScreen {
    var displayID: UInt32? {
        (deviceDescription[NSDeviceDescriptionKey("NSScreenNumber")] as? NSNumber)?.uint32Value
    }
}
