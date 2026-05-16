package com.nativeview.jni;

import android.content.Context;
import com.nativeview.CloseCallback;
import com.nativeview.MessageCallback;
import com.nativeview.NativeViewConfig;
import com.nativeview.ReadyCallback;

/**
 * JNI surface for {@code libnativeview}. Loads the shared library and declares native entry points
 * that map to the nativeview C API ({@code nv.h}).
 *
 * <p><b>Dispatch host:</b> If {@link #nativeInit(Context)} is called with a {@link Context} that
 * implements {@link NativeViewHost} (for example {@code MainActivity extends Activity implements
 * NativeViewHost}), that object receives {@link NativeViewHost#dispatch(int, long, String, String)}
 * for WebView operations from native code. {@link #nativeBindDispatchHost(NativeViewHost)} is also
 * invoked from {@link com.nativeview.NativeViewApp} so binding succeeds even when JNI {@code
 * FindClass} for the interface fails during {@code nativeInit}. Otherwise call the legacy {@code
 * NativeviewBridge} JNI {@code nativeSetHost} from the sample runner.
 *
 * <p>The shared library is loaded lazily from {@link #ensureLoaded()} so embedders can surface a
 * clear error when {@code libnativeview.so} is missing (typical if {@code jniLibs} was not
 * populated; emulators usually need the {@code x86_64} ABI).
 */
public final class NativeViewJNI {

    private static final Object LOAD_LOCK = new Object();
    private static volatile boolean libraryLoaded;

    /** @see NativeViewJniOpcodes#OP_WINDOW_CREATE */
    public static final int OP_WINDOW_CREATE = NativeViewJniOpcodes.OP_WINDOW_CREATE;
    /** @see NativeViewJniOpcodes#OP_WINDOW_DESTROY */
    public static final int OP_WINDOW_DESTROY = NativeViewJniOpcodes.OP_WINDOW_DESTROY;
    /** @see NativeViewJniOpcodes#OP_WINDOW_SHOW */
    public static final int OP_WINDOW_SHOW = NativeViewJniOpcodes.OP_WINDOW_SHOW;
    /** @see NativeViewJniOpcodes#OP_WINDOW_HIDE */
    public static final int OP_WINDOW_HIDE = NativeViewJniOpcodes.OP_WINDOW_HIDE;
    /** @see NativeViewJniOpcodes#OP_WINDOW_LOAD_URL */
    public static final int OP_WINDOW_LOAD_URL = NativeViewJniOpcodes.OP_WINDOW_LOAD_URL;
    /** @see NativeViewJniOpcodes#OP_WINDOW_LOAD_HTML */
    public static final int OP_WINDOW_LOAD_HTML = NativeViewJniOpcodes.OP_WINDOW_LOAD_HTML;
    /** @see NativeViewJniOpcodes#OP_WINDOW_EVAL_JS */
    public static final int OP_WINDOW_EVAL_JS = NativeViewJniOpcodes.OP_WINDOW_EVAL_JS;

    private NativeViewJNI() {}

    /**
     * Loads {@code libnativeview.so} once. Call before any JNI native method (for example via
     * {@link com.nativeview.NativeViewApp} construction).
     *
     * @throws IllegalStateException when the shared library is not packaged for this process ABI
     */
    public static void ensureLoaded() {
        if (libraryLoaded) {
            return;
        }
        synchronized (LOAD_LOCK) {
            if (libraryLoaded) {
                return;
            }
            try {
                System.loadLibrary("nativeview");
            } catch (UnsatisfiedLinkError e) {
                throw new IllegalStateException(
                        "Could not load libnativeview.so. Build the nativeview Android shared library and"
                            + " copy it to bindings/android/src/main/jniLibs/<abi>/ (see"
                            + " bindings/android/scripts/build_nativeview_so.sh). Most emulators use"
                            + " the x86_64 ABI; devices typically use arm64-v8a.",
                        e);
            }
            libraryLoaded = true;
        }
    }

    /**
     * Creates the native application singleton (once) and, when {@code context} implements {@link
     * NativeViewHost}, registers it for C→Java WebView dispatch.
     */
    public static native void nativeInit(Context context);

    /**
     * Registers {@code host} for C→Java WebView dispatch ({@link NativeViewHost#dispatch}). Pass
     * {@code null} to clear. Idempotent; safe to call after {@link #nativeInit(Context)}.
     */
    public static native void nativeBindDispatchHost(NativeViewHost host);

    /** Destroys all windows for the JNI singleton app, clears the dispatch host, and frees the app. */
    public static native void nativeDestroy();

    /** Creates a window; requires {@link #nativeInit(Context)} first. */
    public static native long nativeCreateWindow(NativeViewConfig config);

    public static native void nativeDestroyWindow(long windowId);

    public static native void nativeLoadUrl(long windowId, String url);

    public static native void nativeLoadHtml(long windowId, String html, String baseUrl);

    public static native void nativeEvalJs(long windowId, String js);

    public static native void nativeSetTitle(long windowId, String title);

    public static native void nativeSetSize(long windowId, int width, int height);

    public static native void nativeShow(long windowId);

    public static native void nativeHide(long windowId);

    /** Sends an event to the embedded page ({@code nv_send}). */
    public static native void nativeEmit(long windowId, String event, String json);

    /** Forwards a raw bridge wire JSON string to native IPC ({@code nv_ipc_dispatch}). */
    public static native void nativeDispatchWebMessage(long windowId, String wireJson);

    public static native void nativeOnMessage(long windowId, MessageCallback callback);

    public static native void nativeOnReady(long windowId, ReadyCallback callback);

    public static native void nativeOnClose(long windowId, CloseCallback callback);

    /** Signals native {@code nv_app_quit} when a run loop is active (no-op on typical Android). */
    public static native void nativeQuit();
}
