#!/bin/sh
# Static-link nativeview .a archives into a pure FPC nv_minimal (no NATIVEVIEW_SHARED).
# Run from examples/pascal.
#
# macOS note: linking succeeds, but a normal FPC "program" entry can still hit
# AppKit EXC_BAD_INSTRUCTION before the window appears (same as with a dylib).
# Use ./build_example.sh (Clang main + Pascal dylib) for a working macOS GUI.
# This script is mainly for Linux or for experimenting / future FPC fixes.
set -e
SCRIPT_DIR="$(CDPATH= cd -- "$(dirname "$0")" && pwd)"
REPO_ROOT="$(CDPATH= cd -- "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="${NV_CMAKE_BUILD_STATIC_DIR:-$REPO_ROOT/build-pascal-static}"
FPC="${FPC:-fpc}"
MIN_OS="${CMAKE_OSX_DEPLOYMENT_TARGET:-11.0}"

cmake -S "$REPO_ROOT" -B "$BUILD_DIR" \
  -DNV_BUILD_SHARED=OFF \
  -DNV_BUILD_TESTS=OFF \
  -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-Release}" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET="$MIN_OS"
cmake --build "$BUILD_DIR" --target nv-platform-mac -j"${NV_JOBS:-8}" >/dev/null

cd "$SCRIPT_DIR"
PAS_FU="-Fu$REPO_ROOT/bindings/pascal"
# Same archive order as CMake target hello (root CMakeLists.txt).
NV_A="$BUILD_DIR/modules/nv-platform-mac/libnv-platform-mac.a"
NV_A="$NV_A $BUILD_DIR/modules/nv-runtime/libnv-runtime.a"
NV_A="$NV_A $BUILD_DIR/modules/nv-ops/libnv-ops.a"
NV_A="$NV_A $BUILD_DIR/modules/nv-ipc/libnv-ipc.a"
NV_A="$NV_A $BUILD_DIR/modules/nv-core/libnv-core.a"

PAS_LIBS=""
for a in $NV_A; do
  PAS_LIBS="$PAS_LIBS -k$a"
done

case "$(uname -s)" in
Darwin)
  "$FPC" $PAS_FU $PAS_LIBS \
    -k-framework -kCarbon \
    -k-framework -kCocoa \
    -k-framework -kCoreServices \
    -k-framework -kWebKit \
    -k-framework -kUserNotifications \
    -k-lobjc \
    -onv_minimal_static \
    nv_minimal.lpr
  echo "Built: $SCRIPT_DIR/nv_minimal_static (static nativeview, no -dNATIVEVIEW_SHARED)"
  echo "On macOS, if the window crashes at launch, use ./build_example.sh instead."
  ;;
*)
  "$FPC" $PAS_FU $PAS_LIBS \
    -onv_minimal_static \
    nv_minimal.lpr
  echo "Built: $SCRIPT_DIR/nv_minimal_static"
  echo "You may need extra -k flags for WebKitGTK on Linux (see docs/Pascal.md)."
  ;;
esac
