import AppKit
import Combine
import SwiftUI

@MainActor
final class NotchPanelController: NSObject {
    private enum PanelSize {
        static let compact = NSSize(width: 328, height: 62)
        static let expanded = NSSize(width: 328, height: 136)
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
        panel.hasShadow = false
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
        preferences.$tokonotchPosition
            .receive(on: RunLoop.main)
            .sink { [weak self] _ in self?.position() }
            .store(in: &subscriptions)
        preferences.$preferredScreenID
            .receive(on: RunLoop.main)
            .sink { [weak self] _ in self?.position() }
            .store(in: &subscriptions)
        NotificationCenter.default.publisher(for: NSApplication.didChangeScreenParametersNotification)
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
        guard let screen = targetScreen() else { return }
        let screenFrame = positioningFrame(for: screen)
        let y = preferences.tokonotchPosition.edge == .top ? screenFrame.maxY - size.height : screenFrame.minY
        let frame = NSRect(
            x: horizontalOrigin(in: screenFrame, width: size.width),
            y: y,
            width: size.width,
            height: size.height
        )
        NSAnimationContext.runAnimationGroup { context in
            context.duration = 0.22
            context.timingFunction = CAMediaTimingFunction(name: .easeInEaseOut)
            panel.animator().setFrame(frame, display: true)
        }
    }

    private func position() {
        guard let screen = targetScreen() else { return }
        let screenFrame = positioningFrame(for: screen)
        let size = panel.frame.size
        let y = preferences.tokonotchPosition.edge == .top ? screenFrame.maxY - size.height : screenFrame.minY
        panel.setFrameOrigin(NSPoint(x: horizontalOrigin(in: screenFrame, width: size.width), y: y))
    }

    private func targetScreen() -> NSScreen? {
        if let displayID = preferences.preferredScreenID,
           let selected = NSScreen.screens.first(where: { $0.displayID == displayID }) {
            return selected
        }
        return NSScreen.screens.first
    }

    private func positioningFrame(for screen: NSScreen) -> NSRect {
        guard preferences.tokonotchPosition.edge == .top,
              screen.displayID != NSScreen.screens.first?.displayID else {
            return screen.visibleFrame
        }
        return screen.frame
    }

    private func horizontalOrigin(in frame: NSRect, width: CGFloat) -> CGFloat {
        switch preferences.tokonotchPosition.horizontalPlacement {
        case .leading: frame.minX
        case .center: frame.midX - width / 2
        case .trailing: frame.maxX - width
        }
    }
}

private struct NotchView: View {
    @ObservedObject var store: UsageStore
    @ObservedObject var preferences: AppPreferences
    let onExpansionChange: (Bool) -> Void

    @State private var expandedProvider: ProviderID?
    @State private var hoverExitTask: Task<Void, Never>?

    private var snapshots: [ProviderSnapshot] {
        store.snapshots.filter { preferences.isVisible($0.id) }
    }

    private var expandedSnapshot: ProviderSnapshot? {
        guard let expandedProvider else { return nil }
        return snapshots.first { $0.id == expandedProvider }
    }

    var body: some View {
        ZStack {
            AeroNotchShape(position: preferences.tokonotchPosition)
                .fill(.black.opacity(0.98))

            if let expandedSnapshot {
                providerDrop(snapshot: expandedSnapshot)
                    .transition(.opacity)
            } else {
                compactContent
                    .transition(.opacity)
            }
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
        .contentShape(AeroNotchShape(position: preferences.tokonotchPosition))
        .animation(.easeOut(duration: 0.12), value: expandedProvider)
        .onHover { hovering in
            hoverExitTask?.cancel()
            if !hovering {
                hoverExitTask = Task {
                    try? await Task.sleep(for: .milliseconds(240))
                    guard !Task.isCancelled else { return }
                    await MainActor.run { closeDrop() }
                }
            }
        }
        .onDisappear { hoverExitTask?.cancel() }
    }

    private var compactContent: some View {
        HStack(spacing: 14) {
            ForEach(snapshots) { snapshot in
                CompactUsage(snapshot: snapshot) {
                    openDrop(for: snapshot.id)
                }
            }
        }
        .padding(.horizontal, 18)
        .foregroundStyle(.white)
    }

    private func providerDrop(snapshot: ProviderSnapshot) -> some View {
        ExpandedUsage(snapshot: snapshot)
            .padding(.horizontal, 30)
            .padding(.vertical, 10)
        .foregroundStyle(.white)
    }

    private func openDrop(for provider: ProviderID) {
        hoverExitTask?.cancel()
        guard expandedProvider != provider else { return }
        expandedProvider = provider
        onExpansionChange(true)
    }

    private func closeDrop() {
        guard expandedProvider != nil else { return }
        expandedProvider = nil
        onExpansionChange(false)
    }
}

private struct AeroNotchShape: Shape {
    let position: TokonotchPosition

    func path(in rect: CGRect) -> Path {
        let inset = min(20, rect.width * 0.07)
        let cornerRadius = min(16, rect.height * 0.32)
        let slopeHeight = rect.height - cornerRadius
        let slopeLength = sqrt(inset * inset + slopeHeight * slopeHeight)
        let controlDistance = min(12, slopeLength * 0.22)

        let rightCurveStart = CGPoint(x: rect.maxX - inset, y: rect.maxY - cornerRadius)
        let rightCurveEnd = CGPoint(x: rect.maxX - inset - cornerRadius, y: rect.maxY)
        let leftCurveStart = CGPoint(x: rect.minX + inset, y: rect.maxY - cornerRadius)
        let leftCurveEnd = CGPoint(x: rect.minX + inset + cornerRadius, y: rect.maxY)

        let rightControl1 = CGPoint(
            x: rightCurveStart.x - (inset / slopeLength * controlDistance),
            y: rightCurveStart.y + (slopeHeight / slopeLength * controlDistance)
        )
        let rightControl2 = CGPoint(x: rightCurveEnd.x + controlDistance, y: rightCurveEnd.y)
        let leftControl1 = CGPoint(
            x: leftCurveStart.x + (inset / slopeLength * controlDistance),
            y: leftCurveStart.y + (slopeHeight / slopeLength * controlDistance)
        )
        let leftControl2 = CGPoint(x: leftCurveEnd.x - controlDistance, y: leftCurveEnd.y)

        var path = Path()
        switch position.horizontalPlacement {
        case .leading:
            path.move(to: CGPoint(x: rect.minX, y: rect.minY))
            path.addLine(to: CGPoint(x: rect.maxX, y: rect.minY))
            path.addLine(to: rightCurveStart)
            path.addCurve(to: rightCurveEnd, control1: rightControl1, control2: rightControl2)
            path.addLine(to: CGPoint(x: rect.minX, y: rect.maxY))
        case .center:
            path.move(to: CGPoint(x: rect.minX, y: rect.minY))
            path.addLine(to: CGPoint(x: rect.maxX, y: rect.minY))
            path.addLine(to: rightCurveStart)
            path.addCurve(to: rightCurveEnd, control1: rightControl1, control2: rightControl2)
            path.addLine(to: leftCurveEnd)
            path.addCurve(to: leftCurveStart, control1: leftControl2, control2: leftControl1)
        case .trailing:
            path.move(to: CGPoint(x: rect.minX, y: rect.minY))
            path.addLine(to: CGPoint(x: rect.maxX, y: rect.minY))
            path.addLine(to: CGPoint(x: rect.maxX, y: rect.maxY))
            path.addLine(to: leftCurveEnd)
            path.addCurve(to: leftCurveStart, control1: leftControl2, control2: leftControl1)
        }
        path.closeSubpath()
        guard position.edge == .bottom else { return path }
        return path.applying(CGAffineTransform(translationX: 0, y: rect.height).scaledBy(x: 1, y: -1))
    }
}

private struct CompactUsage: View {
    let snapshot: ProviderSnapshot
    let onHover: () -> Void

    var body: some View {
        HStack(spacing: 6) {
            UsageRing(snapshot: snapshot, diameter: 30, lineWidth: 3)
            Text(percent)
                .font(.caption2.monospacedDigit())
                .foregroundStyle(.white.opacity(0.62))
        }
        .contentShape(Rectangle())
        .onHover { hovering in
            if hovering {
                onHover()
            }
        }
    }

    private var percent: String {
        guard snapshot.hasData, let fraction = snapshot.headline?.usedFraction else { return "--" }
        return "\(Int((fraction * 100).rounded()))%"
    }
}

private struct ExpandedUsage: View {
    let snapshot: ProviderSnapshot

    var body: some View {
        HStack(spacing: 10) {
            UsageRing(snapshot: snapshot, diameter: 32, lineWidth: 3)
            VStack(alignment: .leading, spacing: 7) {
                HStack {
                    Text(snapshot.displayName)
                        .font(.subheadline.weight(.semibold))
                    Spacer()
                }
                if snapshot.hasData {
                    ForEach(snapshot.windows) { window in
                        UsageLimit(window: window, color: accentColor)
                    }
                } else {
                    Text("Sem dados disponíveis")
                        .font(.caption2)
                        .foregroundStyle(.white.opacity(0.55))
                }
            }
        }
    }

    private var accentColor: Color {
        ProviderAccent.color(for: snapshot)
    }
}

private struct UsageLimit: View {
    let window: UsageWindow
    let color: Color

    private var fraction: Double { window.usedFraction ?? 0 }

    var body: some View {
        VStack(spacing: 4) {
            HStack {
                Text(window.label)
                    .font(.caption2)
                    .foregroundStyle(.white.opacity(0.58))
                Spacer()
                Text("\(Int((fraction * 100).rounded()))%")
                    .font(.caption2.monospacedDigit().weight(.medium))
                    .foregroundStyle(.white.opacity(0.78))
            }
            GeometryReader { geometry in
                ZStack(alignment: .leading) {
                    Capsule().fill(.white.opacity(0.12))
                    Capsule()
                        .fill(color)
                        .frame(width: geometry.size.width * max(0, min(fraction, 1)))
                }
            }
            .frame(height: 3)
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
        ProviderAccent.color(for: snapshot)
    }

}

private enum ProviderAccent {
    static func color(for snapshot: ProviderSnapshot) -> Color {
        switch snapshot.status {
        case .needsAuth, .accessDenied, .unsupported, .error:
            .red
        case .ok, .stale:
            switch snapshot.id {
            case .claude: Color(red: 0.93, green: 0.34, blue: 0.12)
            case .codex: Color(red: 0.18, green: 0.82, blue: 0.43)
            case .cursor: .white
            }
        }
    }
}

struct ProviderIcon: View {
    let provider: ProviderID
    let size: CGFloat

    var body: some View {
        if let image = NSImage(contentsOf: iconURL) {
            Image(nsImage: image)
                .resizable()
                .interpolation(.high)
                .scaledToFit()
                .frame(width: size, height: size)
                .accessibilityLabel(provider.displayName)
        }
    }

    private var iconURL: URL {
        Bundle.module.url(forResource: provider.rawValue, withExtension: "png")!
    }
}
