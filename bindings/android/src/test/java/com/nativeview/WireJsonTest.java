package com.nativeview;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;

import org.json.JSONException;
import org.junit.Test;

public class WireJsonTest {

    @Test
    public void extractMethod_readsEField() {
        assertEquals("ping", WireJson.extractMethod("{\"e\":\"ping\",\"s\":0,\"d\":1}"));
        assertNull(WireJson.extractMethod("{\"s\":0}"));
        assertNull(WireJson.extractMethod(null));
    }

    @Test
    public void extractSeq_readsSField() {
        assertEquals(42L, WireJson.extractSeq("{\"e\":\"ping\",\"s\":42,\"d\":1}"));
        assertEquals(0L, WireJson.extractSeq("{\"e\":\"ping\",\"d\":1}"));
        assertEquals(0L, WireJson.extractSeq(null));
    }

    @Test
    public void extractArgsJson_objectAndNull() throws JSONException {
        assertEquals("null", WireJson.extractArgsJson("{\"e\":\"x\",\"d\":null}"));
        assertEquals("{\"a\":1}", WireJson.extractArgsJson("{\"e\":\"x\",\"d\":{\"a\":1}}"));
    }

    @Test
    public void extractArgsJson_stringUsesJsonQuote() throws JSONException {
        assertEquals("\"hi\"", WireJson.extractArgsJson("{\"e\":\"x\",\"d\":\"hi\"}"));
    }

    @Test
    public void parseWireObject_acceptsObject() throws JSONException {
        assertEquals("ping", WireJson.parseWireObject("{\"e\":\"ping\",\"s\":1}").optString("e"));
    }

    @Test(expected = JSONException.class)
    public void parseWireObject_rejectsArray() throws JSONException {
        WireJson.parseWireObject("[1,2]");
    }
}
