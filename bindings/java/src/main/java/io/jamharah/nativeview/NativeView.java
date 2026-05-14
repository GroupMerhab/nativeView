package io.jamharah.nativeview;

/**
 * Low-level JNI to the nativeview public C API ({@code include/nv.h}, {@code include/nv_hotkey.h},
 * and {@code nv_version_string} / {@code nv_get_version_info} / {@code nv_bench_now} from {@code
 * include/nv_util.h}).
 *
 * <p>Call {@link #loadLibrary()} once before any native method. The JNI shared library must be on
 * {@code java.library.path} (typically {@code libnativeview_jni.so}, {@code libnativeview_jni.dylib},
 * or {@code nativeview_jni.dll}). Build it with CMake {@code -DNV_BUILD_SHARED=ON
 * -DNV_BUILD_JAVA_JNI=ON}; place {@code libnativeview} and {@code libnativeview_jni} in the same
 * directory (RPATH is set on macOS/Linux when using the provided {@code bindings/java/CMakeLists.txt}).
 *
 * <p>On macOS, start the JVM with {@code -XstartOnFirstThread} so {@code public static void main}
 * runs on the process primordial thread (required by AppKit). Otherwise {@link #loadLibrary()}
 * throws {@link IllegalStateException}.
 *
 * <p>Opaque handles are {@code long} address values (zero when invalid). Callback interfaces must
 * stay reachable for as long as they are registered; {@code event} / {@code json} in {@link
 * NvMessageListener} are only valid during the callback.
 *
 * <p>SPDX-License-Identifier: Apache-2.0
 */
public final class NativeView {

  public static final int NV_HOTKEY_OK = 0;
  public static final int NV_HOTKEY_ERR_INVALID = -1;
  public static final int NV_HOTKEY_ERR_ALREADY_EXISTS = -2;
  public static final int NV_HOTKEY_ERR_NOT_SUPPORTED = -3;
  public static final int NV_HOTKEY_ERR_PLATFORM = -4;

  private static volatile boolean libraryLoaded;

  private static native boolean nvIsProcessMainThread();

  private NativeView() {}

  private static boolean isMacOs() {
    String os = System.getProperty("os.name", "");
    return os != null && os.toLowerCase(java.util.Locale.ROOT).contains("mac");
  }

  /** Loads {@code nativeview_jni} via {@link System#loadLibrary(String)}. Idempotent. */
  public static synchronized void loadLibrary() {
    if (!libraryLoaded) {
      System.loadLibrary("nativeview_jni");
      if (isMacOs()
          && !Boolean.getBoolean("nativeview.mac.skipMainThreadCheck")
          && !nvIsProcessMainThread()) {
        throw new IllegalStateException(
            "nativeview on macOS requires the JVM primordial thread for AppKit. "
                + "Start Java with -XstartOnFirstThread (see docs/Java.md). "
                + "To skip this check, set -Dnativeview.mac.skipMainThreadCheck=true (not recommended).");
      }
      libraryLoaded = true;
    }
  }

  public static long nvWindowCreate(long app, NvWindowCfg cfg) {
    if (cfg == null) {
      return nvWindowCreate0(app, null, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    }
    return nvWindowCreate0(
        app,
        cfg.title,
        cfg.width,
        cfg.height,
        cfg.minWidth,
        cfg.minHeight,
        cfg.resizable,
        cfg.frameless,
        cfg.transparent,
        cfg.devtools,
        cfg.modal);
  }

  private static native long nvWindowCreate0(
      long app,
      String title,
      int width,
      int height,
      int minWidth,
      int minHeight,
      int resizable,
      int frameless,
      int transparent,
      int devtools,
      int modal);

  public static native long nvAppCreate();

  public static native void nvAppRun(long app);

  public static native void nvAppQuit(long app);

  public static native void nvAppDestroy(long app);

  public static native void nvWindowClose(long window);

  public static native void nvWindowDestroy(long window);

  public static native void nvWindowShow(long window);

  public static native void nvWindowHide(long window);

  public static native void nvWindowSetTitle(long window, String title);

  public static native void nvWindowSetSize(long window, int width, int height);

  public static native void nvWindowSetMinSize(long window, int width, int height);

  public static native void nvWindowCenter(long window);

  public static native void nvWindowFullscreen(long window, int enable);

  public static native int nvWindowIsFullscreen(long window);

  public static native void nvWindowMinimize(long window);

  public static native void nvWindowMaximize(long window);

  public static native void nvWindowRestore(long window);

  public static native void nvWindowFocus(long window);

  public static native int nvWindowIsFocused(long window);

  public static native void nvWindowSetResizable(long window, int enable);

  public static native void nvWindowSetAlwaysOnTop(long window, int enable);

  public static native void nvWindowSetOpacity(long window, int opacityPct);

  public static native void nvWindowSetZoomFactor(long window, double factor);

  public static native void nvWindowRequestClose(long window);

  public static native void nvLoadUrl(long window, String url);

  public static native void nvLoadHtml(long window, String html, String baseUrl);

  /**
   * @param html UTF-8 bytes; {@code length} &lt; 0 uses full array length
   */
  public static native void nvLoadHtmlRef(long window, byte[] html, long length, String baseUrl);

  public static native void nvEvalJs(long window, String js);

  public static native void nvEvalJsBatch(long window, String[] scripts);

  public static native void nvOnMessage(long window, NvMessageListener listener);

  public static native void nvOnReady(long window, NvReadyListener listener);

  public static native void nvWindowOnClose(long window, NvCloseListener listener);

  public static native void nvSend(long window, String event, String json);

  public static native int nvWindowRegisterHotkey(long window, String id, String combo);

  public static native void nvWindowUnregisterHotkey(long window, String id);

  public static native String nvVersionString();

  public static native NvVersionInfo nvGetVersionInfo();

  public static native long nvBenchNow();
}
