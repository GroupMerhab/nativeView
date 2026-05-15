package com.nativeview;

import android.app.Activity;
import android.content.Intent;
import android.content.res.Configuration;
import android.net.Uri;
import android.os.Bundle;
import android.view.ViewGroup;
import com.nativeview.bridge.BridgeJavaScript;
import com.nativeview.jni.NativeViewHost;
import com.nativeview.jni.NativeViewJNI;
import org.json.JSONException;
import org.json.JSONObject;

/**
 * Single-window host activity: {@link NativeViewApp} + {@link NativeViewWindow} + {@link
 * NativeViewWebView}, {@link NativeViewHost} dispatch, lifecycle, back, deep links, and orientation
 * notifications. Registers itself with {@link NativeViewAndroid#attachHostActivity(Activity)} in
 * {@code onResume} so Android-only helpers ({@link NativeViewAndroid#setStatusBarColor(String)},
 * {@link NativeViewAndroid#setFullScreen(boolean)}, …) apply to this activity.
 *
 * <p>Subclass {@link #getContentUrl()} for a document URL (or return {@code null} / empty and
 * override {@link #getContentHtml()}). Override {@link #onNativeViewAppCreated(NativeViewApp)} to
 * call {@link NativeViewApp#allow_origin} for each non-file origin (for example {@code https://})
 * that loads your UI; the bridge defaults to {@code file://} only until you add origins.
 *
 * <p><b>Manifest (consumer app):</b> for deep links add an {@code intent-filter} with {@code
 * android.intent.action.VIEW}, {@code DEFAULT}, {@code BROWSABLE}, and {@code data} schemes; use
 * {@code android:launchMode="singleTask"} or {@code singleTop} on this activity so {@link
 * #onNewIntent(Intent)} receives new URIs. For rotation without an activity recreate, add {@code
 * android:configChanges="orientation|screenSize|screenLayout|keyboardHidden"}; otherwise {@link
 * android.webkit.WebView#saveState} / {@link android.webkit.WebView#restoreState} preserve
 * navigation across recreate.
 *
 * <p>Push events use the same wire shape as the desktop bridge ({@code e}, {@code d}); subscribe
 * from JavaScript with {@code NativeView.on('app.resume', fn)} (and similarly for {@code
 * app.pause}, {@code app.destroy}, {@code app.deeplink}, {@code device.orientation}).
 */
public abstract class NativeViewActivity extends Activity implements NativeViewHost {

    private static final String KEY_LAST_ORIENTATION = "nv.last_orientation";

    private NativeViewApp nvApp;
    private NativeViewWindow nvWindow;
    private NativeViewWebView nvWebView;
    private boolean tornDown;
    private boolean bridgeReady;
    private int lastEmittedOrientation = Configuration.ORIENTATION_UNDEFINED;
    private Uri pendingDeepLink;

    protected NativeViewApp getNativeViewApp() {
        return nvApp;
    }

    protected NativeViewWindow getNativeViewWindow() {
        return nvWindow;
    }

    protected NativeViewWebView getNativeViewWebView() {
        return nvWebView;
    }

    /** Primary document URL, or {@code null} / empty to use {@link #getContentHtml()} instead. */
    protected abstract String getContentUrl();

    /** Used when {@link #getContentUrl()} is null or empty. */
    protected String getContentHtml() {
        return "";
    }

    /** Base URL for {@link #getContentHtml()}. */
    protected String getContentHtmlBaseUrl() {
        return "about:blank";
    }

    protected NativeViewConfig createWindowConfig() {
        return new NativeViewConfig();
    }

    /** Called after {@link NativeViewApp} is constructed; override to call {@link NativeViewApp#allow_origin}. */
    protected void onNativeViewAppCreated(NativeViewApp app) {}

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        nvWebView = new NativeViewWebView(this);
        setContentView(nvWebView);

        nvApp = new NativeViewApp(this);
        nvApp.setPermissionActivity(this);
        nvApp.registerDefaultAndroidHandlers();
        onNativeViewAppCreated(nvApp);

        nvWindow = nvApp.create_window(createWindowConfig());
        if (nvWindow == null || nvWindow.getNativeHandle() == 0L) {
            nvApp.destroy();
            nvApp = null;
            if (nvWebView.getParent() instanceof ViewGroup) {
                ((ViewGroup) nvWebView.getParent()).removeView(nvWebView);
            }
            nvWebView.destroy();
            nvWebView = null;
            finish();
            return;
        }

        nvWebView.attachNativeBridge(nvApp, nvWindow);
        nvWebView.setOnClose(() -> runOnUiThread(this::finish));
        nvWebView.setOnReady(this::onWebViewBridgeReady);
        nvWindow.on_close(() -> runOnUiThread(this::finish));

        String url = getContentUrl();
        if (url != null && !url.trim().isEmpty()) {
            nvApp.allow_origin(url);
            nvWindow.load_url(url.trim());
        } else {
            String html = getContentHtml();
            nvWindow.load_html(html != null ? html : "", getContentHtmlBaseUrl());
        }
        nvWindow.show();

        if (savedInstanceState != null) {
            nvWebView.restoreState(savedInstanceState);
            lastEmittedOrientation =
                    savedInstanceState.getInt(KEY_LAST_ORIENTATION, Configuration.ORIENTATION_UNDEFINED);
        }

        noteDeepLink(getIntent());
    }

    private void onWebViewBridgeReady() {
        bridgeReady = true;
        flushPendingDeepLink();
    }

    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        setIntent(intent);
        noteDeepLink(intent);
        if (bridgeReady) {
            flushPendingDeepLink();
        }
    }

    private void noteDeepLink(Intent intent) {
        if (intent == null) {
            return;
        }
        Uri data = intent.getData();
        if (data != null) {
            pendingDeepLink = data;
        }
    }

    private void flushPendingDeepLink() {
        if (pendingDeepLink == null || nvWebView == null) {
            return;
        }
        String urlStr = pendingDeepLink.toString();
        pendingDeepLink = null;
        try {
            JSONObject d = new JSONObject();
            d.put("url", urlStr);
            BridgeJavaScript.emitEvent(nvWebView, "app.deeplink", d.toString());
        } catch (JSONException ignored) {
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        NativeViewAndroid.attachHostActivity(this);
        if (nvWebView != null) {
            nvWebView.onResume();
        }
        emitAppEvent("app.resume", "{}");
        maybeEmitOrientationChanged();
    }

    @Override
    protected void onPause() {
        emitAppEvent("app.pause", "{}");
        if (nvWebView != null) {
            nvWebView.onPause();
        }
        super.onPause();
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        maybeEmitOrientationChanged();
    }

    @Override
    protected void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        if (nvWebView != null) {
            nvWebView.saveState(outState);
        }
        outState.putInt(KEY_LAST_ORIENTATION, lastEmittedOrientation);
    }

    @SuppressWarnings("deprecation")
    @Override
    public void onBackPressed() {
        if (nvWebView != null && nvWebView.handleBackPressed()) {
            return;
        }
        super.onBackPressed();
    }

    @Override
    protected void onDestroy() {
        NativeViewAndroid.detachHostActivity(this);
        if (!tornDown) {
            tornDown = true;
            emitAppEvent("app.destroy", "{}");
            if (nvWebView != null) {
                nvWebView.setOnClose(null);
                nvWebView.setOnReady(null);
                bridgeReady = false;
                if (nvWebView.getParent() instanceof ViewGroup) {
                    ((ViewGroup) nvWebView.getParent()).removeView(nvWebView);
                }
                nvWebView.destroy();
                nvWebView = null;
            }
            nvWindow = null;
            if (nvApp != null) {
                nvApp.destroy();
                nvApp = null;
            }
        }
        super.onDestroy();
    }

    private void emitAppEvent(String event, String dJson) {
        if (nvWebView == null) {
            return;
        }
        BridgeJavaScript.emitEvent(nvWebView, event, dJson);
    }

    /**
     * Emits {@code device.orientation} when {@link Configuration#orientation} changes after the
     * initial baseline (first {@link #onResume()} records orientation without emitting). Handles
     * both {@link #onConfigurationChanged(Configuration)} ({@code android:configChanges}) and
     * process death / activity recreate.
     */
    private void maybeEmitOrientationChanged() {
        if (nvWebView == null) {
            return;
        }
        int o = getResources().getConfiguration().orientation;
        if (o == Configuration.ORIENTATION_UNDEFINED) {
            return;
        }
        if (lastEmittedOrientation == Configuration.ORIENTATION_UNDEFINED) {
            lastEmittedOrientation = o;
            return;
        }
        if (o == lastEmittedOrientation) {
            return;
        }
        lastEmittedOrientation = o;
        try {
            JSONObject d = new JSONObject();
            d.put("landscape", o == Configuration.ORIENTATION_LANDSCAPE);
            BridgeJavaScript.emitEvent(nvWebView, "device.orientation", d.toString());
        } catch (JSONException ignored) {
        }
    }

    @Override
    public void dispatch(int op, long windowPtr, String s1, String s2) {
        if (nvWindow == null || windowPtr != nvWindow.getNativeHandle() || nvWebView == null) {
            return;
        }
        runOnUiThread(
                () -> {
                    if (tornDown || nvWebView == null) {
                        return;
                    }
                    switch (op) {
                        case NativeViewJNI.OP_WINDOW_CREATE:
                        case NativeViewJNI.OP_WINDOW_DESTROY:
                        case NativeViewJNI.OP_WINDOW_SHOW:
                        case NativeViewJNI.OP_WINDOW_HIDE:
                            break;
                        case NativeViewJNI.OP_WINDOW_LOAD_URL:
                            if (s1 != null) {
                                nvWebView.loadUrl(s1);
                            }
                            break;
                        case NativeViewJNI.OP_WINDOW_LOAD_HTML:
                            nvWebView.loadDataWithBaseURL(
                                    s2 != null ? s2 : "about:blank",
                                    s1 != null ? s1 : "",
                                    "text/html",
                                    "utf-8",
                                    null);
                            break;
                        case NativeViewJNI.OP_WINDOW_EVAL_JS:
                            nvWebView.evaluateJavascript(s1 != null ? s1 : "", null);
                            break;
                        default:
                            break;
                    }
                });
    }
}
