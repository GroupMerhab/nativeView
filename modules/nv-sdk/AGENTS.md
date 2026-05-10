# AGENTS.md — nv-sdk

## Scope
Meta-package for application developers. `nv-sdk` has **no source files** of
its own. It is a pure CMake INTERFACE library that links the full platform
stack (`nativeview`) together with the component model (`nv-ui`), giving
application code a single link target.

```cmake
target_link_libraries(my_app PRIVATE nv-sdk)
```

## Owns
- `CMakeLists.txt` — INTERFACE target definition only
- `AGENTS.md`      — this file

## What nv-sdk provides to consumers
By linking `nv-sdk`, an application automatically gets:

| Module              | What it provides                                  |
|---------------------|---------------------------------------------------|
| `nv-core`           | Arena allocator, string, JSON, logging            |
| `nv-ipc`            | Binary IPC frames, JS script dispatch             |
| `nv-ops`            | Clipboard, dialog, filesystem, shell ops          |
| `nv-runtime`        | App lifecycle, window management                  |
| `nv-platform-*`     | WebView backend for the current OS                |
| `nv-ui`             | Component model, widgets, DOM diff engine         |

## Dependency graph
```
nv-core → nv-ipc → nv-ops → nv-runtime → nv-platform-{mac,win,linux}
                 └→ nv-ui
nv-sdk (meta: nativeview + nv-ui)
```

## Allowed dependencies
- `nativeview` INTERFACE target (defined in root CMakeLists.txt)
- `nv-ui` INTERFACE/STATIC target

## Forbidden
- Do NOT add any `.c` or `.h` source files to this module
- Do NOT add platform `#ifdef` blocks — platform selection is handled by
  `nv-platform-*` modules
- Do NOT link `nv-designer` — it is a build tool, not a runtime dependency
- Do NOT change the LICENSE file

## Coding rules
- This module contains only CMake; there is no C to write
- Any change to the public API of `nv-ui` or `nativeview` that affects
  consumers must be reflected in the `nv-sdk` include path list

## Task labels
- `[nv-sdk]`       — scoped to this module
- `[nv-sdk][deps]` — adding or removing a linked module
