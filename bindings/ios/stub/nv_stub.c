// SPDX-License-Identifier: Apache-2.0
//
// Minimal symbols for `create_nativeview_xcframework.sh --stub` so Swift can
// link against the same `nv_util.h` entry points as the real static archive.
// Do not link this object into the CMake-produced libnativeview.a.

#include <string.h>

#include "nv_util.h"

NV_API const char* nv_version_string(void) { return "nativeview-ios-stub"; }

NV_API void nv_get_version_info(NvVersionInfo* out_info) {
  if (!out_info) {
    return;
  }
  memset(out_info, 0, sizeof(*out_info));
  out_info->major = 0;
  out_info->minor = 0;
  out_info->patch = 0;
  out_info->build_sha = "stub";
}

NV_API uint64_t nv_bench_now(void) { return 0ULL; }

void nv_ios_binding_stub(void) {}
