# Changelog — nativeview Android (`bindings/android`)

All notable changes to the Android library module are documented here. Version numbers follow **`bindings/java/pom.xml`** (`io.jamharah:nativeview`) for parity with the desktop JNI binding.

## 0.1.0 — 2026-05-15

### Added
- **`packageNativeviewRelease`** Gradle task: writes `build/distributions/nativeview-android-<version>/nativeview-android.jar` (compiled library bytecode) and `jni/<abi>/libnativeview.so` extracted from the release **AAR** (native code is built via **`externalNativeBuild`** / root **CMake** `nativeview_shared`, not from checked-in `jniLibs/`).
- **`bindings/android/scripts/package_release.sh`**: clean Android build, package release layout, `mvn test` under `bindings/java/` (requires SDK; NDK required unless `libnativeview.so` is already present for both ABIs).
- **`gradle.properties`**: `android.useAndroidX=true` (required for AndroidX dependencies).
- Library **`versionName` / `versionCode`** in `defaultConfig`, aligned with **0.1.0** desktop artifact.

### Changed
- **16 KB page-size / Play:** **`libnativeview.so`** and **`libnv-platform-android.so`** use **16 KB ELF** link flags; **`android:extractNativeLibs="false"`** in the library manifest for correct **APK** mmap alignment with **AGP 8.5.2** **`useLegacyPackaging = false`**.
- **Gradle wrapper** updated to **8.14.3** (run Gradle on **JDK 17–21** when the host JDK is newer than Groovy’s ASM supports for buildscript classpath).
- **ProGuard / R8 consumer rules** expanded for JNI, `JavascriptInterface`, JNI-driven callbacks, `NativeViewConfig` fields, and `ResolveCallback` / `RejectCallback`.

### Fixed
- **`FileOps`**: missing `NativeViewApp` import (release compile).
- **`BridgeOriginTest`**: missing class terminator; **Robolectric** for `Uri` in JVM unit tests.
- **`ActivityResultRouterTest`**, **`NVPermissionManagerTest`**, **`NVBridgeTest`**, **`BridgeJavaScriptTest`**: Robolectric / Mockito 5 / `WebView.post` test harness fixes.

### Verification (maintainer checklist)
- Clean build: `./gradlew clean assembleRelease` (with `ANDROID_HOME`, `JAVA_HOME` for Gradle).
- Unit tests: `./gradlew test`.
- Instrumented: `./gradlew connectedAndroidTest` with **`libnativeview.so`** installed per ABI (device or emulator).
- Wire parity: shared **`WireContractParityTest`** + **`NativeViewJniOpcodes`** tests; desktop **`mvn test`** under **`bindings/java/`**.

### Git tag (after commit)
Match desktop Maven version **0.1.0**:
```bash
git tag -a v0.1.0 -m "nativeview 0.1.0 — Android library + Java binding parity"
```
