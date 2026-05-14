#!/usr/bin/env bash
# Run Nim unit tests (requires: nimble install db_connector).
set -euo pipefail
SCRIPT_DIR="$(CDPATH= cd -- "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"
nimble install -y db_connector >/dev/null
nim r --path:. tests/test_todo_store.nim
nim r --path:. tests/test_todo_bridge.nim
echo "nim_todo tests: OK"
