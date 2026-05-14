# Nim examples — nativeview

**macOS:** `build_static.sh` links the static `.a` chain in the same order as the C examples; **`minimal.nim`** runs with that layout. **Optional:** build **`nativeview_shared`** (`cmake -DNV_BUILD_SHARED=ON`), put **`libnativeview.dylib`** on the loader path, and compile with **`-d:nativeviewShared`** if you prefer a shared library (see **`docs/Nim.md`**). **Linux:** the script’s **`--start-group`** / WebKitGTK **`pkg-config`** line is the primary automation.

## Files

| File | Purpose |
|------|---------|
| `minimal.nim` | Tiny app: version string, `nv_on_ready` → `nv_load_html`, `nv_eval_js_batch` cast pattern, `nv_app_destroy`. |
| `build_static.sh` | CMake static build + `nim c` — Linux (`--start-group`, WebKitGTK `pkg-config`) and macOS (ordered `.a` + frameworks). |
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
