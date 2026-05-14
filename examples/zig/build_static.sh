#!/usr/bin/env bash
# Static-link nativeview archives into the Zig minimal sample.
# Run from examples/zig.
#
# macOS note: linking succeeds, but a normal Zig main can still hit AppKit
# EXC_BAD_INSTRUCTION before the window appears (same class of issue as Nim/FPC).
# Use shared libnativeview.dylib for macOS GUI (see README.md).
set -euo pipefail
SCRIPT_DIR="$(CDPATH= cd -- "$(dirname "$0")" && pwd)"
REPO_ROOT="$(CDPATH= cd -- "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="${NV_CMAKE_BUILD_DIR:-$REPO_ROOT/build-zig-static}"
ZIG="${ZIG:-zig}"
MIN_OS="${CMAKE_OSX_DEPLOYMENT_TARGET:-11.0}"

cmake -S "$REPO_ROOT" -B "$BUILD_DIR" \
  -DNV_BUILD_SHARED=OFF \
  -DNV_BUILD_TESTS=OFF \
  -DNV_ENFORCE_FILE_LIMITS=OFF \
  -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-Release}" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET="$MIN_OS"

NV_MOD="$REPO_ROOT/bindings/zig/nativeview.zig"
ZIG_BASE=(build-exe --dep nativeview -Mroot="$SCRIPT_DIR/minimal.zig" -Mnativeview="$NV_MOD" -lc -OReleaseSmall)

case "$(uname -s)" in
Linux)
  cmake --build "$BUILD_DIR" --target nv-core nv-ipc nv-ops nv-runtime nv-platform-linux -j"${NV_JOBS:-8}"
  NV_RUNTIME="$BUILD_DIR/modules/nv-runtime/libnv-runtime.a"
  NV_PLAT="$BUILD_DIR/modules/nv-platform-linux/libnv-platform-linux.a"
  NV_OPS="$BUILD_DIR/modules/nv-ops/libnv-ops.a"
  NV_IPC="$BUILD_DIR/modules/nv-ipc/libnv-ipc.a"
  NV_CORE="$BUILD_DIR/modules/nv-core/libnv-core.a"
  PKG_LIBS="$(pkg-config --libs webkit2gtk-4.1)"
  PKG_LIBS="${PKG_LIBS//-pthread/-lpthread}"
  for pkg in ayatana-appindicator3-1 ayatana-appindicator-0.1 appindicator3-0.1 libnotify x11; do
    if pkg-config --exists "$pkg" 2>/dev/null; then
      PKG_LIBS+=" $(pkg-config --libs "$pkg")"
    fi
  done
  PKG_LIBS="${PKG_LIBS//-pthread/-lpthread}"
  cd "$SCRIPT_DIR"
  # shellcheck disable=SC2086
  "$ZIG" "${ZIG_BASE[@]}" \
    "$NV_RUNTIME" "$NV_PLAT" "$NV_OPS" "$NV_IPC" "$NV_CORE" \
    $PKG_LIBS
  echo "Built: $SCRIPT_DIR/minimal (static nativeview)"
  ;;
Darwin)
  cmake --build "$BUILD_DIR" --target nv-core nv-ipc nv-ops nv-runtime nv-platform-mac -j"${NV_JOBS:-8}"
  NV_PLAT="$BUILD_DIR/modules/nv-platform-mac/libnv-platform-mac.a"
  NV_RUNTIME="$BUILD_DIR/modules/nv-runtime/libnv-runtime.a"
  NV_OPS="$BUILD_DIR/modules/nv-ops/libnv-ops.a"
  NV_IPC="$BUILD_DIR/modules/nv-ipc/libnv-ipc.a"
  NV_CORE="$BUILD_DIR/modules/nv-core/libnv-core.a"
  cd "$SCRIPT_DIR"
  "$ZIG" "${ZIG_BASE[@]}" \
    "$NV_PLAT" \
    "$NV_RUNTIME" \
    "$NV_OPS" \
    "$NV_IPC" \
    "$NV_CORE" \
    -framework Carbon -framework Cocoa -framework CoreServices -framework WebKit -framework UserNotifications -lobjc
  echo "Built: $SCRIPT_DIR/minimal (static nativeview)"
  echo "On macOS, if the window crashes at launch, use shared + libnativeview.dylib (see README.md)."
  ;;
*)
  echo "Unsupported host for this script: $(uname -s). See docs/Zig.md for manual link flags."
  exit 1
  ;;
esac
