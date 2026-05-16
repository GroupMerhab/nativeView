// swift-tools-version: 5.9
// SPDX-License-Identifier: Apache-2.0
// Executable package (linked with nativeview via ../build_static.sh).

import PackageDescription

let package = Package(
  name: "swift_todo_app",
  products: [
    .executable(name: "swift_todo", targets: ["swift_todo"]),
  ],
  dependencies: [
    .package(name: "swift_todo", path: ".."),
    .package(name: "Nativeview", path: "../../../../bindings/swift"),
  ],
  targets: [
    .executableTarget(
      name: "swift_todo",
      dependencies: [
        .product(name: "TodoBackend", package: "swift_todo"),
        .product(name: "TodoHtmlEmbed", package: "swift_todo"),
        .product(name: "Nativeview", package: "Nativeview"),
      ],
      path: "Sources"
    ),
  ]
)
