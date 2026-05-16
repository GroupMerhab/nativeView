// SPDX-License-Identifier: Apache-2.0

import XCTest
@testable import NativeViewIOS

/// Canonical default bridge method keys — keep in lockstep with
/// `bindings/android/src/test/java/com/nativeview/ops/DefaultAndroidBridgeMethodKeysTest.java`.
enum IOSDefaultBridgeContract {
    static let sortedMethodKeys: [String] = [
        "app.exit",
        "app.getInfo",
        "app.getPlatform",
        "app.getVersion",
        "biometric.authenticate",
        "biometric.isAvailable",
        "camera.pickImage",
        "camera.takePicture",
        "clipboard.clear",
        "clipboard.readText",
        "clipboard.writeText",
        "device.getInfo",
        "fs.deleteFile",
        "fs.exists",
        "fs.listDir",
        "fs.readFile",
        "fs.writeFile",
        "location.get",
        "location.unwatch",
        "location.watch",
        "network.getStatus",
        "network.onChange",
        "notification.cancel",
        "notification.cancelAll",
        "notification.show",
        "storage.clear",
        "storage.get",
        "storage.remove",
        "storage.set",
    ]

    static func assertSortedUnique(file: StaticString = #file, line: UInt = #line) {
        let sorted = sortedMethodKeys.sorted()
        XCTAssertEqual(sortedMethodKeys, sorted, "Keep sortedMethodKeys alphabetically sorted (Android parity test).", file: file, line: line)
        XCTAssertEqual(Set(sortedMethodKeys).count, sortedMethodKeys.count, "Duplicate method key", file: file, line: line)
    }
}

extension XCTestCase {
    /// Drains nested `DispatchQueue.main.async` work (e.g. bridge `evalJs` after permission / `rejectWire`).
    func flushMain(rounds: Int = 4, timeout: TimeInterval = 3.0) {
        for _ in 0 ..< rounds {
            let e = expectation(description: "main flush")
            DispatchQueue.main.async { e.fulfill() }
            wait(for: [e], timeout: timeout)
        }
    }
}

/// Skips real permission prompts; use for bridge dispatch tests that list non-empty `permissions`.
final class ImmediateGrantPermissionManager: NVPermissionManager, @unchecked Sendable {
    override func check(
        permissions: [NVPermission],
        then: @escaping () -> Void,
        denied: @escaping () -> Void
    ) {
        if permissions.isEmpty {
            super.check(permissions: permissions, then: then, denied: denied)
            return
        }
        DispatchQueue.global(qos: .userInitiated).async(execute: then)
    }
}

final class ImmediateDeniedPermissionManager: NVPermissionManager, @unchecked Sendable {
    override func check(
        permissions: [NVPermission],
        then: @escaping () -> Void,
        denied: @escaping () -> Void
    ) {
        if permissions.isEmpty {
            super.check(permissions: permissions, then: then, denied: denied)
            return
        }
        DispatchQueue.main.async(execute: denied)
    }
}

final class PrefixDecodeProbeHandler: NVHandler {
    func handle(methodKey _: String, arguments _: [String: Any]) async throws -> Any? {
        nil
    }
}
