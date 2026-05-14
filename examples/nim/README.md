# Nim examples — nativeview

**On macOS, static linking tends to crash before the window appears (AppKit event-loop ownership). Use `-d:nativeviewShared` for macOS GUI apps.** For a working Darwin GUI, build **`nativeview_shared`** (`cmake -DNV_BUILD_SHARED=ON`), install or copy `libnativeview.dylib` onto the loader path, then compile with **`-d:nativeviewShared`** (see **`docs/Nim.md`**). The **`build_static.sh`** flow here is mainly for **Linux** (and for experimentation).

## Files

| File | Purpose |
|------|---------|
| `minimal.nim` | Tiny app: version string, `nv_on_ready` → `nv_load_html`, `nv_eval_js_batch` cast pattern, `nv_app_destroy`. |
| `build_static.sh` | CMake static build + `nim c` with GNU ld `--start-group` / WebKitGTK `pkg-config` flags (Linux). |
| `build_static.ps1` | Outline for Windows MSVC users (fill archive paths + WebView2). |

Full linking rules: **`docs/Nim.md`**. Import path: **`bindings/nim/nativeview.nim`** (this script passes **`--path:`** to the repo root’s `bindings/nim`).

## Quick build (Linux)

From **`examples/nim`**:

```sh
./build_static.sh
```

Override the CMake build tree:

```sh
NV_CMAKE_BUILD_DIR=/path/to/build ./build_static.sh
```

## Shared build (all platforms)

```sh
cmake -S ../.. -B ../../build-nim-shared -DNV_BUILD_SHARED=ON -DNV_BUILD_TESTS=OFF -DNV_ENFORCE_FILE_LIMITS=OFF
cmake --build ../../build-nim-shared --target nativeview_shared -j8
export LD_LIBRARY_PATH="../../build-nim-shared:$LD_LIBRARY_PATH"   # Linux
nim c -d:release -d:nativeviewShared --path:../../bindings/nim minimal.nim
```

On Windows, ensure **`nativeview.dll`** is on **`PATH`** or next to **`minimal.exe`**.
