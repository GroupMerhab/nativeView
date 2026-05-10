/*
 * Copyright (c) 2026
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 */

#ifdef _WIN32

#ifndef COBJMACROS
#define COBJMACROS
#endif

#include <windows.h>
#include <initguid.h>
#include <objbase.h>
#include <propkey.h>
#include <propvarutil.h>
#include <roapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <shobjidl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <winstring.h>

#include "nv_core_internal.h"
#include "nv.h"
#include "nv_window_internal.h"

#if defined(_MSC_VER) || (defined(__has_include) && __has_include(<windows.ui.notifications.h>) && \
                          __has_include(<windows.data.xml.dom.h>))
#include <windows.data.xml.dom.h>
#include <windows.ui.notifications.h>
#define NV_WIN_TOAST_SDK 1
#else
#define NV_WIN_TOAST_SDK 0
#endif

#define NV_WIN_TOAST_AUMID L"Elkotobi.NativeView"
#define NV_WIN_TOAST_SHORTCUT_NAME L"NativeView.lnk"

#if defined(__GNUC__) || defined(__clang__)
#define NV_WIN_NOTIF_ATTR __attribute__((weak))
#else
#define NV_WIN_NOTIF_ATTR
#endif

static void nv_win_notif_log_hresult(const char* msg, HRESULT hr) {
  char* sys = NULL;
  FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                 NULL, hr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&sys, 0, NULL);
  if (sys) {
    NV_ERR("%s (0x%08lx): %s", msg, (unsigned long)hr, sys);
    LocalFree(sys);
  } else {
    NV_ERR("%s (0x%08lx)", msg, (unsigned long)hr);
  }
}

static int nv_win_notif_test_stub(void) {
  const char* e = getenv("NATIVEVIEW_STUB_NOTIFICATION");
  return (e && e[0] == '1' && e[1] == '\0') ? 1 : 0;
}

static void nv_win_notif_irelease(IUnknown* p) {
  if (p) (void)p->lpVtbl->Release(p);
}

static void nv_win_notif_esc_xml_utf8(const char* in, char* out, size_t cap) {
  size_t j = 0;
  const unsigned char* p = (const unsigned char*)(in ? in : "");
  while (*p && j + 8 < cap) {
    if (*p == '&') {
      memcpy(out + j, "&amp;", 5);
      j += 5;
    } else if (*p == '<') {
      memcpy(out + j, "&lt;", 4);
      j += 4;
    } else if (*p == '>') {
      memcpy(out + j, "&gt;", 4);
      j += 4;
    } else if (*p == '\"') {
      memcpy(out + j, "&quot;", 6);
      j += 6;
    } else {
      out[j++] = (char)*p;
    }
    p++;
  }
  out[j] = '\0';
}

static HRESULT nv_win_notif_ensure_shortcut(void) {
  WCHAR appdata[MAX_PATH];
  WCHAR linkPath[MAX_PATH];
  WCHAR exePath[MAX_PATH];
  IShellLinkW* shellLink = NULL;
  IPersistFile* persist = NULL;
  IPropertyStore* store = NULL;
  HRESULT hr;
  DWORD n;

  n = GetEnvironmentVariableW(L"APPDATA", appdata, MAX_PATH);
  if (n == 0 || n >= MAX_PATH) return HRESULT_FROM_WIN32(ERROR_ENVVAR_NOT_FOUND);
  appdata[MAX_PATH - 1] = L'\0';
  if (wcscpy_s(linkPath, MAX_PATH, appdata) != 0) return HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW);
  PathAppendW(linkPath, L"Microsoft\\Windows\\Start Menu\\Programs");
  if (!PathAppendW(linkPath, NV_WIN_TOAST_SHORTCUT_NAME)) return E_FAIL;

  if (GetFileAttributesW(linkPath) != INVALID_FILE_ATTRIBUTES) return S_OK;

  n = GetModuleFileNameW(NULL, exePath, MAX_PATH);
  if (n == 0 || n >= MAX_PATH) return HRESULT_FROM_WIN32(GetLastError());

  hr = CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLinkW, (void**)&shellLink);
  if (FAILED(hr) || !shellLink) return hr;

  hr = IShellLinkW_SetPath(shellLink, exePath);
  if (FAILED(hr)) goto done;

  hr = IShellLinkW_QueryInterface(shellLink, &IID_IPropertyStore, (void**)&store);
  if (FAILED(hr) || !store) goto done;

  {
    PROPVARIANT pv;
    PropVariantInit(&pv);
    hr = InitPropVariantFromString(NV_WIN_TOAST_AUMID, &pv);
    if (SUCCEEDED(hr)) {
      hr = IPropertyStore_SetValue(store, &PKEY_AppUserModel_ID, &pv);
      PropVariantClear(&pv);
    }
  }
  if (FAILED(hr)) goto done;

  hr = IPropertyStore_Commit(store);
  if (FAILED(hr)) goto done;

  hr = IShellLinkW_QueryInterface(shellLink, &IID_IPersistFile, (void**)&persist);
  if (FAILED(hr) || !persist) goto done;

  hr = IPersistFile_Save(persist, linkPath, TRUE);

done:
  if (persist) IPersistFile_Release(persist);
  if (store) IPropertyStore_Release(store);
  if (shellLink) IShellLinkW_Release(shellLink);
  return hr;
}

static int nv_win_notif_once_init(void) {
  static volatile LONG s_done;
  HRESULT hr;
  if (InterlockedCompareExchange(&s_done, 1, 0) != 0) return 0;

  hr = SetCurrentProcessExplicitAppUserModelID(NV_WIN_TOAST_AUMID);
  if (FAILED(hr)) {
    nv_win_notif_log_hresult("SetCurrentProcessExplicitAppUserModelID", hr);
    InterlockedExchange(&s_done, 0);
    return -1;
  }

  hr = nv_win_notif_ensure_shortcut();
  if (FAILED(hr)) {
    nv_win_notif_log_hresult("toast Start Menu shortcut", hr);
    InterlockedExchange(&s_done, 0);
    return -1;
  }

  return 0;
}

#if NV_WIN_TOAST_SDK

static int nv_win_notif_build_toast_xml_utf8(const char* title, const char* body, const char* icon_path, char* out,
                                            size_t out_cap) {
  char tesc[768];
  char besc[1536];
  (void)icon_path; /* appLogoOverride needs stable file URI; title/body only for now */
  nv_win_notif_esc_xml_utf8(title, tesc, sizeof tesc);
  nv_win_notif_esc_xml_utf8(body, besc, sizeof besc);
  if (snprintf(out, out_cap,
               "<toast><visual><binding template=\"ToastGeneric\">"
               "<text id=\"1\">%s</text><text id=\"2\">%s</text>"
               "</binding></visual></toast>",
               tesc, besc) < 0)
    return -1;
  return 0;
}

#endif /* NV_WIN_TOAST_SDK */

NV_INTERNAL NV_WIN_NOTIF_ATTR int nv_win_notification_show(long long id, const char* title, const char* body,
                                                         const char* icon_path, nv_window_t* w) {
  (void)w;
#if NV_WIN_TOAST_SDK
  {
    HRESULT hr;
    HSTRING_HEADER hcls_mgr, hcls_doc, hcls_toast, htag_hdr, hxml_hdr;
    HSTRING hcls_mgr_s = NULL, hcls_doc_s = NULL, hcls_toast_s = NULL;
    HSTRING htag = NULL, hxml = NULL;
    __x_ABI_CWindows_CUI_CNotifications_CIToastNotificationManagerStatics* mgr = NULL;
    __x_ABI_CWindows_CUI_CNotifications_CIToastNotifier* notifier = NULL;
    __x_ABI_CWindows_CUI_CNotifications_CIToastNotificationFactory* fact = NULL;
    __x_ABI_CWindows_CUI_CNotifications_CIToastNotification* toast = NULL;
    __x_ABI_CWindows_CUI_CNotifications_CIToastNotification2* toast2 = NULL;
    IInspectable* xmlInspectable = NULL;
    __x_ABI_CWindows_CData_CXml_CDom_CIXmlDocument* xmlDoc = NULL;
    __x_ABI_CWindows_CData_CXml_CDom_CIXmlDocumentIO* xmlIo = NULL;
    char xml8[4096];
    WCHAR* xmlW = NULL;
    WCHAR tagBuf[32];
    __x_ABI_CWindows_CUI_CNotifications_NotificationSetting setting;
    int rc = -2;
    int nxml;

    if (nv_win_notif_test_stub()) return 0;
    if (nv_win_notif_once_init() != 0) return -2;

    if (nv_win_notif_build_toast_xml_utf8(title, body, icon_path, xml8, sizeof xml8) != 0) return -2;

    nxml = MultiByteToWideChar(CP_UTF8, 0, xml8, -1, NULL, 0);
    if (nxml <= 0) return -2;
    xmlW = (WCHAR*)HeapAlloc(GetProcessHeap(), 0, (size_t)nxml * sizeof(WCHAR));
    if (!xmlW) return -2;
    if (MultiByteToWideChar(CP_UTF8, 0, xml8, -1, xmlW, nxml) <= 0) {
      HeapFree(GetProcessHeap(), 0, xmlW);
      return -2;
    }

    hr = WindowsCreateStringReference(RuntimeClass_Windows_UI_Notifications_ToastNotificationManager,
                                      (UINT32)wcslen(RuntimeClass_Windows_UI_Notifications_ToastNotificationManager),
                                      &hcls_mgr, &hcls_mgr_s);
    if (FAILED(hr)) goto cleanup;

    hr = RoGetActivationFactory(hcls_mgr_s, &IID___x_ABI_CWindows_CUI_CNotifications_CIToastNotificationManagerStatics,
                              (void**)&mgr);
    if (FAILED(hr) || !mgr) {
      nv_win_notif_log_hresult("RoGetActivationFactory(ToastNotificationManager)", hr);
      goto cleanup;
    }

    {
      HSTRING_HEADER haumid_hdr;
      HSTRING haumid = NULL;
      hr = WindowsCreateStringReference(NV_WIN_TOAST_AUMID, (UINT32)wcslen(NV_WIN_TOAST_AUMID), &haumid_hdr, &haumid);
      if (FAILED(hr)) goto cleanup;
      hr = __x_ABI_CWindows_CUI_CNotifications_CIToastNotificationManagerStatics_CreateToastNotifierWithId(
          mgr, haumid, &notifier);
      if (FAILED(hr) || !notifier) {
        nv_win_notif_log_hresult("CreateToastNotifierWithId", hr);
        goto cleanup;
      }
    }

    hr = __x_ABI_CWindows_CUI_CNotifications_CIToastNotifier_get_Setting(notifier, &setting);
    if (FAILED(hr)) goto cleanup;
    if (setting != __x_ABI_CWindows_CUI_CNotifications_NotificationSetting_Enabled) {
      rc = -1;
      goto cleanup;
    }

    hr = WindowsCreateStringReference(RuntimeClass_Windows_Data_Xml_Dom_XmlDocument,
                                      (UINT32)wcslen(RuntimeClass_Windows_Data_Xml_Dom_XmlDocument), &hcls_doc,
                                      &hcls_doc_s);
    if (FAILED(hr)) goto cleanup;

    hr = RoActivateInstance(hcls_doc_s, &xmlInspectable);
    if (FAILED(hr) || !xmlInspectable) goto cleanup;

    hr = xmlInspectable->lpVtbl->QueryInterface(xmlInspectable, &IID___x_ABI_CWindows_CData_CXml_CDom_CIXmlDocument,
                                                (void**)&xmlDoc);
    if (FAILED(hr) || !xmlDoc) goto cleanup;

    hr = xmlDoc->lpVtbl->QueryInterface(xmlDoc, &IID___x_ABI_CWindows_CData_CXml_CDom_CIXmlDocumentIO, (void**)&xmlIo);
    if (FAILED(hr) || !xmlIo) goto cleanup;

    hr = WindowsCreateStringReference(xmlW, (UINT32)(wcslen(xmlW)), &hxml_hdr, &hxml);
    if (FAILED(hr)) goto cleanup;

    hr = __x_ABI_CWindows_CData_CXml_CDom_CIXmlDocumentIO_LoadXml(xmlIo, hxml);
    if (FAILED(hr)) {
      nv_win_notif_log_hresult("LoadXml(toast)", hr);
      goto cleanup;
    }

    hr = WindowsCreateStringReference(RuntimeClass_Windows_UI_Notifications_ToastNotification,
                                      (UINT32)wcslen(RuntimeClass_Windows_UI_Notifications_ToastNotification),
                                      &hcls_toast, &hcls_toast_s);
    if (FAILED(hr)) goto cleanup;

    hr = RoGetActivationFactory(hcls_toast_s, &IID___x_ABI_CWindows_CUI_CNotifications_CIToastNotificationFactory,
                                (void**)&fact);
    if (FAILED(hr) || !fact) goto cleanup;

    hr = __x_ABI_CWindows_CUI_CNotifications_CIToastNotificationFactory_CreateToastNotification(fact, xmlDoc, &toast);
    if (FAILED(hr) || !toast) goto cleanup;

    hr = toast->lpVtbl->QueryInterface(toast, &IID___x_ABI_CWindows_CUI_CNotifications_CIToastNotification2,
                                       (void**)&toast2);
    if (FAILED(hr) || !toast2) goto cleanup;

    if (_snwprintf_s(tagBuf, 32, _TRUNCATE, L"%lld", id) < 0) goto cleanup;
    hr = WindowsCreateStringReference(tagBuf, (UINT32)wcslen(tagBuf), &htag_hdr, &htag);
    if (FAILED(hr)) goto cleanup;

    hr = __x_ABI_CWindows_CUI_CNotifications_CIToastNotification2_put_Tag(toast2, htag);
    if (FAILED(hr)) goto cleanup;

    hr = __x_ABI_CWindows_CUI_CNotifications_CIToastNotifier_Show(notifier, toast);
    if (FAILED(hr)) {
      nv_win_notif_log_hresult("ToastNotifier.Show", hr);
      goto cleanup;
    }

    rc = 0;

  cleanup:
    if (toast2) __x_ABI_CWindows_CUI_CNotifications_CIToastNotification2_Release(toast2);
    if (toast) __x_ABI_CWindows_CUI_CNotifications_CIToastNotification_Release(toast);
    if (fact) __x_ABI_CWindows_CUI_CNotifications_CIToastNotificationFactory_Release(fact);
    if (xmlIo) __x_ABI_CWindows_CData_CXml_CDom_CIXmlDocumentIO_Release(xmlIo);
    if (xmlDoc) __x_ABI_CWindows_CData_CXml_CDom_CIXmlDocument_Release(xmlDoc);
    nv_win_notif_irelease((IUnknown*)xmlInspectable);
    if (notifier) __x_ABI_CWindows_CUI_CNotifications_CIToastNotifier_Release(notifier);
    if (mgr) __x_ABI_CWindows_CUI_CNotifications_CIToastNotificationManagerStatics_Release(mgr);
    if (xmlW) HeapFree(GetProcessHeap(), 0, xmlW);
    return rc;
  }
#else
  (void)title;
  (void)body;
  (void)icon_path;
  (void)id;
  if (nv_win_notif_test_stub()) return 0;
  return -2;
#endif
}

NV_INTERNAL NV_WIN_NOTIF_ATTR int nv_win_notification_close(long long id) {
#if NV_WIN_TOAST_SDK
  {
    HRESULT hr;
    HSTRING_HEADER hcls_mgr, htag_hdr;
    HSTRING hcls_mgr_s = NULL, htag = NULL;
    __x_ABI_CWindows_CUI_CNotifications_CIToastNotificationManagerStatics* mgr = NULL;
    __x_ABI_CWindows_CUI_CNotifications_CIToastNotificationManagerStatics2* mgr2 = NULL;
    __x_ABI_CWindows_CUI_CNotifications_CIToastNotificationHistory* hist = NULL;
    WCHAR tagBuf[32];
    int rc = -2;

    if (nv_win_notif_test_stub()) return 0;
    if (nv_win_notif_once_init() != 0) return -2;

    hr = WindowsCreateStringReference(RuntimeClass_Windows_UI_Notifications_ToastNotificationManager,
                                      (UINT32)wcslen(RuntimeClass_Windows_UI_Notifications_ToastNotificationManager),
                                      &hcls_mgr, &hcls_mgr_s);
    if (FAILED(hr)) return -2;

    hr = RoGetActivationFactory(hcls_mgr_s, &IID___x_ABI_CWindows_CUI_CNotifications_CIToastNotificationManagerStatics,
                                (void**)&mgr);
    if (FAILED(hr) || !mgr) goto done;

    hr = mgr->lpVtbl->QueryInterface(mgr, &IID___x_ABI_CWindows_CUI_CNotifications_CIToastNotificationManagerStatics2,
                                     (void**)&mgr2);
    if (FAILED(hr) || !mgr2) goto done;

    hr = __x_ABI_CWindows_CUI_CNotifications_CIToastNotificationManagerStatics2_get_History(mgr2, &hist);
    if (FAILED(hr) || !hist) goto done;

    if (_snwprintf_s(tagBuf, 32, _TRUNCATE, L"%lld", id) < 0) goto done;
    hr = WindowsCreateStringReference(tagBuf, (UINT32)wcslen(tagBuf), &htag_hdr, &htag);
    if (FAILED(hr)) goto done;

    hr = __x_ABI_CWindows_CUI_CNotifications_CIToastNotificationHistory_Remove(hist, htag);
    if (FAILED(hr)) {
      nv_win_notif_log_hresult("ToastNotificationHistory.Remove", hr);
      goto done;
    }
    rc = 0;

  done:
    if (hist) __x_ABI_CWindows_CUI_CNotifications_CIToastNotificationHistory_Release(hist);
    if (mgr2) __x_ABI_CWindows_CUI_CNotifications_CIToastNotificationManagerStatics2_Release(mgr2);
    if (mgr) __x_ABI_CWindows_CUI_CNotifications_CIToastNotificationManagerStatics_Release(mgr);
    return rc;
  }
#else
  (void)id;
  if (nv_win_notif_test_stub()) return 0;
  return -2;
#endif
}

#endif /* _WIN32 */
