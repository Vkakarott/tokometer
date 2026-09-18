import Combine
import Foundation

@MainActor
final class UsageStore: ObservableObject {
    @Published private(set) var snapshots: [ProviderSnapshot]
    @Published private(set) var updatedAt: Date?
    @Published private(set) var sourceMessage = "Aguardando serviço local"
    @Published private(set) var isDemoMode = false

    init() {
        snapshots = ProviderID.allCases.map {
            ProviderSnapshot(
                id: $0,
                displayName: $0.displayName,
                fidelity: .legacy,
                status: .error(reason: "Aguardando serviço local"),
                windows: [],
                headlineID: nil,
                hasData: false
            )
        }
    }

    func apply(_ view: LegacyProvidersView) {
        snapshots = view.canonicalSnapshots()
        updatedAt = .now
        sourceMessage = "Conectado ao serviço legado local"
    }

    func fail(with error: Error) {
        let message = error.localizedDescription
        snapshots = snapshots.map {
            ProviderSnapshot(
                id: $0.id,
                displayName: $0.displayName,
                fidelity: $0.fidelity,
                status: .error(reason: message),
                windows: $0.windows,
                headlineID: $0.headlineID,
                hasData: $0.hasData
            )
        }
        sourceMessage = message
    }

    func setDemoMode(_ enabled: Bool) {
        isDemoMode = enabled
        guard enabled else {
            sourceMessage = "Aguardando serviço local"
            return
        }
        snapshots = [
            ProviderSnapshot(
                id: .claude,
                displayName: ProviderID.claude.displayName,
                fidelity: .manual,
                status: .ok,
                windows: [UsageWindow(id: "five_hour", label: "5 horas", usedFraction: 0.42, resetsAt: .now.addingTimeInterval(3_600))],
                headlineID: "five_hour",
                hasData: true
            ),
            ProviderSnapshot(
                id: .codex,
                displayName: ProviderID.codex.displayName,
                fidelity: .manual,
                status: .stale(ageSeconds: 1_260),
                windows: [UsageWindow(id: "seven_day", label: "7 dias", usedFraction: 0.78, resetsAt: .now.addingTimeInterval(86_400))],
                headlineID: "seven_day",
                hasData: true
            ),
        ]
        updatedAt = .now
        sourceMessage = "Dados de demonstração"
    }
}

@MainActor
final class PollingController {
    private let provider: LegacySnapshotProvider
    private let store: UsageStore
    private var task: Task<Void, Never>?

    init(provider: LegacySnapshotProvider, store: UsageStore) {
        self.provider = provider
        self.store = store
    }

    func start() {
        guard task == nil else { return }
        task = Task { [weak self] in
            while !Task.isCancelled {
                await self?.refresh()
                try? await Task.sleep(for: .seconds(15))
            }
        }
    }

    func stop() {
        task?.cancel()
        task = nil
    }

    func refresh() async {
        guard !store.isDemoMode else { return }
        do {
            store.apply(try await provider.load())
        } catch {
            store.fail(with: error)
        }
    }
}
