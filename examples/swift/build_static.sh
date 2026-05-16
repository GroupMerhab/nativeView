#!/usr/bin/env bash
# Build the Swift minimal sample with statically linked nativeview (no shared dylib).
# Run from examples/swift. Requires Swift 5.9+.
set -euo pipefail
SCRIPT_DIR="$(CDPATH= cd -- "$(dirname "$0")" && pwd)"
REPO_ROOT="$(CDPATH= cd -- "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="${NV_CMAKE_BUILD_DIR:-$REPO_ROOT/build-swift-static}"
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
  cd "$SCRIPT_DIR/minimal"
  LINK_FLAGS=(
    -Xlinker -Wl -Xlinker --start-group
    -Xlinker "$NV_RUNTIME"
    -Xlinker "$NV_PLAT"
    -Xlinker "$NV_OPS"
    -Xlinker "$NV_IPC"
    -Xlinker "$NV_CORE"
    -Xlinker -Wl -Xlinker --end-group
  )
  # shellcheck disable=SC2086
  for word in $PKG_LIBS; do
    LINK_FLAGS+=( -Xlinker "$word" )
  done
  swift build -c release "${LINK_FLAGS[@]}"
  echo "Built: $SCRIPT_DIR/minimal/.build/release/minimal"
  ;;
Darwin)
  cmake --build "$BUILD_DIR" --target nv-core nv-ipc nv-ops nv-runtime nv-platform-mac -j"${NV_JOBS:-8}"
  NV_PLAT="$BUILD_DIR/modules/nv-platform-mac/libnv-platform-mac.a"
  NV_RUNTIME="$BUILD_DIR/modules/nv-runtime/libnv-runtime.a"
  NV_OPS="$BUILD_DIR/modules/nv-ops/libnv-ops.a"
  NV_IPC="$BUILD_DIR/modules/nv-ipc/libnv-ipc.a"
  NV_CORE="$BUILD_DIR/modules/nv-core/libnv-core.a"
  cd "$SCRIPT_DIR/minimal"
  swift build -c release \
    -Xlinker "$NV_PLAT" \
    -Xlinker "$NV_RUNTIME" \
    -Xlinker "$NV_OPS" \
    -Xlinker "$NV_IPC" \
    -Xlinker "$NV_CORE" \
    -Xlinker -framework -Xlinker Carbon \
    -Xlinker -framework -Xlinker Cocoa \
    -Xlinker -framework -Xlinker CoreServices \
    -Xlinker -framework -Xlinker WebKit \
    -Xlinker -framework -Xlinker UserNotifications \
    -Xlinker -lobjc
  echo "Built: $SCRIPT_DIR/minimal/.build/release/minimal"
  ;;
*)
  echo "Unsupported host for this script: $(uname -s). See docs/Swift.md for manual link flags."
  exit 1
  ;;
esac
