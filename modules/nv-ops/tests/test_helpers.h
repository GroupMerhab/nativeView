/* Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0 */

#ifndef NV_TEST_HELPERS_H
#define NV_TEST_HELPERS_H

#include "nv.h"
#include "nv_json.h"

// Reset the test reply state
void test_reset_replies(void);

// Get the last reply status
int test_last_reply_ok(void);

// Get the last reply JSON payload (as string)
const char* test_last_reply_json(void);

// Get the last error code
const char* test_last_reply_error_code(void);

// True if last reply JSON (from test harness) contains an "error" key
int test_last_reply_json_has_error_key(void);

// Helper to create a test window
nv_window_t* test_make_window(const char* title);

// Helper to parse JSON
nv_json_val_t* test_parse(nv_arena_t* arena, const char* s);

#endif
