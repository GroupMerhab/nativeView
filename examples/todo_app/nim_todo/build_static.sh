#!/usr/bin/env bash
# Build nim_todo: optional Vue UI, embed HTML, static-link nativeview + SQLite.
# Run from examples/todo_app/nim_todo.
#
# Env:
#   NV_TODO_SKIP_UI_BUILD=ON  — embed ../ui/fallback_index.html only (no npm)
#   NV_CMAKE_BUILD_DIR        — CMake binary dir (default: repo build-nim-todo-static)
#   NIM, NV_JOBS, CMAKE_BUILD_TYPE — same spirit as examples/nim/build_static.sh
#
# macOS: static link may crash at launch; prefer shared nativeview (see examples/nim/README.md).
set -euo pipefail
SCRIPT_DIR="$(CDPATH= cd -- "$(dirname "$0")" && pwd)"
REPO_ROOT="$(CDPATH= cd -- "$SCRIPT_DIR/../../.." && pwd)"
BUILD_DIR="${NV_CMAKE_BUILD_DIR:-$REPO_ROOT/build-nim-todo-static}"
NIM="${NIM:-nim}"
NIMBLE="${NIMBLE:-nimble}"
MIN_OS="${CMAKE_OSX_DEPLOYMENT_TARGET:-11.0}"
GEN_DIR="$SCRIPT_DIR/generated"
OUT_HTML_NIM="$GEN_DIR/todo_html_embed.nim"
PYTHON="${PYTHON:-python3}"

mkdir -p "$GEN_DIR"
cd "$SCRIPT_DIR"
"$NIMBLE" install -y db_connector

if [[ "${NV_TODO_SKIP_UI_BUILD:-}" == "ON" ]] || [[ "${NV_TODO_SKIP_UI_BUILD:-}" == "1" ]]; then
  HTML_IN="$SCRIPT_DIR/../ui/fallback_index.html"
else
  UI_ROOT="$SCRIPT_DIR/../ui"
  (cd "$UI_ROOT" && npm install && npm run build)
  HTML_IN="$UI_ROOT/dist/index.html"
fi

"$PYTHON" "$SCRIPT_DIR/tools/embed_html_nim.py" "$HTML_IN" "$OUT_HTML_NIM" \
  "Embedded Vue todo UI (see examples/todo_app/ui)"

cmake -S "$REPO_ROOT" -B "$BUILD_DIR" \
  -DNV_BUILD_SHARED=OFF \
  -DNV_BUILD_TESTS=OFF \
  -DNV_ENFORCE_FILE_LIMITS=OFF \
  -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-Release}" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET="$MIN_OS"

SQLITE_LIBS="${SQLITE_LIBS:-}"
if [[ -z "$SQLITE_LIBS" ]] && pkg-config --exists sqlite3 2>/dev/null; then
  SQLITE_LIBS="$(pkg-config --libs sqlite3)"
fi
if [[ -z "$SQLITE_LIBS" ]]; then
  SQLITE_LIBS="-lsqlite3"
fi

case "$(uname -s)" in
Linux)
  cmake --build "$BUILD_DIR" --target nv-core nv-ipc nv-ops nv-runtime nv-platform-linux -j"${NV_JOBS:-8}"
  NV_RUNTIME="$BUILD_DIR/modules/nv-runtime/libnv-runtime.a"
  NV_PLAT="$BUILD_DIR/modules/nv-platform-linux/libnv-platform-linux.a"
  NV_OPS="$BUILD_DIR/modules/nv-ops/libnv-ops.a"
  NV_IPC="$BUILD_DIR/modules/nv-ipc/libnv-ipc.a"
  NV_CORE="$BUILD_DIR/modules/nv-core/libnv-core.a"
  PKG_LIBS="$(pkg-config --libs webkit2gtk-4.1)"
  for pkg in ayatana-appindicator3-1 ayatana-appindicator-0.1 appindicator3-0.1 libnotify x11; do
    if pkg-config --exists "$pkg" 2>/dev/null; then
      PKG_LIBS+=" $(pkg-config --libs "$pkg")"
    fi
  done
  GROUP="-Wl,--start-group $NV_RUNTIME $NV_PLAT $NV_OPS $NV_IPC $NV_CORE -Wl,--end-group"
  "$NIM" c -d:release --path:"$REPO_ROOT/bindings/nim" \
    --passL:"$GROUP" \
    --passL:"$PKG_LIBS" \
    --passL:"$SQLITE_LIBS" \
    -o:"$SCRIPT_DIR/nim_todo" \
    "$SCRIPT_DIR/todo_app.nim"
  echo "Built: $SCRIPT_DIR/nim_todo"
  ;;
Darwin)
  cmake --build "$BUILD_DIR" --target nv-core nv-ipc nv-ops nv-runtime nv-platform-mac -j"${NV_JOBS:-8}"
  NV_PLAT="$BUILD_DIR/modules/nv-platform-mac/libnv-platform-mac.a"
  NV_RUNTIME="$BUILD_DIR/modules/nv-runtime/libnv-runtime.a"
  NV_OPS="$BUILD_DIR/modules/nv-ops/libnv-ops.a"
  NV_IPC="$BUILD_DIR/modules/nv-ipc/libnv-ipc.a"
  NV_CORE="$BUILD_DIR/modules/nv-core/libnv-core.a"
  "$NIM" c -d:release --path:"$REPO_ROOT/bindings/nim" \
    --passL:"$NV_PLAT" \
    --passL:"$NV_RUNTIME" \
    --passL:"$NV_OPS" \
    --passL:"$NV_IPC" \
    --passL:"$NV_CORE" \
    --passL:"-framework Carbon -framework Cocoa -framework CoreServices -framework WebKit -framework UserNotifications -lobjc" \
    --passL:"$SQLITE_LIBS" \
    -o:"$SCRIPT_DIR/nim_todo" \
    "$SCRIPT_DIR/todo_app.nim"
  echo "Built: $SCRIPT_DIR/nim_todo"
  echo "On macOS, if the window crashes at launch, use shared nativeview (see examples/nim/README.md)."
  ;;
*)
  echo "Unsupported host for this script: $(uname -s). Adapt passL from docs/Nim.md."
  exit 1
  ;;
esac
