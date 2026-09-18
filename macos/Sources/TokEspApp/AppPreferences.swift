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

enum TokonotchPosition: String, CaseIterable, Identifiable {
    case topLeading
    case topCenter
    case topTrailing
    case bottomLeading
    case bottomCenter
    case bottomTrailing

    var id: String { rawValue }

    var label: String {
        switch self {
        case .topLeading: "Superior — esquerda"
        case .topCenter: "Superior — centro"
        case .topTrailing: "Superior — direita"
        case .bottomLeading: "Inferior — esquerda"
        case .bottomCenter: "Inferior — centro"
        case .bottomTrailing: "Inferior — direita"
        }
    }

    var edge: NotchEdge {
        switch self {
        case .topLeading, .topCenter, .topTrailing: .top
        case .bottomLeading, .bottomCenter, .bottomTrailing: .bottom
        }
    }

    var horizontalPlacement: HorizontalPlacement {
        switch self {
        case .topLeading, .bottomLeading: .leading
        case .topCenter, .bottomCenter: .center
        case .topTrailing, .bottomTrailing: .trailing
        }
    }
}

enum HorizontalPlacement {
    case leading
    case center
    case trailing
}

@MainActor
final class AppPreferences: ObservableObject {
    @Published var tokonotchPosition: TokonotchPosition {
        didSet { defaults.set(tokonotchPosition.rawValue, forKey: Keys.tokonotchPosition) }
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
        if let storedPosition = defaults.string(forKey: Keys.tokonotchPosition),
           let position = TokonotchPosition(rawValue: storedPosition) {
            tokonotchPosition = position
        } else {
            tokonotchPosition = NotchEdge(rawValue: defaults.string(forKey: Keys.edge) ?? "") == .bottom ? .bottomCenter : .topCenter
        }
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
    static let tokonotchPosition = "tokonotch.position"
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
