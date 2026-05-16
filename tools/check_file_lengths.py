#!/usr/bin/env python3
import os
import sys


ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))

LIMITS = {
    ".c": 800,
    ".m": 800,
    ".mm": 800,
    ".cpp": 800,
}

IGNORED_DIRS = {
    ".git",
    ".cxx",
    ".gradle",
    ".idea",
    ".vscode",
    "build",
    "node_modules",
    "sqlite",
}

ALLOWLIST = {
    "modules/nv-platform-win/src/nv_win.c",
    "modules/nv-platform-mac/src/nv_mac.m",
    "modules/nv-platform-linux/src/nv_linux.c",
    "modules/nv-core/src/nv_json.c",
    "modules/nv-ui/src/nv_widget.c",
    "modules/nv-ipc/src/nv_ipc.c",
    "modules/nv-ipc/tests/test_ipc.c",
    "modules/nv-platform-win/src/nv_win_tray.c",
    "modules/nv-platform-linux/src/nv_linux_tray.c",
    "modules/nv-runtime/src/nv_window.c",
    "modules/nv-core/tests/test_json.c",
    "modules/nv-core/tests/test_json_compose.c",
    "modules/nv-ops/tests/test_op_clipboard.c",
    "modules/nv-core/tests/test_arena.c",
}


def iter_source_files(root: str):
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = [
            d
            for d in dirnames
            if d not in IGNORED_DIRS
            and not d.startswith(".")
            and not d.startswith("cmake-build")
            # Local out-of-tree CMake dirs (e.g. build-zig-todo-static/)
            and not d.startswith("build-")
        ]
        for filename in filenames:
            _, ext = os.path.splitext(filename)
            if ext in LIMITS:
                yield os.path.join(dirpath, filename)


def count_lines(path: str) -> int:
    with open(path, "rb") as f:
        return sum(1 for _ in f)


def main() -> int:
    violations = []
    for abs_path in iter_source_files(ROOT):
        rel_path = os.path.relpath(abs_path, ROOT).replace(os.sep, "/")
        if rel_path in ALLOWLIST:
            continue
        _, ext = os.path.splitext(rel_path)
        limit = LIMITS.get(ext)
        if not limit:
            continue
        try:
            n = count_lines(abs_path)
        except OSError as e:
            violations.append((rel_path, limit, -1, f"read failed: {e}"))
            continue
        if n > limit:
            violations.append((rel_path, limit, n, "too long"))

    if not violations:
        return 0

    violations.sort(key=lambda x: (x[1], x[2]), reverse=True)
    sys.stderr.write("File length check failed:\n")
    for rel_path, limit, n, why in violations:
        if n < 0:
            sys.stderr.write(f"  - {rel_path}: {why}\n")
        else:
            sys.stderr.write(f"  - {rel_path}: {n} lines (limit {limit})\n")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
