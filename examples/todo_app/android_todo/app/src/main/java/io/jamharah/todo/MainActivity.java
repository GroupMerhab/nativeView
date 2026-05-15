/* SPDX-License-Identifier: Apache-2.0 */
package io.jamharah.todo;

import android.content.res.Configuration;
import android.content.Intent;
import android.os.Bundle;
import android.view.ViewGroup;
import android.widget.Toast;
import androidx.activity.OnBackPressedCallback;
import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import com.google.gson.Gson;
import com.google.gson.JsonObject;
import com.google.gson.JsonParseException;
import com.google.gson.JsonParser;
import com.nativeview.NativeViewAndroid;
import com.nativeview.NativeViewApp;
import com.nativeview.NativeViewConfig;
import com.nativeview.NativeViewWebView;
import com.nativeview.NativeViewWindow;
import com.nativeview.bridge.BridgeDispatchContext;
import com.nativeview.bridge.BridgeJavaScript;
import com.nativeview.jni.NativeViewHost;
import com.nativeview.jni.NativeViewJNI;
import org.json.JSONException;
import org.json.JSONObject;

/**
 * Vue todo UI + SQLite + {@code com.nativeview} Android binding (parity with {@code nim_todo} /
 * {@code java_todo}).
 */
public final class MainActivity extends AppCompatActivity implements NativeViewHost {

  private static final Gson WIRE_GSON = new Gson();
  private static final String KEY_LAST_ORIENTATION = "nv.last_orientation";

  private NativeViewApp nvApp;
  private NativeViewWindow nvWindow;
  private NativeViewWebView nvWebView;
  private AndroidTodoStore todoStore;
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

    todoStore = AndroidTodoStore.storeOpen(this);
    if (todoStore == null) {
      finish();
      return;
    }

    try {
      nvApp = new NativeViewApp(this);
    } catch (IllegalStateException e) {
      android.util.Log.e("TodoApp", "Native library load failed", e);
      Toast.makeText(
              this,
              "Missing libnativeview.so. Build native libs for arm64-v8a and x86_64, then copy into bindings/android/src/main/jniLibs/. See android_todo/README.md.",
              Toast.LENGTH_LONG)
          .show();
      AndroidTodoStore.storeClose(todoStore);
      todoStore = null;
      finish();
      return;
    }

    nvApp.setPermissionActivity(this);

    nvWebView = new NativeViewWebView(this);
    setContentView(nvWebView);

    nvApp.handle(
        "todo",
        new String[] {},
        (argsJson, resolve, reject) -> {
          NativeViewWindow win = BridgeDispatchContext.window();
          if (win == null || todoStore == null || argsJson == null) {
            return;
          }
          JsonObject root = new JsonObject();
          try {
            root.add("d", JsonParser.parseString(argsJson));
          } catch (JsonParseException e) {
            return;
          }
          TodoBridge.bridgeHandleWire(todoStore, WIRE_GSON.toJson(root), (ev, j) -> win.emit(ev, j));
        });

    nvApp.registerDefaultAndroidHandlers();

    nvWindow = nvApp.create_window(new NativeViewConfig());
    if (nvWindow == null || nvWindow.getNativeHandle() == 0L) {
      tearDownStoreAndApp();
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

    nvWindow.load_url("file:///android_asset/todo_ui.html");
    nvWindow.show();

    if (savedInstanceState != null) {
      nvWebView.restoreState(savedInstanceState);
      lastEmittedOrientation =
          savedInstanceState.getInt(KEY_LAST_ORIENTATION, Configuration.ORIENTATION_UNDEFINED);
    }
  }

  private void tearDownStoreAndApp() {
    AndroidTodoStore.storeClose(todoStore);
    todoStore = null;
    if (nvApp != null) {
      nvApp.destroy();
      nvApp = null;
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
      tearDownStoreAndApp();
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
