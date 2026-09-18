import AppKit
import SwiftUI
import Darwin

@main
struct TokEspApp: App {
    @NSApplicationDelegateAdaptor(AppDelegate.self) private var appDelegate

    var body: some Scene {
        MenuBarExtra("tokEsp", systemImage: "gauge.with.dots.needle.67percent") {
            Button("Atualizar agora") {
                Task { await appDelegate.polling.refresh() }
            }
            Button("Mostrar ou ocultar tokonotch") {
                appDelegate.notch.toggleVisibility()
            }
            Button("Configurar provedores…") {
                appDelegate.showOnboarding()
            }
            Divider()
            SettingsLink {
                Text("Configurações…")
            }
            Divider()
            Button("Encerrar tokEsp") {
                NSApp.terminate(nil)
            }
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
