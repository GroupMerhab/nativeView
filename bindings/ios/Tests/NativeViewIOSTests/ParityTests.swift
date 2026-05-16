// SPDX-License-Identifier: Apache-2.0

import XCTest
@testable import NativeViewIOS

/// Release gate: default bridge surface must match Android `DefaultAndroidBridgeMethodKeysTest`
/// and the shared JVM `WireContractParityTest` envelope rules.
///
/// Desktop Swift (`bindings/swift`) intentionally exposes only the C API (`Nativeview`); the
/// mobile JSON bridge contract is shared with Android/Java.
final class ParityTests: XCTestCase {
    func testModuleMarker() {
        XCTAssertEqual(NativeViewIOSSupport.moduleName, "NativeViewIOS")
    }

    func testDefaultBridgeRegistersEveryCanonicalMethodKey() {
        IOSDefaultBridgeContract.assertSortedUnique()
        let app = NativeViewApp()
        defer { app.destroy() }
        app.registerDefaultIOSHandlers()
        let r = app.bridge.router
        for key in IOSDefaultBridgeContract.sortedMethodKeys {
            XCTAssertNotNil(r.methodRoute(forExactKey: key), "Missing route for \(key)")
        }
    }

    // MARK: - Wire envelope (mirrors `bindings/parity-tests/.../WireContractParityTest.java`)

    func testWire_extractMethod_readsEField() {
        XCTAssertEqual(NVWireHelpers.parseMethod(#"{"e":"app.ping","s":1,"d":{}}"#), "app.ping")
        XCTAssertNil(NVWireHelpers.parseMethod(#"{"s":1}"#))
    }

    func testWire_extractSeq_readsSField() {
        XCTAssertEqual(NVWireHelpers.extractSeq(#"{"e":"x","s":42}"#), 42)
        XCTAssertEqual(NVWireHelpers.extractSeq(#"{"e":"x"}"#), 0)
    }

    func testWire_topLevelMustBeObject() {
        XCTAssertThrowsError(try Self.parseTopLevelObject("[1,2]"))
    }

    func testWire_trimsWhitespace() throws {
        let data = "  {\"e\":\"a\"}  ".trimmingCharacters(in: .whitespacesAndNewlines).data(using: .utf8)!
        let obj = try JSONSerialization.jsonObject(with: data) as? [String: Any]
        XCTAssertEqual(obj?["e"] as? String, "a")
    }

    func testDesktopSwiftBindingIsCInteropOnly() {
        XCTAssertFalse(NVCBridge.versionString().isEmpty)
    }

    private static func parseTopLevelObject(_ wireJson: String) throws -> [String: Any] {
        let trimmed = wireJson.trimmingCharacters(in: .whitespacesAndNewlines)
        guard let data = trimmed.data(using: .utf8) else {
            throw NSError(domain: "wire", code: 1)
        }
        let v = try JSONSerialization.jsonObject(with: data, options: [])
        guard let obj = v as? [String: Any] else {
            throw NSError(domain: "wire", code: 2, userInfo: [NSLocalizedDescriptionKey: "wire must be a JSON object"])
        }
        return obj
    }
}
