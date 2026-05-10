/* Private tray helpers for nv_win.c — Windows only. */
#ifdef _WIN32
#ifndef NV_WIN_TRAY_H
#define NV_WIN_TRAY_H

#include <windows.h>

#include "nv.h"

struct nv_win_window_platform;

NV_INTERNAL HWND nv_win_window_hwnd(nv_window_t* window);

NV_INTERNAL int nv_win_tray_wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam,
                                    struct nv_win_window_platform* platform, LRESULT* lr_out);
NV_INTERNAL void nv_win_tray_detach_hwnd(HWND hwnd);

#endif /* NV_WIN_TRAY_H */
#endif /* _WIN32 */
