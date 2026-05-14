#!/usr/bin/env bash
# Build java_todo: optional Vue UI, copy HTML to generated/, shared nativeview + JNI, Maven package.
# Run from examples/todo_app/java_todo.
#
# Env:
#   NV_TODO_SKIP_UI_BUILD=ON  — embed ../ui/fallback_index.html only (no npm)
#   NV_CMAKE_BUILD_DIR        — CMake binary dir (default: repo build-java-todo-shared)
#   CMAKE_BUILD_TYPE, NV_JOBS — same spirit as py_todo/build_shared.sh
#
# After build, run with JAVA_LIBRARY_PATH printed below (see README.md).
set -euo pipefail
SCRIPT_DIR="$(CDPATH= cd -- "$(dirname "$0")" && pwd)"
REPO_ROOT="$(CDPATH= cd -- "$SCRIPT_DIR/../../.." && pwd)"
BUILD_DIR="${NV_CMAKE_BUILD_DIR:-$REPO_ROOT/build-java-todo-shared}"
MIN_OS="${CMAKE_OSX_DEPLOYMENT_TARGET:-11.0}"
GEN_DIR="$SCRIPT_DIR/generated"
OUT_HTML="$GEN_DIR/todo_ui.html"
PYTHON="${PYTHON:-python3}"
MVN="${MVN:-mvn}"

mkdir -p "$GEN_DIR"
cd "$SCRIPT_DIR"

if [[ "${NV_TODO_SKIP_UI_BUILD:-}" == "ON" ]] || [[ "${NV_TODO_SKIP_UI_BUILD:-}" == "1" ]]; then
  HTML_IN="$SCRIPT_DIR/../ui/fallback_index.html"
else
  UI_ROOT="$SCRIPT_DIR/../ui"
  (cd "$UI_ROOT" && npm install && npm run build)
  HTML_IN="$UI_ROOT/dist/index.html"
fi

cp -f "$HTML_IN" "$OUT_HTML"

cmake -S "$REPO_ROOT" -B "$BUILD_DIR" \
  -DNV_BUILD_SHARED=ON \
  -DNV_BUILD_JAVA_JNI=ON \
  -DNV_BUILD_TESTS=OFF \
  -DNV_ENFORCE_FILE_LIMITS=OFF \
  -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-Release}" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET="$MIN_OS"

cmake --build "$BUILD_DIR" --target nativeview_jni -j"${NV_JOBS:-8}"

_root_lib="$(find "$BUILD_DIR" -maxdepth 1 \( -name 'libnativeview.dylib' -o -name 'libnativeview.so' -o -name 'nativeview.dll' \) -print -quit)"
if [[ -z "$_root_lib" ]]; then
  echo "Could not find libnativeview under $BUILD_DIR" >&2
  exit 1
fi
_root_dir="$(CDPATH= cd -- "$(dirname "$_root_lib")" && pwd)"
_jni="$(find "$BUILD_DIR" \( -name 'libnativeview_jni.dylib' -o -name 'libnativeview_jni.so' -o -name 'nativeview_jni.dll' \) -print -quit)"
if [[ -z "$_jni" ]]; then
  echo "Could not find nativeview_jni under $BUILD_DIR" >&2
  exit 1
fi
_jni_dir="$(CDPATH= cd -- "$(dirname "$_jni")" && pwd)"
_libpath="$_jni_dir"
if [[ "$_jni_dir" != "$_root_dir" ]]; then
  _libpath="$_jni_dir:$_root_dir"
fi

"$MVN" -q -f "$SCRIPT_DIR/pom.xml" package -DskipTests

echo "Built nativeview_jni + java-todo JARs (use the *-all* JAR so SQLite + Gson are on the classpath)."
echo "Run the app (macOS/Linux example):"
if [[ "$(uname -s)" == "Darwin" ]]; then
  echo "  java -XstartOnFirstThread --enable-native-access=ALL-UNNAMED -Djava.library.path='$_libpath' -jar \"$SCRIPT_DIR/target/java-todo-0.1.0-all.jar\""
  echo "  # or: java -XstartOnFirstThread --enable-native-access=ALL-UNNAMED -Djava.library.path='$_libpath' -cp \"$SCRIPT_DIR/target/java-todo-0.1.0-all.jar\" io.jamharah.todo.TodoApp"
else
  echo "  java --enable-native-access=ALL-UNNAMED -Djava.library.path='$_libpath' -jar \"$SCRIPT_DIR/target/java-todo-0.1.0-all.jar\""
  echo "  # or: java --enable-native-access=ALL-UNNAMED -Djava.library.path='$_libpath' -cp \"$SCRIPT_DIR/target/java-todo-0.1.0-all.jar\" io.jamharah.todo.TodoApp"
fi
echo "Optional DB path: first argument (default ./todo_app.db)."
