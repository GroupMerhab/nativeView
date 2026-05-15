#!/usr/bin/env bash
# Optional: build libnativeview.so for Android ABIs and copy into bindings/android jniLibs.
# Normal app/library builds use Gradle externalNativeBuild (root CMakeLists.txt); this script
# is for CI, pre-seeding jniLibs, or environments where you want standalone CMake output.
#
# Requires ANDROID_NDK_HOME or ANDROID_NDK pointing at an NDK (r26+; r27+ recommended).
# Produces 16 KB ELF-aligned libs (CMake adds -Wl,-z,max-page-size=16384 on Android).
# Usage (from repo root):
#   bindings/android/scripts/build_nativeview_so.sh
#   bindings/android/scripts/build_nativeview_so.sh arm64-v8a
#   bindings/android/scripts/build_nativeview_so.sh arm64-v8a x86_64
# Or: NV_ANDROID_ABIS="arm64-v8a" bindings/android/scripts/build_nativeview_so.sh
#
# SPDX-License-Identifier: Apache-2.0
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
OUT_BASE="${ROOT}/bindings/android/src/main/jniLibs"
NDK="${ANDROID_NDK_HOME:-${ANDROID_NDK:-}}"
if [[ -z "${NDK}" ]]; then
  echo "Set ANDROID_NDK_HOME or ANDROID_NDK to your Android NDK path." >&2
  exit 1
fi
TOOLCHAIN="${NDK}/build/cmake/android.toolchain.cmake"
if [[ ! -f "${TOOLCHAIN}" ]]; then
  echo "Missing cmake toolchain: ${TOOLCHAIN}" >&2
  exit 1
fi

if [[ "${#}" -gt 0 ]]; then
  ABIS=("$@")
elif [[ -n "${NV_ANDROID_ABIS:-}" ]]; then
  # shellcheck disable=SC2206
  ABIS=(${NV_ANDROID_ABIS})
else
  ABIS=(arm64-v8a x86_64)
fi

_nv_abi_ok() {
  case "$1" in
    arm64-v8a | armeabi-v7a | x86_64 | x86) return 0 ;;
    *)
      echo "Unsupported ABI: $1 (expected arm64-v8a, armeabi-v7a, x86_64, or x86)" >&2
      return 1
      ;;
  esac
}

for ABI in "${ABIS[@]}"; do
  _nv_abi_ok "${ABI}" || exit 1
done

for ABI in "${ABIS[@]}"; do
  BUILD_DIR="${ROOT}/bindings/android/.cmake-nativeview/${ABI}"
  cmake -S "${ROOT}" -B "${BUILD_DIR}" \
    -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN}" \
    -DANDROID_ABI="${ABI}" \
    -DANDROID_PLATFORM=android-24 \
    -DNV_BUILD_SHARED=ON \
    -DNV_BUILD_TESTS=OFF \
    -DNV_BUILD_UI=OFF
  cmake --build "${BUILD_DIR}" --target nativeview_shared -j"$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"
  mkdir -p "${OUT_BASE}/${ABI}"
  cp -f "${BUILD_DIR}/libnativeview.so" "${OUT_BASE}/${ABI}/libnativeview.so"
  echo "Installed ${OUT_BASE}/${ABI}/libnativeview.so"
done
