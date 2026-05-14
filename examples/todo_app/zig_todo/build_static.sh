#!/usr/bin/env bash
# Build zig_todo: optional Vue UI, embed HTML, static-link nativeview + SQLite.
# Run from examples/todo_app/zig_todo.
#
# Env:
#   NV_TODO_SKIP_UI_BUILD=ON  — embed ../ui/fallback_index.html only (no npm)
#   NV_CMAKE_BUILD_DIR        — CMake binary dir (default: repo build-zig-todo-static)
#   NV_JOBS, CMAKE_BUILD_TYPE, ZIG — same spirit as examples/zig/build_static.sh
set -euo pipefail
SCRIPT_DIR="$(CDPATH= cd -- "$(dirname "$0")" && pwd)"
REPO_ROOT="$(CDPATH= cd -- "$SCRIPT_DIR/../../.." && pwd)"
BUILD_DIR="${NV_CMAKE_BUILD_DIR:-$REPO_ROOT/build-zig-todo-static}"
ZIG="${ZIG:-zig}"
if ! command -v "$ZIG" >/dev/null 2>&1; then
  for cand in "${HOME}/.local/bin/zig" "${HOME}/zig/zig" "/opt/homebrew/bin/zig" "/usr/local/bin/zig" "/snap/bin/zig"; do
    if [[ -x "$cand" ]]; then
      ZIG="$cand"
      break
    fi
  done
fi
if ! command -v "$ZIG" >/dev/null 2>&1; then
  echo "build_static.sh: Zig not found (tried PATH and common install paths)." >&2
  echo "  Install from https://ziglang.org/download , add zig to PATH, or set ZIG to the executable." >&2
  exit 127
fi
MIN_OS="${CMAKE_OSX_DEPLOYMENT_TARGET:-11.0}"
GEN_DIR="$SCRIPT_DIR/generated"
OUT_HTML_ZIG="$GEN_DIR/todo_html_embed.zig"
PYTHON="${PYTHON:-python3}"
NV_MOD="$REPO_ROOT/bindings/zig/nativeview.zig"

mkdir -p "$GEN_DIR"
cd "$SCRIPT_DIR"

if [[ "${NV_TODO_SKIP_UI_BUILD:-}" == "ON" ]] || [[ "${NV_TODO_SKIP_UI_BUILD:-}" == "1" ]]; then
  HTML_IN="$SCRIPT_DIR/../ui/fallback_index.html"
else
  UI_ROOT="$SCRIPT_DIR/../ui"
  (cd "$UI_ROOT" && npm install && npm run build)
  HTML_IN="$UI_ROOT/dist/index.html"
fi

"$PYTHON" "$SCRIPT_DIR/tools/embed_html_zig.py" "$HTML_IN" "$OUT_HTML_ZIG" \
  "Embedded Vue todo UI (see examples/todo_app/ui)"

cmake -S "$REPO_ROOT" -B "$BUILD_DIR" \
  -DNV_BUILD_SHARED=OFF \
  -DNV_BUILD_TESTS=OFF \
  -DNV_ENFORCE_FILE_LIMITS=OFF \
  -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-Release}" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET="$MIN_OS"

SQLITE_LIBS="${SQLITE_LIBS:-}"
SQLITE_CFLAGS="${SQLITE_CFLAGS:-}"
if [[ -z "$SQLITE_LIBS" ]] && pkg-config --exists sqlite3 2>/dev/null; then
  SQLITE_LIBS="$(pkg-config --libs sqlite3)"
fi
if [[ -z "$SQLITE_LIBS" ]]; then
  SQLITE_LIBS="-lsqlite3"
fi
if [[ -z "$SQLITE_CFLAGS" ]] && pkg-config --exists sqlite3 2>/dev/null; then
  SQLITE_CFLAGS="$(pkg-config --cflags-only-I sqlite3 2>/dev/null || true)"
fi

ZIG_ROOT=(
  build-exe
  $SQLITE_CFLAGS
  --dep nv --dep ts --dep br --dep embed
  -Mroot="$SCRIPT_DIR/todo_app.zig"
  -Mnv="$NV_MOD"
  -Mts="$SCRIPT_DIR/todo_store.zig"
  --dep ts
  -Mbr="$SCRIPT_DIR/todo_bridge.zig"
  -Membed="$OUT_HTML_ZIG"
  -lc
  -OReleaseSmall
  -fstrip
  -femit-bin="$SCRIPT_DIR/zig_todo"
)
# Zig is the final link driver; CMake's Release linker flags do not apply here.
# Strip Zig's debug symbols and GC unreachable sections from static archives.
case "$(uname -s)" in
Darwin) ZIG_ROOT+=(-dead_strip) ;;
Linux) ZIG_ROOT+=(--gc-sections) ;;
esac

case "$(uname -s)" in
Linux)
  cmake --build "$BUILD_DIR" --target nv-core nv-ipc nv-ops nv-runtime nv-platform-linux -j"${NV_JOBS:-8}"
  NV_RUNTIME="$BUILD_DIR/modules/nv-runtime/libnv-runtime.a"
  NV_PLAT="$BUILD_DIR/modules/nv-platform-linux/libnv-platform-linux.a"
  NV_OPS="$BUILD_DIR/modules/nv-ops/libnv-ops.a"
  NV_IPC="$BUILD_DIR/modules/nv-ipc/libnv-ipc.a"
  NV_CORE="$BUILD_DIR/modules/nv-core/libnv-core.a"
  PKG_LIBS="$(pkg-config --libs webkit2gtk-4.1)"
  PKG_LIBS="${PKG_LIBS//-pthread/-lpthread}"
  for pkg in ayatana-appindicator3-1 ayatana-appindicator-0.1 appindicator3-0.1 libnotify x11; do
    if pkg-config --exists "$pkg" 2>/dev/null; then
      PKG_LIBS+=" $(pkg-config --libs "$pkg")"
    fi
  done
  PKG_LIBS="${PKG_LIBS//-pthread/-lpthread}"
  cd "$SCRIPT_DIR"
  # shellcheck disable=SC2086
  "$ZIG" "${ZIG_ROOT[@]}" \
    "$NV_RUNTIME" "$NV_PLAT" "$NV_OPS" "$NV_IPC" "$NV_CORE" \
    $PKG_LIBS \
    $SQLITE_LIBS
  echo "Built: $SCRIPT_DIR/zig_todo"
  ;;
Darwin)
  cmake --build "$BUILD_DIR" --target nv-core nv-ipc nv-ops nv-runtime nv-platform-mac -j"${NV_JOBS:-8}"
  NV_PLAT="$BUILD_DIR/modules/nv-platform-mac/libnv-platform-mac.a"
  NV_RUNTIME="$BUILD_DIR/modules/nv-runtime/libnv-runtime.a"
  NV_OPS="$BUILD_DIR/modules/nv-ops/libnv-ops.a"
  NV_IPC="$BUILD_DIR/modules/nv-ipc/libnv-ipc.a"
  NV_CORE="$BUILD_DIR/modules/nv-core/libnv-core.a"
  cd "$SCRIPT_DIR"
  "$ZIG" "${ZIG_ROOT[@]}" \
    "$NV_PLAT" \
    "$NV_RUNTIME" \
    "$NV_OPS" \
    "$NV_IPC" \
    "$NV_CORE" \
    -framework Carbon -framework Cocoa -framework CoreServices -framework WebKit -framework UserNotifications -lobjc \
    $SQLITE_LIBS
  echo "Built: $SCRIPT_DIR/zig_todo"
  ;;
*)
  echo "Unsupported host for this script: $(uname -s). See docs/Zig.md for manual link flags."
  exit 1
  ;;
esac
