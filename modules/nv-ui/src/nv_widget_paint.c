/* Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0 */

/* =============================================================================
 * nv_widget_paint.c - Legacy nv_component_paint (unit tests only)
 *
 * Generates raw HTML from the component tree without Vue reactivity.
 * Used exclusively by unit tests to verify component tree structure.
 * NOT used by the main nv_ui_flush path.
 *
 * Apache 2.0 - See LICENSE for details
 * ============================================================================= */

#include "nv_ui.h"
#include "nv_str.h"
#include "nv_arena.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct nv_canvas {
  nv_window_t* window;
  nv_arena_t*  arena;
  nv_str_t*    html;
};

static void canvas_append(nv_canvas_t* c, const char* s) {
  if (c && c->html && s) nv_str_append(c->html, s);
}

static void append_escaped(nv_canvas_t* c, const char* s) {
  if (!c || !c->html || !s) return;
  for (; *s; s++) {
    switch (*s) {
      case '&':  nv_str_append(c->html, "&amp;");  break;
      case '<':  nv_str_append(c->html, "&lt;");   break;
      case '>':  nv_str_append(c->html, "&gt;");   break;
      case '"':  nv_str_append(c->html, "&quot;"); break;
      case '\'': nv_str_append(c->html, "&#39;");  break;
      default:   nv_str_append_len(c->html, s, 1); break;
    }
  }
}

#ifdef NV_TEST
NV_INTERNAL nv_canvas_t* nv_canvas_create_for_test(nv_arena_t* arena) {
  if (!arena) return NULL;
  nv_canvas_t* c = (nv_canvas_t*)nv_arena_alloc(arena, sizeof(nv_canvas_t));
  if (!c) return NULL;
  c->window = NULL;
  c->arena  = arena;
  c->html   = nv_str_create(arena);
  if (!c->html) return NULL;
  return c;
}
NV_INTERNAL const char* nv_canvas_get_html(nv_canvas_t* canvas) {
  return canvas && canvas->html ? nv_str_cstr(canvas->html) : NULL;
}
#endif

NV_INTERNAL void nv_component_paint(nv_component_t* comp, nv_canvas_t* canvas) {
  if (!comp || !canvas) return;
  switch (comp->type) {
    case NV_COMP_FORM: {
      canvas_append(canvas, "<!DOCTYPE html><html><body><div id=\"nv-root\">");
      for (nv_component_t* c = comp->owned_head; c; c = c->owned_next)
        nv_component_paint(c, canvas);
      canvas_append(canvas, "</div></body></html>");
      break;
    }
    case NV_COMP_PANEL: {
      nv_str_appendf(canvas->html,
        "<div class=\"nv-panel\" id=\"%s\" "
        "style=\"position:absolute;left:%dpx;top:%dpx;width:%dpx;height:%dpx;\">",
        comp->name[0] ? comp->name : "panel",
        comp->state.panel.x, comp->state.panel.y,
        comp->state.panel.w, comp->state.panel.h);
      for (nv_component_t* c = comp->owned_head; c; c = c->owned_next)
        nv_component_paint(c, canvas);
      canvas_append(canvas, "</div>");
      break;
    }
    case NV_COMP_BUTTON: {
      const char* id = comp->name[0] ? comp->name : "button";
      canvas_append(canvas, "<button class=\"nv-button\" id=\"");
      append_escaped(canvas, id);
      canvas_append(canvas, "\" type=\"button\"");
      if (!comp->state.button.enabled) canvas_append(canvas, " disabled");
      canvas_append(canvas, ">");
      append_escaped(canvas, comp->state.button.caption);
      canvas_append(canvas, "</button>");
      break;
    }
    case NV_COMP_LABEL: {
      canvas_append(canvas, "<span class=\"nv-label\" id=\"");
      append_escaped(canvas, comp->name[0] ? comp->name : "label");
      canvas_append(canvas, "\">");
      append_escaped(canvas, comp->state.label.text);
      canvas_append(canvas, "</span>");
      break;
    }
    case NV_COMP_INPUT: {
      canvas_append(canvas, "<input class=\"nv-input\" id=\"");
      append_escaped(canvas, comp->name[0] ? comp->name : "input");
      canvas_append(canvas, "\" type=\"text\" value=\"");
      append_escaped(canvas, comp->state.input.text);
      canvas_append(canvas, "\" placeholder=\"");
      append_escaped(canvas, comp->state.input.placeholder);
      canvas_append(canvas, "\">");
      break;
    }
    default:
      break;
  }
}
