package com.nativeview.bridge;

import static org.junit.Assert.assertEquals;

import org.json.JSONException;
import org.json.JSONObject;
import org.junit.Test;

public class BridgeArgsTest {

    @Test
    public void requireObject_emptyAndNullLiteral() throws JSONException {
        assertEquals(0, BridgeArgs.requireObject(null).length());
        assertEquals(0, BridgeArgs.requireObject("   ").length());
        assertEquals(0, BridgeArgs.requireObject("null").length());
    }

    @Test
    public void requireObject_parsesObject() throws JSONException {
        JSONObject o = BridgeArgs.requireObject("{\"a\":1}");
        assertEquals(1, o.optInt("a"));
    }

    @Test(expected = JSONException.class)
    public void requireObject_rejectsArray() throws JSONException {
        BridgeArgs.requireObject("[1,2]");
    }

    @Test(expected = JSONException.class)
    public void requireObject_rejectsBareString() throws JSONException {
        BridgeArgs.requireObject("\"x\"");
    }

    @Test(expected = JSONException.class)
    public void requireObject_rejectsPrimitiveNumber() throws JSONException {
        BridgeArgs.requireObject("42");
    }
}
