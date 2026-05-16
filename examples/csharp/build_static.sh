#!/usr/bin/env bash
# Static-link nativeview archives into the C# minimal sample (no NATIVEVIEW_SHARED).
# Run from examples/csharp.
#
# SPDX-License-Identifier: Apache-2.0
set -euo pipefail
SCRIPT_DIR="$(CDPATH= cd -- "$(dirname "$0")" && pwd)"
REPO_ROOT="$(CDPATH= cd -- "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="${NV_CMAKE_BUILD_DIR:-$REPO_ROOT/build-csharp-static}"
CMAKE_CONFIG="${CMAKE_BUILD_TYPE:-Release}"
MIN_OS="${CMAKE_OSX_DEPLOYMENT_TARGET:-11.0}"
PROPS="$SCRIPT_DIR/NativeView.Static.props"

cmake -S "$REPO_ROOT" -B "$BUILD_DIR" \
  -DNV_BUILD_SHARED=OFF \
  -DNV_BUILD_TESTS=OFF \
  -DNV_ENFORCE_FILE_LIMITS=OFF \
  -DCMAKE_BUILD_TYPE="$CMAKE_CONFIG" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET="$MIN_OS"

write_props_header() {
  cat >"$PROPS" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<Project>
  <PropertyGroup>
    <NV_CMAKE_BUILD>$BUILD_DIR</NV_CMAKE_BUILD>
    <NativeViewCMakeConfig>$CMAKE_CONFIG</NativeViewCMakeConfig>
  </PropertyGroup>
  <ItemGroup>
EOF
}

write_props_footer() {
  echo '  </ItemGroup>' >>"$PROPS"
  echo '</Project>' >>"$PROPS"
}

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
  write_props_header
  {
    echo '    <LinkerArg Include="-Wl,--start-group" />'
    for lib in "$NV_RUNTIME" "$NV_PLAT" "$NV_OPS" "$NV_IPC" "$NV_CORE"; do
      echo "    <NativeLibrary Include=\"$lib\" />"
    done
    echo '    <LinkerArg Include="-Wl,--end-group" />'
    for tok in $PKG_LIBS; do
      echo "    <LinkerArg Include=\"$tok\" />"
    done
  } >>"$PROPS"
  write_props_footer
  RID=linux-x64
  ;;
Darwin)
  cmake --build "$BUILD_DIR" --target nv-core nv-ipc nv-ops nv-runtime nv-platform-mac -j"${NV_JOBS:-8}"
  write_props_header
  for lib in \
    "$BUILD_DIR/modules/nv-platform-mac/libnv-platform-mac.a" \
    "$BUILD_DIR/modules/nv-runtime/libnv-runtime.a" \
    "$BUILD_DIR/modules/nv-ops/libnv-ops.a" \
    "$BUILD_DIR/modules/nv-ipc/libnv-ipc.a" \
    "$BUILD_DIR/modules/nv-core/libnv-core.a"; do
    echo "    <NativeLibrary Include=\"$lib\" />" >>"$PROPS"
  done
  for fw in Carbon Cocoa CoreServices WebKit UserNotifications; do
    echo "    <LinkerArg Include=\"-framework\" />" >>"$PROPS"
    echo "    <LinkerArg Include=\"$fw\" />" >>"$PROPS"
  done
  echo '    <LinkerArg Include="-lobjc" />' >>"$PROPS"
  write_props_footer
  RID=osx-$(uname -m)
  ;;
*)
  echo "Unsupported host: $(uname -s). See docs/CSharp.md."
  exit 1
  ;;
esac

cd "$SCRIPT_DIR"
dotnet publish Minimal/Minimal.csproj -c Release -r "$RID" --self-contained false
echo "Built under: $SCRIPT_DIR/Minimal/bin/Release/net8.0/$RID/publish/"
