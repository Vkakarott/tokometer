import AppKit
import Combine
import SwiftUI

@MainActor
final class NotchPanelController: NSObject {
    private enum PanelSize {
        static let compact = NSSize(width: 142, height: 142)
        static let expanded = NSSize(width: 390, height: 290)
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
        let frame = screen.visibleFrame
        let size = panel.frame.size
        let y = preferences.edge == .top ? frame.maxY - size.height - 8 : frame.minY + 8
        panel.setFrameOrigin(NSPoint(x: frame.midX - size.width / 2, y: y))
    }
}

private struct NotchView: View {
    @ObservedObject var store: UsageStore
    @ObservedObject var preferences: AppPreferences
    let onExpansionChange: (Bool) -> Void

    @State private var isHovering = false
    @State private var rotation = 0.0
    @State private var isPulsing = false
    @State private var activeIndex = 0
    @State private var hoverExitTask: Task<Void, Never>?

    private var snapshots: [ProviderSnapshot] {
        store.snapshots.filter { preferences.isVisible($0.id) }
    }

    private var activeSnapshot: ProviderSnapshot? {
        guard !snapshots.isEmpty else { return nil }
        return snapshots[activeIndex % snapshots.count]
    }

    private var providerKey: String {
        snapshots.map(\.id.rawValue).joined(separator: ",")
    }

    var body: some View {
        ZStack {
            if isHovering {
                expandedConstellation
                    .transition(.opacity.combined(with: .scale(scale: 0.8)))
            } else {
                OrbCore(snapshot: activeSnapshot, rotation: rotation, isPulsing: isPulsing)
                    .transition(.opacity.combined(with: .scale(scale: 0.8)))
            }
        }
        .frame(width: isHovering ? 390 : 142, height: isHovering ? 290 : 142)
        .contentShape(Rectangle())
        .onHover { hovering in
            hoverExitTask?.cancel()
            if hovering {
                setExpanded(true)
            } else {
                hoverExitTask = Task {
                    try? await Task.sleep(for: .milliseconds(220))
                    guard !Task.isCancelled else { return }
                    await MainActor.run { setExpanded(false) }
                }
            }
        }
        .task(id: providerKey) {
            guard !snapshots.isEmpty else { return }
            while !Task.isCancelled {
                try? await Task.sleep(for: .seconds(3))
                guard !Task.isCancelled else { return }
                withAnimation(.easeInOut(duration: 0.45)) {
                    activeIndex = (activeIndex + 1) % max(snapshots.count, 1)
                }
            }
        }
        .onAppear {
            withAnimation(.linear(duration: 9).repeatForever(autoreverses: false)) {
                rotation = 360
            }
            withAnimation(.easeInOut(duration: 1.8).repeatForever(autoreverses: true)) {
                isPulsing = true
            }
        }
        .onDisappear { hoverExitTask?.cancel() }
    }

    private func setExpanded(_ expanded: Bool) {
        guard isHovering != expanded else { return }
        withAnimation(.spring(response: 0.35, dampingFraction: 0.78)) {
            isHovering = expanded
        }
        onExpansionChange(expanded)
    }

    private var expandedConstellation: some View {
        ZStack {
            Circle()
                .fill(Color.cyan.opacity(0.16))
                .blur(radius: 36)
                .frame(width: 180, height: 180)

            OrbCore(snapshot: activeSnapshot, rotation: rotation, isPulsing: isPulsing)
                .frame(width: 136, height: 136)

            ForEach(Array(snapshots.enumerated()), id: \.element.id) { index, snapshot in
                ProviderSatellite(snapshot: snapshot)
                    .offset(satelliteOffset(for: index, count: snapshots.count))
                    .transition(.scale.combined(with: .opacity))
            }
        }
    }

    private func satelliteOffset(for index: Int, count: Int) -> CGSize {
        switch count {
        case 1:
            .init(width: 0, height: 90)
        case 2:
            index == 0 ? .init(width: -112, height: 58) : .init(width: 112, height: 58)
        default:
            switch index {
            case 0: .init(width: -116, height: -66)
            case 1: .init(width: 116, height: -66)
            default: .init(width: 0, height: 90)
            }
        }
    }
}

private struct OrbCore: View {
    let snapshot: ProviderSnapshot?
    let rotation: Double
    let isPulsing: Bool

    private var fraction: Double {
        snapshot?.headline?.usedFraction ?? 0
    }

    var body: some View {
        ZStack {
            Circle()
                .fill(
                    RadialGradient(
                        colors: [Color(red: 0.05, green: 0.2, blue: 0.24), Color.black.opacity(0.96)],
                        center: .center,
                        startRadius: 4,
                        endRadius: 82
                    )
                )

            Circle()
                .stroke(Color.white.opacity(0.1), lineWidth: 7)

            Circle()
                .trim(from: 0.025, to: max(0.035, fraction))
                .stroke(
                    AngularGradient(colors: [.cyan, .mint, .teal, .cyan], center: .center),
                    style: StrokeStyle(lineWidth: 7, lineCap: .round)
                )
                .rotationEffect(.degrees(rotation - 90))
                .shadow(color: .cyan.opacity(0.75), radius: isPulsing ? 12 : 6)

            VStack(spacing: 3) {
                Text(percent)
                    .font(.system(size: 29, weight: .bold, design: .rounded).monospacedDigit())
                    .contentTransition(.numericText())
                Text(snapshot?.displayName ?? "tokEsp")
                    .font(.caption.weight(.semibold))
                    .foregroundStyle(.white.opacity(0.72))
                    .lineLimit(1)
            }
            .id(snapshot?.id)
            .transition(.opacity.combined(with: .scale(scale: 0.88)))
        }
        .frame(width: 122, height: 122)
        .scaleEffect(isPulsing ? 1.015 : 0.985)
        .accessibilityElement(children: .ignore)
        .accessibilityLabel("Uso de \(snapshot?.displayName ?? "tokEsp"): \(percent)")
    }

    private var percent: String {
        guard snapshot?.hasData == true else { return "--" }
        return "\(Int((fraction * 100).rounded()))%"
    }
}

private struct ProviderSatellite: View {
    let snapshot: ProviderSnapshot

    private var fraction: Double { snapshot.headline?.usedFraction ?? 0 }

    var body: some View {
        VStack(spacing: 6) {
            ZStack {
                Circle()
                    .fill(.black.opacity(0.88))
                Circle()
                    .stroke(Color.white.opacity(0.12), lineWidth: 4)
                Circle()
                    .trim(from: 0.025, to: max(0.035, fraction))
                    .stroke(statusColor, style: StrokeStyle(lineWidth: 4, lineCap: .round))
                    .rotationEffect(.degrees(-90))
                Text(percent)
                    .font(.caption.weight(.bold).monospacedDigit())
            }
            .frame(width: 58, height: 58)
            .shadow(color: statusColor.opacity(0.45), radius: 9)

            VStack(spacing: 1) {
                Text(snapshot.displayName)
                    .font(.caption.weight(.bold))
                Text(snapshot.headline?.label ?? snapshot.status.label)
                    .font(.caption2)
                    .foregroundStyle(.white.opacity(0.58))
                    .lineLimit(1)
            }
        }
        .foregroundStyle(.white)
        .frame(width: 104)
    }

    private var percent: String {
        guard snapshot.hasData else { return "--" }
        return "\(Int((fraction * 100).rounded()))%"
    }

    private var statusColor: Color {
        switch snapshot.status {
        case .ok: .mint
        case .stale: .orange
        case .needsAuth, .accessDenied, .unsupported, .error: .red
        }
    }
}
