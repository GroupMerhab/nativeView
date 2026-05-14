#include "nv_op_dialog.h"
#include "nv.h"
#include "nv_core_internal.h"
#include "nv_window_internal.h"
#include <stdlib.h>

NV_INTERNAL void nv_ipc_reply_ok(nv_window_t* w, int seq, nv_json_t* result, nv_arena_t* arena);
NV_INTERNAL void nv_ipc_reply_err(nv_window_t* w, int seq, const char* code, const char* message, nv_arena_t* arena);

/* When args is non-NULL, require JSON string keys "title" and "body". */
static int nv_dialog_title_body_from_args(const nv_json_val_t* args, nv_window_t* w, int seq, nv_arena_t* arena,
    const char* err_detail, const char** title, const char** body, const char* def_title, const char* def_body) {
  if (!args) {
    *title = def_title;
    *body = def_body;
    return 1;
  }
  if (!nv_json_get_str(args, "title") || !nv_json_get_str(args, "body")) {
    nv_ipc_reply_err(w, seq, "ERR_INVALID_ARGS", err_detail, arena);
    return 0;
  }
  *title = nv_json_get_str(args, "title");
  *body = nv_json_get_str(args, "body");
  return 1;
}

static void nv_dialog_open_file_done(nv_dialog_ctx_t* ctx, int canceled, void* result) {
  nv_json_t* obj = nv_json_object(ctx->arena);
  nv_json_bool(obj, "canceled", canceled ? 1 : 0);
  nv_json_t* arr = nv_json_array(ctx->arena);
  if (!canceled && result) {
    char** paths = (char**)result;
    size_t count = *((size_t*)paths - 1);
    for (size_t i = 0; i < count; i++) {
      nv_json_t* item = nv_json_object(ctx->arena);
      nv_json_str(item, "path", paths[i]);
      nv_json_array_push(arr, item);
      free(paths[i]);
    }
    free(paths);
  }
  nv_json_nest(obj, "paths", arr);
  nv_ipc_reply_ok(ctx->window, ctx->seq, obj, ctx->arena);
  free(ctx);
}

static void nv_dialog_save_file_done(nv_dialog_ctx_t* ctx, int canceled, void* result) {
  nv_json_t* obj = nv_json_object(ctx->arena);
  if (!canceled && result) {
    nv_json_str(obj, "path", (const char*)result);
    free(result);
  } else {
    nv_json_str(obj, "path", "");
  }
  nv_ipc_reply_ok(ctx->window, ctx->seq, obj, ctx->arena);
  free(ctx);
}

static void nv_dialog_open_folder_done(nv_dialog_ctx_t* ctx, int canceled, void* result) {
  nv_json_t* obj = nv_json_object(ctx->arena);
  if (!canceled && result) {
    nv_json_str(obj, "path", (const char*)result);
    free(result);
  } else {
    nv_json_str(obj, "path", "");
  }
  nv_ipc_reply_ok(ctx->window, ctx->seq, obj, ctx->arena);
  free(ctx);
}

static void nv_dialog_message_done(nv_dialog_ctx_t* ctx, int canceled, void* result) {
  (void)canceled;
  nv_json_t* obj = nv_json_object(ctx->arena);
  nv_json_int(obj, "buttonIndex", result ? *((int*)result) : 0);
  if (result) free(result);
  nv_ipc_reply_ok(ctx->window, ctx->seq, obj, ctx->arena);
  free(ctx);
}

static void nv_dialog_confirm_done(nv_dialog_ctx_t* ctx, int canceled, void* result) {
  (void)canceled;
  nv_json_t* obj = nv_json_object(ctx->arena);
  nv_json_bool(obj, "confirmed", result ? *((int*)result) : 0);
  if (result) free(result);
  nv_ipc_reply_ok(ctx->window, ctx->seq, obj, ctx->arena);
  free(ctx);
}

NV_INTERNAL void nv_op_dialog_open_file(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
  int multiple = args ? nv_json_get_bool(args, "multiple") : 0;
  nv_dialog_ctx_t* ctx = (nv_dialog_ctx_t*)malloc(sizeof(nv_dialog_ctx_t));
  if (!ctx) {
    nv_ipc_reply_err(w, seq, "ERR_IO", "dialog alloc failed", arena);
    return;
  }
  ctx->window = w;
  ctx->seq = seq;
  ctx->arena = arena;
  if (!w || !w->app) {
    nv_json_t* obj = nv_json_object(arena);
    nv_json_bool(obj, "canceled", 1);
    nv_json_t* arr = nv_json_array(arena);
    nv_json_nest(obj, "paths", arr);
    nv_ipc_reply_ok(w, seq, obj, arena);
    free(ctx);
    return;
  }
  const nv_platform_api_t* api = &w->app->platform_api;
  if (!api->dialog_open_file_async) {
    nv_ipc_reply_err(w, seq, "ERR_NOT_SUPPORTED", "dialog not supported", arena);
    free(ctx);
    return;
  }
  api->dialog_open_file_async(multiple ? 1 : 0, ctx, nv_dialog_open_file_done);
}

NV_INTERNAL void nv_op_dialog_save_file(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
  (void)args;
  nv_dialog_ctx_t* ctx = (nv_dialog_ctx_t*)malloc(sizeof(nv_dialog_ctx_t));
  if (!ctx) {
    nv_ipc_reply_err(w, seq, "ERR_IO", "dialog alloc failed", arena);
    return;
  }
  ctx->window = w;
  ctx->seq = seq;
  ctx->arena = arena;
  if (!w || !w->app) {
    nv_json_t* obj = nv_json_object(arena);
    nv_json_str(obj, "path", "");
    nv_ipc_reply_ok(w, seq, obj, arena);
    free(ctx);
    return;
  }
  const nv_platform_api_t* api = &w->app->platform_api;
  if (!api->dialog_save_file_async) {
    nv_ipc_reply_err(w, seq, "ERR_NOT_SUPPORTED", "dialog not supported", arena);
    free(ctx);
    return;
  }
  api->dialog_save_file_async(ctx, nv_dialog_save_file_done);
}

NV_INTERNAL void nv_op_dialog_open_folder(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
  (void)args;
  nv_dialog_ctx_t* ctx = (nv_dialog_ctx_t*)malloc(sizeof(nv_dialog_ctx_t));
  if (!ctx) {
    nv_ipc_reply_err(w, seq, "ERR_IO", "dialog alloc failed", arena);
    return;
  }
  ctx->window = w;
  ctx->seq = seq;
  ctx->arena = arena;
  if (!w || !w->app) {
    nv_json_t* obj = nv_json_object(arena);
    nv_json_str(obj, "path", "");
    nv_ipc_reply_ok(w, seq, obj, arena);
    free(ctx);
    return;
  }
  const nv_platform_api_t* api = &w->app->platform_api;
  if (!api->dialog_open_folder_async) {
    nv_ipc_reply_err(w, seq, "ERR_NOT_SUPPORTED", "dialog not supported", arena);
    free(ctx);
    return;
  }
  api->dialog_open_folder_async(ctx, nv_dialog_open_folder_done);
}

NV_INTERNAL void nv_op_dialog_message(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
  const char* title;
  const char* body;
  const char* type = args ? nv_json_get_str(args, "type") : "info";
  const char* btnA = args ? nv_json_get_str(args, "buttonA") : NULL;
  const char* btnB = args ? nv_json_get_str(args, "buttonB") : NULL;
  const char* buttons[2];
  if (!nv_dialog_title_body_from_args(args, w, seq, arena, "dialog.message requires title and body", &title, &body, "Message", ""))
    return;
  if (!btnA) btnA = "OK";
  buttons[0] = btnA;
  buttons[1] = btnB;
  nv_dialog_ctx_t* ctx = (nv_dialog_ctx_t*)malloc(sizeof(nv_dialog_ctx_t));
  if (!ctx) {
    nv_ipc_reply_err(w, seq, "ERR_IO", "dialog alloc failed", arena);
    return;
  }
  ctx->window = w;
  ctx->seq = seq;
  ctx->arena = arena;
  if (!w || !w->app) {
    nv_json_t* obj = nv_json_object(arena);
    nv_json_int(obj, "buttonIndex", 0);
    nv_ipc_reply_ok(w, seq, obj, arena);
    free(ctx);
    return;
  }
  const nv_platform_api_t* api = &w->app->platform_api;
  if (!api->dialog_message_async) {
    nv_ipc_reply_err(w, seq, "ERR_NOT_SUPPORTED", "dialog not supported", arena);
    free(ctx);
    return;
  }
  api->dialog_message_async(title, body, type, buttons, btnB ? 2 : 1, ctx, nv_dialog_message_done);
}

NV_INTERNAL void nv_op_dialog_confirm(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
  const char* title;
  const char* body;
  if (!nv_dialog_title_body_from_args(args, w, seq, arena, "dialog.confirm requires title and body", &title, &body, "Confirm", ""))
    return;
  nv_dialog_ctx_t* ctx = (nv_dialog_ctx_t*)malloc(sizeof(nv_dialog_ctx_t));
  if (!ctx) {
    nv_ipc_reply_err(w, seq, "ERR_IO", "dialog alloc failed", arena);
    return;
  }
  ctx->window = w;
  ctx->seq = seq;
  ctx->arena = arena;
  if (!w || !w->app) {
    nv_json_t* obj = nv_json_object(arena);
    nv_json_bool(obj, "confirmed", 0);
    nv_ipc_reply_ok(w, seq, obj, arena);
    free(ctx);
    return;
  }
  const nv_platform_api_t* api = &w->app->platform_api;
  if (!api->dialog_confirm_async) {
    nv_ipc_reply_err(w, seq, "ERR_NOT_SUPPORTED", "dialog not supported", arena);
    free(ctx);
    return;
  }
  api->dialog_confirm_async(title, body, ctx, nv_dialog_confirm_done);
}
