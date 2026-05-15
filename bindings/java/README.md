# nativeview — Java (JNI) binding

Low-level desktop JNI for the same surface as **`bindings/nim/nativeview.nim`**: `io.jamharah.nativeview.NativeView`.

## Layout

| Path | Role |
|------|------|
| `src/main/java/io/jamharah/nativeview/*.java` | Public API + listener interfaces |
| `src/main/c/nativeview_jni_*.c` | JNI implementation |
| `CMakeLists.txt` | `nativeview_jni` shared target (requires root `-DNV_BUILD_SHARED=ON -DNV_BUILD_JAVA_JNI=ON`) |
| `pom.xml` | Minimal Maven project (`mvn compile`) |

## Quick build

From the repository build directory (after a successful CMake configure with **`NV_BUILD_SHARED=ON`** and **`NV_BUILD_JAVA_JNI=ON`**):

```bash
cmake --build . --target nativeview_jni
```

Full guide: **`docs/Java.md`** (includes **macOS** `-XstartOnFirstThread` for JNI + AppKit).

Android embedding (different artifact): **`docs/Android.md`** (`bindings/android`).
