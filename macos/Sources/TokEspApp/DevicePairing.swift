import Foundation

/// Talks to the local backend's pairing endpoints, which only answer this machine.
struct DevicePairingService: Sendable {
    typealias Send = @Sendable (URLRequest) async throws -> (Data, URLResponse)

    let baseURL: URL
    let send: Send

    init(
        baseURL: URL = URL(string: "http://127.0.0.1:43110")!,
        send: @escaping Send = { try await URLSession.shared.data(for: $0) }
    ) {
        self.baseURL = baseURL
        self.send = send
    }

    /// Approves the code a human read off the display.
    func approve(code: String) async throws {
        let normalized = code.uppercased().filter { $0.isLetter || $0.isNumber }
        guard normalized.count == 8 else {
            throw DevicePairingError.invalidCode
        }
        try await postApproval(["user_code": normalized])
    }

    /// Approves a board the app has just configured, before it asks for a code.
    func approve(hardwareID: String) async throws {
        try await postApproval(["hardware_id": hardwareID])
    }

    /// True once the board has redeemed its approval for an access token.
    func isPaired(hardwareID: String) async throws -> Bool {
        var components = URLComponents(url: baseURL.appending(path: "pair/status"), resolvingAgainstBaseURL: false)!
        components.queryItems = [URLQueryItem(name: "hardware_id", value: hardwareID)]
        var request = URLRequest(url: components.url!)
        request.timeoutInterval = 5
        let (data, response) = try await send(request)
        guard (response as? HTTPURLResponse)?.statusCode == 200 else {
            throw DevicePairingError.notApproved
        }
        return try JSONDecoder().decode(PairingStatus.self, from: data).paired
    }

    private func postApproval(_ body: [String: String]) async throws {
        var request = URLRequest(url: baseURL.appending(path: "pair/approve"))
        request.httpMethod = "POST"
        request.timeoutInterval = 5
        request.setValue("application/json", forHTTPHeaderField: "Content-Type")
        request.httpBody = try JSONEncoder().encode(body)
        let (_, response) = try await send(request)
        guard (response as? HTTPURLResponse)?.statusCode == 204 else {
            throw DevicePairingError.notApproved
        }
    }
}

private struct PairingStatus: Decodable {
    let paired: Bool
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
