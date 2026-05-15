package com.nativeview.jni;

/**
 * JNI dispatch opcodes mirrored from {@code NV_ANDROID_OP_*} in {@code nv_android.c}. Kept in a
 * separate type so unit tests can assert alignment without loading {@link NativeViewJNI} (which
 * loads {@code libnativeview}).
 */
public final class NativeViewJniOpcodes {

    private NativeViewJniOpcodes() {}

    public static final int OP_WINDOW_CREATE = 1;
    public static final int OP_WINDOW_DESTROY = 2;
    public static final int OP_WINDOW_SHOW = 3;
    public static final int OP_WINDOW_HIDE = 4;
    public static final int OP_WINDOW_LOAD_URL = 5;
    public static final int OP_WINDOW_LOAD_HTML = 6;
    public static final int OP_WINDOW_EVAL_JS = 7;
}
