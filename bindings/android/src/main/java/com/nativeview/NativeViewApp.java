package com.nativeview;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.webkit.WebView;
import com.nativeview.bridge.ActivityResultRouter;
import com.nativeview.bridge.AndroidBridgeRegistry;
import com.nativeview.bridge.BridgeClientScript;
import com.nativeview.bridge.BridgeDispatchContext;
import com.nativeview.bridge.BridgeOrigin;
import com.nativeview.bridge.NVPermissionManager;
import com.nativeview.bridge.NVRouter;
import com.nativeview.jni.NativeViewHost;
import com.nativeview.jni.NativeViewJNI;
import java.lang.ref.WeakReference;
import java.util.Collections;
import java.util.LinkedHashSet;
import java.util.Set;
import org.json.JSONException;
import org.json.JSONObject;

/**
 * Android application host for nativeview: JNI init, window factory, bridge routing, and
 * lifecycle helpers. {@link #run()} is a no-op on Android (the activity lifecycle replaces the
 * desktop event loop).
 */
public class NativeViewApp {

    private final Context appContext;
    private final NVRouter router = new NVRouter();
    private final Set<String> allowedOrigins = Collections.synchronizedSet(new LinkedHashSet<>());

    private volatile WeakReference<Activity> permissionActivity = new WeakReference<>(null);

    private volatile boolean destroyed;

    /**
     * Initializes the native library and registers {@code context} as a {@link
     * com.nativeview.jni.NativeViewHost} when it implements that interface.
     */
    public NativeViewApp(Context context) {
        if (context == null) {
            throw new IllegalArgumentException("context");
        }
        this.appContext = context.getApplicationContext();
        NativeViewJNI.ensureLoaded();
        NativeViewJNI.nativeInit(context);
        if (context instanceof NativeViewHost) {
            NativeViewJNI.nativeBindDispatchHost((NativeViewHost) context);
        }
        synchronized (allowedOrigins) {
            String fileOrigin = BridgeOrigin.normalizeAllowedOrigin("file://");
            if (fileOrigin != null) {
                allowedOrigins.add(fileOrigin);
            }
        }
    }

    public NativeViewWindow create_window(NativeViewConfig config) {
        if (destroyed) {
            return null;
        }
        NativeViewConfig c = config != null ? config : new NativeViewConfig();
        long id = NativeViewJNI.nativeCreateWindow(c);
        if (id == 0L) {
            return null;
        }
        return new NativeViewWindow(id);
    }

    /** No-op on Android; the UI thread is driven by the activity lifecycle. */
    public void run() {
        // nv_app_run blocks on desktop; Android does not use a native blocking loop here.
    }

    /** Best-effort parity with desktop {@code nv_app_quit}; the native Android app hook is a
     * no-op unless a native run loop is active. */
    public void quit() {
        NativeViewJNI.nativeQuit();
    }

    public void destroy() {
        if (destroyed) {
            return;
        }
        AndroidBridgeRegistry.unregisterAll(this);
        destroyed = true;
        permissionActivity = new WeakReference<>(null);
        NativeViewJNI.nativeDestroy();
    }

    /** Application context passed to the constructor; never {@code null} after construction. */
    public Context getAppContext() {
        return appContext;
    }

    /**
     * Activity set via {@link #setPermissionActivity(Activity)} (runtime permissions, {@code
     * app.exit}, camera intents). May be {@code null}.
     */
    public Activity getHostActivity() {
        return permissionActivity != null ? permissionActivity.get() : null;
    }

    /**
     * Registers built-in Android bridge methods ({@code device.*}, {@code storage.*}, …). Safe to
     * call once per app instance; re-registering overwrites the same method keys.
     */
    public void registerDefaultAndroidHandlers() {
        AndroidBridgeRegistry.registerAll(this);
    }

    /**
     * Forwards activity results to camera / gallery bridge flows. Call from {@link
     * Activity#onActivityResult(int, int, Intent)}.
     *
     * @return {@code true} when the result was consumed by the bridge.
     */
    public boolean onActivityResult(int requestCode, int resultCode, Intent data) {
        return ActivityResultRouter.deliver(requestCode, resultCode, data);
    }

    /**
     * Adds an allowed document origin for the WebView bridge (scheme + host + non-default port).
     * Entries are normalized; duplicates are ignored. The default whitelist already includes {@code
     * file://}; call this for {@code https://} / {@code http://} hosts (and optional ports) used by
     * your pages.
     */
    public void allow_origin(String origin) {
        String n = BridgeOrigin.normalizeAllowedOrigin(origin);
        if (n == null) {
            return;
        }
        allowedOrigins.add(n);
    }

    /**
     * Snapshot of origins passed to {@link #allow_origin}; never {@code null} (may be empty).
     */
    public Set<String> allowedOriginsSnapshot() {
        synchronized (allowedOrigins) {
            return Collections.unmodifiableSet(new LinkedHashSet<>(allowedOrigins));
        }
    }

    /**
     * Activity used for {@link android.app.Activity#requestPermissions(String[], int)} when a
     * registered handler lists runtime {@link android.Manifest.permission} strings. Forward
     * {@link Activity#onRequestPermissionsResult(int, String[], int[])} to {@link
     * #onRequestPermissionsResult(int, String[], int[])} on this app instance.
     */
    public void setPermissionActivity(Activity activity) {
        permissionActivity = new WeakReference<>(activity);
        if (activity instanceof NativeViewHost) {
            NativeViewJNI.nativeBindDispatchHost((NativeViewHost) activity);
        }
    }

    /**
     * Forwards permission results to {@link NVPermissionManager}. Returns {@code true} when the
     * request was consumed by an in-flight bridge permission flow.
     */
    public boolean onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        return NVPermissionManager.deliverPermissionResult(requestCode, permissions, grantResults);
    }

    /** Wire method registry used by {@link #handle(String, String[], NVHandler)}. */
    public NVRouter getRouter() {
        return router;
    }

    /**
     * Registers a Java-side handler for bridge method {@code method}. The host must call {@link
     * #deliverWebMessage(WebView, NativeViewWindow, String)} (or the JNI overload) with the raw
     * wire JSON so this handler can run before native IPC dispatch.
     */
    public void handle(String method, String[] permissions, NVHandler handler) {
        router.register(method, permissions, handler);
    }

    /**
     * Entry point when the message is not tied to a WebView origin check (for example native
     * forwarding): runs Java handlers first, then {@code nv_ipc_dispatch}.
     */
    public void deliverWebMessage(NativeViewWindow window, String wireJson) {
        deliverWebMessage(null, window, wireJson);
    }

    /**
     * WebView entry: optional origin filter, Java handlers with async permission checks, then
     * native IPC dispatch ({@link NativeViewJNI#nativeDispatchWebMessage}) for methods not handled in
     * Java (for example {@code app.handshake} and other C-side ops).
     */
    public void deliverWebMessage(WebView webView, NativeViewWindow window, String wireJson) {
        if (destroyed || window == null || wireJson == null || window.getNativeHandle() == 0L) {
            return;
        }
        long seqHint = WireJson.extractSeq(wireJson);
        JSONObject wire;
        try {
            wire = WireJson.parseWireObject(wireJson);
        } catch (JSONException ex) {
            rejectWire(
                    window,
                    seqHint,
                    "ERR_INVALID_JSON",
                    ex.getMessage() != null ? ex.getMessage() : "parse error");
            return;
        }
        long seq = wire.optLong("s", seqHint);
        if (webView != null
                && !BridgeOrigin.isAllowed(webView.getUrl(), allowedOriginsSnapshot())) {
            rejectWire(window, seq, "PERMISSION_DENIED", "Origin not allowed");
            return;
        }
        if (tryDispatchJavaHandler(window, wire)) {
            return;
        }
        NativeViewJNI.nativeDispatchWebMessage(window.getNativeHandle(), wireJson);
    }

    private boolean tryDispatchJavaHandler(NativeViewWindow window, JSONObject wire) {
        String method = wire.optString("e", "");
        if (method.isEmpty()) {
            return false;
        }
        NVRouter.RouteResult route = router.route(method);
        if (!route.isAvailable()) {
            return false;
        }
        dispatchUserHandler(window, wire, route);
        return true;
    }

    private void dispatchUserHandler(NativeViewWindow window, JSONObject wire, NVRouter.RouteResult route) {
        Activity act = permissionActivity != null ? permissionActivity.get() : null;
        long seq = wire.optLong("s", 0L);
        NVPermissionManager.checkThenRun(
                appContext,
                act,
                route.getPermissions(),
                () -> invokeUserHandler(window, wire, route.getHandler()),
                (code, message) -> rejectWire(window, seq, code, message));
    }

    private void invokeUserHandler(NativeViewWindow window, JSONObject wire, NVHandler user) {
        long seq = wire.optLong("s", 0L);
        String argsJson;
        try {
            argsJson = WireJson.extractArgsJson(wire);
        } catch (JSONException ex) {
            rejectWire(window, seq, "ERR_INVALID_JSON", ex.getMessage() != null ? ex.getMessage() : "parse error");
            return;
        }

        ResolveCallback resolve =
                json -> {
                    if (seq <= 0L) {
                        return;
                    }
                    replyOk(window, seq, json);
                };
        RejectCallback reject =
                (code, message) -> {
                    if (seq <= 0L) {
                        return;
                    }
                    rejectWire(window, seq, code, message);
                };

        BridgeDispatchContext.set(this, window);
        try {
            user.handle(argsJson, resolve, reject);
        } catch (RuntimeException ex) {
            rejectWire(
                    window,
                    seq,
                    "ERR_HANDLER",
                    ex.getMessage() != null ? ex.getMessage() : "handler threw");
        } finally {
            BridgeDispatchContext.clear();
        }
    }

    private static void replyOk(NativeViewWindow window, long seq, String json) {
        if (window == null || seq <= 0L) {
            return;
        }
        try {
            String js = BridgeClientScript.buildResolveInvoke(seq, json);
            if (js == null) {
                return;
            }
            window.eval_js(js);
        } catch (Exception ex) {
            rejectWire(window, seq, "ERR_JSON", ex.getMessage() != null ? ex.getMessage() : "invalid resolve json");
        }
    }

    private static void rejectWire(NativeViewWindow window, long seq, String code, String message) {
        if (window == null) {
            return;
        }
        if (seq <= 0L) {
            return;
        }
        String js = BridgeClientScript.buildRejectInvoke(seq, code, message);
        if (js != null) {
            window.eval_js(js);
        }
    }
}
