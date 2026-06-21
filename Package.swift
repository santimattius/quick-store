// swift-tools-version:5.9
import PackageDescription

let package = Package(
    name: "QuickStore",
    platforms: [
        .iOS(.v13)
    ],
    products: [
        .library(name: "QuickStore", targets: ["QuickStore"])
    ],
    targets: [
        .binaryTarget(
            name: "QuickStore",
            url: "https://github.com/santimattius/quick-store/releases/download/v0.1.0-alpha02/QuickStore.xcframework.zip",
            checksum: "ed46aa2a20c48a33dbbb79c8513ce66d78a852f982b7cb283c695656c68837b5"
        )
    ]
)
