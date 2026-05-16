// SPDX-License-Identifier: Apache-2.0

import OSLog
import UIKit
import UniformTypeIdentifiers
import WebKit

/// Posted to ``WKUserContentController`` for `window.__nv.send` → native (`_NVBridge`).
public enum NativeViewWebViewBridge {
    public static let messageHandlerName = "_NVBridge"
}

/// `WKWebView` subclass wired for nativeview bridge injection, navigation policy, and JS UI prompts.
public final class NativeViewWebView: WKWebView, WKNavigationDelegate, WKUIDelegate {
    private static let log = Logger(subsystem: "NativeViewIOS", category: "WebView")

    public weak var bridgeOwner: NVBridge?
    public weak var hostWindow: NativeViewWindow?

    private let windowConfig: NativeViewConfig
    private var retainedDocumentPickerDelegate: NSObject?

    /// Builds configuration with JS enabled, inline media, bridge handler, and injected scripts.
    private static func makeConfiguration(bridgeHandler: WKScriptMessageHandler) -> WKWebViewConfiguration {
        let prefs = WKPreferences()
        prefs.javaScriptEnabled = true

        let config = WKWebViewConfiguration()
        config.preferences = prefs
        config.allowsInlineMediaPlayback = true
        config.mediaTypesRequiringUserActionForPlayback = []

        let controller = WKUserContentController()
        controller.add(bridgeHandler, name: NativeViewWebViewBridge.messageHandlerName)
        config.userContentController = controller

        let shimSource = """
        (function(){
          window.__nv = window.__nv || {};
          window.__nv.send = function(_event, json) {
            try {
              window.webkit.messageHandlers.\(NativeViewWebViewBridge.messageHandlerName).postMessage(String(json));
            } catch (e) {}
          };
        })();
        """
        controller.addUserScript(
            WKUserScript(source: shimSource, injectionTime: .atDocumentStart, forMainFrameOnly: true)
        )

        let js = loadNativeViewJS()
        if !js.isEmpty {
            controller.addUserScript(
                WKUserScript(source: js, injectionTime: .atDocumentStart, forMainFrameOnly: true)
            )
        }

        return config
    }

    private static func loadNativeViewJS() -> String {
        guard let url = Bundle.module.url(forResource: "nativeview", withExtension: "js"),
              let js = try? String(contentsOf: url, encoding: .utf8)
        else {
            return ""
        }
        return js
    }

    public init(windowConfig: NativeViewConfig, bridgeHandler: NVBridge) {
        self.windowConfig = windowConfig
        let configuration = Self.makeConfiguration(bridgeHandler: bridgeHandler)
        super.init(frame: .zero, configuration: configuration)
        navigationDelegate = self
        uiDelegate = self
        translatesAutoresizingMaskIntoConstraints = false
        if #available(iOS 16.4, *) {
            isInspectable = windowConfig.devtools
        }
    }

    @available(*, unavailable)
    public required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    func prepareForTeardown() {
        configuration.userContentController.removeScriptMessageHandler(forName: NativeViewWebViewBridge.messageHandlerName)
        navigationDelegate = nil
        uiDelegate = nil
        retainedDocumentPickerDelegate = nil
    }

    func refreshPopGestureEnabled() {
        guard let nav = findNavigationController() else { return }
        if NativeViewIOS.isSwipeBackGestureForcedOff() {
            nav.interactivePopGestureRecognizer?.isEnabled = false
            return
        }
        nav.interactivePopGestureRecognizer?.isEnabled = !canGoBack
    }

    private func findNavigationController() -> UINavigationController? {
        var r: UIResponder? = self
        while let cur = r {
            if let nav = cur as? UINavigationController { return nav }
            r = cur.next
        }
        return nil
    }

    // MARK: - WKNavigationDelegate

    public func webView(_ webView: WKWebView, decidePolicyFor navigationAction: WKNavigationAction, decisionHandler: @escaping (WKNavigationActionPolicy) -> Void) {
        guard let url = navigationAction.request.url else {
            decisionHandler(.allow)
            return
        }
        let scheme = (url.scheme ?? "").lowercased()
        if scheme == "about" || scheme == "data" {
            decisionHandler(.allow)
            return
        }
        guard let app = hostWindow?.app else {
            decisionHandler(.allow)
            return
        }
        let allowed = app.allowedOriginsSnapshot()
        if NVBridgeOrigin.isAllowed(pageURL: url, allowed: allowed) {
            decisionHandler(.allow)
        } else {
            Self.log.warning("Navigation cancelled: origin not allowed for \(url.absoluteString, privacy: .public)")
            decisionHandler(.cancel)
        }
    }

    public func webView(_ webView: WKWebView, didCommit navigation: WKNavigation!) {
        refreshPopGestureEnabled()
    }

    public func webView(_ webView: WKWebView, didFinish navigation: WKNavigation!) {
        hostWindow?.notifyReady()
        refreshPopGestureEnabled()
    }

    public func webView(_ webView: WKWebView, didFail navigation: WKNavigation!, withError error: Error) {
        Self.log.error("Navigation failed: \(error.localizedDescription, privacy: .public)")
    }

    public func webView(_ webView: WKWebView, didFailProvisionalNavigation navigation: WKNavigation!, withError error: Error) {
        Self.log.error("Provisional navigation failed: \(error.localizedDescription, privacy: .public)")
    }

    public func webViewWebContentProcessDidTerminate(_ webView: WKWebView) {
        hostWindow?.notifyClose()
    }

    // MARK: - WKUIDelegate

    public func webView(
        _ webView: WKWebView,
        runJavaScriptAlertPanelWithMessage message: String,
        initiatedByFrame frame: WKFrameInfo,
        completionHandler: @escaping () -> Void
    ) {
        presentBlockingAlert(title: nil, message: message, confirmTitle: "OK") { _ in
            completionHandler()
        }
    }

    public func webView(
        _ webView: WKWebView,
        runJavaScriptConfirmPanelWithMessage message: String,
        initiatedByFrame frame: WKFrameInfo,
        completionHandler: @escaping (Bool) -> Void
    ) {
        presentBlockingAlert(title: nil, message: message, confirmTitle: "OK", cancelTitle: "Cancel") { ok in
            completionHandler(ok)
        }
    }

    public func webView(
        _ webView: WKWebView,
        runJavaScriptTextInputPanelWithPrompt prompt: String,
        defaultText: String?,
        initiatedByFrame frame: WKFrameInfo,
        completionHandler: @escaping (String?) -> Void
    ) {
        guard let host = hostViewController() else {
            completionHandler(nil)
            return
        }
        let alert = UIAlertController(title: nil, message: prompt, preferredStyle: .alert)
        alert.addTextField { $0.text = defaultText }
        alert.addAction(UIAlertAction(title: "OK", style: .default) { _ in
            completionHandler(alert.textFields?.first?.text)
        })
        alert.addAction(UIAlertAction(title: "Cancel", style: .cancel) { _ in
            completionHandler(nil)
        })
        host.present(alert, animated: true)
    }

    @available(iOS 18.4, *)
    public func webView(
        _ webView: WKWebView,
        runOpenPanelWith parameters: WKOpenPanelParameters,
        initiatedByFrame frame: WKFrameInfo,
        completionHandler: @escaping ([URL]?) -> Void
    ) {
        guard let host = hostViewController() else {
            completionHandler(nil)
            return
        }
        let picker = UIDocumentPickerViewController(forOpeningContentTypes: [.item], asCopy: true)
        picker.allowsMultipleSelection = parameters.allowsMultipleSelection
        let delegate = DocumentPickerCompletionDelegate(webView: self, completionHandler: completionHandler)
        retainedDocumentPickerDelegate = delegate
        picker.delegate = delegate
        host.present(picker, animated: true)
    }

    fileprivate func clearRetainedDocumentPickerDelegate() {
        retainedDocumentPickerDelegate = nil
    }

    private func hostViewController() -> UIViewController? {
        var r: UIResponder? = self
        while let cur = r {
            if let vc = cur as? UIViewController { return vc }
            r = cur.next
        }
        return nil
    }

    private func presentBlockingAlert(
        title: String?,
        message: String,
        confirmTitle: String,
        cancelTitle: String? = nil,
        completion: @escaping (Bool) -> Void
    ) {
        guard let host = hostViewController() else {
            completion(cancelTitle == nil)
            return
        }
        let alert = UIAlertController(title: title, message: message, preferredStyle: .alert)
        alert.addAction(UIAlertAction(title: confirmTitle, style: .default) { _ in
            completion(true)
        })
        if let cancelTitle {
            alert.addAction(UIAlertAction(title: cancelTitle, style: .cancel) { _ in
                completion(false)
            })
        }
        host.present(alert, animated: true)
    }
}

@available(iOS 18.4, *)
private final class DocumentPickerCompletionDelegate: NSObject, UIDocumentPickerDelegate {
    private weak var webView: NativeViewWebView?
    private let completionHandler: ([URL]?) -> Void

    init(webView: NativeViewWebView, completionHandler: @escaping ([URL]?) -> Void) {
        self.webView = webView
        self.completionHandler = completionHandler
    }

    func documentPicker(_ controller: UIDocumentPickerViewController, didPickDocumentsAt urls: [URL]) {
        completionHandler(urls.isEmpty ? nil : urls)
        webView?.clearRetainedDocumentPickerDelegate()
    }

    func documentPickerWasCancelled(_ controller: UIDocumentPickerViewController) {
        completionHandler(nil)
        webView?.clearRetainedDocumentPickerDelegate()
    }
}
