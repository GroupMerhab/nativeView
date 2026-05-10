# Hello NativeView Example

A minimal cross‑platform example using only the public `nv.h` API. It creates an 800×600 window, loads inline HTML, and wires a simple counter with C↔JS messaging.

## Build

Assumes you already generated the build directory with CMake.

- macOS
  - Prerequisites: Xcode command line tools
  - Build:
    ```
    cd build
    cmake --build . --target hello --config Release
    ```
  - Run:
    ```
    ./hello
    ```

- Windows (WebView2)
  - Prerequisites: Visual Studio 2022, CMake, Microsoft Edge WebView2 Runtime
  - Generate and build:
    ```
    cd build
    cmake -G "Visual Studio 17 2022" -A x64 ..
    cmake --build . --target hello --config Release
    ```
  - Run:
    ```
    .\\Release\\hello.exe
    ```

- Linux (WebKitGTK)
  - Prerequisites: CMake, a compiler toolchain, and WebKitGTK development files
    - Debian/Ubuntu (example):
      ```
      sudo apt update
      sudo apt install libwebkit2gtk-4.1-dev cmake build-essential pkg-config
      ```
  - Build:
    ```
    cd build
    cmake --build . --target hello --config Release
    ```
  - Run:
    ```
    ./hello
    ```

## Expected Behavior

- A window titled “Hello NativeView” opens at 800×600.
- The page shows a large counter starting at 0 and two buttons:
  - Increment → sends `window.__nv.send("counter", JSON.stringify({action:"increment"}))`
  - Reset → sends `window.__nv.send("counter", JSON.stringify({action:"reset"}))`
- The native side maintains an integer counter and responds via:
  - `nv_send(window, "counter_update", "{\"value\":<n>}")`
- The page updates the display on `window.__nv.on("counter_update", d => display.textContent = d.value)`.
- “window ready” prints once the webview finishes loading.

## Troubleshooting

- Windows: WebView2 not found
  - Install the Microsoft Edge WebView2 Runtime (Evergreen). Ensure it is available system‑wide.
  - If building with MSVC, make sure the WebView2 SDK is discoverable by the build system.
  - If the app opens a blank/native window only, verify the runtime is installed and up to date.

- Linux: Missing WebKitGTK
  - Install WebKitGTK development headers (e.g., `libwebkit2gtk-4.1-dev`). Verify `pkg-config --modversion webkit2gtk-4.1` reports a version.
  - On distributions that ship only `webkit2gtk-4.0`, ensure the CMake config matches the available version or install the 4.1 package if provided.

- macOS: Blank content or build issues
  - Ensure Xcode Command Line Tools are installed (`xcode-select --install`).
  - If the app launches but shows no content, check Console logs for WebKit errors and re-run from a terminal to capture stdout/stderr.
