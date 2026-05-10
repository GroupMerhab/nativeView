/*
 * Copyright (c) 2026
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 */

#ifdef _WIN32

#include <windows.h>
#include <stdlib.h>
#include <string.h>

#include "nv_op_dialog.h"
#include "nv_window_internal.h"

NV_INTERNAL HWND nv_win_dialog_owner_hwnd(nv_window_t* window);

#if defined(__GNUC__) || defined(__clang__)
#define NV_WIN_DLG_ATTR __attribute__((weak))
#else
#define NV_WIN_DLG_ATTR
#endif

static WCHAR* nv_win_utf8_to_wide_malloc(const char* utf8) {
  if (!utf8) return NULL;
  int n = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
  if (n <= 0) return NULL;
  WCHAR* w = (WCHAR*)malloc((size_t)n * sizeof(WCHAR));
  if (!w) return NULL;
  if (MultiByteToWideChar(CP_UTF8, 0, utf8, -1, w, n) <= 0) {
    free(w);
    return NULL;
  }
  return w;
}

static UINT nv_win_message_box_icon(const char* type) {
  if (!type) return MB_ICONINFORMATION;
  if (strcmp(type, "warning") == 0) return MB_ICONWARNING;
  if (strcmp(type, "error") == 0) return MB_ICONERROR;
  return MB_ICONINFORMATION;
}

NV_INTERNAL NV_WIN_DLG_ATTR void nv_win_dialog_message_async(const char* title,
                                                             const char* body,
                                                             const char* type,
                                                             const char** buttons,
                                                             size_t btn_count,
                                                             nv_dialog_ctx_t* ctx,
                                                             nv_dialog_cb_t cb) {
  if (!ctx || !cb) return;
  (void)buttons;

  const char* t = title ? title : "Message";
  const char* b = body ? body : "";
  WCHAR* wtitle = nv_win_utf8_to_wide_malloc(t);
  WCHAR* wbody = nv_win_utf8_to_wide_malloc(b);
  if (!wtitle || !wbody) {
    free(wtitle);
    free(wbody);
    cb(ctx, 1, NULL);
    return;
  }

  UINT uType = nv_win_message_box_icon(type) | MB_SETFOREGROUND;
  if (btn_count >= 2)
    uType |= MB_OKCANCEL;
  else
    uType |= MB_OK;

  HWND owner = nv_win_dialog_owner_hwnd(ctx->window);
  int mr = MessageBoxW(owner, wbody, wtitle, uType);
  free(wbody);
  free(wtitle);

  int* idx = (int*)malloc(sizeof(int));
  if (!idx) {
    cb(ctx, 1, NULL);
    return;
  }
  if (btn_count >= 2) {
    if (mr == IDOK)
      *idx = 0;
    else if (mr == IDCANCEL)
      *idx = 1;
    else
      *idx = 0;
  } else {
    *idx = (mr == IDOK) ? 0 : 0;
  }
  cb(ctx, 0, idx);
}

NV_INTERNAL NV_WIN_DLG_ATTR void nv_win_dialog_confirm_async(const char* title,
                                                            const char* body,
                                                            nv_dialog_ctx_t* ctx,
                                                            nv_dialog_cb_t cb) {
  if (!ctx || !cb) return;

  const char* t = title ? title : "Confirm";
  const char* b = body ? body : "";
  WCHAR* wtitle = nv_win_utf8_to_wide_malloc(t);
  WCHAR* wbody = nv_win_utf8_to_wide_malloc(b);
  if (!wtitle || !wbody) {
    free(wtitle);
    free(wbody);
    cb(ctx, 1, NULL);
    return;
  }

  HWND owner = nv_win_dialog_owner_hwnd(ctx->window);
  int mr = MessageBoxW(owner, wbody, wtitle,
                       MB_OKCANCEL | MB_ICONQUESTION | MB_SETFOREGROUND);
  free(wbody);
  free(wtitle);

  int* confirmed = (int*)malloc(sizeof(int));
  if (!confirmed) {
    cb(ctx, 1, NULL);
    return;
  }
  *confirmed = (mr == IDOK) ? 1 : 0;
  cb(ctx, 0, confirmed);
}

#endif /* _WIN32 */
