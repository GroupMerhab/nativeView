package com.nativeview.bridge;

import android.webkit.JavascriptInterface;
import android.webkit.WebView;
import com.nativeview.NativeViewApp;
import com.nativeview.NativeViewWindow;

/**
 * {@link JavascriptInterface} entry point for {@code nativeview.js}: {@code window.__nv.send}
 * forwards the serialized wire object to {@link #post(String)} ({@code _NVBridge.post} on Android).
 */
public final class NVBridge {

    private final NativeViewApp app;
    private final NativeViewWindow window;
    private final WebView webView;

    public NVBridge(NativeViewApp app, NativeViewWindow window, WebView webView) {
        this.app = app;
        this.window = window;
        this.webView = webView;
    }

    /**
     * Receives the full wire JSON string (fields {@code e}, {@code d}, optional {@code s}) from
     * JavaScript. Runs on a background thread; work is posted to the WebView UI thread.
     */
    @JavascriptInterface
    public void post(String json) {
        if (json == null || app == null || window == null || webView == null) {
            return;
        }
        webView.post(() -> app.deliverWebMessage(webView, window, json));
    }
}
