import AppKit
import SwiftUI
import Darwin

@main
struct TokEspApp: App {
    @NSApplicationDelegateAdaptor(AppDelegate.self) private var appDelegate

    var body: some Scene {
        MenuBarExtra("tokEsp", systemImage: "gauge.with.dots.needle.67percent") {
            VStack(spacing: 3) {
                MenuActionRow(title: "Atualizar agora", symbol: "arrow.clockwise") {
                    Task { await appDelegate.polling.refresh() }
                }
                MenuActionRow(title: "Mostrar ou ocultar tokonotch", symbol: "eye") {
                    appDelegate.notch.toggleVisibility()
                }
                MenuActionRow(title: "Configurar provedores", symbol: "slider.horizontal.3") {
                    appDelegate.showOnboarding()
                }

                Divider()
                    .padding(.vertical, 4)

                SettingsLink {
                    MenuRowLabel(title: "Configurações", symbol: "gearshape")
                }
                .buttonStyle(.plain)

                MenuActionRow(title: "Encerrar tokEsp", symbol: "power") {
                    NSApp.terminate(nil)
                }
            }
            .padding(7)
            .frame(width: 252)
        }
        .menuBarExtraStyle(.window)

        Settings {
            SettingsView(
                store: appDelegate.store,
                preferences: appDelegate.preferences,
                showOnboarding: appDelegate.showOnboarding
            )
        }
    }
}

private struct MenuActionRow: View {
    let title: String
    let symbol: String
    let action: () -> Void

    var body: some View {
        Button(action: action) {
            MenuRowLabel(title: title, symbol: symbol)
        }
        .buttonStyle(.plain)
    }
}

private struct MenuRowLabel: View {
    let title: String
    let symbol: String
    @State private var isHovering = false

    var body: some View {
        HStack(spacing: 10) {
            Image(systemName: symbol)
                .frame(width: 16)
                .foregroundStyle(.secondary)
            Text(title)
                .font(.subheadline)
            Spacer()
        }
        .padding(.horizontal, 10)
        .frame(height: 32)
        .contentShape(RoundedRectangle(cornerRadius: 7))
        .background(isHovering ? .white.opacity(0.1) : .clear, in: RoundedRectangle(cornerRadius: 7))
        .onHover { isHovering = $0 }
    }
}

@MainActor
final class AppDelegate: NSObject, NSApplicationDelegate {
    let store = UsageStore()
    let preferences = AppPreferences()
    lazy var polling = NativePollingController(
        providers: [ClaudeUsageProvider(), CodexUsageProvider(), CursorUsageProvider()],
        store: store
    )
    lazy var notch = NotchPanelController(store: store, preferences: preferences)
    lazy var onboarding = OnboardingPanelController(
        store: store,
        preferences: preferences,
        refresh: { [weak self] in await self?.polling.refresh() },
        finish: { [weak self] in self?.notch.show() }
    )
    private let instanceLock = SingleInstanceLock()
    private var isPrimaryInstance = false

    func applicationWillFinishLaunching(_ notification: Notification) {
        isPrimaryInstance = instanceLock.acquire()
        if !isPrimaryInstance {
            NSApp.terminate(nil)
        }
    }

    func applicationDidFinishLaunching(_ notification: Notification) {
        guard isPrimaryInstance else { return }
        store.setDemoMode(preferences.demoMode)
        polling.start()
        if preferences.hasCompletedOnboarding {
            notch.show()
        } else {
            onboarding.show()
        }
    }

    func applicationWillTerminate(_ notification: Notification) {
        polling.stop()
    }

    func showOnboarding() {
        onboarding.show()
    }
}

private final class SingleInstanceLock {
    private var descriptor: Int32 = -1

    deinit {
        if descriptor >= 0 { close(descriptor) }
    }

    func acquire() -> Bool {
        let path = (NSTemporaryDirectory() as NSString).appendingPathComponent("com.tokesp.app.lock")
        descriptor = open(path, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR)
        guard descriptor >= 0 else { return true }
        return flock(descriptor, LOCK_EX | LOCK_NB) == 0
    }
}
