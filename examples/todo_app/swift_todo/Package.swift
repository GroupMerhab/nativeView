// swift-tools-version: 5.9
// SPDX-License-Identifier: Apache-2.0
//
// Libraries + unit tests. The GUI executable lives in app/ (separate Package) so
// `swift test` does not need to link nativeview static archives.

import PackageDescription

let package = Package(
  name: "swift_todo",
  products: [
    .library(name: "TodoBackend", targets: ["TodoBackend"]),
    .library(name: "TodoHtmlEmbed", targets: ["TodoHtmlEmbed"]),
  ],
  targets: [
    .target(
      name: "TodoHtmlEmbed",
      dependencies: [],
      path: "Sources/TodoHtmlEmbed"
    ),
    .target(
      name: "TodoBackend",
      dependencies: [],
      path: "Sources/TodoBackend",
      linkerSettings: [
        .linkedLibrary("sqlite3"),
      ]
    ),
    .testTarget(
      name: "TodoBackendTests",
      dependencies: ["TodoBackend"],
      path: "Tests/TodoBackendTests"
    ),
  ]
)
