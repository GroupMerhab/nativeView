# Vue slot guide — `NV_COMP_VUE`

Use a **Vue slot** when built-in widgets (button, label, input, …) are not enough. C owns the slot's **position and size**; **Vue owns all DOM inside the slot**.

Header: [`nv_ui.h`](../modules/nv-ui/include/nv_ui.h)  
Implementation: [`nv_vue_slot.c`](../modules/nv-ui/src/nv_vue_slot.c)

---

## Rules

1. Create the slot on a form with `nv_vue_new`.
2. Load the form scaffold first (`nv_form_set_window` + `nv_ui_flush`) so the WebView is initialized.
3. Call `nv_vue_load_component` to mount Vue via `createApp` in that slot.
4. Update props with `nv_vue_set_props` — do **not** write HTML into the slot from C.
5. Handle JS → C events through the normal `nv.event` bridge or custom `NativeView` handlers.

---

## Step-by-step

### 1. Create form and slot

```c
#include "nv.h"
#include "nv_ui.h"

nv_app_t* app = nv_app_create();
nv_form_cfg_t fcfg = { .title = "Vue Slot Demo", .width = 640, .height = 480 };
nv_form_t* form = nv_form_new(app, &fcfg);

nv_vue_cfg_t vcfg = {
  .name = "chart_slot",
  .component_name = "Chart",
  .props_json = "{\"title\":\"Sales\"}",
  .x = 10, .y = 10, .w = 600, .h = 400
};
nv_vue_t* slot = nv_vue_new(form, &vcfg);
```

### 2. Show window and flush scaffold

```c
nv_window_cfg_t wcfg = { .title = "Vue Slot", .width = 640, .height = 480, .resizable = 1 };
nv_window_t* win = nv_window_create(app, &wcfg);
nv_form_set_window(form, win);
nv_window_show(win);
nv_ui_flush(form);   /* first flush: full Vue scaffold */
```

### 3. Load a Vue component

`js_fn_body` is the **body** of a function that **returns** a Vue component definition:

```c
const char* chart_def =
  "return {"
  "  props: ['title'],"
  "  template: '<motion.div><h2>{{ title }}</h2></motion.div>'"
  "};";

/* After form is initialized in the WebView: */
nv_vue_load_component(slot, chart_def);
```

This evaluates `window.__nv_load_vue_component(name, fnBody, initialPropsJson)` in the WebView.

### 4. Update props from C

```c
nv_vue_set_props(slot, "{\"title\":\"Q2 Revenue\",\"value\":42}");
```

Merges into the slot's reactive props object via `window.__nv_set_vue_props`.

### 5. Run the loop

```c
nv_app_run(app);
nv_app_destroy(app);
```

---

## Component definition shape

The JS function body should return a standard Vue 3 options object:

```javascript
return {
  props: ['foo', 'bar'],
  data() { return { local: 0 }; },
  template: '<motion.div>{{ foo }} — {{ local }}</motion.div>',
  methods: {
    bump() { this.local++; }
  }
};
```

Use `props` for values driven from C via `nv_vue_set_props`.

---

## Events (JS → C)

Inside the Vue component, use the bridge to notify C:

```javascript
NativeView.send('chart.clicked', { id: 1 });
```

Register in C with `nv_on_message` on the window, or bind nv-ui events on sibling widgets.

---

## Option B architecture

Each slot gets its own `createApp` instance (isolated from the form scaffold app). This avoids prop/event collisions between multiple custom components on one form.

```
Form (Vue scaffold app)
├── Button, Label, …  (scaffold widgets)
└── chart_slot        (separate createApp — C sets geometry only)
```

---

## Troubleshooting

| Issue | Fix |
|-------|-----|
| `nv_vue_load_component` no-op | Ensure `nv_ui_flush` ran once and `form->state.form.initialized` is set |
| Blank slot | Check `js_fn_body` syntax; open WebView devtools |
| Props not updating | Pass valid JSON to `nv_vue_set_props`; keep strings alive until call returns |
| C mutates slot HTML | **Don't** — Vue owns slot DOM after mount |

---

## See also

- [widget-reference.md](widget-reference.md) — built-in widgets
- [architecture.md](architecture.md) — flush / diff cycle
- [UI_UX_doc.md](UI_UX_doc.md) — UX principles
