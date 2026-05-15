package com.nativeview;

/**
 * Window creation options for Android; passed to native {@code nv_window_cfg_t} via JNI.
 *
 * <p>Field names and types match the desktop {@code NvWindowCfg} style for binding parity.
 */
public class NativeViewConfig {

    /** Window title; may be {@code null} (native default applies). */
    public String title;

    public int width = 1024;
    public int height = 768;

    /** When {@code true}, initial layout may allow resize where the host supports it. */
    public boolean resizable = true;

    /** When {@code true}, enables WebView developer tooling where supported. */
    public boolean devtools;
}
