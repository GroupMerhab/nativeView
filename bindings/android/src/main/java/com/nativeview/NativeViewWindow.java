package com.nativeview;

import com.nativeview.jni.NativeViewJNI;

/**
 * A native-backed window handle; mirrors the desktop Java binding surface while WebView hosting
 * remains in the Android {@link com.nativeview.jni.NativeViewHost} implementation.
 */
public class NativeViewWindow {

    private final long nativeHandle;

    NativeViewWindow(long nativeHandle) {
        this.nativeHandle = nativeHandle;
    }

    /** Opaque native address; zero when invalid. */
    public long getNativeHandle() {
        return nativeHandle;
    }

    public void show() {
        if (nativeHandle == 0L) {
            return;
        }
        NativeViewJNI.nativeShow(nativeHandle);
    }

    public void hide() {
        if (nativeHandle == 0L) {
            return;
        }
        NativeViewJNI.nativeHide(nativeHandle);
    }

    public void load_url(String url) {
        if (nativeHandle == 0L || url == null) {
            return;
        }
        NativeViewJNI.nativeLoadUrl(nativeHandle, url);
    }

    public void load_html(String html, String baseUrl) {
        if (nativeHandle == 0L) {
            return;
        }
        NativeViewJNI.nativeLoadHtml(nativeHandle, html != null ? html : "", baseUrl);
    }

    public void eval_js(String js) {
        if (nativeHandle == 0L) {
            return;
        }
        NativeViewJNI.nativeEvalJs(nativeHandle, js != null ? js : "");
    }

    public void set_title(String title) {
        if (nativeHandle == 0L || title == null) {
            return;
        }
        NativeViewJNI.nativeSetTitle(nativeHandle, title);
    }

    public void set_size(int width, int height) {
        if (nativeHandle == 0L) {
            return;
        }
        NativeViewJNI.nativeSetSize(nativeHandle, width, height);
    }

    public void on_message(MessageCallback callback) {
        if (nativeHandle == 0L) {
            return;
        }
        NativeViewJNI.nativeOnMessage(nativeHandle, callback);
    }

    public void on_ready(ReadyCallback callback) {
        if (nativeHandle == 0L) {
            return;
        }
        NativeViewJNI.nativeOnReady(nativeHandle, callback);
    }

    public void on_close(CloseCallback callback) {
        if (nativeHandle == 0L) {
            return;
        }
        NativeViewJNI.nativeOnClose(nativeHandle, callback);
    }

    /**
     * Sends an event to the page via {@code nv_send}. {@code event} must be non-null; use {@code
     * ""} for IPC-style payloads consumed by {@code window.__nv._emit}.
     */
    public void emit(String event, String json) {
        if (nativeHandle == 0L || event == null) {
            return;
        }
        NativeViewJNI.nativeEmit(nativeHandle, event, json != null ? json : "null");
    }
}
