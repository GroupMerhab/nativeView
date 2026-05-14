#!/usr/bin/env bash
# Run Rust unit / integration tests (no nativeview link required).
set -euo pipefail
SCRIPT_DIR="$(CDPATH= cd -- "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"
CARGO="${CARGO:-cargo}"
"$CARGO" test
echo "rust_todo tests: OK"
