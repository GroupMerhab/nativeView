package com.nativeview;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Test;

public class NativeViewConfigTest {

    @Test
    public void defaults() {
        NativeViewConfig c = new NativeViewConfig();
        assertEquals(1024, c.width);
        assertEquals(768, c.height);
        assertTrue(c.resizable);
        assertFalse(c.devtools);
    }
}
