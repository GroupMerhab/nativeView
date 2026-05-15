package com.nativeview.bridge;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import org.junit.Test;

public class BridgeJavaScriptTest {

    @Test
    public void buildEmitEventJavascript_nullEventIsNoOp() {
        assertEquals("(function(){})();", BridgeJavaScript.buildEmitEventJavascript(null, "{}"));
    }

    @Test
    public void buildEmitEventJavascript_containsWireEventAndIpcReceive() {
        String js = BridgeJavaScript.buildEmitEventJavascript("app.resume", "{}");
        assertTrue(js.contains("_ipc.receive"));
        assertTrue(js.contains("NativeView._emit"));
        assertTrue(js.contains("app.resume"));
    }

    @Test
    public void buildEmitEventJavascript_orientationPayload() {
        String js = BridgeJavaScript.buildEmitEventJavascript("device.orientation", "{\"landscape\":true}");
        assertTrue(js.contains("device.orientation"));
        assertTrue(js.contains("landscape"));
    }

    @Test
    public void buildEmitEventJavascript_invalidJsonPayloadUsesNullD() {
        // JSONTokener does not throw on "not-json{" (it yields the string "not-json"); "{" forces JSONException.
        String js = BridgeJavaScript.buildEmitEventJavascript("evt", "{");
        assertTrue(js.contains("evt"));
        assertTrue(js.contains("null"));
    }
}
