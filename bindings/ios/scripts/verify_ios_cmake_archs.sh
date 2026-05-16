#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
#
# Verifies the nativeview C stack configures and builds for:
#   - iphoneos + arm64 (device)
#   - iphonesimulator + x86_64 (Intel simulator)
#   - iphonesimulator + arm64 (Apple Silicon simulator)
#
# Builds target: nv-platform-ios (pulls nv-runtime, nv-ops, nv-ipc, nv-core).
set -euo pipefail

REPO="$(cd "$(dirname "$0")/../../.." && pwd)"

verify_one() {
  local sysroot=$1
  local arch=$2
  local label=$3
  echo "=== verify: $label ($sysroot / $arch) ==="
  local work
  work="$(mktemp -d)"
  trap 'rm -rf "$work"' RETURN
  cmake -S "$REPO" -B "$work" -G Xcode \
    -DCMAKE_SYSTEM_NAME=iOS \
    -DCMAKE_OSX_SYSROOT="$sysroot" \
    -DCMAKE_OSX_ARCHITECTURES="$arch" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 \
    -DNV_BUILD_TESTS=OFF \
    -DNV_BUILD_UI=OFF \
    -DNV_BUILD_SHARED=OFF
  cmake --build "$work" --config Release --target nv-platform-ios
}

verify_one iphoneos arm64 "device arm64"
verify_one iphonesimulator x86_64 "simulator x86_64"
verify_one iphonesimulator arm64 "simulator arm64"
echo "All iOS CMake architecture smoke checks passed."
