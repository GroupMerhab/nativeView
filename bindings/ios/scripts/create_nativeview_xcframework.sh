#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
#
# Creates NativeView.xcframework from libs/*/libnativeview.a (device + merged
# simulator). Headers: full repo include/ + NativeViewC.h.
#
# Usage:
#   ./create_nativeview_xcframework.sh              # expects libs populated (see build_ios_static_libs.sh)
#   ./create_nativeview_xcframework.sh --stub       # tiny stub archives + same headers (fast SwiftPM link)
#
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
REPO="$(cd "$ROOT/../.." && pwd)"
OUT="$ROOT/NativeView.xcframework"
LIBS="$ROOT/libs"
STUB_SRC="$ROOT/stub/nv_stub.c"
IOS_SDK="$(xcrun --sdk iphoneos --show-sdk-path)"
SIM_SDK="$(xcrun --sdk iphonesimulator --show-sdk-path)"

stage_headers() {
  local dest=$1
  rm -rf "$dest"
  mkdir -p "$dest"
  rsync -a --delete "$REPO/include/" "$dest/"
  cp "$ROOT/Headers/NativeViewC.h" "$dest/NativeViewC.h"
  cp "$ROOT/Headers/module.modulemap" "$dest/module.modulemap"
}

run_create() {
  local device_a=$1
  local sim_a=$2
  local hdr_dir=$3
  rm -rf "$OUT"
  xcodebuild -create-xcframework \
    -library "$device_a" -headers "$hdr_dir" \
    -library "$sim_a" -headers "$hdr_dir" \
    -output "$OUT"
  echo "Wrote $OUT"
}

if [[ "${1:-}" == "--stub" ]]; then
  WORK="$(mktemp -d)"
  trap 'rm -rf "$WORK"' EXIT
  HDR_STAGE="$WORK/headers"
  stage_headers "$HDR_STAGE"
  clang -c "$STUB_SRC" -I"$REPO/include" -arch arm64 -isysroot "$IOS_SDK" \
    -miphoneos-version-min=14.0 -o "$WORK/nv_ios.o"
  clang -c "$STUB_SRC" -I"$REPO/include" -arch arm64 -isysroot "$SIM_SDK" \
    -mios-simulator-version-min=14.0 -o "$WORK/nv_sim_arm64.o"
  clang -c "$STUB_SRC" -I"$REPO/include" -arch x86_64 -isysroot "$SIM_SDK" \
    -mios-simulator-version-min=14.0 -o "$WORK/nv_sim_x86.o"
  mkdir -p "$WORK/device" "$WORK/simulator"
  ar rcs "$WORK/device/libnativeview.a" "$WORK/nv_ios.o"
  libtool -static -o "$WORK/simulator/libnativeview.a" "$WORK/nv_sim_arm64.o" "$WORK/nv_sim_x86.o"
  run_create "$WORK/device/libnativeview.a" "$WORK/simulator/libnativeview.a" "$HDR_STAGE"
  exit 0
fi

DEVICE_A="$LIBS/arm64/libnativeview.a"
X86_SIM="$LIBS/x86_64-simulator/libnativeview.a"
ARM64_SIM="$LIBS/arm64-simulator/libnativeview.a"

if [[ ! -f "$DEVICE_A" ]]; then
  echo "error: missing $DEVICE_A (run scripts/build_ios_static_libs.sh first)" >&2
  exit 1
fi

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
SIM_OUT="$WORK/libnativeview-simulator.a"

HDR_STAGE="$WORK/headers"
stage_headers "$HDR_STAGE"

if [[ -f "$ARM64_SIM" && -f "$X86_SIM" ]]; then
  lipo -create "$X86_SIM" "$ARM64_SIM" -output "$SIM_OUT"
elif [[ -f "$ARM64_SIM" ]]; then
  cp "$ARM64_SIM" "$SIM_OUT"
elif [[ -f "$X86_SIM" ]]; then
  cp "$X86_SIM" "$SIM_OUT"
else
  echo "error: need at least one simulator lib under $LIBS/*-simulator/" >&2
  exit 1
fi

mkdir -p "$WORK/pack-device" "$WORK/pack-sim"
cp "$DEVICE_A" "$WORK/pack-device/libnativeview.a"
cp "$SIM_OUT" "$WORK/pack-sim/libnativeview.a"

run_create "$WORK/pack-device/libnativeview.a" "$WORK/pack-sim/libnativeview.a" "$HDR_STAGE"
