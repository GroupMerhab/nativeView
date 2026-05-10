/*
 * Copyright (c) 2026
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* Windows platform implementation using Win32 + WebView2 */

#ifdef _WIN32

#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <winnls.h>
#include <objbase.h>
#include <roapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <wchar.h>

#include "WebView2.h"

/* Use CreateCoreWebView2EnvironmentWithOptions(nullptr, nullptr, nullptr, handler)
 * same as cross_dev - no extra options. */

#include "nv_core_internal.h"
#include "nv_window_internal.h"
#include "nv_window_manager.h"
#include "nv_ipc_internal.h"
#include "nv_arena.h"
#include "nv_util.h"
#include "nv_op_windows.h"
#include "nv_win_fs_watch.h"
#include "nv_win_tray.h"
#include "nv_win_hotkey.h"
#include "nv_json.h"
#include "nv.h"

/* =============================================================================
 * Helpers
 * =============================================================================
 */

NV_INTERNAL static void nv_win_log_hresult(const char* msg, HRESULT hr) {
  char* sys = NULL;
  FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                     FORMAT_MESSAGE_IGNORE_INSERTS,
                 NULL, hr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                 (LPSTR)&sys, 0, NULL);
  if (sys) {
    NV_ERR("%s (0x%08lx): %s", msg, (unsigned long)hr, sys);
    LocalFree(sys);
  } else {
    NV_ERR("%s (0x%08lx)", msg, (unsigned long)hr);
  }
}

static int nv_win_env_truthy(const char* value) {
  if (!value || !*value) return 0;
  if (value[0] == '1' && value[1] == '\0') return 1;
  if (value[0] == 't' || value[0] == 'T') return 1;
  if (value[0] == 'y' || value[0] == 'Y') return 1;
  if ((value[0] == 'o' || value[0] == 'O') && (value[1] == 'n' || value[1] == 'N') && value[2] == '\0') return 1;
  return 0;
}

static int nv_win_context_menu_enabled(void) {
  const char* value = getenv("NV_WEBVIEW_CONTEXT_MENU");
  return nv_win_env_truthy(value);
}

/* =============================================================================
 * App-level Platform State
 * =============================================================================
 */

typedef struct nv_win_app_platform {
  ICoreWebView2Environment* env;
  HANDLE env_event;
} nv_win_app_platform_t;

/* Environment completed handler */

typedef struct nv_win_env_handler {
  ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandlerVtbl* lpVtbl;
  LONG ref_count;
  HANDLE event;
  ICoreWebView2Environment** out_env;
} nv_win_env_handler_t;

static HRESULT STDMETHODCALLTYPE nv_win_env_handler_QueryInterface(
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler* self,
    REFIID riid,
    void** ppv) {
  if (!ppv) return E_POINTER;
  *ppv = NULL;
  if (IsEqualIID(riid, &IID_IUnknown) ||
      IsEqualIID(riid,
                 &IID_ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler)) {
    *ppv = self;
    ((nv_win_env_handler_t*)self)->ref_count++;
    return S_OK;
  }
  return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE nv_win_env_handler_AddRef(
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler* self) {
  nv_win_env_handler_t* h = (nv_win_env_handler_t*)self;
  return (ULONG)InterlockedIncrement(&h->ref_count);
}

static ULONG STDMETHODCALLTYPE nv_win_env_handler_Release(
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler* self) {
  nv_win_env_handler_t* h = (nv_win_env_handler_t*)self;
  LONG ref = InterlockedDecrement(&h->ref_count);
  if (ref == 0) {
    HeapFree(GetProcessHeap(), 0, h);
  }
  return (ULONG)ref;
}

static HRESULT STDMETHODCALLTYPE nv_win_env_handler_Invoke(
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler* self,
    HRESULT result,
    ICoreWebView2Environment* env) {
  nv_win_env_handler_t* h = (nv_win_env_handler_t*)self;
  if (SUCCEEDED(result) && env && h->out_env) {
    *h->out_env = env;
    env->lpVtbl->AddRef(env);
  } else {
    nv_win_log_hresult("WebView2 environment creation failed", result);
  }
  if (h->event) {
    SetEvent(h->event);
  }
  return S_OK;
}

static ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandlerVtbl
    g_env_handler_vtbl = {
        nv_win_env_handler_QueryInterface,
        nv_win_env_handler_AddRef,
        nv_win_env_handler_Release,
        nv_win_env_handler_Invoke};

static nv_win_env_handler_t* nv_win_env_handler_create(HANDLE event,
                                                       ICoreWebView2Environment** out_env) {
  nv_win_env_handler_t* h =
      (nv_win_env_handler_t*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                                       sizeof(nv_win_env_handler_t));
  if (!h) return NULL;
  h->lpVtbl = &g_env_handler_vtbl;
  h->ref_count = 1;
  h->event = event;
  h->out_env = out_env;
  return h;
}

/* =============================================================================
 * Window Platform State (moved up so controller handler Invoke can access members)
 * ============================================================================= */

#define WM_NV_EVAL_JS (WM_APP + 1)
#define WM_NV_SET_ZOOM (WM_APP + 2)

#define ID_EDIT_CUT      40001
#define ID_EDIT_COPY     40002
#define ID_EDIT_PASTE    40003
#define ID_EDIT_SELECT   40004

struct nv_win_window_platform {
  HWND hwnd;
  HWND hwndMsg;
  DWORD main_thread_id;  /* UI thread; ExecuteScript must run here */
  ICoreWebView2Controller* controller;
  ICoreWebView2* webview;
  nv_window_t* window;
};
typedef struct nv_win_window_platform nv_win_window_platform_t;

/* =============================================================================
 * WebView2 Controller Creation Handler
 * ============================================================================= */

typedef struct nv_win_ctrl_handler {
  ICoreWebView2CreateCoreWebView2ControllerCompletedHandlerVtbl* lpVtbl;
  LONG ref_count;
  HANDLE event;
  nv_win_window_platform_t* platform;
  nv_window_t* window;
} nv_win_ctrl_handler_t;

static HRESULT STDMETHODCALLTYPE nv_win_ctrl_handler_QueryInterface(
    ICoreWebView2CreateCoreWebView2ControllerCompletedHandler* self,
    REFIID riid, void** ppv) {
  if (!ppv) return E_POINTER;
  *ppv = NULL;
  if (IsEqualIID(riid, &IID_IUnknown) ||
      IsEqualIID(riid, &IID_ICoreWebView2CreateCoreWebView2ControllerCompletedHandler)) {
    *ppv = self;
    ((nv_win_ctrl_handler_t*)self)->ref_count++;
    return S_OK;
  }
  return E_NOINTERFACE;
}

/* QueryInterface for WebMessageReceived handler (supports IID_ICoreWebView2WebMessageReceivedEventHandler) */
static HRESULT STDMETHODCALLTYPE nv_win_webmsg_qi(
    ICoreWebView2WebMessageReceivedEventHandler* self, REFIID riid, void** ppv) {
  if (!ppv) return E_POINTER;
  *ppv = NULL;
  if (IsEqualIID(riid, &IID_IUnknown) ||
      IsEqualIID(riid, &IID_ICoreWebView2WebMessageReceivedEventHandler)) {
    *ppv = self;
    ((nv_win_ctrl_handler_t*)self)->ref_count++;
    return S_OK;
  }
  return E_NOINTERFACE;
}

/* NavigationCompleted handler (defined before nv_win_nav_qi uses it) */
typedef struct nv_win_nav_handler {
  ICoreWebView2NavigationCompletedEventHandlerVtbl* lpVtbl;
  LONG ref_count;
  nv_window_t* window;
} nv_win_nav_handler_t;

/* QueryInterface for NavigationCompleted handler */
static HRESULT STDMETHODCALLTYPE nv_win_nav_qi(
    ICoreWebView2NavigationCompletedEventHandler* self, REFIID riid, void** ppv) {
  if (!ppv) return E_POINTER;
  *ppv = NULL;
  if (IsEqualIID(riid, &IID_IUnknown) ||
      IsEqualIID(riid, &IID_ICoreWebView2NavigationCompletedEventHandler)) {
    *ppv = self;
    ((nv_win_nav_handler_t*)self)->ref_count++;
    return S_OK;
  }
  return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE nv_win_ctrl_handler_AddRef(
    ICoreWebView2CreateCoreWebView2ControllerCompletedHandler* self) {
  return (ULONG)InterlockedIncrement(&((nv_win_ctrl_handler_t*)self)->ref_count);
}

static ULONG STDMETHODCALLTYPE nv_win_ctrl_handler_Release(
    ICoreWebView2CreateCoreWebView2ControllerCompletedHandler* self) {
  nv_win_ctrl_handler_t* h = (nv_win_ctrl_handler_t*)self;
  LONG r = InterlockedDecrement(&h->ref_count);
  if (r == 0) HeapFree(GetProcessHeap(), 0, h);
  return (ULONG)r;
}

/* WebMessageReceived: forward JSON from JS to nv_ipc_dispatch */
static HRESULT STDMETHODCALLTYPE nv_win_webmessage_Invoke(
    ICoreWebView2WebMessageReceivedEventHandler* self,
    ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args) {
  (void)sender;
  nv_win_ctrl_handler_t* h = (nv_win_ctrl_handler_t*)self;
  nv_window_t* window = h->window;
  LPWSTR msg = NULL;
  HRESULT hr = args->lpVtbl->TryGetWebMessageAsString(args, &msg);
  if (FAILED(hr) || !msg) return S_OK;
  int len = WideCharToMultiByte(CP_UTF8, 0, msg, -1, NULL, 0, NULL, NULL);
  if (len > 0) {
    char* json = (char*)HeapAlloc(GetProcessHeap(), 0, (size_t)len);
    if (json) {
      WideCharToMultiByte(CP_UTF8, 0, msg, -1, json, len, NULL, NULL);
      nv_ipc_state_t* ipc = nv_window_get_ipc(window);
      if (ipc) nv_ipc_dispatch(window, ipc, json);
      HeapFree(GetProcessHeap(), 0, json);
    }
  }
  CoTaskMemFree(msg);
  return S_OK;
}

static ULONG STDMETHODCALLTYPE nv_win_webmsg_addref(
    ICoreWebView2WebMessageReceivedEventHandler* self) {
  return (ULONG)InterlockedIncrement(&((nv_win_ctrl_handler_t*)self)->ref_count);
}
static ULONG STDMETHODCALLTYPE nv_win_webmsg_release(
    ICoreWebView2WebMessageReceivedEventHandler* self) {
  nv_win_ctrl_handler_t* h = (nv_win_ctrl_handler_t*)self;
  LONG r = InterlockedDecrement(&h->ref_count);
  if (r == 0) HeapFree(GetProcessHeap(), 0, h);
  return (ULONG)r;
}

static ICoreWebView2WebMessageReceivedEventHandlerVtbl g_webmsg_vtbl = {
  (void*)nv_win_webmsg_qi,
  (void*)nv_win_webmsg_addref,
  (void*)nv_win_webmsg_release,
  nv_win_webmessage_Invoke
};

/* NavigationCompleted: invoke ready callback */
static ULONG STDMETHODCALLTYPE nv_win_nav_addref(
    ICoreWebView2NavigationCompletedEventHandler* self) {
  return (ULONG)InterlockedIncrement(&((nv_win_nav_handler_t*)self)->ref_count);
}
static ULONG STDMETHODCALLTYPE nv_win_nav_release(
    ICoreWebView2NavigationCompletedEventHandler* self) {
  nv_win_nav_handler_t* h = (nv_win_nav_handler_t*)self;
  LONG r = InterlockedDecrement(&h->ref_count);
  if (r == 0) HeapFree(GetProcessHeap(), 0, h);
  return (ULONG)r;
}

static HRESULT STDMETHODCALLTYPE nv_win_nav_Invoke(
    ICoreWebView2NavigationCompletedEventHandler* self,
    ICoreWebView2* sender, ICoreWebView2NavigationCompletedEventArgs* args) {
  (void)sender;
  (void)args;
  nv_win_nav_handler_t* h = (nv_win_nav_handler_t*)self;
  nv_ipc_state_t* ipc = nv_window_get_ipc(h->window);
  if (ipc) nv_ipc_invoke_ready(h->window, ipc);
  return S_OK;
}

static ICoreWebView2NavigationCompletedEventHandlerVtbl g_nav_vtbl = {
  (void*)nv_win_nav_qi,
  (void*)nv_win_nav_addref,
  (void*)nv_win_nav_release,
  nv_win_nav_Invoke
};

typedef struct nv_win_ctxreq_handler {
  ICoreWebView2ContextMenuRequestedEventHandlerVtbl* lpVtbl;
  LONG ref_count;
  nv_win_window_platform_t* platform;
} nv_win_ctxreq_handler_t;

static HRESULT STDMETHODCALLTYPE nv_win_ctxreq_qi(
    ICoreWebView2ContextMenuRequestedEventHandler* self, REFIID riid, void** ppv) {
  if (!ppv) return E_POINTER;
  *ppv = NULL;
  if (IsEqualIID(riid, &IID_IUnknown) ||
      IsEqualIID(riid, &IID_ICoreWebView2ContextMenuRequestedEventHandler)) {
    *ppv = self;
    ((nv_win_ctxreq_handler_t*)self)->ref_count++;
    return S_OK;
  }
  return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE nv_win_ctxreq_addref(ICoreWebView2ContextMenuRequestedEventHandler* self) {
  return (ULONG)InterlockedIncrement(&((nv_win_ctxreq_handler_t*)self)->ref_count);
}

static ULONG STDMETHODCALLTYPE nv_win_ctxreq_release(ICoreWebView2ContextMenuRequestedEventHandler* self) {
  nv_win_ctxreq_handler_t* h = (nv_win_ctxreq_handler_t*)self;
  LONG r = InterlockedDecrement(&h->ref_count);
  if (r == 0) HeapFree(GetProcessHeap(), 0, h);
  return (ULONG)r;
}

static HRESULT STDMETHODCALLTYPE nv_win_ctxreq_Invoke(ICoreWebView2ContextMenuRequestedEventHandler* self,
                                                      ICoreWebView2* sender,
                                                      ICoreWebView2ContextMenuRequestedEventArgs* args) {
  (void)sender;
  nv_win_ctxreq_handler_t* h = (nv_win_ctxreq_handler_t*)self;
  nv_win_window_platform_t* plat = h->platform;
  nv_window_t* w = plat && plat->window ? plat->window : NULL;
  if (!w || w->ctx_menu_count <= 0 || !w->ctx_menu_items || !plat || !plat->hwnd) {
    return S_OK;
  }
  (void)args->lpVtbl->put_Handled(args, TRUE);
  HMENU hm = CreatePopupMenu();
  if (!hm) return S_OK;

  int id_to_item[128];
  int nmap = 0;
  UINT menu_cmd = 100;
  for (int i = 0; i < w->ctx_menu_count && nmap < 128; i++) {
    const nv_menu_item_t* it = &w->ctx_menu_items[i];
    if (it->separator) {
      AppendMenuW(hm, MF_SEPARATOR, 0, NULL);
      continue;
    }
    WCHAR wlab[512];
    wlab[0] = 0;
    if (it->label) {
      MultiByteToWideChar(CP_UTF8, 0, it->label, -1, wlab, (int)(sizeof(wlab) / sizeof(wlab[0])));
    }
    UINT flags = MF_STRING;
    if (!it->enabled) flags |= MF_GRAYED;
    if (!AppendMenuW(hm, flags, (UINT_PTR)menu_cmd, wlab[0] ? wlab : L"")) {
      continue;
    }
    id_to_item[nmap++] = i;
    menu_cmd++;
  }

  if (nmap == 0) {
    DestroyMenu(hm);
    return S_OK;
  }

  POINT pt;
  if (!GetCursorPos(&pt)) {
    pt.x = 0;
    pt.y = 0;
  }

  int sel = (int)TrackPopupMenu(hm, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY, pt.x, pt.y, 0, plat->hwnd, NULL);
  DestroyMenu(hm);
  if (sel < 100) return S_OK;
  int idx = sel - 100;
  if (idx < 0 || idx >= nmap) return S_OK;
  int itemidx = id_to_item[idx];
  const char* mid = w->ctx_menu_items[itemidx].id;

  nv_arena_t* ar = nv_window_get_arena(w);
  if (!ar || !mid) return S_OK;
  nv_json_t* jo = nv_json_object(ar);
  if (!jo) return S_OK;
  nv_json_str(jo, "id", mid);
  const char* pl = nv_json_serialize(jo);
  if (pl) nv_send(w, "window.contextMenuAction", pl);
  return S_OK;
}

static ICoreWebView2ContextMenuRequestedEventHandlerVtbl g_ctxreq_vtbl = {
    (void*)nv_win_ctxreq_qi,
    (void*)nv_win_ctxreq_addref,
    (void*)nv_win_ctxreq_release,
    nv_win_ctxreq_Invoke};

static HRESULT STDMETHODCALLTYPE nv_win_ctrl_handler_Invoke(
    ICoreWebView2CreateCoreWebView2ControllerCompletedHandler* self,
    HRESULT result, ICoreWebView2Controller* controller) {
  nv_win_ctrl_handler_t* h = (nv_win_ctrl_handler_t*)self;
  nv_win_window_platform_t* platform = h->platform;
  nv_window_t* window = h->window;

  if (FAILED(result) || !controller) {
    nv_win_log_hresult("CreateCoreWebView2Controller failed", result);
    if (h->event) SetEvent(h->event);
    return S_OK;
  }

  platform->controller = controller;
  controller->lpVtbl->AddRef(controller);

  ICoreWebView2* webview = NULL;
  result = controller->lpVtbl->get_CoreWebView2(controller, &webview);
  if (FAILED(result) || !webview) {
    nv_win_log_hresult("get_CoreWebView2 failed", result);
    if (h->event) SetEvent(h->event);
    return S_OK;
  }
  platform->webview = webview;

  {
    ICoreWebView2Settings* settings = NULL;
    HRESULT shr = webview->lpVtbl->get_Settings(webview, &settings);
    if (SUCCEEDED(shr) && settings) {
      int enable_ctx = nv_win_context_menu_enabled() || (window && window->ctx_menu_count > 0);
      settings->lpVtbl->put_AreDefaultContextMenusEnabled(settings, enable_ctx ? TRUE : FALSE);
      settings->lpVtbl->Release(settings);
    }
  }

  RECT rc;
  GetClientRect(platform->hwnd, &rc);
  controller->lpVtbl->put_Bounds(controller, rc);
  controller->lpVtbl->put_IsVisible(controller, TRUE);

  /* Inject IPC bridge script (Windows: window.chrome.webview.postMessage) */
  const char* raw = nv_ipc_inject_script();
  if (raw) {
    const char* needle = "/*{NV_POST}*/";
    const char* repl = "window.chrome.webview.postMessage";
    size_t raw_len = strlen(raw);
    char* buf = (char*)HeapAlloc(GetProcessHeap(), 0, raw_len + 256);
    if (buf) {
      const char* pos = strstr(raw, needle);
      if (pos) {
        size_t pre = (size_t)(pos - raw);
        memcpy(buf, raw, pre);
        memcpy(buf + pre, repl, strlen(repl));
        strcpy(buf + pre + strlen(repl), pos + strlen(needle));
      } else {
        strcpy(buf, raw);
      }
      int wlen = MultiByteToWideChar(CP_UTF8, 0, buf, -1, NULL, 0);
      if (wlen > 0) {
        WCHAR* wbuf = (WCHAR*)HeapAlloc(GetProcessHeap(), 0, (size_t)wlen * sizeof(WCHAR));
        if (wbuf) {
          MultiByteToWideChar(CP_UTF8, 0, buf, -1, wbuf, wlen);
          webview->lpVtbl->AddScriptToExecuteOnDocumentCreated(webview, wbuf, NULL);
          HeapFree(GetProcessHeap(), 0, wbuf);
        }
      }
      HeapFree(GetProcessHeap(), 0, buf);
    }
  }

  /* WebMessageReceived: forward JS postMessage to nv_ipc_dispatch */
  {
    ICoreWebView2WebMessageReceivedEventHandler* handler =
        (ICoreWebView2WebMessageReceivedEventHandler*)HeapAlloc(GetProcessHeap(),
            HEAP_ZERO_MEMORY, sizeof(nv_win_ctrl_handler_t));
    if (handler) {
      nv_win_ctrl_handler_t* wh = (nv_win_ctrl_handler_t*)handler;
      wh->lpVtbl = (void*)&g_webmsg_vtbl;
      wh->ref_count = 1;
      wh->window = window;
      EventRegistrationToken tok;
      webview->lpVtbl->add_WebMessageReceived(webview, handler, &tok);
      /* Don't release - handler stays for lifetime of webview */
    }
  }

  /* NavigationCompleted for ready callback */
  {
    nv_win_nav_handler_t* nh = (nv_win_nav_handler_t*)HeapAlloc(GetProcessHeap(),
        HEAP_ZERO_MEMORY, sizeof(nv_win_nav_handler_t));
    if (nh) {
      nh->lpVtbl = (void*)&g_nav_vtbl;
      nh->ref_count = 1;
      nh->window = window;
      EventRegistrationToken tok;
      webview->lpVtbl->add_NavigationCompleted(webview,
          (ICoreWebView2NavigationCompletedEventHandler*)nh, &tok);
    }
  }

  {
    ICoreWebView2_11* w11 = NULL;
    if (SUCCEEDED(webview->lpVtbl->QueryInterface(webview, &IID_ICoreWebView2_11, (void**)&w11)) && w11) {
      nv_win_ctxreq_handler_t* ch =
          (nv_win_ctxreq_handler_t*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(nv_win_ctxreq_handler_t));
      if (ch) {
        ch->lpVtbl = &g_ctxreq_vtbl;
        ch->ref_count = 1;
        ch->platform = platform;
        EventRegistrationToken ctok;
        w11->lpVtbl->add_ContextMenuRequested(w11, (ICoreWebView2ContextMenuRequestedEventHandler*)ch, &ctok);
      }
      w11->lpVtbl->Release(w11);
    }
  }

  if (window->cfg.devtools) {
    webview->lpVtbl->OpenDevToolsWindow(webview);
  }

  if (h->event) SetEvent(h->event);
  return S_OK;
}

static ICoreWebView2CreateCoreWebView2ControllerCompletedHandlerVtbl g_ctrl_handler_vtbl = {
  nv_win_ctrl_handler_QueryInterface,
  nv_win_ctrl_handler_AddRef,
  nv_win_ctrl_handler_Release,
  nv_win_ctrl_handler_Invoke
};

/* =============================================================================
 * App Platform Hooks
 * =============================================================================
 */

/* Persistent WebView2 profile under %LOCALAPPDATA%\\elkotobi\\webview2 (faster cold start).
 * Falls back to a unique per-run temp folder if LOCALAPPDATA is unavailable. */
NV_INTERNAL static LPCWSTR nv_win_make_ephemeral_user_data_folder(wchar_t* buf, DWORD buf_count);

NV_INTERNAL static LPCWSTR nv_win_elkotobi_webview2_user_data_folder(wchar_t* buf, DWORD buf_count) {
  wchar_t lad[MAX_PATH];
  DWORD nlad = GetEnvironmentVariableW(L"LOCALAPPDATA", lad, (DWORD)(sizeof(lad) / sizeof(wchar_t)));
  if (nlad == 0 || nlad >= sizeof(lad) / sizeof(wchar_t))
    return nv_win_make_ephemeral_user_data_folder(buf, buf_count);
  wchar_t elk[MAX_PATH];
  int n1 = _snwprintf_s(elk, (DWORD)(sizeof(elk) / sizeof(wchar_t)), _TRUNCATE, L"%ls\\elkotobi", lad);
  if (n1 < 0 || n1 >= (int)(sizeof(elk) / sizeof(wchar_t)))
    return nv_win_make_ephemeral_user_data_folder(buf, buf_count);
  if (!CreateDirectoryW(elk, NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
    return nv_win_make_ephemeral_user_data_folder(buf, buf_count);
  int n2 = _snwprintf_s(buf, buf_count, _TRUNCATE, L"%ls\\webview2", elk);
  if (n2 < 0 || n2 >= (int)buf_count)
    return nv_win_make_ephemeral_user_data_folder(buf, buf_count);
  if (!CreateDirectoryW(buf, NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
    return nv_win_make_ephemeral_user_data_folder(buf, buf_count);
  return buf;
}

NV_INTERNAL static LPCWSTR nv_win_make_ephemeral_user_data_folder(wchar_t* buf, DWORD buf_count) {
  wchar_t temp[MAX_PATH];
  if (GetTempPathW((DWORD)(sizeof(temp) / sizeof(wchar_t)), temp) == 0)
    return NULL;
  /* Unique subdir per process and tick so each run gets a fresh profile */
  DWORD pid = GetCurrentProcessId();
  DWORD tick = GetTickCount();
  int n = _snwprintf_s(buf, buf_count, _TRUNCATE, L"%lsnv_webview_%lu_%lu", temp, (unsigned long)pid, (unsigned long)tick);
  if (n < 0 || n >= (int)buf_count)
    return NULL;
  if (!CreateDirectoryW(buf, NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
    return NULL;
  return buf;
}

typedef LONG(WINAPI* RtlGetVersion_t)(OSVERSIONINFOEXW*);

/* True kernel version (no manifest); rejects Win7/8/8.1 where WebView2 is unsupported. */
static int nv_win_is_windows10_or_greater(void) {
  HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
  if (!ntdll) return 0;
  RtlGetVersion_t fn = (RtlGetVersion_t)(void*)GetProcAddress(ntdll, "RtlGetVersion");
  if (!fn) return 0;
  OSVERSIONINFOEXW vi;
  ZeroMemory(&vi, sizeof(vi));
  vi.dwOSVersionInfoSize = sizeof(vi);
  if (fn(&vi) != 0 /* STATUS_SUCCESS */) return 0;
  return (vi.dwMajorVersion > 10) ||
         (vi.dwMajorVersion == 10 && vi.dwMinorVersion >= 0);
}

NV_INTERNAL void nv_app_platform_init(nv_app_t* app) {
  if (!app) return;

  if (!nv_win_is_windows10_or_greater()) {
    MessageBoxW(
        NULL,
        L"This application requires Windows 10 or later.\n"
        L"Please upgrade your operating system.",
        L"Unsupported Windows Version",
        MB_OK | MB_ICONERROR);
    return;
  }

  HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
  if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
    nv_win_log_hresult("CoInitializeEx failed", hr);
    return;
  }

  {
    HRESULT roro = RoInitialize(RO_INIT_SINGLETHREADED);
    if (FAILED(roro) && roro != RPC_E_CHANGED_MODE) {
      nv_win_log_hresult("RoInitialize failed", roro);
    }
  }

  nv_win_app_platform_t* platform =
      (nv_win_app_platform_t*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                                        sizeof(nv_win_app_platform_t));
  if (!platform) {
    NV_ERR("Win32: failed to allocate app platform");
    return;
  }

  platform->env_event =
      CreateEventW(NULL, TRUE, FALSE, NULL); /* manual-reset, initially non-signaled */
  if (!platform->env_event) {
    NV_ERR("Win32: CreateEvent for WebView2 env failed");
    HeapFree(GetProcessHeap(), 0, platform);
    return;
  }

  ICoreWebView2Environment* env = NULL;
  nv_win_env_handler_t* handler =
      nv_win_env_handler_create(platform->env_event, &env);
  if (!handler) {
    NV_ERR("Win32: failed to allocate env handler");
    CloseHandle(platform->env_event);
    HeapFree(GetProcessHeap(), 0, platform);
    return;
  }

  /* Prefer %LOCALAPPDATA%\\elkotobi\\webview2; fall back to ephemeral temp on failure. */
  static wchar_t user_data_folder[MAX_PATH];
  LPCWSTR user_data_path = nv_win_elkotobi_webview2_user_data_folder(user_data_folder, MAX_PATH);
  if (!user_data_path) {
    NV_ERR("Win32: failed to create ephemeral WebView2 user data folder");
    handler->lpVtbl->Release(
        (ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler*)handler);
    CloseHandle(platform->env_event);
    HeapFree(GetProcessHeap(), 0, platform);
    return;
  }

  hr = CreateCoreWebView2EnvironmentWithOptions(
      NULL, user_data_path, NULL,
      (ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler*)handler);

  if (FAILED(hr)) {
    nv_win_log_hresult("CreateCoreWebView2EnvironmentWithOptions failed", hr);
    handler->lpVtbl->Release(
        (ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler*)handler);
    CloseHandle(platform->env_event);
    HeapFree(GetProcessHeap(), 0, platform);
    return;
  }

  /* Wait up to 10 seconds for environment creation */
  DWORD wait = WaitForSingleObject(platform->env_event, 10000);
  if (wait != WAIT_OBJECT_0) {
    NV_ERR("Win32: WebView2 environment creation timed out");
    handler->lpVtbl->Release(
        (ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler*)handler);
    CloseHandle(platform->env_event);
    if (env) env->lpVtbl->Release(env);
    HeapFree(GetProcessHeap(), 0, platform);
    return;
  }

  handler->lpVtbl->Release(
      (ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler*)handler);

  platform->env = env;

  nv_app_set_platform(app, platform);
}

NV_INTERNAL void nv_app_platform_run(nv_app_t* app) {
  (void)app;
  MSG msg;
  while (GetMessageW(&msg, NULL, 0, 0) > 0) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }
}

NV_INTERNAL void nv_app_platform_quit(nv_app_t* app) {
  (void)app;
  PostQuitMessage(0);
}

/* =============================================================================
 * Window Platform State (struct defined above)
 * =============================================================================
 */

static LRESULT CALLBACK nv_win_wndproc(HWND hwnd, UINT msg, WPARAM wparam,
                                       LPARAM lparam) {
  nv_win_window_platform_t* platform =
      (nv_win_window_platform_t*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
  {
    LRESULT tray_lr = 0;
    if (nv_win_tray_wndproc(hwnd, msg, wparam, lparam, platform, &tray_lr)) {
      return tray_lr;
    }
  }
  {
    LRESULT fs_lr = 0;
    if (nv_win_fs_watch_wndproc(hwnd, msg, wparam, lparam, platform, &fs_lr)) {
      return fs_lr;
    }
  }
  {
    LRESULT hk_lr = 0;
    if (nv_win_hotkey_wndproc(hwnd, msg, wparam, lparam, platform, &hk_lr)) {
      return hk_lr;
    }
  }

  switch (msg) {
    case WM_NV_EVAL_JS: {
      /* ExecuteScript marshaled from worker thread; lParam = heap-allocated UTF-8 script */
      char* js = (char*)(void*)lparam;
      if (platform && platform->webview && js) {
        int len = (int)strlen(js);
        int wlen = MultiByteToWideChar(CP_UTF8, 0, js, len, NULL, 0);
        if (wlen > 0) {
          WCHAR* wjs = (WCHAR*)HeapAlloc(GetProcessHeap(), 0, (size_t)(wlen + 1) * sizeof(WCHAR));
          if (wjs) {
            MultiByteToWideChar(CP_UTF8, 0, js, len, wjs, wlen);
            wjs[wlen] = L'\0';
            platform->webview->lpVtbl->ExecuteScript(platform->webview, wjs, NULL);
            HeapFree(GetProcessHeap(), 0, wjs);
          }
        }
        HeapFree(GetProcessHeap(), 0, js);
      } else if (js) {
        HeapFree(GetProcessHeap(), 0, js);
      }
      return 0;
    }
    case WM_NV_SET_ZOOM: {
      double* pf = (double*)(void*)lparam;
      if (platform && platform->controller && pf) {
        platform->controller->lpVtbl->put_ZoomFactor(platform->controller, *pf);
      }
      if (pf) HeapFree(GetProcessHeap(), 0, pf);
      return 0;
    }
    case WM_SIZE:
      if (platform && platform->controller) {
        RECT rc;
        GetClientRect(hwnd, &rc);
        platform->controller->lpVtbl->put_Bounds(platform->controller, rc);
      }
      break;
    case WM_CLOSE:
      if (platform && platform->window) {
        /* Hide immediately so the user sees the window close without waiting for teardown */
        ShowWindow(hwnd, SW_HIDE);
        const char *id = nv_wm_get_id_by_window(platform->window);
        if (id) {
          nv_arena_t *arena = nv_arena_create(1024);
          nv_json_t *before = nv_json_object(arena);
          nv_json_str(before, "id", id);
          nv_op_windows_push_all("windows.beforeClose", before, arena);
          nv_arena_destroy(arena);
        }
        /* destroy the window, which will also unregister it */
        /* clear userdata so WM_DESTROY doesn't try to use freed pointer */
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        nv_window_destroy(platform->window);
      }
      return 0;
    case WM_DESTROY:
      PostQuitMessage(0);
      break;
    case WM_COMMAND: {
      /* Edit menu: Cut/Copy/Paste/Select All - forward to WebView so Ctrl+C/X/V/A work */
      if (platform && platform->webview) {
        UINT id = LOWORD(wparam);
        const char* cmd = NULL;
        if (id == ID_EDIT_CUT) cmd = "cut";
        else if (id == ID_EDIT_COPY) cmd = "copy";
        else if (id == ID_EDIT_PASTE) cmd = "paste";
        else if (id == ID_EDIT_SELECT) cmd = "selectAll";
        if (cmd) {
          char js[64];
          int n = snprintf(js, sizeof(js), "document.execCommand('%s')", cmd);
          if (n > 0 && (size_t)n < sizeof(js)) {
            int wlen = MultiByteToWideChar(CP_UTF8, 0, js, -1, NULL, 0);
            if (wlen > 0) {
              WCHAR* wjs = (WCHAR*)HeapAlloc(GetProcessHeap(), 0, (size_t)wlen * sizeof(WCHAR));
              if (wjs) {
                MultiByteToWideChar(CP_UTF8, 0, js, -1, wjs, wlen);
                platform->webview->lpVtbl->ExecuteScript(platform->webview, wjs, NULL);
                HeapFree(GetProcessHeap(), 0, wjs);
              }
            }
          }
          return 0;
        }
      }
      break;
    }
    default:
      break;
  }

  return DefWindowProcW(hwnd, msg, wparam, lparam);
}

static nv_win_app_platform_t* nv_win_get_app_platform(nv_window_t* window) {
  if (!window || !window->app) return NULL;
  return (nv_win_app_platform_t*)nv_app_get_platform(window->app);
}

static nv_win_window_platform_t* nv_win_get_window_platform(
    nv_window_t* window) {
  if (!window) return NULL;
  return (nv_win_window_platform_t*)nv_window_get_platform(window);
}

NV_INTERNAL HWND nv_win_window_hwnd(nv_window_t* window) {
  nv_win_window_platform_t* p = nv_win_get_window_platform(window);
  return (p && p->hwnd) ? p->hwnd : NULL;
}

/* =============================================================================
 * Window Platform Hooks
 * =============================================================================
 */

NV_INTERNAL void nv_window_platform_create(nv_window_t* window) {
  if (!window) return;

  nv_win_app_platform_t* app_platform = nv_win_get_app_platform(window);
  if (!app_platform || !app_platform->env) {
    NV_ERR("Win32: no WebView2 environment available for window");
    return;
  }

  /* Register window class */
  WNDCLASSEXW wc;
  ZeroMemory(&wc, sizeof(wc));
  wc.cbSize = sizeof(wc);
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = nv_win_wndproc;
  wc.hInstance = GetModuleHandleW(NULL);
  /* MinGW: IDC_ARROW is a narrow MAKEINTRESOURCE; LoadCursorW expects LPCWSTR. */
  wc.hCursor = LoadCursorW(NULL, (LPCWSTR)(ULONG_PTR)IDC_ARROW);
  wc.lpszClassName = L"nativeview_win32";
  RegisterClassExW(&wc);

  DWORD style = WS_OVERLAPPEDWINDOW;
  if (!window->cfg.resizable) {
    style &= ~WS_MAXIMIZEBOX;
    style &= ~WS_THICKFRAME;
  }

  RECT rc = {0, 0, window->cfg.width, window->cfg.height};
  AdjustWindowRect(&rc, style, FALSE);

  LPCWSTR title = L"App";
  if (window->cfg.title) {
    /* best-effort narrow→wide conversion */
    int len = (int)strlen(window->cfg.title);
    int wlen =
        MultiByteToWideChar(CP_UTF8, 0, window->cfg.title, len, NULL, 0);
    if (wlen > 0) {
      WCHAR* buf =
          (WCHAR*)HeapAlloc(GetProcessHeap(), 0, (wlen + 1) * sizeof(WCHAR));
      if (buf) {
        MultiByteToWideChar(CP_UTF8, 0, window->cfg.title, len, buf, wlen);
        buf[wlen] = L'\0';
        title = buf;
      }
    }
  }

  HWND hwnd = CreateWindowExW(0, wc.lpszClassName, title, style,
                              CW_USEDEFAULT, CW_USEDEFAULT,
                              rc.right - rc.left, rc.bottom - rc.top, NULL,
                              NULL, wc.hInstance, NULL);

  if (title != L"App") {
    HeapFree(GetProcessHeap(), 0, (void*)title);
  }

  if (!hwnd) {
    NV_ERR("Win32: CreateWindowEx failed");
    return;
  }

  /* .exe icon resources do not apply to the HWND; set big/small icons (RT_ICON id 1). */
  {
    HINSTANCE hi = wc.hInstance;
    HICON icon_big = (HICON)LoadImageW(
        hi, MAKEINTRESOURCEW(1), IMAGE_ICON, GetSystemMetrics(SM_CXICON),
        GetSystemMetrics(SM_CYICON), 0);
    HICON icon_sm =
        (HICON)LoadImageW(hi, MAKEINTRESOURCEW(1), IMAGE_ICON,
                          GetSystemMetrics(SM_CXSMICON),
                          GetSystemMetrics(SM_CYSMICON), 0);
    if (icon_big) {
      SendMessageW(hwnd, WM_SETICON, ICON_BIG, (LPARAM)icon_big);
    }
    if (icon_sm) {
      SendMessageW(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)icon_sm);
    }
  }

  nv_win_window_platform_t* platform =
      (nv_win_window_platform_t*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                                           sizeof(nv_win_window_platform_t));
  if (!platform) {
    NV_ERR("Win32: failed to allocate window platform");
    DestroyWindow(hwnd);
    return;
  }

  platform->hwnd = hwnd;
  platform->window = window;
  platform->main_thread_id = GetCurrentThreadId();

  SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)platform);

  /* Create Edit menu so Ctrl+C/X/V/A (copy/cut/paste/select all) work in WebView */
  {
    HMENU editMenu = CreateMenu();
    if (editMenu) {
      AppendMenuW(editMenu, MF_STRING, ID_EDIT_CUT, L"Cu&t\tCtrl+X");
      AppendMenuW(editMenu, MF_STRING, ID_EDIT_COPY, L"&Copy\tCtrl+C");
      AppendMenuW(editMenu, MF_STRING, ID_EDIT_PASTE, L"&Paste\tCtrl+V");
      AppendMenuW(editMenu, MF_STRING, ID_EDIT_SELECT, L"Select &All\tCtrl+A");
      HMENU menuBar = CreateMenu();
      if (menuBar) {
        AppendMenuW(menuBar, MF_POPUP, (UINT_PTR)editMenu, L"&Edit");
        SetMenu(hwnd, menuBar);
      }
    }
  }

  /* Create WebView2 controller (async; wait with message pump) */
  HANDLE ctrl_event = CreateEventW(NULL, TRUE, FALSE, NULL);
  if (!ctrl_event) {
    NV_ERR("Win32: CreateEvent for WebView2 controller failed");
    nv_window_set_platform(window, platform);
    return;
  }

  nv_win_ctrl_handler_t* ctrl_h =
      (nv_win_ctrl_handler_t*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                                        sizeof(nv_win_ctrl_handler_t));
  if (!ctrl_h) {
    CloseHandle(ctrl_event);
    nv_window_set_platform(window, platform);
    return;
  }
  ctrl_h->lpVtbl = (void*)&g_ctrl_handler_vtbl;
  ctrl_h->ref_count = 1;
  ctrl_h->event = ctrl_event;
  ctrl_h->platform = platform;
  ctrl_h->window = window;

  HRESULT hr = app_platform->env->lpVtbl->CreateCoreWebView2Controller(
      app_platform->env, hwnd,
      (ICoreWebView2CreateCoreWebView2ControllerCompletedHandler*)ctrl_h);

  if (FAILED(hr)) {
    nv_win_log_hresult("CreateCoreWebView2Controller failed", hr);
    ctrl_h->lpVtbl->Release(
        (ICoreWebView2CreateCoreWebView2ControllerCompletedHandler*)ctrl_h);
    CloseHandle(ctrl_event);
    nv_window_set_platform(window, platform);
    return;
  }

  /* Pump messages while waiting (WebView2 callbacks run on this thread) */
  while (WaitForSingleObject(ctrl_event, 0) != WAIT_OBJECT_0) {
    MSG msg;
    while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
    if (WaitForSingleObject(ctrl_event, 100) == WAIT_OBJECT_0) break;
  }
  CloseHandle(ctrl_event);
  ctrl_h->lpVtbl->Release(
      (ICoreWebView2CreateCoreWebView2ControllerCompletedHandler*)ctrl_h);

  nv_window_set_platform(window, platform);
}

NV_INTERNAL void nv_window_platform_destroy(nv_window_t* window) {
  if (!window) return;

  nv_win_window_platform_t* platform = nv_win_get_window_platform(window);
  if (!platform) return;

  nv_win_fs_watch_detach_for_window(window);

  if (platform->hwnd) {
    nv_win_tray_detach_hwnd(platform->hwnd);
  }

  if (platform->controller) {
    platform->controller->lpVtbl->Close(platform->controller);
    platform->controller->lpVtbl->Release(platform->controller);
  }
  if (platform->webview) {
    platform->webview->lpVtbl->Release(platform->webview);
  }
  if (platform->hwnd) {
    DestroyWindow(platform->hwnd);
  }

  HeapFree(GetProcessHeap(), 0, platform);
  nv_window_set_platform(window, NULL);
}

NV_INTERNAL void nv_window_platform_show(nv_window_t* window) {
  nv_win_window_platform_t* platform = nv_win_get_window_platform(window);
  if (!platform || !platform->hwnd) return;

  ShowWindow(platform->hwnd, SW_SHOW);
  UpdateWindow(platform->hwnd);
  /* Refresh WebView2 bounds so it paints correctly when window becomes visible */
  if (platform->controller) {
    RECT rc;
    if (GetClientRect(platform->hwnd, &rc)) {
      platform->controller->lpVtbl->put_Bounds(platform->controller, rc);
    }
  }
  window->visible = 1;
}

NV_INTERNAL void nv_window_platform_hide(nv_window_t* window) {
  nv_win_window_platform_t* platform = nv_win_get_window_platform(window);
  if (!platform || !platform->hwnd) return;

  ShowWindow(platform->hwnd, SW_HIDE);
  window->visible = 0;
}

NV_INTERNAL void nv_window_platform_set_modal(nv_window_t* window, int enable) {
  nv_win_window_platform_t* platform = nv_win_get_window_platform(window);
  if (!platform || !platform->hwnd) return;

  if (enable) {
    HWND parent = GetAncestor(platform->hwnd, GA_PARENT);
    if (parent) EnableWindow(parent, FALSE);
  } else {
    HWND parent = GetAncestor(platform->hwnd, GA_PARENT);
    if (parent) EnableWindow(parent, TRUE);
  }
}

NV_INTERNAL void nv_window_platform_set_title(nv_window_t* window, const char* title) {
  nv_win_window_platform_t* platform = nv_win_get_window_platform(window);
  if (!platform || !platform->hwnd) return;
  if (!title) return;
  int len = (int)strlen(title);
  int wlen = MultiByteToWideChar(CP_UTF8, 0, title, len, NULL, 0);
  if (wlen <= 0) return;
  WCHAR* buf = (WCHAR*)HeapAlloc(GetProcessHeap(), 0, (size_t)(wlen + 1) * sizeof(WCHAR));
  if (!buf) return;
  MultiByteToWideChar(CP_UTF8, 0, title, len, buf, wlen);
  buf[wlen] = L'\0';
  SetWindowTextW(platform->hwnd, buf);
  HeapFree(GetProcessHeap(), 0, buf);
}

NV_INTERNAL void nv_window_platform_set_size(nv_window_t* window, int width, int height) {
  nv_win_window_platform_t* platform = nv_win_get_window_platform(window);
  if (!platform || !platform->hwnd) return;
  SetWindowPos(platform->hwnd, NULL, 0, 0, width, height,
               SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

NV_INTERNAL void nv_window_platform_set_position(nv_window_t* window, int x, int y) {
  nv_win_window_platform_t* platform = nv_win_get_window_platform(window);
  if (!platform || !platform->hwnd) return;
  SetWindowPos(platform->hwnd, NULL, x, y, 0, 0,
               SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

NV_INTERNAL void nv_window_platform_get_size(nv_window_t* window, int* out_w, int* out_h) {
  nv_win_window_platform_t* platform = nv_win_get_window_platform(window);
  if (!platform || !platform->hwnd || !out_w || !out_h) return;
  RECT rc;
  if (!GetClientRect(platform->hwnd, &rc)) return;
  *out_w = rc.right - rc.left;
  *out_h = rc.bottom - rc.top;
}

NV_INTERNAL void nv_window_platform_get_position(nv_window_t* window, int* out_x, int* out_y) {
  nv_win_window_platform_t* platform = nv_win_get_window_platform(window);
  if (!platform || !platform->hwnd || !out_x || !out_y) return;
  RECT rc;
  if (!GetWindowRect(platform->hwnd, &rc)) return;
  *out_x = rc.left;
  *out_y = rc.top;
}

NV_INTERNAL void nv_window_platform_center(nv_window_t* window) {
  nv_win_window_platform_t* platform = nv_win_get_window_platform(window);
  if (!platform || !platform->hwnd) return;
  RECT rc;
  if (!GetWindowRect(platform->hwnd, &rc)) return;
  int w = rc.right - rc.left, h = rc.bottom - rc.top;
  int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
  int x = (sw - w) / 2, y = (sh - h) / 2;
  SetWindowPos(platform->hwnd, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

NV_INTERNAL void nv_window_platform_minimize(nv_window_t* window) {
  nv_win_window_platform_t* platform = nv_win_get_window_platform(window);
  if (platform && platform->hwnd) ShowWindow(platform->hwnd, SW_MINIMIZE);
}

NV_INTERNAL void nv_window_platform_maximize(nv_window_t* window) {
  nv_win_window_platform_t* platform = nv_win_get_window_platform(window);
  if (platform && platform->hwnd) ShowWindow(platform->hwnd, SW_MAXIMIZE);
}

NV_INTERNAL void nv_window_platform_restore(nv_window_t* window) {
  nv_win_window_platform_t* platform = nv_win_get_window_platform(window);
  if (platform && platform->hwnd) ShowWindow(platform->hwnd, SW_RESTORE);
}

NV_INTERNAL void nv_window_platform_fullscreen(nv_window_t* window, int enable) {
  nv_win_window_platform_t* platform = nv_win_get_window_platform(window);
  if (!platform || !platform->hwnd) return;
  if (enable) {
    SetWindowLongW(platform->hwnd, GWL_STYLE,
                  GetWindowLongW(platform->hwnd, GWL_STYLE) & ~(WS_CAPTION | WS_THICKFRAME));
    SetWindowPos(platform->hwnd, HWND_TOP, 0, 0,
                 GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
                 SWP_FRAMECHANGED);
  } else {
    SetWindowLongW(platform->hwnd, GWL_STYLE,
                  GetWindowLongW(platform->hwnd, GWL_STYLE) | WS_CAPTION | WS_THICKFRAME);
    SetWindowPos(platform->hwnd, NULL, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);
  }
}

NV_INTERNAL int nv_window_platform_is_fullscreen(nv_window_t* window) {
  nv_win_window_platform_t* platform = nv_win_get_window_platform(window);
  if (!platform || !platform->hwnd) return 0;
  return (GetWindowLongW(platform->hwnd, GWL_STYLE) & WS_CAPTION) ? 0 : 1;
}

NV_INTERNAL void nv_window_platform_focus(nv_window_t* window) {
  nv_win_window_platform_t* platform = nv_win_get_window_platform(window);
  if (platform && platform->hwnd) SetForegroundWindow(platform->hwnd);
}

NV_INTERNAL int nv_window_platform_is_focused(nv_window_t* window) {
  nv_win_window_platform_t* platform = nv_win_get_window_platform(window);
  if (!platform || !platform->hwnd) return 0;
  return (GetForegroundWindow() == platform->hwnd) ? 1 : 0;
}

NV_INTERNAL void nv_window_platform_set_resizable(nv_window_t* window, int enable) {
  nv_win_window_platform_t* platform = nv_win_get_window_platform(window);
  if (!platform || !platform->hwnd) return;
  LONG style = GetWindowLongW(platform->hwnd, GWL_STYLE);
  if (enable)
    style |= WS_MAXIMIZEBOX | WS_THICKFRAME;
  else
    style &= ~(WS_MAXIMIZEBOX | WS_THICKFRAME);
  SetWindowLongW(platform->hwnd, GWL_STYLE, style);
}

NV_INTERNAL void nv_window_platform_set_always_on_top(nv_window_t* window, int enable) {
  nv_win_window_platform_t* platform = nv_win_get_window_platform(window);
  if (!platform || !platform->hwnd) return;
  SetWindowPos(platform->hwnd, enable ? HWND_TOPMOST : HWND_NOTOPMOST,
               0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

NV_INTERNAL void nv_window_platform_set_opacity(nv_window_t* window, int opacity_pct) {
  nv_win_window_platform_t* platform = nv_win_get_window_platform(window);
  if (!platform || !platform->hwnd) return;
  SetWindowLongW(platform->hwnd, GWL_EXSTYLE,
                GetWindowLongW(platform->hwnd, GWL_EXSTYLE) | WS_EX_LAYERED);
  BYTE alpha = (opacity_pct <= 0) ? 0 : (opacity_pct >= 100) ? 255 : (BYTE)(opacity_pct * 255 / 100);
  SetLayeredWindowAttributes(platform->hwnd, 0, alpha, LWA_ALPHA);
}

NV_INTERNAL void nv_window_platform_set_zoom_factor(nv_window_t* window, double factor) {
  nv_win_window_platform_t* platform = nv_win_get_window_platform(window);
  if (!platform || !platform->controller) return;
  if (factor < 0.25) factor = 0.25;
  if (factor > 5.0) factor = 5.0;
  if (GetCurrentThreadId() == platform->main_thread_id) {
    platform->controller->lpVtbl->put_ZoomFactor(platform->controller, factor);
    return;
  }
  double* copy = (double*)HeapAlloc(GetProcessHeap(), 0, sizeof(double));
  if (!copy) return;
  *copy = factor;
  PostMessageW(platform->hwnd, WM_NV_SET_ZOOM, 0, (LPARAM)(void*)copy);
}

NV_INTERNAL void nv_window_platform_close(nv_window_t* window) {
  nv_win_window_platform_t* platform = nv_win_get_window_platform(window);
  if (platform && platform->hwnd) PostMessageW(platform->hwnd, WM_CLOSE, 0, 0);
}

/* WebView2 NavigateToString is limited (~2MiB UTF-16); large embedded SPAs hang or never paint. */
#define NV_WIN_NAVIGATE_TO_STRING_UTF8_MAX (1536u * 1024u)

static int nv_win_navigate_html_via_temp_file(ICoreWebView2* webview, const char* html) {
  if (!webview || !html) return 0;
  WCHAR tmp_dir[MAX_PATH];
  DWORD tdl = GetTempPathW((DWORD)(sizeof(tmp_dir) / sizeof(tmp_dir[0])), tmp_dir);
  if (tdl == 0 || tdl >= sizeof(tmp_dir) / sizeof(WCHAR)) return 0;
  WCHAR tmp_path[MAX_PATH];
  if (GetTempFileNameW(tmp_dir, L"nvu", 0, tmp_path) == 0) return 0;

  FILE* fp = _wfopen(tmp_path, L"wb");
  if (!fp) {
    DeleteFileW(tmp_path);
    return 0;
  }
  size_t n = strlen(html);
  if (fwrite(html, 1, n, fp) != n) {
    fclose(fp);
    DeleteFileW(tmp_path);
    return 0;
  }
  fclose(fp);

  /* GetTempFileNameW uses a ".tmp" suffix; WebView2/file URL treats that as text/plain
   * and shows raw markup. Use ".html" so the document is parsed as HTML. */
  const WCHAR* path_for_url = tmp_path;
  WCHAR html_path[MAX_PATH];
  WCHAR* dot = wcsrchr(tmp_path, L'.');
  if (dot && _wcsicmp(dot, L".tmp") == 0) {
    size_t prefix = (size_t)(dot - tmp_path);
    if (prefix + (sizeof(L".html") / sizeof(WCHAR)) <= MAX_PATH) {
      wmemcpy(html_path, tmp_path, prefix);
      html_path[prefix] = L'\0';
      wcscat(html_path, L".html");
      if (MoveFileExW(tmp_path, html_path, MOVEFILE_REPLACE_EXISTING))
        path_for_url = html_path;
    }
  }

  WCHAR full[MAX_PATH];
  DWORD gpl = GetFullPathNameW(path_for_url, (DWORD)(sizeof(full) / sizeof(WCHAR)), full, NULL);
  if (gpl == 0 || gpl >= sizeof(full) / sizeof(WCHAR)) {
    DeleteFileW(path_for_url);
    return 0;
  }
  size_t path_chars = wcslen(full);
  if (path_chars + 9u > 4096u) {
    DeleteFileW(tmp_path);
    return 0;
  }
  WCHAR url[4096];
  url[0] = L'f';
  url[1] = L'i';
  url[2] = L'l';
  url[3] = L'e';
  url[4] = L':';
  url[5] = L'/';
  url[6] = L'/';
  url[7] = L'/';
  size_t j = 8;
  for (size_t i = 0; i < path_chars; i++) {
    WCHAR c = full[i];
    url[j++] = (c == L'\\') ? L'/' : c;
  }
  url[j] = L'\0';

  HRESULT hr = webview->lpVtbl->Navigate(webview, url);
  if (FAILED(hr)) {
    nv_win_log_hresult("Navigate(file temp html)", hr);
    DeleteFileW(tmp_path);
    return 0;
  }
  /* Leave temp file on disk until process exit (Navigate may still read it). */
  return 1;
}

NV_INTERNAL void nv_window_platform_load_html(nv_window_t* window, const char* html, const char* base_url) {
  nv_win_window_platform_t* platform = nv_win_get_window_platform(window);
  if (!platform || !platform->webview) return;
  (void)base_url;
  size_t blen = strlen(html);
  if (blen > NV_WIN_NAVIGATE_TO_STRING_UTF8_MAX) {
    if (nv_win_navigate_html_via_temp_file(platform->webview, html)) return;
  }
  WCHAR* whtml = NULL;
  int len = (int)(blen > (size_t)INT_MAX ? (size_t)INT_MAX : blen);
  int wlen = MultiByteToWideChar(CP_UTF8, 0, html, len, NULL, 0);
  if (wlen > 0) {
    whtml = (WCHAR*)HeapAlloc(GetProcessHeap(), 0, (size_t)(wlen + 1) * sizeof(WCHAR));
    if (whtml) {
      MultiByteToWideChar(CP_UTF8, 0, html, len, whtml, wlen);
      whtml[wlen] = L'\0';
      platform->webview->lpVtbl->NavigateToString(platform->webview, whtml);
      HeapFree(GetProcessHeap(), 0, whtml);
    }
  }
}

/* Load file:// URL. Uses --allow-file-access-from-files from environment options. */
static void nv_win_load_file_url(ICoreWebView2* webview, const char* file_url) {
  static const char prefix[] = "file:///";
  if (strncmp(file_url, prefix, sizeof(prefix) - 1) != 0) return;
  const char* path = file_url + sizeof(prefix) - 1;
  WCHAR wpath[4096];
  int wlen = MultiByteToWideChar(CP_UTF8, 0, path, -1, NULL, 0);
  if (wlen <= 0 || wlen > (int)(sizeof(wpath) / sizeof(WCHAR))) return;
  MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, wlen);
  for (int i = 0; wpath[i]; i++)
    if (wpath[i] == L'/') wpath[i] = L'\\';
  WCHAR fullPath[4096];
  if (GetFullPathNameW(wpath, (DWORD)(sizeof(fullPath) / sizeof(WCHAR)), fullPath, NULL) == 0)
    return;
  size_t n = 0;
  while (fullPath[n]) n++;
  WCHAR url[4096];
  if (n + 9 > 4096) return;
  url[0] = L'f'; url[1] = L'i'; url[2] = L'l'; url[3] = L'e'; url[4] = L':';
  url[5] = L'/'; url[6] = L'/'; url[7] = L'/';
  for (size_t i = 0; i < n; i++)
    url[8 + i] = (fullPath[i] == L'\\') ? L'/' : fullPath[i];
  url[8 + n] = L'\0';
  webview->lpVtbl->Navigate(webview, url);
}

NV_INTERNAL void nv_window_platform_load_url(nv_window_t* window, const char* url) {
  nv_win_window_platform_t* platform = nv_win_get_window_platform(window);
  if (!platform || !platform->webview || !url) return;
  if (strncmp(url, "file:///", 8) == 0) {
    nv_win_load_file_url(platform->webview, url);
    return;
  }
  WCHAR* wurl = NULL;
  int len = (int)strlen(url);
  int wlen = MultiByteToWideChar(CP_UTF8, 0, url, len, NULL, 0);
  if (wlen > 0) {
    wurl = (WCHAR*)HeapAlloc(GetProcessHeap(), 0, (size_t)(wlen + 1) * sizeof(WCHAR));
    if (wurl) {
      MultiByteToWideChar(CP_UTF8, 0, url, len, wurl, wlen);
      wurl[wlen] = L'\0';
      platform->webview->lpVtbl->Navigate(platform->webview, wurl);
      HeapFree(GetProcessHeap(), 0, wurl);
    }
  }
}

NV_INTERNAL void nv_window_platform_eval_js(nv_window_t* window, const char* js) {
  nv_win_window_platform_t* platform = nv_win_get_window_platform(window);
  if (!platform || !platform->webview || !js || !platform->hwnd) return;

  /* WebView2 ExecuteScript must run on the UI thread that created the WebView.
   * Elkotobi calls nv_send from worker threads (ready_task, global_search_task, etc.).
   * Marshal to main thread via PostMessage. */
  if (GetCurrentThreadId() != platform->main_thread_id) {
    size_t len = strlen(js) + 1;
    char* copy = (char*)HeapAlloc(GetProcessHeap(), 0, len);
    if (!copy) return;
    memcpy(copy, js, len);
    PostMessageW(platform->hwnd, WM_NV_EVAL_JS, 0, (LPARAM)(void*)copy);
    return;
  }

  int len = (int)strlen(js);
  int wlen = MultiByteToWideChar(CP_UTF8, 0, js, len, NULL, 0);
  if (wlen <= 0) return;
  WCHAR* wjs = (WCHAR*)HeapAlloc(GetProcessHeap(), 0, (size_t)(wlen + 1) * sizeof(WCHAR));
  if (!wjs) return;
  MultiByteToWideChar(CP_UTF8, 0, js, len, wjs, wlen);
  wjs[wlen] = L'\0';
  platform->webview->lpVtbl->ExecuteScript(platform->webview, wjs, NULL);
  HeapFree(GetProcessHeap(), 0, wjs);
}

/* =============================================================================
 * Clipboard (CF_UNICODETEXT ↔ UTF-8)
 *
 * read_text returns a malloc'd C string; caller frees after copying to arena.
 * On GCC/Clang the definitions are weak so unit tests can override with stubs.
 * =============================================================================
 */

#if defined(__GNUC__) || defined(__clang__)
#define NV_WIN_CLIP_ATTR __attribute__((weak))
#else
#define NV_WIN_CLIP_ATTR
#endif

NV_INTERNAL NV_WIN_CLIP_ATTR char* nv_win_clipboard_read_text(void) {
  if (!OpenClipboard(NULL)) return NULL;
  HANDLE h = GetClipboardData(CF_UNICODETEXT);
  if (!h) {
    CloseClipboard();
    return NULL;
  }
  WCHAR* w = (WCHAR*)GlobalLock(h);
  if (!w) {
    CloseClipboard();
    return NULL;
  }
  int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, NULL, 0, NULL, NULL);
  if (n <= 0) {
    GlobalUnlock(h);
    CloseClipboard();
    return NULL;
  }
  char* out = (char*)malloc((size_t)n);
  if (!out) {
    GlobalUnlock(h);
    CloseClipboard();
    return NULL;
  }
  if (WideCharToMultiByte(CP_UTF8, 0, w, -1, out, n, NULL, NULL) <= 0) {
    GlobalUnlock(h);
    CloseClipboard();
    free(out);
    return NULL;
  }
  GlobalUnlock(h);
  CloseClipboard();
  return out;
}

NV_INTERNAL NV_WIN_CLIP_ATTR int nv_win_clipboard_write_text(const char* utf8) {
  if (!utf8) return -1;
  if (!OpenClipboard(NULL)) return -1;
  if (!EmptyClipboard()) {
    CloseClipboard();
    return -1;
  }
  int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
  if (wlen <= 0) {
    CloseClipboard();
    return -1;
  }
  size_t bytes = (size_t)wlen * sizeof(WCHAR);
  HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, bytes);
  if (!h) {
    CloseClipboard();
    return -1;
  }
  WCHAR* pw = (WCHAR*)GlobalLock(h);
  if (!pw) {
    GlobalFree(h);
    CloseClipboard();
    return -1;
  }
  if (MultiByteToWideChar(CP_UTF8, 0, utf8, -1, pw, wlen) <= 0) {
    GlobalUnlock(h);
    GlobalFree(h);
    CloseClipboard();
    return -1;
  }
  GlobalUnlock(h);
  if (!SetClipboardData(CF_UNICODETEXT, h)) {
    GlobalFree(h);
    CloseClipboard();
    return -1;
  }
  CloseClipboard();
  return 0;
}

NV_INTERNAL NV_WIN_CLIP_ATTR void nv_win_clipboard_clear(void) {
  if (!OpenClipboard(NULL)) return;
  EmptyClipboard();
  CloseClipboard();
}

NV_INTERNAL NV_WIN_CLIP_ATTR int nv_win_clipboard_has_text(void) {
  return IsClipboardFormatAvailable(CF_UNICODETEXT) ? 1 : 0;
}

/* =============================================================================
 * Shell (open URL/path, reveal in Explorer)
 * =============================================================================
 */

static int nv_win_shell_utf8_to_wpath(const char* utf8, WCHAR* out, int out_wchars) {
  if (!utf8 || !out || out_wchars < 2) return -1;
  int n = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
  if (n <= 0 || n > out_wchars) return -1;
  if (MultiByteToWideChar(CP_UTF8, 0, utf8, -1, out, n) <= 0) return -1;
  return 0;
}

static int nv_win_shell_execute_open(const WCHAR* wpath) {
  if (!wpath) return -1;
  HINSTANCE hi = ShellExecuteW(NULL, L"open", wpath, NULL, NULL, SW_SHOWNORMAL);
  return ((INT_PTR)hi > 32) ? 0 : -1;
}

NV_INTERNAL int nv_win_shell_open_url(const char* url) {
  if (!url) return -1;
  WCHAR wpath[MAX_PATH];
  if (nv_win_shell_utf8_to_wpath(url, wpath, MAX_PATH) != 0) return -1;
  return nv_win_shell_execute_open(wpath);
}

NV_INTERNAL int nv_win_shell_open_path(const char* path) {
  if (!path) return -1;
  WCHAR wpath[MAX_PATH];
  if (nv_win_shell_utf8_to_wpath(path, wpath, MAX_PATH) != 0) return -1;
  return nv_win_shell_execute_open(wpath);
}

static int nv_win_shell_show_in_folder_via_sh(const WCHAR* wpath) {
  PIDLIST_ABSOLUTE pidlFull = ILCreateFromPathW(wpath);
  if (!pidlFull) return -1;

  DWORD attr = GetFileAttributesW(wpath);
  if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
    HRESULT hr = SHOpenFolderAndSelectItems(pidlFull, 0, NULL, 0);
    ILFree(pidlFull);
    return SUCCEEDED(hr) ? 0 : -1;
  }

  PIDLIST_ABSOLUTE pidlFolder = ILCloneFull(pidlFull);
  if (!pidlFolder) {
    ILFree(pidlFull);
    return -1;
  }
  if (!ILRemoveLastID(pidlFolder)) {
    ILFree(pidlFolder);
    ILFree(pidlFull);
    return -1;
  }

  PCUITEMID_CHILD pidlChild = ILFindLastID(pidlFull);
  PCUITEMID_CHILD kids[1] = {pidlChild};
  HRESULT hr = SHOpenFolderAndSelectItems(pidlFolder, 1, kids, 0);
  ILFree(pidlFolder);
  ILFree(pidlFull);
  return SUCCEEDED(hr) ? 0 : -1;
}

static int nv_win_shell_open_parent_folder(const WCHAR* wpath) {
  WCHAR dir[MAX_PATH];
  if (!wpath) return -1;
  if (wcslen(wpath) >= (size_t)MAX_PATH) return -1;
  wmemcpy(dir, wpath, MAX_PATH);
  dir[MAX_PATH - 1] = L'\0';
  WCHAR* sep = wcsrchr(dir, L'\\');
  if (!sep) sep = wcsrchr(dir, L'/');
  if (!sep || sep == dir) return -1;
  *sep = L'\0';
  return nv_win_shell_execute_open(dir);
}

NV_INTERNAL int nv_win_shell_show_in_folder(const char* path) {
  if (!path) return -1;
  WCHAR wpath[MAX_PATH];
  if (nv_win_shell_utf8_to_wpath(path, wpath, MAX_PATH) != 0) return -1;

  if (nv_win_shell_show_in_folder_via_sh(wpath) == 0) return 0;
  return nv_win_shell_open_parent_folder(wpath);
}

/* =============================================================================
 * Shell exec (cmd.exe /c, captured stdout/stderr)
 * =============================================================================
 */

enum { nv_win_shell_capture_cap = 1048576 };

static int nv_win_read_handle_to_buffer(HANDLE h, char** out_buf, int* was_trunc) {
  size_t len = 0;
  size_t cap = 4096;
  char* buf = (char*)malloc(cap);
  int truncated = 0;
  if (!buf) return -1;
  for (;;) {
    char temp[8192];
    DWORD nr = 0;
    if (!ReadFile(h, temp, sizeof temp, &nr, NULL)) {
      DWORD err = GetLastError();
      if (err == ERROR_BROKEN_PIPE) break;
      free(buf);
      return -1;
    }
    if (nr == 0) break;
    if (!truncated) {
      size_t room = len < (size_t)nv_win_shell_capture_cap
                        ? (size_t)(nv_win_shell_capture_cap - len)
                        : 0;
      size_t chunk = (size_t)nr;
      size_t take = chunk < room ? chunk : room;
      if (take > 0) {
        if (len + take > cap) {
          size_t ncap = cap;
          while (len + take > ncap) ncap *= 2;
          char* nb = (char*)realloc(buf, ncap);
          if (!nb) {
            free(buf);
            return -1;
          }
          buf = nb;
          cap = ncap;
        }
        memcpy(buf + len, temp, take);
        len += take;
      }
      if (chunk > take) truncated = 1;
    }
  }
  {
    char* fin = (char*)realloc(buf, len + 1);
    if (!fin) {
      free(buf);
      return -1;
    }
    fin[len] = '\0';
    buf = fin;
  }
  *out_buf = buf;
  *was_trunc = truncated;
  return 0;
}

typedef struct {
  HANDLE h;
  char** out;
  int* trunc;
  volatile LONG failed;
} nv_win_pipe_reader_ctx;

static DWORD WINAPI nv_win_pipe_reader_thread(LPVOID p) {
  nv_win_pipe_reader_ctx* c = (nv_win_pipe_reader_ctx*)p;
  if (nv_win_read_handle_to_buffer(c->h, c->out, c->trunc) != 0) {
    InterlockedExchange(&c->failed, 1);
  }
  return 0;
}

NV_INTERNAL nv_shell_result_t* nv_win_shell_exec(const char* cmd) {
  SECURITY_ATTRIBUTES sa;
  HANDLE rd_out = INVALID_HANDLE_VALUE;
  HANDLE wr_out = INVALID_HANDLE_VALUE;
  HANDLE rd_err = INVALID_HANDLE_VALUE;
  HANDLE wr_err = INVALID_HANDLE_VALUE;
  HANDLE rd_in = INVALID_HANDLE_VALUE;
  HANDLE wr_in = INVALID_HANDLE_VALUE;
  PROCESS_INFORMATION pi;
  STARTUPINFOW si;
  WCHAR cmdline[32768];
  nv_shell_result_t* res = NULL;
  int out_trunc = 0;
  int err_trunc = 0;
  DWORD exit_code = 1;
  HANDLE err_thr = NULL;
  nv_win_pipe_reader_ctx err_ctx;

  ZeroMemory(&sa, sizeof(sa));
  sa.nLength = sizeof(sa);
  sa.bInheritHandle = TRUE;
  sa.lpSecurityDescriptor = NULL;
  ZeroMemory(&pi, sizeof(pi));
  ZeroMemory(&si, sizeof(si));
  si.cb = sizeof(si);
  ZeroMemory(&err_ctx, sizeof(err_ctx));

  if (!cmd) return NULL;

  if (!CreatePipe(&rd_out, &wr_out, &sa, 0)) return NULL;
  if (!SetHandleInformation(rd_out, HANDLE_FLAG_INHERIT, 0)) goto fail;
  if (!CreatePipe(&rd_err, &wr_err, &sa, 0)) goto fail;
  if (!SetHandleInformation(rd_err, HANDLE_FLAG_INHERIT, 0)) goto fail;
  if (!CreatePipe(&rd_in, &wr_in, &sa, 0)) goto fail;
  if (!SetHandleInformation(wr_in, HANDLE_FLAG_INHERIT, 0)) goto fail;

  {
    const WCHAR prefix[] = L"cmd.exe /c ";
    size_t plen = (sizeof(prefix) / sizeof(WCHAR)) - 1;
    if (plen >= sizeof(cmdline) / sizeof(WCHAR)) goto fail;
    wmemcpy(cmdline, prefix, plen + 1);
    if (MultiByteToWideChar(CP_UTF8, 0, cmd, -1, cmdline + plen,
                            (int)(sizeof(cmdline) / sizeof(WCHAR) - plen)) <= 0) {
      goto fail;
    }
  }

  si.dwFlags = STARTF_USESTDHANDLES;
  si.hStdInput = rd_in;
  si.hStdOutput = wr_out;
  si.hStdError = wr_err;

  if (!CreateProcessW(NULL, cmdline, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
    goto fail;
  }

  CloseHandle(wr_out);
  wr_out = INVALID_HANDLE_VALUE;
  CloseHandle(wr_err);
  wr_err = INVALID_HANDLE_VALUE;
  CloseHandle(wr_in);
  wr_in = INVALID_HANDLE_VALUE;
  CloseHandle(pi.hThread);
  pi.hThread = NULL;

  CloseHandle(rd_in);
  rd_in = INVALID_HANDLE_VALUE;

  res = (nv_shell_result_t*)calloc(1, sizeof(nv_shell_result_t));
  if (!res) goto fail_after_proc;

  err_ctx.h = rd_err;
  err_ctx.out = &res->err;
  err_ctx.trunc = &err_trunc;
  err_ctx.failed = 0;
  err_thr = CreateThread(NULL, 0, nv_win_pipe_reader_thread, &err_ctx, 0, NULL);
  if (!err_thr) goto fail_after_proc;

  {
    int stdout_fail =
        (nv_win_read_handle_to_buffer(rd_out, &res->out, &out_trunc) != 0) ? 1 : 0;
    WaitForSingleObject(err_thr, INFINITE);
    CloseHandle(err_thr);
    err_thr = NULL;
    if (stdout_fail || err_ctx.failed) goto fail_after_proc;
  }

  CloseHandle(rd_out);
  rd_out = INVALID_HANDLE_VALUE;
  CloseHandle(rd_err);
  rd_err = INVALID_HANDLE_VALUE;

  WaitForSingleObject(pi.hProcess, INFINITE);
  if (!GetExitCodeProcess(pi.hProcess, &exit_code)) {
    goto fail_after_proc;
  }
  CloseHandle(pi.hProcess);
  pi.hProcess = NULL;

  res->exit_code = (int)exit_code;
  res->truncated = (out_trunc || err_trunc) ? 1 : 0;
  return res;

fail_after_proc:
  if (err_thr && err_thr != INVALID_HANDLE_VALUE) {
    WaitForSingleObject(err_thr, INFINITE);
    CloseHandle(err_thr);
    err_thr = NULL;
  }
  if (res) {
    free(res->out);
    free(res->err);
    free(res);
    res = NULL;
  }
  if (pi.hProcess && pi.hProcess != INVALID_HANDLE_VALUE) {
    TerminateProcess(pi.hProcess, 1);
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    pi.hProcess = NULL;
  }
  if (pi.hThread && pi.hThread != INVALID_HANDLE_VALUE) CloseHandle(pi.hThread);

fail:
  if (err_thr && err_thr != INVALID_HANDLE_VALUE) {
    CloseHandle(err_thr);
  }
  if (rd_out != INVALID_HANDLE_VALUE) CloseHandle(rd_out);
  if (wr_out != INVALID_HANDLE_VALUE) CloseHandle(wr_out);
  if (rd_err != INVALID_HANDLE_VALUE) CloseHandle(rd_err);
  if (wr_err != INVALID_HANDLE_VALUE) CloseHandle(wr_err);
  if (rd_in != INVALID_HANDLE_VALUE) CloseHandle(rd_in);
  if (wr_in != INVALID_HANDLE_VALUE) CloseHandle(wr_in);
  if (res) {
    free(res->out);
    free(res->err);
    free(res);
  }
  return NULL;
}

/* =============================================================================
 * Dialog helpers (owner HWND for IFileDialog / MessageBox)
 * =============================================================================
 */

NV_INTERNAL HWND nv_win_dialog_owner_hwnd(nv_window_t* window) {
  nv_win_window_platform_t* platform = nv_win_get_window_platform(window);
  return (platform && platform->hwnd) ? platform->hwnd : NULL;
}

/* =============================================================================
 * App paths & locale (UTF-8 via malloc; caller frees)
 * Weak on GCC/Clang so nv-ops tests can override with stubs.
 * =============================================================================
 */

#if defined(__GNUC__) || defined(__clang__)
#define NV_WIN_APP_PATH_ATTR __attribute__((weak))
#else
#define NV_WIN_APP_PATH_ATTR
#endif

static char* nv_win_wide_to_utf8_malloc(const WCHAR* wstr) {
  if (!wstr) return NULL;
  int n = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL);
  if (n <= 0) return NULL;
  char* out = (char*)malloc((size_t)n);
  if (!out) return NULL;
  if (WideCharToMultiByte(CP_UTF8, 0, wstr, -1, out, n, NULL, NULL) <= 0) {
    free(out);
    return NULL;
  }
  return out;
}

NV_INTERNAL NV_WIN_APP_PATH_ATTR char* nv_win_get_exe_path(void) {
  WCHAR buf[MAX_PATH];
  DWORD n = GetModuleFileNameW(NULL, buf, MAX_PATH);
  if (n == 0 || n >= MAX_PATH) return NULL;
  buf[MAX_PATH - 1] = L'\0';
  return nv_win_wide_to_utf8_malloc(buf);
}

NV_INTERNAL NV_WIN_APP_PATH_ATTR char* nv_win_get_resource_dir(void) {
  WCHAR buf[MAX_PATH];
  DWORD n = GetModuleFileNameW(NULL, buf, MAX_PATH);
  if (n == 0 || n >= MAX_PATH) return NULL;
  buf[MAX_PATH - 1] = L'\0';
  WCHAR* sep = wcsrchr(buf, L'\\');
  if (!sep) sep = wcsrchr(buf, L'/');
  if (sep && sep > buf) {
    *sep = L'\0';
  } else if (!sep) {
    buf[0] = L'.';
    buf[1] = L'\0';
  }
  return nv_win_wide_to_utf8_malloc(buf);
}

NV_INTERNAL NV_WIN_APP_PATH_ATTR char* nv_win_get_data_dir(void) {
  WCHAR appdata[MAX_PATH];
  HRESULT hr = SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, appdata);
  if (!SUCCEEDED(hr)) return NULL;
  appdata[MAX_PATH - 1] = L'\0';
  size_t la = wcslen(appdata);
  static const WCHAR suffix[] = L"\\nativeview";
  const size_t suflen = (sizeof(suffix) / sizeof(suffix[0])) - 1u;
  if (la == 0 || la + suflen >= (size_t)MAX_PATH) return NULL;
  WCHAR full[MAX_PATH];
  wmemcpy(full, appdata, la);
  wmemcpy(full + la, suffix, suflen + 1u);
  (void)CreateDirectoryW(full, NULL);
  return nv_win_wide_to_utf8_malloc(full);
}

NV_INTERNAL NV_WIN_APP_PATH_ATTR char* nv_win_get_locale(void) {
  WCHAR buf[LOCALE_NAME_MAX_LENGTH];
  if (GetUserDefaultLocaleName(buf, LOCALE_NAME_MAX_LENGTH) <= 0) return NULL;
  buf[LOCALE_NAME_MAX_LENGTH - 1] = L'\0';
  return nv_win_wide_to_utf8_malloc(buf);
}

/* =============================================================================
 * Screen (malloc'd UTF-8 JSON for nv_op_screen)
 * =============================================================================
 */

typedef HRESULT(WINAPI* nv_win_GetDpiForMonitor_fn)(HMONITOR hmon, int dpiType, UINT* dx, UINT* dy);

typedef struct {
  char* data;
  size_t len;
  size_t cap;
} nv_win_jbuf_t;

static int nv_win_jbuf_grow(nv_win_jbuf_t* b, size_t extra) {
  size_t need = b->len + extra + 1;
  if (b->data && need <= b->cap) return 0;
  size_t ncap = b->cap ? b->cap : 1024u;
  while (ncap < need) {
    if (ncap > ((size_t)1 << 30)) return -1;
    ncap *= 2u;
  }
  {
    char* n = (char*)realloc(b->data, ncap);
    if (!n) return -1;
    b->data = n;
    b->cap = ncap;
  }
  return 0;
}

static int nv_win_jbuf_cstr(nv_win_jbuf_t* b, const char* s) {
  size_t n;
  if (!s) s = "";
  n = strlen(s);
  if (nv_win_jbuf_grow(b, n) != 0) return -1;
  memcpy(b->data + b->len, s, n);
  b->len += n;
  b->data[b->len] = '\0';
  return 0;
}

static int nv_win_jbuf_append_fmt(nv_win_jbuf_t* b, const char* fmt, ...) {
  char tmp[2048];
  va_list ap;
  int n;
  va_start(ap, fmt);
  n = vsnprintf(tmp, sizeof(tmp), fmt, ap);
  va_end(ap);
  if (n < 0 || (size_t)n >= sizeof(tmp)) return -1;
  return nv_win_jbuf_cstr(b, tmp);
}

static int nv_win_jbuf_append_json_string_wide(nv_win_jbuf_t* b, const WCHAR* w) {
  int n8;
  char* u8;
  const unsigned char* p;
  if (!w) w = L"";
  n8 = WideCharToMultiByte(CP_UTF8, 0, w, -1, NULL, 0, NULL, NULL);
  if (n8 <= 0) return nv_win_jbuf_cstr(b, "\"\"");
  u8 = (char*)malloc((size_t)n8);
  if (!u8) return -1;
  if (WideCharToMultiByte(CP_UTF8, 0, w, -1, u8, n8, NULL, NULL) <= 0) {
    free(u8);
    return -1;
  }
  if (nv_win_jbuf_cstr(b, "\"") != 0) {
    free(u8);
    return -1;
  }
  for (p = (const unsigned char*)u8; *p;) {
    unsigned char c = *p;
    if (c == '"' || c == '\\') {
      char esc[3] = {'\\', (char)c, '\0'};
      if (nv_win_jbuf_cstr(b, esc) != 0) {
        free(u8);
        return -1;
      }
      p++;
    } else if (c < 32u) {
      char tmp[8];
      (void)snprintf(tmp, sizeof(tmp), "\\u%04x", (unsigned)c);
      if (nv_win_jbuf_cstr(b, tmp) != 0) {
        free(u8);
        return -1;
      }
      p++;
    } else if (c < 128u) {
      char ch[2] = {(char)c, '\0'};
      if (nv_win_jbuf_cstr(b, ch) != 0) {
        free(u8);
        return -1;
      }
      p++;
    } else {
      int len = 1;
      if ((c & 0xE0u) == 0xC0u)
        len = 2;
      else if ((c & 0xF0u) == 0xE0u)
        len = 3;
      else if ((c & 0xF8u) == 0xF0u)
        len = 4;
      if (nv_win_jbuf_grow(b, (size_t)len) != 0) {
        free(u8);
        return -1;
      }
      memcpy(b->data + b->len, p, (size_t)len);
      b->len += (size_t)len;
      b->data[b->len] = '\0';
      p += (size_t)len;
    }
  }
  free(u8);
  return nv_win_jbuf_cstr(b, "\"");
}

static nv_win_GetDpiForMonitor_fn nv_win_resolve_GetDpiForMonitor(void) {
  static int resolved;
  static nv_win_GetDpiForMonitor_fn fn;
  if (!resolved) {
    HMODULE sh = LoadLibraryW(L"shcore.dll");
    fn = sh ? (nv_win_GetDpiForMonitor_fn)(void*)GetProcAddress(sh, "GetDpiForMonitor") : NULL;
    resolved = 1;
  }
  return fn;
}

static double nv_win_monitor_scale(HMONITOR mon, const MONITORINFOEXW* mi, nv_win_GetDpiForMonitor_fn getDpi) {
  if (getDpi && mon) {
    UINT dx = 96u, dy = 96u;
    if (SUCCEEDED(getDpi(mon, 0 /* MDT_EFFECTIVE_DPI */, &dx, &dy)) && dx > 0u)
      return (double)dx / 96.0;
  }
  if (mi) {
    HDC hdc = CreateDCW(L"DISPLAY", mi->szDevice, NULL, NULL);
    if (hdc) {
      int dpi = GetDeviceCaps(hdc, LOGPIXELSX);
      (void)DeleteDC(hdc);
      if (dpi > 0) return (double)dpi / 96.0;
    }
  }
  return 1.0;
}

static int nv_win_jbuf_append_display(nv_win_jbuf_t* b, int id, const RECT* bounds, const RECT* work,
                                      double scale, int isPrimary, const WCHAR* name) {
  int bx = (int)bounds->left;
  int by = (int)bounds->top;
  int bw = (int)(bounds->right - bounds->left);
  int bh = (int)(bounds->bottom - bounds->top);
  int wx = (int)work->left;
  int wy = (int)work->top;
  int ww = (int)(work->right - work->left);
  int wh = (int)(work->bottom - work->top);
  if (nv_win_jbuf_append_fmt(b, "{\"id\":%d,\"x\":%d,\"y\":%d,\"width\":%d,\"height\":%d,\"scaleFactor\":%.9g,", id, bx,
                             by, bw, bh, scale) != 0)
    return -1;
  if (nv_win_jbuf_append_fmt(b, "\"localizedName\":") != 0) return -1;
  if (nv_win_jbuf_append_json_string_wide(b, name) != 0) return -1;
  if (nv_win_jbuf_append_fmt(b, ",\"isPrimary\":%s,\"bounds\":{\"x\":%d,\"y\":%d,\"width\":%d,\"height\":%d},"
                             "\"workArea\":{\"x\":%d,\"y\":%d,\"width\":%d,\"height\":%d}}",
                             isPrimary ? "true" : "false", bx, by, bw, bh, wx, wy, ww, wh) != 0)
    return -1;
  return 0;
}

typedef struct {
  nv_win_jbuf_t* buf;
  nv_win_GetDpiForMonitor_fn getDpi;
  int next_id;
  int need_comma;
  int want_primary_only;
  int found_primary;
  int failed;
} nv_win_screen_enum_ctx_t;

static BOOL CALLBACK nv_win_screen_enum_proc(HMONITOR hmon, HDC hdc, LPRECT lprcMonitor, LPARAM dwData) {
  MONITORINFOEXW mi;
  nv_win_screen_enum_ctx_t* ctx = (nv_win_screen_enum_ctx_t*)dwData;
  double scale;
  int isPrimary;
  (void)hdc;
  (void)lprcMonitor;
  if (!ctx || !ctx->buf) return FALSE;
  ZeroMemory(&mi, sizeof(mi));
  mi.cbSize = sizeof(mi);
  if (!GetMonitorInfoW(hmon, (MONITORINFO*)&mi)) return TRUE;
  isPrimary = (mi.dwFlags & MONITORINFOF_PRIMARY) ? 1 : 0;
  if (ctx->want_primary_only) {
    if (!isPrimary) return TRUE;
    scale = nv_win_monitor_scale(hmon, &mi, ctx->getDpi);
    if (nv_win_jbuf_append_display(ctx->buf, 0, &mi.rcMonitor, &mi.rcWork, scale, 1, mi.szDevice) != 0) {
      ctx->failed = 1;
      return FALSE;
    }
    ctx->found_primary = 1;
    return FALSE;
  }
  scale = nv_win_monitor_scale(hmon, &mi, ctx->getDpi);
  if (ctx->need_comma) {
    if (nv_win_jbuf_cstr(ctx->buf, ",") != 0) {
      ctx->failed = 1;
      return FALSE;
    }
  }
  ctx->need_comma = 1;
  if (nv_win_jbuf_append_display(ctx->buf, ctx->next_id++, &mi.rcMonitor, &mi.rcWork, scale, isPrimary, mi.szDevice) !=
      0) {
    ctx->failed = 1;
    return FALSE;
  }
  return TRUE;
}

NV_INTERNAL char* nv_win_screen_get_all(void) {
  nv_win_jbuf_t buf;
  nv_win_screen_enum_ctx_t ctx;
  ZeroMemory(&buf, sizeof(buf));
  ZeroMemory(&ctx, sizeof(ctx));
  ctx.buf = &buf;
  ctx.getDpi = nv_win_resolve_GetDpiForMonitor();
  ctx.next_id = 0;
  ctx.need_comma = 0;
  ctx.want_primary_only = 0;
  if (nv_win_jbuf_cstr(&buf, "[") != 0) return NULL;
  if (!EnumDisplayMonitors(NULL, NULL, nv_win_screen_enum_proc, (LPARAM)&ctx)) {
    free(buf.data);
    return NULL;
  }
  if (ctx.failed) {
    free(buf.data);
    return NULL;
  }
  if (nv_win_jbuf_cstr(&buf, "]") != 0) {
    free(buf.data);
    return NULL;
  }
  return buf.data;
}

NV_INTERNAL char* nv_win_screen_get_primary(void) {
  nv_win_jbuf_t buf;
  nv_win_screen_enum_ctx_t ctx;
  ZeroMemory(&buf, sizeof(buf));
  ZeroMemory(&ctx, sizeof(ctx));
  ctx.buf = &buf;
  ctx.getDpi = nv_win_resolve_GetDpiForMonitor();
  ctx.want_primary_only = 1;
  if (!EnumDisplayMonitors(NULL, NULL, nv_win_screen_enum_proc, (LPARAM)&ctx) || !ctx.found_primary || ctx.failed) {
    free(buf.data);
    return NULL;
  }
  return buf.data;
}

NV_INTERNAL char* nv_win_screen_get_cursor(void) {
  nv_win_jbuf_t buf;
  POINT pt;
  ZeroMemory(&buf, sizeof(buf));
  if (!GetCursorPos(&pt)) return NULL;
  if (nv_win_jbuf_append_fmt(&buf, "{\"x\":%d,\"y\":%d}", (int)pt.x, (int)pt.y) != 0) {
    free(buf.data);
    return NULL;
  }
  return buf.data;
}

NV_INTERNAL void nv_win_window_set_context_menu(nv_window_t* w, const nv_menu_item_t* items, int count) {
  (void)items;
  (void)count;
  if (!w) return;
  nv_win_window_platform_t* p = nv_win_get_window_platform(w);
  if (!p || !p->webview) return;
  ICoreWebView2Settings* settings = NULL;
  HRESULT shr = p->webview->lpVtbl->get_Settings(p->webview, &settings);
  if (SUCCEEDED(shr) && settings) {
    int enable = nv_win_context_menu_enabled() || (w->ctx_menu_count > 0);
    settings->lpVtbl->put_AreDefaultContextMenusEnabled(settings, enable ? TRUE : FALSE);
    settings->lpVtbl->Release(settings);
  }
}

#endif /* _WIN32 */
