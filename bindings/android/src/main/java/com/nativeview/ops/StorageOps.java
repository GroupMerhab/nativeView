package com.nativeview.ops;

import android.content.Context;
import android.content.SharedPreferences;
import com.nativeview.NativeViewApp;
import com.nativeview.bridge.BridgeArgs;
import org.json.JSONException;
import org.json.JSONObject;

/** Key/value storage backed by {@link SharedPreferences}. */
public final class StorageOps {

    private static final String DEFAULT_PREFS = "nativeview";

    private StorageOps() {}

    public static void register(NativeViewApp app) {
        if (app == null) {
            return;
        }
        Context appCtx = app.getAppContext();
        app.handle("storage.set", null, (argsJson, resolve, reject) -> handleSet(appCtx, argsJson, resolve, reject));
        app.handle("storage.get", null, (argsJson, resolve, reject) -> handleGet(appCtx, argsJson, resolve, reject));
        app.handle("storage.remove", null, (argsJson, resolve, reject) -> handleRemove(appCtx, argsJson, resolve, reject));
        app.handle("storage.clear", null, (argsJson, resolve, reject) -> handleClear(appCtx, argsJson, resolve, reject));
    }

    private static SharedPreferences prefs(Context ctx, String name) throws JSONException {
        validateStorageSegment(name, "prefs");
        String n = name != null && !name.isEmpty() ? name : DEFAULT_PREFS;
        return ctx.getSharedPreferences(n, Context.MODE_PRIVATE);
    }

    /** Rejects path-like or traversal segments in preference names (used as filenames). */
    private static void validateStorageSegment(String value, String field) throws JSONException {
        if (value == null || value.isEmpty()) {
            return;
        }
        if (value.indexOf('\u0000') >= 0
                || value.contains("..")
                || value.indexOf('/') >= 0
                || value.indexOf('\\') >= 0) {
            throw new JSONException("invalid " + field);
        }
    }

    private static void handleSet(
            Context ctx, String argsJson, com.nativeview.ResolveCallback resolve, com.nativeview.RejectCallback reject) {
        try {
            JSONObject a = BridgeArgs.requireObject(argsJson);
            String key = a.optString("key", "");
            if (key.isEmpty()) {
                reject.reject("ERR_INVALID_ARG", "key required");
                return;
            }
            validateStorageSegment(key, "key");
            String prefsName = a.optString("prefs", null);
            SharedPreferences.Editor ed = prefs(ctx, prefsName).edit();
            if (a.has("value") && !a.isNull("value")) {
                Object v = a.get("value");
                if (v instanceof Boolean) {
                    ed.putBoolean(key, (Boolean) v);
                } else if (v instanceof Integer) {
                    ed.putInt(key, (Integer) v);
                } else if (v instanceof Long) {
                    ed.putLong(key, (Long) v);
                } else if (v instanceof Float || v instanceof Double) {
                    ed.putFloat(key, ((Number) v).floatValue());
                } else {
                    ed.putString(key, String.valueOf(v));
                }
            } else {
                ed.remove(key);
            }
            ed.apply();
            resolve.resolve("{}");
        } catch (JSONException ex) {
            reject.reject("ERR_INVALID_JSON", ex.getMessage() != null ? ex.getMessage() : "parse error");
        }
    }

    private static void handleGet(
            Context ctx, String argsJson, com.nativeview.ResolveCallback resolve, com.nativeview.RejectCallback reject) {
        try {
            JSONObject a = BridgeArgs.requireObject(argsJson);
            String key = a.optString("key", "");
            if (key.isEmpty()) {
                reject.reject("ERR_INVALID_ARG", "key required");
                return;
            }
            validateStorageSegment(key, "key");
            String prefsName = a.optString("prefs", null);
            String type = a.optString("type", "string");
            SharedPreferences p = prefs(ctx, prefsName);
            if (!p.contains(key)) {
                JSONObject o = new JSONObject();
                o.put("value", JSONObject.NULL);
                resolve.resolve(o.toString());
                return;
            }
            JSONObject o = new JSONObject();
            switch (type) {
                case "boolean":
                    o.put("value", p.getBoolean(key, false));
                    break;
                case "int":
                    o.put("value", p.getInt(key, 0));
                    break;
                case "long":
                    o.put("value", p.getLong(key, 0L));
                    break;
                case "float":
                    o.put("value", p.getFloat(key, 0f));
                    break;
                default:
                    o.put("value", p.getString(key, null));
                    break;
            }
            resolve.resolve(o.toString());
        } catch (JSONException ex) {
            reject.reject("ERR_INVALID_JSON", ex.getMessage() != null ? ex.getMessage() : "parse error");
        }
    }

    private static void handleRemove(
            Context ctx, String argsJson, com.nativeview.ResolveCallback resolve, com.nativeview.RejectCallback reject) {
        try {
            JSONObject a = BridgeArgs.requireObject(argsJson);
            String key = a.optString("key", "");
            if (key.isEmpty()) {
                reject.reject("ERR_INVALID_ARG", "key required");
                return;
            }
            validateStorageSegment(key, "key");
            String prefsName = a.optString("prefs", null);
            prefs(ctx, prefsName).edit().remove(key).apply();
            resolve.resolve("{}");
        } catch (JSONException ex) {
            reject.reject("ERR_INVALID_JSON", ex.getMessage() != null ? ex.getMessage() : "parse error");
        }
    }

    private static void handleClear(
            Context ctx, String argsJson, com.nativeview.ResolveCallback resolve, com.nativeview.RejectCallback reject) {
        try {
            JSONObject a = BridgeArgs.requireObject(argsJson);
            String prefsName = a.optString("prefs", null);
            prefs(ctx, prefsName).edit().clear().apply();
            resolve.resolve("{}");
        } catch (JSONException ex) {
            reject.reject("ERR_INVALID_JSON", ex.getMessage() != null ? ex.getMessage() : "parse error");
        }
    }
}
