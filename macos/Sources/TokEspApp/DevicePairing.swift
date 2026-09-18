import Foundation

struct DevicePairingService {
    let endpoint: URL

    init(endpoint: URL = URL(string: "http://127.0.0.1:43110/pair/approve")!) {
        self.endpoint = endpoint
    }

    func approve(code: String) async throws {
        let normalized = code.uppercased().filter { $0.isLetter || $0.isNumber }
        guard normalized.count == 8 else {
            throw DevicePairingError.invalidCode
        }
        var request = URLRequest(url: endpoint)
        request.httpMethod = "POST"
        request.timeoutInterval = 5
        request.setValue("application/json", forHTTPHeaderField: "Content-Type")
        request.httpBody = try JSONEncoder().encode([String: String](dictionaryLiteral: ("user_code", normalized)))
        let (_, response) = try await URLSession.shared.data(for: request)
        guard let http = response as? HTTPURLResponse, http.statusCode == 204 else {
            throw DevicePairingError.notApproved
        }
    }
}

enum DevicePairingError: LocalizedError {
    case invalidCode
    case notApproved

    var errorDescription: String? {
        switch self {
        case .invalidCode: "Informe os oito caracteres exibidos no display."
        case .notApproved: "Não foi possível parear o dispositivo. Confira o código e tente novamente."
        }
    }
}
