package com.nativeview.bridge;

import android.webkit.WebView;
import org.json.JSONException;
import org.json.JSONObject;
import org.json.JSONTokener;

/** Builds {@link WebView#evaluateJavascript(String, android.webkit.ValueCallback)} payloads for {@code NativeView} helpers. */
public final class BridgeJavaScript {

    private BridgeJavaScript() {}

    /**
     * Builds the JS snippet that delivers a push event through {@code _ipc.receive} / {@code
     * NativeView._emit}. {@code jsonPayload} is the JSON value for wire field {@code d} (object
     * text, {@code null}, etc.).
     */
    static String buildEmitEventJavascript(String event, String jsonPayload) {
        if (event == null) {
            return "(function(){})();";
        }
        JSONObject wire = new JSONObject();
        try {
            wire.put("e", event);
            wire.put("d", parsePayloadValue(jsonPayload));
        } catch (JSONException ex) {
            try {
                wire.put("e", event);
                wire.put("d", JSONObject.NULL);
            } catch (JSONException ignored) {
                return "(function(){})();";
            }
        }
        String wireStr = wire.toString();
        String quoted = JSONObject.quote(wireStr);
        return "(function(){try{if(typeof NativeView!=='undefined'&&NativeView._emit){NativeView._emit('',"
                + quoted
                + ");}else if(typeof _ipc!=='undefined'&&_ipc.receive){_ipc.receive('',"
                + quoted
                + ");}}catch(e){console.error('NativeView push',e);}})()";
    }

    private static Object parsePayloadValue(String jsonPayload) throws JSONException {
        if (jsonPayload == null) {
            return JSONObject.NULL;
        }
        String t = jsonPayload.trim();
        if (t.isEmpty() || "null".equals(t)) {
            return JSONObject.NULL;
        }
        return new JSONTokener(t).nextValue();
    }

    /**
     * Delivers a push-style event to the page (same shape as native {@code nv_send}): wire {@code
     * {e, d}} parsed by {@code _ipc.receive}. {@code jsonPayload} is serialized JSON for the
     * {@code d} field (for example {@code "{}"} or {@code "{\"url\":\"myapp://x\"}"}).
     */
    public static void emitEvent(WebView webView, String event, String jsonPayload) {
        if (webView == null || event == null) {
            return;
        }
        webView.evaluateJavascript(buildEmitEventJavascript(event, jsonPayload), null);
    }
}
