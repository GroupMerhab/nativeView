package com.nativeview;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import java.nio.charset.StandardCharsets;
import java.util.Base64;
import org.junit.Test;

public class NativeViewWebViewTest {

    @Test
    public void createInjectJavascriptExpression_roundTripsUtf8() {
        String src = "var π = 2;\nconsole.log('ok');";
        String expr = NativeViewWebView.createInjectJavascriptExpression(src);
        assertTrue(expr.contains("atob('"));
        int start = expr.indexOf("atob('") + 6;
        int end = expr.indexOf("')", start);
        String b64 = expr.substring(start, end);
        String decoded = new String(Base64.getDecoder().decode(b64), StandardCharsets.UTF_8);
        assertEquals(src, decoded);
    }

    @Test
    public void createInjectJavascriptExpression_emptyYieldsNoOp() {
        assertEquals("(function(){})();", NativeViewWebView.createInjectJavascriptExpression(""));
        assertEquals("(function(){})();", NativeViewWebView.createInjectJavascriptExpression(null));
    }

    @Test
    public void wrapNativeViewJsForAndroid_includesBridgeAndNativeViewShims() {
        String wrapped = NativeViewWebView.wrapNativeViewJsForAndroid("var x=1;");
        assertTrue(wrapped.contains("_NVBridge.post"));
        assertTrue(wrapped.contains("NativeView._emit"));
        assertTrue(wrapped.contains("NativeView._resolve"));
        assertTrue(wrapped.contains("NativeView._reject"));
    }
}
