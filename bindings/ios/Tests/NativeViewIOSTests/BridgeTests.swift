// SPDX-License-Identifier: Apache-2.0

import XCTest
@testable import NativeViewIOS

final class BridgeTests: XCTestCase {
    func testNVCBridgeVersionString() {
        let v = NVCBridge.versionString()
        XCTAssertFalse(v.isEmpty)
    }

    func testNVCBridgeVersionInfo() {
        let info = NVCBridge.versionInfo()
        XCTAssertGreaterThanOrEqual(info.major, 0)
        XCTAssertGreaterThanOrEqual(info.minor, 0)
        XCTAssertGreaterThanOrEqual(info.patch, 0)
        XCTAssertGreaterThanOrEqual(NVCBridge.benchNow(), 0)
    }

    func testRouterRegistersHandlerPrefix() {
        let router = NVRouter()
        final class H: NVHandler {
            func handle(methodKey _: String, arguments _: [String: Any]) async throws -> Any? { nil }
        }
        let h = H()
        router.register(prefix: "device.", handler: h)
        XCTAssertTrue(router.handler(forMethodKey: "device.orientation") === h)
    }

    func testRouterRegistersExactMethodHandler() {
        let router = NVRouter()
        let app = NativeViewApp()
        defer { app.destroy() }
        let win = app.createWindow(config: NativeViewConfig())
        let exp = expectation(description: "handler")
        router.register(method: "demo.echo") { window, args, resolve, _ in
            XCTAssertTrue(window === win)
            guard let data = args.data(using: .utf8),
                  let obj = try? JSONSerialization.jsonObject(with: data) as? [String: Any]
            else {
                XCTFail("args")
                return
            }
            XCTAssertEqual(obj["x"] as? Int, 1)
            resolve(args)
            exp.fulfill()
        }
        let route = router.methodRoute(forExactKey: "demo.echo")
        XCTAssertNotNil(route)
        route?.handler(win, #"{"x":1}"#, { _ in }, { _, _ in })
        wait(for: [exp], timeout: 1.0)
    }

    func testRouterRouteReturnsFalseWhenMissing() {
        let router = NVRouter()
        let app = NativeViewApp()
        defer { app.destroy() }
        let win = app.createWindow(config: NativeViewConfig())
        let routed = router.route(
            method: "missing.op",
            window: win,
            args: "{}",
            resolve: { _ in XCTFail("resolve") },
            reject: { _, _ in XCTFail("reject") }
        )
        XCTAssertFalse(routed)
    }

    func testRouterRouteInvokesRegisteredMethod() {
        let router = NVRouter()
        let app = NativeViewApp()
        defer { app.destroy() }
        let win = app.createWindow(config: NativeViewConfig())
        let exp = expectation(description: "routed")
        router.register(method: "demo.ping") { window, args, resolve, _ in
            XCTAssertTrue(window === win)
            XCTAssertEqual(args, "null")
            resolve("\"pong\"")
            exp.fulfill()
        }
        XCTAssertTrue(router.route(method: "demo.ping", window: win, args: "null", resolve: { _ in }, reject: { _, _ in XCTFail("reject") }))
        wait(for: [exp], timeout: 1.0)
    }

    func testWireParseMethodAndArgs() {
        let wire = #"{"e":"app.handshake","s":3,"d":{"wireVersion":1}}"#
        XCTAssertEqual(NVWireHelpers.parseMethod(wire), "app.handshake")
        XCTAssertEqual(NVWireHelpers.extractSeq(wire), 3)
        guard let args = NVWireHelpers.extractArgsJson(wire),
              let data = args.data(using: .utf8),
              let obj = try? JSONSerialization.jsonObject(with: data) as? [String: Any]
        else {
            return XCTFail("args json")
        }
        XCTAssertEqual(obj["wireVersion"] as? Int, 1)
    }

    func testNativeViewConfigDefaults() {
        let c = NativeViewConfig()
        XCTAssertEqual(c.width, 390)
        XCTAssertEqual(c.height, 844)
        XCTAssertTrue(c.resizable)
        XCTAssertFalse(c.devtools)
    }

    func testAllowOriginNormalizes() {
        let app = NativeViewApp()
        defer { app.destroy() }
        app.allowOrigin("HTTPS://Example.com:443/path")
        XCTAssertTrue(app.allowedOriginsSnapshot().contains("https://example.com"))
    }

    func testOriginPolicyRejectsNilURL() {
        let allowed: Set<String> = ["file://"]
        XCTAssertFalse(NVBridgeOrigin.isAllowed(pageURL: nil, allowed: allowed))
    }

    func testOriginPolicyAllowsFileWhenFileOriginAllowed() {
        let allowed: Set<String> = ["file://"]
        let u = URL(fileURLWithPath: "/tmp/index.html")
        XCTAssertTrue(NVBridgeOrigin.isAllowed(pageURL: u, allowed: allowed))
    }

    func testOriginPolicyAllowsHttpsWhenHostMatches() {
        var c = URLComponents()
        c.scheme = "https"
        c.host = "example.com"
        c.path = "/index.html"
        guard let u = c.url else {
            return XCTFail("url")
        }
        XCTAssertTrue(NVBridgeOrigin.isAllowed(pageURL: u, allowed: ["https://example.com"]))
    }

    func testJsonWireDecodeObjectRejectsNonObject() {
        XCTAssertThrowsError(try NVJsonWire.decodeObject(argsJson: "[1,2]")) { error in
            XCTAssertEqual(error as? NVJsonWire.WireError, NVJsonWire.WireError.invalidArgs)
        }
    }

    func testJsonWireDecodeObjectRejectsInvalidJson() {
        XCTAssertThrowsError(try NVJsonWire.decodeObject(argsJson: "{")) { error in
            XCTAssertEqual(error as? NVJsonWire.WireError, NVJsonWire.WireError.invalidJson)
        }
    }

    func testJavaScriptStringEncodingEscapesQuotes() {
        let original = "a\"b\\c\u{000A}d"
        let lit = NVJavaScriptStringEncoding.jsStringLiteral(original)
        guard let data = lit.data(using: .utf8),
              let roundtrip = try? JSONSerialization.jsonObject(with: data, options: [.fragmentsAllowed]) as? String
        else {
            return XCTFail("expected JSON string literal")
        }
        XCTAssertEqual(roundtrip, original)
    }

    func testCreateWindowLoadsTitle() {
        let app = NativeViewApp()
        defer { app.destroy() }
        let w = app.createWindow(config: NativeViewConfig(title: "Hello"))
        let vc = w.viewController() as! NativeViewViewController
        vc.loadViewIfNeeded()
        XCTAssertEqual(vc.title, "Hello")
    }

    // MARK: - Promise JS snippets (parity with Android `BridgeClientScript`)

    func testWebBridgeClient_buildResolveInvoke_embedsJsonParse() {
        let js = NVWebBridgeClient.buildResolveInvoke(seq: 12, json: #"{"a":1}"#)
        XCTAssertNotNil(js)
        XCTAssertTrue(js!.contains("NativeView._resolve(12,"), js!)
        XCTAssertTrue(js!.contains("JSON.parse"), js!)
    }

    func testWebBridgeClient_buildResolveInvoke_emptyUsesNull() {
        let js = NVWebBridgeClient.buildResolveInvoke(seq: 3, json: "   ")
        XCTAssertNotNil(js)
        XCTAssertTrue(js!.contains("null"), js!)
    }

    func testWebBridgeClient_buildRejectInvoke_embedsCodeAndMessage() {
        let js = NVWebBridgeClient.buildRejectInvoke(seq: 9, code: "PERMISSION_DENIED", message: "no")
        XCTAssertNotNil(js)
        XCTAssertTrue(js!.contains("NativeView._reject(9,"), js!)
        XCTAssertTrue(js!.contains("PERMISSION_DENIED"), js!)
        XCTAssertTrue(js!.contains("no"), js!)
    }

    func testWebBridgeClient_buildRejectInvoke_zeroSeqReturnsNil() {
        XCTAssertNil(NVWebBridgeClient.buildResolveInvoke(seq: 0, json: "{}"))
        XCTAssertNil(NVWebBridgeClient.buildRejectInvoke(seq: 0, code: "X", message: "Y"))
    }
}
