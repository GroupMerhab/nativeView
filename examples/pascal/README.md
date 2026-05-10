# Pascal examples — nativeview

Thin **`.lpr` + app unit`** layout; full notes in **[`docs/Pascal.md`](../../docs/Pascal.md)**.

## Files

| File | Purpose |
|------|---------|
| `nv_minimal.lpr` | Pure FPC **program** entry; calls `RunNativeView` (fine on Windows / Linux for many setups). |
| `nv_minimal_lib.lpr` | FPC **library** (`libnv_minimal_lib.dylib` / `.so`) exporting `_nv_pascal_minimal_run` → `RunNativeView`. Used on **macOS** with the C host below. |
| `nv_minimal_host.c` | Tiny **Clang `main`** that calls `_nv_pascal_minimal_run` (avoids a macOS + shared-`libnativeview` issue with FPC’s program startup; see below). |
| `nv_minimal_app.pas` | **`RunNativeView`**, window setup, **`cdecl`** callbacks, counter UI (same idea as `examples/hello/main.c`). |
| `build_example.sh` | Configures CMake (`nativeview_shared`), runs FPC, and on Darwin links **`nv_minimal_gui`**. |
| `build_static_example.sh` | CMake **static** archives + pure FPC **`nv_minimal_static`** (no `-dNATIVEVIEW_SHARED`). See **Static** below. |

The import unit is **`bindings/pascal/nativeview.pas`**.

## Quick build (recommended)

From **`examples/pascal`**:

```sh
./build_example.sh
```

- Sets **`NV_CMAKE_BUILD_DIR`** (default: `../../build-pascal-example`) and **`CMAKE_OSX_DEPLOYMENT_TARGET`** (default `11.0`) so the dylib matches FPC’s usual macOS target.
- **Darwin:** produces **`nv_minimal_gui`** + **`libnv_minimal_lib.dylib`**. Run **`./nv_minimal_gui`** from this directory.
- **Other Unix:** builds the pure FPC **`nv_minimal`** binary (adjust linker flags for `libnativeview.so` if the script’s defaults are wrong for your distro).

Override the CMake build directory:

```sh
NV_CMAKE_BUILD_DIR=/path/to/build ./build_example.sh
```

## Manual FPC flags (reference)

**Unit search path**

```text
-Fu../../bindings/pascal
```

**Shared `libnativeview` (after `cmake -DNV_BUILD_SHARED=ON` + `nativeview_shared`):**

```text
fpc -dNATIVEVIEW_SHARED -Fu../../bindings/pascal -Fl/path/to/build \
  -k-L/path/to/build -k-lnativeview …
```

On **macOS** add **`-k-framework -kCocoa`** (and usually **`-k-rpath -k/path/to/build`** so `libnativeview.dylib` is found).

**macOS + FPC `program`:** With **Free Pascal 3.2.x aarch64** as the main executable, **both** shared and **static** links into nativeview have been observed to fault in AppKit early (`EXC_BAD_INSTRUCTION` around window creation). That is tied to **FPC’s process startup**, not missing `-lobjc`. For a working GUI on Darwin, use **`./build_example.sh`** (Clang **`main`** + Pascal in **`libnv_minimal_lib.dylib`**).

**Windows:** import library / `PATH` for `nativeview.dll` as in **`docs/Pascal.md`**.

### Static linking (no `libnativeview` dylib)

```sh
./build_static_example.sh
```

Produces **`nv_minimal_static`**: **do not** pass `-dNATIVEVIEW_SHARED`; FPC links the same **`.a`** order as the C **`hello`** target plus Apple frameworks (`Carbon`, `Cocoa`, `CoreServices`, `WebKit`, `UserNotifications`, `objc`). Override the build tree with **`NV_CMAKE_BUILD_STATIC_DIR`**.

On **Linux**, extend the script (or your Lazarus link step) with whatever **`nv-platform-linux`** pulls (e.g. WebKitGTK) — mirror that module’s `CMakeLists.txt`.

## Lazarus

Add **`nv_minimal_app.pas`**, **`nativeview.pas`**, **`-dNATIVEVIEW_SHARED`** when using the DLL, and the same **Darwin** caveat if you link **`libnativeview.dylib`** dynamically.
