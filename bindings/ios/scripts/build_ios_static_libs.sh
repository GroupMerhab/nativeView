#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
#
# Builds merged libnativeview.a per iOS slice via CMake (Xcode generator) and
# writes:
#   libs/arm64/libnativeview.a              (iphoneos, arm64)
#   libs/x86_64-simulator/libnativeview.a   (iphonesimulator, x86_64)
#   libs/arm64-simulator/libnativeview.a    (iphonesimulator, arm64)
#
# Requires: Xcode CLT, CMake 3.16+.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
REPO="$(cd "$ROOT/../.." && pwd)"
LIBS="$ROOT/libs"
NV_BUILD_TESTS=OFF
NV_BUILD_UI=OFF
NV_BUILD_SHARED=OFF

merge_archives() {
  local build_dir=$1
  local rel=$2
  local out=$3
  local base="$build_dir/modules"
  libtool -static -o "$out" \
    "$base/nv-platform-ios/$rel/libnv-platform-ios.a" \
    "$base/nv-runtime/$rel/libnv-runtime.a" \
    "$base/nv-ops/$rel/libnv-ops.a" \
    "$base/nv-ipc/$rel/libnv-ipc.a" \
    "$base/nv-core/$rel/libnv-core.a"
}

build_slice() {
  local sysroot_name=$1
  local arch=$2
  local rel=$3
  local out_a=$4
  local work
  work="$(mktemp -d)"
  trap 'rm -rf "$work"' RETURN

  cmake -S "$REPO" -B "$work" -G Xcode \
    -DCMAKE_SYSTEM_NAME=iOS \
    -DCMAKE_OSX_SYSROOT="$sysroot_name" \
    -DCMAKE_OSX_ARCHITECTURES="$arch" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 \
    -DNV_BUILD_TESTS="$NV_BUILD_TESTS" \
    -DNV_BUILD_UI="$NV_BUILD_UI" \
    -DNV_BUILD_SHARED="$NV_BUILD_SHARED"

  cmake --build "$work" --config Release --target nv-platform-ios
  merge_archives "$work" "$rel" "$out_a"
}

mkdir -p "$LIBS/arm64" "$LIBS/x86_64-simulator" "$LIBS/arm64-simulator"

echo "=== iphoneos / arm64 ==="
build_slice iphoneos arm64 Release-iphoneos "$LIBS/arm64/libnativeview.a"

echo "=== iphonesimulator / x86_64 ==="
build_slice iphonesimulator x86_64 Release-iphonesimulator "$LIBS/x86_64-simulator/libnativeview.a"

echo "=== iphonesimulator / arm64 ==="
build_slice iphonesimulator arm64 Release-iphonesimulator "$LIBS/arm64-simulator/libnativeview.a"

echo "Wrote:"
ls -la "$LIBS/arm64/libnativeview.a" "$LIBS/x86_64-simulator/libnativeview.a" "$LIBS/arm64-simulator/libnativeview.a"
