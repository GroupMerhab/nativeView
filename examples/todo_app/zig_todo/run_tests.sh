#!/usr/bin/env bash
# Run Zig unit tests (store + bridge); no nativeview required.
set -euo pipefail
SCRIPT_DIR="$(CDPATH= cd -- "$(dirname "$0")" && pwd)"
ZIG="${ZIG:-zig}"
cd "$SCRIPT_DIR"
SQLITE_CFLAGS="${SQLITE_CFLAGS:-}"
if [[ -z "$SQLITE_CFLAGS" ]] && pkg-config --exists sqlite3 2>/dev/null; then
  SQLITE_CFLAGS="$(pkg-config --cflags-only-I sqlite3 2>/dev/null || true)"
fi
"$ZIG" build-exe $SQLITE_CFLAGS --dep ts -Mroot=tests/test_todo_store.zig -Mts=todo_store.zig -lc -lsqlite3 -ODebug -femit-bin="$SCRIPT_DIR/test_todo_store"
"$SCRIPT_DIR/test_todo_store"
"$ZIG" build-exe $SQLITE_CFLAGS --dep ts --dep br -Mroot=tests/test_todo_bridge.zig -Mts=todo_store.zig --dep ts -Mbr=todo_bridge.zig -lc -lsqlite3 -ODebug -femit-bin="$SCRIPT_DIR/test_todo_bridge"
"$SCRIPT_DIR/test_todo_bridge"
echo "zig_todo tests: OK"
