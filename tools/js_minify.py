#!/usr/bin/env python3
import argparse
import sys
from pathlib import Path

NV_START = "/* NV_DEBUG_START */"
NV_END = "/* NV_DEBUG_END */"

def read_text(p: Path) -> str:
    with open(p, "r", encoding="utf-8") as f:
        return f.read()

def write_text(p: Path, s: str) -> None:
    p.parent.mkdir(parents=True, exist_ok=True)
    with open(p, "w", encoding="utf-8") as f:
        f.write(s)

def strip_nv_debug(s: str):
    removed = 0
    out = []
    i = 0
    n = len(s)
    while i < n:
        start = s.find(NV_START, i)
        if start == -1:
            out.append(s[i:])
            break
        end = s.find(NV_END, start)
        if end == -1:
            # If unmatched, treat as normal content
            out.append(s[i:])
            break
        # include end marker length
        end += len(NV_END)
        out.append(s[i:start])
        removed += (end - start)
        i = end
    return ("".join(out), removed)

def strip_comments_and_whitespace(s: str) -> str:
    out = []
    i = 0
    n = len(s)
    state = "normal"  # normal | sl_comment | ml_comment | sq | dq | bt
    while i < n:
        ch = s[i]
        nxt = s[i+1] if i+1 < n else ""
        if state == "normal":
            if ch == "/" and nxt == "/":
                state = "sl_comment"
                i += 2
                continue
            if ch == "/" and nxt == "*":
                state = "ml_comment"
                i += 2
                continue
            if ch == "'":
                state = "sq"
                out.append(ch)
                i += 1
                continue
            if ch == '"':
                state = "dq"
                out.append(ch)
                i += 1
                continue
            if ch == "`":
                state = "bt"
                out.append(ch)
                i += 1
                continue
            out.append(ch)
            i += 1
        elif state == "sl_comment":
            if ch == "\n":
                out.append(ch)  # preserve newline to keep line structure for next step
                state = "normal"
            i += 1
        elif state == "ml_comment":
            if ch == "*" and nxt == "/":
                state = "normal"
                i += 2
            else:
                i += 1
        elif state in ("sq", "dq"):
            out.append(ch)
            if ch == "\\":
                # escape next char inside string
                if i+1 < n:
                    out.append(s[i+1])
                    i += 2
                    continue
            end_quote = "'" if state == "sq" else '"'
            if ch == end_quote:
                state = "normal"
            i += 1
        elif state == "bt":  # backtick template (simple: read until next unescaped backtick)
            out.append(ch)
            if ch == "\\":
                if i+1 < n:
                    out.append(s[i+1])
                    i += 2
                    continue
            if ch == "`":
                state = "normal"
            i += 1

    # Trim lines and remove blanks
    lines = "".join(out).splitlines()
    trimmed = [ln.strip() for ln in lines]
    nonblank = [ln for ln in trimmed if ln]
    return "\n".join(nonblank) + ("\n" if nonblank else "")

def braces_balance_ok(s: str) -> bool:
    # Check {} [] () balance ignoring content in strings
    stack = []
    i = 0
    n = len(s)
    state = "normal"
    while i < n:
        ch = s[i]
        if state == "normal":
            if ch in "'\"`":
                state = ch
            elif ch in "{[(":
                stack.append(ch)
            elif ch in "}])":
                if not stack:
                    return False
                top = stack.pop()
                if (top, ch) not in (("{","}"), ("[","]"), ("(",")")):
                    return False
        else:
            if ch == "\\":
                i += 2
                continue
            if (state == "'" and ch == "'") or (state == '"' and ch == '"') or (state == "`" and ch == "`"):
                state = "normal"
        i += 1
    return not stack and state == "normal"

def main():
    ap = argparse.ArgumentParser(description="Minify nativeview.js by stripping debug blocks and comments")
    ap.add_argument("--input", default="js/dist/nativeview.js", help="Input JS bundle")
    ap.add_argument("--output", default="js/dist/nativeview.min.js", help="Output minified bundle")
    args = ap.parse_args()

    root = Path(__file__).resolve().parents[1]
    in_path = (root / args.input).resolve()
    out_path = (root / args.output).resolve()

    if not in_path.exists():
        print(f"Missing input: {in_path}", file=sys.stderr)
        sys.exit(2)

    original = read_text(in_path)
    orig_size = len(original.encode("utf-8"))

    no_debug, removed_debug_bytes = strip_nv_debug(original)
    minified = strip_comments_and_whitespace(no_debug)
    min_size = len(minified.encode("utf-8"))

    if not braces_balance_ok(minified):
        print("Brace balance check failed on minified output", file=sys.stderr)
        sys.exit(3)

    write_text(out_path, minified)

    reduction = (1 - (min_size / orig_size)) * 100 if orig_size else 0.0
    print(f"Wrote {out_path}")
    print(f"Original size:  {orig_size} B")
    print(f"Minified size:  {min_size} B")
    print(f"Reduction:      {reduction:.2f}%")
    print(f"NV_DEBUG bytes removed: {removed_debug_bytes}")

if __name__ == "__main__":
    main()
