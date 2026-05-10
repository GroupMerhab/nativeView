/* =============================================================================
 * test_op_fs.c - FS Ops Tests (path traversal rejection)
 * =============================================================================
 */
#include "nv_op_fs.h"
#include "nv_window_internal.h"
#include "nv_arena.h"
#include "nv_json.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "test_helpers.h"

static int tests_passed = 0, tests_failed = 0;
static void ok(const char* name){ printf("✓ %s\n", name); tests_passed++; }
static void fail(const char* name, const char* why){ printf("✗ %s: %s\n", name, why); tests_failed++; }

static nv_window_t* make_window(void) {
  nv_window_cfg_t cfg = {
    .title = "FsOpsTest",
    .width = 640,
    .height = 480,
    .min_width = 0,
    .min_height = 0,
    .resizable = 1,
    .frameless = 0,
    .transparent = 0,
    .devtools = 0
  };
  return nv_window_alloc(NULL, &cfg);
}

static nv_json_val_t* parse(nv_arena_t* arena, const char* s){
  return nv_json_parse(arena, s ? s : "null");
}

static void test_path_traversal_rejection(void) {
  nv_window_t* win = make_window();
  if (!win) { fail("make_window", "nv_window_alloc failed"); return; }
  nv_arena_t* arena = nv_arena_create(2048);
  if (!arena) { fail("arena", "nv_arena_create failed"); nv_window_free(win); return; }

  /* read_text with traversal */
  test_reset_replies();
  nv_json_val_t* a1 = parse(arena, "{\"path\":\"../etc/passwd\"}");
  nv_op_fs_read_text(win, 1, a1, arena);
  if (test_last_reply_ok()) fail("read_text traversal", "expected ERR_PERMISSION");
  else if (!test_last_reply_error_code() || strcmp(test_last_reply_error_code(),"ERR_PERMISSION")!=0)
    fail("read_text traversal", "wrong error code");
  else ok("read_text traversal");

  /* remove with traversal */
  test_reset_replies();
  nv_arena_reset(arena);
  nv_json_val_t* a2 = parse(arena, "{\"path\":\"../../tmp/file\"}");
  nv_op_fs_remove(win, 2, a2, arena);
  if (test_last_reply_ok()) fail("remove traversal", "expected ERR_PERMISSION");
  else if (!test_last_reply_error_code() || strcmp(test_last_reply_error_code(),"ERR_PERMISSION")!=0)
    fail("remove traversal", "wrong error code");
  else ok("remove traversal");

  /* move with traversal */
  test_reset_replies();
  nv_arena_reset(arena);
  nv_json_val_t* a3 = parse(arena, "{\"src\":\"a\",\"dst\":\"../b\"}");
  nv_op_fs_move(win, 3, a3, arena);
  if (test_last_reply_ok()) fail("move traversal", "expected ERR_PERMISSION");
  else if (!test_last_reply_error_code() || strcmp(test_last_reply_error_code(),"ERR_PERMISSION")!=0)
    fail("move traversal", "wrong error code");
  else ok("move traversal");

  nv_arena_destroy(arena);
  nv_window_free(win);
}

int main(void){
  printf("=== nativeview FS Ops Tests ===\n\n");
  test_path_traversal_rejection();
  printf("\n=== Summary ===\n");
  printf("Passed: %d/%d\n", tests_passed, tests_passed + tests_failed);
  return (tests_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
