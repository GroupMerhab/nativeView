/*
 * Copyright (c) 2026
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 */

#include "http_protocol.h"
#include "nv_arena.h"
#include <stdio.h>
#include <string.h>

static int g_count, g_passed;

static void ok(const char* name) { g_count++; g_passed++; printf("  [OK] %s\n", name); }
static void fail(const char* name, const char* r) { g_count++; printf("  [FAIL] %s: %s\n", name, r); }

static void test_status_text(void) {
  if (!http_status_text(200) || strcmp(http_status_text(200), "OK") != 0)
    fail("status_200", "expected OK");
  else ok("status_200");
  if (strcmp(http_status_text(404), "Not Found") != 0)
    fail("status_404", "expected Not Found");
  else ok("status_404");
}

static void test_request_parse(void) {
  const char* raw = "GET /path HTTP/1.1\r\nHost: localhost\r\n\r\n";
  HttpRequest r;
  nv_arena_t* a = nv_arena_create(4096);
  if (!a) { fail("parse_get", "arena create failed"); return; }

  int err = http_request_parse(a, raw, strlen(raw), &r);
  if (err != HTTP_OK) {
    fail("parse_get", "expected HTTP_OK");
    nv_arena_destroy(a);
    return;
  }
  if (strcmp(r.method, "GET") != 0 || strcmp(r.path, "/path") != 0 ||
      r.http_version_major != 1 || r.http_version_minor != 1) {
    fail("parse_get", "method/path/version mismatch");
    nv_arena_destroy(a);
    return;
  }
  ok("parse_get");

  const char* host = http_request_get_header(&r, "Host");
  if (!host || strcmp(host, "localhost") != 0)
    fail("get_header", "expected Host: localhost");
  else ok("get_header");
  nv_arena_destroy(a);
}

static void test_request_incomplete(void) {
  const char* req = "GET / HTTP/1.1\r\n";
  HttpRequest r;
  int err = http_request_parse(NULL, req, strlen(req), &r);
  if (err != HTTP_ERROR_INCOMPLETE)
    fail("incomplete", "expected HTTP_ERROR_INCOMPLETE");
  else ok("incomplete");
}

static void test_response_create_serialize(void) {
  nv_arena_t* a = nv_arena_create(32768);
  if (!a) { fail("resp_create", "arena create"); return; }

  HttpResponse* resp = http_response_create(a, 200);
  if (!resp) { fail("resp_create", "create failed"); nv_arena_destroy(a); return; }
  ok("resp_create");

  http_response_set_body(a, resp, "hello", 5, "text/plain");
  char buf[1024];
  int n = http_response_serialize(resp, buf, sizeof(buf));
  if (n <= 0) {
    fail("resp_serialize", "serialize failed");
    nv_arena_destroy(a);
    return;
  }
  if (strstr(buf, "HTTP/1.1 200") == NULL || strstr(buf, "Content-Length: 5") == NULL ||
      strstr(buf, "Content-Type: text/plain") == NULL)
    fail("resp_serialize", "missing headers");
  else ok("resp_serialize");

  http_response_free(&resp);
  if (resp != NULL) fail("resp_free", "pointer not cleared");
  else ok("resp_free");

  resp = http_response_create(a, 200);
  http_response_set_body(a, resp, "x", 1, "text/plain");
  int hdr_len = http_response_serialize_headers(resp, buf, sizeof(buf));
  if (hdr_len <= 0 || (size_t)hdr_len >= sizeof(buf))
    fail("serialize_headers", "invalid length");
  else if (strstr(buf, "HTTP/1.1 200") == NULL ||
           strstr(buf, "Content-Length: 1") == NULL)
    fail("serialize_headers", "missing headers");
  else
    ok("serialize_headers");

  nv_arena_destroy(a);
}

static void test_null_safety(void) {
  http_request_get_header(NULL, "x");
  ok("get_header_null_req");
  HttpRequest r;
  memset(&r, 0, sizeof(r));
  if (http_request_get_header(&r, NULL) != NULL)
    fail("get_header_null_name", "expected NULL");
  else ok("get_header_null_name");
  http_response_set_header(NULL, "x", "y");
  http_response_set_body(NULL, NULL, NULL, 0, NULL);
  http_response_free(NULL);
  ok("response_null_safety");
}

int main(void) {
  printf("test_http_protocol\n");
  printf("==================\n");

  test_status_text();
  test_request_parse();
  test_request_incomplete();
  test_response_create_serialize();
  test_null_safety();

  printf("==================\n");
  printf("%d/%d passed\n", g_passed, g_count);
  return (g_passed == g_count) ? 0 : 1;
}
