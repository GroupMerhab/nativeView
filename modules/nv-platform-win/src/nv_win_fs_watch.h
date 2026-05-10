/* Windows fs.watch — private helpers for nv_win.c */
#ifdef _WIN32
#ifndef NV_WIN_FS_WATCH_H
#define NV_WIN_FS_WATCH_H

#include <windows.h>

#include "nv.h"

struct nv_win_window_platform;

#define WM_NV_FS_CHANGE (WM_APP + 6)

NV_INTERNAL int nv_win_fs_watch_wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam,
                                          struct nv_win_window_platform* platform, LRESULT* lr_out);
NV_INTERNAL void nv_win_fs_watch_detach_for_window(nv_window_t* window);

#endif
#endif
