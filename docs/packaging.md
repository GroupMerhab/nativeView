# Packaging & integration — nativeview

How to consume nativeview as a library in your application or binding.

---

## CMake subdirectory (recommended)

Add nativeview as a subdirectory in your project:

```cmake
add_subdirectory(path/to/nativeview EXCLUDE_FROM_ALL)
target_link_libraries(my_app PRIVATE nativeview)
target_include_directories(my_app PRIVATE path/to/nativeview/include)
```

Configure from your build tree:

```bash
cmake -S . -B build
cmake --build build
```

Options:

| Option | Default | Description |
|--------|---------|-------------|
| `NV_BUILD_UI` | `ON` | Build nv-ui (requires Vue for examples) |
| `NV_BUILD_SHARED` | `OFF` | Build `nativeview_shared` (.so/.dylib/.dll) |
| `NV_ENFORCE_FILE_LIMITS` | `ON` | Fail configure on line-limit violations |

Embedders that only need windowing + IPC can set `-DNV_BUILD_UI=OFF`.

---

## CMake install + pkg-config

Build and install:

```bash
cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/usr/local
cmake --build build
cmake --install build
```

Installed artifacts:

- `lib/libnativeview.a` (static archive; default target)
- `include/*.h` — public headers (`nv.h` and friends)
- `lib/pkgconfig/nativeview.pc`

Consumer with pkg-config:

```bash
gcc myapp.c $(pkg-config --cflags --libs nativeview) -o myapp
```

The template is [nativeview.pc.in](../nativeview.pc.in); CMake generates `nativeview.pc` at configure time.

---

## Static vs shared library

### Static (default)

- Target: `nativeview` (INTERFACE/static archive chain per platform)
- Link into your binary; no separate nativeview DLL at runtime
- Typical for Rust/Nim/Zig examples using `link-static` or CMake static archives

### Shared

```bash
cmake -S . -B build -DNV_BUILD_SHARED=ON
cmake --build build
```

- Target: `nativeview_shared`
- On Linux, ensure `RPATH` or `LD_LIBRARY_PATH` includes the install `lib/` directory
- Required for some language bindings (e.g. Python ctypes against `libnativeview.so`)

---

## Amalgamation (single-file distribution)

For minimal integration without the full module tree:

```bash
python3 tools/amalgamate.py --platform linux --output dist/
```

Produces:

- `dist/nativeview.c` — combined translation unit for one platform
- `dist/nv.h` — copy of `include/nv.h`

Compile:

```bash
cc -c dist/nativeview.c -I dist/ -o nativeview.o
```

Platform must match (`mac`, `win`, or `linux`). See [tools/amalgamate.py](../tools/amalgamate.py).

---

## Public headers

Primary entry point: **`include/nv.h`**

Supporting headers (installed alongside):

- `nv_json.h`, `nv_str.h`, `nv_arena.h`, `nv_util.h`, `nv_ipc.h`, `nv_window.h`, …

Only `nv.h` is required for basic embedding; other headers are for advanced use or bindings.

---

## JavaScript bundle

Desktop/mobile WebViews load **`nativeview.js`**, built from `js/src/`:

```bash
python3 tools/js_build.py
```

Android Gradle runs this on `:nativeview` `preBuild` unless `nativeview.skipJsBundle=true`.

---

## Android (Gradle AAR)

1. `include ':nativeview'` in `settings.gradle` pointing to `bindings/android`
2. `implementation project(':nativeview')`
3. Subclass `NativeViewActivity`, implement `getContentUrl()`

Full guide: [Android.md](Android.md). Working sample: [examples/android_full_bridge/](../examples/android_full_bridge/).

---

## Language bindings

See [bindings.md](bindings.md) for a full comparison. Per-binding guides:

| Binding | Location | Doc |
|---------|----------|-----|
| Java (desktop JNI) | `bindings/java` | [Java.md](Java.md) |
| Python | `bindings/python` | [Python.md](Python.md) |
| Nim | `bindings/nim` | [Nim.md](Nim.md) |
| Zig | `bindings/zig` | [Zig.md](Zig.md) |
| Pascal | `bindings/pascal` | [Pascal.md](Pascal.md) |
| Rust | `bindings/rust` | [Rust.md](Rust.md) |
| Swift (desktop) | `bindings/swift` | [Swift.md](Swift.md) |
| Android | `bindings/android` | [Android.md](Android.md) |
| iOS | `bindings/ios` | [iOS.md](iOS.md) |

---

## Versioning

Version metadata is generated at CMake configure time from the project version. Check runtime:

- C: `nv_get_version_info()` / version helpers in `nv_util.h`
- JS: `NativeView.version`, `NativeView.wireVersion` after handshake

See [CHANGELOG.md](../CHANGELOG.md) for release history.
