#!/usr/bin/env bash
# Full verification: Android Gradle builds libnativeview.so via CMake, unit tests + release packaging, desktop Java tests.
#
# Requires:
#   - JDK 17–21 recommended for the Gradle daemon (JDK 26 hosts: use JAVA_HOME pointing at JDK 17+ for Gradle).
#   - ANDROID_HOME or ANDROID_SDK_ROOT (Android SDK with platform 34 + build-tools + NDK matching bindings/android ndkVersion).
#
# Usage (from repo root):
#   bindings/android/scripts/package_release.sh
#
# SPDX-License-Identifier: Apache-2.0
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
ANDROID_DIR="${ROOT}/bindings/android"
JAVA_DIR="${ROOT}/bindings/java"

if [[ -z "${ANDROID_HOME:-}" && -z "${ANDROID_SDK_ROOT:-}" ]]; then
  if [[ -d "${HOME}/Library/Android/sdk" ]]; then
    export ANDROID_HOME="${HOME}/Library/Android/sdk"
  elif [[ -d "/opt/android-sdk" ]]; then
    export ANDROID_HOME="/opt/android-sdk"
  fi
fi
if [[ -z "${ANDROID_HOME:-}" ]]; then
  echo "Set ANDROID_HOME or ANDROID_SDK_ROOT to your Android SDK." >&2
  exit 1
fi

JAVA_BIN="java"
if [[ -n "${JAVA_HOME:-}" ]]; then
  JAVA_BIN="${JAVA_HOME}/bin/java"
fi
JAVA_MAJOR="$("${JAVA_BIN}" -version 2>&1 | head -1 | sed -E 's/.* version "([0-9]+).*/\1/')"
if [[ "${JAVA_MAJOR}" -gt 21 ]]; then
  echo "Warning: Gradle Groovy may not support running on JDK ${JAVA_MAJOR}. Use JAVA_HOME to JDK 17–21 for ./gradlew." >&2
fi

cd "${ANDROID_DIR}"
./gradlew clean assembleRelease packageNativeviewRelease test --no-daemon

cd "${JAVA_DIR}"
mvn test

echo "Release layout: ${ANDROID_DIR}/build/distributions/"
ls -la "${ANDROID_DIR}/build/distributions/"/*/ 2>/dev/null || true

echo "After committing, tag to match bindings/java/pom.xml version, e.g.:"
echo "  git tag -a v0.1.0 -m \"nativeview 0.1.0 (Android + Java bindings)\""
