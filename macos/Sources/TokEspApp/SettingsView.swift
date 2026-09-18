import SwiftUI

struct SettingsView: View {
    @ObservedObject var store: UsageStore
    @ObservedObject var preferences: AppPreferences

    var body: some View {
        Form {
            Section("Fonte de dados") {
                LabeledContent("Modo") {
                    Text("Local")
                }
                LabeledContent("Estado") {
                    Text(store.sourceMessage)
                }
                Text("Claude e Codex são consultados diretamente a partir das sessões deste Mac. O backend antigo não é usado para alimentar o notch.")
                    .font(.caption)
                    .foregroundStyle(.secondary)
                Toggle("Usar dados de demonstração", isOn: $preferences.demoMode)
            }

            Section("Provedores") {
                ForEach(store.snapshots) { snapshot in
                    Toggle(snapshot.displayName, isOn: visibleBinding(for: snapshot.id))
                    Text(snapshot.status.label)
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
            }

            Section("Notch") {
                Picker("Borda", selection: $preferences.edge) {
                    ForEach(NotchEdge.allCases) { edge in
                        Text(edge.label).tag(edge)
                    }
                }
            }
        }
        .formStyle(.grouped)
        .frame(width: 460, height: 340)
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
}
