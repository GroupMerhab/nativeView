# Language bindings — nativeview

This directory contains **language-specific wrappers** and **mobile embedding libraries** around the C API in [`include/nv.h`](../include/nv.h).

**Start here:** [docs/bindings.md](../docs/bindings.md) — overview of every binding, desktop vs mobile, and examples.

---

## Quick map

| Path | Platform | Type | Documentation |
|------|----------|------|----------------|
| [`java/`](java/) | Desktop (JVM) | JNI | [docs/Java.md](../docs/Java.md) |
| [`python/`](python/) | Desktop | ctypes | [docs/Python.md](../docs/Python.md) |
| [`nim/`](nim/) | Desktop | `importc` FFI | [docs/Nim.md](../docs/Nim.md) |
| [`csharp/`](csharp/) | Desktop | P/Invoke | [docs/CSharp.md](../docs/CSharp.md) |
| [`zig/`](zig/) | Desktop | Zig module | [docs/Zig.md](../docs/Zig.md) |
| [`pascal/`](pascal/) | Desktop | FPC unit | [docs/Pascal.md](../docs/Pascal.md) |
| [`rust/`](rust/) | Desktop | `extern "C"` crate | [docs/Rust.md](../docs/Rust.md) |
| [`swift/`](swift/) | Desktop (macOS/Linux) | SwiftPM C interop | [docs/Swift.md](../docs/Swift.md) |
| [`android/`](android/) | Android | Gradle AAR + JNI | [docs/Android.md](../docs/Android.md) |
| [`ios/`](ios/) | iOS | SwiftPM + xcframework | [docs/iOS.md](../docs/iOS.md) |

---

## Not a language binding

| Path | Purpose |
|------|---------|
| [`parity-tests/`](parity-tests/) | Shared JVM wire-contract tests for Android + desktop Java |
| [`nv_link_shim.c`](nv_link_shim.c) | Anchor TU for the `nativeview_shared` CMake target |

---

## Desktop vs mobile

```text
Desktop (nv.h + WebView on macOS / Windows / Linux)
  java, python, nim, csharp, zig, pascal, rust, swift

Mobile (platform WebView + JSON bridge)
  android  → com.nativeview (Java/Kotlin)
  ios      → NativeViewIOS (Swift/UIKit)
```

- **Desktop Swift** (`bindings/swift`) — C headers only; link `libnativeview` yourself.
- **iOS** (`bindings/ios`) — UIKit host, `NativeView.xcframework`, bundled `nativeview.js`, Java-style bridge ops in Swift.

Do not confuse `bindings/java` (desktop JNI) with `bindings/android` (mobile library).

---

## Examples

| Binding | Example |
|---------|---------|
| C | [examples/hello/](../examples/hello/) |
| Java | [examples/todo_app/java_todo/](../examples/todo_app/java_todo/) |
| Python | [examples/python/](../examples/python/), [examples/todo_app/py_todo/](../examples/todo_app/py_todo/) |
| Nim | [examples/nim/](../examples/nim/), [examples/todo_app/nim_todo/](../examples/todo_app/nim_todo/) |
| C# | [examples/csharp/](../examples/csharp/) |
| Zig | [examples/zig/](../examples/zig/), [examples/todo_app/zig_todo/](../examples/todo_app/zig_todo/) |
| Pascal | [examples/pascal/](../examples/pascal/) |
| Rust | [examples/todo_app/rust_todo/](../examples/todo_app/rust_todo/) |
| Swift (desktop) | [examples/swift/](../examples/swift/), [examples/todo_app/swift_todo/](../examples/todo_app/swift_todo/) |
| Android | [examples/android_full_bridge/](../examples/android_full_bridge/) |
| iOS | [bindings/ios/Example/](../bindings/ios/Example/) |

---

## Building native libraries

All bindings ultimately need a CMake-built nativeview stack. See [docs/getting-started.md](../docs/getting-started.md) and [docs/packaging.md](../docs/packaging.md).

| Binding style | Typical CMake flag |
|---------------|-------------------|
| Shared (`python`, `java` JNI) | `-DNV_BUILD_SHARED=ON` |
| Static (Nim, Zig, Pascal, desktop Swift, Rust `link-static`) | default static archives |
| Android / iOS | Platform modules + Gradle / Xcode scripts in each binding |
