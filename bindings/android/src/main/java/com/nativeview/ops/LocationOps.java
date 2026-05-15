package com.nativeview.ops;

import android.Manifest;
import android.content.Context;
import android.location.Criteria;
import android.location.Location;
import android.location.LocationListener;
import android.location.LocationManager;
import android.os.Bundle;
import android.os.Looper;
import com.nativeview.NativeViewApp;
import com.nativeview.NativeViewWindow;
import com.nativeview.bridge.BridgeArgs;
import com.nativeview.bridge.BridgeDispatchContext;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.atomic.AtomicInteger;
import org.json.JSONException;
import org.json.JSONObject;

/** Framework {@link LocationManager} access (no Play Services). */
public final class LocationOps {

    private static final AtomicInteger NEXT_WATCH = new AtomicInteger(1);

    private static final ConcurrentHashMap<NativeViewApp, ConcurrentHashMap<Integer, Watch>> watchesByApp =
            new ConcurrentHashMap<>();

    private LocationOps() {}

    public static void register(NativeViewApp app) {
        if (app == null) {
            return;
        }
        String[] loc = new String[] {Manifest.permission.ACCESS_FINE_LOCATION};
        Context ctx = app.getAppContext();
        app.handle(
                "location.get",
                loc,
                (argsJson, resolve, reject) -> {
                    try {
                        Location l = readLastBest(ctx);
                        if (l == null) {
                            reject.reject("ERR_IO", "location unavailable");
                            return;
                        }
                        resolve.resolve(locationJson(l).toString());
                    } catch (JSONException ex) {
                        reject.reject("ERR_JSON", ex.getMessage() != null ? ex.getMessage() : "json error");
                    }
                });
        app.handle(
                "location.watch",
                loc,
                (argsJson, resolve, reject) -> {
                    NativeViewWindow win = BridgeDispatchContext.window();
                    if (win == null || win.getNativeHandle() == 0L) {
                        reject.reject("ERR_INTERNAL", "no window context");
                        return;
                    }
                    try {
                        JSONObject a = BridgeArgs.requireObject(argsJson);
                        long minTimeMs = a.optLong("minTime", 2000L);
                        float minDistance = (float) a.optDouble("minDistance", 5.0);
                        int watchId = NEXT_WATCH.getAndIncrement();
                        LocationManager lm =
                                (LocationManager) ctx.getSystemService(Context.LOCATION_SERVICE);
                        if (lm == null) {
                            reject.reject("ERR_IO", "LocationManager unavailable");
                            return;
                        }
                        String provider = bestProvider(lm);
                        if (provider == null) {
                            reject.reject("ERR_NOT_SUPPORTED", "no location provider");
                            return;
                        }
                        LocationListener listener =
                                new LocationListener() {
                                    @Override
                                    public void onLocationChanged(Location location) {
                                        if (location == null || win.getNativeHandle() == 0L) {
                                            return;
                                        }
                                        try {
                                            win.emit("location.update", locationJson(location).toString());
                                        } catch (JSONException ignored) {
                                        }
                                    }

                                    @Override
                                    public void onStatusChanged(String provider, int status, Bundle extras) {}

                                    @Override
                                    public void onProviderEnabled(String provider) {}

                                    @Override
                                    public void onProviderDisabled(String provider) {}
                                };
                        lm.requestLocationUpdates(provider, minTimeMs, minDistance, listener, Looper.getMainLooper());
                        watchesByApp.computeIfAbsent(app, k -> new ConcurrentHashMap<>()).put(watchId, new Watch(lm, listener));
                        JSONObject o = new JSONObject();
                        o.put("watchId", watchId);
                        resolve.resolve(o.toString());
                    } catch (JSONException ex) {
                        reject.reject("ERR_INVALID_JSON", ex.getMessage() != null ? ex.getMessage() : "parse error");
                    }
                });
        app.handle(
                "location.unwatch",
                null,
                (argsJson, resolve, reject) -> {
                    try {
                        JSONObject a = BridgeArgs.requireObject(argsJson);
                        int watchId = a.optInt("watchId", 0);
                        ConcurrentHashMap<Integer, Watch> forApp = watchesByApp.get(app);
                        Watch w = forApp != null ? forApp.remove(watchId) : null;
                        if (w != null) {
                            w.stop();
                            if (forApp.isEmpty()) {
                                watchesByApp.remove(app, forApp);
                            }
                        }
                        resolve.resolve("{}");
                    } catch (JSONException ex) {
                        reject.reject("ERR_INVALID_JSON", ex.getMessage() != null ? ex.getMessage() : "parse error");
                    }
                });
    }

    public static void unregister(NativeViewApp app) {
        if (app == null) {
            return;
        }
        ConcurrentHashMap<Integer, Watch> forApp = watchesByApp.remove(app);
        if (forApp == null) {
            return;
        }
        for (Watch w : forApp.values()) {
            if (w != null) {
                w.stop();
            }
        }
    }

    private static Location readLastBest(Context ctx) {
        LocationManager lm = (LocationManager) ctx.getSystemService(Context.LOCATION_SERVICE);
        if (lm == null) {
            return null;
        }
        Location net = null;
        Location gps = null;
        try {
            if (lm.isProviderEnabled(LocationManager.NETWORK_PROVIDER)) {
                net = lm.getLastKnownLocation(LocationManager.NETWORK_PROVIDER);
            }
        } catch (SecurityException ignored) {
        }
        try {
            if (lm.isProviderEnabled(LocationManager.GPS_PROVIDER)) {
                gps = lm.getLastKnownLocation(LocationManager.GPS_PROVIDER);
            }
        } catch (SecurityException ignored) {
        }
        if (gps != null && net != null) {
            return gps.getTime() >= net.getTime() ? gps : net;
        }
        return gps != null ? gps : net;
    }

    private static String bestProvider(LocationManager lm) {
        Criteria c = new Criteria();
        c.setAccuracy(Criteria.ACCURACY_FINE);
        String p = lm.getBestProvider(c, true);
        if (p != null) {
            return p;
        }
        if (lm.isProviderEnabled(LocationManager.GPS_PROVIDER)) {
            return LocationManager.GPS_PROVIDER;
        }
        if (lm.isProviderEnabled(LocationManager.NETWORK_PROVIDER)) {
            return LocationManager.NETWORK_PROVIDER;
        }
        return null;
    }

    private static JSONObject locationJson(Location l) throws JSONException {
        JSONObject o = new JSONObject();
        o.put("lat", l.getLatitude());
        o.put("lng", l.getLongitude());
        if (l.hasAccuracy()) {
            o.put("accuracy", l.getAccuracy());
        } else {
            o.put("accuracy", JSONObject.NULL);
        }
        return o;
    }

    private static final class Watch {
        final LocationManager lm;
        final LocationListener listener;

        Watch(LocationManager lm, LocationListener listener) {
            this.lm = lm;
            this.listener = listener;
        }

        void stop() {
            try {
                lm.removeUpdates(listener);
            } catch (Exception ignored) {
            }
        }
    }
}
