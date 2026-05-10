# Project Structure — nativeview

## Root Directory
The nativeview subproject is a CMake-based C11 codebase organized as a set of independent modules under `modules/`. The dependency graph is a strict DAG: modules depend “downwards” only, and platform-specific code is isolated in `modules/nv-platform-*/`.

```
nativeView/
├── include/                # Public headers (stable C API surface)
├── modules/                # Modular implementation (each module is standalone)
├── bindings/pascal/        # Free Pascal import unit (nativeview.pas) for nv.h
├── js/                     # JavaScript bridge sources, build tools, tests, types
├── examples/               # Small apps exercising the public C API
│   └── pascal/             # FPC sample: thin .lpr + nv_* app unit (see docs/Pascal.md)
├── demo/                   # Larger integration demo app (manual + automated checks)
├── benchmarks/             # Microbenchmarks (IPC/shm)
├── tools/                  # Build tooling (JS build, Vue embedding, amalgamation)
├── CMakeLists.txt          # Top-level build and integration wiring
├── nativeview.pc.in        # pkg-config template
├── AGENTS.md               # Agent rules and module index
├── TODO.md                 # Roadmap and project principles
└── docs/                   # Subproject documentation (this directory)
```

## Modules
Each module lives under `modules/nv-*/` and should have its own `CMakeLists.txt`, `AGENTS.md`, `src/`, and (where applicable) `tests/`.

```
modules/
├── nv-core/                # Arena, JSON, base64, strings, logging, utilities (no platform code)
├── nv-http/                # HTTP server + socket layer (platform-neutral + platform shims)
├── nv-ipc/                 # IPC framing, dispatch, shared memory helpers
├── nv-ops/                 # Capability modules (fs, dialog, clipboard, tray, etc.); includes `window.setContextMenu` tests (`test_op_window_menu.c`)
├── nv-runtime/             # App/window lifecycle + window manager (includes `nv_window_context_menu.c` weak stubs for context-menu platform hooks)
├── nv-platform-mac/        # macOS backend (WKWebView)
├── nv-platform-win/        # Windows backend (WebView2)
├── nv-platform-linux/      # Linux backend (WebKitGTK)
├── nv-ui/                  # Optional reactive UI layer (Vue-owned DOM) when enabled
├── nv-designer/            # Optional designer tool (pure C output) when enabled
└── nv-sdk/                 # Optional meta-package target when enabled
```

### Dependency Direction (Conceptual)
- nv-core → nv-ipc → nv-ops → nv-runtime → nv-platform-{mac,win,linux}
- nv-ipc → nv-ui → nv-designer
- nv-sdk depends on the selected stack targets

## Public API Surface
- Primary public header: `include/nv.h`
- Supporting public headers live alongside it in `include/` (core, ipc, window, shm, json, str, util)

Conventions for public API:
- Opaque handle types for public structs (`nv_app_t`, `nv_window_t`, etc.)
- All public functions are NULL-safe
- No platform-specific `#ifdef` blocks outside `modules/nv-platform-*/`

## JavaScript Bridge Layout
```
js/
├── src/
│   ├── core/               # Transport, routing, request/response, state, init
│   ├── modules/            # High-level capability APIs (window/app/fs/etc.)
│   ├── debug/              # Debug helpers (stripped from production builds)
│   └── public/             # Final assembled API surface (NativeView global)
├── tests/                  # JS integration tests + mocks
├── types/                  # TypeScript typings for the JS API
└── vue.js                  # Vue bundle source location used by embedding pipeline
```

## Tests
Tests are registered via CTest at the top level and in module CMakeLists where applicable.

Common patterns:
- Each test is a small executable with its own `main()` runner.
- Full-stack tests link against the `nativeview` interface target, which pulls in the platform backend.

From a build directory:
- Configure: `cmake ..`
- Build: `cmake --build .`
- Run tests: `ctest`

## Examples and Demo
- `examples/` contains focused programs that exercise a narrow part of the API.
- `examples/pascal/` contains a **Free Pascal** sample (`nv_minimal.lpr` + `nv_minimal_app.pas`) showing a **thin main file** and app logic in a unit; see **`docs/Pascal.md`**.
- `demo/` contains a larger “mega demo” that acts as an integration harness and showcase for the full ops surface.

## Tooling
- `tools/js_build.py`: assembles JS from `js/src/build.manifest`
- `tools/js_minify.py`: strips debug blocks and minifies output
- `tools/fetch_vue.sh`: fetches Vue source for nv-ui examples (when needed)
- `tools/embed_vue.py`: embeds Vue bundle into C for offline/production builds
- `tools/amalgamate.py`: produces a single-file distribution build (optional)

## Documentation Placement
Subproject documentation lives under:
- `nativeView/docs/Implementation.md`
- `nativeView/docs/project_structure.md`
- `nativeView/docs/UI_UX_doc.md`
- `nativeView/docs/Pascal.md` — Free Pascal / Lazarus linking, defines, and main-file alternatives
