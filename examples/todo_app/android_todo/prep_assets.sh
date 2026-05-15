#!/usr/bin/env bash
# Copy built Vue UI (or fallback) into app/src/main/assets/todo_ui.html.
# Run from examples/todo_app/android_todo.
#
# Note: ./gradlew :app:assemble* runs the same npm step automatically (prepareTodoAppUiAssets).
# Use this script when you only want the asset file without a full Android build.
set -euo pipefail
SCRIPT_DIR="$(CDPATH= cd -- "$(dirname "$0")" && pwd)"
OUT="$SCRIPT_DIR/app/src/main/assets/todo_ui.html"
mkdir -p "$(dirname "$OUT")"

if [[ "${NV_TODO_SKIP_UI_BUILD:-}" == "ON" ]] || [[ "${NV_TODO_SKIP_UI_BUILD:-}" == "1" ]]; then
  cp -f "$SCRIPT_DIR/../ui/fallback_index.html" "$OUT"
else
  UI_ROOT="$SCRIPT_DIR/../ui"
  (cd "$UI_ROOT" && npm install && npm run build)
  cp -f "$UI_ROOT/dist/index.html" "$OUT"
fi
echo "Wrote $OUT"
