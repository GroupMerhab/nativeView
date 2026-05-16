#!/usr/bin/env bash
# Run C# unit tests (no nativeview link required).
set -euo pipefail
SCRIPT_DIR="$(CDPATH= cd -- "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

GEN="$SCRIPT_DIR/generated/TodoHtmlEmbed.cs"
if [[ ! -f "$GEN" ]]; then
  PYTHON="${PYTHON:-python3}"
  HTML_IN="$SCRIPT_DIR/../ui/fallback_index.html"
  mkdir -p "$SCRIPT_DIR/generated"
  "$PYTHON" "$SCRIPT_DIR/tools/embed_html_cs.py" "$HTML_IN" "$GEN" \
    "Embedded Vue todo UI (fallback for tests)"
fi

dotnet test tests/TodoApp.Tests.csproj -c Release --no-restore 2>/dev/null || dotnet test tests/TodoApp.Tests.csproj -c Release
echo "csharp_todo tests: OK"
