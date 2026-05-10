#!/bin/sh
# Build the Pascal + nativeview sample (shared lib). Run from examples/pascal.
set -e
SCRIPT_DIR="$(CDPATH= cd -- "$(dirname "$0")" && pwd)"
REPO_ROOT="$(CDPATH= cd -- "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="${NV_CMAKE_BUILD_DIR:-$REPO_ROOT/build-pascal-example}"
FPC="${FPC:-fpc}"

cmake -S "$REPO_ROOT" -B "$BUILD_DIR" \
  -DNV_BUILD_SHARED=ON \
  -DNV_BUILD_TESTS=OFF \
  -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-Release}" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET="${CMAKE_OSX_DEPLOYMENT_TARGET:-11.0}"
cmake --build "$BUILD_DIR" --target nativeview_shared -j"${NV_JOBS:-8}"

cd "$SCRIPT_DIR"
PAS_FU="-Fu$REPO_ROOT/bindings/pascal"
PAS_FL="-Fl$BUILD_DIR"
PAS_LD="-k-L$BUILD_DIR -k-lnativeview"
# Embed absolute path so libnativeview resolves regardless of cwd (dev builds).
PAS_RPATH="-k-rpath -k$BUILD_DIR"

case "$(uname -s)" in
Darwin)
  "$FPC" -dNATIVEVIEW_SHARED $PAS_FU $PAS_FL $PAS_LD -k-framework -kCocoa \
    $PAS_RPATH -Xs- nv_minimal_lib.lpr
  cc -O2 -o nv_minimal_gui nv_minimal_host.c -L. -lnv_minimal_lib \
    -Wl,-dead_strip \
    -Wl,-rpath,"$BUILD_DIR" \
    -Wl,-rpath,"@loader_path" \
    -mmacos-version-min="${CMAKE_OSX_DEPLOYMENT_TARGET:-11.0}"
  echo "Built: $SCRIPT_DIR/nv_minimal_gui (and libnv_minimal_lib.dylib)"
  echo "Run from this directory: ./nv_minimal_gui"
  ;;
*)
  "$FPC" -dNATIVEVIEW_SHARED $PAS_FU $PAS_FL $PAS_LD $PAS_RPATH nv_minimal.lpr
  echo "Built: $SCRIPT_DIR/nv_minimal (pure FPC program)"
  ;;
esac
