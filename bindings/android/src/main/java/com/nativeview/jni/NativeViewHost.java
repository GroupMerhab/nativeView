package com.nativeview.jni;

/**
 * Implemented by the Android UI host (typically an {@code Activity}) that owns the {@link
 * android.webkit.WebView}. Native code forwards window operations (load URL/HTML, eval JS) via
 * {@link #dispatch(int, long, String, String)} using the opcode constants on {@link NativeViewJNI}.
 */
public interface NativeViewHost {

    /**
     * @param op        One of {@link NativeViewJNI#OP_WINDOW_CREATE}, etc.
     * @param windowPtr Opaque native window address (same as Java-side window handle).
     * @param s1        Primary string argument (e.g. URL, HTML, or script).
     * @param s2        Secondary string argument (e.g. base URL for HTML load).
     */
    void dispatch(int op, long windowPtr, String s1, String s2);
}
