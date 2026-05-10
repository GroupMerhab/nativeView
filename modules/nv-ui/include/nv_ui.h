/* =============================================================================
 * nv_ui.h - NativeView UI Component Model (Public API)
 *
 * Composite model: single nv_component_t node type with type enum and
 * type-specific state; paint/destroy/resize switch on type and walk the tree.
 *
 * Apache 2.0 - See LICENSE for details
 * ============================================================================= */

#ifndef NV_UI_H
#define NV_UI_H

#include "nv_arena.h"
#include "nv_util.h"
#include "nv.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Visibility macros when not defined by build */
#ifndef NV_API
#define NV_API
#endif
#ifndef NV_INTERNAL
#define NV_INTERNAL
#endif

/* -----------------------------------------------------------------------------
 * Component type (single node kind; behavior by type)
 * ----------------------------------------------------------------------------- */
typedef enum nv_component_type {
  NV_COMP_FORM,
  NV_COMP_BUTTON,
  NV_COMP_LABEL,
  NV_COMP_INPUT,
  NV_COMP_PANEL,
  NV_COMP_CHECKBOX,
  NV_COMP_SELECT,
  NV_COMP_SLIDER,
  NV_COMP_TEXTAREA,
  NV_COMP_VUE,   /* Option B: createApp() per slot; C owns geometry, Vue owns DOM */
  NV_COMP_TYPE_COUNT
} nv_component_type_t;

/* -----------------------------------------------------------------------------
 * Forward declaration
 * ----------------------------------------------------------------------------- */
struct nv_component;
typedef struct nv_component nv_component_t;

struct nv_canvas;
typedef struct nv_canvas nv_canvas_t;

/* -----------------------------------------------------------------------------
 * Type-specific state (union for ABI; no extra alloc)
 * ----------------------------------------------------------------------------- */
#define NV_OBJECT_NAME_MAX 64

/* forward-declare so nv_form_state_t can hold a pointer */
struct nv_form_snap;

typedef struct nv_form_state {
  nv_window_t*       window;       /* set when form is bound to a window */
  int                width;
  int                height;
  int                dirty;        /* 1 = needs paint/flush */
  int                initialized;  /* 1 = Vue scaffold already loaded into WebView */
  struct nv_form_snap* snap;       /* snapshot table for Vue patch diffing; NULL until first flush */
} nv_form_state_t;

typedef struct nv_button_state {
  const char* caption;
  int         enabled;
  int         x, y, w, h;
} nv_button_state_t;

typedef struct nv_label_state {
  const char* text;
  int         x, y, w, h;
} nv_label_state_t;

typedef struct nv_input_state {
  const char* text;
  const char* placeholder;
  int         x, y, w, h;
} nv_input_state_t;

typedef struct nv_panel_state {
  int x, y, w, h;
} nv_panel_state_t;

typedef struct nv_checkbox_state {
  int         checked;
  const char* label;
  int         x, y, w, h;
} nv_checkbox_state_t;

typedef struct nv_select_state {
  const char** items;
  int          item_count;
  int          selected_index;
  int          x, y, w, h;
} nv_select_state_t;

typedef struct nv_slider_state {
  float value;
  float min;
  float max;
  float step;
  int   x, y, w, h;
  void* shm;   /* optional: nv_shm_t* for fast-path live reads (Phase 4) */
  size_t shm_offset;  /* byte offset in shm for value (0 = first float) */
} nv_slider_state_t;

typedef struct nv_textarea_state {
  const char* text;
  int         rows;
  int         x, y, w, h;
} nv_textarea_state_t;

typedef struct nv_vue_state {
  const char* component_name;  /* optional display name */
  const char* props_json;      /* initial props as JSON; caller keeps alive */
  int         x, y, w, h;
} nv_vue_state_t;

typedef union nv_component_state {
  nv_form_state_t     form;
  nv_button_state_t   button;
  nv_label_state_t    label;
  nv_input_state_t    input;
  nv_panel_state_t    panel;
  nv_checkbox_state_t checkbox;
  nv_select_state_t   select_;   /* select is a C++ keyword; use select_ */
  nv_slider_state_t   slider;
  nv_textarea_state_t textarea;
  nv_vue_state_t     vue;
} nv_component_state_t;

/* -----------------------------------------------------------------------------
 * Component snapshot (used by nv_widget.c for Vue patch diffing)
 * One entry per component; stored in the form's arena after first flush.
 * ----------------------------------------------------------------------------- */
#define NV_SNAP_MAX_COMPS 256

typedef struct nv_comp_snap {
  char                name[NV_OBJECT_NAME_MAX];
  nv_component_type_t type;
  union {
    struct { const char* text;        int x, y, w, h; }                                  label;
    struct { const char* caption; int enabled; int x, y, w, h; }                         button;
    struct { const char* text; const char* placeholder; int x, y, w, h; }               input;
    struct { int checked; const char* label; int x, y, w, h; }                           checkbox;
    struct { int selected_index; int x, y, w, h; }                                       select_;
    struct { float value; float min; float max; float step; int x, y, w, h; }            slider;
    struct { const char* text; int rows; int x, y, w, h; }                               textarea;
  } s;
} nv_comp_snap_t;

/* Snapshot table attached to a form (allocated in form arena on first flush) */
typedef struct nv_form_snap {
  nv_comp_snap_t entries[NV_SNAP_MAX_COMPS];
  int            count;
} nv_form_snap_t;

/* -----------------------------------------------------------------------------
 * Config structs (input to nv_*_new; caller keeps string pointers alive)
 * ----------------------------------------------------------------------------- */
typedef struct nv_form_cfg {
  const char* title;
  int         width;
  int         height;
} nv_form_cfg_t;

typedef struct nv_button_cfg {
  const char* name;
  const char* caption;
  int         enabled;
  int         x, y, w, h;
} nv_button_cfg_t;

typedef struct nv_label_cfg {
  const char* name;
  const char* text;
  int         x, y, w, h;
} nv_label_cfg_t;

typedef struct nv_input_cfg {
  const char* name;
  const char* text;
  const char* placeholder;
  int         x, y, w, h;
} nv_input_cfg_t;

typedef struct nv_panel_cfg {
  const char* name;
  int         x, y, w, h;
} nv_panel_cfg_t;

typedef struct nv_checkbox_cfg {
  const char* name;
  const char* label;
  int         checked;
  int         x, y, w, h;
} nv_checkbox_cfg_t;

typedef struct nv_select_cfg {
  const char*  name;
  const char** items;
  int          item_count;
  int          selected_index;
  int          x, y, w, h;
} nv_select_cfg_t;

typedef struct nv_slider_cfg {
  const char* name;
  float       value;
  float       min;
  float       max;
  float       step;
  int         x, y, w, h;
} nv_slider_cfg_t;

typedef struct nv_textarea_cfg {
  const char* name;
  const char* text;
  int         rows;
  int         x, y, w, h;
} nv_textarea_cfg_t;

typedef struct nv_vue_cfg {
  const char* name;
  const char* component_name;  /* optional; for display/debug */
  const char* props_json;      /* optional initial props JSON; caller keeps alive */
  int         x, y, w, h;
} nv_vue_cfg_t;

/* -----------------------------------------------------------------------------
 * Event slots (fixed size; slot index 0..NV_MAX_EVENTS-1)
 * ----------------------------------------------------------------------------- */
#define NV_MAX_EVENTS 8

typedef void (*nv_event_fn_t)(nv_component_t* comp, void* ctx);

/* -----------------------------------------------------------------------------
 * Component (single node type: type + arena + name + tree + events + state)
 * ----------------------------------------------------------------------------- */
struct nv_component {
  nv_component_type_t  type;
  nv_arena_t*          arena;
  char                 name[NV_OBJECT_NAME_MAX];
  nv_component_t*      owner;
  nv_component_t*      parent;
  nv_component_t*      owned_head;
  nv_component_t*      owned_next;
  nv_event_fn_t        events[NV_MAX_EVENTS];
  void*                event_ctx[NV_MAX_EVENTS];
  int                  event_count;
  nv_component_state_t state;
};

/* API compatibility: widget types are the same as component */
typedef nv_component_t nv_form_t;
typedef nv_component_t nv_button_t;
typedef nv_component_t nv_label_t;
typedef nv_component_t nv_input_t;
typedef nv_component_t nv_panel_t;
typedef nv_component_t nv_checkbox_t;
typedef nv_component_t nv_select_t;
typedef nv_component_t nv_slider_t;
typedef nv_component_t nv_textarea_t;
typedef nv_component_t nv_vue_t;

/* -----------------------------------------------------------------------------
 * Event slot indices (use with nv_component_bind_event / nv_component_emit_event)
 * ----------------------------------------------------------------------------- */
#define NV_EVENT_CLICK   0  /* button / checkbox click */
#define NV_EVENT_INPUT   1  /* input / slider / textarea live change */
#define NV_EVENT_CHANGE  2  /* input / select / textarea committed change */

/* -----------------------------------------------------------------------------
 * Macros for reactive state and events
 * ----------------------------------------------------------------------------- */
#define NV_STATE(type, field, default_value)  type field
#define NV_EVENT(name)  /* placeholder for event slot name */

NV_INTERNAL void nv_ui_schedule_diff(nv_component_t* comp);
/** Flush dirty form: paint tree to HTML and send to form's window. No-op if form has no window or not dirty. */
NV_INTERNAL void nv_ui_flush(nv_form_t* form);

#define NV_SET_STATE(comp, field, value) \
  do { (comp)->field = (value); nv_ui_schedule_diff((nv_component_t*)(comp)); } while (0)

/* -----------------------------------------------------------------------------
 * Composite operations (no vtable; dispatch by type)
 * ----------------------------------------------------------------------------- */
NV_INTERNAL void nv_component_paint(nv_component_t* comp, nv_canvas_t* canvas);
#ifdef NV_TEST
NV_INTERNAL nv_canvas_t* nv_canvas_create_for_test(nv_arena_t* arena);
NV_INTERNAL const char* nv_canvas_get_html(nv_canvas_t* canvas);
#endif
NV_INTERNAL void nv_component_resize(nv_component_t* comp, int w, int h);
NV_INTERNAL void nv_component_destroy(nv_component_t* comp);

/* -----------------------------------------------------------------------------
 * Component lifecycle and tree
 * ----------------------------------------------------------------------------- */
NV_INTERNAL void nv_component_init(nv_component_t* comp, nv_component_type_t type,
                                   nv_component_t* owner, nv_component_t* parent,
                                   nv_arena_t* arena, const char* name);
NV_INTERNAL void nv_component_add_owned(nv_component_t* owner, nv_component_t* child);
/** Unlink child from owner's owned list. Child remains allocated (arena); tree structure changes require full scaffold reload. */
NV_API void nv_component_remove(nv_component_t* owner, nv_component_t* child);
NV_INTERNAL void nv_component_bind_event(nv_component_t* comp, int slot,
                                         nv_event_fn_t fn, void* ctx);
NV_INTERNAL void nv_component_emit_event(nv_component_t* comp, int slot);
/** Walk the subtree rooted at root; return first node whose name equals name, or NULL. */
NV_INTERNAL nv_component_t* nv_component_find_by_name(nv_component_t* root, const char* name);
/** Walk up owner chain; return the form ancestor, or NULL. */
NV_INTERNAL nv_form_t* nv_component_get_form(nv_component_t* comp);

/* -----------------------------------------------------------------------------
 * Creation API (designer and apps)
 * ----------------------------------------------------------------------------- */
NV_API nv_form_t*     nv_form_new(nv_app_t* app, const nv_form_cfg_t* cfg);
NV_API nv_button_t*   nv_button_new(nv_component_t* owner, const nv_button_cfg_t* cfg);
NV_API nv_label_t*    nv_label_new(nv_component_t* owner, const nv_label_cfg_t* cfg);
NV_API nv_input_t*    nv_input_new(nv_component_t* owner, const nv_input_cfg_t* cfg);
NV_API nv_panel_t*    nv_panel_new(nv_component_t* owner, const nv_panel_cfg_t* cfg);
NV_API nv_checkbox_t* nv_checkbox_new(nv_component_t* owner, const nv_checkbox_cfg_t* cfg);
NV_API nv_select_t*   nv_select_new(nv_component_t* owner, const nv_select_cfg_t* cfg);
NV_API nv_slider_t*   nv_slider_new(nv_component_t* owner, const nv_slider_cfg_t* cfg);
/** Attach shared memory for fast-path live value reads (Phase 4). Platform must register buffer with JS via shm_register. */
NV_API void nv_slider_attach_shm(nv_slider_t* slider, void* shm);
NV_API nv_textarea_t* nv_textarea_new(nv_component_t* owner, const nv_textarea_cfg_t* cfg);
NV_API nv_vue_t*     nv_vue_new(nv_component_t* owner, const nv_vue_cfg_t* cfg);

/** Load a Vue component into a slot (Option B: createApp per slot). js_fn_body must return the component def, e.g. "return { template: '<div>Hi</div>' };". */
NV_API void nv_vue_load_component(nv_vue_t* slot, const char* js_fn_body);

/** Update slot props; props_json is merged into the reactive props object. */
NV_API void nv_vue_set_props(nv_vue_t* slot, const char* props_json);

/** Bind form to a window (so flush sends HTML to that window). */
NV_API void nv_form_set_window(nv_form_t* form, nv_window_t* window);

#ifdef __cplusplus
}
#endif

#endif /* NV_UI_H */
