package com.nativeview.testutil;

import android.app.Activity;
import android.os.Bundle;
import android.webkit.WebView;
import com.nativeview.CountingNativeViewApp;
import com.nativeview.NativeViewConfig;
import com.nativeview.NativeViewWindow;
import com.nativeview.bridge.NVBridge;

/**
 * Minimal activity for instrumented bridge tests: real {@link WebView}, {@link NativeViewWindow},
 * and {@link NVBridge}.
 */
public class BridgeProbeActivity extends Activity {

    private CountingNativeViewApp app;
    private NativeViewWindow window;
    private WebView webView;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        app = new CountingNativeViewApp(this);
        app.registerDefaultAndroidHandlers();
        window = app.create_window(new NativeViewConfig());
        if (window == null) {
            throw new IllegalStateException("create_window returned null (JNI / native init)");
        }
        webView = new WebView(this);
        webView.getSettings().setJavaScriptEnabled(true);
        webView.addJavascriptInterface(new NVBridge(app, window, webView), "_NVBridge");
        setContentView(webView);
    }

    @Override
    protected void onDestroy() {
        if (app != null) {
            app.destroy();
        }
        super.onDestroy();
    }

    public CountingNativeViewApp getCountingApp() {
        return app;
    }

    public NativeViewWindow getWindowHandle() {
        return window;
    }

    public WebView getWebView() {
        return webView;
    }
}
