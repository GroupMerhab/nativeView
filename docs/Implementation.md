# Implementation Plan for nativeview

## Overview
nativeview is a cross-platform C11 library for embedding native WebViews into applications, with a bidirectional JavaScript ↔ native bridge and an optional reactive UI layer (nv-ui) that renders widgets into a WebView-managed DOM.

### Goals
- Provide a small, fast, dependency-minimal embedding layer for macOS (WKWebView), Windows (WebView2), and Linux (WebKitGTK).
- Provide a stable C API (`include/nv.h` and friends) with NULL-safe public functions.
- Provide a robust bridge for request/response and event-style IPC between C and JavaScript.
- Provide modular “ops” (fs, dialog, clipboard, shell, tray, screen, notification, window, windows, ipc-bus) that are easy to extend.
- Provide an optional UI layer (nv-ui) where Vue owns DOM updates and C sends minimal state diffs/patches.

### Non-Goals
- A full web framework or SPA runtime beyond the minimal bootstrap needed for the bridge.
- A third-party C dependency graph (prefer platform APIs and in-tree modules).
- A platform abstraction layer that leaks `#ifdef`s across the codebase (platform code stays in `modules/nv-platform-*/`).

## Feature Analysis
### Identified Features
- Window and app lifecycle: create app instance, create/show windows, run loop, quit.
- WebView embedding backends: platform-specific host for loading URL/HTML and evaluating JS.
- IPC and routing: JSON-based request/response, event dispatch, message bus (window-to-window).
- Ops: native capabilities exposed to JS and/or app code (fs/dialog/clipboard/etc.).
- Multi-window manager: registry + ID routing.
- Optional UI layer (nv-ui): widget tree, reactive state → DOM diff patch, Vue-owned DOM.
- Tooling: JS build/minify, Vue bundling/embedding, amalgamation support.
- Tests and benchmarks: unit tests per module and performance probes (IPC/shm).

### Feature Categorization
- Must-Have
  - Stable C API for app/window lifecycle and WebView control
  - Cross-platform backends (macOS/WKWebView first, Windows/WebView2 and Linux/WebKitGTK parity)
  - Bridge correctness: versioning, routing, timeouts, error surfaces
  - Ops correctness and security boundaries (file access, shell invocation, dialogs)
  - Test coverage for public APIs and core invariants
- Should-Have
  - nv-ui reactive widget layer with minimal diffs and predictable event routing
  - IPC bus for window-to-window messaging and broadcast
  - Hybrid IPC fast paths where supported (e.g., shared memory / SharedArrayBuffer integration)
  - Packaging ergonomics (pkg-config, install targets, amalgamation)
- Nice-to-Have
  - nv-designer tool (pure C output, round-trip merge)
  - CI matrix builds and cross-platform runtime verification
  - Expanded documentation set (widget reference, architecture guide, binary-size tracking)

## Recommended Tech Stack
### Native (Core)
- Language: C11
- Build: CMake
- Platform APIs
  - macOS: WKWebView (WebKit)
  - Windows: WebView2
  - Linux: WebKitGTK

### JavaScript (Bridge + Optional UI)
- Runtime: WebView JS environment (no Node dependency)
- Bridge code: plain JS modules assembled from `js/src/`
- Optional UI layer: Vue 3 (embedded bundle for offline/production builds)

### Testing
- C tests: per-module executables registered with CTest
- JS tests: node-based integration tests where present (e.g., `js/tests/`)
- **Java / Android bindings**
  - **Unit (JVM):** `bindings/android/` — `./gradlew test` (use **JDK 17–21** for the Gradle daemon; Android Gradle Plugin **8.2.x** with wrapper **8.14.3**); Robolectric for `WebView` / permission flows; shared wire contract tests under `bindings/parity-tests/src/test/java` (also compiled into the Android `test` source set).
  - **Release flat bundle:** `./gradlew packageNativeviewRelease` → `build/distributions/nativeview-android-<version>/` (`nativeview-android.jar` + `jni/<abi>/libnativeview.so`); full verification script **`bindings/android/scripts/package_release.sh`**; version notes **`bindings/android/CHANGELOG.md`** (version aligned with **`bindings/java/pom.xml`**).
  - **Instrumented (device/emulator):** `bindings/android/` — `./gradlew connectedAndroidTest` with native **`libnativeview.so`** built per ABI by Gradle **CMake**; covers JNI init, default bridge registration, `NVBridge.post` → `deliverWebMessage`, and lifecycle.
  - **Desktop Java:** `bindings/java/` — `mvn test` runs `WireContractParityTest` plus `NativeViewLoadTest` (assumption-skipped when JNI native libraries are absent).
  - **API parity:** identical `io.jamharah.nativeview.parity.WireContractParityTest` sources on both bindings; JNI opcode alignment via `NativeViewJniOpcodes` (unit tests avoid loading `NativeViewJNI` until native is needed).
  - **Device matrix (manual / CI labels):** Android **7.0 (API 24)** and **14 (API 34)**; **x86_64** emulator and **arm64-v8a** hardware — run `connectedAndroidTest` on each combination you ship.
  - **Android embedding docs:** **`docs/Android.md`**, **`docs/Android-migration.md`**, sample app **`examples/android_full_bridge/`**; binding-local example **`bindings/android/Example/`** (Gradle **`app`** → parent **`:nativeview`**) — **known broken** (**`app.handshake`** timeouts / bridge not ready; documented in **`bindings/android/Example/README.md`**; use **`examples/android_full_bridge/`** until fixed); **`bindings/android/build.gradle`** forces **`externalNativeBuild`** (non-Clean) tasks on each assemble and **`preBuild`** runs **`tools/js_build.py`** → **`assets/nativeview.js`** unless **`nativeview.skipJsBundle=true`**. WebView **`deliverWebMessage`** falls through to **`nativeDispatchWebMessage`** when no Java handler matches so **`app.handshake`** and other C-side ops run; **`NativeViewJNI.nativeBindDispatchHost`** complements **`nativeInit`** when JNI **`FindClass(NativeViewHost)`** fails so C→WebView **`evaluateJavascript`** stays wired.
- **iOS (Swift):** **`bindings/ios/`** — SwiftPM library **`NativeViewIOS`** + XCTest targets under **`Tests/NativeViewIOSTests/`**; C interop via binary **`NativeView.xcframework`** (module **`NativeView`**, umbrella **`Headers/NativeViewC.h`**, build with **`bindings/ios/scripts/create_nativeview_xcframework.sh`** or **`--stub`**); merged per-slice **`libnativeview.a`** under **`bindings/ios/libs/`** from **`bindings/ios/scripts/build_ios_static_libs.sh`**; architecture smoke script **`bindings/ios/scripts/verify_ios_cmake_archs.sh`**; UIKit example **`bindings/ios/Example/ExampleApp.xcodeproj`** (local package **`..`**). **`nativeview.js`** is kept byte-identical to **`bindings/android/src/main/assets/nativeview.js`**. **`NativeViewApp`** registers **`UIApplication`** notifications plus **`UIScene`** (iOS 13+) for **`app.resume` / `app.pause` / `app.destroy`** (resume/pause coalesced when both fire), **`UIDevice.orientationDidChangeNotification`** → **`device.orientation`**, and **`handleDeepLink(url:)`** → **`NativeView._emit('app.deeplink', { url })`** on all windows; **`NativeViewViewController`** lays out the WebView against **`safeAreaLayoutGuide`** or full-bleed per **`NativeViewIOS`** (iOS-only Swift helpers — **not** in **`nv.h`**, JNI, or portable **`NativeView`** JS). Default bridge ops: **`NativeViewApp.registerDefaultIOSHandlers()`** (see **`IOSDefaultBridge`** + `Sources/NativeViewIOS/Ops/*`).
  - **iOS tests:** **`Tests/NativeViewIOSTests/`** — unit (`BridgeTests`, `PermissionTests`, `OpsTests`), integration (`BridgeIntegrationTests`, `AppLifecycleTests`), **`ParityTests`** (default bridge keys aligned with **`DefaultAndroidBridgeMethodKeysTest.java`** + wire envelope parity with **`WireContractParityTest`**), **`AppStoreComplianceTests`** (no `exit()`, no `UIWebView`, Example **`INFOPLIST_KEY_*`** privacy strings for camera/photo/location/Face ID/contacts/mic). Prefer **`xcodebuild test -scheme NativeViewIOS`** with an **iOS Simulator** or device destination (SwiftPM **`swift test`** alone cannot link **UIKit**).
  - **iOS device matrix (manual / CI):** **iPhone** simulator (**x86_64** and **arm64** slices where available), **physical iPhone (arm64)**, **iPad** simulator and **physical iPad**, **iOS 14** (package minimum in **`Package.swift`**) and a **current iOS** train (e.g. **17+**) before release.

## Implementation Stages

### Stage 1: Foundation & Build System
**Dependencies:** None
**Effort:** Medium

#### Sub-steps
- [X] Confirm and document module dependency DAG and boundaries (no cycles, strict layering)
- [X] Standardize compiler flags and warning cleanliness across platforms (Clang/GCC/MSVC)
- [X] Ensure install targets cover headers, pkg-config, and library artifacts cleanly
- [X] Ensure NV_BUILD_UI and NV_BUILD_TESTS options remain consistent and documented
- [X] Optional shared library: `-DNV_BUILD_SHARED=ON` builds `nativeview_shared` (`nativeview.dll` / `libnativeview.so` / `libnativeview.dylib`) with `NV_SHARED_BUILD` / consumer `NV_SHARED` via `NV_API` in `include/util/nv_log.h`; static consumers keep using the `nativeview` CMake INTERFACE target. Pascal: import unit `bindings/pascal/nativeview.pas`, guide **`docs/Pascal.md`**, sample **`examples/pascal/`** (thin `.lpr` + app unit; **`examples/pascal/build_example.sh`** builds **`nv_minimal_gui`** on macOS via a small Clang `main` + FPC dylib—see **`examples/pascal/README.md`**). Nim: import module **`bindings/nim/nativeview.nim`**, guide **`docs/Nim.md`**, sample **`examples/nim/`** with **`examples/nim/build_static.sh`** / **`build_static.ps1`**, and template **`bindings/nim/config.static.nims.example`**. Zig: FFI module **`bindings/zig/nativeview.zig`**, guide **`docs/Zig.md`**, sample **`examples/zig/`** with **`examples/zig/build_static.sh`** / **`build_static.ps1`**. Swift: SwiftPM package **`bindings/swift/`** (`CNativeview` + **`Nativeview`**), guide **`docs/Swift.md`**, sample **`examples/swift/`** with **`examples/swift/build_static.sh`**. Python: ctypes package **`bindings/python/nativeview/`**, guide **`docs/Python.md`**, sample **`examples/python/`** with **`examples/python/build_shared.sh`** / **`build_shared.ps1`**. Java: JNI package **`bindings/java/`** (`io.jamharah.nativeview`), guide **`docs/Java.md`**, CMake target **`nativeview_jni`** when **`-DNV_BUILD_JAVA_JNI=ON`** (requires **`NV_BUILD_SHARED=ON`**). Android library **`bindings/android/`**: **`com.nativeview.jni.NativeViewJNI`** + **`com.nativeview.jni.NativeViewHost`** over **`libnativeview.so`** (build with **`bindings/android/scripts/build_nativeview_so.sh`**, **`arm64-v8a`** / **`x86_64`**); call **`NativeViewApp.registerDefaultAndroidHandlers()`** after construction to register Java bridge ops (`device.getInfo`, `storage.*`, `network.getStatus` / `network.onChange` → `network.change`, `notification.*`, `app.*`, `camera.*`, `location.*`, `fs.*`, `clipboard.*`, `biometric.*`) and forward **`Activity#onActivityResult`** to **`NativeViewApp.onActivityResult`**. WebView bridge security: default **`NativeViewApp#allow_origin`** is **`file://`** only (other origins **`PERMISSION_DENIED`** until **`allow_origin`**); validated wire JSON (`WireJson.parseWireObject`), **`BridgeArgs`** / **`SandboxPath`** for **`fs.*`**. **`NativeViewActivity`** (subclass **`getContentUrl()`**) wires **`WebView#onPause`/`onResume`**, **`saveState`/`restoreState`**, push **`app.resume` / `app.pause` / `app.destroy` / `app.deeplink`** (`{url}`) / **`device.orientation`** (`{landscape}`), back (**history** then **`on_close`** → **`finish()`**); **`NativeViewAndroid`** + **`Orientation`** are Android-only Java (status bar color, immersive fullscreen, **`setRequestedOrientation`**, keep screen on, IME); they are **not** in **`nv.h`** or the portable JS **`NativeView`** API—use **`attachHostActivity`/`detachHostActivity`** (wired by **`NativeViewActivity`**). Consumer manifest: **`VIEW` intent-filter** + **`singleTask`/`singleTop`** for deep links, optional **`android:configChanges`** for in-place rotation. **`BridgeJavaScript.emitEvent`** emits **`{e,d}`** wire for **`NativeView.on`**. Full Vue+SQLite todo sample (Zig): **`examples/todo_app/zig_todo/`** (see **`examples/todo_app/zig_todo/README.md`**).

### Stage 2: Core Runtime, Platform Backends, and IPC Correctness
**Dependencies:** Stage 1 completion
**Effort:** High

#### Sub-steps
- [X] Define and enforce JS↔C wire versioning and compatibility rules (mismatch errors, upgrade path)
- [ ] Harden IPC request/response semantics (timeouts, cancellation strategy, error normalization)
- [X] Validate multi-window routing invariants (registry correctness, lifecycle edge cases)
- [ ] Align macOS/Windows/Linux backend feature parity for:
  - window creation/show/close lifecycle
  - navigation/load URL and local file loading behavior
  - JS evaluation behavior and error reporting
- [X] Add targeted integration tests that exercise the full stack (platform backend + IPC + a representative op)

### Stage 3: Ops Surface (Capability Modules)
**Dependencies:** Stage 2 completion
**Effort:** High

#### Sub-steps
- [ ] Specify a consistent “op contract” shape: input validation, error codes, payload schema
- [ ] Audit each op for:
  - argument validation and NULL safety
  - permission boundaries (filesystem roots, shell command execution, dialogs)
  - platform parity and explicit NOT_SUPPORTED handling
  - [X] **P-00**: Notification, screen (`getAll` / `getPrimary` / `getCursor`), and similar ops no longer fake success or emit fabricated events where not implemented; they reply with `ERR_NOT_IMPLEMENTED` until platform hooks exist. Clipboard image: implemented under **P-20**. (`shell.exec` is implemented under **P-05**.) Tray: macOS **P-13**; other OSes use stub hooks (see `nv_op_tray.c`) until Win/Linux tray backends exist.
  - [X] **P-13**: macOS tray (`tray.create` / `destroy` / `setIcon` / `setTooltip` / `setMenu`) via `NSStatusItem` in `nv_mac.m`: status button image/tooltip, click → `nv_send(..., "tray.clicked", ...)`, `NSMenu` items → `tray.menuAction`. Ops live in `nv_op_tray.c` (JSON `items` with `label`, numeric `id`, optional `separator`). Non-mac builds use weak tray hook stubs in `nv_op_tray.c` so `test_op_tray.c` can override with strong mocks (MSVC uses in-file stubs returning failure; tests expect `ERR_IO`).
  - [X] **P-04**: Linux `shell.openUrl`, `shell.openPath`, and `shell.showInFolder` use `gtk_show_uri_on_window` with `g_app_info_launch_default_for_uri` fallback; `showInFolder` opens the parent directory `file://` URI. Ops dispatch in `nv_op_shell.c` via the non-macOS/non-Windows `#else` branch to `nv_linux_shell_*` in `nv_linux.c` (weak symbols; tests override with stubs).
  - [X] **P-05**: `shell.exec` on macOS, Windows, and Linux: platform hooks `nv_*_shell_exec` return a heap `nv_shell_result_t` (stdout/stderr capped at 1 MiB each, `truncated` flag); `nv_op_shell_exec` keeps the metacharacter guard unless `NV_ALLOW_SHELL_EXEC`, copies into the IPC arena via `nv_arena_str_dup`, and replies with `{ exitCode, stdout, stderr, truncated? }`. Tests in `test_op_shell.c` cover validation, metachar rejection, and `echo hello`.
  - [X] **P-01**: Windows clipboard text (`nv_win_clipboard_*` in `nv_win.c`, `nv_op_clipboard.c` `_WIN32` branches); `test_op_clipboard_win` exercises op replies (GCC/Clang stubs override weak symbols; MSVC uses live clipboard).
  - [X] **P-02**: Linux clipboard text (`nv_linux_clipboard_*` in `nv_linux.c` via GTK `GDK_SELECTION_CLIPBOARD`, `nv_op_clipboard.c` Linux branches); `test_op_clipboard_linux` exercises op replies (GCC/Clang stubs override weak symbols).
  - [X] **P-20**: Clipboard image on all platforms (`nv_mac_clipboard_*` / `nv_win_clipboard_*` + `nv_win_clipboard_image.cpp` GDI+ / `nv_linux_clipboard_*`), wire `{ width, height, format: "png", data }` / write `{ format, data }`, `nv_base64` in `nv-core` with `test_base64.c`, mock hooks in `test_op_clipboard.c` (mac stubs + Win/Linux weak overrides).
  - [X] **P-21**: Application menu bar: public `include/nv_menu.h` (`nv_app_set_menu`), op `app.setMenu` (`nv_op_app_menu.c`), macOS `NSMenu` in `nv_mac_menu.m` → `app.menuAction`, Linux `GtkMenuBar` builder in `nv_linux.c` (replaces hardcoded Edit/Debug), Windows `ERR_NOT_SUPPORTED` and MSVC no-op menu hook symbols in `nv_menu.c`. Tests: `test_op_app_menu.c`. JS: `NativeView.app.setMenu` in `js/src/modules/app.js`.
  - [X] **P-22**: Per-window WebView context menu: op `window.setContextMenu` with `{ "items": [ { "id", "label", "disabled"? } ] }` (items stored on `nv_window_t` pool arena; boolean `disabled` mirrors `app.setMenu`), event `window.contextMenuAction` with `{ "id" }`; platform hooks `nv_*_window_set_context_menu` in `nv_mac.m`, `nv_win.c` (WebView2 `ContextMenuRequested` + `TrackPopupMenu`), `nv_linux.c` (WebKit context-menu signal + `GSimpleActionGroup`); weak fallbacks in `nv-runtime/src/nv_window_context_menu.c`. Env `NV_WEBVIEW_CONTEXT_MENU` still controls the default menu when no custom items are set. Tests: `test_op_window_menu.c`.
  - [X] **P-23**: Global hotkeys: public `include/nv_hotkey.h` (`nv_window_register_hotkey` / `nv_window_unregister_hotkey`), ops `window.registerHotkey` / `window.unregisterHotkey`, event `window.hotkeyFired` with `{ "id" }`; platform hooks `nv_{mac,win,linux}_register_hotkey` / `unregister` (Carbon `RegisterEventHotKey`, Win32 `RegisterHotKey`/`WM_HOTKEY`, X11 `XGrabKey` + root `gdk_window_add_filter`; Wayland returns `ERR_NOT_SUPPORTED`). Tests: `test_op_hotkey.c`. JS: `NativeView.window.registerHotkey` in `js/src/modules/window.js`.
  - [X] **P-08**: Windows `app.getDataDir` / `app.getExePath` / `app.getResourceDir` / `app.getLocale` via `nv_win_get_*` in `nv_win.c` (`SHGetFolderPathW` + `%APPDATA%\\nativeview`, `GetModuleFileNameW`, `GetUserDefaultLocaleName` → UTF-8 `malloc`); `nv_op_app.c` `_WIN32` branches; `test_op_app.c` exercises replies (MinGW/Clang stubs override weak `nv_win_get_*`; macOS exercises native path ops + default locale).
  - [X] **P-19**: `fs.watch` / `fs.unwatch` drive platform watchers (FSEvents in `nv_mac_fs_watch.m`, `ReadDirectoryChangesW` worker thread + `WM_NV_FS_CHANGE` in `nv_win_fs_watch.c`, inotify + `g_io_add_watch` in `nv_linux_fs_watch.c`). Ops and `nv_send(..., "fs.changed", …)` live in `nv_op_fs_watch.c` (`nv_fs_changed_emit`). Tests: `test_op_fs_watch.c` (mock hooks), `test_op_fs_watch_integration.c` (temp file, 500 ms).
- [ ] Expand per-op tests to cover common edge cases and error surfaces
- [X] Ensure the demo app exercises every op in at least one representative flow

### Stage 4: nv-ui (Reactive UI Layer) and Vue Slot
**Dependencies:** Stage 2 completion
**Effort:** High

#### Sub-steps
- [ ] Document the nv-ui contract:
  - widget geometry model (x/y/w/h)
  - state ownership (arena ownership, lifecycle)
  - flush/patch semantics and event routing
- [ ] Ensure widget inventory is complete and stable (button/label/input/checkbox/select/slider/textarea/vue slot)
- [ ] Implement and verify Vue slot lifecycle callbacks (mounted/unmounted) wired to C callbacks
- [ ] Add platform-agnostic nv-ui tests for diff generation and event routing invariants

### Stage 5: Hybrid IPC Fast Paths (Optional)
**Dependencies:** Stage 2 completion
**Effort:** Medium

#### Sub-steps
- [ ] Define the feature detection and fallback behavior (shm present vs IPC-only)
- [ ] Integrate SharedArrayBuffer / shared memory support in platform backends where available
- [ ] Update nv-ui widgets that can benefit from shm reads (e.g., slider telemetry) with a safe fallback
- [ ] Add benchmarks and regression thresholds (IPC latency vs shm write/read latency)

### Stage 6: Tooling and Distribution
**Dependencies:** Stage 1 completion
**Effort:** Medium

#### Sub-steps
- [X] Stabilize JS build pipeline (manifest assembly, minification, debug stripping rules)
- [X] Stabilize Vue embedding pipeline and document reproducible steps
- [ ] Validate amalgamation output (headers + sources) and ensure it builds in isolation
- [ ] Validate pkg-config correctness with an external consumer sample

### Stage 7: Cross-Platform Verification and CI
**Dependencies:** Stages 2–4 completion
**Effort:** Medium

#### Sub-steps
- [ ] Add CI matrix builds for macOS + Linux (and Windows where feasible)
- [ ] Add smoke tests that run a minimal app/window and validate a handshake message round-trip
- [ ] Record platform-specific build prerequisites and common failure modes

## Resource Links
- CMake Documentation: https://cmake.org/cmake/help/latest/
- Apple WKWebView (WebKit): https://developer.apple.com/documentation/webkit/wkwebview
- Microsoft WebView2: https://learn.microsoft.com/microsoft-edge/webview2/
- WebKitGTK: https://webkitgtk.org/reference/webkit2gtk/stable/
- Vue 3: https://vuejs.org/guide/introduction.html
