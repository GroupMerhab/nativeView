# NativeView Android — in-tree example

## Status: not working (known issues)

**This sample is currently broken and is shipped as-is for layout reference only.** Running the app commonly ends with **`app.handshake`** **timing out**, the JS client never reaching “bridge ready”, and follow-on **`NativeView.invoke`** calls failing. The wiring (WebView ↔ JNI ↔ native IPC) still needs debugging and fixes.

**Use a maintained sample instead:** **`examples/android_full_bridge/`** in this repo, which is the supported full-bridge Gradle app until this folder is repaired.

---

This folder is the **binding-local** sample app (parallel to **`bindings/ios/Example/ExampleApp.xcodeproj`**). It depends on the parent Gradle project at **`..`** (`:nativeview` library) via composite `settings.gradle`.

## Run

**Toolchain:** **JDK 17+**, Android SDK with **NDK** matching **`../build.gradle`** (`ndkVersion`), and **`python3`** on **`PATH`** (runs **`tools/js_build.py`** → **`../src/main/assets/nativeview.js`** on **`:nativeview` `preBuild`**). To skip the JS step (stale bundle risk): **`gradle.properties`** **`nativeview.skipJsBundle=true`**.

From this directory:

```bash
./gradlew :app:assembleDebug
./gradlew :app:installDebug
```

Open **`bindings/android/Example/`** in **Android Studio** as a project; pick an emulator or device and run **`app`**.

**Expect runtime failure** until the sample is fixed: **`app.handshake`** may time out even when **`assembleDebug`** succeeds.

## What it exercises

- **`NativeViewApp`** + **`NativeViewWindow`** + **`NativeViewWebView`** with the same lifecycle / **`NativeViewHost.dispatch`** wiring as **`examples/android_full_bridge/`** (uses **`AppCompatActivity`** for **`biometric.*`**).
- Bundled **`app/src/main/assets/index.html`** (`file:///android_asset/index.html`) plus overflow menu entries to reload bundled URL vs. **`load_html`** inline demo.
- Custom bridge handler **`example.echo`** (mirrors the Swift **`ExampleApp`** handler).
- **`nativeview-example://`** deep link → **`app.deeplink`** event ( **`singleTop`** + **`onNewIntent`** ).
- Default Java ops from **`registerDefaultAndroidHandlers()`** (storage, network, camera, …).

For a second full-sample tree under **`examples/`**, see **`examples/android_full_bridge/`**.
