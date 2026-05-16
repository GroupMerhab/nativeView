# Bindings overview — nativeview

nativeview ships multiple ways to call the same C API (`include/nv.h`) and, on mobile, the JSON JavaScript bridge.

Folder index: [`bindings/README.md`](../bindings/README.md).

---

## Desktop bindings (direct C API)

These wrap **`nv_app_*`**, **`nv_window_*`**, **`nv_load_*`**, **`nv_send`**, hotkeys, etc. You build nativeview with CMake, then link or load the native library from your language runtime.

| Language | Location | Link model | Guide |
|----------|----------|------------|-------|
| **C** | `include/nv.h` | Static or shared | [getting-started.md](getting-started.md) |
| **Java** | `bindings/java` | JNI + `libnativeview` shared | [Java.md](Java.md) |
| **Python** | `bindings/python` | ctypes + shared lib | [Python.md](Python.md) |
| **Nim** | `bindings/nim` | Static archives (default) or `-d:nativeviewShared` | [Nim.md](Nim.md) |
| **C#** | `bindings/csharp` | Static archives (default) or `NATIVEVIEW_SHARED` | [CSharp.md](CSharp.md) |
| **Zig** | `bindings/zig` | Static archive chain via `zig build` | [Zig.md](Zig.md) |
| **Pascal** | `bindings/pascal` | FPC `external` + static/DLL | [Pascal.md](Pascal.md) |
| **Rust** | `bindings/rust` | `link-shared` / `link-static` / `link-none` features | [Rust.md](Rust.md) |
| **Swift** | `bindings/swift` | SwiftPM + static link script | [Swift.md](Swift.md) |

### Choosing a desktop binding

- **Python** — fastest scripting experiments; requires shared `libnativeview`.
- **Java** — JVM apps on desktop; needs `-DNV_BUILD_JAVA_JNI=ON`; macOS requires `-XstartOnFirstThread`.
- **Nim / C# / Zig / Pascal / Rust** — native binaries with static linking patterns documented per language.
- **Swift (desktop)** — SwiftPM package for macOS/Linux C interop; separate from iOS UIKit package.

---

## Mobile bindings (WebView host + bridge)

Mobile bindings add **activity/view-controller lifecycle**, **permissions**, and **Java/Swift bridge ops** (`device.*`, `storage.*`, …). They still use the same **`nativeview.js`** wire format as desktop where applicable.

| Platform | Location | Package / module | Guide |
|----------|----------|------------------|-------|
| **Android** | `bindings/android` | `com.nativeview` | [Android.md](Android.md) |
| **iOS** | `bindings/ios` | `NativeViewIOS` | [iOS.md](iOS.md) |

### Android vs desktop Java

| | Desktop Java | Android |
|---|--------------|---------|
| Artifact | `bindings/java` JNI | `bindings/android` AAR |
| Entry | `NativeView` + `java.library.path` | `NativeViewActivity` + Gradle |
| Doc | [Java.md](Java.md) | [Android.md](Android.md) |
| Migration | — | [Android-migration.md](Android-migration.md) |

### iOS vs desktop Swift

| | Desktop Swift | iOS |
|---|---------------|-----|
| Path | `bindings/swift` | `bindings/ios` |
| UI | Your app + C API | `NativeViewIOS` + WKWebView |
| Native link | Static archives / dylib | `NativeView.xcframework` |
| Doc | [Swift.md](Swift.md) | [iOS.md](iOS.md) |

---

## JavaScript bridge (all platforms with WebView)

Inside the WebView, pages use **`NativeView.invoke`** / **`NativeView.on`** (built from `js/src/`).

| Platform | Reference |
|----------|-----------|
| Desktop | [js-bridge.md](js-bridge.md) |
| Android | [Android.md](Android.md) (Java ops + wire) |
| iOS | [iOS.md](iOS.md) (Swift ops + wire) |

TypeScript types: `js/types/nativeview.d.ts`.

---

## Shared test harness

**`bindings/parity-tests/`** — JVM tests (`WireContractParityTest`) compiled into:

- `bindings/android` unit tests
- `bindings/java` Maven tests

Ensures JSON wire envelopes stay aligned across Android and desktop Java. Not an embedder-facing API.

---

## Examples by language

| Language | Path |
|----------|------|
| C | `examples/hello/` |
| nv-ui (C + Vue) | `examples/counter_ui/` |
| Java | `examples/todo_app/java_todo/` |
| Python | `examples/python/`, `examples/todo_app/py_todo/` |
| Nim | `examples/nim/`, `examples/todo_app/nim_todo/` |
| C# | `examples/csharp/` (minimal; experimental) |
| C# todo | `examples/todo_app/csharp_todo/` (**not working yet** — see [CSharp.md](CSharp.md#known-limitations)) |
| Zig | `examples/zig/`, `examples/todo_app/zig_todo/` |
| Pascal | `examples/pascal/` |
| Rust | `examples/todo_app/rust_todo/` |
| Swift (desktop) | `examples/swift/`, `examples/todo_app/swift_todo/` |
| Android | `examples/android_full_bridge/` |
| iOS | `bindings/ios/Example/` |
| Multi-language todo | `examples/todo_app/README.md` |

---

## Build prerequisites (summary)

1. Configure nativeview: `cmake -S . -B build` (see [getting-started.md](getting-started.md)).
2. Pick shared vs static per binding doc above.
3. For mobile: install SDK/NDK (Android) or Xcode + iOS SDK (iOS).

Packaging details: [packaging.md](packaging.md).
