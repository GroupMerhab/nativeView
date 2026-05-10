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

#ifndef NV_UTIL_H
#define NV_UTIL_H
#include "util/nv_log.h"
#include <stdint.h>
typedef struct {
    uint16_t major;
    uint16_t minor;
    uint16_t patch;
    const char* build_sha;
} NvVersionInfo;
NV_API const char* nv_version_string(void);
NV_API void nv_get_version_info(NvVersionInfo* out_info);
NV_API uint64_t nv_bench_now(void);
#endif /* NV_UTIL_H */
