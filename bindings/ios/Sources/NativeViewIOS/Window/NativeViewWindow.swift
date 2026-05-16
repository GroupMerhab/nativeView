// SPDX-License-Identifier: Apache-2.0

import UIKit

/// Native-backed window host for a nativeview page (parity with desktop bindings); embed via ``viewController()``.
public final class NativeViewWindow: @unchecked Sendable {
    weak var app: NativeViewApp?

    private let rootViewController: NativeViewViewController
    private let lock = NSLock()
    private var messageCallback: MessageCallback?
    private var readyCallback: ReadyCallback?
    private var closeCallback: CloseCallback?
    private var config: NativeViewConfig

    init(app: NativeViewApp?, config: NativeViewConfig) {
        self.app = app
        self.config = config
        let bridge = app?.bridge ?? NVBridge()
        let vc = NativeViewViewController(bridge: bridge, config: config)
        self.rootViewController = vc
        vc.hostWindow = self
        vc.webView.hostWindow = self
    }

    /// Shows the hosted view (sets `isHidden` on the root view once loaded).
    public func show() {
        DispatchQueue.main.async { [weak self] in
            guard let self else { return }
            self.rootViewController.loadViewIfNeeded()
            self.rootViewController.hostViewIsHidden = false
        }
    }

    /// Hides the hosted view.
    public func hide() {
        DispatchQueue.main.async { [weak self] in
            self?.rootViewController.hostViewIsHidden = true
        }
    }

    public func loadUrl(_ url: String) {
        DispatchQueue.main.async { [weak self] in
            guard let self else { return }
            self.rootViewController.loadViewIfNeeded()
            let wv = self.rootViewController.webView
            guard let u = URL(string: url) else { return }
            // `load(_:)` does not grant the WebContent process read access to `file:` URLs; use `loadFileURL` instead.
            if u.isFileURL {
                let readAccess = u.deletingLastPathComponent()
                wv.loadFileURL(u, allowingReadAccessTo: readAccess)
            } else {
                wv.load(URLRequest(url: u))
            }
        }
    }

    public func loadHtml(_ html: String, baseUrl: String = "about:blank") {
        DispatchQueue.main.async { [weak self] in
            guard let self else { return }
            self.rootViewController.loadViewIfNeeded()
            let wv = self.rootViewController.webView
            wv.loadHTMLString(html, baseURL: URL(string: baseUrl))
        }
    }

    /// When set (e.g. by `@testable` unit tests), ``evalJs(_:)`` forwards here instead of evaluating in WKWebView.
    internal var evalJsCapture: ((String) -> Void)?

    public func evalJs(_ js: String) {
        DispatchQueue.main.async { [weak self] in
            guard let self else { return }
            if let cap = self.evalJsCapture {
                cap(js)
                return
            }
            self.rootViewController.loadViewIfNeeded()
            let wv = self.rootViewController.webView
            wv.evaluateJavaScript(js, completionHandler: nil)
        }
    }

    public func setTitle(_ title: String) {
        config.title = title
        DispatchQueue.main.async { [weak self] in
            self?.rootViewController.title = title
        }
    }

    public func setSize(width: Int, height: Int) {
        config.width = width
        config.height = height
        DispatchQueue.main.async { [weak self] in
            guard let self else { return }
            self.rootViewController.preferredContentSize = CGSize(width: width, height: height)
        }
    }

    public func onMessage(_ callback: @escaping MessageCallback) {
        lock.lock()
        defer { lock.unlock() }
        messageCallback = callback
    }

    public func onReady(_ callback: @escaping ReadyCallback) {
        lock.lock()
        defer { lock.unlock() }
        readyCallback = callback
    }

    public func onClose(_ callback: @escaping CloseCallback) {
        lock.lock()
        defer { lock.unlock() }
        closeCallback = callback
    }

    /// Sends a push event into the page (`window.__nv._emit` when nativeview.js is loaded).
    public func emit(event: String, json: String) {
        let js =
            "(function(){try{if(window.__nv&&typeof window.__nv._emit==='function'){window.__nv._emit(\(NVJavaScriptStringEncoding.jsStringLiteral(event)), \(NVJavaScriptStringEncoding.jsStringLiteral(json)));}}catch(e){}})()"
        evalJs(js)
    }

    /// Returns the root view controller to embed in your hierarchy (UINavigationController, UITabBarController, etc.).
    public func viewController() -> UIViewController {
        rootViewController
    }

    func notifyMessage(event: String, json: String) {
        lock.lock()
        let cb = messageCallback
        lock.unlock()
        cb?(event, json)
    }

    func notifyReady() {
        lock.lock()
        let cb = readyCallback
        lock.unlock()
        cb?()
    }

    func notifyClose() {
        lock.lock()
        let cb = closeCallback
        lock.unlock()
        cb?()
    }

    func tearDown() {
        DispatchQueue.main.async { [weak self] in
            guard let self else { return }
            self.rootViewController.detachWebView()
        }
    }
}
