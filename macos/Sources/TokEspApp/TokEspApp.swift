import AppKit
import SwiftUI

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
        providers: [ClaudeUsageProvider(), CodexUsageProvider()],
        store: store
    )
    lazy var notch = NotchPanelController(store: store, preferences: preferences)

    func applicationDidFinishLaunching(_ notification: Notification) {
        store.setDemoMode(preferences.demoMode)
        notch.show()
        polling.start()
    }

    func applicationWillTerminate(_ notification: Notification) {
        polling.stop()
    }
}
