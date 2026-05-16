# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

Android-specific release notes may also appear in
[bindings/android/CHANGELOG.md](bindings/android/CHANGELOG.md).

## [0.1.0] - 2026-05-16

### Added

- Core library (`nv-core`): arena allocator, strings, JSON, logging, utilities
- IPC layer (`nv-ipc`): JSON wire, dispatch, shared memory helpers
- Native ops (`nv-ops`): window, app, fs, dialog, clipboard, shell, screen, tray, notification, windows, ipc_bus
- Runtime (`nv-runtime`): app/window lifecycle, window manager
- Platform backends: macOS (WKWebView), Windows (WebView2), Linux (WebKitGTK)
- Mobile backends (in development): Android JNI, iOS
- Optional UI layer (`nv-ui`): reactive widgets, Vue 3 scaffold + JSON patch diff
- Optional HTTP server (`nv-http`) for local/dev static serving
- JavaScript bridge bundle (`nativeview.js`) and TypeScript types
- Language bindings: Java, Python, Swift, Nim, Zig, Pascal, Rust (FFI), Android library
- Examples: `hello`, `counter_ui`, `todo_app` variants, `demo`, Android full bridge
- Public C API in `include/nv.h`
- pkg-config template (`nativeview.pc.in`) and CMake install targets
- Amalgamation tool (`tools/amalgamate.py`) for single-file distribution

### Documentation

- Getting started, architecture, JS bridge reference, packaging guide
- Per-binding guides under `docs/`
- Contributor Covenant, SECURITY policy, CLA, NOTICE

[0.1.0]: https://github.com/jamharah/nativeview/releases/tag/v0.1.0
