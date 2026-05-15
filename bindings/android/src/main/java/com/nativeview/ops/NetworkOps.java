package com.nativeview.ops;

import android.Manifest;
import android.content.Context;
import android.net.ConnectivityManager;
import android.net.Network;
import android.net.NetworkCapabilities;
import com.nativeview.NativeViewApp;
import com.nativeview.NativeViewWindow;
import com.nativeview.bridge.BridgeDispatchContext;
import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.CopyOnWriteArraySet;
import org.json.JSONException;
import org.json.JSONObject;

/** Network connectivity status and change events. */
public final class NetworkOps {

    private static final Object NET_LOCK = new Object();
    private static ConnectivityManager.NetworkCallback registeredCallback;
    private static ConnectivityManager registeredCm;

    /** App → subscriber windows (weak). */
    private static final ConcurrentHashMap<NativeViewApp, CopyOnWriteArraySet<WeakReference<NativeViewWindow>>> subscribers =
            new ConcurrentHashMap<>();

    private NetworkOps() {}

    public static void register(NativeViewApp app) {
        if (app == null) {
            return;
        }
        String[] netState = new String[] {Manifest.permission.ACCESS_NETWORK_STATE};
        Context ctx = app.getAppContext();
        app.handle(
                "network.getStatus",
                netState,
                (argsJson, resolve, reject) -> {
                    try {
                        resolve.resolve(buildStatus(ctx).toString());
                    } catch (JSONException ex) {
                        reject.reject("ERR_JSON", ex.getMessage() != null ? ex.getMessage() : "json error");
                    }
                });
        app.handle(
                "network.onChange",
                netState,
                (argsJson, resolve, reject) -> {
                    NativeViewWindow w = BridgeDispatchContext.window();
                    if (w == null || w.getNativeHandle() == 0L) {
                        reject.reject("ERR_INTERNAL", "no window context");
                        return;
                    }
                    subscribers
                            .computeIfAbsent(app, k -> new CopyOnWriteArraySet<>())
                            .add(new WeakReference<>(w));
                    ensureNetworkCallback(ctx);
                    try {
                        JSONObject o = new JSONObject();
                        o.put("subscribed", true);
                        resolve.resolve(o.toString());
                    } catch (JSONException ex) {
                        reject.reject("ERR_JSON", ex.getMessage() != null ? ex.getMessage() : "json error");
                    }
                });
    }

    public static void unregister(NativeViewApp app) {
        if (app == null) {
            return;
        }
        subscribers.remove(app);
        maybeUnregisterNetworkCallback();
    }

    private static void ensureNetworkCallback(Context ctx) {
        synchronized (NET_LOCK) {
            if (registeredCallback != null) {
                return;
            }
            ConnectivityManager cm =
                    (ConnectivityManager) ctx.getApplicationContext().getSystemService(Context.CONNECTIVITY_SERVICE);
            if (cm == null) {
                return;
            }
            registeredCm = cm;
            final Context appCtx = ctx.getApplicationContext();
            registeredCallback =
                    new ConnectivityManager.NetworkCallback() {
                        @Override
                        public void onAvailable(Network network) {
                            emitStatus(appCtx);
                        }

                        @Override
                        public void onLost(Network network) {
                            emitStatus(appCtx);
                        }

                        @Override
                        public void onCapabilitiesChanged(Network network, NetworkCapabilities networkCapabilities) {
                            emitStatus(appCtx);
                        }
                    };
            try {
                cm.registerDefaultNetworkCallback(registeredCallback);
            } catch (Exception ignored) {
                registeredCallback = null;
                registeredCm = null;
            }
        }
    }

    private static void maybeUnregisterNetworkCallback() {
        synchronized (NET_LOCK) {
            if (hasAnySubscriber()) {
                return;
            }
            if (registeredCm != null && registeredCallback != null) {
                try {
                    registeredCm.unregisterNetworkCallback(registeredCallback);
                } catch (Exception ignored) {
                }
            }
            registeredCm = null;
            registeredCallback = null;
        }
    }

    private static boolean hasAnySubscriber() {
        for (CopyOnWriteArraySet<WeakReference<NativeViewWindow>> set : subscribers.values()) {
            for (WeakReference<NativeViewWindow> ref : set) {
                NativeViewWindow w = ref != null ? ref.get() : null;
                if (w != null && w.getNativeHandle() != 0L) {
                    return true;
                }
            }
        }
        return false;
    }

    private static void emitStatus(Context appCtx) {
        JSONObject status;
        try {
            status = buildStatus(appCtx);
        } catch (JSONException ex) {
            return;
        }
        String payload = status.toString();
        for (CopyOnWriteArraySet<WeakReference<NativeViewWindow>> set : subscribers.values()) {
            for (WeakReference<NativeViewWindow> ref : new ArrayList<>(set)) {
                NativeViewWindow w = ref != null ? ref.get() : null;
                if (w == null || w.getNativeHandle() == 0L) {
                    set.remove(ref);
                    continue;
                }
                w.emit("network.change", payload);
            }
        }
    }

    static JSONObject buildStatus(Context ctx) throws JSONException {
        JSONObject o = new JSONObject();
        if (ctx == null) {
            o.put("connected", false);
            o.put("type", "none");
            return o;
        }
        ConnectivityManager cm =
                (ConnectivityManager) ctx.getSystemService(Context.CONNECTIVITY_SERVICE);
        if (cm == null) {
            o.put("connected", false);
            o.put("type", "none");
            return o;
        }
        Network active = cm.getActiveNetwork();
        NetworkCapabilities caps = active != null ? cm.getNetworkCapabilities(active) : null;
        if (caps == null) {
            o.put("connected", false);
            o.put("type", "none");
            return o;
        }
        String type = "other";
        if (caps.hasTransport(NetworkCapabilities.TRANSPORT_WIFI)
                || caps.hasTransport(NetworkCapabilities.TRANSPORT_WIFI_AWARE)) {
            type = "wifi";
        } else if (caps.hasTransport(NetworkCapabilities.TRANSPORT_CELLULAR)) {
            type = "cellular";
        } else if (!caps.hasTransport(NetworkCapabilities.TRANSPORT_ETHERNET)
                && !caps.hasTransport(NetworkCapabilities.TRANSPORT_BLUETOOTH)
                && !caps.hasTransport(NetworkCapabilities.TRANSPORT_USB)) {
            type = "none";
        }
        boolean connected = caps.hasCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET);
        o.put("connected", connected);
        o.put("type", type);
        return o;
    }
}
