# Android full bridge demo

Gradle application that **depends on the in-tree `bindings/android` library** and loads **`file:///android_asset/index.html`**. The page exercises every default Java bridge namespace (`device`, `app`, `storage`, `network`, `fs`, `clipboard`, `notification`, `location`, `camera`, `biometric`) and listens for **`network.change`**, **`location.update`**, **`app.resume` / `app.pause`**, and **`device.orientation`**.

## Prerequisites

- **JDK 17**
- **Android SDK** with **API 34** platform and **NDK r26+** matching **`ndkVersion`** in **`bindings/android/build.gradle`** (install via SDK Manager). Gradle builds **`libnativeview.so`** from the repo root **CMake** when you build this app.

Optional manual **`.so`** install (offline or CI):

```bash
export ANDROID_NDK_HOME=/path/to/ndk
bindings/android/scripts/build_nativeview_so.sh
```

That script copies **`libnativeview.so`** into **`bindings/android/src/main/jniLibs/<abi>/`**; Gradle normally does not need those checked-in files.

## Build & install

```bash
cd examples/android_full_bridge
./gradlew :app:installDebug
```

## Permissions

Runtime prompts are triggered when you tap buttons that need them (for example **network.getStatus**, **notification.show** on API 33+, **location.*** methods, **camera.takePicture**). **`MainActivity`** forwards **`onRequestPermissionsResult`** and **`onActivityResult`** to **`NativeViewApp`**.

**Biometric** uses **`BiometricPrompt`** and requires a **`FragmentActivity`** host — this demo uses **`AppCompatActivity`** (not plain **`NativeViewActivity`**, which extends **`Activity`**).

## Layout

| Path | Role |
|------|------|
| **`settings.gradle`** | Includes **`bindings/android`** as **`:nativeview`** |
| **`app/.../MainActivity.java`** | WebView + JNI window host (**`NativeViewHost`**) |
| **`app/src/main/assets/index.html`** | Demo UI + **`NativeView.invoke` / `NativeView.on`** |
