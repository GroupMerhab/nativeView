package io.jamharah.nativeview.example;

import android.content.res.Configuration;
import android.content.Intent;
import android.os.Bundle;
import android.view.ViewGroup;
import androidx.activity.OnBackPressedCallback;
import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import com.nativeview.NativeViewAndroid;
import com.nativeview.NativeViewApp;
import com.nativeview.NativeViewConfig;
import com.nativeview.NativeViewWebView;
import com.nativeview.NativeViewWindow;
import com.nativeview.bridge.BridgeJavaScript;
import com.nativeview.jni.NativeViewHost;
import com.nativeview.jni.NativeViewJNI;
import org.json.JSONException;
import org.json.JSONObject;

/**
 * Single-window host using {@link AppCompatActivity} so {@code biometric.*} (AndroidX {@code
 * BiometricPrompt}) can run. Mirrors {@link com.nativeview.NativeViewActivity} wiring: JNI app,
 * default Java ops, {@link NativeViewWebView}, lifecycle events to JS, and permission / activity
 * result forwarding.
 */
public final class MainActivity extends AppCompatActivity implements NativeViewHost {

    private static final String KEY_LAST_ORIENTATION = "nv.last_orientation";

    private NativeViewApp nvApp;
    private NativeViewWindow nvWindow;
    private NativeViewWebView nvWebView;
    private boolean tornDown;
    private boolean bridgeReady;
    private int lastEmittedOrientation = Configuration.ORIENTATION_UNDEFINED;

    private final OnBackPressedCallback backCallback =
            new OnBackPressedCallback(true) {
                @Override
                public void handleOnBackPressed() {
                    if (nvWebView != null && nvWebView.handleBackPressed()) {
                        return;
                    }
                    setEnabled(false);
                    MainActivity.this.getOnBackPressedDispatcher().onBackPressed();
                }
            };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getOnBackPressedDispatcher().addCallback(this, backCallback);

        nvWebView = new NativeViewWebView(this);
        setContentView(nvWebView);

        nvApp = new NativeViewApp(this);
        nvApp.setPermissionActivity(this);
        nvApp.registerDefaultAndroidHandlers();

        nvWindow = nvApp.create_window(new NativeViewConfig());
        if (nvWindow == null || nvWindow.getNativeHandle() == 0L) {
            nvApp.destroy();
            nvApp = null;
            removeWebViewFromParent();
            nvWebView.destroy();
            nvWebView = null;
            finish();
            return;
        }

        nvWebView.attachNativeBridge(nvApp, nvWindow);
        nvWebView.setOnClose(() -> runOnUiThread(this::finish));
        nvWebView.setOnReady(
                () -> {
                    bridgeReady = true;
                    maybeEmitOrientationChanged();
                });
        nvWindow.on_close(() -> runOnUiThread(this::finish));

        nvWindow.load_url("file:///android_asset/index.html");
        nvWindow.show();

        if (savedInstanceState != null) {
            nvWebView.restoreState(savedInstanceState);
            lastEmittedOrientation =
                    savedInstanceState.getInt(KEY_LAST_ORIENTATION, Configuration.ORIENTATION_UNDEFINED);
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
    public void onConfigurationChanged(@NonNull Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        maybeEmitOrientationChanged();
    }

    @Override
    protected void onSaveInstanceState(@NonNull Bundle outState) {
        super.onSaveInstanceState(outState);
        if (nvWebView != null) {
            nvWebView.saveState(outState);
        }
        outState.putInt(KEY_LAST_ORIENTATION, lastEmittedOrientation);
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
                removeWebViewFromParent();
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

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (nvApp != null && nvApp.onActivityResult(requestCode, resultCode, data)) {
            return;
        }
        super.onActivityResult(requestCode, resultCode, data);
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
        if (nvApp != null && nvApp.onRequestPermissionsResult(requestCode, permissions, grantResults)) {
            return;
        }
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
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

    private void removeWebViewFromParent() {
        if (nvWebView != null && nvWebView.getParent() instanceof ViewGroup) {
            ((ViewGroup) nvWebView.getParent()).removeView(nvWebView);
        }
    }

    private void emitAppEvent(String event, String dJson) {
        if (nvWebView == null) {
            return;
        }
        BridgeJavaScript.emitEvent(nvWebView, event, dJson);
    }

    private void maybeEmitOrientationChanged() {
        if (nvWebView == null || !bridgeReady) {
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
}
