// SPDX-License-Identifier: Apache-2.0

import XCTest
@testable import NativeViewIOS

/// Full ``NativeViewApp/deliverWebMessage(from:pageURL:wireJson:)`` dispatch with ``NativeViewWindow/evalJsCapture``.
final class BridgeIntegrationTests: XCTestCase {
    func testInvoke_roundtrip_handshake_resolvesWireVersion() {
        let app = NativeViewApp()
        defer { app.destroy() }
        let win = app.createWindow(config: NativeViewConfig())
        var scripts: [String] = []
        win.evalJsCapture = { scripts.append($0) }

        let page = URL(fileURLWithPath: "/tmp/nv_handshake_test.html")
        app.deliverWebMessage(
            from: win,
            pageURL: page,
            wireJson: #"{"e":"app.handshake","s":99,"d":{"wireVersion":1}}"#
        )
        flushMain()
        XCTAssertEqual(scripts.count, 1)
        let js = scripts[0]
        XCTAssertTrue(js.contains("NativeView._resolve"), js)
        XCTAssertTrue(js.contains("(99,"), js)
        XCTAssertTrue(js.contains("wireVersion"), js)
    }

    func testInvoke_unknownMethod_rejectsNotAvailable() {
        let app = NativeViewApp()
        defer { app.destroy() }
        let win = app.createWindow(config: NativeViewConfig())
        var scripts: [String] = []
        win.evalJsCapture = { scripts.append($0) }

        app.deliverWebMessage(
            from: win,
            pageURL: URL(fileURLWithPath: "/tmp/x.html"),
            wireJson: #"{"e":"missing.bridge.method","s":7,"d":null}"#
        )
        flushMain()
        let js = scripts.joined()
        XCTAssertTrue(js.contains("NativeView._reject"), js)
        XCTAssertTrue(js.contains("NOT_AVAILABLE"), js)
    }

    func testInvoke_originNotAllowed_rejectsPermissionDenied() {
        let app = NativeViewApp()
        defer { app.destroy() }
        app.allowOrigin("https://allowed.example")
        let win = app.createWindow(config: NativeViewConfig())
        var scripts: [String] = []
        win.evalJsCapture = { scripts.append($0) }

        var c = URLComponents()
        c.scheme = "https"
        c.host = "other.example"
        c.path = "/index.html"
        guard let badPage = c.url else {
            return XCTFail("fixture url")
        }

        app.deliverWebMessage(
            from: win,
            pageURL: badPage,
            wireJson: #"{"e":"app.getVersion","s":3,"d":null}"#
        )
        flushMain()
        let js = scripts.joined()
        XCTAssertTrue(js.contains("PERMISSION_DENIED"), js)
    }

    func testInvoke_malformedArgs_invalidArgs() {
        let app = NativeViewApp()
        defer { app.destroy() }
        app.registerDefaultIOSHandlers()
        let win = app.createWindow(config: NativeViewConfig())
        var scripts: [String] = []
        win.evalJsCapture = { scripts.append($0) }

        app.deliverWebMessage(
            from: win,
            pageURL: URL(fileURLWithPath: "/tmp/x.html"),
            wireJson: #"{"e":"storage.get","s":2,"d":[1,2]}"#
        )
        flushMain()
        let js = scripts.joined()
        XCTAssertTrue(js.contains("INVALID_ARGS"), js)
    }

    func testInvoke_prefixHandler_invalidArgsObjectShape() {
        let app = NativeViewApp()
        defer { app.destroy() }
        app.bridge.router.register(prefix: "demo.", handler: PrefixDecodeProbeHandler())
        let win = app.createWindow(config: NativeViewConfig())
        var scripts: [String] = []
        win.evalJsCapture = { scripts.append($0) }

        app.deliverWebMessage(
            from: win,
            pageURL: URL(fileURLWithPath: "/tmp/x.html"),
            wireJson: #"{"e":"demo.any","s":4,"d":[1]}"#
        )
        flushMain()
        let js = scripts.joined()
        XCTAssertTrue(js.contains("INVALID_ARGS"), js)
    }

    func testInvoke_exactRoute_overPrefixHandler() {
        let app = NativeViewApp()
        defer { app.destroy() }
        app.bridge.router.register(prefix: "demo.", handler: PrefixDecodeProbeHandler())
        app.handle("demo.exact", permissions: []) { _, _, resolve, _ in
            resolve("\"ok\"")
        }
        let win = app.createWindow(config: NativeViewConfig())
        var scripts: [String] = []
        win.evalJsCapture = { scripts.append($0) }

        app.deliverWebMessage(
            from: win,
            pageURL: URL(fileURLWithPath: "/tmp/x.html"),
            wireJson: #"{"e":"demo.exact","s":5,"d":null}"#
        )
        flushMain()
        let js = scripts.joined()
        XCTAssertTrue(js.contains("_resolve"), js)
        XCTAssertTrue(js.contains("ok"), js)
    }

    func testEmit_windowPathUsesNvEmit() {
        let app = NativeViewApp()
        defer { app.destroy() }
        let win = app.createWindow(config: NativeViewConfig())
        var scripts: [String] = []
        win.evalJsCapture = { scripts.append($0) }

        win.emit(event: "custom.event", json: #"{"x":1}"#)
        flushMain()
        let js = scripts.joined()
        XCTAssertTrue(js.contains("__nv._emit"), js)
        XCTAssertTrue(js.contains("custom.event"), js)
    }

    func testDeepLink_emitsNativeViewEmit() {
        let app = NativeViewApp()
        defer { app.destroy() }
        let win = app.createWindow(config: NativeViewConfig())
        var scripts: [String] = []
        win.evalJsCapture = { scripts.append($0) }

        guard let url = URL(string: "myapp://deeplink/path") else {
            return XCTFail("url")
        }
        app.handleDeepLink(url: url)
        flushMain()
        let js = scripts.joined()
        XCTAssertTrue(js.contains("NativeView._emit"), js)
        XCTAssertTrue(js.contains("app.deeplink"), js)
        XCTAssertTrue(js.contains("myapp:") && js.contains("deeplink"), js)
    }

}
