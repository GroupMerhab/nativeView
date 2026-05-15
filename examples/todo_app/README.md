# Todo example (Vue + SQLite + nativeView)

Layout:

- **`c_todo/`** — C sources, CMake, embed step, and `clean_build.sh` (executable **`todo_app`**).
- **`ui/`** — Vue 3 + Pinia + Vite (`vite-plugin-singlefile`) → `ui/dist/index.html`; shared by the language ports below.
- **`nim_todo/`** — Nim port (`todo_app.nim` + SQLite + bridge); see [nim_todo/README.md](nim_todo/README.md). Executable **`nim_todo`** via `nim_todo/build_static.sh`.
- **`rust_todo/`** — Rust port (`src/main.rs` + SQLite + bridge); see [rust_todo/README.md](rust_todo/README.md). Executable **`rust_todo`** via `rust_todo/build_static.sh`.
- **`py_todo/`** — Python port (`todo_app.py` + SQLite + bridge); see [py_todo/README.md](py_todo/README.md). Run **`python3 todo_app.py`** after `py_todo/build_shared.sh` (shared `libnativeview` + embedded HTML).
- **`java_todo/`** — Java port (`TodoApp` + SQLite + bridge); see [java_todo/README.md](java_todo/README.md). Run **`java … io.jamharah.todo.TodoApp`** after `java_todo/build_shared.sh` (shared `libnativeview` + JNI + packaged UI).
- **`zig_todo/`** — Zig port (`todo_app.zig` + SQLite + bridge); see [zig_todo/README.md](zig_todo/README.md). Executable **`zig_todo`** via `zig_todo/build_static.sh`.
- **`android_todo/`** — Android app (Java + SQLite + `bindings/android`); see [android_todo/README.md](android_todo/README.md). APK via `android_todo/prep_assets.sh` and `./gradlew :app:assembleDebug`.

## Build (Zig)

From `zig_todo/`:

```bash
./build_static.sh
```

Skip npm (fallback HTML only): `NV_TODO_SKIP_UI_BUILD=ON ./build_static.sh`

Unit tests: `./run_tests.sh`

## Build (Nim)

From `nim_todo/`:

```bash
./build_static.sh
```

Skip npm (fallback HTML only): `NV_TODO_SKIP_UI_BUILD=ON ./build_static.sh`

Unit tests: `./run_tests.sh`

## Build (Rust)

From `rust_todo/`:

```bash
./build_static.sh
```

Skip npm (fallback HTML only): `NV_TODO_SKIP_UI_BUILD=ON ./build_static.sh`

Unit tests: `./run_tests.sh`

## Build (Python)

From `py_todo/`:

```bash
./build_shared.sh
```

Skip npm (fallback HTML only): `NV_TODO_SKIP_UI_BUILD=ON ./build_shared.sh`

Unit tests: `./run_tests.sh`

## Build (Java)

From `java_todo/`:

```bash
./build_shared.sh
```

Skip npm (fallback HTML only): `NV_TODO_SKIP_UI_BUILD=ON ./build_shared.sh`

Unit tests: `./run_tests.sh` (requires Maven)

## Build (Android)

From `android_todo/`:

```bash
./prep_assets.sh
./gradlew :app:assembleDebug
```

Skip npm (fallback HTML only): `NV_TODO_SKIP_UI_BUILD=ON ./prep_assets.sh`

Unit tests: `./gradlew :app:testDebugUnitTest` (JVM / Robolectric; set **`JAVA_HOME`** to JDK **17–21** when your default JDK is newer.)

## Build (C)

From `c_todo/`:

```bash
./clean_build.sh
```

Optional: skip Node/npm (embeds `ui/fallback_index.html` only):

```bash
./clean_build.sh -DNV_TODO_SKIP_UI_BUILD=ON
```

Extra CMake flags append after the script defaults.

## UI dev / tests

```bash
cd ui && npm install && npm run dev
cd ui && npm test
```

## Database

Default path: `./todo_app.db` (override with first CLI argument after the executable name).
