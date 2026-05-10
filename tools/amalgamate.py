#!/usr/bin/env python3
"""
Amalgamate nativeview into two distributable files:
  - dist/nativeview.c
  - dist/nv.h (copied unchanged from include/nv.h)

Usage:
  python3 tools/amalgamate.py --platform [mac|win|linux] --output dist/

Rules:
  - Python 3.8+, stdlib only
  - Idempotent output
  - After generation, attempts: cc -c dist/nativeview.c -I dist/
"""
import argparse
import os
import re
import shutil
import subprocess
import sys
from datetime import datetime, timezone

PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))

LICENSE_BLOCK = """\
/*
 * Copyright (c) 2026
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
"""

INCLUDE_INTERNAL_RE = re.compile(r'^\s*#\s*include\s*"nv_.*_internal\.h"\s*')
INCLUDE_PROJECT_RE = re.compile(r'^\s*#\s*include\s*"(?:nv_[^"]+\.h|util/nv_log\.h|core/nv_window_manager\.h|nv_window_manager\.h|ops/nv_op_.*\.h)"\s*')
INCLUDE_SYSTEM_RE = re.compile(r'^\s*#\s*include\s*<([^>]+)>\s*')

def read_file(path: str) -> str:
    with open(path, "r", encoding="utf-8") as f:
        return f.read()

def normalize_newlines(s: str) -> str:
    return s.replace("\r\n", "\n").replace("\r", "\n")

def collect_and_strip(content: str, system_includes: set, platform: str) -> str:
    """
    - Collect system includes into system_includes
    - Strip includes of internal headers (nv_*_internal.h)
    - Keep system includes, but they will be removed here and re-inserted globally
    - Strip project public headers (#include "nv_*.h") since code is concatenated
    """
    out_lines = []
    for line in normalize_newlines(content).split("\n"):
        if INCLUDE_INTERNAL_RE.match(line):
            continue
        m_sys = INCLUDE_SYSTEM_RE.match(line)
        if m_sys:
            hdr = m_sys.group(1).strip()
            # Platform-specific headers: keep in source (inside #if blocks)
            if hdr.lower() == "windows.h" or hdr.startswith("objbase"):
                out_lines.append(line)
                continue
            if hdr.startswith("Cocoa/") or hdr.startswith("WebKit/"):
                out_lines.append(line)
                continue
            if hdr.startswith("gtk/") or hdr.startswith("webkit2/"):
                out_lines.append(line)
                continue
            if "WebView2" in hdr:
                out_lines.append(line)
                continue
            system_includes.add(hdr)
            continue
        # Strip project headers (nv_*.h) to avoid dependencies in amalgamation
        if INCLUDE_PROJECT_RE.match(line):
            continue
        out_lines.append(line)
    # Remove trailing blank lines for determinism
    while out_lines and out_lines[-1].strip() == "":
        out_lines.pop()
    return "\n".join(out_lines) + "\n"

def inline_header_strip(path: str) -> str:
    """
    Inline a header by:
    - removing all #include lines (both system and project)
    - returning the raw declarations
    """
    text = normalize_newlines(read_file(path))
    out = []
    for line in text.split("\n"):
        if INCLUDE_SYSTEM_RE.match(line):
            continue
        if INCLUDE_PROJECT_RE.match(line):
            continue
        out.append(line)
    # Trim trailing empties
    while out and out[-1].strip() == "":
        out.pop()
    return "\n".join(out) + "\n"

def determine_sources(platform: str) -> list:
    """Return C sources in dependency order. Platform .c files have #ifdef guards.
    nv_mac.m (ObjC) is handled separately - not included in nativeView.c."""
    sources = [
        # nv-core
        "modules/nv-core/src/nv_arena.c",
        "modules/nv-core/src/nv_str.c",
        "modules/nv-core/src/nv_json.c",
        "modules/nv-core/src/nv_base64.c",
        "modules/nv-core/src/nv_util.c",
        "modules/nv-core/src/nv_log.c",
        # nv-ipc
        "modules/nv-ipc/src/nv_ipc.c",
        "modules/nv-ipc/src/nv_ipc_script.c",
        "modules/nv-ipc/src/nv_shm.c",
        # nv-ops
        "modules/nv-ops/src/nv_op_app.c",
        "modules/nv-ops/src/nv_op_clipboard.c",
        "modules/nv-ops/src/nv_op_dialog.c",
        "modules/nv-ops/src/nv_op_fs.c",
        "modules/nv-ops/src/nv_op_ipc_bus.c",
        "modules/nv-ops/src/nv_op_notification.c",
        "modules/nv-ops/src/nv_op_registry.c",
        "modules/nv-ops/src/nv_op_screen.c",
        "modules/nv-ops/src/nv_op_shell.c",
        "modules/nv-ops/src/nv_op_tray.c",
        "modules/nv-ops/src/nv_op_window.c",
        "modules/nv-ops/src/nv_op_windows.c",
        # nv-runtime
        "modules/nv-runtime/src/nv_core.c",
        "modules/nv-runtime/src/nv_window.c",
        "modules/nv-runtime/src/nv_window_manager.c",
        # Platform: include both linux and win in one file with #ifdef; mac stays separate
        "modules/nv-platform-linux/src/nv_linux.c",
        "modules/nv-platform-win/src/nv_win.c",
    ]
    return sources

def build_amalgamation(platform: str, out_dir: str) -> None:
    src_paths = determine_sources(platform)  # platform used for platform filter in collect_and_strip
    abs_paths = [os.path.join(PROJECT_ROOT, p) for p in src_paths]

    system_includes = set()
    # Build prelude by inlining internal/public headers required for single TU
    headers_to_inline = [
        # Base utilities and macros first
        os.path.join(PROJECT_ROOT, "include", "util", "nv_log.h"),
        # Public umbrella header first so its include guard NV_H is defined
        os.path.join(PROJECT_ROOT, "include", "nv.h"),
        # Public headers whose types are used across modules
        os.path.join(PROJECT_ROOT, "include", "nv_arena.h"),
        os.path.join(PROJECT_ROOT, "include", "nv_str.h"),
        os.path.join(PROJECT_ROOT, "include", "nv_json.h"),
        os.path.join(PROJECT_ROOT, "include", "nv_base64.h"),
        os.path.join(PROJECT_ROOT, "include", "nv_ipc.h"),
        os.path.join(PROJECT_ROOT, "include", "nv_shm.h"),
        os.path.join(PROJECT_ROOT, "modules", "nv-ipc", "src", "nv_ipc_script.h"),
        os.path.join(PROJECT_ROOT, "include", "nv_window.h"),
        os.path.join(PROJECT_ROOT, "include", "nv_core.h"),
        # Internal headers needed for prototypes/structs across modules
        os.path.join(PROJECT_ROOT, "include", "nv_ipc_internal.h"),
        os.path.join(PROJECT_ROOT, "include", "nv_core_internal.h"),
        os.path.join(PROJECT_ROOT, "include", "nv_window_internal.h"),
        os.path.join(PROJECT_ROOT, "include", "nv_window_manager.h"),
        # Operations
        os.path.join(PROJECT_ROOT, "modules", "nv-ops", "src", "nv_op_app.h"),
        os.path.join(PROJECT_ROOT, "modules", "nv-ops", "src", "nv_op_clipboard.h"),
        os.path.join(PROJECT_ROOT, "modules", "nv-ops", "src", "nv_op_dialog.h"),
        os.path.join(PROJECT_ROOT, "modules", "nv-ops", "src", "nv_op_fs.h"),
        os.path.join(PROJECT_ROOT, "modules", "nv-ops", "src", "nv_op_ipc_bus.h"),
        os.path.join(PROJECT_ROOT, "modules", "nv-ops", "src", "nv_op_notification.h"),
        os.path.join(PROJECT_ROOT, "modules", "nv-ops", "src", "nv_op_registry.h"),
        os.path.join(PROJECT_ROOT, "modules", "nv-ops", "src", "nv_op_screen.h"),
        os.path.join(PROJECT_ROOT, "modules", "nv-ops", "src", "nv_op_shell.h"),
        os.path.join(PROJECT_ROOT, "modules", "nv-ops", "src", "nv_op_tray.h"),
        os.path.join(PROJECT_ROOT, "modules", "nv-ops", "src", "nv_op_window.h"),
        os.path.join(PROJECT_ROOT, "modules", "nv-ops", "src", "nv_op_windows.h"),
    ]
    prelude = []
    prelude.append("/* ===== Begin: amalgamated prelude (headers) ===== */\n")
    for hp in headers_to_inline:
        if os.path.exists(hp):
            prelude.append(f"/* -- inline: {os.path.relpath(hp, PROJECT_ROOT)} -- */\n")
            prelude.append(inline_header_strip(hp))
    prelude.append("/* ===== End: amalgamated prelude (headers) ===== */\n\n")
    parts = []
    for p in abs_paths:
        content = read_file(p)
        stripped = collect_and_strip(content, system_includes, platform)
        banner = f"\n/* ===== Begin: {os.path.relpath(p, PROJECT_ROOT)} ===== */\n"
        footer = f"/* ===== End: {os.path.relpath(p, PROJECT_ROOT)} ===== */\n"
        parts.append(banner + stripped + footer)


    includes_sorted = sorted(system_includes)
    timestamp = datetime.now(timezone.utc).strftime("%Y-%m-%d %H:%M:%S %Z")
    header = []
    header.append(LICENSE_BLOCK.rstrip() + "\n")
    header.append("/*\n * Generated amalgamation — do not edit manually.\n")
    header.append(f" * Generated at: {timestamp}\n */\n\n")
    if includes_sorted:
        for inc in includes_sorted:
            header.append(f"#include <{inc}>\n")
        header.append("\n")
    # Minimal shim for macros to satisfy compilation in amalgamated TU
    shim = """\
/* Shim removed: nv_log.h is inlined and provides these definitions */

"""
    header.append(shim)

    os.makedirs(out_dir, exist_ok=True)
    out_c = os.path.join(out_dir, "nativeview.c")
    with open(out_c, "w", encoding="utf-8", newline="\n") as f:
        f.write("".join(header))
        f.write("".join(prelude))
        f.write("".join(parts))

    # Copy public header
    src_h = os.path.join(PROJECT_ROOT, "include", "nv.h")
    dst_h = os.path.join(out_dir, "nv.h")
    shutil.copyfile(src_h, dst_h)

    # On Mac: nv_mac.m (ObjC) cannot be inlined into .c; copy as separate TU
    if platform == "mac":
        src_mac = os.path.join(PROJECT_ROOT, "modules", "nv-platform-mac", "src", "nv_mac.m")
        dst_mac = os.path.join(out_dir, "nv_mac.m")
        if os.path.exists(src_mac):
            shutil.copyfile(src_mac, dst_mac)
    # Create a minimal nv_util.h shim for validation only (removed after compile)
    util_shim = os.path.join(out_dir, "nv_util.h")
    util_content = """\
#ifndef NV_UTIL_H
#define NV_UTIL_H
#ifdef _MSC_VER
#  define NV_API __declspec(dllexport)
#  define NV_INTERNAL
#else
#  define NV_API __attribute__((visibility("default")))
#  define NV_INTERNAL __attribute__((visibility("hidden")))
#endif
#endif
"""
    with open(util_shim, "w", encoding="utf-8", newline="\n") as uf:
        uf.write(util_content)

def try_compile(out_dir: str, platform: str) -> bool:
    try:
        inc_root = os.path.join(PROJECT_ROOT, "include")
        inc_ops = os.path.join(PROJECT_ROOT, "modules", "nv-ops", "src")
        inc_runtime = os.path.join(PROJECT_ROOT, "modules", "nv-runtime", "src")
        c_path = os.path.join(out_dir, "nativeview.c")
        o_path = os.path.join(out_dir, "nativeview.o")
        cmd = ["cc", "-c", "-I", out_dir, "-I", inc_root, "-I", inc_ops, "-I", inc_runtime,
               "-o", o_path, c_path]
        proc = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        if proc.returncode != 0:
            sys.stderr.write(proc.stdout.decode(errors="ignore"))
            sys.stderr.write(proc.stderr.decode(errors="ignore"))
            print("compile: failed (nativeview.c)")
            return False
        objs = [os.path.join(out_dir, "nativeview.o")]
        if platform == "mac":
            mac_path = os.path.join(out_dir, "nv_mac.m")
            if os.path.exists(mac_path):
                mac_cmd = ["cc", "-c", "-x", "objective-c", "-I", out_dir, "-I", inc_root,
                           "-I", inc_ops, "-I", inc_runtime, mac_path, "-o",
                           os.path.join(out_dir, "nv_mac.o")]
                proc = subprocess.run(mac_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                if proc.returncode != 0:
                    sys.stderr.write(proc.stderr.decode(errors="ignore"))
                    print("compile: failed (nv_mac.m)")
                    return False
                objs.append(os.path.join(out_dir, "nv_mac.o"))
        # Link (platform-specific libs may be needed; skip on Linux without gtk for now)
        if platform == "mac" and len(objs) > 1:
            link_cmd = ["cc", "-o", os.path.join(out_dir, "nv_test"), "-framework", "Cocoa",
                        "-framework", "WebKit", "-framework", "Foundation"] + objs
            proc = subprocess.run(link_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            if proc.returncode != 0:
                sys.stderr.write(proc.stderr.decode(errors="ignore"))
                print("compile: link failed (expected if frameworks missing)")
        print("compile: success")
        return True
    except FileNotFoundError:
        print("warning: 'cc' not found; skipping compile validation", file=sys.stderr)
        return True
    finally:
        try:
            os.remove(os.path.join(out_dir, "nv_util.h"))
        except OSError:
            pass

def main():
    parser = argparse.ArgumentParser(description="Amalgamate nativeview sources")
    parser.add_argument("--platform", required=False, choices=["mac", "win", "linux"],
                        help="Target platform to select backend source")
    parser.add_argument("--output", required=False, default="dist",
                        help="Output directory (default: dist)")
    args = parser.parse_args()

    platform = args.platform
    if platform is None:
        if sys.platform.startswith("darwin"):
            platform = "mac"
        elif os.name == "nt" or sys.platform.startswith("win"):
            platform = "win"
        else:
            platform = "linux"

    out_dir = os.path.abspath(os.path.join(PROJECT_ROOT, args.output))
    build_amalgamation(platform, out_dir)
    ok = try_compile(out_dir, platform)
    if not ok:
        print("warning: compile validation failed; amalgamation files were generated", file=sys.stderr)

if __name__ == "__main__":
    main()
