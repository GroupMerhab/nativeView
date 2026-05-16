/* Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0 */

/* =============================================================================
 * nv_widget.c - Vue 3 reactive bridge, schedule_diff, flush, paint
 *
 * First flush: loads a full Vue 3 scaffold (HTML + reactive data) via
 * nv_load_html.  Every subsequent flush walks the component tree, diffs
 * against the stored snapshot, and calls nv_eval_js with a minimal JSON
 * patch so Vue surgically updates only the changed DOM nodes.
 *
 * Apache 2.0 - See LICENSE for details
 * ============================================================================= */

#include "nv_ui.h"
#include "nv_vue_bundle.h"
#include "nv_str.h"
#include "nv_json.h"
#include "nv_arena.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * schedule_diff
 * ========================================================================== */

NV_INTERNAL void nv_ui_schedule_diff(nv_component_t* comp) {
  if (!comp) return;
  nv_component_t* p = comp->parent;
  while (p) {
    if (p->type == NV_COMP_FORM) {
      p->state.form.dirty = 1;
      return;
    }
    p = p->parent;
  }
  if (comp->type == NV_COMP_FORM)
    comp->state.form.dirty = 1;
}

/* ============================================================================
 * Vue scaffold builder
 *
 * Generates a self-contained HTML page with:
 *   - Vue 3 (CDN)
 *   - A reactive `nvData.comps` object pre-populated with all component states
 *   - A `window.__nv_patch(patches)` function for surgical updates
 *   - The same delegated event bridge as before
 * ========================================================================== */

/* Escape a string for embedding inside a JS string literal (double-quoted). */
static void str_append_js_str(nv_str_t* s, const char* v) {
  if (!s || !v) { nv_str_append(s, ""); return; }
  for (; *v; v++) {
    switch (*v) {
      case '\\': nv_str_append(s, "\\\\"); break;
      case '"':  nv_str_append(s, "\\\""); break;
      case '\n': nv_str_append(s, "\\n");  break;
      case '\r': nv_str_append(s, "\\r");  break;
      case '\t': nv_str_append(s, "\\t");  break;
      default:   nv_str_append_len(s, v, 1); break;
    }
  }
}

/*
 * Append geometry fields into a JS object literal (no trailing comma).
 * Caller must have already opened the object with '{'.
 * Emits: x:N,y:N,w:N,h:N
 */
static void append_geom_js(nv_str_t* s, int x, int y, int w, int h) {
  char buf[64];
  snprintf(buf, sizeof(buf), "x:%d,y:%d,w:%d,h:%d", x, y, w, h);
  nv_str_append(s, buf);
}

/*
 * Append the initial JS object for one component into the `comps` map.
 * Format:  "<name>": { type:"button", id:"<name>", ..., x:N,y:N,w:N,h:N },
 */
static void append_comp_js_entry(nv_str_t* s, nv_component_t* comp) {
  if (!s || !comp || !comp->name[0]) return;
  char buf[64];

  nv_str_append(s, "\"");
  str_append_js_str(s, comp->name);
  nv_str_append(s, "\":{");

  switch (comp->type) {
    case NV_COMP_LABEL:
      nv_str_append(s, "type:\"label\",id:\"");
      str_append_js_str(s, comp->name);
      nv_str_append(s, "\",text:\"");
      str_append_js_str(s, comp->state.label.text ? comp->state.label.text : "");
      nv_str_append(s, "\",");
      append_geom_js(s, comp->state.label.x, comp->state.label.y,
                        comp->state.label.w, comp->state.label.h);
      nv_str_append(s, "}");
      break;

    case NV_COMP_BUTTON:
      nv_str_append(s, "type:\"button\",id:\"");
      str_append_js_str(s, comp->name);
      nv_str_append(s, "\",caption:\"");
      str_append_js_str(s, comp->state.button.caption ? comp->state.button.caption : "");
      snprintf(buf, sizeof(buf), "\",enabled:%s,",
               comp->state.button.enabled ? "true" : "false");
      nv_str_append(s, buf);
      append_geom_js(s, comp->state.button.x, comp->state.button.y,
                        comp->state.button.w, comp->state.button.h);
      nv_str_append(s, "}");
      break;

    case NV_COMP_INPUT:
      nv_str_append(s, "type:\"input\",id:\"");
      str_append_js_str(s, comp->name);
      nv_str_append(s, "\",text:\"");
      str_append_js_str(s, comp->state.input.text ? comp->state.input.text : "");
      nv_str_append(s, "\",placeholder:\"");
      str_append_js_str(s, comp->state.input.placeholder ? comp->state.input.placeholder : "");
      nv_str_append(s, "\",");
      append_geom_js(s, comp->state.input.x, comp->state.input.y,
                        comp->state.input.w, comp->state.input.h);
      nv_str_append(s, "}");
      break;

    case NV_COMP_PANEL:
      nv_str_append(s, "type:\"panel\",id:\"");
      str_append_js_str(s, comp->name);
      nv_str_append(s, "\",");
      append_geom_js(s, comp->state.panel.x, comp->state.panel.y,
                        comp->state.panel.w, comp->state.panel.h);
      nv_str_append(s, "}");
      break;

    case NV_COMP_CHECKBOX:
      nv_str_append(s, "type:\"checkbox\",id:\"");
      str_append_js_str(s, comp->name);
      nv_str_append(s, "\",label:\"");
      str_append_js_str(s, comp->state.checkbox.label ? comp->state.checkbox.label : "");
      snprintf(buf, sizeof(buf), "\",checked:%s,",
               comp->state.checkbox.checked ? "true" : "false");
      nv_str_append(s, buf);
      append_geom_js(s, comp->state.checkbox.x, comp->state.checkbox.y,
                        comp->state.checkbox.w, comp->state.checkbox.h);
      nv_str_append(s, "}");
      break;

    case NV_COMP_SELECT: {
      nv_str_append(s, "type:\"select\",id:\"");
      str_append_js_str(s, comp->name);
      snprintf(buf, sizeof(buf), "\",selectedIndex:%d,items:[",
               comp->state.select_.selected_index);
      nv_str_append(s, buf);
      for (int i = 0; i < comp->state.select_.item_count; i++) {
        if (i > 0) nv_str_append(s, ",");
        nv_str_append(s, "\"");
        str_append_js_str(s, comp->state.select_.items[i] ? comp->state.select_.items[i] : "");
        nv_str_append(s, "\"");
      }
      nv_str_append(s, "],");
      append_geom_js(s, comp->state.select_.x, comp->state.select_.y,
                        comp->state.select_.w, comp->state.select_.h);
      nv_str_append(s, "}");
      break;
    }

    case NV_COMP_SLIDER:
      nv_str_append(s, "type:\"slider\",id:\"");
      str_append_js_str(s, comp->name);
      snprintf(buf, sizeof(buf), "\",value:%g,min:%g,max:%g,step:%g,",
               (double)comp->state.slider.value, (double)comp->state.slider.min,
               (double)comp->state.slider.max,   (double)comp->state.slider.step);
      nv_str_append(s, buf);
      append_geom_js(s, comp->state.slider.x, comp->state.slider.y,
                        comp->state.slider.w, comp->state.slider.h);
      nv_str_append(s, "}");
      break;

    case NV_COMP_TEXTAREA:
      nv_str_append(s, "type:\"textarea\",id:\"");
      str_append_js_str(s, comp->name);
      nv_str_append(s, "\",text:\"");
      str_append_js_str(s, comp->state.textarea.text ? comp->state.textarea.text : "");
      snprintf(buf, sizeof(buf), "\",rows:%d,", comp->state.textarea.rows);
      nv_str_append(s, buf);
      append_geom_js(s, comp->state.textarea.x, comp->state.textarea.y,
                        comp->state.textarea.w, comp->state.textarea.h);
      nv_str_append(s, "}");
      break;

    default:
      nv_str_append(s, "}");
      break;
  }
}

/* Recursively walk tree and append JS entries for all named non-form nodes.
 * VUE slots are skipped (they are mount points, not reactive comps). */
static void walk_comps_js(nv_str_t* s, nv_component_t* comp, int* first) {
  if (!comp) return;
  if (comp->type != NV_COMP_FORM && comp->type != NV_COMP_VUE && comp->name[0]) {
    if (!*first) nv_str_append(s, ",");
    append_comp_js_entry(s, comp);
    *first = 0;
  }
  for (nv_component_t* c = comp->owned_head; c; c = c->owned_next)
    walk_comps_js(s, c, first);
}

/*
 * Append Vue template nodes for one component.
 * Labels  → <span>
 * Buttons → <button>
 * Inputs  → <input>
 * Panels  → <div> (recurse children)
 */
static void append_vue_template_node(nv_str_t* s, nv_component_t* comp);

static void walk_vue_template(nv_str_t* s, nv_component_t* comp) {
  for (nv_component_t* c = comp->owned_head; c; c = c->owned_next)
    append_vue_template_node(s, c);
}

/*
 * Emit a Vue :style binding that reads geometry from the reactive comps object.
 * This lets C-side position patches move widgets without a full reload.
 * Emitted as: :style="{position:'absolute',left:comps['id'].x+'px',...}"
 */
static void append_vue_style_binding(nv_str_t* s, const char* id) {
  nv_str_append(s, ":style=\"{position:'absolute',left:comps['");
  str_append_js_str(s, id);
  nv_str_append(s, "'].x+'px',top:comps['");
  str_append_js_str(s, id);
  nv_str_append(s, "'].y+'px',width:comps['");
  str_append_js_str(s, id);
  nv_str_append(s, "'].w+'px',height:comps['");
  str_append_js_str(s, id);
  nv_str_append(s, "'].h+'px'}\"");
}

static void append_vue_template_node(nv_str_t* s, nv_component_t* comp) {
  if (!comp || !comp->name[0]) return;
  const char* id = comp->name;

  switch (comp->type) {
    case NV_COMP_LABEL:
      nv_str_append(s, "<span v-if=\"comps['");
      str_append_js_str(s, id);
      nv_str_append(s, "']\" :id=\"comps['");
      str_append_js_str(s, id);
      nv_str_append(s, "'].id\" class=\"nv-label\" ");
      append_vue_style_binding(s, id);
      nv_str_append(s, ">{{ comps['");
      str_append_js_str(s, id);
      nv_str_append(s, "'].text }}</span>");
      break;

    case NV_COMP_BUTTON:
      nv_str_append(s, "<button v-if=\"comps['");
      str_append_js_str(s, id);
      nv_str_append(s, "']\" :id=\"comps['");
      str_append_js_str(s, id);
      nv_str_append(s, "'].id\" class=\"nv-button\" type=\"button\" :disabled=\"!comps['");
      str_append_js_str(s, id);
      nv_str_append(s, "'].enabled\" ");
      append_vue_style_binding(s, id);
      nv_str_append(s, ">{{ comps['");
      str_append_js_str(s, id);
      nv_str_append(s, "'].caption }}</button>");
      break;

    case NV_COMP_INPUT:
      nv_str_append(s, "<input v-if=\"comps['");
      str_append_js_str(s, id);
      nv_str_append(s, "']\" :id=\"comps['");
      str_append_js_str(s, id);
      nv_str_append(s, "'].id\" class=\"nv-input\" type=\"text\" :value=\"comps['");
      str_append_js_str(s, id);
      nv_str_append(s, "'].text\" :placeholder=\"comps['");
      str_append_js_str(s, id);
      nv_str_append(s, "'].placeholder\" ");
      append_vue_style_binding(s, id);
      nv_str_append(s, ">");
      break;

    case NV_COMP_PANEL: {
      char style[128];
      snprintf(style, sizeof(style),
               "position:absolute;left:%dpx;top:%dpx;width:%dpx;height:%dpx;",
               comp->state.panel.x, comp->state.panel.y,
               comp->state.panel.w, comp->state.panel.h);
      nv_str_append(s, "<div class=\"nv-panel\" id=\"");
      str_append_js_str(s, id);
      nv_str_append(s, "\" style=\"");
      nv_str_append(s, style);
      nv_str_append(s, "\">");
      walk_vue_template(s, comp);
      nv_str_append(s, "</div>");
      break;
    }

    case NV_COMP_CHECKBOX:
      nv_str_append(s, "<label v-if=\"comps['");
      str_append_js_str(s, id);
      nv_str_append(s, "']\" class=\"nv-checkbox\" ");
      append_vue_style_binding(s, id);
      nv_str_append(s, "><input type=\"checkbox\" :id=\"comps['");
      str_append_js_str(s, id);
      nv_str_append(s, "'].id\" class=\"nv-checkbox-input\" :checked=\"comps['");
      str_append_js_str(s, id);
      nv_str_append(s, "'].checked\"> {{ comps['");
      str_append_js_str(s, id);
      nv_str_append(s, "'].label }}</label>");
      break;

    case NV_COMP_SELECT:
      nv_str_append(s, "<select v-if=\"comps['");
      str_append_js_str(s, id);
      nv_str_append(s, "']\" :id=\"comps['");
      str_append_js_str(s, id);
      nv_str_append(s, "'].id\" class=\"nv-select\" ");
      append_vue_style_binding(s, id);
      nv_str_append(s, ">"
        "<option v-for=\"(item,i) in comps['");
      str_append_js_str(s, id);
      nv_str_append(s, "'].items\" :key=\"i\" :value=\"i\" :selected=\"i===comps['");
      str_append_js_str(s, id);
      nv_str_append(s, "'].selectedIndex\">{{ item }}</option></select>");
      break;

    case NV_COMP_SLIDER:
      nv_str_append(s, "<input v-if=\"comps['");
      str_append_js_str(s, id);
      nv_str_append(s, "']\" :id=\"comps['");
      str_append_js_str(s, id);
      nv_str_append(s, "'].id\" class=\"nv-slider\" type=\"range\" :value=\"comps['");
      str_append_js_str(s, id);
      nv_str_append(s, "'].value\" :min=\"comps['");
      str_append_js_str(s, id);
      nv_str_append(s, "'].min\" :max=\"comps['");
      str_append_js_str(s, id);
      nv_str_append(s, "'].max\" :step=\"comps['");
      str_append_js_str(s, id);
      nv_str_append(s, "'].step\" ");
      append_vue_style_binding(s, id);
      nv_str_append(s, ">");
      break;

    case NV_COMP_TEXTAREA:
      nv_str_append(s, "<textarea v-if=\"comps['");
      str_append_js_str(s, id);
      nv_str_append(s, "']\" :id=\"comps['");
      str_append_js_str(s, id);
      nv_str_append(s, "'].id\" class=\"nv-textarea\" :rows=\"comps['");
      str_append_js_str(s, id);
      nv_str_append(s, "'].rows\" ");
      append_vue_style_binding(s, id);
      nv_str_append(s, ">{{ comps['");
      str_append_js_str(s, id);
      nv_str_append(s, "'].text }}</textarea>");
      break;

    case NV_COMP_VUE: {
      /* Option B: empty div as mount point; createApp per slot */
      char style[128];
      snprintf(style, sizeof(style),
               "position:absolute;left:%dpx;top:%dpx;width:%dpx;height:%dpx;",
               comp->state.vue.x, comp->state.vue.y,
               comp->state.vue.w, comp->state.vue.h);
      nv_str_append(s, "<div id=\"nv-slot-");
      str_append_js_str(s, id);
      nv_str_append(s, "\" class=\"nv-vue-slot\" style=\"");
      nv_str_append(s, style);
      nv_str_append(s, "\"></div>");
      break;
    }

    default:
      break;
  }
}

/*
 * Build the full Vue 3 scaffold HTML string.
 * Returns a heap-allocated C string; caller must free().
 * Called directly by nv_widget_build_vue_scaffold() below.
 */
static char* build_vue_scaffold(nv_form_t* form) {
  /* Vue bundle (~165KB) + scaffold (~20KB) need a large arena. Pool arena
   * limits single allocations to chunk size (8KB), so use fixed arena. */
  nv_arena_t* arena = nv_arena_create(512 * 1024);
  if (!arena) return NULL;

  nv_str_t* s = nv_str_create(arena);
  if (!s) { nv_arena_destroy(arena); return NULL; }

  /* ---- HTML head ---- */
  nv_str_append(s,
    "<!DOCTYPE html><html><head>"
    "<meta charset=\"utf-8\">"
    "<script>"
  );
  nv_str_append_len(s, nv_vue_bundle(), nv_vue_bundle_len());
  nv_str_append(s, "</script>"
    "<style>"
    "body{font-family:system-ui,sans-serif;margin:0;padding:0;overflow:hidden;}"
    "#app{position:relative;width:100%;height:100%;}"
    "button{box-sizing:border-box;cursor:pointer;}"
    "button:disabled{opacity:0.5;cursor:default;}"
    "input,select,textarea{box-sizing:border-box;}"
    ".nv-label{display:inline-block;box-sizing:border-box;}"
    ".nv-checkbox{display:inline-flex;align-items:center;box-sizing:border-box;}"
    ".nv-slider{display:block;}"
    ".nv-textarea{display:block;resize:none;}"
    ".nv-vue-slot{overflow:hidden;box-sizing:border-box;}"
    "</style>"
    "</head><body>"
    "<div id=\"app\">"
  );

  /* ---- Vue template nodes ---- */
  walk_vue_template(s, (nv_component_t*)form);

  nv_str_append(s, "</div><script>(function(){"
    "try{"
    "if(typeof Vue==='undefined'){"
    "document.body.innerHTML='<p style=\"padding:20px;color:red\">Vue not loaded</p>';return;"
    "}"
  );

  /* ---- Initial reactive data ---- */
  nv_str_append(s, "const{createApp,reactive}=Vue;");
  nv_str_append(s, "const nvData=reactive({comps:{");
  int first = 1;
  walk_comps_js(s, (nv_component_t*)form, &first);
  nv_str_append(s, "}});");

  /* ---- __nv_patch function ---- */
  nv_str_append(s,
    "window.__nv_patch=function(patches){"
      "patches.forEach(function(p){"
        "if(nvData.comps[p.id])nvData.comps[p.id][p.prop]=p.value;"
      "});"
    "};"
  );

  /* ---- Vue slot helpers (Option B: createApp per slot) ---- */
  nv_str_append(s,
    "window.__nv_vue_instances={};"
    "window.__nv_load_vue_component=function(slotId,fnBody,propsJson){"
      "var el=document.getElementById('nv-slot-'+slotId);"
      "if(!el||typeof Vue==='undefined')return;"
      "try{"
        "var def=(new Function(fnBody))();"
        "var props=Vue.reactive(propsJson?JSON.parse(propsJson):{});"
        "var app=Vue.createApp(def,props);"
        "app.mount(el);"
        "window.__nv_vue_instances[slotId]={app:app,props:props};"
      "}catch(e){console.error('nv_load_vue_component',e);}"
    "};"
    "window.__nv_set_vue_props=function(slotId,propsJson){"
      "var inst=window.__nv_vue_instances[slotId];"
      "if(!inst||!inst.props)return;"
      "try{var p=JSON.parse(propsJson);"
      "for(var k in p)if(p.hasOwnProperty(k))inst.props[k]=p[k];"
      "}catch(e){console.error('nv_set_vue_props',e);}"
    "};"
  );

  /* ---- Mount Vue app ---- */
  nv_str_append(s,
    "createApp({setup(){return nvData;}}).mount('#app');"
  );

  /* ---- Event bridge (delegated) ---- */
  nv_str_append(s,
    "document.addEventListener('click',function(e){"
      "var t=e.target.closest('.nv-button');"
      "if(t&&window.__nv)window.__nv.send('nv.event',JSON.stringify({id:t.id,type:'click'}));"
      /* checkbox click: target is the <input type=checkbox> inside .nv-checkbox */
      "var cb=e.target.closest('.nv-checkbox-input');"
      "if(cb&&window.__nv)window.__nv.send('nv.event',JSON.stringify({id:cb.id,type:'click',value:cb.checked}));"
    "});"
    "document.addEventListener('input',function(e){"
      "var t=e.target;"
      "if(!window.__nv)return;"
      "if(t.classList.contains('nv-input')||t.classList.contains('nv-textarea'))"
        "window.__nv.send('nv.event',JSON.stringify({id:t.id,type:'input',value:t.value}));"
      "if(t.classList.contains('nv-slider'))"
        "window.__nv.send('nv.event',JSON.stringify({id:t.id,type:'input',value:parseFloat(t.value)}));"
    "});"
    "document.addEventListener('change',function(e){"
      "var t=e.target;"
      "if(!window.__nv)return;"
      "if(t.classList.contains('nv-input')||t.classList.contains('nv-textarea'))"
        "window.__nv.send('nv.event',JSON.stringify({id:t.id,type:'change',value:t.value}));"
      "if(t.classList.contains('nv-select'))"
        "window.__nv.send('nv.event',JSON.stringify({id:t.id,type:'change',value:parseInt(t.value)}));"
    "});"
    "}catch(e){"
    "document.body.innerHTML='<p style=\"padding:20px;color:red;font:14px sans-serif\">'+"
    "'Error: '+e.message+'</p><pre style=\"white-space:pre-wrap;font-size:11px\">'+e.stack+'</pre>';"
    "}"
  "})();</script></body></html>");

  /* Copy result to a plain heap string before destroying the arena */
  const char* raw = nv_str_cstr(s);
  char* result = NULL;
  if (raw) {
    size_t len = strlen(raw);
    result = (char*)malloc(len + 1);
    if (result) memcpy(result, raw, len + 1);
  }
  nv_arena_destroy(arena);
  return result;
}

/* ============================================================================
 * Snapshot helpers
 * ========================================================================== */

/* Find snapshot entry by name; returns NULL if not found */
static nv_comp_snap_t* snap_find(nv_form_snap_t* snap, const char* name) {
  if (!snap || !name) return NULL;
  for (int i = 0; i < snap->count; i++) {
    if (strcmp(snap->entries[i].name, name) == 0)
      return &snap->entries[i];
  }
  return NULL;
}

/* Add a new snapshot entry; returns NULL if table is full */
static nv_comp_snap_t* snap_add(nv_form_snap_t* snap, const char* name,
                                 nv_component_type_t type) {
  if (!snap || snap->count >= NV_SNAP_MAX_COMPS) return NULL;
  nv_comp_snap_t* e = &snap->entries[snap->count++];
  memset(e, 0, sizeof(*e));
  size_t n = strlen(name);
  if (n >= NV_OBJECT_NAME_MAX) n = NV_OBJECT_NAME_MAX - 1;
  memcpy(e->name, name, n);
  e->name[n] = '\0';
  e->type = type;
  return e;
}

/* ============================================================================
 * Patch JS builder
 *
 * Walks the component tree, diffs against the snapshot, and builds a
 * `window.__nv_patch([...])` JS call string containing only changed fields.
 * Also updates the snapshot to reflect the new state.
 * ========================================================================== */

/* Append one patch entry: {"id":"<id>","prop":"<prop>","value":"<val>"} */
static void append_patch_entry(nv_str_t* s, int* first,
                                const char* id, const char* prop, const char* val) {
  if (!*first) nv_str_append(s, ",");
  nv_str_append(s, "{\"id\":\"");
  str_append_js_str(s, id);
  nv_str_append(s, "\",\"prop\":\"");
  nv_str_append(s, prop);
  nv_str_append(s, "\",\"value\":\"");
  str_append_js_str(s, val ? val : "");
  nv_str_append(s, "\"}");
  *first = 0;
}

static void append_patch_bool(nv_str_t* s, int* first,
                               const char* id, const char* prop, int val) {
  if (!*first) nv_str_append(s, ",");
  nv_str_append(s, "{\"id\":\"");
  str_append_js_str(s, id);
  nv_str_append(s, "\",\"prop\":\"");
  nv_str_append(s, prop);
  nv_str_append(s, "\",\"value\":");
  nv_str_append(s, val ? "true" : "false");
  nv_str_append(s, "}");
  *first = 0;
}

/* Emit a numeric patch entry (integer) */
static void append_patch_int(nv_str_t* s, int* first,
                              const char* id, const char* prop, int val) {
  if (!*first) nv_str_append(s, ",");
  char buf[64];
  snprintf(buf, sizeof(buf),
           "{\"id\":\"%s\",\"prop\":\"%s\",\"value\":%d}", id, prop, val);
  nv_str_append(s, buf);
  *first = 0;
}

/* Emit a float patch entry */
static void append_patch_float(nv_str_t* s, int* first,
                                const char* id, const char* prop, float val) {
  if (!*first) nv_str_append(s, ",");
  char buf[64];
  snprintf(buf, sizeof(buf),
           "{\"id\":\"%s\",\"prop\":\"%s\",\"value\":%g}", id, prop, (double)val);
  nv_str_append(s, buf);
  *first = 0;
}

/* Diff geometry (x,y,w,h) between current and snapshot; emit patches for changed fields */
#define DIFF_GEOM(field_prefix, snap_field_prefix) \
  do { \
    if (comp->state.field_prefix.x != e->s.snap_field_prefix.x) { \
      append_patch_int(s, first, id, "x", comp->state.field_prefix.x); \
      e->s.snap_field_prefix.x = comp->state.field_prefix.x; \
    } \
    if (comp->state.field_prefix.y != e->s.snap_field_prefix.y) { \
      append_patch_int(s, first, id, "y", comp->state.field_prefix.y); \
      e->s.snap_field_prefix.y = comp->state.field_prefix.y; \
    } \
    if (comp->state.field_prefix.w != e->s.snap_field_prefix.w) { \
      append_patch_int(s, first, id, "w", comp->state.field_prefix.w); \
      e->s.snap_field_prefix.w = comp->state.field_prefix.w; \
    } \
    if (comp->state.field_prefix.h != e->s.snap_field_prefix.h) { \
      append_patch_int(s, first, id, "h", comp->state.field_prefix.h); \
      e->s.snap_field_prefix.h = comp->state.field_prefix.h; \
    } \
  } while (0)

static void diff_comp(nv_str_t* s, nv_component_t* comp,
                       nv_form_snap_t* snap, int* first) {
  if (!comp || !comp->name[0]) return;
  if (comp->type == NV_COMP_FORM || comp->type == NV_COMP_PANEL) {
    for (nv_component_t* c = comp->owned_head; c; c = c->owned_next)
      diff_comp(s, c, snap, first);
    return;
  }

  const char* id = comp->name;
  nv_comp_snap_t* e = snap_find(snap, id);
  if (!e) {
    e = snap_add(snap, id, comp->type);
    if (!e) return;
    /* New component — emit all fields as initial patches */
    switch (comp->type) {
      case NV_COMP_LABEL:
        append_patch_entry(s, first, id, "text", comp->state.label.text);
        e->s.label.text = comp->state.label.text;
        e->s.label.x = comp->state.label.x; e->s.label.y = comp->state.label.y;
        e->s.label.w = comp->state.label.w; e->s.label.h = comp->state.label.h;
        break;
      case NV_COMP_BUTTON:
        append_patch_entry(s, first, id, "caption", comp->state.button.caption);
        append_patch_bool(s, first, id, "enabled", comp->state.button.enabled);
        e->s.button.caption = comp->state.button.caption;
        e->s.button.enabled = comp->state.button.enabled;
        e->s.button.x = comp->state.button.x; e->s.button.y = comp->state.button.y;
        e->s.button.w = comp->state.button.w; e->s.button.h = comp->state.button.h;
        break;
      case NV_COMP_INPUT:
        append_patch_entry(s, first, id, "text", comp->state.input.text);
        append_patch_entry(s, first, id, "placeholder", comp->state.input.placeholder);
        e->s.input.text        = comp->state.input.text;
        e->s.input.placeholder = comp->state.input.placeholder;
        e->s.input.x = comp->state.input.x; e->s.input.y = comp->state.input.y;
        e->s.input.w = comp->state.input.w; e->s.input.h = comp->state.input.h;
        break;
      case NV_COMP_CHECKBOX:
        append_patch_entry(s, first, id, "label", comp->state.checkbox.label);
        append_patch_bool(s, first, id, "checked", comp->state.checkbox.checked);
        e->s.checkbox.label   = comp->state.checkbox.label;
        e->s.checkbox.checked = comp->state.checkbox.checked;
        e->s.checkbox.x = comp->state.checkbox.x; e->s.checkbox.y = comp->state.checkbox.y;
        e->s.checkbox.w = comp->state.checkbox.w; e->s.checkbox.h = comp->state.checkbox.h;
        break;
      case NV_COMP_SELECT:
        append_patch_int(s, first, id, "selectedIndex", comp->state.select_.selected_index);
        e->s.select_.selected_index = comp->state.select_.selected_index;
        e->s.select_.x = comp->state.select_.x; e->s.select_.y = comp->state.select_.y;
        e->s.select_.w = comp->state.select_.w; e->s.select_.h = comp->state.select_.h;
        break;
      case NV_COMP_SLIDER:
        append_patch_float(s, first, id, "value", comp->state.slider.value);
        e->s.slider.value = comp->state.slider.value;
        e->s.slider.min   = comp->state.slider.min;
        e->s.slider.max   = comp->state.slider.max;
        e->s.slider.step  = comp->state.slider.step;
        e->s.slider.x = comp->state.slider.x; e->s.slider.y = comp->state.slider.y;
        e->s.slider.w = comp->state.slider.w; e->s.slider.h = comp->state.slider.h;
        break;
      case NV_COMP_TEXTAREA:
        append_patch_entry(s, first, id, "text", comp->state.textarea.text);
        e->s.textarea.text = comp->state.textarea.text;
        e->s.textarea.rows = comp->state.textarea.rows;
        e->s.textarea.x = comp->state.textarea.x; e->s.textarea.y = comp->state.textarea.y;
        e->s.textarea.w = comp->state.textarea.w; e->s.textarea.h = comp->state.textarea.h;
        break;
      default:
        break;
    }
    return;
  }

  /* Existing component — diff each field */
  switch (comp->type) {
    case NV_COMP_LABEL: {
      const char* cur = comp->state.label.text ? comp->state.label.text : "";
      const char* old = e->s.label.text        ? e->s.label.text        : "";
      if (strcmp(cur, old) != 0) {
        append_patch_entry(s, first, id, "text", cur);
        e->s.label.text = comp->state.label.text;
      }
      DIFF_GEOM(label, label);
      break;
    }
    case NV_COMP_BUTTON: {
      const char* cur_cap = comp->state.button.caption ? comp->state.button.caption : "";
      const char* old_cap = e->s.button.caption        ? e->s.button.caption        : "";
      if (strcmp(cur_cap, old_cap) != 0) {
        append_patch_entry(s, first, id, "caption", cur_cap);
        e->s.button.caption = comp->state.button.caption;
      }
      if (comp->state.button.enabled != e->s.button.enabled) {
        append_patch_bool(s, first, id, "enabled", comp->state.button.enabled);
        e->s.button.enabled = comp->state.button.enabled;
      }
      DIFF_GEOM(button, button);
      break;
    }
    case NV_COMP_INPUT: {
      const char* cur_t = comp->state.input.text        ? comp->state.input.text        : "";
      const char* old_t = e->s.input.text               ? e->s.input.text               : "";
      if (strcmp(cur_t, old_t) != 0) {
        append_patch_entry(s, first, id, "text", cur_t);
        e->s.input.text = comp->state.input.text;
      }
      const char* cur_p = comp->state.input.placeholder ? comp->state.input.placeholder : "";
      const char* old_p = e->s.input.placeholder        ? e->s.input.placeholder        : "";
      if (strcmp(cur_p, old_p) != 0) {
        append_patch_entry(s, first, id, "placeholder", cur_p);
        e->s.input.placeholder = comp->state.input.placeholder;
      }
      DIFF_GEOM(input, input);
      break;
    }
    case NV_COMP_CHECKBOX: {
      const char* cur_l = comp->state.checkbox.label ? comp->state.checkbox.label : "";
      const char* old_l = e->s.checkbox.label        ? e->s.checkbox.label        : "";
      if (strcmp(cur_l, old_l) != 0) {
        append_patch_entry(s, first, id, "label", cur_l);
        e->s.checkbox.label = comp->state.checkbox.label;
      }
      if (comp->state.checkbox.checked != e->s.checkbox.checked) {
        append_patch_bool(s, first, id, "checked", comp->state.checkbox.checked);
        e->s.checkbox.checked = comp->state.checkbox.checked;
      }
      DIFF_GEOM(checkbox, checkbox);
      break;
    }
    case NV_COMP_SELECT: {
      if (comp->state.select_.selected_index != e->s.select_.selected_index) {
        append_patch_int(s, first, id, "selectedIndex", comp->state.select_.selected_index);
        e->s.select_.selected_index = comp->state.select_.selected_index;
      }
      DIFF_GEOM(select_, select_);
      break;
    }
    case NV_COMP_SLIDER: {
      if (comp->state.slider.value != e->s.slider.value) {
        append_patch_float(s, first, id, "value", comp->state.slider.value);
        e->s.slider.value = comp->state.slider.value;
      }
      DIFF_GEOM(slider, slider);
      break;
    }
    case NV_COMP_TEXTAREA: {
      const char* cur = comp->state.textarea.text ? comp->state.textarea.text : "";
      const char* old = e->s.textarea.text        ? e->s.textarea.text        : "";
      if (strcmp(cur, old) != 0) {
        append_patch_entry(s, first, id, "text", cur);
        e->s.textarea.text = comp->state.textarea.text;
      }
      if (comp->state.textarea.rows != e->s.textarea.rows) {
        append_patch_int(s, first, id, "rows", comp->state.textarea.rows);
        e->s.textarea.rows = comp->state.textarea.rows;
      }
      DIFF_GEOM(textarea, textarea);
      break;
    }
    default:
      break;
  }
}

/*
 * Build `window.__nv_patch([...])` JS call.
 * Returns heap-allocated string; caller must free().
 * Returns NULL if there are no changes.
 * Exposed as nv_widget_build_patch_js for nv_widget_flush.c.
 */
char* nv_widget_build_patch_js(nv_form_t* form) {
  nv_arena_t* arena = nv_arena_create_pool_growable(4096, 8);
  if (!arena) return NULL;

  nv_str_t* s = nv_str_create(arena);
  if (!s) { nv_arena_destroy(arena); return NULL; }

  nv_str_append(s, "window.__nv_patch([");
  int first = 1;

  nv_form_snap_t* snap = form->state.form.snap;
  diff_comp(s, (nv_component_t*)form, snap, &first);

  nv_str_append(s, "])");

  char* result = NULL;
  if (!first) { /* only if there were actual patches */
    const char* raw = nv_str_cstr(s);
    if (raw) {
      size_t len = strlen(raw);
      result = (char*)malloc(len + 1);
      if (result) memcpy(result, raw, len + 1);
    }
  }
  nv_arena_destroy(arena);
  return result;
}

/* ============================================================================
 * Exposed entry points for nv_widget_flush.c
 * ========================================================================== */

/*
 * Expose build_vue_scaffold under a non-static name so nv_widget_flush.c
 * can call it without including this file.
 */
char* nv_widget_build_vue_scaffold(nv_form_t* form) {
  return build_vue_scaffold(form);
}

/*
 * Expose diff_comp under a non-static name for the initial snapshot seed
 * in nv_widget_flush.c.
 */
void nv_widget_diff_comp_init(nv_str_t* s, nv_component_t* comp,
                               nv_form_snap_t* snap, int* first) {
  diff_comp(s, comp, snap, first);
}

/* nv_component_paint lives in nv_widget_paint.c (kept for unit tests) */
