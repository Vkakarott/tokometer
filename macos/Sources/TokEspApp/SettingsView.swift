import SwiftUI

struct SettingsView: View {
    @ObservedObject var store: UsageStore
    @ObservedObject var preferences: AppPreferences
    let showOnboarding: () -> Void
    @State private var pairingCode = ""
    @State private var pairingStatus: String?

    var body: some View {
        Form {
            Section("Fonte de dados") {
                LabeledContent("Modo") {
                    Text("Local")
                }
                LabeledContent("Estado") {
                    Text(store.sourceMessage)
                }
                Text("Claude, Codex e Cursor são consultados diretamente a partir das sessões deste Mac. O backend antigo não é usado para alimentar o tokonotch.")
                    .font(.caption)
                    .foregroundStyle(.secondary)
                Toggle("Usar dados de demonstração", isOn: $preferences.demoMode)
            }

            Section("Provedores") {
                Button("Reconfigurar provedores", action: showOnboarding)
                ForEach(store.snapshots) { snapshot in
                    Toggle(snapshot.displayName, isOn: visibleBinding(for: snapshot.id))
                    Text(snapshot.status.label)
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
            }

            Section("tokonotch") {
                Picker("Monitor", selection: preferredScreenBinding) {
                    ForEach(screens, id: \.displayID) { screen in
                        Text(screenLabel(for: screen)).tag(screen.displayID ?? 0)
                    }
                }
                Picker("Posição", selection: $preferences.tokonotchPosition) {
                    ForEach(TokonotchPosition.allCases) { position in
                        Text(position.label).tag(position)
                    }
                }
            }

            Section("Display") {
                TextField("Código do dispositivo", text: $pairingCode)
                Button("Parear display") {
                    Task { await approvePairing() }
                }
                if let pairingStatus {
                    Text(pairingStatus)
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
            }
        }
        .formStyle(.grouped)
        .frame(width: 460, height: 440)
        .padding()
        .onChange(of: preferences.demoMode, initial: true) { _, enabled in
            store.setDemoMode(enabled)
        }
    }

    private func visibleBinding(for provider: ProviderID) -> Binding<Bool> {
        Binding(
            get: { preferences.isVisible(provider) },
            set: { preferences.setVisible($0, for: provider) }
        )
    }

    private var screens: [NSScreen] {
        NSScreen.screens.filter { $0.displayID != nil }
    }

    private var primaryScreenID: UInt32 {
        screens.first?.displayID ?? 0
    }

    private var preferredScreenBinding: Binding<UInt32> {
        Binding(
            get: { preferences.preferredScreenID ?? primaryScreenID },
            set: { selected in
                preferences.preferredScreenID = selected == primaryScreenID ? nil : selected
            }
        )
    }

    private func screenLabel(for screen: NSScreen) -> String {
        guard screen.displayID != primaryScreenID else { return "Monitor principal" }
        return screen.localizedName
    }

    private func approvePairing() async {
        do {
            try await DevicePairingService().approve(code: pairingCode)
            pairingCode = ""
            pairingStatus = "Display pareado com sucesso."
        } catch {
            pairingStatus = error.localizedDescription
        }
    }
}
