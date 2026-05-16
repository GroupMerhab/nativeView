// SPDX-License-Identifier: Apache-2.0

package com.nativeview.example;

import android.content.Intent;
import android.content.res.Configuration;
import android.net.Uri;
import android.os.Bundle;
import android.util.Log;
import android.view.Menu;
import android.view.MenuItem;
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
 * In-tree sample for {@code bindings/android}: mirrors {@link com.nativeview.NativeViewActivity}
 * wiring with {@link AppCompatActivity} (BiometricPrompt), {@code example.echo}, toolbar reload
 * demos, and deep links ({@code nativeview-example://}).
 */
public final class MainActivity extends AppCompatActivity implements NativeViewHost {

    private static final String TAG = "NativeViewExample";

    private static final int MENU_LOAD_BUNDLED = 1;
    private static final int MENU_LOAD_INLINE = 2;

    private static final String KEY_LAST_ORIENTATION = "nv.last_orientation";

    private static final String INLINE_HTML =
            """
            <!DOCTYPE html>
            <html lang="en">
            <head>
              <meta charset="utf-8">
              <meta name="viewport" content="width=device-width, initial-scale=1">
              <title>Inline loadHtml</title>
              <style>
                body { font-family: system-ui; margin: 16px; background: #1c1c1e; color: #f2f2f7; }
                code { background: #2c2c2e; padding: 2px 6px; border-radius: 4px; }
                pre { background: #000; padding: 12px; border-radius: 8px; overflow: auto; }
              </style>
            </head>
            <body>
              <p>This screen was loaded with <strong>NativeViewWindow.load_html</strong> using a
              <code>file:///android_asset/</code> base URL so the bridge origin matches bundled assets.</p>
              <p><button type="button" id="echoBtn">Invoke Java handler <code>example.echo</code></button></p>
              <pre id="out"></pre>
              <script>
                function log(msg) {
                  var el = document.getElementById('out');
                  el.textContent += msg + '\\n';
                }
                document.getElementById('echoBtn').onclick = function() {
                  NativeView.invoke('example.echo', { message: 'Hello from inline HTML' })
                    .then(function(r) { log('OK: ' + JSON.stringify(r)); })
                    .catch(function(e) { log('ERR: ' + (e && e.message ? e.message : String(e))); });
                };
              </script>
            </body>
            </html>
            """;

    private NativeViewApp nvApp;
    private NativeViewWindow nvWindow;
    private NativeViewWebView nvWebView;
    private boolean tornDown;
    private boolean bridgeReady;
    private int lastEmittedOrientation = Configuration.ORIENTATION_UNDEFINED;
    private Uri pendingDeepLink;

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
        registerExampleEcho(nvApp);

        NativeViewConfig cfg = new NativeViewConfig();
        cfg.title = "Example";
        cfg.width = 390;
        cfg.height = 844;
        nvWindow = nvApp.create_window(cfg);
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
                    Log.d(TAG, "[Example] WebView ready");
                    maybeEmitOrientationChanged();
                    flushPendingDeepLink();
                });
        nvWindow.on_close(() -> runOnUiThread(this::finish));
        nvWindow.on_message(
                (event, json) ->
                        Log.d(TAG, "[Example] JS → native push (seq 0): " + event + " " + json));

        nvWindow.load_url("file:///android_asset/index.html");
        nvWindow.show();

        if (savedInstanceState != null) {
            nvWebView.restoreState(savedInstanceState);
            lastEmittedOrientation =
                    savedInstanceState.getInt(KEY_LAST_ORIENTATION, Configuration.ORIENTATION_UNDEFINED);
        }

        noteDeepLink(getIntent());
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        menu.add(0, MENU_LOAD_BUNDLED, 0, "Bundled index.html");
        menu.add(0, MENU_LOAD_INLINE, 0, "Inline HTML");
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(@NonNull MenuItem item) {
        int id = item.getItemId();
        if (id == MENU_LOAD_BUNDLED) {
            if (nvWindow != null) {
                nvWindow.load_url("file:///android_asset/index.html");
            }
            return true;
        }
        if (id == MENU_LOAD_INLINE) {
            if (nvWindow != null) {
                nvWindow.load_html(INLINE_HTML, "file:///android_asset/");
            }
            return true;
        }
        return super.onOptionsItemSelected(item);
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
            Log.d(TAG, "[Example] Deep link URI: " + data);
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

    private static void registerExampleEcho(NativeViewApp app) {
        app.handle(
                "example.echo",
                null,
                (argsJson, resolve, reject) -> {
                    try {
                        JSONObject obj = new JSONObject(argsJson != null ? argsJson : "{}");
                        String msg = obj.optString("message", "");
                        JSONObject reply = new JSONObject();
                        reply.put("echo", msg);
                        reply.put("from", "Java");
                        resolve.resolve(reply.toString());
                    } catch (JSONException ex) {
                        reject.reject("ERR_INVALID_JSON", ex.getMessage() != null ? ex.getMessage() : "bad args");
                    }
                });
    }
}
