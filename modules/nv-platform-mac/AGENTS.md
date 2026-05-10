# AGENTS.md — nv-platform-mac

## Scope
macOS-only WebView backend. Implements the platform hooks declared in
`nv_core_internal.h` and `nv_window_internal.h`.

## Owns
- `src/nv_mac.m` — Objective-C backend (WKWebView, NSWindow, ARC)

## Allowed dependencies
- `nv-runtime` (and its full transitive chain)
- Apple frameworks: `Cocoa.framework`, `WebKit.framework`
- Objective-C ARC memory model

## Forbidden
- Do NOT add cross-platform code here — use `nv-runtime` or `nv-ops`
- Do NOT use `retain`/`release` manually — ARC only
- Do NOT change the LICENSE file
- Do NOT add Windows or Linux `#ifdef` blocks

## Coding rules
- Objective-C with ARC, `-Wall -Wextra` clean
- All WKWebView delegate methods must call through to the nv-runtime callbacks
- Max 500 lines in `nv_mac.m` — split into categories if needed
- Memory: all C structs owned by arena; all ObjC objects owned by ARC

## Task labels
- `[platform-mac]`          — scoped to this file
- `[platform-mac][webkit]`  — WKWebView API changes
- `[platform-mac][ios]`     — future iOS port work (TARGET_OS_IPHONE guards)
