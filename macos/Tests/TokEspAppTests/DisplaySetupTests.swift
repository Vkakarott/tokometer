import Foundation
import Testing
@testable import TokEspApp

/// Records every request and answers with `status` and `body`.
private final class FakeServer: @unchecked Sendable {
    private let lock = NSLock()
    private var _requests: [URLRequest] = []
    private let status: Int
    private let body: String

    init(status: Int = 200, body: String = "") {
        self.status = status
        self.body = body
    }

    var requests: [URLRequest] { lock.withLock { _requests } }

    func send(_ request: URLRequest) -> (Data, URLResponse) {
        lock.withLock { _requests.append(request) }
        let response = HTTPURLResponse(url: request.url!, statusCode: status, httpVersion: nil, headerFields: nil)!
        return (Data(body.utf8), response)
    }
}

private func display(_ fake: FakeServer) -> DisplaySetupClient {
    DisplaySetupClient(send: { fake.send($0) })
}

private func pairing(_ fake: FakeServer) -> DevicePairingService {
    DevicePairingService(send: { fake.send($0) })
}

private func jsonBody(_ request: URLRequest) throws -> [String: String] {
    try JSONDecoder().decode([String: String].self, from: #require(request.httpBody))
}

@Test func readsTheHardwareIdFromTheDisplay() async throws {
    let fake = FakeServer(body: #"{"hardware_id":"esp32-0123456789ab"}"#)
    let hardwareID = try await display(fake).hardwareID()

    #expect(hardwareID == "esp32-0123456789ab")
    #expect(fake.requests.first?.url?.absoluteString == "http://192.168.4.1/api/status")
}

@Test func sendsWifiAsAFormTheDisplayCanDecode() async throws {
    let fake = FakeServer(body: #"{"hardware_id":"esp32-0123456789ab"}"#)
    try await display(fake).sendWifi(ssid: "Casa & Cia 2.4", password: "s3nh@+f=ç")

    let request = try #require(fake.requests.first)
    #expect(request.url?.absoluteString == "http://192.168.4.1/api/wifi")
    #expect(request.httpMethod == "POST")
    #expect(request.value(forHTTPHeaderField: "Content-Type") == "application/x-www-form-urlencoded")
    let body = String(decoding: try #require(request.httpBody), as: UTF8.self)
    #expect(body == "ssid=Casa%20%26%20Cia%202.4&password=s3nh%40%2Bf%3D%C3%A7")
}

@Test func reportsACredentialTheDisplayRefuses() async {
    let fake = FakeServer(status: 400, body: #"{"error":"invalid_credentials"}"#)
    await #expect(throws: DisplaySetupError.displayRejected) {
        try await display(fake).sendWifi(ssid: "Casa", password: "senha")
    }
}

@Test func approvesABoardByItsHardwareId() async throws {
    let fake = FakeServer(status: 204)
    try await pairing(fake).approve(hardwareID: "esp32-0123456789ab")

    let request = try #require(fake.requests.first)
    #expect(request.url?.absoluteString == "http://127.0.0.1:43110/pair/approve")
    #expect(request.httpMethod == "POST")
    #expect(try jsonBody(request) == ["hardware_id": "esp32-0123456789ab"])
}

@Test func approvesATypedCodeWithoutItsHyphen() async throws {
    let fake = FakeServer(status: 204)
    try await pairing(fake).approve(code: "k7qm-3f9a")

    #expect(try jsonBody(#require(fake.requests.first)) == ["user_code": "K7QM3F9A"])
}

@Test func refusesACodeOfTheWrongLengthWithoutCallingTheBackend() async {
    let fake = FakeServer(status: 204)
    await #expect(throws: DevicePairingError.invalidCode) {
        try await pairing(fake).approve(code: "K7QM")
    }
    #expect(fake.requests.isEmpty)
}

@Test func readsWhetherTheBoardFinishedPairing() async throws {
    let fake = FakeServer(body: #"{"paired":true}"#)
    let paired = try await pairing(fake).isPaired(hardwareID: "esp32-0123456789ab")

    #expect(paired)
    #expect(fake.requests.first?.url?.absoluteString == "http://127.0.0.1:43110/pair/status?hardware_id=esp32-0123456789ab")
}
