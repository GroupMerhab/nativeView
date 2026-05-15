package com.nativeview;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;

import com.nativeview.bridge.BridgeDispatchContext;
import org.junit.Test;

public class BridgeDispatchContextTest {

    @Test
    public void setThenRead_windowAndApp() {
        NativeViewWindow w = new NativeViewWindow(42L);
        BridgeDispatchContext.set(null, w);
        assertEquals(42L, BridgeDispatchContext.window().getNativeHandle());
        assertNull(BridgeDispatchContext.app());
        BridgeDispatchContext.clear();
        assertNull(BridgeDispatchContext.window());
    }
}
