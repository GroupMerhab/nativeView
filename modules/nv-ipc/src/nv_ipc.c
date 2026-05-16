/* Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0 */

/* =============================================================================
 * nv_ipc.c - Inter-Process Communication Implementation
 *
 * JSON-RPC 2.0 messaging with support for compact wire format.
 * Apache 2.0 - See LICENSE for details
 * =============================================================================
 */

#include "nv_ipc.h"
#include "nv_ipc_internal.h"
#include "nv_json.h"
#include "nv_arena.h"
#include "nv_util.h"
#include "nv_window_internal.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* Operation registry (populated by nv_op_registry_init via nv_ipc_register_ops) */
static const nv_ipc_op_entry_t* g_ipc_ops = NULL;
static size_t g_ipc_ops_count = 0;

/* Register operation table from ops registry */
/* Allow tests to override the registration interceptor by providing their own
 * strong symbol. On platforms supporting weak symbols, expose this as weak.
 * MinGW + static libs: weak defs are often not extracted from .a (same as
 * nv_ipc_reply_* below), so use strong defaults. */
#if !defined(_MSC_VER) && !defined(__MINGW32__)
NV_INTERNAL __attribute__((weak)) void nv_ipc_register_ops(const nv_ipc_op_entry_t* entries, size_t count) {
  g_ipc_ops = entries;
  g_ipc_ops_count = count;
}
#else
NV_INTERNAL void nv_ipc_register_ops(const nv_ipc_op_entry_t* entries, size_t count) {
  g_ipc_ops = entries;
  g_ipc_ops_count = count;
}
#endif

/* Reply helpers used by ops to send responses back to JS */
/* Real implementations used by production code. Named separately so test
 * suites can provide their own `nv_ipc_reply_*` overrides when needed. */
NV_INTERNAL void nv_ipc_send_ok(nv_window_t* w, int seq, nv_json_t* result, nv_arena_t* arena) {
  if (!w) return;
  const char* event = "";
  const char* payload = result ? nv_json_serialize(result) : "null";
  if (!payload) payload = "null";
  nv_arena_t* a = arena ? arena : nv_window_get_arena(w);
  size_t needed = strlen(event) + strlen(payload) + 64;
  char* buf = nv_arena_alloc(a, needed);
  if (!buf) return;
  snprintf(buf, needed, "{\"e\":\"%s\",\"s\":%d,\"ok\":1,\"d\":%s}", event, seq, payload);
  nv_send(w, event, buf);
}

NV_INTERNAL void nv_ipc_send_err(nv_window_t* w, int seq, const char* code, const char* message, nv_arena_t* arena) {
  if (!w) return;
  const char* event = "";
  nv_arena_t* a = arena ? arena : nv_window_get_arena(w);
  const char* c = code ? code : "ERR_UNKNOWN";
  const char* m = message ? message : "error";
  size_t needed = strlen(event) + strlen(c) + strlen(m) + 128;
  char* buf = nv_arena_alloc(a, needed);
  if (!buf) return;
  snprintf(buf, needed, "{\"e\":\"%s\",\"s\":%d,\"ok\":0,\"d\":{\"code\":\"%s\",\"msg\":\"%s\"}}", event, seq, c, m);
  fprintf(stderr, "[nv][error][ipc] send_err seq=%d code=%s msg=%s\n", seq, c, m);
  nv_send(w, event, buf);
}

/* Weak wrapper names that other modules call. Tests may define strong
 * implementations of `nv_ipc_reply_ok/err` to intercept replies — by
 * marking these wrappers weak the linker allows tests to override them.
 *
 * MinGW + static libs: weak symbols in libnv-ipc.a are often not extracted
 * when the archive is visited before dependents' undefineds are known, so
 * use strong defaults (same as MSVC). */
#if !defined(_MSC_VER) && !defined(__MINGW32__)
NV_INTERNAL __attribute__((weak)) void nv_ipc_reply_ok(nv_window_t* w, int seq, nv_json_t* result, nv_arena_t* arena) {
  nv_ipc_send_ok(w, seq, result, arena);
}

NV_INTERNAL __attribute__((weak)) void nv_ipc_reply_err(nv_window_t* w, int seq, const char* code, const char* message, nv_arena_t* arena) {
  nv_ipc_send_err(w, seq, code, message, arena);
}
#else
NV_INTERNAL void nv_ipc_reply_ok(nv_window_t* w, int seq, nv_json_t* result, nv_arena_t* arena) {
  nv_ipc_send_ok(w, seq, result, arena);
}

NV_INTERNAL void nv_ipc_reply_err(nv_window_t* w, int seq, const char* code, const char* message, nv_arena_t* arena) {
  nv_ipc_send_err(w, seq, code, message, arena);
}
#endif

/* =============================================================================
 * Internal Types
 * =============================================================================
 */

struct nv_ipc_message {
  nv_arena_t* arena;
  nv_ipc_message_type_t type;
  int id;
  char* method;
  nv_json_t* params;
  nv_json_t* result;
  int error_code;
  char* error_message;
  nv_json_t* json;        /* Root JSON object */
  char* serialized;       /* Cached serialization */
  size_t serialize_size;
};

/* =============================================================================
 * Helper: Duplicate string in arena
 * =============================================================================
 */

NV_INTERNAL char* ipc_strdup(nv_arena_t* arena, const char* str) {
  if (!str) return NULL;
  size_t len = strlen(str) + 1;
  char* dst = nv_arena_alloc(arena, len);
  if (dst) memcpy(dst, str, len);
  return dst;
}

/* =============================================================================
 * Message Construction
 * =============================================================================
 */

NV_API nv_ipc_message_t* nv_ipc_request(nv_arena_t* arena, const char* method, int id) {
  if (!arena || !method || id <= 0) return NULL;
  
  nv_ipc_message_t* msg = nv_arena_alloc(arena, sizeof(nv_ipc_message_t));
  if (!msg) return NULL;
  
  msg->arena = arena;
  msg->type = NV_IPC_REQUEST;
  msg->id = id;
  msg->method = ipc_strdup(arena, method);
  msg->params = NULL;
  msg->result = NULL;
  msg->error_code = 0;
  msg->error_message = NULL;
  msg->json = nv_json_object(arena);
  msg->serialized = NULL;
  msg->serialize_size = 0;
  
  if (!msg->method || !msg->json) return NULL;
  
  return msg;
}

NV_API nv_ipc_message_t* nv_ipc_response(nv_arena_t* arena, int request_id) {
  if (!arena || request_id <= 0) return NULL;
  
  nv_ipc_message_t* msg = nv_arena_alloc(arena, sizeof(nv_ipc_message_t));
  if (!msg) return NULL;
  
  msg->arena = arena;
  msg->type = NV_IPC_RESPONSE;
  msg->id = request_id;
  msg->method = NULL;
  msg->params = NULL;
  msg->result = NULL;
  msg->error_code = 0;
  msg->error_message = NULL;
  msg->json = nv_json_object(arena);
  msg->serialized = NULL;
  msg->serialize_size = 0;
  
  if (!msg->json) return NULL;
  
  return msg;
}

NV_API nv_ipc_message_t* nv_ipc_error_response(nv_arena_t* arena, int request_id,
                                               int error_code, const char* error_message) {
  if (!arena || request_id <= 0 || error_code >= 0) return NULL;
  
  nv_ipc_message_t* msg = nv_arena_alloc(arena, sizeof(nv_ipc_message_t));
  if (!msg) return NULL;
  
  msg->arena = arena;
  msg->type = NV_IPC_RESPONSE;
  msg->id = request_id;
  msg->method = NULL;
  msg->params = NULL;
  msg->result = NULL;
  msg->error_code = error_code;
  msg->error_message = ipc_strdup(arena, error_message ? error_message : "");
  msg->json = nv_json_object(arena);
  msg->serialized = NULL;
  msg->serialize_size = 0;
  
  if (!msg->json) return NULL;
  
  return msg;
}

NV_API nv_ipc_message_t* nv_ipc_notify(nv_arena_t* arena, const char* method) {
  if (!arena || !method) return NULL;
  
  nv_ipc_message_t* msg = nv_arena_alloc(arena, sizeof(nv_ipc_message_t));
  if (!msg) return NULL;
  
  msg->arena = arena;
  msg->type = NV_IPC_NOTIFICATION;
  msg->id = -1;
  msg->method = ipc_strdup(arena, method);
  msg->params = NULL;
  msg->result = NULL;
  msg->error_code = 0;
  msg->error_message = NULL;
  msg->json = nv_json_object(arena);
  msg->serialized = NULL;
  msg->serialize_size = 0;
  
  if (!msg->method || !msg->json) return NULL;
  
  return msg;
}

/* =============================================================================
 * Message Configuration
 * =============================================================================
 */

NV_API int nv_ipc_set_params(nv_ipc_message_t* msg, nv_json_t* content) {
  if (!msg || !content) return -1;
  
  msg->params = content;
  return 0;
}

NV_API int nv_ipc_set_result(nv_ipc_message_t* msg, nv_json_t* content) {
  if (!msg || !content || msg->type != NV_IPC_RESPONSE) return -1;
  
  msg->result = content;
  return 0;
}

NV_API int nv_ipc_set_event_data(nv_ipc_message_t* msg, nv_json_t* data) {
  if (!msg || !data || msg->type != NV_IPC_NOTIFICATION) return -1;
  
  msg->params = data;
  return 0;
}

/* =============================================================================
 * Serialization
 * =============================================================================
 */

NV_API const char* nv_ipc_serialize(nv_ipc_message_t* msg) {
  if (!msg || !msg->json) return NULL;
  
  /* Clear and rebuild JSON structure */
  nv_json_t* root = msg->json;
  
  /* Standard JSON-RPC version field */
  nv_json_str(root, "jsonrpc", "2.0");
  
  /* Add method for requests/notifications */
  if (msg->type == NV_IPC_REQUEST || msg->type == NV_IPC_NOTIFICATION) {
    if (msg->method) {
      nv_json_str(root, "method", msg->method);
    }
  }
  
  /* Add params if present */
  if (msg->params) {
    nv_json_nest(root, "params", msg->params);
  }
  
  /* Add result for successful responses */
  if (msg->type == NV_IPC_RESPONSE && msg->error_code == 0 && msg->result) {
    nv_json_nest(root, "result", msg->result);
  }
  
  /* Add error for error responses */
  if (msg->type == NV_IPC_RESPONSE && msg->error_code != 0) {
    nv_json_t* error_obj = nv_json_object(msg->arena);
    nv_json_int(error_obj, "code", msg->error_code);
    if (msg->error_message) {
      nv_json_str(error_obj, "message", msg->error_message);
    }
    nv_json_nest(root, "error", error_obj);
  }
  
  /* Add ID for requests and responses */
  if (msg->type == NV_IPC_REQUEST || msg->type == NV_IPC_RESPONSE) {
    nv_json_int(root, "id", msg->id);
  }
  
  /* Serialize to string */
  const char* json_str = nv_json_serialize(root);
  if (!json_str) {
    NV_ERR("IPC: JSON serialization failed");
    return NULL;
  }
  
  msg->serialized = (char*)json_str;
  msg->serialize_size = strlen(json_str);
  
  return json_str;
}

NV_API size_t nv_ipc_get_size(nv_ipc_message_t* msg) {
  if (!msg) return 0;
  return msg->serialize_size;
}

/* =============================================================================
 * Parsing (Stub for now - full parsing would require JSON parser)
 * =============================================================================
 */

NV_API nv_ipc_message_t* nv_ipc_parse(nv_arena_t* arena, const char* json, size_t json_len) {
  if (!arena || !json || json_len == 0) return NULL;
  
  /* For now, return NULL since we don't have a parser.
   * In production, would parse JSON-RPC messages from JS */
  NV_ERR("IPC: parsing not yet implemented");
  return NULL;
}

NV_API nv_ipc_message_type_t nv_ipc_get_type(nv_ipc_message_t* msg) {
  if (!msg) return NV_IPC_NOTIFICATION;
  return msg->type;
}

NV_API const char* nv_ipc_get_method(nv_ipc_message_t* msg) {
  if (!msg) return NULL;
  return msg->method;
}

NV_API int nv_ipc_get_id(nv_ipc_message_t* msg) {
  if (!msg || msg->type == NV_IPC_NOTIFICATION) return -1;
  return msg->id;
}

NV_API nv_json_t* nv_ipc_get_params(nv_ipc_message_t* msg) {
  if (!msg) return NULL;
  return msg->params;
}

NV_API int nv_ipc_get_error(nv_ipc_message_t* msg, int* out_code, const char** out_message) {
  if (!msg || msg->type != NV_IPC_RESPONSE || msg->error_code == 0) {
    return -1;
  }
  
  if (out_code) *out_code = msg->error_code;
  if (out_message) *out_message = msg->error_message;
  
  return 0;
}

/* =============================================================================
 * Cleanup
 * =============================================================================
 */

NV_API void nv_ipc_destroy(nv_ipc_message_t* msg) {
  /* All memory is owned by arena, so nothing to do here */
  (void)msg;
}

/* =============================================================================
 * INTERNAL IPC STATE & CALLBACKS 
 * =============================================================================
 */

/* Per-window IPC state for storing callbacks */
struct nv_ipc_state {
  nv_arena_t* arena;
  
  /* Message callback */
  nv_msg_cb_t msg_cb;
  void* msg_userdata;
  
  /* Ready callback */
  nv_ready_cb_t ready_cb;
  void* ready_userdata;
  
  /* Close callback */
  nv_close_cb_t close_cb;
  void* close_userdata;
  
  /* Pool arena for dispatch messages */
  nv_arena_t* pool_arena;
};

/* Global statistics (static, shared across all instances) */
static nv_ipc_stats_t g_ipc_stats = {0, 0};

/* =============================================================================
 * IPC State Lifecycle
 * =============================================================================
 */

NV_INTERNAL nv_ipc_state_t* nv_ipc_state_create(nv_arena_t* arena) {
  if (!arena) return NULL;
  
  nv_ipc_state_t* state = nv_arena_alloc(arena, sizeof(nv_ipc_state_t));
  if (!state) {
    NV_ERR("IPC: failed to allocate state");
    return NULL;
  }
  
  state->arena = arena;
  state->msg_cb = NULL;
  state->msg_userdata = NULL;
  state->ready_cb = NULL;
  state->ready_userdata = NULL;
  state->close_cb = NULL;
  state->close_userdata = NULL;
  
  /* Chunk size for small allocations; large sends use arena overflow path. */
  state->pool_arena = nv_arena_create_pool_growable(4096, 4);
  if (!state->pool_arena) {
    NV_ERR("IPC: failed to create pool arena");
    return NULL;
  }
  
  return state;
}

NV_INTERNAL nv_arena_t* nv_ipc_get_pool_arena(nv_ipc_state_t* state) {
  if (!state) return NULL;
  return state->pool_arena;
}

/* =============================================================================
 * Callback Registration
 * =============================================================================
 */

NV_INTERNAL void nv_ipc_set_msg_cb(nv_ipc_state_t* state, nv_msg_cb_t callback,
                                    void* userdata) {
  if (!state) return;
  state->msg_cb = callback;
  state->msg_userdata = userdata;
}

NV_INTERNAL void nv_ipc_set_ready_cb(nv_ipc_state_t* state, nv_ready_cb_t callback,
                                      void* userdata) {
  if (!state) return;
  state->ready_cb = callback;
  state->ready_userdata = userdata;
}

NV_INTERNAL void nv_ipc_set_close_cb(nv_ipc_state_t* state, nv_close_cb_t callback,
                                      void* userdata) {
  if (!state) return;
  state->close_cb = callback;
  state->close_userdata = userdata;
}

NV_INTERNAL int nv_ipc_has_close_cb(nv_ipc_state_t* state) {
  return (state && state->close_cb) ? 1 : 0;
}

NV_INTERNAL void nv_ipc_invoke_close_cb(nv_window_t* window, nv_ipc_state_t* state) {
  if (!state || !state->close_cb) return;
  state->close_cb(window, state->close_userdata);
}

/* =============================================================================
 * Message Dispatch
 * =============================================================================
 */

NV_INTERNAL void nv_ipc_dispatch(nv_window_t* window, nv_ipc_state_t* state,
                                  const char* raw_json) {
  if (!state || !raw_json) return;

  /* Reset pool arena for this dispatch cycle */
  nv_arena_reset(state->pool_arena);
  
  /* Parse incoming JSON: {"e":"event_name","d":{...data...}} */
  nv_json_val_t* parsed = nv_json_parse(state->pool_arena, raw_json);
  if (!parsed) {
    NV_ERR("IPC: failed to parse incoming JSON: %s", raw_json);
    return;
  }
  
  if (!nv_json_is_object(parsed)) {
    NV_ERR("IPC: incoming message must be a JSON object");
    return;
  }
  
  /* Extract event name from "e" field */
  const char* event = nv_json_get_str(parsed, "e");
  if (!event) {
    NV_ERR("IPC: incoming message missing 'e' (event) field");
    return;
  }
  
  /* Sequence id: s > 0 => request expecting reply; s == 0 => push */
  long long seq = nv_json_get_int(parsed, "s");
  if (seq > 0) {
    /* Request path: dispatch to registered op handler if available */
    const nv_json_val_t* args = nv_json_get_obj(parsed, "d");
    if (g_ipc_ops && g_ipc_ops_count) {
      for (size_t i = 0; i < g_ipc_ops_count; i++) {
        const nv_ipc_op_entry_t* e = &g_ipc_ops[i];
        if (e->name && strcmp(e->name, event) == 0) {
          if (e->handler) {
            e->handler(window, (int)seq, args, state->pool_arena);
            return;
          }
        }
      }
    }
    /* Unknown operation: reply with not found */
    nv_ipc_reply_err(window, (int)seq, "ERR_NOT_FOUND", "unknown operation", state->pool_arena);
    return;
  }

  /* For callback consumers that want full context, pass the original raw JSON. */
  const char* json_data = raw_json;

  /* Dispatch to message callback if registered */
  if (state->msg_cb) {
    state->msg_cb(window, event, json_data, state->msg_userdata);
  }
}

NV_INTERNAL void nv_ipc_invoke_ready(nv_window_t* window, nv_ipc_state_t* state) {
  if (!state) return;
  if (state->ready_cb) {
    state->ready_cb(window, state->ready_userdata);
  }
}

/* =============================================================================
 * JavaScript Bootstrap Injection
 * =============================================================================
 */

static int nv_ipc_env_truthy(const char* value) {
  if (!value || !*value) return 0;
  if (value[0] == '1' && value[1] == '\0') return 1;
  if (value[0] == 't' || value[0] == 'T') return 1;
  if (value[0] == 'y' || value[0] == 'Y') return 1;
  if ((value[0] == 'o' || value[0] == 'O') && (value[1] == 'n' || value[1] == 'N') && value[2] == '\0') return 1;
  return 0;
}

NV_INTERNAL const char* nv_ipc_inject_script(void) {
  /* JavaScript IIFE that defines window.__nv object for bidirectional messaging */
  static const char* script_default_ctxmenu_off =
    "(function() {"
    "  if (window.__nv) return;"  /* Prevent double-injection */
    "  window.__nv = {};"
    "  try { if (console && console.log) console.log('[NV] injected bridge'); } catch(e) {}"
    "  try { document.addEventListener('contextmenu', function(e) { e.preventDefault(); }, true); } catch(e) {}"
    "  "
    "  /* Internal state: no prototype chain overhead */"
    "  var handlers = Object.create(null);"
    "  "
    "  /* Register event listener */"
    "  window.__nv.on = function(event, fn) {"
    "    if (typeof event !== 'string' || typeof fn !== 'function') return;"
    "    handlers[event] = fn;"
    "  };"
    "  "
    "  /* Unregister event listener */"
    "  window.__nv.off = function(event) {"
    "    if (typeof event !== 'string') return;"
    "    delete handlers[event];"
    "  };"
    "  "
    "  /* Send pre-serialized compact-wire JSON to native */"
    "  window.__nv.send = function(event, json) {"
    "    if (typeof event !== 'string' || typeof json !== 'string') return;"
    "    try { if (console && console.log) console.log('[NV] send', event, json.slice(0,256)); } catch(e) {}"
    "    var wire;"
    "    try {"
    "      wire = JSON.stringify({e:event, s:0, d:JSON.parse(json)});"
    "    } catch(e) {"
    "      wire = JSON.stringify({e:event, s:0, d:json});"
    "    }"
    "    /*{NV_POST}*/(wire);"  /* Placeholder for platform */
    "  };"
    "  "
    "  /* Called by native to emit event to JS */"
    "  window.__nv._emit = function(event, json) {"
    "    if (typeof event !== 'string') return;"
    "    try { if (console && console.log) console.log('[NV] emit', event, (typeof json==='string'? json.slice(0,200)+ (json.length>200 ? '... ('+json.length+' chars)' : '') : json)); } catch(e) {}"
    "    if (!handlers[event]) return;"  /* No listener registered */
    "    try {"
    "      var data = JSON.parse(json);"
    "      handlers[event](data);"
    "    } catch(e) {"
    "      console.error('nativeview: failed to parse data:', e);"
    "    }"
    "  };"
    "  "
    "  /* Shared memory fast path: register SharedArrayBuffer by name, read float via Atomics */"
    "  var _shmViews = Object.create(null);"
    "  window.__nv.shm_register = function(name, buffer) {"
    "    if (typeof name === 'string' && buffer && buffer.byteLength >= 4)"
    "      _shmViews[name] = new Float32Array(buffer);"
    "  };"
    "  window.__nv.shm_read_f32 = function(name, offset) {"
    "    var v = _shmViews[name];"
    "    if (!v || typeof offset !== 'number' || (offset|0) !== offset || offset < 0) return NaN;"
    "    var idx = offset >>> 2;"
    "    if (idx >= v.length) return NaN;"
    "    try { return v[idx]; } catch(e) { return NaN; }"
    "  };"
    "})();";

  static const char* script_default_ctxmenu_on =
    "(function() {"
    "  if (window.__nv) return;"
    "  window.__nv = {};"
    "  try { if (console && console.log) console.log('[NV] injected bridge'); } catch(e) {}"
    "  "
    "  var handlers = Object.create(null);"
    "  "
    "  window.__nv.on = function(event, fn) {"
    "    if (typeof event !== 'string' || typeof fn !== 'function') return;"
    "    handlers[event] = fn;"
    "  };"
    "  "
    "  window.__nv.off = function(event) {"
    "    if (typeof event !== 'string') return;"
    "    delete handlers[event];"
    "  };"
    "  "
    "  window.__nv.send = function(event, json) {"
    "    if (typeof event !== 'string' || typeof json !== 'string') return;"
    "    try { if (console && console.log) console.log('[NV] send', event, json.slice(0,256)); } catch(e) {}"
    "    var wire;"
    "    try {"
    "      wire = JSON.stringify({e:event, s:0, d:JSON.parse(json)});"
    "    } catch(e) {"
    "      wire = JSON.stringify({e:event, s:0, d:json});"
    "    }"
    "    /*{NV_POST}*/(wire);"
    "  };"
    "  "
    "  window.__nv._emit = function(event, json) {"
    "    if (typeof event !== 'string') return;"
    "    try { if (console && console.log) console.log('[NV] emit', event, (typeof json==='string'? json.slice(0,200)+ (json.length>200 ? '... ('+json.length+' chars)' : '') : json)); } catch(e) {}"
    "    if (!handlers[event]) return;"
    "    try {"
    "      var data = JSON.parse(json);"
    "      handlers[event](data);"
    "    } catch(e) {"
    "      console.error('nativeview: failed to parse data:', e);"
    "    }"
    "  };"
    "  "
    "  var _shmViews = Object.create(null);"
    "  window.__nv.shm_register = function(name, buffer) {"
    "    if (typeof name === 'string' && buffer && buffer.byteLength >= 4)"
    "      _shmViews[name] = new Float32Array(buffer);"
    "  };"
    "  window.__nv.shm_read_f32 = function(name, offset) {"
    "    var v = _shmViews[name];"
    "    if (!v || typeof offset !== 'number' || (offset|0) !== offset || offset < 0) return NaN;"
    "    var idx = offset >>> 2;"
    "    if (idx >= v.length) return NaN;"
    "    try { return v[idx]; } catch(e) { return NaN; }"
    "  };"
    "})();";

  const char* env = getenv("NV_WEBVIEW_CONTEXT_MENU");
  if (nv_ipc_env_truthy(env)) return script_default_ctxmenu_on;
  return script_default_ctxmenu_off;
}

/* =============================================================================
 * Build Outgoing Messages for C→JS Delivery
 * =============================================================================
 */

#define NV_IPC_STACK_BUF 2048

NV_INTERNAL const char* nv_ipc_build_send(nv_arena_t* arena, const char* event,
                                           const char* json) {
  if (!event || !json) return NULL;
  
  /* Build result string: window.__nv._emit("event", "...json...") */
  
  const char* prefix = "window.__nv._emit(\"";
  const char* middle = "\",\"";
  const char* suffix = "\")";
  
  size_t prefix_len = strlen(prefix);
  size_t event_len = strlen(event);
  size_t middle_len = strlen(middle);
  size_t json_len = strlen(json);
  size_t suffix_len = strlen(suffix);
  
  /* Estimate size (accounting for potential escape sequences) */
  size_t estimated_len = prefix_len + (event_len * 2) + middle_len + (json_len * 2) + suffix_len + 1;
  
  /* Try fast path: use stack if fits in 2048 bytes, then copy to arena */
  if (estimated_len <= NV_IPC_STACK_BUF && arena) {
    char buf[NV_IPC_STACK_BUF];
    char* p = buf;
    
    /* Copy prefix */
    memcpy(p, prefix, prefix_len);
    p += prefix_len;
    
    /* Copy event with quote escaping */
    for (size_t i = 0; i < event_len; i++) {
      char c = event[i];
      if (c == '"' || c == '\\') {
        *p++ = '\\';
      }
      *p++ = c;
    }
    
    /* Copy middle */
    memcpy(p, middle, middle_len);
    p += middle_len;
    
    /* Copy json (already JSON-encoded, but may contain escaped quotes) */
    for (size_t i = 0; i < json_len; i++) {
      char c = json[i];
      if (c == '"' || c == '\\') {
        *p++ = '\\';
      }
      *p++ = c;
    }
    
    /* Copy suffix */
    memcpy(p, suffix, suffix_len);
    p += suffix_len;
    *p = '\0';
    
    /* Copy from stack to arena */
    size_t result_len = p - buf;
    char* result = nv_arena_alloc(arena, result_len + 1);
    if (result) {
      memcpy(result, buf, result_len + 1);
      g_ipc_stats.stack_allocs++;
      return result;
    }
    /* Fall through to heap path if arena alloc failed */
  }
  
  /* Slow path: build directly in arena */
  if (!arena) {
    NV_ERR("IPC: message too large (%zu bytes) and no arena provided", estimated_len);
    return NULL;
  }
  
  char* result = nv_arena_alloc(arena, estimated_len);
  if (!result) {
    NV_ERR("IPC: arena allocation failed for message (%zu bytes)", estimated_len);
    return NULL;
  }
  
  char* p = result;
  
  /* Copy prefix */
  memcpy(p, prefix, prefix_len);
  p += prefix_len;
  
  /* Copy event with escaping */
  for (size_t i = 0; i < event_len; i++) {
    char c = event[i];
    if (c == '"' || c == '\\') {
      *p++ = '\\';
    }
    *p++ = c;
  }
  
  /* Copy middle */
  memcpy(p, middle, middle_len);
  p += middle_len;
  
  /* Copy json with escaping */
  for (size_t i = 0; i < json_len; i++) {
    char c = json[i];
    if (c == '"' || c == '\\') {
      *p++ = '\\';
    }
    *p++ = c;
  }
  
  /* Copy suffix */
  memcpy(p, suffix, suffix_len);
  p += suffix_len;
  *p = '\0';
  
  g_ipc_stats.heap_allocs++;
  return result;
}

/* =============================================================================
 * Statistics
 * =============================================================================
 */

NV_INTERNAL nv_ipc_stats_t nv_ipc_get_stats(void) {
  return g_ipc_stats;
}
