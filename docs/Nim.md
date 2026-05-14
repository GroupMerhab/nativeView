# Nim — nativeview

**On macOS, static linking tends to crash before the window appears (AppKit event-loop ownership). Use `-d:nativeviewShared` for macOS GUI apps.** This matches the warning in `examples/pascal/build_static_example.sh`: linking succeeds, but a normal Nim `main` can still hit AppKit `EXC_BAD_INSTRUCTION` before the window appears (same class of issue as Free Pascal’s default `program` startup). Prefer the **shared** `libnativeview.dylib` path for interactive GUI work on Darwin.

This guide is for **Nim** users who embed nativeview through **`bindings/nim/nativeview.nim`**, which mirrors the public C API in `include/nv.h` and `include/nv_hotkey.h`, plus `nv_version_string`, `nv_get_version_info`, and `nv_bench_now` from `include/nv_util.h` (included by `nv.h`).

## What you need

- A **built** nativeview library for your platform (see the root `CMakeLists.txt` and `AGENTS.md`). On Windows, the WebView2 runtime must be present (Evergreen is typical). If you configure CMake with the build directory **inside** the repository tree, you may need **`-DNV_ENFORCE_FILE_LIMITS=OFF`** during development (the **`examples/nim/build_static.sh`** and **`build_static.ps1`** scripts pass this so `cmake` is less likely to fail on generated compiler-id sources).
- **Nim** 1.6+ (2.x / ORC is supported). Use the same **CPU** and **C toolchain family** as the static `.a` / `.lib` files you link (e.g. MinGW-built archives with MinGW-backed `nim c`, MSVC-built archives with `nim c` configured for the MSVC backend).

## Static linking (default for this repo’s docs and scripts)

1. Configure CMake with **`-DNV_BUILD_SHARED=OFF`** (default) and build the static module archives (`nv-core`, `nv-ipc`, `nv-ops`, `nv-runtime`, `nv-platform-*`).

2. Do **not** define `-d:nativeviewShared`. The import module uses plain `{.importc, cdecl.}` so the linker resolves symbols from archives you pass with **`--passL:`** (or a copied `config.nims`).

3. **GNU ld / MinGW** (same issue as the root `nativeview` INTERFACE target for GCC): wrap the five in-tree archives in **`-Wl,--start-group`** … **`-Wl,--end-group`** so the linker can rescan until all cross-archive references resolve. The **`examples/nim/build_static.sh`** script does this on Linux.

4. **MSVC**: link the `.lib` archives in an order that satisfies undefined symbols; the shared target in `CMakeLists.txt` uses **`/WHOLEARCHIVE:`** per archive when building `nativeview_shared` — mirror that idea if you see missing symbols from dead stripping. Add WebView2 + Win32 system libraries (see `modules/nv-platform-win/CMakeLists.txt` and the `nativeview_shared` block in the root `CMakeLists.txt`). Do **not** define `NV_SHARED` for a fully static consumer that never includes `nv.h` in a separate C translation unit.

5. **Linux**: after the `--start-group` archive list, append everything **`nv-platform-linux`** needs: at minimum **`pkg-config --libs webkit2gtk-4.1`**. Optional packages pulled when CMake finds them: **ayatana-appindicator**, **appindicator3**, **libnotify**, **x11** — mirror `modules/nv-platform-linux/CMakeLists.txt` or reuse the `pkg-config` probes in **`examples/nim/build_static.sh`**.

6. **macOS (archives)**: archive order matches the C **`hello`** target / `examples/pascal/build_static_example.sh` (platform → runtime → ops → ipc → core). You still need **`-framework Carbon -framework Cocoa -framework CoreServices -framework WebKit -framework UserNotifications -lobjc`**. Read the **warning at the top of this file** before relying on static for GUI.

7. Checked-in template: **`bindings/nim/config.static.nims.example`** — copy to **`config.nims`**, set **`NV_CMAKE_BUILD`**, and extend the Linux `pkg-config` line as needed.

## Shared library (optional)

1. Configure CMake with **`-DNV_BUILD_SHARED=ON`** and build **`nativeview_shared`**. The produced binary is named **`nativeview`** (`nativeview.dll`, `libnativeview.so`, or `libnativeview.dylib`).

2. When compiling Nim code, pass **`-d:nativeviewShared`** so every import uses **`{.dynlib: ...}`** (`nativeview.dll` / `libnativeview.so` / `libnativeview.dylib`).

3. **Windows:** keep **`nativeview.dll`** on **`PATH`** or next to your `.exe`.

4. If you compile **C** sources that include **`include/nv.h`** with MSVC, define **`NV_SHARED`** so `NV_API` resolves to `dllimport` where required (see `include/util/nv_log.h`).

## Callbacks, GC, and string lifetimes

- **cdecl** callbacks must be **top-level** Nim procedures (or otherwise have a **stable** address for the lifetime they are registered). Do not register a closure that Nim lowers to an unstable thunk.

- **`userdata`:** pass a **`pointer`** to **module-level** or **manually pinned** data (for example `addr` of a global record). **Do not** pass a **`ref`** or the address of a **stack** value unless you guarantee it outlives every callback invocation.

- **GC / ORC:** do **not** pass a pointer to a **GC-managed heap object** (`ref`, `string`, `seq` elements, etc.) as `userdata` unless you keep that object **globally reachable** for the whole registration window. With **reference counting (refc)**, `GC_ref` / `GC_unref` can pin `ref` values when available. With **ORC** (Nim 2.x default), prefer a **module-level `var`** holding the `ref` / `seq`, or store plain **`pointer`** / **`cstring`** in module scope — `GC_ref` is not the right mental model on ORC-only builds.

- **`nv_on_message`:** `event` and `json` are valid **only for the duration of the callback**. Copy with `cstring` → `string` (`$`) if you need them later.

- **`nv_window_cfg_t.title`:** must remain valid through **`nv_window_create`** (see `nv.h`); use a **module-level** `cstring` or stable allocation.

## `nv_eval_js_batch`

The C type is `const char **scripts`. From Nim, pass **`cast[ptr UncheckedArray[cstring]](addr arr[0])`** (or the base address of an equivalent buffer) plus **`csize_t(count)`**. A **`seq[string]`** is **not** layout-compatible; build a **`array` or `seq` of `cstring`** whose storage stays alive until the nativeview work runs.

## Mapping defines (summary)

| Build | Nim define | C headers (if you compile C that includes `nv.h`) |
|--------|------------|---------------------------------------------------|
| Shared `nativeview` DLL/SO/dylib | **`-d:nativeviewShared`** | **`NV_SHARED`** (MSVC C TU) |
| Static `.a` / `.lib` chain | *(none)* | *(none)* |

## Example layout in this repo

- **`bindings/nim/nativeview.nim`** — import module.
- **`bindings/nim/nativeview.nimble`** — package metadata; **`nimble staticCheck`** runs `nim check` without linking.
- **`examples/nim/minimal.nim`** — minimal sample (`nv_on_ready`, `nv_load_html`, `nv_eval_js_batch` cast, teardown).
- **`examples/nim/build_static.sh`** / **`build_static.ps1`** — configure CMake (`-DNV_BUILD_SHARED=OFF`), build static targets, run `nim c` with a host-appropriate `--passL` chain (Linux script is the most automated).

## Troubleshooting

- **Undefined reference to `nv_*`:** wrong static link order, missing `--start-group` / `--end-group`, or missing `-d:nativeviewShared` when using the shared library.

- **Crash in callback:** invalid **`userdata`**, **`cstring`** backed by freed memory, or callback not **`cdecl`**.

- **DLL loads but window is blank:** load URL/HTML after **`nv_on_ready`** (see the example), and verify WebView2 / WebKitGTK runtime on the host.

## See also

- `include/nv.h` — authoritative C API
- `docs/Pascal.md` — parallel embedding guide (many linking rules are identical)
- `docs/Implementation.md` — `NV_BUILD_SHARED` and layout pointers
