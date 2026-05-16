# Swift — nativeview

This guide is for **Swift** users who embed nativeview through the SwiftPM package **`bindings/swift/`**: a small **`CNativeview`** target (C shim + umbrella header) and a **`Nativeview`** library that **`@_exported import CNativeview`**, mirroring the public C API in `include/nv.h` and `include/nv_hotkey.h`, plus `nv_version_string`, `nv_get_version_info`, and `nv_bench_now` from `include/nv_util.h` (pulled in by `nv.h`).

## What you need

- **Swift 5.9+** and SwiftPM (`swift build`).
- A **built** nativeview static archive set (or `libnativeview` shared) for your platform; see the root `CMakeLists.txt` and `AGENTS.md`. When the CMake build directory lives **inside** the repository tree, pass **`-DNV_ENFORCE_FILE_LIMITS=OFF`** during development (the **`examples/swift/build_static.sh`** script does this).

## Header layout (`nv_pub` symlink)

SwiftPM rejects `headerSearchPath` values outside the package root. The binding therefore ships a symlink **`bindings/swift/Sources/CNativeview/nv_pub`** → **`../../../../include`** (repo public headers). If the symlink is missing after checkout (some Windows setups), recreate it from **`bindings/swift/Sources/CNativeview`**:

```bash
ln -sf ../../../../include nv_pub
```

## Consuming the package

Add a path dependency. Because the directory is named **`swift`**, give the package an explicit identity so products resolve predictably:

```swift
.package(name: "Nativeview", path: "path/to/nativeview/bindings/swift"),
// …
.target(
  name: "MyApp",
  dependencies: [.product(name: "Nativeview", package: "Nativeview")]
)
```

Then `import Nativeview` and call the generated `nv_*` entry points (same names as C).

## Static linking (this repo’s script)

1. Configure CMake with **`-DNV_BUILD_SHARED=OFF`** (default) and build **`nv-core`**, **`nv-ipc`**, **`nv-ops`**, **`nv-runtime`**, **`nv-platform-*`**.

2. From **`examples/swift`**, run **`./build_static.sh`** (optional **`NV_CMAKE_BUILD_DIR`** / **`NV_JOBS`**). It configures CMake if needed, builds the archives, then runs **`swift build -c release`** in **`examples/swift/minimal`** with **`swift build`’s `-Xlinker`** flags matching **`examples/nim/build_static.sh`** (macOS archive order + frameworks; Linux **`--start-group`** / **`--end-group`** plus **`pkg-config --libs webkit2gtk-4.1`** and optional indicator/notify/x11 packages).

3. **macOS:** keep UI work on the **main** thread (`nv_app_run` blocks there, like the Nim sample).

## Shared library (optional)

1. Build **`nativeview_shared`** with **`-DNV_BUILD_SHARED=ON`** (`libnativeview.dylib` / `.so` / `.dll`).

2. Link the shared library from your final **`swift build`** / Xcode step (**`-L`** / **`-lnativeview`** or an explicit path to the dylib).

3. **Windows:** when compiling **C** sources that include **`nv.h`** with MSVC, define **`NV_SHARED`** where required (see `include/util/nv_log.h`).

## Callbacks, concurrency, and string lifetimes

- Use **`@convention(c)`**-compatible functions (module-level **`private func`** or **`static`** methods) for **`nv_on_ready`**, **`nv_on_message`**, **`nv_window_on_close`**, and similar. Do not register a plain Swift closure unless you know it has a stable C address for the registration lifetime.

- **`userdata`:** pass **`UnsafeMutableRawPointer`** to storage that **outlives** every callback (for example **`withUnsafeMutablePointer`** to a **`var`** that lives until **`nv_app_destroy`**, or a manually allocated box). Do **not** pass transient stack pointers.

- **`nv_on_message`:** `event` and `json` are valid **only inside the callback**. Copy to **`String`** with **`String(cString:)`** if you need them later.

- **`nv_window_cfg_t.title`:** must stay valid through **`nv_window_create`** (see `nv.h`); keep a stable buffer (for example **`ContiguousArray<CChar>`** / **`utf8CString`** with **`withUnsafeMutableBufferPointer`**).

## `nv_eval_js_batch`

The C API is **`const char **scripts`** with a **count**. Build an array of **`UnsafePointer<CChar>?`** (or **`[String]`** lowered to stable **`CString` buffers**), then call **`withUnsafeMutableBufferPointer`** and pass **`scripts.baseAddress!`** plus the count, as in **`examples/swift/minimal/Sources/main.swift`**.

## Checked-in layout

- **`bindings/swift/Package.swift`** — SwiftPM manifest (**`Nativeview`** library product).
- **`bindings/swift/Sources/CNativeview/`** — shim C file, **`include/NativeviewBridge.h`**, **`nv_pub`** symlink.
- **`examples/swift/build_static.sh`** — CMake + **`swift build`** for **`examples/swift/minimal`**.
- **`examples/swift/minimal/`** — minimal executable sample (mirrors **`examples/nim/minimal.nim`**).
- **`examples/todo_app/swift_todo/`** — full todo sample (SQLite + Vue embed + **`app/`** executable package; root package is libraries + **`swift test`** only).

## Troubleshooting

- **`unknown package 'Nativeview'`** in a downstream **`Package.swift`:** add **`.package(name: "Nativeview", path: …)`** (see above).

- **`'nv.h' file not found`:** restore the **`nv_pub`** symlink.

- **Undefined symbols for `nv_*`:** pass the static **`.a`** list (or **`libnativeview`**) on the **final** link of your executable, as **`build_static.sh`** does.

## See also

- `include/nv.h` — authoritative C API
- `docs/Nim.md` — parallel embedding notes (link order, lifetimes)
