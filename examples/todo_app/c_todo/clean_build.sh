#!/usr/bin/env bash
# Clean configure + build todo_app from repo root (default build dir: examples/todo_app/c_todo/build).
set -euo pipefail
SCRIPT_DIR="$(CDPATH= cd -- "$(dirname "$0")" && pwd)"
REPO_ROOT="$(CDPATH= cd -- "$SCRIPT_DIR/../../.." && pwd)"
BUILD_DIR="${TODO_BUILD_DIR:-$SCRIPT_DIR/build}"
EXTRA=("$@")
rm -rf "$BUILD_DIR"
cmake -S "$REPO_ROOT" -B "$BUILD_DIR" \
  -DNV_BUILD_UI=ON \
  -DNV_BUILD_TESTS=ON \
  -DNV_ENFORCE_FILE_LIMITS=OFF \
  -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-Release}" \
  "${EXTRA[@]}"
cmake --build "$BUILD_DIR" --target todo_app test_todo_store test_todo_bridge -j"${NV_JOBS:-$(nproc)}"
echo "Built: $BUILD_DIR/examples/todo_app/c_todo/todo_app"
