# Widget reference — nv-ui

C API for the reactive UI layer (`modules/nv-ui`). Vue owns the DOM after the first flush; C updates state and sends JSON patches.

Header: [`modules/nv-ui/include/nv_ui.h`](../modules/nv-ui/include/nv_ui.h)  
Example: [`examples/counter_ui/`](../examples/counter_ui/)

---

## Common concepts

### Geometry

All widgets support absolute layout via `x`, `y`, `w`, `h` (pixels) in both `*_cfg_t` and state structs.

### Events

Bind with `nv_component_bind_event(comp, slot, fn, ctx)`:

| Slot | Constant | Typical widgets |
|------|----------|-----------------|
| Click | `NV_EVENT_CLICK` (0) | Button, Checkbox |
| Input | `NV_EVENT_INPUT` (1) | Input, Slider, Textarea |
| Change | `NV_EVENT_CHANGE` (2) | Input, Select, Textarea |

### Reactive updates

```c
NV_SET_STATE(widget, state.label.text, "New text");
nv_ui_flush(form);
```

`NV_SET_STATE` schedules a diff; `nv_ui_flush` sends the patch to the WebView.

### Form

| Function | Description |
|----------|-------------|
| `nv_form_new(app, cfg)` | Create root form |
| `nv_form_set_window(form, window)` | Bind form to a window for flush |

**`nv_form_cfg_t`:** `title`, `width`, `height`

---

## Form (`NV_COMP_FORM`)

Root container. Holds child widgets; owns arena memory.

```c
nv_form_cfg_t cfg = { .title = "My App", .width = 800, .height = 600 };
nv_form_t* form = nv_form_new(app, &cfg);
```

---

## Button (`NV_COMP_BUTTON`)

| Field | `nv_button_cfg_t` / state |
|-------|---------------------------|
| `name` | Object name (for events / diff) |
| `caption` | Button label |
| `enabled` | `1` = enabled |
| `x`, `y`, `w`, `h` | Geometry |

```c
nv_button_cfg_t cfg = {
  .name = "btn_inc", .caption = "Increment",
  .enabled = 1, .x = 20, .y = 20, .w = 120, .h = 32
};
nv_button_t* btn = nv_button_new(form, &cfg);
nv_component_bind_event((nv_component_t*)btn, NV_EVENT_CLICK, on_click, ctx);
```

**Events:** `NV_EVENT_CLICK`

---

## Label (`NV_COMP_LABEL`)

| Field | Description |
|-------|-------------|
| `name` | Object name |
| `text` | Display string (caller keeps alive) |
| `x`, `y`, `w`, `h` | Geometry |

```c
nv_label_cfg_t cfg = { .name = "lbl", .text = "Count: 0", .x = 20, .y = 60, .w = 200, .h = 24 };
nv_label_t* lbl = nv_label_new(form, &cfg);
```

---

## Input (`NV_COMP_INPUT`)

| Field | Description |
|-------|-------------|
| `text` | Current value |
| `placeholder` | Placeholder text |
| `x`, `y`, `w`, `h` | Geometry |

**Events:** `NV_EVENT_INPUT`, `NV_EVENT_CHANGE`

---

## Panel (`NV_COMP_PANEL`)

Layout container (geometry only).

| Field | `nv_panel_cfg_t` |
|-------|------------------|
| `name`, `x`, `y`, `w`, `h` | |

---

## Checkbox (`NV_COMP_CHECKBOX`)

| Field | Description |
|-------|-------------|
| `label` | Checkbox label |
| `checked` | `0` or `1` |
| `x`, `y`, `w`, `h` | Geometry |

**Events:** `NV_EVENT_CLICK` (payload includes checked state in JS bridge)

---

## Select (`NV_COMP_SELECT`)

| Field | Description |
|-------|-------------|
| `items` | `const char**` array |
| `item_count` | Number of options |
| `selected_index` | Zero-based index |
| `x`, `y`, `w`, `h` | Geometry |

**Events:** `NV_EVENT_CHANGE`

---

## Slider (`NV_COMP_SLIDER`)

| Field | Description |
|-------|-------------|
| `value`, `min`, `max`, `step` | `float` |
| `x`, `y`, `w`, `h` | Geometry |

Optional: `nv_slider_attach_shm(slider, shm)` for shared-memory fast path.

**Events:** `NV_EVENT_INPUT`

---

## Textarea (`NV_COMP_TEXTAREA`)

| Field | Description |
|-------|-------------|
| `text` | Content |
| `rows` | Visible row count |
| `x`, `y`, `w`, `h` | Geometry |

**Events:** `NV_EVENT_INPUT`, `NV_EVENT_CHANGE`

---

## Vue slot (`NV_COMP_VUE`)

Embeds a custom Vue component in a rectangular region. See [vue-slot-guide.md](vue-slot-guide.md).

| Field | Description |
|-------|-------------|
| `component_name` | Optional debug name |
| `props_json` | Initial props JSON string |
| `x`, `y`, `w`, `h` | Slot geometry |

---

## Tree utilities

| Function | Description |
|----------|-------------|
| `nv_component_find_by_name(root, name)` | Lookup by name |
| `nv_component_remove(owner, child)` | Unlink child (arena-backed) |

---

## Build requirement

nv-ui requires the Vue bundle:

```bash
./tools/fetch_vue.sh
cmake -S . -B build -DNV_BUILD_UI=ON
cmake --build build --target counter_ui
```

Disable UI stack: `-DNV_BUILD_UI=OFF`.
