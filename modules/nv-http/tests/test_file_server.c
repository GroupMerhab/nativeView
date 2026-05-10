/*
 * Copyright (c) 2026
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 */

#include "file_server.h"
#include "http_protocol.h"
#include "nv_arena.h"
#include <stdio.h>
#include <string.h>

static int g_count, g_passed;

static void ok(const char* name) { g_count++; g_passed++; printf("  [OK] %s\n", name); }
static void fail(const char* name, const char* r) {
  g_count++; printf("  [FAIL] %s: %s\n", name, r);
}

static void test_create_free(void) {
  nv_arena_t* a = nv_arena_create(8192);
  if (!a) { fail("create_free", "arena"); return; }

  FileServer* fs = file_server_create(a, "/var/www", 1024 * 1024, 1);
  if (!fs) { fail("create_free", "create"); nv_arena_destroy(a); return; }
  ok("file_server_create");

  file_server_free(&fs);
  if (fs != NULL) fail("create_free", "free did not null");
  else ok("file_server_free");

  nv_arena_destroy(a);
}

static void test_null_safety(void) {
  file_server_free(NULL);
  { FileServer* fs = NULL; file_server_free(&fs); }
  ok("null_safety");
}

static void test_serve_file(void) {
#ifdef NVHTTP_FIXTURES_DIR
  nv_arena_t* a = nv_arena_create(65536);
  if (!a) { fail("serve_file", "arena"); return; }

  char root[512];
  snprintf(root, sizeof(root), "%s", NVHTTP_FIXTURES_DIR);

  FileServer* fs = file_server_create(a, root, 1024 * 1024, 0);
  if (!fs) { nv_arena_destroy(a); return; }

  HttpRequest req;
  memset(&req, 0, sizeof(req));
  strcpy(req.method, "GET");
  strcpy(req.path, "/hello.txt");

  HttpResponse* resp = http_response_create(a, 200);
  if (!resp) { nv_arena_destroy(a); return; }

  int r = file_server_handle_request(a, fs, &req, resp);
  if (r != FILE_OK) {
    char buf[64];
    snprintf(buf, sizeof(buf), "handle_request returned %d", r);
    fail("serve_file", buf);
  } else if (resp->status_code != 200) {
    char buf[64];
    snprintf(buf, sizeof(buf), "status %d", resp->status_code);
    fail("serve_file", buf);
  } else {
    int ok_body = 0;
    if (resp->body_is_file && resp->body_file_path[0]) {
      FILE* f = fopen(resp->body_file_path, "rb");
      if (f) {
        char tmp[64];
        size_t n = fread(tmp, 1, sizeof(tmp) - 1, f);
        fclose(f);
        tmp[n] = '\0';
        if (strstr(tmp, "Hello") != NULL) ok_body = 1;
      }
    } else if (resp->body && resp->body_len >= 5) {
      if (strstr(resp->body, "Hello") != NULL) ok_body = 1;
    }
    if (!ok_body)
      fail("serve_file", "body content mismatch");
    else
      ok("serve_file");
  }

  nv_arena_destroy(a);
#else
  (void)0;
  ok("serve_file (skipped, no NVHTTP_FIXTURES_DIR)");
#endif
}

#if defined(_WIN32) && defined(NVHTTP_FIXTURES_DIR)
/** Root with `\\` must still pass validate_path vs normalized full paths (regression: 403 on UI). */
static void test_win_backslash_root(void) {
  nv_arena_t* a = nv_arena_create(65536);
  if (!a) {
    fail("win_backslash_root", "arena");
    return;
  }

  char root[512];
  snprintf(root, sizeof(root), "%s", NVHTTP_FIXTURES_DIR);
  for (size_t i = 0; root[i]; i++) {
    if (root[i] == '/')
      root[i] = '\\';
  }

  FileServer* fs = file_server_create(a, root, 1024 * 1024, 0);
  if (!fs) {
    fail("win_backslash_root", "create");
    nv_arena_destroy(a);
    return;
  }

  HttpRequest req;
  memset(&req, 0, sizeof(req));
  strcpy(req.method, "GET");
  strcpy(req.path, "/hello.txt");

  HttpResponse* resp = http_response_create(a, 200);
  if (!resp) {
    fail("win_backslash_root", "response");
    nv_arena_destroy(a);
    return;
  }

  int r = file_server_handle_request(a, fs, &req, resp);
  if (r == FILE_OK && resp->status_code == 200 && resp->body && strstr(resp->body, "Hello") != NULL)
    ok("win_backslash_root");
  else
    fail("win_backslash_root", "expected 200 with Hello");

  nv_arena_destroy(a);
}
#endif

static void test_access_denied(void) {
  nv_arena_t* a = nv_arena_create(65536);
  if (!a) return;

  FileServer* fs = file_server_create(a, "/var/www", 1024, 0);
  HttpRequest req;
  memset(&req, 0, sizeof(req));
  strcpy(req.method, "GET");
  strcpy(req.path, "/../../../etc/passwd");

  HttpResponse* resp = http_response_create(a, 200);
  int r = file_server_handle_request(a, fs, &req, resp);

  if (r == FILE_ERROR_ACCESS_DENIED && resp->status_code == 403)
    ok("access_denied_traversal");
  else
    fail("access_denied_traversal", "expected 403");

  nv_arena_destroy(a);
}

static void test_not_found(void) {
  nv_arena_t* a = nv_arena_create(65536);
  if (!a) return;

  FileServer* fs = file_server_create(a, "/nonexistent_dir_xyz", 1024, 0);
  HttpRequest req;
  memset(&req, 0, sizeof(req));
  strcpy(req.method, "GET");
  strcpy(req.path, "/missing.txt");

  HttpResponse* resp = http_response_create(a, 200);
  int r = file_server_handle_request(a, fs, &req, resp);

  if (r == FILE_ERROR_NOT_FOUND && resp->status_code == 404)
    ok("not_found");
  else
    fail("not_found", "expected 404");

  nv_arena_destroy(a);
}

int main(void) {
  printf("test_file_server\n");
  printf("================\n");

  test_create_free();
  test_null_safety();
  test_access_denied();
  test_not_found();
  test_serve_file();
#if defined(_WIN32) && defined(NVHTTP_FIXTURES_DIR)
  test_win_backslash_root();
#endif

  printf("================\n");
  printf("%d/%d passed\n", g_passed, g_count);
  return (g_passed == g_count) ? 0 : 1;
}
