package com.nativeview.bridge;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import org.junit.Test;

public class BridgeClientScriptTest {

    @Test
    public void buildRejectInvoke_embedsCodeAndMessage() {
        String js = BridgeClientScript.buildRejectInvoke(9L, "ERR_PERMISSION", "no");
        assertNotNull(js);
        assertTrue(js.contains("NativeView._reject"));
        assertTrue(js.contains("9"));
        assertTrue(js.contains("\"ERR_PERMISSION\""));
        assertTrue(js.contains("\"no\""));
        assertTrue(js.contains("code:"));
        assertTrue(js.contains("msg:"));
    }

    @Test
    public void buildRejectInvoke_nullCodeUsesUnknown() {
        String js = BridgeClientScript.buildRejectInvoke(1L, null, null);
        assertNotNull(js);
        assertTrue(js.contains("\"ERR_UNKNOWN\""));
    }

    @Test
    public void buildRejectInvoke_nonPositiveSeqReturnsNull() {
        assertNull(BridgeClientScript.buildRejectInvoke(0L, "X", "Y"));
        assertNull(BridgeClientScript.buildRejectInvoke(-1L, "X", "Y"));
    }

    @Test
    public void buildResolveInvoke_emptyJsonUsesNullLiteral() {
        String js = BridgeClientScript.buildResolveInvoke(3L, "   ");
        assertNotNull(js);
        assertTrue(js.contains("NativeView._resolve"));
        assertTrue(js.contains("3,null"));
    }

    @Test
    public void buildResolveInvoke_nonEmptyWrapsJsonParse() {
        String js = BridgeClientScript.buildResolveInvoke(2L, "{\"a\":1}");
        assertNotNull(js);
        assertTrue(js.contains("JSON.parse("));
        assertTrue(js.contains("2"));
    }

    @Test
    public void buildResolveInvoke_nonPositiveSeqReturnsNull() {
        assertNull(BridgeClientScript.buildResolveInvoke(0L, "{}"));
    }
}
