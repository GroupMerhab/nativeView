/*
 * Copyright (c) 2026
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 */

#include "url_parser.h"
#include <stdio.h>
#include <string.h>

static int g_count, g_passed;

static void ok(const char* name) { g_count++; g_passed++; printf("  [OK] %s\n", name); }
static void fail(const char* name, const char* r) {
  g_count++; printf("  [FAIL] %s: %s\n", name, r);
}

static void test_parse_simple(void) {
  ParsedUrl u;
  int r = url_parse("/path/to/resource", &u);
  if (r != URL_OK) { fail("parse_simple", "expected URL_OK"); return; }
  if (strcmp(u.path, "/path/to/resource") != 0)
    fail("parse_simple", "path mismatch");
  else if (u.param_count != 0)
    fail("parse_simple", "expected 0 params");
  else if (u.fragment[0] != '\0')
    fail("parse_simple", "expected empty fragment");
  else
    ok("parse_simple");
}

static void test_parse_query(void) {
  ParsedUrl u;
  int r = url_parse("/api?foo=bar&baz=qux", &u);
  if (r != URL_OK) { fail("parse_query", "parse failed"); return; }
  if (strcmp(u.path, "/api") != 0) { fail("parse_query", "path"); return; }
  if (u.param_count != 2) { fail("parse_query", "param count"); return; }
  const char* v = url_get_param(&u, "foo");
  if (!v || strcmp(v, "bar") != 0) { fail("parse_query", "foo"); return; }
  v = url_get_param(&u, "baz");
  if (!v || strcmp(v, "qux") != 0) { fail("parse_query", "baz"); return; }
  ok("parse_query");
}

static void test_parse_fragment(void) {
  ParsedUrl u;
  int r = url_parse("/page#section1", &u);
  if (r != URL_OK) { fail("parse_fragment", "parse failed"); return; }
  if (strcmp(u.path, "/page") != 0) { fail("parse_fragment", "path"); return; }
  if (strcmp(u.fragment, "section1") != 0) { fail("parse_fragment", "fragment"); return; }
  ok("parse_fragment");
}

static void test_parse_query_and_fragment(void) {
  ParsedUrl u;
  int r = url_parse("/x?a=1#anchor", &u);
  if (r != URL_OK) { fail("parse_qf", "parse failed"); return; }
  if (strcmp(u.path, "/x") != 0) { fail("parse_qf", "path"); return; }
  if (url_get_param(&u, "a") == NULL || strcmp(url_get_param(&u, "a"), "1") != 0)
    fail("parse_qf", "param a");
  else if (strcmp(u.fragment, "anchor") != 0)
    fail("parse_qf", "fragment");
  else
    ok("parse_query_and_fragment");
}

static void test_decode(void) {
  char out[64];
  int n = url_decode("hello%20world", out, sizeof(out));
  if (n < 0) { fail("decode", "returned -1"); return; }
  if (strcmp(out, "hello world") != 0) { fail("decode", "space"); return; }

  url_decode("%2B%2F", out, sizeof(out));
  if (strcmp(out, "+/") != 0) { fail("decode", "plus slash"); return; }

  url_decode("a%2Bb%3Dc", out, sizeof(out));
  if (strcmp(out, "a+b=c") != 0) { fail("decode", "plus equals"); return; }

  url_decode("+plus", out, sizeof(out));
  if (strcmp(out, " plus") != 0) { fail("decode", "plus sign"); return; }

  ok("url_decode");
}

static void test_encode(void) {
  char out[128];
  int n = url_encode("hello world", out, sizeof(out));
  if (n < 0) { fail("encode", "returned -1"); return; }
  if (strcmp(out, "hello%20world") != 0) { fail("encode", "space"); return; }

  url_encode("a+b=c", out, sizeof(out));
  if (strcmp(out, "a%2Bb%3Dc") != 0) { fail("encode", "plus equals"); return; }

  url_encode("safe-._~", out, sizeof(out));
  if (strcmp(out, "safe-._~") != 0) { fail("encode", "unreserved"); return; }

  ok("url_encode");
}

static void test_get_param_by_index(void) {
  ParsedUrl u;
  url_parse("/?x=1&y=2", &u);
  const char* k, * v;
  if (!url_get_param_by_index(&u, 0, &k, &v)) { fail("get_by_idx", "idx 0"); return; }
  if (strcmp(k, "x") != 0 || strcmp(v, "1") != 0) { fail("get_by_idx", "vals 0"); return; }
  if (!url_get_param_by_index(&u, 1, &k, &v)) { fail("get_by_idx", "idx 1"); return; }
  if (strcmp(k, "y") != 0 || strcmp(v, "2") != 0) { fail("get_by_idx", "vals 1"); return; }
  if (url_get_param_by_index(&u, 2, &k, &v)) { fail("get_by_idx", "idx 2 should fail"); return; }
  ok("get_param_by_index");
}

static void test_null_safety(void) {
  ParsedUrl u;
  memset(&u, 0, sizeof(u));
  if (url_parse(NULL, &u) != URL_ERROR_INVALID) fail("parse_null_url", "expected invalid");
  else ok("parse_null_url");
  if (url_parse("/", NULL) != URL_ERROR_INVALID) fail("parse_null_parsed", "expected invalid");
  else ok("parse_null_parsed");
  if (url_get_param(NULL, "x") != NULL) fail("get_param_null", "expected NULL");
  else ok("get_param_null");
  if (url_get_param(&u, NULL) != NULL) fail("get_param_null_key", "expected NULL");
  else ok("get_param_null_key");
  if (url_decode(NULL, (char[1]){0}, 1) != -1) fail("decode_null", "expected -1");
  else ok("decode_null");
  if (url_encode(NULL, (char[1]){0}, 1) != -1) fail("encode_null", "expected -1");
  else ok("encode_null");
}

static void test_invalid(void) {
  ParsedUrl u;
  char buf[2100];
  memset(buf, 'x', sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';
  if (url_parse(buf, &u) != URL_ERROR_PATH_TOO_LONG)
    fail("path_too_long", "expected URL_ERROR_PATH_TOO_LONG");
  else
    ok("path_too_long");
}

int main(void) {
  printf("test_url_parser\n");
  printf("===============\n");

  test_parse_simple();
  test_parse_query();
  test_parse_fragment();
  test_parse_query_and_fragment();
  test_decode();
  test_encode();
  test_get_param_by_index();
  test_null_safety();
  test_invalid();

  printf("===============\n");
  printf("%d/%d passed\n", g_passed, g_count);
  return (g_passed == g_count) ? 0 : 1;
}
