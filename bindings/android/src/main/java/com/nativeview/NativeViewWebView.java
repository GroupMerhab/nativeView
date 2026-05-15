package com.nativeview;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.Bitmap;
import android.net.Uri;
import android.util.AttributeSet;
import android.util.Log;
import android.webkit.ConsoleMessage;
import android.webkit.PermissionRequest;
import android.webkit.ValueCallback;
import android.webkit.WebChromeClient;
import android.webkit.WebResourceError;
import android.webkit.WebResourceRequest;
import android.webkit.WebResourceResponse;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import com.nativeview.bridge.BridgeJavaScript;
import com.nativeview.bridge.NVBridge;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.util.Base64;

/**
 * Opinionated {@link WebView} for nativeview: enables common settings, injects {@code
 * assets/nativeview.js} on each navigation, surfaces ready/close hooks, and wires default {@link
 * WebViewClient} / {@link WebChromeClient} behavior.
 */
@SuppressLint("SetJavaScriptEnabled")
public class NativeViewWebView extends WebView {

    private static final String TAG = "NativeViewWebView";

    private volatile String cachedBridgeScript;
    private volatile boolean bridgeLoadFailed;

    private ReadyCallback onReady;
    private CloseCallback onClose;
    private NavigationListener navigationListener;
    private PageErrorListener pageErrorListener;
    private FileChooserHandler fileChooserHandler;
    private WebPermissionHandler webPermissionHandler;

    public NativeViewWebView(Context context) {
        super(context);
        init();
    }

    public NativeViewWebView(Context context, AttributeSet attrs) {
        super(context, attrs);
        init();
    }

    public NativeViewWebView(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        init();
    }

    /** Invoked after {@code nativeview.js} has been evaluated for the current page. */
    public void setOnReady(ReadyCallback callback) {
        this.onReady = callback;
    }

    /** Invoked when the host should treat the WebView as “closed” (see {@link #handleBackPressed()}). */
    public void setOnClose(CloseCallback callback) {
        this.onClose = callback;
    }

    public void setNavigationListener(NavigationListener listener) {
        this.navigationListener = listener;
    }

    public void setPageErrorListener(PageErrorListener listener) {
        this.pageErrorListener = listener;
    }

    public void setFileChooserHandler(FileChooserHandler handler) {
        this.fileChooserHandler = handler;
    }

    public void setWebPermissionHandler(WebPermissionHandler handler) {
        this.webPermissionHandler = handler;
    }

    /**
     * Back handling: navigate history when possible; otherwise {@link #triggerOnClose()}.
     *
     * @return {@code true} if the press was consumed (went back or close was signaled).
     */
    public boolean handleBackPressed() {
        if (canGoBack()) {
            goBack();
            return true;
        }
        triggerOnClose();
        return true;
    }

    /** Notifies {@link #setOnClose(CloseCallback)} when set. */
    public void triggerOnClose() {
        if (onClose != null) {
            onClose.onClose();
        }
    }

    private void init() {
        WebSettings s = getSettings();
        s.setJavaScriptEnabled(true);
        s.setDomStorageEnabled(true);
        s.setAllowFileAccess(true);
        s.setMediaPlaybackRequiresUserGesture(false);
        s.setMixedContentMode(WebSettings.MIXED_CONTENT_ALWAYS_ALLOW);

        setWebViewClient(new NativeViewWebViewClient());
        setWebChromeClient(new NativeViewWebChromeClient());
    }

    private void injectBridgeThenReady() {
        String script = getBridgeScript();
        if (script == null || script.isEmpty()) {
            fireReady();
            return;
        }
        evaluateJavascript(wrapNativeViewJsForAndroid(script), value -> fireReady());
    }

    /**
     * Installs {@code _NVBridge} for {@code nativeview.js} and must be called for each WebView that
     * should use the Java bridge (typically from the activity after {@link NativeViewWindow} is
     * created).
     */
    public void attachNativeBridge(NativeViewApp app, NativeViewWindow window) {
        if (app == null || window == null) {
            return;
        }
        addJavascriptInterface(new NVBridge(app, window, this), "_NVBridge");
    }

    /**
     * Emits a push event into the page via {@code NativeView._emit(event, json)} (requires {@link
     * #wrapNativeViewJsForAndroid} injection so {@code NativeView._emit} exists).
     */
    public void emitToJs(String event, String jsonPayload) {
        BridgeJavaScript.emitEvent(this, event, jsonPayload);
    }

    private void fireReady() {
        ReadyCallback cb = onReady;
        if (cb != null) {
            cb.onReady();
        }
    }

    private String getBridgeScript() {
        if (bridgeLoadFailed) {
            return null;
        }
        String local = cachedBridgeScript;
        if (local != null) {
            return local;
        }
        synchronized (this) {
            if (cachedBridgeScript != null) {
                return cachedBridgeScript;
            }
            if (bridgeLoadFailed) {
                return null;
            }
            try {
                cachedBridgeScript = NativeViewAndroid.loadBridgeScript(getContext());
                return cachedBridgeScript;
            } catch (IOException ex) {
                bridgeLoadFailed = true;
                Log.e(TAG, "Could not read assets/nativeview.js", ex);
                return null;
            }
        }
    }

    /**
     * Wraps the bridge source as an IIFE that base64-decodes and {@code eval}s it inside the page,
     * avoiding fragile string escaping for {@link #evaluateJavascript(String, ValueCallback)}.
     */
    static String createInjectJavascriptExpression(String script) {
        if (script == null || script.isEmpty()) {
            return "(function(){})();";
        }
        String b64 = Base64.getEncoder().encodeToString(script.getBytes(StandardCharsets.UTF_8));
        return "(function(){try{var s=atob('"
                + b64
                + "');(0,eval)(s);}catch(e){console.error('nativeview inject',e);}})();";
    }

    /**
     * Prepends {@code window.__nv.send} so payloads match desktop {@code nv_ipc} (wraps {@code
     * {e,s,d}} before {@code _NVBridge.post}), then appends {@code NativeView._emit} / {@code
     * _resolve} / {@code _reject} shims around the base64-eval of {@code nativeview.js}.
     */
    public static String wrapNativeViewJsForAndroid(String bridgeSource) {
        if (bridgeSource == null || bridgeSource.isEmpty()) {
            return "(function(){})();";
        }
        String pre =
                "(function(){try{window.__nv=window.__nv||{};window.__nv.send=function(e,json){try{if(window._NVBridge&&window._NVBridge.post){var w;try{w=JSON.stringify({e:e,s:0,d:JSON.parse(json)});}catch(x){w=JSON.stringify({e:e,s:0,d:json});}window._NVBridge.post(w);}}catch(x){}};}catch(e){}})();";
        String core = createInjectJavascriptExpression(bridgeSource);
        String post =
                "(function(){try{if(typeof NativeView!=='undefined'){NativeView._emit=function(ev,j){try{if(typeof _ipc!=='undefined'&&_ipc.receive){if(typeof j==='string'){_ipc.receive('',j);}else{_ipc.receive('',JSON.stringify({e:(ev&&ev.length)?ev:'event',d:j===undefined?null:j}));}}}catch(e){}};NativeView._resolve=function(id,d){try{if(typeof _ipc!=='undefined'&&_ipc.receive)_ipc.receive('',JSON.stringify({s:id,ok:1,d:d===undefined?null:d}));}catch(e){}};NativeView._reject=function(id,err){try{var d=(err&&typeof err==='object')?err:{code:'ERR_UNKNOWN',msg:String(err||'')};if(!d.code)d.code='ERR_UNKNOWN';if(!('msg'in d))d.msg='';if(typeof _ipc!=='undefined'&&_ipc.receive)_ipc.receive('',JSON.stringify({s:id,ok:0,d:d}));}catch(e){}};}}catch(e){}})();";
        return pre + core + post;
    }

    private final class NativeViewWebViewClient extends WebViewClient {

        @Override
        public void onPageStarted(WebView view, String url, Bitmap favicon) {
            NavigationListener l = navigationListener;
            if (l != null) {
                l.onPageStarted(url, favicon);
            }
        }

        @Override
        public void onPageFinished(WebView view, String url) {
            injectBridgeThenReady();
            NavigationListener l = navigationListener;
            if (l != null) {
                l.onPageFinished(url);
            }
        }

        @Override
        public void onReceivedError(WebView view, WebResourceRequest request, WebResourceError error) {
            if (request == null || request.isForMainFrame()) {
                logMainFrameError(request, error);
            }
            PageErrorListener l = pageErrorListener;
            if (l != null) {
                l.onReceivedError(view, request, error);
            }
            super.onReceivedError(view, request, error);
        }

        @Override
        public void onReceivedHttpError(
                WebView view, WebResourceRequest request, WebResourceResponse errorResponse) {
            PageErrorListener l = pageErrorListener;
            if (l != null) {
                l.onReceivedHttpError(view, request, errorResponse);
            }
            super.onReceivedHttpError(view, request, errorResponse);
        }
    }

    private static void logMainFrameError(WebResourceRequest request, WebResourceError error) {
        String url = request != null && request.getUrl() != null ? request.getUrl().toString() : "";
        String desc = error != null ? error.getDescription().toString() : "";
        int code = error != null ? error.getErrorCode() : -1;
        Log.w(TAG, "WebView error code=" + code + " url=" + url + " desc=" + desc);
    }

    private final class NativeViewWebChromeClient extends WebChromeClient {

        @Override
        public boolean onConsoleMessage(ConsoleMessage consoleMessage) {
            String msg =
                    consoleMessage != null && consoleMessage.message() != null
                            ? consoleMessage.message()
                            : "";
            String src =
                    consoleMessage != null && consoleMessage.sourceId() != null
                            ? consoleMessage.sourceId()
                            : "";
            int line = consoleMessage != null ? consoleMessage.lineNumber() : 0;
            Log.d(TAG, "[JS] " + msg + " — " + src + ":" + line);
            return super.onConsoleMessage(consoleMessage);
        }

        @Override
        public boolean onShowFileChooser(
                WebView webView,
                ValueCallback<Uri[]> filePathCallback,
                FileChooserParams fileChooserParams) {
            FileChooserHandler h = fileChooserHandler;
            if (h != null) {
                return h.onShowFileChooser(webView, filePathCallback, fileChooserParams);
            }
            return false;
        }

        @Override
        public void onPermissionRequest(PermissionRequest request) {
            WebPermissionHandler h = webPermissionHandler;
            if (h != null) {
                h.onPermissionRequest(request);
                return;
            }
            if (request != null) {
                request.deny();
            }
        }
    }

    /** Optional navigation hooks (main frame). */
    public interface NavigationListener {

        void onPageStarted(String url, Bitmap favicon);

        void onPageFinished(String url);
    }

    /** Optional error reporting for the embedding activity. */
    public interface PageErrorListener {

        void onReceivedError(WebView view, WebResourceRequest request, WebResourceError error);

        void onReceivedHttpError(
                WebView view, WebResourceRequest request, WebResourceResponse errorResponse);
    }

    /** Host handles {@code <input type="file">} and must forward activity results to the callback. */
    public interface FileChooserHandler {

        boolean onShowFileChooser(
                WebView webView,
                ValueCallback<Uri[]> filePathCallback,
                WebChromeClient.FileChooserParams fileChooserParams);
    }

    /** Host grants or denies WebView media / device permissions. */
    public interface WebPermissionHandler {

        void onPermissionRequest(PermissionRequest request);
    }
}
