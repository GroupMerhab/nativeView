# NativeView Mega Demo

## What is This

The NativeView Mega Demo is a comprehensive integration test suite and showcase for the `nativeview` library. It demonstrates all available operations across the API surface (Window, App, Filesystem, Dialog, Clipboard, Shell, Screen, Notification, Tray, IPC) in a single, interactive application.

It serves two main purposes:
1.  **Verification**: Automated and manual testing of native bridge functionality on macOS, Windows, and Linux.
2.  **Reference**: Example implementation of every API call, showing how to structure requests and handle responses.

The demo uses a shared core (`demo_core.js`) to provide a consistent UI, test runner, and reporting mechanism without external dependencies.

## Building

### Prerequisites
- CMake 3.15+
- C compiler (Clang/GCC/MSVC)
- **macOS**: Xcode Command Line Tools
- **Windows**: Visual Studio 2019+ or MinGW
- **Linux**: GTK3 and WebKit2GTK dev packages (`libgtk-3-dev`, `libwebkit2gtk-4.0-dev`)

### Build Steps

1.  **Create a build directory**:
    ```bash
    mkdir build && cd build
    ```

2.  **Configure the project**:
    ```bash
    cmake ..
    ```

3.  **Build the executable**:
    ```bash
    cmake --build . --target nv_demo
    ```

## Running

After building, the executable will be located in the `build` directory (or `build/Debug` on Windows).

**macOS / Linux**:
```bash
./build/nv_demo
```

**Windows**:
```cmd
.\build\Debug\nv_demo.exe
```

The application should launch a window displaying the demo interface. If the bridge is working correctly, the status bar will show "Connected: YES" in green.

## Tabs Overview

- **Window**: Tests window management operations like resizing, moving, maximizing, minimizing, and fullscreen toggling. verify window state persistence and event handling.
- **Multi-Window**: Demonstrates creating new windows, listing active windows, and broadcasting messages between them.
- **App**: Displays application metadata (version, name, paths) and handles lifecycle events like quitting.
- **Filesystem**: Demonstrates reading/writing files, listing directories, and checking file stats. Includes safety checks for path traversal.
- **Dialog**: Triggers native system dialogs for opening files, saving files, and displaying messages (alert/confirm).
- **Clipboard**: Tests reading and writing text to the system clipboard.
- **Shell**: Executes external commands and opens URLs in the default browser. **Note**: Shell execution requires the `NV_ALLOW_SHELL_EXEC` flag.
- **Screen**: Enumerates available displays and retrieves their properties (resolution, scale factor, work area).
- **Notification**: Sends system notifications with titles, bodies, and optional actions.
- **Tray**: Manages a system tray icon and menu. **Note**: Behavior varies significantly by OS.
- **IPC Bench**: Benchmarks the performance of the IPC bridge, measuring latency, throughput, and concurrency.

## Run All Tests

To run the full suite:
1.  Press `Shift+R` (or click "Run All" if available).
2.  The runner will cycle through each tab, executing all automated operations.
3.  Watch the status bar for `Total PASS` and `Total FAIL` counts.
4.  Inspect the "Live IPC Log" (press `L`) for detailed request/response traces.

**Interpreting Results**:
- **PASS**: The operation completed successfully and returned expected data.
- **FAIL**: The operation threw an error or returned invalid data.
- **PENDING**: The operation has not yet been run.

## Keyboard Shortcuts

| Key | Action |
| :--- | :--- |
| `1-9`, `0` | Switch to tab 1-10 |
| `R` | Run all ops on the **current** tab |
| `Shift+R` | Run all ops on **all** tabs (sequential) |
| `L` | Toggle the IPC Log panel |
| `C` | Clear results on the current tab |
| `F5` | Reload the current tab (re-runs auto-ops) |
| `Esc` | Close any open modal or hint bar |
| `?` | Show the keyboard shortcut help modal |

## Known Limitations

- **Tray**: May not appear on some Linux desktop environments (e.g., GNOME without extensions) or requires specific system configuration.
- **Notifications**: macOS and Windows may require the application to be signed or added to a notification allowlist. Linux requires a notification daemon.
- **Shell**: `shell.exec` commands will fail with `ERR_PERMISSION` unless the library is initialized with `NV_ALLOW_SHELL_EXEC`.
- **Dialog**: Modal dialogs block the UI thread; use with caution in performance-critical flows.
