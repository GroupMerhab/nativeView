#!/usr/bin/env bash
# Static-link nativeview archives into the Nim minimal sample (no -d:nativeviewShared).
# Run from examples/nim.
#
# macOS note: linking succeeds, but a normal Nim main can still hit AppKit
# EXC_BAD_INSTRUCTION before the window appears (same class of issue as FPC).
# Use -d:nativeviewShared + libnativeview.dylib for macOS GUI (see README.md).
# This script still builds archives and links so you can experiment.
set -euo pipefail
SCRIPT_DIR="$(CDPATH= cd -- "$(dirname "$0")" && pwd)"
REPO_ROOT="$(CDPATH= cd -- "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="${NV_CMAKE_BUILD_DIR:-$REPO_ROOT/build-nim-static}"
NIM="${NIM:-nim}"
MIN_OS="${CMAKE_OSX_DEPLOYMENT_TARGET:-11.0}"

cmake -S "$REPO_ROOT" -B "$BUILD_DIR" \
  -DNV_BUILD_SHARED=OFF \
  -DNV_BUILD_TESTS=OFF \
  -DNV_ENFORCE_FILE_LIMITS=OFF \
  -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-Release}" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET="$MIN_OS"

case "$(uname -s)" in
Linux)
  cmake --build "$BUILD_DIR" --target nv-core nv-ipc nv-ops nv-runtime nv-platform-linux -j"${NV_JOBS:-8}"
  NV_RUNTIME="$BUILD_DIR/modules/nv-runtime/libnv-runtime.a"
  NV_PLAT="$BUILD_DIR/modules/nv-platform-linux/libnv-platform-linux.a"
  NV_OPS="$BUILD_DIR/modules/nv-ops/libnv-ops.a"
  NV_IPC="$BUILD_DIR/modules/nv-ipc/libnv-ipc.a"
  NV_CORE="$BUILD_DIR/modules/nv-core/libnv-core.a"
  PKG_LIBS="$(pkg-config --libs webkit2gtk-4.1)"
  for pkg in ayatana-appindicator3-1 ayatana-appindicator-0.1 appindicator3-0.1 libnotify x11; do
    if pkg-config --exists "$pkg" 2>/dev/null; then
      PKG_LIBS+=" $(pkg-config --libs "$pkg")"
    fi
  done
  GROUP="-Wl,--start-group $NV_RUNTIME $NV_PLAT $NV_OPS $NV_IPC $NV_CORE -Wl,--end-group"
  cd "$SCRIPT_DIR"
  "$NIM" c -d:release --path:"$REPO_ROOT/bindings/nim" \
    --passL:"$GROUP" \
    --passL:"$PKG_LIBS" \
    minimal.nim
  echo "Built: $SCRIPT_DIR/minimal (static nativeview, no -d:nativeviewShared)"
  ;;
Darwin)
  cmake --build "$BUILD_DIR" --target nv-core nv-ipc nv-ops nv-runtime nv-platform-mac -j"${NV_JOBS:-8}"
  NV_PLAT="$BUILD_DIR/modules/nv-platform-mac/libnv-platform-mac.a"
  NV_RUNTIME="$BUILD_DIR/modules/nv-runtime/libnv-runtime.a"
  NV_OPS="$BUILD_DIR/modules/nv-ops/libnv-ops.a"
  NV_IPC="$BUILD_DIR/modules/nv-ipc/libnv-ipc.a"
  NV_CORE="$BUILD_DIR/modules/nv-core/libnv-core.a"
  cd "$SCRIPT_DIR"
  "$NIM" c -d:release --path:"$REPO_ROOT/bindings/nim" \
    --passL:"$NV_PLAT" \
    --passL:"$NV_RUNTIME" \
    --passL:"$NV_OPS" \
    --passL:"$NV_IPC" \
    --passL:"$NV_CORE" \
    --passL:"-framework Carbon -framework Cocoa -framework CoreServices -framework WebKit -framework UserNotifications -lobjc" \
    minimal.nim
  echo "Built: $SCRIPT_DIR/minimal (static nativeview, no -d:nativeviewShared)"
  echo "On macOS, if the window crashes at launch, use shared + -d:nativeviewShared (see README.md)."
  ;;
*)
  echo "Unsupported host for this script: $(uname -s). See docs/Nim.md for manual --passL."
  exit 1
  ;;
esac
