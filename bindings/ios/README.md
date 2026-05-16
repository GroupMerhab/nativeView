# NativeViewIOS

Swift package and CocoaPods scaffold for embedding **nativeview** on iOS (WKWebView, JSON bridge, ops). Layout matches the intended module split (App, Window, Bridge, Ops, Helpers, Resources, Tests, Example).

## Layout

- **`Sources/NativeViewIOS/`** — App host, window/WebView, bridge routing, ops namespaces, bundled **`Resources/nativeview.js`**, Swift C helpers in **`Bridge/NVCBridge.swift`** (`import NativeView`).
- **`Headers/`** — **`NativeViewC.h`** (`#include "nv.h"` + **`nv_util.h`**) and **`module.modulemap`** (Clang module **`NativeView`**), copied into **`NativeView.xcframework`** slices.
- **`NativeView.xcframework`** — Static **`libnativeview.a`** per platform (device + fat simulator) plus full public headers. Generate with **`scripts/create_nativeview_xcframework.sh`** (real libs from **`libs/`**) or **`scripts/create_nativeview_xcframework.sh --stub`** for a minimal link-only archive.
- **`libs/`** — Per-slice merged **`libnativeview.a`**; see **`libs/README.md`**. Populate with **`scripts/build_ios_static_libs.sh`**.
- **`Tests/NativeViewIOSTests/`** — XCTest targets (`BridgeTests`, `OpsTests`, `PermissionTests`, `ParityTests`).
- **`Example/`** — UIKit sample **`ExampleApp.xcodeproj`** with local Swift package reference **`..`** (this directory).

## Requirements

- iOS **14+**, Swift **5.7+**
- Build and run the example or tests from **Xcode** with an **iOS Simulator** or device destination (`swift build` on a macOS host alone is not sufficient because the library target imports **UIKit**).

### Swift Package Manager (`Package.swift`)

1. Produce **`NativeView.xcframework`** at the package root (next to **`Package.swift`**):

   ```bash
   ./scripts/create_nativeview_xcframework.sh --stub
   ```

   For the real CMake stack (recommended for shipping):

   ```bash
   ./scripts/build_ios_static_libs.sh
   ./scripts/create_nativeview_xcframework.sh
   ```

2. The binary target is **`NativeView`** (path **`NativeView.xcframework`**). Swift code uses **`import NativeView`** for C symbols and **`NVCBridge`** for small Swift wrappers.

### CocoaPods (`NativeViewIOS.podspec`)

The podspec uses **`vendored_frameworks = 'NativeView.xcframework'`**. Build the xcframework first (same scripts as above).

### Verify C core for all requested slices

From the repository root, this runs three independent Xcode CMake configures (device arm64, simulator x86_64, simulator arm64):

```bash
bindings/ios/scripts/verify_ios_cmake_archs.sh
```

## iOS-only UI shell (`NativeViewIOS`)

// iOS only — Swift/UIKit helpers in **`Sources/NativeViewIOS/Helpers/NativeViewIOS.swift`**. These APIs are **not** in `include/nv.h`, the portable **`Nativeview`** Swift package (`bindings/swift/`), JNI, or the cross-platform **`NativeView`** JavaScript surface (`js/`). Use them from your app delegate or view controllers when you need UIKit-specific behavior (status bar style, orientation mask, idle-timer / keep-awake, navigation interactive-pop override, keyboard dismissal, and toggling whether the embedded **`WKWebView`** respects the safe area).

## `nativeview.js`

There is no checked-in **`nativeview.js`** under **`bindings/swift/`** (desktop Swift is C headers only). This binding ships the same built bundle as **`bindings/android/src/main/assets/nativeview.js`**. After **`tools/js_build.py`**, copy the output into **`Sources/NativeViewIOS/Resources/nativeview.js`** so all bindings stay identical (`cmp` against the Android asset).

## Example app

Open **`Example/ExampleApp.xcodeproj`**, select an iOS Simulator, **⌘B**. The **`NativeViewIOS`** product is resolved from the parent directory’s **`Package.swift`**.

**Bundled `file://` pages:** ``NativeViewWindow/loadUrl(_:)`` uses **`WKWebView.loadFileURL(_:allowingReadAccessTo:)`** so the WebContent process can read **`index.html`** and sibling resources (plain **`load(URLRequest:)`** can yield a blank page and `hasAssumedReadAccessToURL` / “no access” in the log). The hosted view is visible by default; call ``NativeViewWindow/hide()`` / ``NativeViewWindow/show()`` if you need to defer visibility.

## Tests

Run **`NativeViewIOSTests`** from Xcode (Package tests) or **`xcodebuild test -scheme NativeViewIOS -destination 'platform=iOS Simulator,…'`**; command-line **`swift test`** on macOS alone cannot import **UIKit** for this package.

### Device / OS matrix (manual QA before release)

| Target | Notes |
|--------|--------|
| iPhone Simulator (x86_64) | e.g. Rosetta / older simulator runtimes when available |
| iPhone Simulator (arm64) | Default on Apple Silicon Macs |
| Physical iPhone (arm64) | Push permissions, pasteboard, biometrics, camera |
| iPad Simulator | Layout + multitasking |
| Physical iPad | Same as iPhone + larger safe area |
| iOS 14 | **`Package.swift`** minimum; smoke bridge + default ops |
| iOS 17+ (or latest Xcode SDK) | Regression on current APIs |

### Release gates

- **`ParityTests`**: every default bridge method key matches **`bindings/android/src/test/java/com/nativeview/ops/DefaultAndroidBridgeMethodKeysTest.java`**; wire helpers mirror **`WireContractParityTest`**. Desktop **`bindings/swift`** remains C-only; mobile JSON bridge parity is shared with Android/Java.
- **`AppStoreComplianceTests`**: no `exit()`, no `UIWebView`, Example **`INFOPLIST_KEY_*UsageDescription`** entries for sensitive capabilities.

## Legacy stub script

**`scripts/build_stub_xcframework.sh`** forwards to **`create_nativeview_xcframework.sh --stub`**. Older **`CNativeview.xcframework`** references are replaced by **`NativeView.xcframework`**.
