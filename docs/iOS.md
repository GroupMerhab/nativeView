# iOS embedding — nativeview

Swift/UIKit library under **`bindings/ios/`** (`NativeViewIOS`): **WKWebView**, **`NativeView.xcframework`**, JSON bridge, and Swift ops aligned with the Android wire contract.

**Not** the desktop SwiftPM package in **`bindings/swift/`** (C-only interop). See [Swift.md](Swift.md) for macOS/Linux desktop Swift.

---

## What it is

- **SwiftPM library** `NativeViewIOS` — app host, `NativeViewWebView`, bridge routing, ops namespaces, bundled **`Resources/nativeview.js`**.
- **C module `NativeView`** — static **`NativeView.xcframework`** built from the repo CMake iOS toolchain (`nv-platform-ios`).
- **Default bridge ops** — register with `NativeViewApp.registerDefaultIOSHandlers()` (`device`, `storage`, `network`, `clipboard`, …).
- **UIKit helpers** — `NativeViewIOS` (status bar, orientation, safe area) in Swift only; not in `nv.h`.

## What it is not

- A replacement for every desktop C op on day one (gaps are platform-specific).
- The same artifact as **`bindings/swift`** (no UIKit; headers + link flags only).
- Runnable with `swift build` on macOS alone — the library imports **UIKit**; use Xcode + iOS Simulator or device.

---

## Requirements

- iOS **14+**, Swift **5.7+**
- Xcode with iOS SDK
- Python 3 (to run `tools/js_build.py` when refreshing `nativeview.js`)

---

## Quick start

### 1. Build `NativeView.xcframework`

From **`bindings/ios/`**:

```bash
# Stub (link-only smoke):
./scripts/create_nativeview_xcframework.sh --stub

# Production (CMake static libs per slice):
./scripts/build_ios_static_libs.sh
./scripts/create_nativeview_xcframework.sh
```

Per-slice archives also live under **`bindings/ios/libs/`** — see **`bindings/ios/libs/README.md`**.

Verify CMake arch configs from repo root:

```bash
bindings/ios/scripts/verify_ios_cmake_archs.sh
```

### 2. Add the Swift package

In your app’s `Package.swift` or Xcode project, depend on the local package at **`bindings/ios/`**. The binary target path is **`NativeView.xcframework`** next to **`Package.swift`**.

```swift
import NativeView      // C API
import NativeViewIOS   // UIKit host + bridge
```

### 3. Host a WebView

Use **`NativeViewApp`**, **`NativeViewWindow`**, and **`NativeViewViewController`** (see **`bindings/ios/Sources/NativeViewIOS/`**).

Call **`registerDefaultIOSHandlers()`** before loading content.

### 4. Keep `nativeview.js` in sync

After `python3 tools/js_build.py` at repo root, copy output to:

```text
bindings/ios/Sources/NativeViewIOS/Resources/nativeview.js
```

Match **`bindings/android/src/main/assets/nativeview.js`** (`cmp` the files).

### 5. Example app

Open **`bindings/ios/Example/ExampleApp.xcodeproj`**, select an iOS Simulator, build and run.

**Bundled `file://` pages:** use `loadFileURL(_:allowingReadAccessTo:)` so WKWebView can read sibling assets (see **`bindings/ios/README.md`**).

---

## CocoaPods

**`NativeViewIOS.podspec`** uses `vendored_frameworks = 'NativeView.xcframework'`. Build the xcframework first (same scripts as above).

---

## JavaScript bridge

Same client API as desktop after handshake: `NativeView.invoke`, `NativeView.on`, etc.

- Desktop op reference: [js-bridge.md](js-bridge.md)
- Android (closest mobile cousin): [Android.md](Android.md)

---

## Tests

**`Tests/NativeViewIOSTests/`** — run from Xcode or:

```bash
xcodebuild test -scheme NativeViewIOS -destination 'platform=iOS Simulator,name=iPhone 16'
```

Includes **`ParityTests`** (wire keys vs Android), **`BridgeTests`**, **`OpsTests`**, **`PermissionTests`**, **`AppStoreComplianceTests`**.

Shared JVM parity sources: **`bindings/parity-tests/`** (used by Android + desktop Java, not iOS directly).

---

## Layout reference

| Path | Role |
|------|------|
| `Sources/NativeViewIOS/` | Swift library sources |
| `Headers/NativeViewC.h` | C umbrella for xcframework |
| `NativeView.xcframework` | Prebuilt or generated C framework |
| `scripts/` | xcframework and static lib build scripts |
| `Example/` | UIKit sample Xcode project |

More detail: [**`bindings/ios/README.md`**](../bindings/ios/README.md).

---

## Related docs

- [bindings.md](bindings.md) — all language bindings
- [Android.md](Android.md) — Android embedding
- [Android-migration.md](Android-migration.md) — desktop Java → Android (conceptual parallel for iOS: new host APIs)
- [mobile_platforms_plan.md](mobile_platforms_plan.md) — roadmap
