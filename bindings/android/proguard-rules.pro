# nativeview Android library — merged into consuming apps via consumerProguardFiles.

# --- JNI (names must match nv_android_nativeview_jni.c / JNI_OnLoad) ---
-keepclasseswithmembernames class com.nativeview.jni.NativeViewJNI {
    native <methods>;
}

# IsInstanceOf + C→Java WebView dispatch: dispatch(int, long, String, String)
-keep public interface com.nativeview.jni.NativeViewHost {
    void dispatch(int, long, java.lang.String, java.lang.String);
}

# GetFieldID in nativeCreateWindow — field names and descriptors must stay stable.
-keepclassmembers class com.nativeview.NativeViewConfig {
    public java.lang.String title;
    public int width;
    public int height;
    public boolean resizable;
    public boolean devtools;
}

# --- JavascriptInterface (WebView addJavascriptInterface name is _NVBridge) ---
-keep class com.nativeview.bridge.NVBridge {
    @android.webkit.JavascriptInterface <methods>;
}

# --- JNI GetMethodID callbacks (nv_android_jni_hooks.c) ---
-keepclassmembers class * implements com.nativeview.MessageCallback {
    void onMessage(java.lang.String, java.lang.String);
}
-keepclassmembers class * implements com.nativeview.ReadyCallback {
    void onReady();
}
-keepclassmembers class * implements com.nativeview.CloseCallback {
    void onClose();
}

# Bridge handler completion (not JNI; keeps lambda synthetic method names stable under R8).
-keepclassmembers class * implements com.nativeview.ResolveCallback {
    void resolve(java.lang.String);
}
-keepclassmembers class * implements com.nativeview.RejectCallback {
    void reject(java.lang.String, java.lang.String);
}

# Optional host activity subclass hooks for consumers.
-keep public class com.nativeview.NativeViewActivity {
    public <methods>;
    protected <methods>;
}
