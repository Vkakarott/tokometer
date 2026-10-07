import Foundation

/// The display's own setup API, reachable only while this Mac is on its `tokEsp-xxxx` network.
struct DisplaySetupClient: Sendable {
    typealias Send = @Sendable (URLRequest) async throws -> (Data, URLResponse)

    static let portalURL = URL(string: "http://192.168.4.1")!

    let baseURL: URL
    let send: Send

    init(
        baseURL: URL = DisplaySetupClient.portalURL,
        send: @escaping Send = { try await URLSession.shared.data(for: $0) }
    ) {
        self.baseURL = baseURL
        self.send = send
    }

    func hardwareID() async throws -> String {
        var request = URLRequest(url: baseURL.appending(path: "api/status"))
        request.timeoutInterval = 2
        return try await hardwareID(from: request)
    }

    /// Hands the display its network. It leaves setup mode right after answering.
    func sendWifi(ssid: String, password: String) async throws {
        var request = URLRequest(url: baseURL.appending(path: "api/wifi"))
        request.httpMethod = "POST"
        request.timeoutInterval = 5
        request.setValue("application/x-www-form-urlencoded", forHTTPHeaderField: "Content-Type")
        request.httpBody = Data("ssid=\(Self.formEncoded(ssid))&password=\(Self.formEncoded(password))".utf8)
        _ = try await hardwareID(from: request)
    }

    private func hardwareID(from request: URLRequest) async throws -> String {
        let (data, response) = try await send(request)
        guard (response as? HTTPURLResponse)?.statusCode == 200 else {
            throw DisplaySetupError.displayRejected
        }
        return try JSONDecoder().decode(DisplayStatus.self, from: data).hardwareID
    }

    private static func formEncoded(_ value: String) -> String {
        var allowed = CharacterSet.alphanumerics
        allowed.insert(charactersIn: "-._~")
        return value.addingPercentEncoding(withAllowedCharacters: allowed) ?? ""
    }
}

private struct DisplayStatus: Decodable {
    let hardwareID: String

    enum CodingKeys: String, CodingKey {
        case hardwareID = "hardware_id"
    }
}

enum DisplaySetupError: LocalizedError {
    case wifiUnavailable
    case locationRequired
    case displayNotFound
    case displayUnreachable
    case displayRejected
    case backendUnavailable
    case notPaired

    var errorDescription: String? {
        switch self {
        case .wifiUnavailable:
            "Não foi possível acessar o Wi-Fi deste Mac."
        case .locationRequired:
            "O macOS só mostra nomes de redes Wi-Fi com a Localização permitida. Libere o tokEsp em Ajustes > Privacidade e Segurança > Serviços de Localização."
        case .displayNotFound:
            "Nenhuma rede tokEsp-xxxx por perto. Confira se o display mostra a tela de configuração e tente de novo."
        case .displayUnreachable:
            "O Mac entrou na rede do display, mas ele não respondeu. Tente de novo."
        case .displayRejected:
            "O display recusou a rede. O nome pode ter até 32 caracteres e a senha até 64."
        case .backendUnavailable:
            "O serviço local do tokEsp não respondeu. Confira se ele está em execução."
        case .notPaired:
            "O display não entrou na rede. Confira a senha e se a rede é de 2,4 GHz. Ele volta para a tela de configuração em instantes."
        }
    }
}
