package com.nativeview.bridge;

import org.json.JSONException;
import org.json.JSONObject;
import org.json.JSONTokener;

/**
 * Validates bridge handler argument payloads: must decode to a JSON object (not array, string, or
 * primitive).
 */
public final class BridgeArgs {

    private BridgeArgs() {}

    /**
     * Parses {@code argsJson} as a JSON object. Empty, whitespace, and JSON {@code null} yield an
     * empty object.
     *
     * @throws JSONException when the payload is not a JSON object
     */
    public static JSONObject requireObject(String argsJson) throws JSONException {
        if (argsJson == null || argsJson.trim().isEmpty()) {
            return new JSONObject();
        }
        String t = argsJson.trim();
        if ("null".equals(t)) {
            return new JSONObject();
        }
        Object v = new JSONTokener(t).nextValue();
        if (!(v instanceof JSONObject)) {
            throw new JSONException("args must be a JSON object");
        }
        return (JSONObject) v;
    }
}
