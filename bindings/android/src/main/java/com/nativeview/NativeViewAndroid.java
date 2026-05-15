package com.nativeview;

import android.app.Activity;
import android.content.Context;
import android.content.res.AssetManager;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.view.View;
import android.view.Window;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;
import android.view.inputmethod.InputMethodManager;
import com.nativeview.jni.NativeViewJNI;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.lang.ref.WeakReference;
import java.nio.charset.StandardCharsets;

/**
 * Android-specific embedding helpers for nativeview.
 *
 * <p><b>Not part of the core API:</b> nothing in this class corresponds to {@code nv_*} C
 * functions, desktop JNI, or the portable {@code NativeView.*} JavaScript module. These entry
 * points exist only in the {@code com.nativeview} Android library and are safe to ignore on
 * macOS, Windows, and Linux.
 *
 * <p>Runtime UI helpers ({@link #setStatusBarColor}, {@link #setFullScreen}, {@link
 * #setOrientation}, {@link #setKeepScreenOn}, {@link #hideKeyboard}, {@link #showKeyboard})
 * apply to the activity registered with {@link #attachHostActivity(Activity)}. {@link
 * NativeViewActivity} attaches in {@code onResume} and detaches in {@code onDestroy}; custom
 * activities should do the same.
 */
public final class NativeViewAndroid {

    private static final Object HOST_LOCK = new Object();
    private static WeakReference<Activity> uiHostRef = new WeakReference<>(null);

    private NativeViewAndroid() {}

    /**
     * Registers {@code activity} as the target for Android-only UI helpers below. Call from {@code
     * Activity.onResume}; pair with {@link #detachHostActivity(Activity)} in {@code onDestroy}.
     */
    public static void attachHostActivity(Activity activity) {
        if (activity == null) {
            return;
        }
        synchronized (HOST_LOCK) {
            uiHostRef = new WeakReference<>(activity);
        }
    }

    /**
     * Clears the UI host when it is still the registered activity. Safe if {@code activity} was
     * never attached.
     */
    public static void detachHostActivity(Activity activity) {
        if (activity == null) {
            return;
        }
        synchronized (HOST_LOCK) {
            Activity cur = uiHostRef.get();
            if (cur == activity) {
                uiHostRef = new WeakReference<>(null);
            }
        }
    }

    /** Loads {@code libnativeview} from {@code jniLibs} (idempotent via {@link NativeViewJNI}). */
    public static void initNativeLibrary() {
        NativeViewJNI.ensureLoaded();
    }

    /**
     * Reads {@code assets/nativeview.js} (built from {@code tools/js_build.py}, same bundle as
     * desktop).
     */
    public static String loadBridgeScript(Context context) throws IOException {
        if (context == null) {
            throw new IllegalArgumentException("context");
        }
        AssetManager assets = context.getApplicationContext().getAssets();
        try (InputStream in = assets.open("nativeview.js");
                ByteArrayOutputStream buf = new ByteArrayOutputStream()) {
            byte[] tmp = new byte[8192];
            int n;
            while ((n = in.read(tmp)) != -1) {
                buf.write(tmp, 0, n);
            }
            return buf.toString(StandardCharsets.UTF_8);
        }
    }

    /**
     * Sets the status bar background color (Android 5.0+). {@code hex} may be {@code #RGB}, {@code
     * #RRGGBB}, or {@code #AARRGGBB} (alpha is ignored for the bar paint; use {@code #FF...} for
     * opaque). No-op when there is no host activity, the activity is finishing, or {@code hex} is
     * null/blank.
     *
     * @throws IllegalArgumentException when {@code hex} is non-blank but not parseable
     */
    public static void setStatusBarColor(String hex) {
        if (hex == null || hex.trim().isEmpty()) {
            return;
        }
        final int color;
        try {
            color = parseArgbFromHex(hex);
        } catch (IllegalArgumentException ex) {
            throw ex;
        }
        runOnHostUi(
                activity -> {
                    Window w = activity.getWindow();
                    if (w == null) {
                        return;
                    }
                    w.addFlags(WindowManager.LayoutParams.FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS);
                    w.setStatusBarColor(color);
                });
    }

    /**
     * When {@code true}, hides status and navigation bars (immersive); when {@code false}, shows
     * them again. API level dependent; no-op without a host activity.
     */
    public static void setFullScreen(boolean enable) {
        runOnHostUi(
                activity -> {
                    Window window = activity.getWindow();
                    if (window == null) {
                        return;
                    }
                    View decor = window.getDecorView();
                    if (decor == null) {
                        return;
                    }
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                        WindowInsetsController c = window.getInsetsController();
                        if (c != null) {
                            int types = WindowInsets.Type.statusBars() | WindowInsets.Type.navigationBars();
                            if (enable) {
                                c.hide(types);
                                c.setSystemBarsBehavior(
                                        WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
                            } else {
                                c.show(types);
                            }
                        }
                        window.clearFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
                    } else {
                        applyLegacyFullScreen(decor, window, enable);
                    }
                });
    }

    @SuppressWarnings("deprecation")
    private static void applyLegacyFullScreen(View decor, Window window, boolean enable) {
        int vis = decor.getSystemUiVisibility();
        int mask =
                View.SYSTEM_UI_FLAG_FULLSCREEN
                        | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                        | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                        | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                        | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION;
        if (enable) {
            vis |= mask;
            window.addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
        } else {
            vis &= ~mask;
            window.clearFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
        }
        decor.setSystemUiVisibility(vis);
    }

    /** Sets {@link Activity#setRequestedOrientation(int)} using {@link Orientation}. */
    public static void setOrientation(Orientation orientation) {
        if (orientation == null) {
            return;
        }
        final int requested = orientation.asActivityInfoValue();
        runOnHostUi(activity -> activity.setRequestedOrientation(requested));
    }

    /** When {@code true}, keeps the screen on while the host window is shown. */
    public static void setKeepScreenOn(boolean enable) {
        runOnHostUi(
                activity -> {
                    Window w = activity.getWindow();
                    if (w == null) {
                        return;
                    }
                    if (enable) {
                        w.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
                    } else {
                        w.clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
                    }
                });
    }

    /** Hides the soft keyboard for the current IME focus in the host activity. */
    public static void hideKeyboard() {
        runOnHostUi(
                activity -> {
                    View v = activity.getCurrentFocus();
                    if (v == null) {
                        v = activity.findViewById(android.R.id.content);
                    }
                    if (v == null) {
                        return;
                    }
                    InputMethodManager imm =
                            (InputMethodManager) activity.getSystemService(Context.INPUT_METHOD_SERVICE);
                    if (imm == null) {
                        return;
                    }
                    imm.hideSoftInputFromWindow(v.getWindowToken(), 0);
                });
    }

    /** Shows the soft keyboard for the focused view in the host activity, if any. */
    public static void showKeyboard() {
        runOnHostUi(
                activity -> {
                    View v = activity.getCurrentFocus();
                    if (v == null) {
                        v = activity.findViewById(android.R.id.content);
                    }
                    if (v == null) {
                        return;
                    }
                    v.requestFocus();
                    InputMethodManager imm =
                            (InputMethodManager) activity.getSystemService(Context.INPUT_METHOD_SERVICE);
                    if (imm == null) {
                        return;
                    }
                    imm.showSoftInput(v, InputMethodManager.SHOW_IMPLICIT);
                });
    }

    /**
     * Parses {@code #RGB}, {@code #RRGGBB}, or {@code #AARRGGBB} into a packed ARGB int. Package
     * visibility for unit tests.
     */
    static int parseArgbFromHex(String hex) {
        if (hex == null) {
            throw new IllegalArgumentException("hex is null");
        }
        String s = hex.trim();
        if (s.startsWith("#")) {
            s = s.substring(1);
        }
        if (s.length() == 3) {
            char r = s.charAt(0);
            char g = s.charAt(1);
            char b = s.charAt(2);
            s = new String(new char[] {r, r, g, g, b, b});
        }
        if (s.length() != 6 && s.length() != 8) {
            throw new IllegalArgumentException("hex must be #RGB, #RRGGBB, or #AARRGGBB");
        }
        for (int i = 0; i < s.length(); i++) {
            char c = s.charAt(i);
            boolean hexDigit =
                    (c >= '0' && c <= '9')
                            || (c >= 'a' && c <= 'f')
                            || (c >= 'A' && c <= 'F');
            if (!hexDigit) {
                throw new IllegalArgumentException("invalid hex digit");
            }
        }
        try {
            long v = Long.parseLong(s, 16);
            if (s.length() == 6) {
                return 0xFF000000 | (int) v;
            }
            return (int) v;
        } catch (NumberFormatException ex) {
            throw new IllegalArgumentException("invalid hex", ex);
        }
    }

    private interface HostAction {
        void run(Activity activity);
    }

    private static Activity peekHost() {
        synchronized (HOST_LOCK) {
            Activity a = uiHostRef.get();
            if (a == null || a.isFinishing() || a.isDestroyed()) {
                return null;
            }
            return a;
        }
    }

    private static void runOnHostUi(HostAction action) {
        Activity activity = peekHost();
        if (activity == null || action == null) {
            return;
        }
        Runnable r =
                () -> {
                    Activity a = peekHost();
                    if (a == null) {
                        return;
                    }
                    action.run(a);
                };
        if (Looper.myLooper() == Looper.getMainLooper()) {
            r.run();
        } else {
            new Handler(Looper.getMainLooper()).post(r);
        }
    }
}
