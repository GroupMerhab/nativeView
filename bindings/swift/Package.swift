// swift-tools-version: 5.9
// SPDX-License-Identifier: Apache-2.0
import PackageDescription

let package = Package(
  name: "Nativeview",
  products: [
    .library(name: "Nativeview", targets: ["Nativeview"]),
  ],
  targets: [
    .target(
      name: "CNativeview",
      path: "Sources/CNativeview",
      sources: ["shim.c"],
      publicHeadersPath: "include",
    ),
    .target(
      name: "Nativeview",
      dependencies: ["CNativeview"],
      path: "Sources/Nativeview",
      sources: ["Nativeview.swift"]
    ),
  ]
)
