package com.nativeview.bridge;

import org.json.JSONObject;

/**
 * JavaScript snippets delivered to the page for bridge resolve/reject. Shared with unit tests so
 * the error surface stays stable.
 */
public final class BridgeClientScript {

    private BridgeClientScript() {}

    /** {@code null} when {@code seq <= 0} (caller should not evaluate). */
    public static String buildResolveInvoke(long seq, String json) {
        if (seq <= 0L) {
            return null;
        }
        String secondArg;
        if (json == null || json.trim().isEmpty()) {
            secondArg = "null";
        } else {
            secondArg = "JSON.parse(" + JSONObject.quote(json) + ")";
        }
        return "(function(){try{if(typeof NativeView!=='undefined'&&NativeView._resolve){NativeView._resolve("
                + seq
                + ","
                + secondArg
                + ");}}catch(e){}})()";
    }

    /** {@code null} when {@code seq <= 0} (caller should not evaluate). */
    public static String buildRejectInvoke(long seq, String code, String message) {
        if (seq <= 0L) {
            return null;
        }
        String c = JSONObject.quote(code != null ? code : "ERR_UNKNOWN");
        String m = JSONObject.quote(message != null ? message : "");
        return "(function(){try{if(typeof NativeView!=='undefined'&&NativeView._reject){NativeView._reject("
                + seq
                + ",{code:"
                + c
                + ",msg:"
                + m
                + "});}}catch(e){}})()";
    }
}
