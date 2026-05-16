// SPDX-License-Identifier: Apache-2.0

import XCTest
@testable import NativeViewIOS

final class OpsTests: XCTestCase {
    func testNamespacesDistinct() {
        XCTAssertEqual(DeviceOps.namespace, "device")
        XCTAssertEqual(StorageOps.namespace, "storage")
        XCTAssertEqual(NetworkOps.namespace, "network")
        XCTAssertEqual(FileOps.namespace, "fs")
        XCTAssertEqual(AppOps.namespace, "app")
        XCTAssertEqual(NotificationOps.namespace, "notification")
        XCTAssertEqual(CameraOps.namespace, "camera")
        XCTAssertEqual(LocationOps.namespace, "location")
        XCTAssertEqual(ClipboardOps.namespace, "clipboard")
        XCTAssertEqual(BiometricOps.namespace, "biometric")
    }

    func testRegisterDefaultIOSHandlersInstallsRoutes() {
        IOSDefaultBridgeContract.assertSortedUnique()
        let app = NativeViewApp()
        defer { app.destroy() }
        app.registerDefaultIOSHandlers()
        let r = app.bridge.router
        for k in IOSDefaultBridgeContract.sortedMethodKeys {
            XCTAssertNotNil(r.methodRoute(forExactKey: k), "missing route: \(k)")
        }
    }

    // MARK: - Happy-path shapes (direct router; no WKWebView)

    func testAppGetPlatform_returnsIos() {
        let app = NativeViewApp()
        defer { app.destroy() }
        app.registerDefaultIOSHandlers()
        let win = app.createWindow(config: NativeViewConfig())
        let exp = expectation(description: "resolve")
        XCTAssertTrue(app.bridge.router.route(
            method: "app.getPlatform",
            window: win,
            args: "null",
            resolve: { json in
                XCTAssertTrue(json.contains("\"ios\""), json)
                exp.fulfill()
            },
            reject: { c, m in XCTFail("\(c) \(m)") }
        ))
        wait(for: [exp], timeout: 3.0)
    }

    func testStorageRoundTrip() {
        let app = NativeViewApp()
        defer { app.destroy() }
        app.registerDefaultIOSHandlers()
        let win = app.createWindow(config: NativeViewConfig())
        let key = "nv_ops_test_\(UUID().uuidString)"

        let setExp = expectation(description: "set")
        XCTAssertTrue(app.bridge.router.route(
            method: "storage.set",
            window: win,
            args: #"{"key":"\#(key)","value":"hello"}"#,
            resolve: { _ in setExp.fulfill() },
            reject: { c, m in XCTFail("\(c) \(m)") }
        ))
        wait(for: [setExp], timeout: 3.0)

        let getExp = expectation(description: "get")
        XCTAssertTrue(app.bridge.router.route(
            method: "storage.get",
            window: win,
            args: #"{"key":"\#(key)","type":"string"}"#,
            resolve: { json in
                XCTAssertTrue(json.contains("hello"), json)
                getExp.fulfill()
            },
            reject: { c, m in XCTFail("\(c) \(m)") }
        ))
        wait(for: [getExp], timeout: 3.0)

        let rmExp = expectation(description: "remove")
        XCTAssertTrue(app.bridge.router.route(
            method: "storage.remove",
            window: win,
            args: #"{"key":"\#(key)"}"#,
            resolve: { _ in rmExp.fulfill() },
            reject: { c, m in XCTFail("\(c) \(m)") }
        ))
        wait(for: [rmExp], timeout: 3.0)
    }

    func testNetworkGetStatus_returnsConnectedShape() {
        let app = NativeViewApp()
        defer { app.destroy() }
        app.registerDefaultIOSHandlers()
        let win = app.createWindow(config: NativeViewConfig())
        let exp = expectation(description: "net")
        XCTAssertTrue(app.bridge.router.route(
            method: "network.getStatus",
            window: win,
            args: "null",
            resolve: { json in
                XCTAssertTrue(json.contains("connected") || json.contains("wifi") || json.contains("cellular") || json.contains("unavailable"), json)
                exp.fulfill()
            },
            reject: { c, m in XCTFail("\(c) \(m)") }
        ))
        wait(for: [exp], timeout: 5.0)
    }

    func testClipboardWriteText_resolves() {
        let app = NativeViewApp()
        defer { app.destroy() }
        app.registerDefaultIOSHandlers()
        let win = app.createWindow(config: NativeViewConfig())
        let exp = expectation(description: "write")
        XCTAssertTrue(app.bridge.router.route(
            method: "clipboard.writeText",
            window: win,
            args: #"{"text":"hello"}"#,
            resolve: { _ in exp.fulfill() },
            reject: { c, m in XCTFail("\(c) \(m)") }
        ))
        wait(for: [exp], timeout: 10.0)
    }

    func testClipboardReadText_returnsTextPayload() {
        let app = NativeViewApp()
        defer { app.destroy() }
        app.registerDefaultIOSHandlers()
        let win = app.createWindow(config: NativeViewConfig())
        let exp = expectation(description: "read")
        XCTAssertTrue(app.bridge.router.route(
            method: "clipboard.readText",
            window: win,
            args: "null",
            resolve: { json in
                XCTAssertTrue(json.contains("\"text\""), json)
                exp.fulfill()
            },
            reject: { c, m in XCTFail("\(c) \(m)") }
        ))
        wait(for: [exp], timeout: 10.0)
    }

    func testFsWriteReadDelete_roundTripInDocumentsSandbox() {
        let app = NativeViewApp()
        defer { app.destroy() }
        app.registerDefaultIOSHandlers()
        let win = app.createWindow(config: NativeViewConfig())
        let name = "nv_fs_\(UUID().uuidString).txt"

        let wrExp = expectation(description: "write")
        XCTAssertTrue(app.bridge.router.route(
            method: "fs.writeFile",
            window: win,
            args: #"{"path":"\#(name)","encoding":"text","data":"abc"}"#,
            resolve: { _ in wrExp.fulfill() },
            reject: { c, m in XCTFail("\(c) \(m)") }
        ))
        wait(for: [wrExp], timeout: 5.0)

        let rdExp = expectation(description: "read")
        XCTAssertTrue(app.bridge.router.route(
            method: "fs.readFile",
            window: win,
            args: #"{"path":"\#(name)","encoding":"text"}"#,
            resolve: { json in
                XCTAssertTrue(json.contains("abc"), json)
                rdExp.fulfill()
            },
            reject: { c, m in XCTFail("\(c) \(m)") }
        ))
        wait(for: [rdExp], timeout: 5.0)

        let exExp = expectation(description: "exists")
        XCTAssertTrue(app.bridge.router.route(
            method: "fs.exists",
            window: win,
            args: #"{"path":"\#(name)"}"#,
            resolve: { json in
        XCTAssertTrue(json.contains("\"exists\"") && json.contains("true"), json)
                exExp.fulfill()
            },
            reject: { c, m in XCTFail("\(c) \(m)") }
        ))
        wait(for: [exExp], timeout: 5.0)

        let lsExp = expectation(description: "listDir")
        XCTAssertTrue(app.bridge.router.route(
            method: "fs.listDir",
            window: win,
            args: #"{"path":"."}"#,
            resolve: { json in
                XCTAssertTrue(json.contains("\"files\""), json)
                XCTAssertTrue(json.contains(name), json)
                lsExp.fulfill()
            },
            reject: { c, m in XCTFail("\(c) \(m)") }
        ))
        wait(for: [lsExp], timeout: 5.0)

        let dlExp = expectation(description: "delete")
        XCTAssertTrue(app.bridge.router.route(
            method: "fs.deleteFile",
            window: win,
            args: #"{"path":"\#(name)"}"#,
            resolve: { _ in dlExp.fulfill() },
            reject: { c, m in XCTFail("\(c) \(m)") }
        ))
        wait(for: [dlExp], timeout: 5.0)
    }

    func testBiometricIsAvailable_returnsDecodableShape() {
        let app = NativeViewApp()
        defer { app.destroy() }
        app.registerDefaultIOSHandlers()
        let win = app.createWindow(config: NativeViewConfig())
        let exp = expectation(description: "bio")
        XCTAssertTrue(app.bridge.router.route(
            method: "biometric.isAvailable",
            window: win,
            args: "null",
            resolve: { json in
                XCTAssertTrue(json.contains("available") || json.contains("false") || json.contains("true"), json)
                exp.fulfill()
            },
            reject: { c, m in XCTFail("\(c) \(m)") }
        ))
        wait(for: [exp], timeout: 3.0)
    }

    func testLocationUnwatch_noPermissionsRequired() {
        let app = NativeViewApp()
        defer { app.destroy() }
        app.registerDefaultIOSHandlers()
        let win = app.createWindow(config: NativeViewConfig())
        let exp = expectation(description: "unwatch")
        XCTAssertTrue(app.bridge.router.route(
            method: "location.unwatch",
            window: win,
            args: "null",
            resolve: { _ in exp.fulfill() },
            reject: { c, m in XCTFail("\(c) \(m)") }
        ))
        wait(for: [exp], timeout: 3.0)
    }

    func testNotificationCancel_rejectsInvalidArgsWithoutCallingUNCenter() {
        let app = NativeViewApp()
        defer { app.destroy() }
        app.registerDefaultIOSHandlers()
        let win = app.createWindow(config: NativeViewConfig())
        let exp = expectation(description: "reject")
        XCTAssertTrue(app.bridge.router.route(
            method: "notification.cancel",
            window: win,
            args: "[1]",
            resolve: { _ in XCTFail("resolve") },
            reject: { code, _ in
                XCTAssertEqual(code, NVRejectCode.invalidArgs)
                exp.fulfill()
            }
        ))
        wait(for: [exp], timeout: 5.0)
    }

    // MARK: - Error paths

    func testAppExit_rejectsNotAvailableOnIOS() {
        let app = NativeViewApp()
        defer { app.destroy() }
        app.registerDefaultIOSHandlers()
        let win = app.createWindow(config: NativeViewConfig())
        let exp = expectation(description: "reject")
        XCTAssertTrue(app.bridge.router.route(
            method: "app.exit",
            window: win,
            args: "null",
            resolve: { _ in XCTFail("resolve") },
            reject: { code, msg in
                XCTAssertEqual(code, "NOT_AVAILABLE")
                XCTAssertFalse(msg.isEmpty)
                exp.fulfill()
            }
        ))
        wait(for: [exp], timeout: 3.0)
    }

    func testStorageGet_rejectsWhenKeyMissing() {
        let app = NativeViewApp()
        defer { app.destroy() }
        app.registerDefaultIOSHandlers()
        let win = app.createWindow(config: NativeViewConfig())
        let exp = expectation(description: "reject")
        XCTAssertTrue(app.bridge.router.route(
            method: "storage.get",
            window: win,
            args: #"{"key":""}"#,
            resolve: { _ in XCTFail("resolve") },
            reject: { code, _ in
                XCTAssertEqual(code, "ERR_INVALID_ARG")
                exp.fulfill()
            }
        ))
        wait(for: [exp], timeout: 3.0)
    }

    func testFsReadFile_rejectsInvalidArgs() {
        let app = NativeViewApp()
        defer { app.destroy() }
        app.registerDefaultIOSHandlers()
        let win = app.createWindow(config: NativeViewConfig())
        let exp = expectation(description: "reject")
        XCTAssertTrue(app.bridge.router.route(
            method: "fs.readFile",
            window: win,
            args: #"{"oops":1}"#,
            resolve: { _ in XCTFail("resolve") },
            reject: { code, _ in
                XCTAssertEqual(code, NVRejectCode.invalidArgs)
                exp.fulfill()
            }
        ))
        wait(for: [exp], timeout: 3.0)
    }
}
