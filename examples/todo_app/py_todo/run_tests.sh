#!/usr/bin/env bash
# Run Python unit tests for py_todo (stdlib sqlite3; no nativeview required).
set -euo pipefail
SCRIPT_DIR="$(CDPATH= cd -- "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"
PYTHON="${PYTHON:-python3}"
export PYTHONPATH="$SCRIPT_DIR${PYTHONPATH:+:${PYTHONPATH}}"
"$PYTHON" tests/test_todo_store.py
"$PYTHON" tests/test_todo_bridge.py
echo "py_todo tests: OK"
