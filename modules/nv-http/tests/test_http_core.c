/*
 * Copyright (c) 2026
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 */

#include "http_core.h"
#include "http_protocol.h"
#include "nv_arena.h"
#include <stdio.h>
#include <string.h>

static int g_count, g_passed;

static void ok(const char* name) { g_count++; g_passed++; printf("  [OK] %s\n", name); }
static void fail(const char* name, const char* r) { g_count++; printf("  [FAIL] %s: %s\n", name, r); }

static void dummy_handler(nv_arena_t* arena, const HttpRequest* req,
    HttpResponse* resp, void* data) {
  (void)arena; (void)req; (void)resp; (void)data;
}

static void test_create_free(void) {
  nv_arena_t* a = nv_arena_create(32768);
  if (!a) { fail("create_free", "arena create"); return; }

  HttpCore* c = http_core_create(a, 8080, 5);
  if (!c) { fail("create_free", "core create"); nv_arena_destroy(a); return; }
  ok("core_create");

  http_core_free(&c);
  if (c != NULL) fail("create_free", "free did not null");
  else ok("core_free");
  nv_arena_destroy(a);
}

static void test_register_route(void) {
  nv_arena_t* a = nv_arena_create(32768);
  if (!a) { fail("register_route", "arena"); return; }

  HttpCore* c = http_core_create(a, 9090, 5);
  if (!c) { nv_arena_destroy(a); return; }

  int r = http_core_register_route(c, "GET", "/", dummy_handler, NULL);
  if (r != HTTP_OK) fail("register_route", "expected HTTP_OK");
  else ok("register_route");

  r = http_core_register_route(c, "POST", "/api/*", dummy_handler, (void*)1);
  if (r != HTTP_OK) fail("register_wildcard", "expected HTTP_OK");
  else ok("register_wildcard");

  http_core_free(&c);
  nv_arena_destroy(a);
}

static void test_set_default(void) {
  nv_arena_t* a = nv_arena_create(32768);
  if (!a) return;
  HttpCore* c = http_core_create(a, 7070, 5);
  if (!c) { nv_arena_destroy(a); return; }
  http_core_set_default_handler(c, dummy_handler, NULL);
  ok("set_default_handler");
  http_core_free(&c);
  nv_arena_destroy(a);
}

static void test_get_stats(void) {
  nv_arena_t* a = nv_arena_create(32768);
  if (!a) return;
  HttpCore* c = http_core_create(a, 6060, 5);
  if (!c) { nv_arena_destroy(a); return; }

  HttpStats s;
  http_core_get_stats(c, &s);
  if (s.requests_handled != 0 || s.bytes_sent != 0)
    fail("get_stats", "expected zeros");
  else ok("get_stats");

  http_core_free(&c);
  nv_arena_destroy(a);
}

static void test_route_table_full(void) {
  nv_arena_t* a = nv_arena_create(65536);
  if (!a) {
    fail("route_table_full", "arena");
    return;
  }
  HttpCore* c = http_core_create(a, 5050, 5);
  if (!c) {
    fail("route_table_full", "core create");
    nv_arena_destroy(a);
    return;
  }
  char path[64];
  for (int i = 0; i < HTTP_CORE_MAX_ROUTES; i++) {
    (void)snprintf(path, sizeof(path), "/r%d", i);
    int r = http_core_register_route(c, "GET", path, dummy_handler, NULL);
    if (r != HTTP_OK) {
      fail("route_table_full", "expected HTTP_OK before cap");
      http_core_free(&c);
      nv_arena_destroy(a);
      return;
    }
  }
  int r = http_core_register_route(c, "GET", "/overflow", dummy_handler, NULL);
  if (r != HTTP_ERROR_INVALID)
    fail("route_table_full", "expected HTTP_ERROR_INVALID when table full");
  else
    ok("route_table_full");

  http_core_free(&c);
  nv_arena_destroy(a);
}

static void test_null_safety(void) {
  http_core_free(NULL);
  {
    HttpCore* c = NULL;
    http_core_free(&c);
  }
  http_core_register_route(NULL, "GET", "/", dummy_handler, NULL);
  http_core_set_default_handler(NULL, dummy_handler, NULL);
  http_core_stop(NULL);
  {
    HttpStats s;
    memset(&s, 0xff, sizeof(s));
    http_core_get_stats(NULL, &s);
  }
  ok("null_safety");
}

int main(void) {
  printf("test_http_core\n");
  printf("==============\n");

  test_create_free();
  test_register_route();
  test_set_default();
  test_get_stats();
  test_route_table_full();
  test_null_safety();

  printf("==============\n");
  printf("%d/%d passed\n", g_passed, g_count);
  return (g_passed == g_count) ? 0 : 1;
}
