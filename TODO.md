# NativeView — Project TODO

Status legend: `[ ]` pending · `[x]` done · `[-]` in progress

---

## Project Principles (enforced on every task, every file, every decision)

| Priority | Rule |
|---|---|
| 1 | **Performance first** — every design decision favors runtime speed; measure before choosing |
| 2 | **Smallest binary** — no bloat, no unused code linked in, strip dead symbols |
| 3 | **No C dependencies** — zero third-party C libs unless there is absolutely no OS-level alternative |
| 4 | **No file > 800 lines** — split the file before you reach the limit; one responsibility per file |
| 5 | **Ask before choosing a technology** — always propose options with trade-offs and wait for approval |
| 6 | **Vue.js owns the DOM** — C never writes raw HTML after the scaffold is loaded; all DOM mutations go through Vue reactivity |
| 7 | **Vue/JS components are first-class** — C provides a rectangular slot (`NV_COMP_VUE`); a Vue/JS component fills it; more Vue components will be added in future phases |
| 8 | **AI-friendly** — every module has an `AGENTS.md`; every file has a single clear responsibility; names are self-documenting |
| 9 | **Readable & maintainable** — no clever tricks; comments explain *why*, not *what* |
| 10 | **Modular** — each module compiles independently; no circular deps; can be opened alone in an IDE |
| 11 | **Industrial grade** — `-Wall -Wextra` clean, tests for every public function, zero UB, zero memory leaks |

---

## Phase 1 — Solidify nv-ui (Foundation)

### 1.1 Layout system
- [x] Add `x, y, w, h` fields to `nv_button_cfg_t`, `nv_label_cfg_t`, `nv_input_cfg_t` in `nv_ui.h`
- [x] Add matching `x, y, w, h` to `nv_button_state_t`, `nv_label_state_t`, `nv_input_state_t`
- [x] Emit Vue `:style` binding (position:absolute + x/y/w/h from reactive data) in scaffold for each widget
- [x] Include position fields in `nv_comp_snap_t` and diff them in `build_patch_js()` via `DIFF_GEOM` macro
- [x] Update `examples/counter_ui/main.c` to use explicit positions
- [x] Verify `counter_ui` renders widgets at correct positions (macOS runtime verified)

### 1.2 Widget: Checkbox
- [x] Add `NV_COMP_CHECKBOX` to `nv_component_type_t`
- [x] Add `nv_checkbox_state_t { int checked; const char* label; int x,y,w,h; }` to state union
- [x] Add `nv_checkbox_cfg_t` and `nv_checkbox_t` typedef
- [x] Implement `nv_checkbox_new()` in `nv_component.c`
- [x] Emit `<label><input type="checkbox" :checked="..."> {{ label }}</label>` in scaffold; diff in patch
- [x] Wire `NV_EVENT_CLICK` in JS event bridge for checkboxes (sends `checked` boolean)
- [x] Add test in `modules/nv-ui/tests/test_component.c`

### 1.3 Widget: Select (dropdown)
- [x] Add `NV_COMP_SELECT` to enum
- [x] Add `nv_select_state_t { const char** items; int item_count; int selected_index; int x,y,w,h; }` to state union
- [x] Add `nv_select_cfg_t` and `nv_select_t` typedef
- [x] Implement `nv_select_new()` in `nv_component.c`
- [x] Emit `<select><option v-for="...">` in scaffold; diff `selectedIndex` in patch
- [x] Wire `NV_EVENT_CHANGE` in JS event bridge
- [x] Add test in `tests/test_component.c`

### 1.4 Widget: Slider
- [x] Add `NV_COMP_SLIDER` to enum
- [x] Add `nv_slider_state_t { float value, min, max, step; int x,y,w,h; }` to state union
- [x] Add `nv_slider_cfg_t` and `nv_slider_t` typedef
- [x] Implement `nv_slider_new()` in `nv_component.c`
- [x] Emit `<input type="range" :value :min :max :step>` in scaffold; diff `value` in patch
- [x] Wire `NV_EVENT_INPUT` in JS event bridge (sends float value)
- [x] Add test in `tests/test_component.c`

### 1.5 Widget: Textarea
- [x] Add `NV_COMP_TEXTAREA` to enum
- [x] Add `nv_textarea_state_t { const char* text; int rows; int x,y,w,h; }` to state union
- [x] Add `nv_textarea_cfg_t` and `nv_textarea_t` typedef
- [x] Implement `nv_textarea_new()` in `nv_component.c`
- [x] Emit `<textarea :rows>{{ text }}</textarea>` in scaffold; diff in patch
- [x] Wire `NV_EVENT_INPUT` and `NV_EVENT_CHANGE` in JS event bridge
- [x] Add test in `tests/test_component.c`

### 1.6 Fix arena usage in nv_component.c
- [x] Audit `nv_component.c` for direct `malloc`/`free` calls in `comp_new`
- [x] Replace child component allocation with `nv_arena_alloc` from the form's arena
- [x] Update `nv_component_destroy`: form frees its arena (bulk-frees all children); child nodes are not individually freed
- [x] Verify all tests still pass after arena fix

### 1.7 nv-ui test coverage
- [x] Rewrote `test_component.c` using public API — covers all 8 widget types, geometry fields, find_by_name, event bind/emit, `nv_ui_schedule_diff`, `NV_SET_STATE`
- [x] Create `modules/nv-ui/tests/test_widget.c` — tests for `nv_component_paint` HTML output, `nv_ui_flush` no-op when no window, `schedule_diff` NULL safety
- [x] Ensure all nv-ui tests pass with `-Wall -Wextra` clean

> **File split done** (800-line rule enforced):
> `nv_widget.c` (791) · `nv_widget_flush.c` (150) · `nv_widget_paint.c` (96) · `nv_component.c` (251)

---

## Phase 2 — Vue Component Slot (`NV_COMP_VUE`)

> **Goal**: C owns a named rectangular slot; a Vue.js component fills it entirely.
> **Option B chosen** — createApp per slot (max isolation).

- [x] **Design review** — user chose Option B (createApp per slot)
- [x] Add `NV_COMP_VUE` to `nv_component_type_t` in `nv_ui.h`
- [x] Add `nv_vue_state_t` and `nv_vue_cfg_t` config struct
- [x] Add `nv_vue_t` typedef and `nv_vue_new()` in `nv_component.c`
- [x] Emit slot `<div id="nv-slot-NAME" style="...geometry...">` in `build_vue_scaffold()`
- [x] Add `window.__nv_load_vue_component(slotId, fnBody, propsJson)` in scaffold
- [x] Add `window.__nv_set_vue_props(slotId, propsJson)` in scaffold
- [x] Add `nv_vue_load_component(nv_vue_t*, const char* js_fn_body)` in `nv_vue_slot.c`
- [x] Add `nv_vue_set_props(nv_vue_t*, const char* props_json)` in `nv_vue_slot.c`
- [x] Add `nv_component_get_form(comp)` for slot→form lookup
- [x] Update `AGENTS.md` for `nv-ui`: "HTML inside a VUE slot is owned entirely by JS"
- [x] Add `vue_new`, `find_vue`, `get_form` tests in `test_component.c`
- [x] Add `examples/vue_slot/main.c` — C form + Vue component in slot
- [ ] (Deferred) Wire `nv.vue.mounted` / `nv.vue.unmounted` events → C callbacks

---

## Phase 3 — Bundle Vue (no CDN)

> **No CDN in production** — Vue embedded in the binary via `tools/embed_vue.py`.

- [x] Write `tools/embed_vue.py` — converts `vue.global.prod.js` → `nv_vue_bundle.c` with `const char* nv_vue_bundle(void)`
- [x] Use `js/vue.js` or `modules/nv-ui/assets/vue.global.prod.js` as Vue source
- [x] CMake custom command embeds Vue at build time; output in `modules/nv-ui/` build dir
- [x] `build_vue_scaffold()` uses `nv_vue_bundle()` — no CDN
- [x] Test `counter_ui` runs offline (no network)
- [ ] Measure binary size delta; document in `docs/binary-size.md`
- [ ] (Optional) Extract to `modules/nv-vue/` for cleaner separation

---

## Phase 4 — Hybrid IPC (SharedArrayBuffer fast path)

> **Design confirmed** — Option A: SharedArrayBuffer + shared memory. C creates via `nv_shm_*`; platform posts to JS (WebView2: PostSharedBufferToScript); JS uses `shm_read_f32` in rAF.

- [x] **Design review** — SharedArrayBuffer + Atomics confirmed
- [x] Add `nv_shm_t` and `include/nv_shm.h` with create/destroy/ptr/size/write_f32/name
- [x] Implement `modules/nv-ipc/src/nv_shm.c`: POSIX shm_open+mmap, Windows CreateFileMapping+MapViewOfFile
- [x] Extend JS bootstrap: `window.__nv.shm_register(name, buffer)`, `window.__nv.shm_read_f32(name, offset)`
- [x] Add C API: `nv_slider_attach_shm(nv_slider_t*, void* shm)` in `nv_ui.h` and `nv_component.c`
- [x] Add `benchmarks/bench_shm.c` — measures shm write latency (~5 µs/op vs postMessage 1–4 ms)
- [x] Add `examples/live_data/main.c` — slider + live display (IPC path; shm path when platform supports)
- [x] Add `modules/nv-ipc/tests/test_shm.c`
- [ ] Platform integration: WebView2 `PostSharedBufferToScript`; WKWebView fallback to IPC
- [ ] Update `NV_COMP_SLIDER` Vue template for rAF + shm_read_f32 when shm attached

---

## Phase 5 — nv-designer Tool

> Outputs pure C files (`myform_ui.c`) with `NV_DESIGNER_BEGIN/END` markers.
> No binary/opaque formats. AI agents can read and edit the output directly.
> No extra C dependencies — uses only `nv_json_*` from `nv-core`.

- [ ] Create `modules/nv-designer/` (`src/`, `tests/`, `CMakeLists.txt`, `AGENTS.md`)
- [ ] Define project JSON format: `{ "forms": [ { "name": "myform", "components": [...] } ] }`
- [ ] Implement `nv_dsg_parser.c` — reads project JSON using `nv_json_*` from `nv-core` (no extra deps)
- [ ] Implement `nv_dsg_emit.c` — walks parsed tree, writes `myform_ui.c` with `NV_DESIGNER_BEGIN/END`
- [ ] Implement `nv_dsg_merge.c` — reads existing `myform_ui.c`, replaces designer block, preserves user code
- [ ] Implement `main.c` — CLI: `nv-designer --project myapp.nvproj --emit myform`
- [ ] Write `modules/nv-designer/CMakeLists.txt` (builds `nv-designer` executable)
- [ ] Add `modules/nv-designer` to root `CMakeLists.txt`
- [ ] Test `test_emit.c` — emit a known form, compare output byte-for-byte
- [ ] Test `test_merge.c` — verify user code is preserved, designer block is replaced
- [ ] Round-trip test: parse → emit → parse produces identical output

---

## Phase 6 — More Examples

- [x] `examples/todo_app/c_todo/main.c` — input + button to add items, list with delete buttons (dynamic component list + `NV_SET_STATE`)
- [x] `examples/settings_form/main.c` — labels, inputs, checkboxes, select, slider, textarea, save button (all widget types)
- [x] `examples/live_data/main.c` — slider + live display (Phase 4)
- [x] `examples/vue_slot/main.c` — C form with a Vue component mounted in a `NV_COMP_VUE` slot
- [x] Add todo_app and settings_form to root `CMakeLists.txt`
- [x] Verify all examples build and run on macOS (counter_ui, todo_app, settings_form, vue_slot, hello all build; counter_ui runtime verified)

---

## Phase 7 — Cross-Platform Verification

- [ ] Build `counter_ui` on Windows; fix any issues in `nv_win.c`
- [ ] Verify Vue scaffold + patch IPC work on Windows WebView2
- [ ] Build `counter_ui` on Ubuntu with WebKitGTK; fix any issues in `nv_linux.c`
- [ ] Verify Vue scaffold + patch IPC work on Linux WebKitGTK
- [ ] Create `.github/workflows/ci.yml` — matrix build: macOS + Ubuntu
- [ ] (Optional) Add Windows CI job with WebView2 runner

---

## Phase 8 — Documentation

- [x] `docs/getting-started.md` — install deps, build, run examples on macOS (Linux/Windows to expand)
- [ ] `docs/widget-reference.md` — one section per widget: fields, cfg, events, example code
- [ ] `docs/vue-slot-guide.md` — how to embed Vue/JS components in a C form slot (`NV_COMP_VUE`)
- [ ] `docs/architecture.md` — Vue bridge, composite model, hybrid IPC, designer output, `NV_COMP_VUE` slot
- [ ] `docs/ai-agent-guide.md` — module scope, `AGENTS.md` conventions, task labels, 800-line rule, ask-before-technology rule
- [ ] `docs/binary-size.md` — binary size breakdown per module; track size delta as features are added

---

## Completed

- [x] `nv-core` — arena allocator, string, JSON, logging (`nv_arena.c`, `nv_str.c`, `nv_json.c`, `nv_log.c`, `nv_util.c`)
- [x] `nv-ipc` — binary IPC frames, JS script dispatch (`nv_ipc.c`, `nv_ipc_script.c`)
- [x] `nv-ops` — clipboard, dialog, filesystem, shell, tray, screen, notification ops
- [x] `nv-runtime` — app lifecycle, window management (`nv_core.c`, `nv_window.c`, `nv_window_manager.c`)
- [x] `nv-platform-mac` — WKWebView + NSApplication backend (`nv_mac.m`)
- [x] `nv-platform-win` — WebView2 + Win32 backend (`nv_win.c`)
- [x] `nv-platform-linux` — WebKitGTK + GTK backend (`nv_linux.c`)
- [x] `nv-ui` composite component model — `nv_component_t`, type enum, state union, tree, events (`nv_component.c`)
- [x] `nv-ui` widget layer — split across `nv_widget.c` (scaffold+patch), `nv_widget_flush.c` (flush+event router), `nv_widget_paint.c` (legacy test paint)
- [x] Vue 3 reactive bridge — first flush generates full scaffold; subsequent flushes send JSON patch via `nv_eval_js`
- [x] JS event bridge — delegated `click`/`input`/`change` listeners → `window.__nv.send("nv.event", ...)` → `nv_ui_on_event` C router
- [x] Phase 1.1 — Layout system: `x,y,w,h` on all widget cfg/state structs; Vue `:style` binding; `DIFF_GEOM` macro in patch diff
- [x] Phase 1.2 — Widget: Checkbox (`NV_COMP_CHECKBOX`, `nv_checkbox_new`, Vue template, JS bridge)
- [x] Phase 1.3 — Widget: Select (`NV_COMP_SELECT`, `nv_select_new`, Vue `v-for` options, JS bridge)
- [x] Phase 1.4 — Widget: Slider (`NV_COMP_SLIDER`, `nv_slider_new`, Vue range input, JS bridge)
- [x] Phase 1.5 — Widget: Textarea (`NV_COMP_TEXTAREA`, `nv_textarea_new`, Vue textarea, JS bridge)
- [x] Phase 1.6 — Arena fix: child components allocated from form arena; `nv_component_destroy` bulk-frees via arena
- [x] Phase 1.7 — `test_component.c` rewritten; `test_widget.c` added: nv_component_paint, nv_ui_flush, schedule_diff
- [x] `nv-sdk` — CMake INTERFACE meta-package linking full stack
- [x] `examples/hello` — inline HTML counter demo
- [x] `examples/counter_ui` — nv-ui component model end-to-end demo (label + two buttons, reactive state)
- [x] Root `CMakeLists.txt` — modular build, all modules wired, amalgamation target, pkg-config
- [x] Scaffold arena fix — use `nv_arena_create(512*1024)` for build_vue_scaffold (pool chunks too small for Vue bundle ~165KB)
- [x] nv-ui examples load scaffold before show — avoid `didFinishNavigation` not firing for about:blank on WKWebView
