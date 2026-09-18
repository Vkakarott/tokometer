import AppKit
import CoreWLAN

@MainActor
final class DisplayWifiConnector: ObservableObject {
    @Published private(set) var status = ""
    @Published private(set) var isConnecting = false

    var isConnectedToDisplay: Bool {
        currentSSID?.hasPrefix("tokEsp-") == true
    }

    func connectOrOpenPortal() {
        if isConnectedToDisplay {
            openPortal()
            return
        }

        guard let interface = CWWiFiClient.shared().interface() else {
            status = "Não foi possível acessar o Wi-Fi deste Mac."
            return
        }

        isConnecting = true
        defer { isConnecting = false }
        do {
            let networks = try interface.scanForNetworks(withName: nil)
            guard let displayNetwork = networks.first(where: { $0.ssid?.hasPrefix("tokEsp-") == true }) else {
                status = "Nenhuma rede tokEsp foi encontrada. Ligue o display e tente novamente."
                return
            }
            try interface.associate(to: displayNetwork, password: nil)
            status = "Conectado a \(displayNetwork.ssid ?? "tokEsp"). Abrindo a configuração..."
            openPortal()
        } catch {
            status = "O macOS não conectou automaticamente. Abra o Wi-Fi e escolha a rede tokEsp-xxxx."
        }
    }

    func openWifiSettings() {
        guard let url = URL(string: "x-apple.systempreferences:com.apple.wifi-settings-extension") else { return }
        NSWorkspace.shared.open(url)
    }

    func refresh() {
        status = isConnectedToDisplay ? "Conectado ao display. Abra a configuração para escolher a rede Wi-Fi." : ""
    }

    private var currentSSID: String? {
        CWWiFiClient.shared().interface()?.ssid()
    }

    private func openPortal() {
        guard let url = URL(string: "http://192.168.4.1") else { return }
        NSWorkspace.shared.open(url)
    }
}
