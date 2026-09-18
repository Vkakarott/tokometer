import AppKit
import Combine
import SwiftUI

@MainActor
final class NotchPanelController: NSObject {
    private let panel: NSPanel
    private let preferences: AppPreferences
    private var subscriptions = Set<AnyCancellable>()

    init(store: UsageStore, preferences: AppPreferences) {
        self.preferences = preferences
        panel = NSPanel(
            contentRect: NSRect(x: 0, y: 0, width: 310, height: 180),
            styleMask: [.borderless, .nonactivatingPanel],
            backing: .buffered,
            defer: false
        )
        super.init()
        panel.isOpaque = false
        panel.backgroundColor = .clear
        panel.hasShadow = true
        panel.level = .statusBar
        panel.collectionBehavior = [.canJoinAllSpaces, .fullScreenAuxiliary]
        panel.contentView = NSHostingView(rootView: NotchView(store: store, preferences: preferences))
        store.objectWillChange
            .receive(on: RunLoop.main)
            .sink { [weak self] _ in self?.position() }
            .store(in: &subscriptions)
        preferences.$edge
            .receive(on: RunLoop.main)
            .sink { [weak self] _ in self?.position() }
            .store(in: &subscriptions)
    }

    func show() {
        position()
        panel.orderFrontRegardless()
    }

    func toggleVisibility() {
        panel.isVisible ? panel.orderOut(nil) : show()
    }

    private func position() {
        guard let screen = NSScreen.main else { return }
        let frame = screen.visibleFrame
        let size = panel.frame.size
        let y = preferences.edge == .top ? frame.maxY - size.height - 8 : frame.minY + 8
        panel.setFrameOrigin(NSPoint(x: frame.midX - size.width / 2, y: y))
    }
}

private struct NotchView: View {
    @ObservedObject var store: UsageStore
    @ObservedObject var preferences: AppPreferences
    @State private var expanded = false

    private var hasLiveProvider: Bool {
        store.snapshots.contains { $0.hasData && $0.status.isLive }
    }

    var body: some View {
        VStack(spacing: 8) {
            HStack(spacing: 8) {
                Image(systemName: "gauge.with.dots.needle.67percent")
                    .foregroundStyle(.mint)
                Text("tokEsp")
                    .font(.headline)
                Spacer()
                Text(hasLiveProvider ? "LOCAL" : "OFFLINE")
                    .font(.caption2.weight(.bold))
                    .foregroundStyle(hasLiveProvider ? .green : .orange)
            }
            ForEach(store.snapshots.filter { preferences.isVisible($0.id) }) { snapshot in
                ProviderRow(snapshot: snapshot, expanded: expanded)
            }
            if expanded {
                Text("Clique para recolher · dados locais do Mac")
                    .font(.caption2)
                    .foregroundStyle(.secondary)
            }
        }
        .padding(.horizontal, 14)
        .padding(.vertical, 12)
        .frame(width: 310)
        .background(.black.opacity(0.93), in: RoundedRectangle(cornerRadius: 18, style: .continuous))
        .foregroundStyle(.white)
        .contentShape(Rectangle())
        .onTapGesture { expanded.toggle() }
        .animation(.easeInOut(duration: 0.18), value: expanded)
    }
}

private struct ProviderRow: View {
    let snapshot: ProviderSnapshot
    let expanded: Bool

    var body: some View {
        HStack(spacing: 10) {
            Gauge(value: snapshot.headline?.usedFraction ?? 0, in: 0...1) {
                EmptyView()
            } currentValueLabel: {
                Text(percent)
                    .font(.caption2.monospacedDigit().weight(.bold))
            }
            .gaugeStyle(.accessoryCircularCapacity)
            .tint(tint)
            .frame(width: 36, height: 36)

            VStack(alignment: .leading, spacing: 2) {
                HStack {
                    Text(snapshot.displayName)
                        .font(.subheadline.weight(.semibold))
                    Spacer()
                    Text(snapshot.status.label)
                        .font(.caption2)
                        .foregroundStyle(statusColor)
                }
                if expanded {
                    Text(detail)
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
            }
        }
    }

    private var percent: String {
        guard snapshot.hasData, let fraction = snapshot.headline?.usedFraction else { return "--" }
        return Int((fraction * 100).rounded()).formatted()
    }

    private var detail: String {
        guard snapshot.hasData else { return "Sem dados disponíveis" }
        guard let headline = snapshot.headline else { return "Janela indisponível" }
        guard let reset = headline.resetsAt else { return "Reset desconhecido" }
        return "\(headline.label) · reinicia \(reset.formatted(date: .abbreviated, time: .shortened))"
    }

    private var tint: Color {
        switch snapshot.status {
        case .ok: .mint
        case .stale: .orange
        case .needsAuth, .accessDenied, .unsupported, .error: .red
        }
    }

    private var statusColor: Color {
        switch snapshot.status {
        case .ok: .green
        case .stale: .orange
        case .needsAuth, .accessDenied, .unsupported, .error: .red
        }
    }
}
