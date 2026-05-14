#!/usr/bin/env bash
# Build nativeview_shared and run the Python minimal sample (ctypes).
# Run from examples/python.
#
# macOS: prefer this shared path for GUI (see docs/Python.md).
set -euo pipefail
SCRIPT_DIR="$(CDPATH= cd -- "$(dirname "$0")" && pwd)"
REPO_ROOT="$(CDPATH= cd -- "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="${NV_CMAKE_BUILD_DIR:-$REPO_ROOT/build-python-shared}"
MIN_OS="${CMAKE_OSX_DEPLOYMENT_TARGET:-11.0}"

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

echo "Using NATIVEVIEW_LIB=$NATIVEVIEW_LIB"
python3 "$SCRIPT_DIR/minimal.py"
