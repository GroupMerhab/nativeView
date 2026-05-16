// SPDX-License-Identifier: Apache-2.0

import Foundation
import UIKit

/// Owns app-level bridge registration, origin policy, and window factory for a nativeview host on iOS.
public final class NativeViewApp: @unchecked Sendable {
    private static let wireVersion = 1

    public let bridge: NVBridge

    private let lock = NSLock()
    private var allowedOrigins = Set<String>()
    private var destroyed = false
    private var windows: [WeakNativeViewWindow] = []
    private var notificationObservers: [NSObjectProtocol] = []
    /// Coalesces `UIApplication` + `UIScene` pairs that fire back-to-back on scene-based apps.
    private var lastResumeAt: TimeInterval = -1_000_000
    private var lastPauseAt: TimeInterval = -1_000_000
    private let lifecycleDedupInterval: TimeInterval = 0.08

    public init(bridge: NVBridge = NVBridge()) {
        self.bridge = bridge
        if let fileOrigin = NVBridgeOrigin.normalizeAllowedOrigin("file://") {
            allowedOrigins.insert(fileOrigin)
        }
        installSystemObservers()
    }

    /// Registers built-in iOS bridge methods (`device.*`, `storage.*`, `network.*`, …). Safe to call more than once (overwrites the same keys).
    public func registerDefaultIOSHandlers() {
        IOSDefaultBridge.registerAll(self)
    }

    /// Identical to desktop binding: creates a host window (UIKit + WKWebView).
    public func createWindow(config: NativeViewConfig) -> NativeViewWindow {
        lock.lock()
        let dead = destroyed
        lock.unlock()
        let win = NativeViewWindow(app: dead ? nil : self, config: config)
        if !dead {
            trackWindow(win)
        }
        return win
    }

    /// No-op on iOS; UIKit drives the run loop.
    public func run() {}

    /// Best-effort parity with desktop `nv_app_quit` (native run loop is not used on iOS).
    public func quit() {}

    /// Broadcasts a push event to every tracked window (`window.__nv._emit` when ``nativeview.js`` is loaded).
    public func emit(event: String, json: String) {
        lock.lock()
        let dead = destroyed
        let tracked = windows.compactMap { $0.value }
        lock.unlock()
        if dead { return }
        for w in tracked {
            w.emit(event: event, json: json)
        }
    }

    /// Delivers a deep link to all windows via ``NativeView/_emit`` (`app.deeplink` with `{ url }`).
    public func handleDeepLink(url: URL) {
        lock.lock()
        let dead = destroyed
        let tracked = windows.compactMap { $0.value }
        lock.unlock()
        if dead { return }
        let payload: [String: Any] = ["url": url.absoluteString]
        guard JSONSerialization.isValidJSONObject(payload),
              let data = try? JSONSerialization.data(withJSONObject: payload, options: []),
              let objJson = String(data: data, encoding: .utf8)
        else {
            return
        }
        let eventLit = NVJavaScriptStringEncoding.jsStringLiteral("app.deeplink")
        let js =
            "(function(){try{if(typeof NativeView!=='undefined'&&typeof NativeView._emit==='function'){NativeView._emit(\(eventLit),\(objJson));}}catch(e){}})()"
        for w in tracked {
            w.evalJs(js)
        }
    }

    public func destroy() {
        removeSystemObservers()
        lock.lock()
        if destroyed {
            lock.unlock()
            return
        }
        destroyed = true
        let tracked = windows
        windows.removeAll()
        allowedOrigins.removeAll()
        IOSDefaultBridge.unregisterAll(self)
        bridge.router.clearMethodRoutes()
        bridge.router.clearPrefixHandlers()
        lock.unlock()

        for w in tracked {
            w.value?.tearDown()
        }
    }

    private func installSystemObservers() {
        let center = NotificationCenter.default
        let main = OperationQueue.main

        notificationObservers.append(center.addObserver(
            forName: UIApplication.didBecomeActiveNotification,
            object: nil,
            queue: main
        ) { [weak self] _ in
            self?.emitResumeDeduped()
        })

        notificationObservers.append(center.addObserver(
            forName: UIApplication.willResignActiveNotification,
            object: nil,
            queue: main
        ) { [weak self] _ in
            self?.emitPauseDeduped()
        })

        notificationObservers.append(center.addObserver(
            forName: UIApplication.willTerminateNotification,
            object: nil,
            queue: main
        ) { [weak self] _ in
            self?.emit(event: "app.destroy", json: "{}")
        })

        if #available(iOS 13.0, *) {
            notificationObservers.append(center.addObserver(
                forName: UIScene.didActivateNotification,
                object: nil,
                queue: main
            ) { [weak self] _ in
                self?.emitResumeDeduped()
            })
            notificationObservers.append(center.addObserver(
                forName: UIScene.willDeactivateNotification,
                object: nil,
                queue: main
            ) { [weak self] _ in
                self?.emitPauseDeduped()
            })
            notificationObservers.append(center.addObserver(
                forName: UIScene.didDisconnectNotification,
                object: nil,
                queue: main
            ) { [weak self] _ in
                self?.emit(event: "app.destroy", json: "{}")
            })
        }

        notificationObservers.append(center.addObserver(
            forName: UIDevice.orientationDidChangeNotification,
            object: nil,
            queue: main
        ) { [weak self] _ in
            let landscape = UIDevice.current.orientation.isLandscape
            self?.emit(event: "device.orientation", json: "{\"landscape\":\(landscape)}")
        })
    }

    private func removeSystemObservers() {
        let center = NotificationCenter.default
        for o in notificationObservers {
            center.removeObserver(o)
        }
        notificationObservers.removeAll()
    }

    private func emitResumeDeduped() {
        let t = ProcessInfo.processInfo.systemUptime
        if t - lastResumeAt < lifecycleDedupInterval { return }
        lastResumeAt = t
        emit(event: "app.resume", json: "{}")
    }

    private func emitPauseDeduped() {
        let t = ProcessInfo.processInfo.systemUptime
        if t - lastPauseAt < lifecycleDedupInterval { return }
        lastPauseAt = t
        emit(event: "app.pause", json: "{}")
    }

    /// Adds an allowed document origin for the WebView bridge (scheme + host + optional port).
    public func allowOrigin(_ origin: String) {
        lock.lock()
        defer { lock.unlock() }
        if destroyed { return }
        guard let n = NVBridgeOrigin.normalizeAllowedOrigin(origin) else { return }
        allowedOrigins.insert(n)
    }

    /// Registers a Swift handler for a single bridge method key (e.g. `storage.get`).
    public func handle(
        _ method: String,
        permissions: [NVPermission] = [],
        handler: @escaping NVHandlerFn
    ) {
        lock.lock()
        defer { lock.unlock() }
        if destroyed { return }
        bridge.router.register(method: method, permissions: permissions, handler: handler)
    }

    func allowedOriginsSnapshot() -> Set<String> {
        lock.lock()
        defer { lock.unlock() }
        return allowedOrigins
    }

    func isDestroyed() -> Bool {
        lock.lock()
        defer { lock.unlock() }
        return destroyed
    }

    /// Entry from ``NativeViewViewController`` when the page posts compact wire JSON via `window.__nv.send`.
    func deliverWebMessage(from hostWindow: NativeViewWindow, pageURL: URL?, wireJson: String) {
        if isDestroyed() { return }
        guard hostWindow.app != nil else { return }

        let seq = NVWireHelpers.extractSeq(wireJson)
        guard let method = NVWireHelpers.parseMethod(wireJson) else {
            rejectWire(window: hostWindow, seq: seq, code: "ERR_INVALID_JSON", message: "parse error")
            return
        }

        if !NVBridgeOrigin.isAllowed(pageURL: pageURL, allowed: allowedOriginsSnapshot()) {
            rejectWire(window: hostWindow, seq: seq, code: "PERMISSION_DENIED", message: "Origin not allowed")
            return
        }

        if method == "app.handshake" {
            handleHandshake(window: hostWindow, seq: seq, wireJson: wireJson)
            return
        }

        if let route = bridge.router.methodRoute(forExactKey: method) {
            dispatchClosureRoute(window: hostWindow, seq: seq, wireJson: wireJson, route: route)
            return
        }

        if let handler = bridge.router.handler(forMethodKey: method) {
            Task { @MainActor in
                await dispatchProtocolHandler(
                    window: hostWindow,
                    seq: seq,
                    wireJson: wireJson,
                    method: method,
                    handler: handler
                )
            }
            return
        }

        if seq > 0 {
            rejectWire(window: hostWindow, seq: seq, code: "NOT_AVAILABLE", message: "No handler for \(method)")
        }
    }

    private func handleHandshake(window: NativeViewWindow, seq: Int64, wireJson: String) {
        guard seq > 0 else { return }
        var jsWire: Int?
        if let argsJson = NVWireHelpers.extractArgsJson(wireJson),
           let data = argsJson.data(using: .utf8),
           let obj = try? JSONSerialization.jsonObject(with: data) as? [String: Any]
        {
            if let w = obj["wireVersion"] as? Int {
                jsWire = w
            } else if let n = obj["wireVersion"] as? NSNumber {
                jsWire = n.intValue
            }
        }
        if let jsWire, jsWire != Self.wireVersion {
            rejectWire(window: window, seq: seq, code: "ERR_VERSION_MISMATCH", message: "wire version mismatch")
            return
        }
        let reply: [String: Any] = [
            "wireVersion": Self.wireVersion,
            "cVersion": NVCBridge.versionString(),
            "platform": "ios",
            "windowId": "ios",
        ]
        guard let out = jsonString(reply) else {
            rejectWire(window: window, seq: seq, code: "ERR_JSON", message: "handshake encode failed")
            return
        }
        resolveWire(window: window, seq: seq, json: out)
    }

    private func jsonString(_ obj: [String: Any]) -> String? {
        guard JSONSerialization.isValidJSONObject(obj),
              let data = try? JSONSerialization.data(withJSONObject: obj, options: [])
        else {
            return nil
        }
        return String(data: data, encoding: .utf8)
    }

    private func dispatchClosureRoute(
        window: NativeViewWindow,
        seq: Int64,
        wireJson: String,
        route: NVRouter.MethodRoute
    ) {
        guard let argsJson = NVWireHelpers.extractArgsJson(wireJson) else {
            rejectWire(window: window, seq: seq, code: "ERR_INVALID_JSON", message: "parse error")
            return
        }

        let resolve: ResolveCallback = { [weak self, weak window] json in
            guard let self, let window else { return }
            if seq <= 0 { return }
            self.resolveWire(window: window, seq: seq, json: json)
        }
        let reject: RejectCallback = { [weak self, weak window] code, message in
            guard let self, let window else { return }
            if seq <= 0 { return }
            self.rejectWire(window: window, seq: seq, code: code, message: message)
        }

        bridge.permissionManager.check(
            permissions: route.permissions,
            then: {
                DispatchQueue.global(qos: .userInitiated).async {
                    route.handler(window, argsJson, resolve, reject)
                }
            },
            denied: {
                reject("PERMISSION_DENIED", "Permission denied")
            }
        )
    }

    @MainActor
    private func dispatchProtocolHandler(
        window: NativeViewWindow,
        seq: Int64,
        wireJson: String,
        method: String,
        handler: NVHandler
    ) async {
        guard let argsJson = NVWireHelpers.extractArgsJson(wireJson) else {
            rejectWire(window: window, seq: seq, code: "ERR_INVALID_JSON", message: "parse error")
            return
        }
        let args: [String: Any]
        do {
            args = try NVJsonWire.decodeObject(argsJson: argsJson)
        } catch let e as NVJsonWire.WireError {
            switch e {
            case .invalidJson:
                rejectWire(window: window, seq: seq, code: "ERR_INVALID_JSON", message: "args parse error")
            case .invalidArgs:
                rejectWire(window: window, seq: seq, code: NVRejectCode.invalidArgs, message: "malformed arguments")
            }
            return
        } catch {
            rejectWire(window: window, seq: seq, code: NVRejectCode.invalidArgs, message: "malformed arguments")
            return
        }
        do {
            let result = try await handler.handle(methodKey: method, arguments: args)
            if seq <= 0 { return }
            if let result {
                if JSONSerialization.isValidJSONObject(result),
                   let rd = try? JSONSerialization.data(withJSONObject: result, options: []),
                   let rs = String(data: rd, encoding: .utf8)
                {
                    resolveWire(window: window, seq: seq, json: rs)
                } else if let s = result as? String {
                    resolveWire(window: window, seq: seq, json: jsonQuoteString(s) ?? "\"\"")
                } else {
                    resolveWire(window: window, seq: seq, json: jsonQuoteString(String(describing: result)) ?? "null")
                }
            } else {
                resolveWire(window: window, seq: seq, json: "null")
            }
        } catch {
            rejectWire(
                window: window,
                seq: seq,
                code: "ERR_HANDLER",
                message: error.localizedDescription
            )
        }
    }

    private func jsonQuoteString(_ s: String) -> String? {
        NVJavaScriptStringEncoding.jsStringLiteralFromArraySliceOrNil(s)
    }

    private func resolveWire(window: NativeViewWindow, seq: Int64, json: String) {
        guard let js = NVWebBridgeClient.buildResolveInvoke(seq: seq, json: json) else { return }
        window.evalJs(js)
    }

    func rejectWire(window: NativeViewWindow, seq: Int64, code: String, message: String) {
        guard let js = NVWebBridgeClient.buildRejectInvoke(seq: seq, code: code, message: message) else { return }
        window.evalJs(js)
    }

    private func trackWindow(_ window: NativeViewWindow) {
        lock.lock()
        windows.removeAll { $0.value == nil }
        windows.append(WeakNativeViewWindow(window))
        lock.unlock()
    }
}

private final class WeakNativeViewWindow {
    weak var value: NativeViewWindow?
    init(_ value: NativeViewWindow) {
        self.value = value
    }
}
