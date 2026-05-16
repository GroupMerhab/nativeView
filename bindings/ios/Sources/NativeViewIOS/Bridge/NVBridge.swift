// SPDX-License-Identifier: Apache-2.0

import Foundation
import WebKit

/// Central coordinator for JSON-RPC style messages between native and WebView JS.
/// Implements ``WKScriptMessageHandler`` for `window.__nv.send` → `_NVBridge` (see ``NativeViewWebViewBridge``).
public final class NVBridge: NSObject, WKScriptMessageHandler {
    public let router: NVRouter
    public let permissionManager: NVPermissionManager

    public init(
        router: NVRouter = NVRouter(),
        permissionManager: NVPermissionManager = NVPermissionManager()
    ) {
        self.router = router
        self.permissionManager = permissionManager
        super.init()
    }

    public func userContentController(
        _ controller: WKUserContentController,
        didReceive message: WKScriptMessage
    ) {
        guard message.name == NativeViewWebViewBridge.messageHandlerName,
              let body = message.body as? String,
              let webView = message.webView as? NativeViewWebView,
              let win = webView.hostWindow,
              let app = win.app
        else {
            return
        }

        let pageURL = webView.url
        app.deliverWebMessage(from: win, pageURL: pageURL, wireJson: body)

        let seq = NVWireHelpers.extractSeq(body)
        if seq == 0, let method = NVWireHelpers.parseMethod(body) {
            win.notifyMessage(event: method, json: body)
        }
    }

    /// Resolves a pending JS RPC by wire sequence id (parity with ``NVWebBridgeClient/buildResolveInvoke``).
    public func resolvePromise(webView: WKWebView?, seq: Int64, resultJSON: String) {
        guard let webView, let js = NVWebBridgeClient.buildResolveInvoke(seq: seq, json: resultJSON) else { return }
        evaluateOnMain(webView: webView, js: js)
    }

    /// Rejects a pending JS RPC (parity with ``NVWebBridgeClient/buildRejectInvoke``).
    public func rejectPromise(webView: WKWebView?, seq: Int64, code: String, message: String) {
        guard let webView, let js = NVWebBridgeClient.buildRejectInvoke(seq: seq, code: code, message: message) else { return }
        evaluateOnMain(webView: webView, js: js)
    }

    /// Delivers a push event into the page via `window.__nv._emit` when ``nativeview.js`` is loaded.
    public func emit(webView: WKWebView?, event: String, json: String) {
        guard let webView else { return }
        let ev = NVJavaScriptStringEncoding.jsStringLiteral(event)
        let payload = NVJavaScriptStringEncoding.jsStringLiteral(json)
        let js =
            "(function(){try{if(window.__nv&&typeof window.__nv._emit==='function'){window.__nv._emit(\(ev), \(payload));}}catch(e){}})()"
        evaluateOnMain(webView: webView, js: js)
    }

    private func evaluateOnMain(webView: WKWebView, js: String) {
        if Thread.isMainThread {
            webView.evaluateJavaScript(js, completionHandler: nil)
        } else {
            DispatchQueue.main.async {
                webView.evaluateJavaScript(js, completionHandler: nil)
            }
        }
    }
}
