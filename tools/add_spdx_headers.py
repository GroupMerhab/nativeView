#!/usr/bin/env python3
"""
Add Apache-2.0 SPDX header to C/C++/ObjC sources missing one.

Usage:
  python3 tools/add_spdx_headers.py [--dry-run] [paths...]

Default paths: modules/ include/
Skips: files that already contain SPDX-License-Identifier or full Apache boilerplate.
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EXTENSIONS = {".c", ".h", ".m", ".mm", ".cpp"}
SKIP_DIRS = {"build", ".git", "sqlite", "node_modules", "dist"}

SHORT_HEADER = """\
/* Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0 */
"""

FULL_APACHE_MARKERS = (
    "Licensed under the Apache License, Version 2.0",
    "SPDX-License-Identifier: Apache-2.0",
)


def has_license(text: str) -> bool:
    return any(m in text for m in FULL_APACHE_MARKERS)


def should_process(path: Path) -> bool:
    if path.suffix not in EXTENSIONS:
        return False
    for part in path.parts:
        if part in SKIP_DIRS:
            return False
    return True


def add_header(path: Path, dry_run: bool) -> bool:
    text = path.read_text(encoding="utf-8", errors="replace")
    if has_license(text):
        return False
    new_text = SHORT_HEADER + "\n" + text
    if dry_run:
        print(f"would update: {path.relative_to(ROOT)}")
    else:
        path.write_text(new_text, encoding="utf-8")
        print(f"updated: {path.relative_to(ROOT)}")
    return True


def iter_sources(base: Path):
    if base.is_file():
        if should_process(base):
            yield base
        return
    for p in base.rglob("*"):
        if p.is_file() and should_process(p):
            yield p


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument(
        "paths",
        nargs="*",
        type=Path,
        default=[ROOT / "modules", ROOT / "include"],
    )
    args = ap.parse_args()
    count = 0
    for raw in args.paths:
        base = raw if raw.is_absolute() else ROOT / raw
        if not base.exists():
            print(f"skip missing: {base}", file=sys.stderr)
            continue
        for path in sorted(iter_sources(base)):
            if add_header(path, args.dry_run):
                count += 1
    print(f"{'would update' if args.dry_run else 'updated'} {count} file(s)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
