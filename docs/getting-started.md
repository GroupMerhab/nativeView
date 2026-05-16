# Getting Started — nativeview

This guide walks through building the library, running tests, and running your first example on macOS, Linux, and Windows.

For packaging and integration options, see [packaging.md](packaging.md). For architecture and IPC design, see [architecture.md](architecture.md).

---

## Prerequisites

| Platform | Requirements |
|----------|----------------|
| **macOS** | Xcode Command Line Tools (`xcode-select --install`) |
| **Linux** | CMake 3.16+, a C11 compiler, `pkg-config`, WebKitGTK 4.1 dev package |
| **Windows** | Visual Studio 2022 (or Build Tools), CMake 3.16+, [WebView2 Runtime](https://developer.microsoft.com/microsoft-edge/webview2/) |

### Linux (Debian/Ubuntu example)

```bash
sudo apt update
sudo apt install cmake build-essential pkg-config libwebkit2gtk-4.1-dev
```

Verify WebKitGTK:

```bash
pkg-config --modversion webkit2gtk-4.1
```

On distributions that only ship `webkit2gtk-4.0`, install the matching dev package and ensure CMake finds it.

### Windows

- Install the **Microsoft Edge WebView2 Runtime** (Evergreen) system-wide.
- Generate with: `cmake -G "Visual Studio 17 2022" -A x64 ..`

---

## Build the library

From the repository root:

```bash
cmake -S . -B build
cmake --build build
```

### Run tests

```bash
ctest --test-dir build --output-on-failure
```

Desktop builds register unit tests via CTest. Mobile-only targets are skipped on desktop.

### Optional: disable file-length check

By default, CMake fails configure if source files exceed repo line limits. To bypass locally:

```bash
cmake -S . -B build -DNV_ENFORCE_FILE_LIMITS=OFF
```

Do not disable this for contributions meant to be merged. See [CONTRIBUTING.md](../CONTRIBUTING.md).

### Optional: build without nv-ui (no Vue)

```bash
cmake -S . -B build -DNV_BUILD_UI=OFF
cmake --build build
```

---

## Run the `hello` example

The [hello](../examples/hello/) example uses only the public `nv.h` API: one window, inline HTML, and C↔JS messaging.

### macOS

```bash
cd build
cmake --build . --target hello --config Release
./hello
```

### Linux

```bash
cd build
cmake --build . --target hello
./hello
```

### Windows

```bash
cd build
cmake --build . --target hello --config Release
.\Release\hello.exe
```

### Expected behavior

- Window titled **Hello NativeView** at 800×600.
- Counter UI with Increment / Reset buttons.
- Native side responds with `counter_update` events; JS updates the display.

More detail and troubleshooting: [examples/hello/README.md](../examples/hello/README.md).

---

## Optional: nv-ui + Vue (`counter_ui`)

The reactive UI layer (`nv-ui`) embeds a Vue 3 bundle for offline builds.

1. Fetch Vue (once per clone):

   ```bash
   ./tools/fetch_vue.sh
   ```

2. Configure and build (default `NV_BUILD_UI=ON`):

   ```bash
   cmake -S . -B build
   cmake --build build --target counter_ui
   ./build/counter_ui
   ```

---

## Next steps

| Topic | Document |
|-------|----------|
| Project layout | [project_structure.md](project_structure.md) |
| Architecture & IPC | [architecture.md](architecture.md) |
| JavaScript bridge API | [js-bridge.md](js-bridge.md) |
| Language bindings | [bindings.md](bindings.md) |
| All docs | [index.md](index.md) |
| Contributing | [CONTRIBUTING.md](../CONTRIBUTING.md) |

---

## Troubleshooting

### Windows: WebView2 not found

- Install the WebView2 Evergreen Runtime.
- Ensure the WebView2 SDK is discoverable when building with MSVC.
- Blank window: verify the runtime is installed and up to date.

### Linux: Missing WebKitGTK

- Install `libwebkit2gtk-4.1-dev` (or your distro equivalent).
- Confirm `pkg-config --modversion webkit2gtk-4.1` prints a version.

### macOS: Blank content

- Install Xcode Command Line Tools.
- Run from a terminal and check Console for WebKit errors.
