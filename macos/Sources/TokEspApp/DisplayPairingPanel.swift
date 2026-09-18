import AppKit
import SwiftUI

@MainActor
final class DisplayPairingPanelController: NSObject {
    private let panel: NSPanel

    override init() {
        panel = NSPanel(
            contentRect: NSRect(x: 0, y: 0, width: 440, height: 430),
            styleMask: [.titled, .fullSizeContentView],
            backing: .buffered,
            defer: false
        )
        super.init()
        panel.title = "Conectar display"
        panel.isReleasedWhenClosed = false
        panel.center()
        panel.contentView = NSHostingView(rootView: DisplayPairingView())
    }

    func show() {
        panel.center()
        panel.makeKeyAndOrderFront(nil)
        NSApp.activate(ignoringOtherApps: true)
    }
}

private struct DisplayPairingView: View {
    @StateObject private var wifi = DisplayWifiConnector()
    @State private var pairingCode = ""
    @State private var pairingStatus: String?
    @State private var isPairing = false
    @FocusState private var isCodeFocused: Bool

    var body: some View {
        VStack(alignment: .leading, spacing: 20) {
            VStack(alignment: .leading, spacing: 6) {
                Text("Conectar display")
                    .font(.title2.weight(.semibold))
                Text("Configure a rede uma vez e aprove o display neste Mac.")
                    .foregroundStyle(.secondary)
            }

            VStack(alignment: .leading, spacing: 12) {
                Text("1. Conecte o Mac ao display")
                    .font(.headline)
                Text("O tokometer procura a rede tokEsp-xxxx e tenta conectar automaticamente. Depois abre a página de configuração do display.")
                    .font(.subheadline)
                    .foregroundStyle(.secondary)
                HStack {
                    Button(wifi.isConnectedToDisplay ? "Abrir configuração do display" : "Conectar ao display") {
                        wifi.connectOrOpenPortal()
                    }
                    .buttonStyle(.borderedProminent)
                    .disabled(wifi.isConnecting)
                    Button("Abrir Wi-Fi") {
                        wifi.openWifiSettings()
                    }
                    .buttonStyle(.bordered)
                }
                if !wifi.status.isEmpty {
                    Text(wifi.status)
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
            }

            Divider()

            Text("2. Confirme o código do display")
                .font(.headline)
            TextField("Código exibido no display", text: $pairingCode)
                .textFieldStyle(.roundedBorder)
                .focused($isCodeFocused)
                .onChange(of: pairingCode) { _, value in
                    pairingCode = value.uppercased().filter { $0.isLetter || $0.isNumber }
                }

            HStack {
                if let pairingStatus {
                    Text(pairingStatus)
                        .font(.caption)
                        .foregroundStyle(pairingStatus == "Display pareado com sucesso." ? .green : .red)
                }
                Spacer()
                Button("Parear display") {
                    Task { await approvePairing() }
                }
                .buttonStyle(.borderedProminent)
                .disabled(pairingCode.count != 8 || isPairing)
            }
        }
        .padding(28)
        .frame(width: 440, height: 430)
        .onAppear {
            isCodeFocused = true
            wifi.refresh()
        }
    }

    private func approvePairing() async {
        isPairing = true
        defer { isPairing = false }
        do {
            try await DevicePairingService().approve(code: pairingCode)
            pairingCode = ""
            pairingStatus = "Display pareado com sucesso."
        } catch {
            pairingStatus = error.localizedDescription
        }
    }
}
