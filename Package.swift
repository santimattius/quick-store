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
            url: "https://github.com/santimattius/quick-store/releases/download/v0.1.0-alpha01/QuickStore.xcframework.zip",
            checksum: "REPLACE_WITH_CHECKSUM"
        )
    ]
)
