package com.nativeview.bridge;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import java.util.LinkedHashSet;
import java.util.Set;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

@RunWith(RobolectricTestRunner.class)
public class BridgeOriginTest {

    @Test
    public void normalize_file() {
        assertEquals("file://", BridgeOrigin.normalizeAllowedOrigin("file://"));
        assertEquals("file://", BridgeOrigin.normalizeAllowedOrigin("FILE:///x"));
    }

    @Test
    public void normalize_https_stripsDefaultPortAndPath() {
        assertEquals("https://myapp.com", BridgeOrigin.normalizeAllowedOrigin("https://MyApp.com/foo"));
        assertEquals("https://myapp.com", BridgeOrigin.normalizeAllowedOrigin("https://myapp.com:443"));
        assertEquals("https://myapp.com:8443", BridgeOrigin.normalizeAllowedOrigin("https://myapp.com:8443/"));
    }

    @Test
    public void normalize_invalid_returnsNull() {
        assertNull(BridgeOrigin.normalizeAllowedOrigin(""));
        assertNull(BridgeOrigin.normalizeAllowedOrigin("notaurl"));
    }

    @Test
    public void pageOrigin_https() {
        assertEquals("https://myapp.com", BridgeOrigin.pageOrigin("https://myapp.com/a?x=1#f"));
    }

    @Test
    public void pageOrigin_file() {
        assertEquals("file://", BridgeOrigin.pageOrigin("file:///android_asset/index.html"));
    }

    @Test
    public void isAllowed_exactHost_noPrefixBypass() {
        Set<String> allowed = new LinkedHashSet<>();
        allowed.add("https://myapp.com");
        assertTrue(BridgeOrigin.isAllowed("https://myapp.com/page", allowed));
        assertFalse(BridgeOrigin.isAllowed("https://myapp.com.evil/page", allowed));
        assertFalse(BridgeOrigin.isAllowed("https://evil.com", allowed));
    }

    @Test
    public void normalize_http_stripsDefaultPort() {
        assertEquals("http://api.local", BridgeOrigin.normalizeAllowedOrigin("http://api.local:80/path"));
    }

    @Test
    public void pageOrigin_ws_normalizesPort() {
        assertEquals("ws://sock.local", BridgeOrigin.pageOrigin("ws://sock.local:80/x"));
        assertEquals("wss://sock.local", BridgeOrigin.pageOrigin("wss://sock.local:443/x"));
    }

    @Test
    public void isAllowed_nullSet_false() {
        assertFalse(BridgeOrigin.isAllowed("https://a.com", null));
    }
}
