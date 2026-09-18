import AppKit
import SwiftUI

@MainActor
final class DisplayPairingPanelController: NSObject {
    private let panel: NSPanel

    override init() {
        panel = NSPanel(
            contentRect: NSRect(x: 0, y: 0, width: 440, height: 410),
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

            VStack(alignment: .leading, spacing: 14) {
                PairingStep(number: "1", text: "No display, conecte-se à rede tokEsp-xxxx e abra 192.168.4.1.")
                PairingStep(number: "2", text: "Escolha o Wi-Fi do local. O display mostrará um código ao conectar.")
                PairingStep(number: "3", text: "Digite o código abaixo para finalizar.")
            }

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
        .frame(width: 440, height: 410)
        .onAppear { isCodeFocused = true }
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

private struct PairingStep: View {
    let number: String
    let text: String

    var body: some View {
        HStack(alignment: .top, spacing: 10) {
            Text(number)
                .font(.caption.weight(.bold))
                .foregroundStyle(.secondary)
                .frame(width: 20, height: 20)
                .background(.quaternary, in: Circle())
            Text(text)
                .font(.subheadline)
                .fixedSize(horizontal: false, vertical: true)
        }
    }
}
