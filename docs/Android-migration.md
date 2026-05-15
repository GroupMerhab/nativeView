# Migrating from desktop Java to Android

Full context: **`docs/Android.md`**.

## Dependency (one line)

```gradle
implementation project(':nativeview')
```

Replace your desktop **`implementation files('…/nativeview-java.jar')`** (or Maven coordinate for **`bindings/java`**) with the **Android library** (**`bindings/android`** as a Gradle subproject or published AAR). Desktop still needs **`nativeview_jni`** on **`java.library.path`**; Android loads **`libnativeview.so`** built by Gradle **CMake** (or pre-copied under **`jniLibs`** via **`bindings/android/scripts/build_nativeview_so.sh`**).

## Entry point (conceptual one-liner)

```java
// Desktop
NativeView.loadLibrary();
NvApp app = NativeView.nvAppCreate();

// Android
NativeViewApp app = new NativeViewApp(context);
```

Wire calls from JavaScript stay **`NativeView.invoke(...)`**; Android registers Java ops and forwards the rest to **native IPC** (including **`app.handshake`**).
