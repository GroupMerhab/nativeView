package com.nativeview.jni;

import static org.junit.Assert.assertEquals;

import org.junit.Test;

/** Opcode alignment with native {@code nv_android.c} (does not load {@link NativeViewJNI}). */
public class NativeViewJNIOpsTest {

    @Test
    public void opCodesMatchNativeRunner() {
        assertEquals(1, NativeViewJniOpcodes.OP_WINDOW_CREATE);
        assertEquals(2, NativeViewJniOpcodes.OP_WINDOW_DESTROY);
        assertEquals(3, NativeViewJniOpcodes.OP_WINDOW_SHOW);
        assertEquals(4, NativeViewJniOpcodes.OP_WINDOW_HIDE);
        assertEquals(5, NativeViewJniOpcodes.OP_WINDOW_LOAD_URL);
        assertEquals(6, NativeViewJniOpcodes.OP_WINDOW_LOAD_HTML);
        assertEquals(7, NativeViewJniOpcodes.OP_WINDOW_EVAL_JS);
    }
}
