/*
 * Copyright (c) 2026
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 */

#ifdef _WIN32

#include <windows.h>
#include <shobjidl.h>
#include <stdlib.h>

#include "nv_op_dialog.h"
#include "nv_window_internal.h"

NV_INTERNAL HWND nv_win_dialog_owner_hwnd(nv_window_t* window);

#if defined(__GNUC__) || defined(__clang__)
#define NV_WIN_DLG_ATTR __attribute__((weak))
#else
#define NV_WIN_DLG_ATTR
#endif

static char* nv_win_wide_to_utf8_malloc(const WCHAR* w) {
  if (!w) return NULL;
  int cb = WideCharToMultiByte(CP_UTF8, 0, w, -1, NULL, 0, NULL, NULL);
  if (cb <= 0) return NULL;
  char* out = (char*)malloc((size_t)cb);
  if (!out) return NULL;
  if (WideCharToMultiByte(CP_UTF8, 0, w, -1, out, cb, NULL, NULL) <= 0) {
    free(out);
    return NULL;
  }
  return out;
}

static void nv_win_release_shell_item_array(IShellItemArray* arr) {
  if (arr) arr->lpVtbl->Release(arr);
}

static void nv_win_release_shell_item(IShellItem* it) {
  if (it) it->lpVtbl->Release(it);
}

static void nv_win_release_file_open(IFileOpenDialog* d) {
  if (d) d->lpVtbl->Release(d);
}

static void nv_win_release_file_save(IFileSaveDialog* d) {
  if (d) d->lpVtbl->Release(d);
}

static char* nv_win_shell_item_path_malloc(IShellItem* item) {
  if (!item) return NULL;
  PWSTR wpath = NULL;
  HRESULT hr = item->lpVtbl->GetDisplayName(item, SIGDN_FILESYSPATH, &wpath);
  if (FAILED(hr) || !wpath) return NULL;
  char* utf8 = nv_win_wide_to_utf8_malloc(wpath);
  CoTaskMemFree(wpath);
  return utf8;
}

NV_INTERNAL NV_WIN_DLG_ATTR void nv_win_dialog_open_file_async(int allow_multiple,
                                                               nv_dialog_ctx_t* ctx,
                                                               nv_dialog_cb_t cb) {
  if (!ctx || !cb) return;

  IFileOpenDialog* dlg = NULL;
  HRESULT hr = CoCreateInstance(&CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER,
                                &IID_IFileOpenDialog, (void**)&dlg);
  if (FAILED(hr) || !dlg) {
    cb(ctx, 1, NULL);
    return;
  }

  IFileDialog* fd = (IFileDialog*)dlg;
  DWORD opts = 0;
  hr = fd->lpVtbl->GetOptions(fd, &opts);
  if (FAILED(hr)) {
    nv_win_release_file_open(dlg);
    cb(ctx, 1, NULL);
    return;
  }
  opts |= FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST;
  if (allow_multiple)
    opts |= FOS_ALLOWMULTISELECT;
  else
    opts &= (DWORD)~FOS_ALLOWMULTISELECT;
  hr = fd->lpVtbl->SetOptions(fd, opts);
  if (FAILED(hr)) {
    nv_win_release_file_open(dlg);
    cb(ctx, 1, NULL);
    return;
  }

  HWND owner = nv_win_dialog_owner_hwnd(ctx->window);
  hr = fd->lpVtbl->Show(fd, owner);
  if (FAILED(hr)) {
    nv_win_release_file_open(dlg);
    cb(ctx, 1, NULL);
    return;
  }

  if (allow_multiple) {
    IShellItemArray* items = NULL;
    hr = dlg->lpVtbl->GetResults(dlg, &items);
    if (FAILED(hr) || !items) {
      nv_win_release_file_open(dlg);
      cb(ctx, 1, NULL);
      return;
    }
    DWORD n = 0;
    items->lpVtbl->GetCount(items, &n);
    if (n == 0) {
      nv_win_release_shell_item_array(items);
      nv_win_release_file_open(dlg);
      cb(ctx, 1, NULL);
      return;
    }
    void* block = malloc(sizeof(size_t) + (size_t)n * sizeof(char*));
    if (!block) {
      nv_win_release_shell_item_array(items);
      nv_win_release_file_open(dlg);
      cb(ctx, 1, NULL);
      return;
    }
    *(size_t*)block = (size_t)n;
    char** paths = (char**)((size_t*)block + 1);
    for (DWORD i = 0; i < n; i++) {
      IShellItem* it = NULL;
      hr = items->lpVtbl->GetItemAt(items, i, &it);
      if (FAILED(hr) || !it) {
        for (DWORD j = 0; j < i; j++) free(paths[j]);
        free(block);
        nv_win_release_shell_item_array(items);
        nv_win_release_file_open(dlg);
        cb(ctx, 1, NULL);
        return;
      }
      paths[i] = nv_win_shell_item_path_malloc(it);
      nv_win_release_shell_item(it);
      if (!paths[i]) {
        for (DWORD j = 0; j <= i; j++) free(paths[j]);
        free(block);
        nv_win_release_shell_item_array(items);
        nv_win_release_file_open(dlg);
        cb(ctx, 1, NULL);
        return;
      }
    }
    nv_win_release_shell_item_array(items);
    nv_win_release_file_open(dlg);
    cb(ctx, 0, paths);
    return;
  }

  IShellItem* one = NULL;
  hr = dlg->lpVtbl->GetResult(dlg, &one);
  if (FAILED(hr) || !one) {
    nv_win_release_file_open(dlg);
    cb(ctx, 1, NULL);
    return;
  }
  char* path = nv_win_shell_item_path_malloc(one);
  nv_win_release_shell_item(one);
  nv_win_release_file_open(dlg);
  if (!path) {
    cb(ctx, 1, NULL);
    return;
  }
  void* block = malloc(sizeof(size_t) + sizeof(char*));
  if (!block) {
    free(path);
    cb(ctx, 1, NULL);
    return;
  }
  *(size_t*)block = 1u;
  char** paths = (char**)((size_t*)block + 1);
  paths[0] = path;
  cb(ctx, 0, paths);
}

NV_INTERNAL NV_WIN_DLG_ATTR void nv_win_dialog_save_file_async(nv_dialog_ctx_t* ctx,
                                                               nv_dialog_cb_t cb) {
  if (!ctx || !cb) return;

  IFileSaveDialog* dlg = NULL;
  HRESULT hr = CoCreateInstance(&CLSID_FileSaveDialog, NULL, CLSCTX_INPROC_SERVER,
                                &IID_IFileSaveDialog, (void**)&dlg);
  if (FAILED(hr) || !dlg) {
    cb(ctx, 1, NULL);
    return;
  }

  IFileDialog* fd = (IFileDialog*)dlg;
  DWORD opts = 0;
  hr = fd->lpVtbl->GetOptions(fd, &opts);
  if (FAILED(hr)) {
    nv_win_release_file_save(dlg);
    cb(ctx, 1, NULL);
    return;
  }
  opts |= FOS_FORCEFILESYSTEM | FOS_OVERWRITEPROMPT;
  hr = fd->lpVtbl->SetOptions(fd, opts);
  if (FAILED(hr)) {
    nv_win_release_file_save(dlg);
    cb(ctx, 1, NULL);
    return;
  }

  HWND owner = nv_win_dialog_owner_hwnd(ctx->window);
  hr = fd->lpVtbl->Show(fd, owner);
  if (FAILED(hr)) {
    nv_win_release_file_save(dlg);
    cb(ctx, 1, NULL);
    return;
  }

  IShellItem* item = NULL;
  hr = dlg->lpVtbl->GetResult(dlg, &item);
  if (FAILED(hr) || !item) {
    nv_win_release_file_save(dlg);
    cb(ctx, 1, NULL);
    return;
  }
  char* path = nv_win_shell_item_path_malloc(item);
  nv_win_release_shell_item(item);
  nv_win_release_file_save(dlg);
  if (!path) {
    cb(ctx, 1, NULL);
    return;
  }
  cb(ctx, 0, path);
}

NV_INTERNAL NV_WIN_DLG_ATTR void nv_win_dialog_open_folder_async(nv_dialog_ctx_t* ctx,
                                                                  nv_dialog_cb_t cb) {
  if (!ctx || !cb) return;

  IFileOpenDialog* dlg = NULL;
  HRESULT hr = CoCreateInstance(&CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER,
                                &IID_IFileOpenDialog, (void**)&dlg);
  if (FAILED(hr) || !dlg) {
    cb(ctx, 1, NULL);
    return;
  }

  IFileDialog* fd = (IFileDialog*)dlg;
  DWORD opts = 0;
  hr = fd->lpVtbl->GetOptions(fd, &opts);
  if (FAILED(hr)) {
    nv_win_release_file_open(dlg);
    cb(ctx, 1, NULL);
    return;
  }
  opts |= FOS_FORCEFILESYSTEM | FOS_PICKFOLDERS | FOS_PATHMUSTEXIST;
  opts &= (DWORD)~FOS_FILEMUSTEXIST;
  hr = fd->lpVtbl->SetOptions(fd, opts);
  if (FAILED(hr)) {
    nv_win_release_file_open(dlg);
    cb(ctx, 1, NULL);
    return;
  }

  HWND owner = nv_win_dialog_owner_hwnd(ctx->window);
  hr = fd->lpVtbl->Show(fd, owner);
  if (FAILED(hr)) {
    nv_win_release_file_open(dlg);
    cb(ctx, 1, NULL);
    return;
  }

  IShellItem* item = NULL;
  hr = dlg->lpVtbl->GetResult(dlg, &item);
  if (FAILED(hr) || !item) {
    nv_win_release_file_open(dlg);
    cb(ctx, 1, NULL);
    return;
  }
  char* path = nv_win_shell_item_path_malloc(item);
  nv_win_release_shell_item(item);
  nv_win_release_file_open(dlg);
  if (!path) {
    cb(ctx, 1, NULL);
    return;
  }
  cb(ctx, 0, path);
}

#endif /* _WIN32 */
