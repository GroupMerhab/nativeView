#!/usr/bin/env bash
# Run Swift unit tests (TodoBackend only; no nativeview link).
set -euo pipefail
SCRIPT_DIR="$(CDPATH= cd -- "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"
swift test
echo "swift_todo tests: OK"
