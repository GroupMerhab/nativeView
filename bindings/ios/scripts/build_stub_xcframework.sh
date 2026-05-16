#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Deprecated: use scripts/create_nativeview_xcframework.sh --stub instead.
# Builds NativeView.xcframework (stub archives + full public headers + module map).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
exec "$ROOT/scripts/create_nativeview_xcframework.sh" --stub
