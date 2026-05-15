package io.jamharah.nativeview.parity;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;

import org.json.JSONException;
import org.json.JSONObject;
import org.json.JSONTokener;
import org.junit.Test;

/**
 * Portable wire-envelope checks compiled into both the Android library unit tests and the desktop
 * {@code bindings/java} Maven tests. Keeps the JS bridge contract aligned across bindings without
 * Android APIs.
 */
public class WireContractParityTest {

    private static JSONObject parseTopLevelObject(String wireJson) throws JSONException {
        if (wireJson == null) {
            throw new JSONException("wire is null");
        }
        Object v = new JSONTokener(wireJson.trim()).nextValue();
        if (!(v instanceof JSONObject)) {
            throw new JSONException("wire must be a JSON object");
        }
        return (JSONObject) v;
    }

    private static String extractMethod(String wireJson) {
        if (wireJson == null) {
            return null;
        }
        try {
            String e = new JSONObject(wireJson).optString("e", "");
            return e.isEmpty() ? null : e;
        } catch (JSONException unused) {
            return null;
        }
    }

    private static long extractSeq(String wireJson) {
        if (wireJson == null) {
            return 0L;
        }
        try {
            return new JSONObject(wireJson).optLong("s", 0L);
        } catch (JSONException unused) {
            return 0L;
        }
    }

    @Test
    public void extractMethod_readsEField() {
        assertEquals("app.ping", extractMethod("{\"e\":\"app.ping\",\"s\":1,\"d\":{}}"));
        assertNull(extractMethod("{\"s\":1}"));
        assertNull(extractMethod(null));
    }

    @Test
    public void extractSeq_readsSField() {
        assertEquals(42L, extractSeq("{\"e\":\"x\",\"s\":42}"));
        assertEquals(0L, extractSeq("{\"e\":\"x\"}"));
        assertEquals(0L, extractSeq(null));
    }

    @Test
    public void parseTopLevelObject_rejectsArray() {
        JSONException ex = assertThrows(JSONException.class, () -> parseTopLevelObject("[1,2]"));
        assertTrue(ex.getMessage().contains("object"));
    }

    @Test
    public void parseTopLevelObject_trimsWhitespace() throws JSONException {
        JSONObject o = parseTopLevelObject("  {\"e\":\"a\"}  ");
        assertEquals("a", o.optString("e"));
    }
}
