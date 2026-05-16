// SPDX-License-Identifier: Apache-2.0

import XCTest
@testable import NativeViewIOS

final class AppLifecycleTests: XCTestCase {
    func testAppEmit_broadcastsToWindowEvalCapture() {
        let app = NativeViewApp()
        defer { app.destroy() }
        let win = app.createWindow(config: NativeViewConfig())
        var scripts: [String] = []
        win.evalJsCapture = { scripts.append($0) }
        app.emit(event: "app.pause", json: "{}")
        flushMain()
        let js = scripts.joined()
        XCTAssertTrue(js.contains("__nv._emit"), js)
        XCTAssertTrue(js.contains("app.pause"), js)
    }

    func testEmitWithNoWindowsDoesNotCrash() {
        let app = NativeViewApp()
        app.emit(event: "app.resume", json: "{}")
        app.destroy()
    }

    func testHandleDeepLinkWithNoWindowsDoesNotCrash() {
        let app = NativeViewApp()
        guard let url = URL(string: "myapp://host/path?q=1") else {
            return XCTFail("fixture URL")
        }
        app.handleDeepLink(url: url)
        app.destroy()
    }

    func testEmitAndDeepLinkIgnoredAfterDestroy() {
        let app = NativeViewApp()
        app.destroy()
        app.emit(event: "app.resume", json: "{}")
        guard let url = URL(string: "myapp://host/x") else {
            return XCTFail("fixture URL")
        }
        app.handleDeepLink(url: url)
    }
}
