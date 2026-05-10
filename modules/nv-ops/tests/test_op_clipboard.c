#include "nv_op_clipboard.h"
#include "nv_window_internal.h"
#include "nv_arena.h"
#include "nv_json.h"
#include "test_helpers.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* 1×1 PNG (red pixel) — base64 for clipboard image stub replies */
static const char k_stub_png_b64[] =
    "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8BQDwAEhQGAhKmMIQAAAABJRU5ErkJggg==";

#if defined(__APPLE__)
char *nv_mac_clipboard_read_image(int *w, int *h) {
  const char *s = k_stub_png_b64;
  size_t       n = strlen(s) + 1u;
  char        *out;
  if (w) *w = 1;
  if (h) *h = 1;
  out = (char *)malloc(n);
  if (!out) return NULL;
  memcpy(out, s, n);
  return out;
}

int nv_mac_clipboard_write_image(const char *b64) {
  (void)b64;
  return 0;
}

int nv_mac_clipboard_has_image(void) {
  return 1;
}
#endif

/* Strong overrides for weak nv_win_clipboard_* (MinGW/Clang on Windows). */
#if defined(_WIN32) && (defined(__GNUC__) || defined(__clang__))
char *nv_win_clipboard_read_text(void) {
  const char *s = "nv_win_clipboard_stub_read";
  size_t        n = strlen(s) + 1;
  char         *out = (char *)malloc(n);
  if (!out) return NULL;
  memcpy(out, s, n);
  return out;
}

int nv_win_clipboard_write_text(const char *utf8) {
  (void)utf8;
  return 0;
}

void nv_win_clipboard_clear(void) {}

int nv_win_clipboard_has_text(void) {
  return 1;
}

char *nv_win_clipboard_read_image(int *w, int *h) {
  const char *s = k_stub_png_b64;
  size_t        n = strlen(s) + 1u;
  char         *out;
  if (w) *w = 1;
  if (h) *h = 1;
  out = (char *)malloc(n);
  if (!out) return NULL;
  memcpy(out, s, n);
  return out;
}

int nv_win_clipboard_write_image(const char *b64) {
  (void)b64;
  return 0;
}

int nv_win_clipboard_has_image(void) {
  return 1;
}
#endif

/* Strong overrides for weak nv_linux_clipboard_* (Linux + GCC/Clang). */
#if defined(__linux__) && !defined(__APPLE__) && (defined(__GNUC__) || defined(__clang__))
char *nv_linux_clipboard_read_text(void) {
  const char *s = "nv_linux_clipboard_stub_read";
  size_t        n = strlen(s) + 1;
  char         *out = (char *)malloc(n);
  if (!out) return NULL;
  memcpy(out, s, n);
  return out;
}

int nv_linux_clipboard_write_text(const char *utf8) {
  (void)utf8;
  return 0;
}

void nv_linux_clipboard_clear(void) {}

int nv_linux_clipboard_has_text(void) {
  return 1;
}

char *nv_linux_clipboard_read_image(int *w, int *h) {
  const char *s = k_stub_png_b64;
  size_t        n = strlen(s) + 1u;
  char         *out;
  if (w) *w = 1;
  if (h) *h = 1;
  out = (char *)malloc(n);
  if (!out) return NULL;
  memcpy(out, s, n);
  return out;
}

int nv_linux_clipboard_write_image(const char *b64) {
  (void)b64;
  return 0;
}

int nv_linux_clipboard_has_image(void) {
  return 1;
}
#endif

static int tests_passed = 0, tests_failed = 0;
static void ok(const char *name) {
  printf("✓ %s\n", name);
  tests_passed++;
}
static void fail(const char *name, const char *why) {
  printf("✗ %s: %s\n", name, why);
  tests_failed++;
}

static nv_window_t *make_window(void) {
  nv_window_cfg_t cfg = {"ClipboardTest", 640, 480, 0, 0, 1, 0, 0, 0, 0};
  return nv_window_alloc(NULL, &cfg);
}

static nv_json_val_t *parse(nv_arena_t *arena, const char *s) {
  return nv_json_parse(arena, s ? s : "null");
}

static void expect_image_read_ok(const char *name) {
  const char *j = test_last_reply_json();
  if (!test_last_reply_ok()) {
    fail(name, "expected ok");
    return;
  }
  if (!j || strstr(j, "\"data\"") == NULL || strstr(j, "\"width\"") == NULL ||
      strstr(j, "\"height\"") == NULL || strstr(j, "\"format\"") == NULL) {
    fail(name, "expected width/height/format/data in JSON");
    return;
  }
  ok(name);
}

static void test_clipboard_ops(void) {
  nv_window_t *win = make_window();
  if (!win) {
    fail("make_window", "alloc failed");
    return;
  }
  nv_arena_t *arena = nv_arena_create(65536);
  if (!arena) {
    fail("arena", "alloc failed");
    nv_window_free(win);
    return;
  }

  test_reset_replies();
  nv_arena_reset(arena);
  nv_op_clipboard_read_text(win, 1, NULL, arena);
  if (!test_last_reply_ok()) fail("read_text", "expected ok");
  else ok("read_text");

  test_reset_replies();
  nv_arena_reset(arena);
  nv_op_clipboard_write_text(win, 2, parse(arena, "{\"text\":\"hello\"}"), arena);
  if (!test_last_reply_ok()) fail("write_text", "expected ok");
  else ok("write_text");

  test_reset_replies();
  nv_arena_reset(arena);
  nv_op_clipboard_read_image(win, 3, NULL, arena);
  expect_image_read_ok("read_image");

  test_reset_replies();
  nv_arena_reset(arena);
  {
    char payload[512];
    (void)snprintf(payload, sizeof payload, "{\"format\":\"png\",\"data\":\"%s\"}", k_stub_png_b64);
    nv_op_clipboard_write_image(win, 4, parse(arena, payload), arena);
  }
  if (!test_last_reply_ok()) fail("write_image", "expected ok");
  else ok("write_image");

  test_reset_replies();
  nv_arena_reset(arena);
  nv_op_clipboard_clear(win, 5, NULL, arena);
  if (!test_last_reply_ok()) fail("clear", "expected ok");
  else ok("clear");

  test_reset_replies();
  nv_arena_reset(arena);
  nv_op_clipboard_has_text(win, 6, NULL, arena);
  if (!test_last_reply_ok()) fail("has_text", "expected ok");
  else ok("has_text");

  test_reset_replies();
  nv_arena_reset(arena);
  nv_op_clipboard_has_image(win, 7, NULL, arena);
  if (!test_last_reply_ok()) {
    fail("has_image", "expected ok");
  } else if (!test_last_reply_json() || strstr(test_last_reply_json(), "true") == NULL) {
    fail("has_image", "expected true");
  } else ok("has_image");

  nv_arena_destroy(arena);
  nv_window_free(win);
}

/* Windows: clipboard ops call nv_win_*; GCC/Clang builds override with stubs above. */
static void test_op_clipboard_win(void) {
#if defined(_WIN32)
  nv_window_t *win = make_window();
  if (!win) {
    fail("win make_window", "alloc failed");
    return;
  }
  nv_arena_t *arena = nv_arena_create(65536);
  if (!arena) {
    fail("win arena", "alloc failed");
    nv_window_free(win);
    return;
  }

  test_reset_replies();
  nv_arena_reset(arena);
  nv_op_clipboard_read_text(win, 201, NULL, arena);
  if (!test_last_reply_ok()) {
    fail("win read_text", "expected ok");
  } else if (!test_last_reply_json()) {
    fail("win read_text", "missing reply json");
  } else {
#if defined(__GNUC__) || defined(__clang__)
    if (strstr(test_last_reply_json(), "nv_win_clipboard_stub_read") == NULL) {
      fail("win read_text", "expected stub text in reply JSON");
    } else {
      ok("win read_text (stub)");
    }
#else
    if (strstr(test_last_reply_json(), "\"text\"") == NULL) {
      fail("win read_text", "expected text field in reply JSON");
    } else {
      ok("win read_text (live)");
    }
#endif
  }

  test_reset_replies();
  nv_arena_reset(arena);
  nv_op_clipboard_read_image(win, 205, NULL, arena);
#if defined(__GNUC__) || defined(__clang__)
  expect_image_read_ok("win read_image (stub)");
#else
  if (!test_last_reply_ok() && test_last_reply_error_code() &&
      strcmp(test_last_reply_error_code(), "ERR_IO") == 0)
    ok("win read_image (live, empty clipboard ok)");
  else if (test_last_reply_ok())
    ok("win read_image (live)");
  else
    fail("win read_image", "unexpected reply");
#endif

  test_reset_replies();
  nv_arena_reset(arena);
  nv_op_clipboard_has_text(win, 202, NULL, arena);
  if (!test_last_reply_ok()) {
    fail("win has_text", "expected ok");
  } else if (!test_last_reply_json()) {
    fail("win has_text", "missing reply json");
  } else {
#if defined(__GNUC__) || defined(__clang__)
    if (strstr(test_last_reply_json(), "true") == NULL) {
      fail("win has_text", "expected true in reply JSON");
    } else {
      ok("win has_text (stub)");
    }
#else
    if (strstr(test_last_reply_json(), "\"value\"") == NULL) {
      fail("win has_text", "expected value field in reply JSON");
    } else {
      ok("win has_text (live)");
    }
#endif
  }

  test_reset_replies();
  nv_arena_reset(arena);
  nv_op_clipboard_write_text(win, 203, parse(arena, "{\"text\":\"ignored-by-stub\"}"), arena);
  if (!test_last_reply_ok()) {
    fail("win write_text", "expected ok");
  } else {
#if defined(__GNUC__) || defined(__clang__)
    ok("win write_text (stub)");
#else
    ok("win write_text (live)");
#endif
  }

  test_reset_replies();
  nv_arena_reset(arena);
  nv_op_clipboard_clear(win, 204, NULL, arena);
  if (!test_last_reply_ok()) {
    fail("win clear", "expected ok");
  } else {
#if defined(__GNUC__) || defined(__clang__)
    ok("win clear (stub)");
#else
    ok("win clear (live)");
#endif
  }

  nv_arena_destroy(arena);
  nv_window_free(win);
#else
  (void)0;
#endif
}

/* Linux: clipboard ops call nv_linux_*; GCC/Clang builds override with stubs above. */
static void test_op_clipboard_linux(void) {
#if defined(__linux__) && !defined(__APPLE__) && (defined(__GNUC__) || defined(__clang__))
  nv_window_t *win = make_window();
  if (!win) {
    fail("linux make_window", "alloc failed");
    return;
  }
  nv_arena_t *arena = nv_arena_create(65536);
  if (!arena) {
    fail("linux arena", "alloc failed");
    nv_window_free(win);
    return;
  }

  test_reset_replies();
  nv_arena_reset(arena);
  nv_op_clipboard_read_text(win, 301, NULL, arena);
  if (!test_last_reply_ok()) {
    fail("linux read_text", "expected ok");
  } else if (!test_last_reply_json()) {
    fail("linux read_text", "missing reply json");
  } else if (strstr(test_last_reply_json(), "nv_linux_clipboard_stub_read") == NULL) {
    fail("linux read_text", "expected stub text in reply JSON");
  } else {
    ok("linux read_text (stub)");
  }

  test_reset_replies();
  nv_arena_reset(arena);
  nv_op_clipboard_read_image(win, 305, NULL, arena);
  expect_image_read_ok("linux read_image (stub)");

  test_reset_replies();
  nv_arena_reset(arena);
  nv_op_clipboard_has_text(win, 302, NULL, arena);
  if (!test_last_reply_ok()) {
    fail("linux has_text", "expected ok");
  } else if (!test_last_reply_json()) {
    fail("linux has_text", "missing reply json");
  } else if (strstr(test_last_reply_json(), "true") == NULL) {
    fail("linux has_text", "expected true in reply JSON");
  } else {
    ok("linux has_text (stub)");
  }

  test_reset_replies();
  nv_arena_reset(arena);
  nv_op_clipboard_write_text(win, 303, parse(arena, "{\"text\":\"ignored-by-stub\"}"), arena);
  if (!test_last_reply_ok()) {
    fail("linux write_text", "expected ok");
  } else {
    ok("linux write_text (stub)");
  }

  test_reset_replies();
  nv_arena_reset(arena);
  nv_op_clipboard_clear(win, 304, NULL, arena);
  if (!test_last_reply_ok()) {
    fail("linux clear", "expected ok");
  } else {
    ok("linux clear (stub)");
  }

  nv_arena_destroy(arena);
  nv_window_free(win);
#else
  (void)0;
#endif
}

int main(void) {
  printf("=== nativeview Clipboard Ops Tests ===\n\n");
  test_clipboard_ops();
  test_op_clipboard_win();
  test_op_clipboard_linux();
  printf("\n=== Summary ===\n");
  printf("Passed: %d/%d\n", tests_passed, tests_passed + tests_failed);
  return (tests_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
