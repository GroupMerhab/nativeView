#!/usr/bin/env bash
# Build py_todo: optional Vue UI, embed HTML, shared nativeview for ctypes Python.
# Run from examples/todo_app/py_todo.
#
# Env:
#   NV_TODO_SKIP_UI_BUILD=ON  — embed ../ui/fallback_index.html only (no npm)
#   NV_CMAKE_BUILD_DIR        — CMake binary dir (default: repo build-python-todo-shared)
#   CMAKE_BUILD_TYPE, NV_JOBS — same spirit as examples/python/build_shared.sh
#
# After build, run with PYTHONPATH and NATIVEVIEW_LIB printed below (see README.md).
set -euo pipefail
SCRIPT_DIR="$(CDPATH= cd -- "$(dirname "$0")" && pwd)"
REPO_ROOT="$(CDPATH= cd -- "$SCRIPT_DIR/../../.." && pwd)"
BUILD_DIR="${NV_CMAKE_BUILD_DIR:-$REPO_ROOT/build-python-todo-shared}"
MIN_OS="${CMAKE_OSX_DEPLOYMENT_TARGET:-11.0}"
GEN_DIR="$SCRIPT_DIR/generated"
OUT_HTML_PY="$GEN_DIR/todo_html_embed.py"
PYTHON="${PYTHON:-python3}"

mkdir -p "$GEN_DIR"
cd "$SCRIPT_DIR"

if [[ "${NV_TODO_SKIP_UI_BUILD:-}" == "ON" ]] || [[ "${NV_TODO_SKIP_UI_BUILD:-}" == "1" ]]; then
  HTML_IN="$SCRIPT_DIR/../ui/fallback_index.html"
else
  UI_ROOT="$SCRIPT_DIR/../ui"
  (cd "$UI_ROOT" && npm install && npm run build)
  HTML_IN="$UI_ROOT/dist/index.html"
fi

"$PYTHON" "$SCRIPT_DIR/tools/embed_html_py.py" "$HTML_IN" "$OUT_HTML_PY" \
  "Embedded Vue todo UI (see examples/todo_app/ui)"

cmake -S "$REPO_ROOT" -B "$BUILD_DIR" \
  -DNV_BUILD_SHARED=ON \
  -DNV_BUILD_TESTS=OFF \
  -DNV_ENFORCE_FILE_LIMITS=OFF \
  -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-Release}" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET="$MIN_OS"

cmake --build "$BUILD_DIR" --target nativeview_shared -j"${NV_JOBS:-8}"

_lib="$(find "$BUILD_DIR" \( -name 'libnativeview.so' -o -name 'libnativeview.dylib' -o -name 'nativeview.dll' \) -print -quit)"
if [[ -z "${_lib}" ]]; then
  echo "Could not find nativeview shared library under $BUILD_DIR" >&2
  exit 1
fi

export NATIVEVIEW_LIB="$(CDPATH= cd -- "$(dirname "$_lib")" && pwd)/$(basename "$_lib")"
export PYTHONPATH="${REPO_ROOT}/bindings/python${PYTHONPATH:+:${PYTHONPATH}}"

echo "Built nativeview shared + embedded HTML."
echo "Run the app:"
echo "  export NATIVEVIEW_LIB=$NATIVEVIEW_LIB"
echo "  export PYTHONPATH=${REPO_ROOT}/bindings/python\${PYTHONPATH:+:\$PYTHONPATH}"
echo "  cd $SCRIPT_DIR && python3 todo_app.py"
