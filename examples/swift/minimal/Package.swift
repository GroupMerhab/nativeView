// swift-tools-version: 5.9
// SPDX-License-Identifier: Apache-2.0
import PackageDescription

let package = Package(
  name: "minimal",
  dependencies: [
    .package(name: "Nativeview", path: "../../../bindings/swift"),
  ],
  targets: [
    .executableTarget(
      name: "minimal",
      dependencies: [
        .product(name: "Nativeview", package: "Nativeview"),
      ],
      path: "Sources"
    ),
  ]
)
