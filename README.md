# nativeview

[![CI](https://github.com/GroupMerhab/nativeView/actions/workflows/ci.yml/badge.svg)](https://github.com/GroupMerhab/nativeView/actions/workflows/ci.yml)

A cross-platform C library for embedding native WebViews in applications.

## Features

- **Cross-Platform**: macOS (WKWebView), Windows (WebView2), and Linux (WebKitGTK) are supported; **Android** (System WebView) and **iOS** (WKWebView) are **in development**.
- **Multi-Window Support**: Create and manage multiple windows with a global registry and ID-based routing.
- **Bidirectional IPC**: Efficient JSON-based messaging between native and web layers.
- **IPC Bus**: Built-in message bus for window-to-window and broadcast communication.
- **HTTP Server (nv-http)**: Single-threaded and blocking by design; intended for simple local/dev use (e.g. serving static assets), not high-concurrency production workloads.
- **Minimal Dependencies**: Pure C with platform-native APIs.
- **Memory Efficient**: Arena-based allocator, zero-copy where possible.
- **Single File Distribution**: Optional amalgamation build.

## Platforms

| Platform | Backend | Status |
|----------|---------|--------|
| macOS | WKWebView | **Supported** |
| Windows | WebView2 | **Supported** |
| Linux | WebKitGTK | **Supported** |
| Android | System WebView | **In Development** |
| iOS | WKWebView | **In Development** |

## Building

See [docs/getting-started.md](docs/getting-started.md) for full instructions.

```bash
./tools/fetch_vue.sh   # Required for nv-ui examples
mkdir build && cd build
cmake ..
cmake --build .
./counter_ui           # Run nv-ui example
```

### Run Tests

```bash
ctest --test-dir build --output-on-failure
```

## Example Usage

```c
#include <nv.h>

int main(void) {
    nv_app_t* app = nv_app_create();

    nv_window_cfg_t cfg = {
        .title = "My App",
        .width = 800,
        .height = 600,
        .resizable = 1
    };

    nv_window_t* win = nv_window_create(app, &cfg);
    nv_window_show(win);

    nv_load_url(win, "https://example.com");

    nv_app_run(app);
    nv_app_destroy(app);
    return 0;
}
```

## Architecture

See [docs/architecture.md](docs/architecture.md) for detailed design documentation.

## Module Overview

| Module | Role |
|--------|------|
| `nv-core` | Arena, JSON, strings, logging, utilities |
| `nv-ipc` | JSON-RPC messaging, JS script dispatch |
| `nv-ops` | Native capability handlers (fs, dialog, clipboard, …) |
| `nv-runtime` | App/window lifecycle, window manager |
| `nv-platform-*` | WKWebView, WebView2, WebKitGTK, Android, iOS |
| `nv-ui` | Optional reactive UI (Vue-owned DOM) |
| `nv-http` | Optional embedded HTTP server (local/dev) |
| `nv-sdk` | Meta-package linking the full stack |

Public API: [`include/nv.h`](include/nv.h). Full layout: [docs/project_structure.md](docs/project_structure.md).

## Language bindings

Overview: **[docs/bindings.md](docs/bindings.md)** · Index: **[bindings/README.md](bindings/README.md)**

| Language | Path | Documentation |
|----------|------|----------------|
| Java (desktop) | `bindings/java` | [docs/Java.md](docs/Java.md) |
| Python | `bindings/python` | [docs/Python.md](docs/Python.md) |
| Nim | `bindings/nim` | [docs/Nim.md](docs/Nim.md) |
| C# | `bindings/csharp` | [docs/CSharp.md](docs/CSharp.md) |
| Zig | `bindings/zig` | [docs/Zig.md](docs/Zig.md) |
| Pascal | `bindings/pascal` | [docs/Pascal.md](docs/Pascal.md) |
| Rust | `bindings/rust` | [docs/Rust.md](docs/Rust.md) |
| Swift (desktop) | `bindings/swift` | [docs/Swift.md](docs/Swift.md) |
| Android | `bindings/android` | [docs/Android.md](docs/Android.md) |
| iOS | `bindings/ios` | [docs/iOS.md](docs/iOS.md) |

All docs: [docs/index.md](docs/index.md).

## Android library (`bindings/android/`)

**What it is:** Gradle **library** (`com.nativeview`) — **JNI** + **System WebView**, **`NativeViewActivity`** host, **`assets/nativeview.js`**, and built-in Java bridge ops (`device`, `storage`, `network`, …) aligned with the same JSON wire as desktop.

**What it is *not*:** The desktop **`bindings/java`** JNI binding (`io.jamharah.nativeview`), not a cross-platform replacement for every desktop C op, and not a portable substitute for **`nv.h`** (Android helpers such as **`NativeViewAndroid`** are mobile-only).

**Quick start:** (1) Android SDK + NDK (match **`ndkVersion`** in **`bindings/android/build.gradle`**). (2) `include ':nativeview'` in Gradle. (3) `implementation project(':nativeview')`. (4) Subclass **`NativeViewActivity`**, implement **`getContentUrl()`**. (5) Forward permission/activity results to **`NativeViewApp`**.

**Docs:** [docs/Android.md](docs/Android.md), [docs/Android-migration.md](docs/Android-migration.md).

**Sample:** Use **[examples/android_full_bridge/](examples/android_full_bridge/)** (supported). The in-tree **`bindings/android/Example/`** is currently broken — see its README.

```java
public final class MainActivity extends NativeViewActivity {
    @Override protected String getContentUrl() {
        return "file:///android_asset/index.html";
    }
}
```

## Development

- Source file line limits enforced at CMake configure (see [CONTRIBUTING.md](CONTRIBUTING.md))
- Opaque handle pattern for all public types
- Apache 2.0 license — see [LICENSE](LICENSE) and [NOTICE](NOTICE)

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md). Contributors must agree to the [CLA](CLA.md). Please read our [Code of Conduct](CODE_OF_CONDUCT.md) and [Security Policy](SECURITY.md).

## License

Apache 2.0 — See [LICENSE](LICENSE).

This project is open source under the Apache License 2.0, which grants all rights needed for personal and commercial use, including modification and distribution. Optional commercial support or indemnification offerings may be provided separately in the future; they do not restrict your rights under Apache 2.0.
