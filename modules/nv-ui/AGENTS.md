# AGENTS.md — nv-ui

## Scope
Reactive component model for the C GUI layer. **Composite pattern**: a single
`nv_component_t` node type with a type enum (`NV_COMP_FORM`, `NV_COMP_BUTTON`,
etc.); paint, destroy, and resize are implemented by switching on type and
walking the tree. No vtables. Owner/parent tree and `NV_STATE` reactive macro
drive a DOM diff engine. Components are virtual — they generate HTML/CSS patches
sent to the WebView via `nv-ipc`.

Both `nv-ipc` (IPC dispatch) and `nv-runtime` (window handle) are dependencies.
`nv-designer` depends on this module.

## Owns
- `src/nv_component.c`  — composite: init(type), destroy, paint/resize stubs, tree, events
- `src/nv_widget.c`     — (planned) visual state, dirty tracking, HTML diff generation
- `include/nv_ui.h`    — single public header: type enum, state union, composite API
- `tests/`             — unit tests

## Public headers
- `include/nv_ui.h` — exposes `nv_component_type_t`, `nv_component_t`, type-specific
  state union, `nv_component_init`/`paint`/`resize`/`destroy`, `nv_component_add_owned`,
  bind/emit event, and `NV_STATE` / `NV_SET_STATE`. Designer-facing `nv_*_new` (e.g.
  `nv_button_new`) return `nv_component_t*` with type and state set.

## Component model (composite)
- **One struct**: `nv_component_t` with `type`, `arena`, `name`, owner/parent,
  `owned_head`/`owned_next`, event slots, and `nv_component_state_t` union.
- **Operations**: `nv_component_paint(comp, canvas)`, `nv_component_resize(comp, w, h)`,
  `nv_component_destroy(comp)` — no vtable; dispatch by `comp->type`, recurse for containers.
- **Types**: `NV_COMP_FORM`, `NV_COMP_BUTTON`, `NV_COMP_LABEL`, `NV_COMP_INPUT`, `NV_COMP_PANEL`,
  `NV_COMP_CHECKBOX`, `NV_COMP_SELECT`, `NV_COMP_SLIDER`, `NV_COMP_TEXTAREA`, `NV_COMP_VUE`.
- **API aliases**: `nv_form_t`, `nv_button_t`, etc. are typedefs to `nv_component_t`.

## Owner / Parent Duality
- **Owner** — memory responsibility. Freeing the owner frees all owned
  components. Implemented as a linked list of owned children.
- **Parent** — visual hierarchy. Determines layout position and clipping.
  A component can have a different owner and parent.

## NV_STATE Reactive Macro Contract
```c
NV_COMPONENT(nv_button) {
    NV_STATE(const char*, caption, "Click me");
    NV_STATE(int,         enabled, 1);
    NV_EVENT(on_click);
};
```
- `NV_SET_STATE(comp, field, value)` marks the field dirty and schedules a diff.
- The diff engine batches all dirty components per frame and sends one minimal
  DOM patch to the WebView via `nv_ipc`.
- Do NOT call `nv_eval_js` directly from widget code — always go through the
  diff engine.

## Designer Output Contract
`nv-designer` generates `myform_ui.c` files that call `nv_*_new(...)` functions
from this module. The API surface of `nv_ui.h` is therefore the designer's
output language. Any breaking change to `nv_*_new` signatures requires updating
the designer's code generator simultaneously.

## Allowed dependencies
- `nv-ipc` → `nv-core`
- `nv-runtime` (for `nv_window_t*` handle in `nv_form_t`)
- C11 stdlib

## Forbidden
- Do NOT call WebView APIs directly — use `nv_ipc` for all JS communication
- Do NOT create OS windows directly — wrap `nv_window_t` from `nv-runtime`
- Do NOT use bare `malloc`/`free` — use the component's arena
- Do NOT change the LICENSE file
- Do NOT add platform `#ifdef` blocks — nv-ui is platform-agnostic

## Vue Slot (NV_COMP_VUE, Option B)
- C owns a rectangular slot; Vue mounts via `createApp` per slot.
- **HTML inside a VUE slot is owned entirely by JS; C must not touch it.**
- Use `nv_vue_load_component(slot, js_fn_body)` — `js_fn_body` returns the component def, e.g.
  `"return { template: '<div>Hi</div>', props: ['foo'] };"`
- Use `nv_vue_set_props(slot, props_json)` to merge new props into the slot's reactive props.

## Coding rules
- C11, `-Wall -Wextra` clean
- Every new widget type needs a test in `tests/`
- Max 800 lines per `.c` file — split if exceeded (per project principles)
- All `nv_*_new` functions must be NULL-safe on all pointer arguments
- ABI: never change the layout of `nv_component_t` without a major version bump

## Task labels
- `[nv-ui]`              — scoped to this module
- `[nv-ui][widget]`      — adding or modifying a widget type
- `[nv-ui][diff]`        — changes to the DOM diff engine
- `[nv-ui][abi]`         — changes that affect public struct layout
- `[nv-ui][designer-api]`— changes to nv_*_new signatures (affects nv-designer)
