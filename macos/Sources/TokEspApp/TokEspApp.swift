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
            Button("Mostrar ou ocultar notch") {
                appDelegate.notch.toggleVisibility()
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
            SettingsView(store: appDelegate.store, preferences: appDelegate.preferences)
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
        notch.show()
        polling.start()
    }

    func applicationWillTerminate(_ notification: Notification) {
        polling.stop()
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
