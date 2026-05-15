package com.nativeview.bridge;

import com.nativeview.NativeViewApp;
import com.nativeview.NativeViewWindow;

/**
 * Holds the {@link NativeViewWindow} for the in-flight bridge handler (set on the UI thread around
 * {@link com.nativeview.NVHandler#handle}).
 */
public final class BridgeDispatchContext {

    private static final ThreadLocal<NativeViewWindow> WINDOW = new ThreadLocal<>();
    private static final ThreadLocal<NativeViewApp> APP = new ThreadLocal<>();

    private BridgeDispatchContext() {}

    public static void set(NativeViewApp app, NativeViewWindow window) {
        APP.set(app);
        WINDOW.set(window);
    }

    public static void clear() {
        WINDOW.remove();
        APP.remove();
    }

    /** May be {@code null} if not inside a bridge dispatch. */
    public static NativeViewWindow window() {
        return WINDOW.get();
    }

    /** May be {@code null} if not inside a bridge dispatch. */
    public static NativeViewApp app() {
        return APP.get();
    }
}
