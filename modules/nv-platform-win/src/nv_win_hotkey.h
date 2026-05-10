/* Windows global hotkey helper — used from nv_win.c WndProc. */
#ifdef _WIN32
#ifndef NV_WIN_HOTKEY_H
#define NV_WIN_HOTKEY_H

#include <windows.h>

#include "util/nv_log.h"

struct nv_win_window_platform;

NV_INTERNAL int nv_win_hotkey_wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam,
                                      struct nv_win_window_platform* platform, LRESULT* lr_out);

#endif /* NV_WIN_HOTKEY_H */
#endif /* _WIN32 */
