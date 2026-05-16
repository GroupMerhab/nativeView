/* Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0 */

#ifndef NV_IPC_SCRIPT_H
#define NV_IPC_SCRIPT_H

#include "util/nv_log.h"

typedef enum {
  NV_PLATFORM_MAC = 0,
  NV_PLATFORM_WIN = 1,
  NV_PLATFORM_LINUX = 2
} nv_platform_t;

NV_INTERNAL const char* nv_ipc_script_raw(void);
NV_INTERNAL const char* nv_ipc_script_for_platform(nv_platform_t platform);

#endif
