// swift-tools-version: 6.2

import PackageDescription

let package = Package(
    name: "TokEsp",
    platforms: [.macOS(.v14)],
    products: [
        .executable(name: "TokEsp", targets: ["TokEspApp"]),
    ],
    targets: [
        .executableTarget(
            name: "TokEspApp",
            resources: [.process("Resources")]
        ),
        .testTarget(name: "TokEspAppTests", dependencies: ["TokEspApp"]),
    ]
)
