import AppKit
import Combine
import SwiftUI

@MainActor
final class NotchPanelController: NSObject {
    private enum PanelSize {
        static let compact = NSSize(width: 328, height: 70)
        static let expanded = NSSize(width: 418, height: 210)
    }

    private let panel: NSPanel
    private let preferences: AppPreferences
    private var subscriptions = Set<AnyCancellable>()
    private var isExpanded = false

    init(store: UsageStore, preferences: AppPreferences) {
        self.preferences = preferences
        panel = NSPanel(
            contentRect: NSRect(origin: .zero, size: PanelSize.compact),
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
        panel.contentView = NSHostingView(
            rootView: NotchView(store: store, preferences: preferences) { [weak self] expanded in
                self?.setExpanded(expanded)
            }
        )
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

    private func setExpanded(_ expanded: Bool) {
        guard isExpanded != expanded else { return }
        isExpanded = expanded
        let size = expanded ? PanelSize.expanded : PanelSize.compact
        var frame = panel.frame
        frame.size = size
        panel.setFrame(frame, display: true, animate: true)
        position()
    }

    private func position() {
        guard let screen = NSScreen.main else { return }
        let screenFrame = screen.visibleFrame
        let size = panel.frame.size
        let y = preferences.edge == .top ? screenFrame.maxY - size.height : screenFrame.minY
        panel.setFrameOrigin(NSPoint(x: screenFrame.midX - size.width / 2, y: y))
    }
}

private struct NotchView: View {
    @ObservedObject var store: UsageStore
    @ObservedObject var preferences: AppPreferences
    let onExpansionChange: (Bool) -> Void

    @State private var isExpanded = false
    @State private var hoverExitTask: Task<Void, Never>?

    private var snapshots: [ProviderSnapshot] {
        store.snapshots.filter { preferences.isVisible($0.id) }
    }

    var body: some View {
        ZStack {
            AeroNotchShape()
                .fill(.ultraThinMaterial)
                .overlay { AeroNotchShape().fill(.black.opacity(0.63)) }
                .overlay(alignment: .bottom) { AeroBottomEdge().stroke(.black.opacity(0.95), lineWidth: 4) }

            if isExpanded {
                expandedContent
                    .transition(.opacity.combined(with: .move(edge: .top)))
            } else {
                compactContent
                    .transition(.opacity)
            }
        }
        .frame(width: isExpanded ? 418 : 328, height: isExpanded ? 210 : 70)
        .contentShape(AeroNotchShape())
        .onHover { hovering in
            hoverExitTask?.cancel()
            if hovering {
                setExpanded(true)
            } else {
                hoverExitTask = Task {
                    try? await Task.sleep(for: .milliseconds(240))
                    guard !Task.isCancelled else { return }
                    await MainActor.run { setExpanded(false) }
                }
            }
        }
        .onDisappear { hoverExitTask?.cancel() }
    }

    private var compactContent: some View {
        HStack(spacing: 9) {
            Text("Tokometer")
                .font(.subheadline.weight(.bold))
                .frame(width: 70, alignment: .leading)

            ForEach(snapshots) { snapshot in
                CompactUsage(snapshot: snapshot)
            }
        }
        .padding(.horizontal, 18)
        .foregroundStyle(.white)
    }

    private var expandedContent: some View {
        VStack(alignment: .leading, spacing: 10) {
            HStack {
                VStack(alignment: .leading, spacing: 1) {
                    Text("Tokometer")
                        .font(.headline.weight(.bold))
                    Text("Consumo das suas ferramentas")
                        .font(.caption)
                        .foregroundStyle(.white.opacity(0.55))
                }
                Spacer()
                Text("3 provedores")
                    .font(.caption2)
                    .foregroundStyle(.white.opacity(0.5))
            }

            ForEach(snapshots) { snapshot in
                ExpandedUsage(snapshot: snapshot)
            }
        }
        .padding(.horizontal, 24)
        .padding(.vertical, 18)
        .foregroundStyle(.white)
    }

    private func setExpanded(_ expanded: Bool) {
        guard isExpanded != expanded else { return }
        withAnimation(.easeOut(duration: 0.18)) {
            isExpanded = expanded
        }
        onExpansionChange(expanded)
    }
}

private struct AeroNotchShape: Shape {
    func path(in rect: CGRect) -> Path {
        let inset = min(22, rect.width * 0.08)
        let cornerRadius = min(12, rect.height * 0.22)
        var path = Path()
        path.move(to: CGPoint(x: 0, y: 0))
        path.addLine(to: CGPoint(x: rect.maxX, y: 0))
        path.addLine(to: CGPoint(x: rect.maxX - inset, y: rect.maxY - cornerRadius))
        path.addQuadCurve(
            to: CGPoint(x: rect.maxX - inset - cornerRadius, y: rect.maxY),
            control: CGPoint(x: rect.maxX - inset, y: rect.maxY)
        )
        path.addLine(to: CGPoint(x: rect.minX + inset + cornerRadius, y: rect.maxY))
        path.addQuadCurve(
            to: CGPoint(x: rect.minX + inset, y: rect.maxY - cornerRadius),
            control: CGPoint(x: rect.minX + inset, y: rect.maxY)
        )
        path.closeSubpath()
        return path
    }
}

private struct AeroBottomEdge: Shape {
    func path(in rect: CGRect) -> Path {
        let inset = min(22, rect.width * 0.08)
        let cornerRadius = min(12, rect.height * 0.22)
        var path = Path()
        path.move(to: CGPoint(x: rect.minX + inset + cornerRadius, y: rect.maxY - 1))
        path.addLine(to: CGPoint(x: rect.maxX - inset - cornerRadius, y: rect.maxY - 1))
        return path
    }
}

private struct CompactUsage: View {
    let snapshot: ProviderSnapshot

    var body: some View {
        HStack(spacing: 6) {
            UsageRing(snapshot: snapshot, diameter: 30, lineWidth: 3)
            Text(percent)
                .font(.caption2.monospacedDigit())
                .foregroundStyle(.white.opacity(0.62))
        }
    }

    private var percent: String {
        guard snapshot.hasData, let fraction = snapshot.headline?.usedFraction else { return "--" }
        return "\(Int((fraction * 100).rounded()))%"
    }
}

private struct ExpandedUsage: View {
    let snapshot: ProviderSnapshot

    private var fraction: Double { snapshot.headline?.usedFraction ?? 0 }

    var body: some View {
        HStack(spacing: 10) {
            UsageRing(snapshot: snapshot, diameter: 32, lineWidth: 3)
            VStack(alignment: .leading, spacing: 5) {
                HStack {
                    Text(snapshot.displayName)
                        .font(.subheadline.weight(.semibold))
                    Spacer()
                    Text(snapshot.status.label)
                        .font(.caption2)
                        .foregroundStyle(statusColor)
                }
                GeometryReader { geometry in
                    ZStack(alignment: .leading) {
                        Rectangle().fill(.white.opacity(0.12))
                        Rectangle()
                            .fill(statusColor)
                            .frame(width: geometry.size.width * max(0, min(fraction, 1)))
                    }
                }
                .frame(height: 4)
                Text(detail)
                    .font(.caption2)
                    .foregroundStyle(.white.opacity(0.55))
            }
        }
    }

    private var detail: String {
        guard snapshot.hasData, let headline = snapshot.headline else { return "Sem dados disponíveis" }
        guard let reset = headline.resetsAt else { return headline.label }
        return "\(headline.label) · reinicia \(reset.formatted(date: .abbreviated, time: .shortened))"
    }

    private var statusColor: Color {
        switch snapshot.status {
        case .ok: .mint
        case .stale: .orange
        case .needsAuth, .accessDenied, .unsupported, .error: .red
        }
    }
}

private struct UsageRing: View {
    let snapshot: ProviderSnapshot
    let diameter: CGFloat
    let lineWidth: CGFloat

    private var fraction: Double { snapshot.headline?.usedFraction ?? 0 }

    var body: some View {
        ZStack {
            Circle().stroke(.white.opacity(0.14), lineWidth: lineWidth)
            Circle()
                .trim(from: 0.025, to: max(0.035, fraction))
                .stroke(color, style: StrokeStyle(lineWidth: lineWidth, lineCap: .round))
                .rotationEffect(.degrees(-90))
            ProviderIcon(provider: snapshot.id, size: diameter * 0.36)
        }
        .frame(width: diameter, height: diameter)
    }

    private var color: Color {
        switch snapshot.status {
        case .ok: .mint
        case .stale: .orange
        case .needsAuth, .accessDenied, .unsupported, .error: .red
        }
    }

}

private struct ProviderIcon: View {
    let provider: ProviderID
    let size: CGFloat

    var body: some View {
        Image(systemName: symbolName)
            .font(.system(size: size, weight: .medium))
            .foregroundStyle(.white.opacity(0.84))
            .accessibilityLabel(provider.displayName)
    }

    private var symbolName: String {
        switch provider {
        case .claude: "asterisk"
        case .codex: "circle.hexagongrid.fill"
        case .cursor: "cursorarrow"
        }
    }
}
