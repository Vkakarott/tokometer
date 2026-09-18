import Foundation

struct LegacySnapshotProvider {
    let endpoint: URL

    init(endpoint: URL = URL(string: "http://127.0.0.1:43110/usage/local")!) {
        self.endpoint = endpoint
    }

    func load() async throws -> LegacyProvidersView {
        var request = URLRequest(url: endpoint)
        request.timeoutInterval = 5
        let (data, response) = try await URLSession.shared.data(for: request)
        guard let http = response as? HTTPURLResponse, http.statusCode == 200 else {
            throw LegacySnapshotError.unavailable
        }
        return try JSONDecoder().decode(LegacyProvidersView.self, from: data)
    }
}

enum LegacySnapshotError: LocalizedError {
    case unavailable

    var errorDescription: String? {
        switch self {
        case .unavailable: "O serviço legado não respondeu."
        }
    }
}
