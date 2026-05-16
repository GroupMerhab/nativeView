// SPDX-License-Identifier: Apache-2.0

import XCTest
@testable import NativeViewIOS

final class PermissionTests: XCTestCase {
    func testPermissionManagerConstructs() {
        let pm = NVPermissionManager()
        XCTAssertNotNil(pm)
    }

    func testPermissionCheckEmptyInvokesThen() {
        let pm = NVPermissionManager()
        let exp = expectation(description: "then")
        pm.check(permissions: [], then: {
            exp.fulfill()
        }, denied: {
            XCTFail("denied should not run for empty permissions")
        })
        wait(for: [exp], timeout: 2.0)
    }

    func testNVPermissionCases() {
        let cases: [NVPermission] = [
            .camera, .location, .microphone, .photoLibrary, .notifications, .biometric, .contacts,
        ]
        XCTAssertEqual(cases.count, 7)
    }

    func testPermissionDenied_rejectsBridgeCall() {
        let bridge = NVBridge(permissionManager: ImmediateDeniedPermissionManager())
        let app = NativeViewApp(bridge: bridge)
        defer { app.destroy() }
        app.handle("demo.guarded", permissions: [.camera]) { _, _, _, reject in
            XCTFail("handler should not run when permission manager denies")
            reject("NO", "no")
        }
        let win = app.createWindow(config: NativeViewConfig())
        var scripts: [String] = []
        win.evalJsCapture = { scripts.append($0) }

        app.deliverWebMessage(
            from: win,
            pageURL: URL(fileURLWithPath: "/tmp/x.html"),
            wireJson: #"{"e":"demo.guarded","s":1,"d":null}"#
        )
        flushMain()
        let js = scripts.joined()
        XCTAssertTrue(js.contains("PERMISSION_DENIED"), js)
    }

    func testImmediateGrant_permissionChainInvokesHandler() {
        let bridge = NVBridge(permissionManager: ImmediateGrantPermissionManager())
        let app = NativeViewApp(bridge: bridge)
        defer { app.destroy() }
        app.handle("demo.granted", permissions: [.camera]) { _, _, resolve, _ in
            resolve("\"ok\"")
        }
        let win = app.createWindow(config: NativeViewConfig())
        var scripts: [String] = []
        win.evalJsCapture = { scripts.append($0) }

        app.deliverWebMessage(
            from: win,
            pageURL: URL(fileURLWithPath: "/tmp/x.html"),
            wireJson: #"{"e":"demo.granted","s":2,"d":null}"#
        )
        flushMain()
        let js = scripts.joined()
        XCTAssertTrue(js.contains("_resolve"), js)
        XCTAssertTrue(js.contains("ok"), js)
    }
}
