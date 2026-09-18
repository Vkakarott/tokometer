import AppKit
import SwiftUI

@MainActor
final class OnboardingPanelController: NSObject {
    private let panel: NSPanel

    init(
        store: UsageStore,
        preferences: AppPreferences,
        refresh: @escaping @MainActor () async -> Void,
        finish: @escaping @MainActor () -> Void
    ) {
        panel = NSPanel(
            contentRect: NSRect(x: 0, y: 0, width: 520, height: 440),
            styleMask: [.titled, .fullSizeContentView],
            backing: .buffered,
            defer: false
        )
        super.init()
        panel.title = "Configurar provedores"
        panel.isReleasedWhenClosed = false
        panel.center()
        panel.contentView = NSHostingView(
            rootView: OnboardingView(store: store, preferences: preferences, refresh: refresh) { [weak panel] in
                panel?.orderOut(nil)
                finish()
            }
        )
    }

    func show() {
        panel.center()
        panel.makeKeyAndOrderFront(nil)
        NSApp.activate(ignoringOtherApps: true)
    }
}

private struct OnboardingView: View {
    @ObservedObject var store: UsageStore
    @ObservedObject var preferences: AppPreferences
    let refresh: @MainActor () async -> Void
    let finish: () -> Void

    @State private var selectedProviders = Set<ProviderID>()
    @State private var hasSelectedDetectedProviders = false
    @State private var isRefreshing = false

    var body: some View {
        VStack(alignment: .leading, spacing: 20) {
            VStack(alignment: .leading, spacing: 6) {
                Text("Escolha o que acompanhar")
                    .font(.title2.weight(.semibold))
                Text("O tokEsp procura sessões já conectadas neste Mac. Seus dados permanecem locais e são consultados diretamente nos provedores.")
                    .foregroundStyle(.secondary)
            }

            VStack(spacing: 10) {
                ForEach(store.snapshots) { snapshot in
                    ProviderSetupRow(
                        snapshot: snapshot,
                        isSelected: selectionBinding(for: snapshot.id)
                    )
                }
            }

            HStack {
                Button("Verificar novamente") {
                    Task {
                        isRefreshing = true
                        await refresh()
                        isRefreshing = false
                    }
                }
                .disabled(isRefreshing)

                Spacer()

                Button("Configurar depois") {
                    finishOnboarding(with: [])
                }

                Button("Continuar") {
                    finishOnboarding(with: selectedProviders)
                }
                .buttonStyle(.borderedProminent)
            }
        }
        .padding(28)
        .frame(width: 520, height: 440)
        .onAppear { selectDetectedProviders(from: store.snapshots) }
        .onChange(of: store.snapshots) { _, snapshots in
            selectDetectedProviders(from: snapshots)
        }
    }

    private func selectionBinding(for provider: ProviderID) -> Binding<Bool> {
        Binding(
            get: { selectedProviders.contains(provider) },
            set: { selected in
                if selected {
                    selectedProviders.insert(provider)
                } else {
                    selectedProviders.remove(provider)
                }
            }
        )
    }

    private func selectDetectedProviders(from snapshots: [ProviderSnapshot]) {
        guard !hasSelectedDetectedProviders else { return }
        let detected = Set(snapshots.filter(\.hasData).map(\.id))
        guard !detected.isEmpty else { return }
        selectedProviders = detected
        hasSelectedDetectedProviders = true
    }

    private func finishOnboarding(with providers: Set<ProviderID>) {
        preferences.completeOnboarding(with: providers)
        finish()
    }
}

private struct ProviderSetupRow: View {
    let snapshot: ProviderSnapshot
    @Binding var isSelected: Bool

    var body: some View {
        Toggle(isOn: $isSelected) {
            HStack(spacing: 12) {
                ProviderIcon(provider: snapshot.id, size: 22)
                    .frame(width: 26, height: 26)
                VStack(alignment: .leading, spacing: 2) {
                    Text(snapshot.displayName)
                        .font(.headline)
                    Text(detail)
                        .font(.caption)
                        .foregroundStyle(snapshot.hasData ? Color.secondary : Color.orange)
                }
            }
        }
        .toggleStyle(.switch)
        .disabled(!snapshot.hasData)
        .padding(14)
        .background(.quaternary, in: RoundedRectangle(cornerRadius: 12))
    }

    private var detail: String {
        if snapshot.hasData {
            return "Pronto para aparecer no tokonotch"
        }
        switch snapshot.status {
        case .needsAuth:
            return "Entre na sua conta e verifique novamente"
        case .unsupported:
            return "Esta conta não informou um limite disponível"
        default:
            return "Ainda não foi possível conectar neste Mac"
        }
    }
}
