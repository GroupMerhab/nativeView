# Python — nativeview

**On macOS, static linking tends to crash before the window appears (AppKit event-loop ownership). Prefer the shared `libnativeview.dylib` path for macOS GUI apps.** Same guidance as `docs/Nim.md` and `examples/pascal/build_static_example.sh`.

This guide is for **Python** 3.10+ users who embed nativeview through **`bindings/python/nativeview/`**, a **ctypes** layer that mirrors the public C API in `include/nv.h` and `include/nv_hotkey.h`, plus `nv_version_string`, `nv_get_version_info`, and `nv_bench_now` from `include/nv_util.h` (included by `nv.h`).

## What you need

- A **built** `nativeview_shared` shared library (`-DNV_BUILD_SHARED=ON`): `nativeview.dll`, `libnativeview.so`, or `libnativeview.dylib`. Pure **ctypes** loads that binary at runtime; it does not link static `.a` archives into the interpreter the way Nim’s `--passL` static link does.
- **Linux:** WebKitGTK (`webkit2gtk-4.1`) and the same optional packages as the C stack (see `modules/nv-platform-linux/CMakeLists.txt` and `docs/Nim.md`).
- **Windows:** WebView2 runtime (Evergreen is typical).
- If CMake’s build directory lives **inside** the repo tree, you may need **`-DNV_ENFORCE_FILE_LIMITS=OFF`** (the **`examples/python/build_shared.sh`** script passes this).

## Shared library (default for Python)

1. Configure CMake with **`-DNV_BUILD_SHARED=ON`** and build target **`nativeview_shared`**. The produced file is named **`nativeview`** (`nativeview.dll`, `libnativeview.so`, `libnativeview.dylib`).

2. Install or extend **`PYTHONPATH`** so `import nativeview` resolves to **`bindings/python/`** (the parent of the `nativeview` package directory), or `pip install -e bindings/python` from the repo root.

3. Point the loader at the shared library:
   - Set **`NATIVEVIEW_LIB`** to the absolute path of the `.dll` / `.so` / `.dylib`, **or**
   - Put that file on the default dynamic loader search path, **or**
   - Call **`nativeview.load_library("/path/to/libnativeview.so")`** before other calls.

4. If you compile **C** sources that include **`include/nv.h`** with MSVC, define **`NV_SHARED`** where `NV_API` must be `dllimport` (see `include/util/nv_log.h`). The ctypes binding does not compile C itself.

## Callbacks and object lifetimes

- **`CFUNCTYPE` callbacks:** keep a **strong reference** to each callback object for as long as it is registered (assign to a module-level or app object attribute). If the callback is garbage-collected, native code may jump to freed memory.

- **`userdata`:** pass an **integer** (converted with `ctypes.c_void_p` internally) or a **`ctypes.py_object`** / stable address only if you control lifetime. Do not pass a bare **`id()`** of a short-lived object unless you retain the object.

- **`nv_on_message`:** `event` and `json` are **`bytes`** (or depend on ctypes conversion); they are valid **only inside the callback**. Copy with `.decode("utf-8")` if you need them later.

- **`NvWindowCfg.title`:** must remain valid through **`nv_window_create`** (see `nv.h`). Keep the backing **`bytes`** alive until `nv_window_create` returns (store in a local variable or module-level buffer, as in **`examples/python/minimal.py`**).

## `nv_eval_js_batch`

Pass a **`list`** or **`tuple`** of **`str`** or **`bytes`**. The wrapper UTF-8-encodes **`str`** and builds a contiguous **`(c_char_p * n)`** array for the duration of the call.

## Layout in this repo

- **`bindings/python/nativeview/`** — ctypes package.
- **`bindings/python/pyproject.toml`** — setuptools metadata (`pip install -e bindings/python`).
- **`docs/Python.md`** — this file.
- **`examples/python/minimal.py`** — minimal sample (`nv_on_ready`, `nv_load_html`, `nv_eval_js_batch`, teardown).
- **`examples/python/build_shared.sh`** / **`build_shared.ps1`** — configure CMake with shared on, build **`nativeview_shared`**, print **`NATIVEVIEW_LIB`** usage.

## Troubleshooting

- **`OSError: Could not load nativeview shared library`:** set **`NATIVEVIEW_LIB`**, or build **`nativeview_shared`**, or run **`load_library(...)`** with a full path.

- **Blank window:** load URL/HTML after **`nv_on_ready`** (see the example).

## See also

- `include/nv.h` — authoritative C API
- `docs/Nim.md` — parallel embedding notes (linking detail for static archives applies to Nim/Zig, not ctypes)
- `docs/Implementation.md` — `NV_BUILD_SHARED` and layout pointers
