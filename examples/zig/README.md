# Zig minimal sample

**macOS:** `build_static.sh` links the static `.a` chain in the same order as the C examples; **`minimal.zig`** runs with that layout. **Optional:** build **`nativeview_shared`** and link **`libnativeview.dylib`** if you prefer a shared library (see **`docs/Zig.md`**).

## Build (Linux / macOS)

From this directory:

```bash
./build_static.sh
```

Uses **`zig build-exe`** with **`--dep nativeview`** so `minimal.zig` can `@import("nativeview")`. Override **`NV_CMAKE_BUILD_DIR`** to reuse an existing CMake binary directory, or **`ZIG`** to pick a non-default Zig binary.

## Build (Windows)

PowerShell, MSVC-built nativeview archives (same expectations as **`examples/nim/build_static.ps1`** — WebView2 static loader path):

```powershell
.\build_static.ps1
```

## Shared library (optional)

Configure CMake with **`-DNV_BUILD_SHARED=ON`**, build **`nativeview_shared`**, then link `nativeview.dll` / `libnativeview.so` / `libnativeview.dylib` from your Zig command or `build.zig`. Keep the DLL on **`PATH`** on Windows.

## Source

- **`minimal.zig`** — `nv_on_ready` → `nv_load_html`, `nv_eval_js_batch`, `nv_send`, `nv_app_run`
- Module: **`bindings/zig/nativeview.zig`** (see **`docs/Zig.md`**)
