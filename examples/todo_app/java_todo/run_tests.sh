#!/usr/bin/env bash
# Run Java unit tests (Maven). Requires Maven on PATH or set MVN=/path/to/mvn.
set -euo pipefail
SCRIPT_DIR="$(CDPATH= cd -- "$(dirname "$0")" && pwd)"
MVN="${MVN:-mvn}"
if ! command -v "$MVN" >/dev/null 2>&1; then
  echo "Maven not found (set MVN=/path/to/mvn or install Maven)." >&2
  exit 1
fi
cd "$SCRIPT_DIR"
"$MVN" -q test
echo "java_todo tests: OK"
