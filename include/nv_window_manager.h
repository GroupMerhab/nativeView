/* Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0 */

#ifndef NV_WINDOW_MANAGER_H
#define NV_WINDOW_MANAGER_H

#include "nv_window.h"
#include "util/nv_log.h"

#define NV_WINDOW_ID_MAX  65
#define NV_MAX_WINDOWS    32
#define NV_WINDOW_ID_MAIN "main"

typedef struct {
    char         id[NV_WINDOW_ID_MAX];
    nv_window_t *window;
    int          active;   // 1=open 0=free slot
} nv_wm_entry_t;

/* 
 * Not thread-safe.
 * nv_wm_init called once from nv_app_create.
 */

NV_INTERNAL void         nv_wm_init(void);

// returns 0 on success 
// NV_ERR + return -1 if: duplicate id, registry full, invalid id 
NV_INTERNAL int          nv_wm_register(const char* id, nv_window_t* w);

NV_INTERNAL void         nv_wm_unregister(const char* id);

// returns NULL if not found 
NV_INTERNAL nv_window_t* nv_wm_get_by_id(const char* id);

// returns NULL if not found 
NV_INTERNAL const char*  nv_wm_get_id_by_window(const nv_window_t* w);

// fills out[] with active entries, sets *count 
NV_INTERNAL int          nv_wm_list(nv_wm_entry_t* out, size_t* count);

NV_INTERNAL int          nv_wm_count(void);

// valid: 1-64 chars, alphanumeric + dash + underscore only 
// invalid: empty, "*", too long, bad chars -> return 0 
NV_INTERNAL int          nv_wm_id_valid(const char* id);

#endif // NV_WINDOW_MANAGER_H
