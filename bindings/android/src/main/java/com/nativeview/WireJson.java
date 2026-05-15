package com.nativeview;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.json.JSONTokener;

/** Parses nativeview bridge wire JSON for the Android Java binding. */
public final class WireJson {

    private WireJson() {}

    /**
     * Parses the top-level bridge envelope as a JSON object (rejects arrays and primitives).
     *
     * @throws JSONException when the payload is not a JSON object
     */
    public static JSONObject parseWireObject(String wireJson) throws JSONException {
        if (wireJson == null) {
            throw new JSONException("wire is null");
        }
        Object v = new JSONTokener(wireJson.trim()).nextValue();
        if (!(v instanceof JSONObject)) {
            throw new JSONException("wire must be a JSON object");
        }
        return (JSONObject) v;
    }

    public static String extractMethod(String wireJson) {
        if (wireJson == null) {
            return null;
        }
        try {
            JSONObject o = new JSONObject(wireJson);
            String e = o.optString("e", "");
            return e.isEmpty() ? null : e;
        } catch (JSONException unused) {
            return null;
        }
    }

    public static long extractSeq(String wireJson) {
        if (wireJson == null) {
            return 0L;
        }
        try {
            return new JSONObject(wireJson).optLong("s", 0L);
        } catch (JSONException unused) {
            return 0L;
        }
    }

    public static String extractArgsJson(JSONObject wire) throws JSONException {
        return jsonArgFromWire(wire);
    }

    public static String extractArgsJson(String wireJson) throws JSONException {
        return jsonArgFromWire(new JSONObject(wireJson));
    }

    private static String jsonArgFromWire(JSONObject wire) throws JSONException {
        if (!wire.has("d") || wire.isNull("d")) {
            return "null";
        }
        Object d = wire.get("d");
        if (d instanceof JSONObject) {
            return ((JSONObject) d).toString();
        }
        if (d instanceof JSONArray) {
            return ((JSONArray) d).toString();
        }
        if (d instanceof String) {
            return JSONObject.quote((String) d);
        }
        if (d instanceof Boolean) {
            return ((Boolean) d) ? "true" : "false";
        }
        if (d instanceof Number) {
            return String.valueOf(d);
        }
        return JSONObject.quote(String.valueOf(d));
    }
}
