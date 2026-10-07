import CoreLocation
import CoreWLAN

/// Moves this Mac between its own network and the display's setup network.
/// Every call blocks while CoreWLAN works, so run them off the main actor.
struct DisplayWifiSwitcher: Sendable {
    static let displayPrefix = "tokEsp-"
    private static let scanAttempts = 3

    func currentSSID() -> String? {
        CWWiFiClient.shared().interface()?.ssid()
    }

    /// Joins the strongest `tokEsp-xxxx` network and returns its name.
    func joinDisplayNetwork() throws -> String {
        guard let interface = CWWiFiClient.shared().interface() else {
            throw DisplaySetupError.wifiUnavailable
        }
        for _ in 0..<Self.scanAttempts {
            let networks = try interface.scanForNetworks(withName: nil)
            if !networks.isEmpty, networks.allSatisfy({ $0.ssid == nil }) {
                throw DisplaySetupError.locationRequired
            }
            let displays = networks.filter { $0.ssid?.hasPrefix(Self.displayPrefix) == true }
            guard let display = displays.max(by: { $0.rssiValue < $1.rssiValue }) else { continue }
            try interface.associate(to: display, password: nil)
            return display.ssid ?? Self.displayPrefix
        }
        throw DisplaySetupError.displayNotFound
    }

    /// Best effort: when it fails, macOS rejoins a known network by itself once
    /// the display's network disappears.
    func rejoin(ssid: String, password: String) {
        guard let interface = CWWiFiClient.shared().interface(),
              let network = try? interface.scanForNetworks(withName: ssid).first else { return }
        try? interface.associate(to: network, password: password.isEmpty ? nil : password)
    }
}

/// macOS hides Wi-Fi network names from apps without Location authorization.
@MainActor
final class LocationAccess: NSObject, CLLocationManagerDelegate {
    private let manager = CLLocationManager()
    private var pending: CheckedContinuation<Bool, Never>?

    override init() {
        super.init()
        manager.delegate = self
    }

    func ensureAuthorized() async -> Bool {
        switch manager.authorizationStatus {
        case .authorizedAlways:
            return true
        case .notDetermined:
            return await withCheckedContinuation { continuation in
                pending = continuation
                manager.requestWhenInUseAuthorization()
            }
        default:
            return false
        }
    }

    nonisolated func locationManagerDidChangeAuthorization(_ manager: CLLocationManager) {
        let status = manager.authorizationStatus
        Task { @MainActor in
            guard status != .notDetermined, let pending else { return }
            self.pending = nil
            pending.resume(returning: status == .authorizedAlways)
        }
    }
}
