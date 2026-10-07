import AppKit
import SwiftUI

@MainActor
final class DisplayPairingPanelController: NSObject {
    private let panel: NSPanel

    override init() {
        panel = NSPanel(
            contentRect: NSRect(x: 0, y: 0, width: 440, height: 380),
            styleMask: [.titled, .closable, .fullSizeContentView],
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
    @StateObject private var setup = DisplaySetup()
    @State private var ssid = ""
    @State private var password = ""

    var body: some View {
        VStack(alignment: .leading, spacing: 18) {
            VStack(alignment: .leading, spacing: 6) {
                Text("Conectar display")
                    .font(.title2.weight(.semibold))
                Text("Informe a rede Wi-Fi que o display deve usar. O tokometer envia a rede e pareia o display sozinho.")
                    .foregroundStyle(.secondary)
                    .fixedSize(horizontal: false, vertical: true)
            }

            VStack(alignment: .leading, spacing: 10) {
                TextField("Rede Wi-Fi", text: $ssid)
                SecureField("Senha", text: $password)
                    .onSubmit(start)
                Text("O display só usa redes de 2,4 GHz. O Mac sai da sua rede por alguns segundos durante o envio.")
                    .font(.caption)
                    .foregroundStyle(.secondary)
                    .fixedSize(horizontal: false, vertical: true)
            }
            .textFieldStyle(.roundedBorder)
            .disabled(setup.isWorking)

            HStack(spacing: 10) {
                SetupStatusView(phase: setup.phase)
                Spacer()
                Button("Conectar display", action: start)
                    .buttonStyle(.borderedProminent)
                    .disabled(setup.isWorking || ssid.trimmingCharacters(in: .whitespaces).isEmpty)
            }

            Divider()

            DisclosureGroup("Configurar manualmente") {
                ManualPairingView()
                    .padding(.top, 8)
            }
        }
        .padding(28)
        .frame(width: 440)
        .task {
            if ssid.isEmpty { ssid = await setup.suggestedSSID() }
        }
    }

    private func start() {
        guard !setup.isWorking, !ssid.trimmingCharacters(in: .whitespaces).isEmpty else { return }
        Task { await setup.run(ssid: ssid, password: password) }
    }
}

private struct SetupStatusView: View {
    let phase: DisplaySetup.Phase

    var body: some View {
        switch phase {
        case .idle:
            EmptyView()
        case .working(let message):
            ProgressView()
                .controlSize(.small)
            Text(message)
                .font(.caption)
                .foregroundStyle(.secondary)
        case .succeeded:
            Label("Display pareado com sucesso.", systemImage: "checkmark.circle.fill")
                .font(.caption)
                .foregroundStyle(.green)
        case .failed(let message):
            Text(message)
                .font(.caption)
                .foregroundStyle(.red)
                .fixedSize(horizontal: false, vertical: true)
        }
    }
}

/// The path that needs no automation: join the display's network by hand, use
/// its page, then type the code it shows.
private struct ManualPairingView: View {
    @State private var pairingCode = ""
    @State private var pairingStatus: String?
    @State private var paired = false
    @State private var isPairing = false

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            Text("Entre na rede tokEsp-xxxx, escolha a rede na página do display e depois digite o código que ele mostrar.")
                .font(.caption)
                .foregroundStyle(.secondary)
                .fixedSize(horizontal: false, vertical: true)
            HStack {
                Button("Abrir Wi-Fi") { open("x-apple.systempreferences:com.apple.wifi-settings-extension") }
                Button("Abrir página do display") { NSWorkspace.shared.open(DisplaySetupClient.portalURL) }
            }
            HStack {
                TextField("Código exibido no display", text: $pairingCode)
                    .textFieldStyle(.roundedBorder)
                    .onChange(of: pairingCode) { _, value in
                        pairingCode = value.uppercased().filter { $0.isLetter || $0.isNumber }
                    }
                Button("Parear") {
                    Task { await approvePairing() }
                }
                .disabled(pairingCode.count != 8 || isPairing)
            }
            if let pairingStatus {
                Text(pairingStatus)
                    .font(.caption)
                    .foregroundStyle(paired ? .green : .red)
            }
        }
    }

    private func open(_ address: String) {
        guard let url = URL(string: address) else { return }
        NSWorkspace.shared.open(url)
    }

    private func approvePairing() async {
        isPairing = true
        defer { isPairing = false }
        do {
            try await DevicePairingService().approve(code: pairingCode)
            pairingCode = ""
            paired = true
            pairingStatus = "Display pareado com sucesso."
        } catch {
            paired = false
            pairingStatus = error.localizedDescription
        }
    }
}
