# Zig — nativeview

**macOS:** Static linking with the archive order and frameworks used in **`examples/zig/build_static.sh`** matches the C examples and is the normal path for **`minimal.zig`** and the Zig todo sample. **Optional:** link **`libnativeview.dylib`** (build **`nativeview_shared`** with **`cmake -DNV_BUILD_SHARED=ON`**) if you prefer a shared library. (Some other language runtimes have hit AppKit startup edge cases when static-linking GUI code; the checked-in Zig samples do not require shared linking.)

This guide is for **Zig** users who embed nativeview through **`bindings/zig/nativeview.zig`**, which mirrors the public C API in `include/nv.h` and `include/nv_hotkey.h`, plus `nv_version_string`, `nv_get_version_info`, and `nv_bench_now` from `include/nv_util.h` (included by `nv.h`).

## What you need

- A **built** nativeview library for your platform (see the root `CMakeLists.txt` and `AGENTS.md`). On Windows, the WebView2 runtime must be present (Evergreen is typical). If you configure CMake with the build directory **inside** the repository tree, you may need **`-DNV_ENFORCE_FILE_LIMITS=OFF`** during development (the **`examples/zig/build_static.sh`** script passes this so `cmake` is less likely to fail on generated compiler-id sources).
- **Zig** 0.13+ (this repo is tested with recent nightly; use a stable release that supports `-M` / `--dep` module flags). Use the same **CPU** and **C toolchain family** as the static `.a` / `.lib` files you link (e.g. MinGW-built archives with Zig’s MinGW target, MSVC-built archives with `-target x86_64-windows-msvc`).

## Importing the binding

Zig 0.14+ expects the `nativeview` module to be wired from the **root** module (CLI or `build.zig`):

```text
zig build-exe --dep nativeview -Mroot=src/app.zig -Mnativeview=/path/to/nativeview/bindings/zig/nativeview.zig …
```

In your root file: `const nv = @import("nativeview");`

The checked-in sample uses this pattern in **`examples/zig/build_static.sh`** / **`build_static.ps1`**.

## Static linking (default for this repo’s scripts)

1. Configure CMake with **`-DNV_BUILD_SHARED=OFF`** (default) and build the static module archives (`nv-core`, `nv-ipc`, `nv-ops`, `nv-runtime`, `nv-platform-*`).

2. Pass **`zig build-exe … -lc`** on Unix so the C runtime matches the nativeview objects.

3. **Linux:** append the five `.a` files in the same order as **`examples/nim/build_static.sh`**, then everything **`nv-platform-linux`** needs: at minimum **`pkg-config --libs webkit2gtk-4.1`**. Zig’s CLI does not accept raw **`-pthread`** from some `pkg-config` lines; replace it with **`-lpthread`** (the **`examples/zig/build_static.sh`** script does this). Optional packages when CMake finds them: **ayatana-appindicator**, **appindicator3**, **libnotify**, **x11** — mirror `modules/nv-platform-linux/CMakeLists.txt` or reuse the `pkg-config` probes in the script.

4. **GNU ld / MinGW:** if you see unresolved symbols between static archives, wrap them in **`-Wl,--start-group`** … **`-Wl,--end-group`** when driving **`cc`** / **`zig cc`** as the linker. The stock **`zig build-exe`** path in **`build_static.sh`** often succeeds without a group on LLD; fall back to a group or **`zig cc`** if your linker requires it.

5. **MSVC:** link the `.lib` archives with **`/WHOLEARCHIVE:`** per archive when needed (see the `nativeview_shared` block in the root `CMakeLists.txt`). Add WebView2 + Win32 system libraries (see **`examples/nim/build_static.ps1`**). Do **not** define `NV_SHARED` for a fully static consumer that never includes `nv.h` in a separate C translation unit.

6. **macOS (archives):** archive order matches the C **`hello`** target / `examples/pascal/build_static_example.sh` (platform → runtime → ops → ipc → core). Use **`zig build-exe`** **`-framework Carbon`** **`-framework Cocoa`** **`-framework CoreServices`** **`-framework WebKit`** **`-framework UserNotifications`** **`-lobjc`**. Shared **`libnativeview.dylib`** remains optional if you prefer that layout (see **Shared library** below).

## Shared library (optional)

1. Configure CMake with **`-DNV_BUILD_SHARED=ON`** and build **`nativeview_shared`**. The produced binary is named **`nativeview`** (`nativeview.dll`, `libnativeview.so`, or `libnativeview.dylib`).

2. Link the import library / `.so` / `.dylib` from Zig (`zig build-exe …` with the `.dylib` / `.lib` path, or `-L` + `-lnativeview` as appropriate for your target).

3. **Windows:** keep **`nativeview.dll`** on **`PATH`** or next to your `.exe`.

4. If you compile **C** sources that include **`include/nv.h`** with MSVC, define **`NV_SHARED`** so `NV_API` resolves to `dllimport` where required (see `include/util/nv_log.h`).

## Callbacks and string lifetimes

- **C calling convention:** callbacks registered with `nv_on_ready`, `nv_on_message`, or `nv_window_on_close` must use **`callconv(.c)`** and remain valid for the whole time they can be invoked (typically **file scope** `fn` in Zig).

- **`userdata`:** must outlive every callback use. Do not pass a pointer to a **stack** value unless you prove it cannot fire after the stack frame returns. Prefer **static** / **global** storage or heap memory you own for the app lifetime.

- **`nv_on_message`:** `event` and `json` are valid **only for the duration of the callback**. Copy with `std.mem.span` (or similar) into owned memory if you need them later.

- **`nv_window_cfg_t.title`:** must remain valid through **`nv_window_create`** (see `nv.h`); use a **comptime** string, **`static`** storage, or other stable allocation.

## `nv_eval_js_batch`

The C type is `const char **scripts`. Pass a many-item pointer to a contiguous array of null-terminated C strings, e.g. `scripts.ptr` where `scripts` is a **`[]const [*:0]const u8`** slice of literals. Storage must stay valid until nativeview consumes the call.

## Mapping defines (summary)

| Build | Zig / C headers |
|--------|-----------------|
| Shared `nativeview` DLL/SO/dylib | Link the shared artifact; if MSVC C TU includes `nv.h`, define **`NV_SHARED`**. |
| Static `.a` / `.lib` chain | Plain `extern "c"` from **`nativeview.zig`**; no `NV_SHARED` for pure-Zig apps. |

## Example layout in this repo

- **`bindings/zig/nativeview.zig`** — low-level `extern "c"` module.
- **`examples/zig/minimal.zig`** — minimal sample (`nv_on_ready`, `nv_load_html`, `nv_eval_js_batch`, teardown).
- **`examples/zig/build_static.sh`** / **`build_static.ps1`** — configure CMake (`-DNV_BUILD_SHARED=OFF`), build static targets, run `zig build-exe` with a host-appropriate link line.

## Troubleshooting

- **Undefined reference to `nv_*`:** wrong static link order, missing archives, or missing WebKitGTK / WebView2 / framework libraries.

- **Unrecognized parameter `-pthread`:** Zig’s `build-exe` driver may reject it; use **`-lpthread`** instead (see **`examples/zig/build_static.sh`**).

- **Crash in callback:** invalid **`userdata`**, or dangling **`[*:0]const u8`** backing store.

- **DLL loads but window is blank:** load URL/HTML after **`nv_on_ready`** (see the example), and verify WebView2 / WebKitGTK runtime on the host.

## See also

- `include/nv.h` — authoritative C API
- `docs/Nim.md` — parallel embedding guide (many linking rules are identical)
- `docs/Pascal.md` — parallel embedding guide
- `docs/Implementation.md` — `NV_BUILD_SHARED` and layout pointers
