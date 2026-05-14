# Java — nativeview

This guide is for **Java** (desktop JVM) embedders using **`bindings/java/`**: the `io.jamharah.nativeview.NativeView` class mirrors the public C API in `include/nv.h` and `include/nv_hotkey.h`, plus `nv_version_string`, `nv_get_version_info`, and `nv_bench_now` from `include/nv_util.h` (included by `nv.h`). The layout matches the low-level Nim module **`bindings/nim/nativeview.nim`** and the Rust crate **`bindings/rust`**.

## What you need

- A **shared** nativeview build (`-DNV_BUILD_SHARED=ON`) producing `nativeview.dll` / `libnativeview.so` / `libnativeview.dylib` (see the root `CMakeLists.txt` and **`docs/Nim.md`** / **`docs/Python.md`** for runtime notes).
- A **JDK** (11+) so CMake `find_package(JNI)` can locate headers and (on Windows) `jvm.lib`.
- **Maven** (optional) to compile the small JAR from `bindings/java/pom.xml`, or run `javac` yourself.

## Building the JNI shared library

From a clean binary directory:

```bash
cmake .. -DNV_BUILD_SHARED=ON -DNV_BUILD_JAVA_JNI=ON
cmake --build . --target nativeview_jni
```

Artifacts (names may vary slightly by OS):

- `libnativeview_jni.so` / `libnativeview_jni.dylib` / `nativeview_jni.dll` — JNI glue; **`System.loadLibrary("nativeview_jni")`** loads this.
- The main **`libnativeview`** / **`nativeview`** shared library from the same build.

On **macOS** and **Linux**, the `nativeview_jni` target is given an **RPATH** of `@loader_path` / `$ORIGIN` so that if both native libraries sit in the same folder, the dynamic loader finds `libnativeview` without extra environment variables. Otherwise set **`DYLD_LIBRARY_PATH`** / **`LD_LIBRARY_PATH`** as you would for any shared library.

On **Windows**, keep **`nativeview.dll`** and **`nativeview_jni.dll`** on **`PATH`** or next to your **`java.exe`** / application launcher.

## macOS (JNI + AppKit)

The HotSpot JVM normally runs **`main`** on a worker pthread, while AppKit expects UI setup on the **process primordial thread**. **`NativeView.loadLibrary()`** therefore fails fast on macOS unless the current thread is that primordial thread.

**Run your app with:**

```bash
java -XstartOnFirstThread --enable-native-access=ALL-UNNAMED -Djava.library.path=/path/to/jni+libnativeview ...
```

For advanced cases only, you can disable the check with **`-Dnativeview.mac.skipMainThreadCheck=true`** (not recommended).

## Compiling the Java side

```bash
cd bindings/java
mvn -q compile
# JAR classes under target/classes; add to your app’s classpath
```

Or manually:

```bash
javac --release 11 -d out src/main/java/io/jamharah/nativeview/*.java
java -Djava.library.path=/path/to/dir/with/jni ... your.Main
```

## Callbacks and threads

- **Listeners** (`NvMessageListener`, `NvReadyListener`, `NvCloseListener`) are stored as JNI global references. Keep the Java object reachable for as long as it is registered; pass **`null`** to clear a listener.
- **`NvMessageListener.onMessage`:** `event` and `json` are only valid during the callback (same as C **`nv_on_message`**).
- Nativeview may invoke callbacks from **non-Java** threads; the JNI layer attaches temporarily. Avoid heavy work in callbacks; marshal to your UI thread if needed.

## Shared vs static

The checked-in **`bindings/java/CMakeLists.txt`** links **`nativeview_jni`** against **`nativeview_shared`**. Fully static single-library embedding (like Nim’s multi-archive **`--passL`**) is not scripted here; extend CMake locally if you need that layout.

## See also

- `include/nv.h` — authoritative C API
- **`docs/Nim.md`** — linking and lifecycle notes (many rules apply equally)
- **`bindings/java/README.md`** — short path reference
