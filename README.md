# nativeview

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

## Android library (`bindings/android/`)

**What it is:** Gradle **library** (`com.nativeview`) — **JNI** + **System WebView**, **`NativeViewActivity`** host, **`assets/nativeview.js`**, and built-in Java bridge ops (`device`, `storage`, `network`, …) aligned with the same JSON wire as desktop.

**What it is *not*:** The desktop **`bindings/java`** JNI binding (`io.jamharah.nativeview`), not a cross-platform replacement for every desktop C op, and not a portable substitute for **`nv.h`** (Android helpers such as **`NativeViewAndroid`** are mobile-only).

**Quick start:** (1) Open the app in Android Studio with the **Android SDK** and an **NDK** installed (match **`ndkVersion`** in **`bindings/android/build.gradle`**). Gradle builds **`libnativeview.so`** from the repo root **CMake** when you build the **`:nativeview`** library. Optional: copy **`.so`** files with **`bindings/android/scripts/build_nativeview_so.sh`** (`ANDROID_NDK_HOME` required). (2) `include ':nativeview'` in Gradle with `project(':nativeview').projectDir = file('…/bindings/android')`. (3) `implementation project(':nativeview')`. (4) Subclass **`NativeViewActivity`** and implement **`getContentUrl()`**. (5) Declare runtime permissions your UI uses; forward **`onRequestPermissionsResult`** / **`onActivityResult`** to **`NativeViewApp`**.

**Docs:** **[docs/Android.md](docs/Android.md)** (API + ops), **[docs/Android-migration.md](docs/Android-migration.md)** (from desktop Java). **Sample:** **[examples/android_full_bridge/](examples/android_full_bridge/)**.

```java
public final class MainActivity extends NativeViewActivity {
    @Override protected String getContentUrl() {
        return "file:///android_asset/index.html";
    }
}
```

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
ctest
```

## Example Usage

```c
#include <nv.h>

int main(void) {
    // 1. Create the application instance
    nv_app_t* app = nv_app_create();

    // 2. Configure the main window
    nv_window_cfg_t cfg = {
        .title = "My App",
        .width = 800,
        .height = 600,
        .resizable = 1
    };

    // 3. Create and show the window
    nv_window_t* win = nv_window_create(app, &cfg);
    nv_window_show(win);

    // 4. Load content
    nv_load_url(win, "https://example.com");
    // Or load local HTML: nv_load_html(win, "<h1>Hello</h1>");

    // 5. Run the event loop
    nv_app_run(app);

    // 6. Cleanup
    nv_app_destroy(app);
    return 0;
}
```

## Architecture

See [docs/architecture.md](docs/architecture.md) for detailed design documentation.

## Module Overview

- **nv_core**: Library initialization and app loop.
- **nv_window**: Window configuration and lifecycle.
- **nv_window_manager**: Registry for multi-window management.
- **nv_ipc**: JSON-RPC messaging and event bus.
- **nv_ops**: Modular operation handlers (FileSystem, Dialog, Clipboard, etc.).
- **nv_http**: Embedded HTTP server (single-threaded, blocking). Useful for lightweight scenarios; not intended for production concurrency.
- **nv_arena**: Fast, safe memory allocation.
- **nv_json**: Strict JSON parsing and serialization.

## Development

- All source files: < 800 lines
- All headers: < 300 lines
- Opaque handle pattern for all public types
- Apache 2.0 license

## License

Apache 2.0 - See [LICENSE](LICENSE) file.

### Open Source & Commercial Use
This project is open source under the Apache 2.0 license, allowing free use for personal and commercial applications. 

A commercial license option with dedicated support and indemnification may be available in the future for enterprise needs.
