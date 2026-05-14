# Todo app (Java + nativeview JNI)

Java port of [../nim_todo](../nim_todo) / [../py_todo](../py_todo): same SQLite schema and Vue `__nv` bridge. The window uses [../../../bindings/java](../../../bindings/java) (`io.jamharah.nativeview.NativeView`) against **shared** `libnativeview` + `libnativeview_jni` — see [../../../docs/Java.md](../../../docs/Java.md).

## Build

From this directory:

```bash
chmod +x build_shared.sh run_tests.sh
./build_shared.sh
```

- **Full UI**: runs `npm install` + `npm run build` in [../ui](../ui), copies `dist/index.html` to [generated/todo_ui.html](generated/todo_ui.html), configures CMake with `NV_BUILD_SHARED=ON` and `NV_BUILD_JAVA_JNI=ON`, builds target **`nativeview_jni`**, then **`mvn package`**.
- **Skip npm**: `NV_TODO_SKIP_UI_BUILD=ON ./build_shared.sh` copies [../ui/fallback_index.html](../ui/fallback_index.html) only.

If you run Maven yourself, use a bare command such as **`mvn -q package -DskipTests`**. Some environments pass a literal **`#`** through to Maven (for example a pasted line like `mvn … # comment`), which produces `Unknown lifecycle phase "#"`.

The script prints a `java …` example using **`target/java-todo-0.1.0-all.jar`** (fat JAR: includes **sqlite-jdbc** and **gson**). The plain **`java-todo-0.1.0.jar`** is thin and is **not** enough to run the app unless you add those JARs to `-cp` yourself.

On recent JDKs you may see native-access warnings from **JNI** (`nativeview_jni`) and **SQLite JDBC**; add **`--enable-native-access=ALL-UNNAMED`** (or put the JAR on the module path with the right opens) to silence them.

Optional DB path: first CLI argument (default `./todo_app.db`).

## Tests

```bash
./run_tests.sh
```

(Requires **Maven** on `PATH`, or set `MVN=/path/to/mvn`.)

## UI packaging

The built HTML is copied to **`generated/todo_ui.html`** and packaged as the classpath resource **`/todo_ui.html`** (avoids huge Java static initializers). A small stub ships in git so `mvn test` works before the first UI build.

## macOS

Use the directory that contains both `libnativeview.dylib` and `libnativeview_jni.dylib` for `-Djava.library.path` (CMake sets `@loader_path` / `$ORIGIN` on the JNI library when built from the repo).

Run the JVM with **`-XstartOnFirstThread`** so `NativeView.loadLibrary()` succeeds (AppKit requires the primordial thread). **`build_shared.sh`** prints a `java …` line that includes this flag on macOS.
