# Todo app (Android + nativeview)

Android port of [../nim_todo](../nim_todo) / [../java_todo](../java_todo): same SQLite schema and Vue `__nv` bridge, using the [bindings/android](../../../bindings/android) library (`NativeViewApp`, `NativeViewWebView`, JNI).

## Prerequisites

- **JDK 17–21** (or set **`JAVA_HOME`** to such a JDK) for Gradle and the Android Gradle Plugin. Running Gradle on **JDK 22+** is not supported for **`./gradlew`**; use **17–21**.
- Android SDK (**`ANDROID_HOME`**) with platform **android-34** and build-tools.
- Android NDK (see `bindings/android/build.gradle` `ndkVersion`) when building **`libnativeview.so`** locally.
- **NDK** installed in Android Studio (same **major** as **`ndkVersion`** in `bindings/android/build.gradle`); Gradle builds **`libnativeview.so`** from the repo root **CMake** when you build/run. Default ABIs are **arm64-v8a** and **x86_64** (emulators). **Device-only (arm64):** set `nativeviewJniAbis=arm64-v8a` in `gradle.properties` or pass `-PnativeviewJniAbis=arm64-v8a`. Optional manual **`.so`** copy: [build_nativeview_so.sh](../../../bindings/android/scripts/build_nativeview_so.sh).

- **Node.js** + **npm** on your **`PATH`** (for **`npm ci`** / **`npm run build`** in [../ui](../ui)). Gradle runs this on every **`preBuild`** unless you skip it (below).

## UI asset

Gradle **`:app`** **`preBuild`** depends on **`prepareTodoAppUiAssets`**, which runs **`npm ci && npm run build`** in [../ui](../ui) and copies **`dist/index.html`** to **`app/src/main/assets/todo_ui.html`**.

**Skip npm** (use checked-in fallback HTML): add **`-PskipTodoUiBuild`** to Gradle, or set **`NV_TODO_SKIP_UI_BUILD=ON`** (or **`1`**), same as [prep_assets.sh](prep_assets.sh).

Optional manual step (asset only, no full Android build):

```bash
chmod +x prep_assets.sh
./prep_assets.sh
```

- **Full UI**: same as Gradle — `npm install` + `npm run build` in [../ui](../ui), copies `dist/index.html` to `app/src/main/assets/todo_ui.html`.
- **Skip npm**: `NV_TODO_SKIP_UI_BUILD=ON ./prep_assets.sh` copies [../ui/fallback_index.html](../ui/fallback_index.html).

A small fallback **`todo_ui.html`** may already be present so the tree is buildable before the first UI run when npm is skipped.

## Build APK

```bash
chmod +x gradlew
./gradlew :app:assembleDebug
```

Install the generated APK from `app/build/outputs/apk/debug/`.

## Tests

```bash
./gradlew :app:testDebugUnitTest
```

Unit tests also run **`preBuild`**, so **Node/npm** are required unless you pass **`-PskipTodoUiBuild`** (fallback **`todo_ui.html`**).

## Database

SQLite file: **`todo_app.db`** in the app’s private databases directory (same basename as other todo ports).

## If the app crashes on launch

1. **Missing native library** — Logcat often shows `UnsatisfiedLinkError` / `libnativeview.so not found`. Install the **NDK** (SDK Manager) matching **`ndkVersion`** in **`bindings/android`**, then **Rebuild Project**. Default packaging includes **arm64-v8a** and **x86_64**; **emulators almost always need `x86_64`**. For **physical arm64 only**, use **`nativeviewJniAbis=arm64-v8a`** in **`gradle.properties`**. Offline/script build: **`bindings/android/scripts/build_nativeview_so.sh`**.
2. **`npm` / `sh` not found during Gradle** — Install **Node.js**, ensure **`npm`** is on **`PATH`**, or build with **`-PskipTodoUiBuild`** / **`NV_TODO_SKIP_UI_BUILD=ON`** to use the fallback HTML.
3. After a failed native load, the todo sample shows a **Toast** with a short hint instead of dying inside the JNI class initializer.
