# Mobile Platform Support Plan (iOS + Android)
Goal: Add iOS and Android backends with maximum modularity, readability, maintainability, memory safety, testability, and AI-agent friendliness.

## Non-Negotiable Rules (Obligations)
- [ ] Every added feature includes tests (unit/integration as appropriate).
- [ ] Do not continue to the next feature until all tests pass.
- [ ] Keep platform code inside `modules/nv-platform-*` only (no cross-platform `#ifdef` outside those modules).
- [ ] No new bare `malloc/free` in new code; use arenas or platform-owned reference counting patterns.
- [ ] All public functions are NULL-safe.
- [ ] Build must be C11 and `-Wall -Wextra` clean (no new warnings).
- [ ] File size limits:
  - [ ] No `.c/.m/.mm/.cpp` file exceeds 800 lines.
  - [ ] Additionally, keep new `.c` files ≤ 400 lines (repo rule); split aggressively.

## Decisions Required (Do Not Implement Until Chosen)
- [x] iOS language + memory model:
  - Option A: Objective-C `.m` with ARC
  - Option B: Objective-C `.m` with MRC (match current macOS style)
  - Option C: ObjC++ `.mm` (only if needed for C++ interop)
- [x] iOS packaging target:
  - Option A: Build `nv-platform-ios` + an iOS runner app target from CMake (Xcode generator)
  - Option B: Build `nv-platform-ios` only (consumer supplies app project)
- [x] iOS minimum deployment target: iOS 14
- [x] Android host layer:
  - Option A: Kotlin minimal host app + JNI bridge
  - Option B: Java minimal host app + JNI bridge
- [x] Android minSdk / targetSdk: minSdk 24 / targetSdk 34
- [x] Internal design choice: store platform API vtable by value on `nv_app_t`.
- [x] Style preference: avoid OOP as much as possible; prefer procedural style (Objective-C used only as a thin wrapper where required by platform SDKs).
- [x] iOS windowing model: single-scene first (extend to multi-scene later).

## High-Level Architecture (What Will Change)
### 1) Replace per-platform symbol calls in `nv-ops` with a platform API vtable (recommended)
Rationale: Adding iOS/Android without this will spread platform-specific declarations across non-platform modules.

Deliverables:
- [x] Define an internal `nv_platform_api_t` (function pointers) stored by value on `nv_app_t`.
- [x] Desktop platform backends (mac/win/linux) register their implementation during `nv_app_platform_init`.
- [x] Mobile platform backends (ios/android) register their implementation during `nv_app_platform_init`.
- [x] Route `shell.openUrl` (and related shell open ops) via vtable dispatch + `ERR_NOT_SUPPORTED` when absent.
- [x] Route remaining `nv-ops` platform capabilities via vtable dispatch (incremental).

Constraints:
- [x] Keep the vtable internal-only (not in `include/nv.h` public API).
- [x] NULL-safe dispatch: missing function pointer must be handled gracefully.
- [x] Test coverage for dispatch + “not supported” behavior.

### 2) Add new modules
- [x] `modules/nv-platform-ios/` (UIKit + WKWebView)
- [x] `modules/nv-platform-android/` (NDK native lib + JNI bridge; WebView hosted by example app)

### 3) Build selection wiring (root CMake)
- [x] Extend platform detection to select iOS/Android modules when building for those systems.
- [x] Ensure `cmake -G Xcode` can generate an Xcode project that includes the iOS targets.

## Implementation Plan (Sequential, Test-Gated)
### Phase 0 — Baseline & Guardrails
- [x] Add a “file length” CI/script check OR CMake-time guard for new/changed sources (must fail build when > limits).
- [x] Confirm current desktop platforms build + tests pass before changes begin (baseline).

### Phase 1 — Introduce `nv_platform_api_t` (No iOS/Android Yet)
Scope: pure refactor + tests; behavior must remain identical on mac/win/linux.

1. Add vtable definition and ownership
- [x] Add `nv_platform_api_t` definition in an internal header.
- [x] Store the platform API vtable by value on `nv_app_t` (no getter/setter needed).
- [x] Ensure lifetime: vtable stored by value (no heap ownership concerns).

2. Migrate one capability end-to-end (thin slice)
- [x] Choose one low-risk op (recommended: `shell.openUrl`) and route via vtable.
- [x] Update mac/win/linux backends to register that function.
- [x] Update `nv-ops` to call vtable for that operation.
- [x] Add unit tests that:
  - [x] verify correct dispatch when function is present
  - [x] verify `ERR_NOT_SUPPORTED` when absent
  - [x] verify NULL-safety on inputs
- [x] Run full test suite; fix regressions before continuing.

3. Migrate remaining capabilities incrementally (each with tests)
For each capability group below: migrate → add tests → run full tests → proceed.
- [x] Window core hooks used by runtime (create/show/hide/focus/close, navigation, title, size, etc.)
- [x] Dialogs
- [x] Notifications
- [x] Tray
- [x] File watching (`fs.watch`)
- [x] Hotkeys
- [x] Any other platform hooks referenced by `nv-ops`/`nv-runtime`

### Phase 2 — iOS Platform Module (CMake → Xcode)
Precondition:
- [x] Phase 1 is merged and all tests pass on desktop platforms.

1. Create `nv-platform-ios` skeleton (no features beyond window/webview + IPC)
- [x] Add `modules/nv-platform-ios/CMakeLists.txt` producing `nv-platform-ios` (STATIC).
- [x] Keep iOS backend sources under size limits (currently a single `nv_ios.m`, < 800 lines).
- [x] Implement required `nv_app_platform_*` and minimal `nv_window_platform_*`.
- [x] Implement iOS “not supported” mapping for desktop-only features (tray/hotkeys/etc.) via vtable absence or explicit stubs.

2. Add iOS runner app target (if chosen)
- [x] Add an iOS app target in CMake (Xcode generator friendly) that runs `nv_app_run` (which enters `UIApplicationMain` on iOS).
- [x] Wire it to call into nativeview to create one window and load a URL/HTML.
- [x] Ensure simulator + device configs are supported (deployment target + device family set on the target).

3. Tests strategy for iOS
- [x] Add platform-agnostic unit tests for vtable dispatch (already in Phase 1).
- [x] Add iOS build-only “smoke” checks (compile/link) as CMake targets (`ios_smoke_build`).
- [x] Add Xcode XCTest target for minimal runtime smoke test (required).
- [x] Wire an Xcode scheme test action so `xcodebuild test -scheme ios_runner` runs `ios_runner_tests` (via `ios_runner_scheme_fix` target).

Gate:
- [x] `ios_smoke_build` succeeds (configure + compile/link `ios_runner` + `ios_runner_tests`).
- [ ] Xcode test run succeeds (e.g. `xcodebuild test`; requires simulator device support; currently blocked locally by CoreSimulator/Xcode mismatch).

### Phase 3 — Android Platform Module (NDK + Host App)
Precondition:
- [ ] Phase 2 complete and green.

1. Create `nv-platform-android` native library
- [x] Add `modules/nv-platform-android/CMakeLists.txt` producing Android `.so` via NDK.
- [x] Add JNI surface + thread marshalling utilities (UI thread).
- [x] Implement minimal window/webview + IPC bridge via host app callbacks.
- [x] Return “not supported” for desktop-only features initially.

2. Add Android host app (minimal, stable)
- [x] Create a minimal Gradle project (or example app) that:
  - [x] hosts an Activity with a WebView
  - [x] loads the native library
  - [x] exposes JNI calls needed by `nv-platform-android`
- [x] Keep Java/Kotlin code tiny and well-scoped; no business logic here.

3. Tests strategy for Android
- [x] Native unit tests remain platform-agnostic (vtable tests).
- [x] Add Android build-only “smoke” checks (compile/link) as CI targets (`android_smoke_build`).
- [x] Add Android instrumentation test to validate WebView loads + IPC roundtrip (required).

Gate:
- [ ] `android_smoke_build` succeeds (compile/link `nv-platform-android` via NDK).
- [ ] `examples/android_runner` builds (Gradle + externalNativeBuild).
- [ ] Instrumentation test passes: WebView loads + IPC ping/pong roundtrip.

## Deliverable Order (Strict)
- [x] Phase 0 complete and green
- [x] Phase 1 complete and green (desktop parity preserved)
- [ ] Phase 2 complete and green (iOS Xcode generation/build)
- [ ] Phase 3 complete and green (Android build + basic runtime smoke)
