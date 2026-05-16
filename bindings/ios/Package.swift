// swift-tools-version: 5.7
// SPDX-License-Identifier: Apache-2.0

import PackageDescription

let package = Package(
    name: "NativeViewIOS",
    platforms: [
        .iOS(.v14),
    ],
    products: [
        .library(
            name: "NativeViewIOS",
            targets: ["NativeViewIOS"]
        ),
    ],
    targets: [
        /// Built by `scripts/create_nativeview_xcframework.sh` (real libs from `scripts/build_ios_static_libs.sh`, or `--stub` for a link-only archive).
        .binaryTarget(
            name: "NativeView",
            path: "NativeView.xcframework"
        ),
        .target(
            name: "NativeViewIOS",
            dependencies: ["NativeView"],
            path: "Sources/NativeViewIOS",
            resources: [
                .process("Resources/nativeview.js"),
            ],
        ),
        .testTarget(
            name: "NativeViewIOSTests",
            dependencies: ["NativeViewIOS"],
            path: "Tests/NativeViewIOSTests"
        ),
    ]
)
