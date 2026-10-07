import Foundation

/// Runs the whole display setup: join its network, hand over the Wi-Fi, come
/// back and wait until the backend confirms the pairing.
@MainActor
final class DisplaySetup: ObservableObject {
    enum Phase: Equatable {
        case idle
        case working(String)
        case succeeded
        case failed(String)
    }

    @Published private(set) var phase: Phase = .idle

    private let wifi = DisplayWifiSwitcher()
    private let location = LocationAccess()
    private let display = DisplaySetupClient()
    private let pairing = DevicePairingService()

    private static let displayAnswerAttempts = 12
    private static let pairingTimeoutSeconds = 60

    var isWorking: Bool {
        if case .working = phase { return true }
        return false
    }

    /// The network this Mac is on, as a starting point for the form.
    func suggestedSSID() async -> String {
        guard await location.ensureAuthorized() else { return "" }
        let wifi = wifi
        let ssid = await Task.detached { wifi.currentSSID() }.value ?? ""
        return ssid.hasPrefix(DisplayWifiSwitcher.displayPrefix) ? "" : ssid
    }

    func run(ssid: String, password: String) async {
        do {
            try await configure(ssid: ssid.trimmingCharacters(in: .whitespaces), password: password)
            phase = .succeeded
        } catch {
            phase = .failed(error.localizedDescription)
        }
    }

    private func configure(ssid: String, password: String) async throws {
        phase = .working("Procurando o display...")
        guard await location.ensureAuthorized() else { throw DisplaySetupError.locationRequired }
        let wifi = wifi
        let previousSSID = await Task.detached { wifi.currentSSID() }.value
        _ = try await Task.detached { try wifi.joinDisplayNetwork() }.value

        phase = .working("Enviando a rede para o display...")
        let hardwareID = try await waitForDisplay()
        try await approve(hardwareID)
        try await display.sendWifi(ssid: ssid, password: password)

        phase = .working("Aguardando o display entrar na rede...")
        if previousSSID == ssid {
            await Task.detached { wifi.rejoin(ssid: ssid, password: password) }.value
        }
        try await waitUntilPaired(hardwareID)
    }

    /// The display answers a moment after the Mac gets its address.
    private func waitForDisplay() async throws -> String {
        for _ in 0..<Self.displayAnswerAttempts {
            if let hardwareID = try? await display.hardwareID() { return hardwareID }
            try await Task.sleep(for: .milliseconds(500))
        }
        throw DisplaySetupError.displayUnreachable
    }

    private func approve(_ hardwareID: String) async throws {
        do {
            try await pairing.approve(hardwareID: hardwareID)
        } catch {
            throw DisplaySetupError.backendUnavailable
        }
    }

    private func waitUntilPaired(_ hardwareID: String) async throws {
        for _ in 0..<Self.pairingTimeoutSeconds {
            if (try? await pairing.isPaired(hardwareID: hardwareID)) == true { return }
            try await Task.sleep(for: .seconds(1))
        }
        throw DisplaySetupError.notPaired
    }
}
