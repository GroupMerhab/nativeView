/*
 * Copyright (c) 2026
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 */

#include "nv_base64.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int tests_passed;
static int tests_failed;

static void ok(const char* n) {
  printf("✓ %s\n", n);
  tests_passed++;
}
static void fail(const char* n, const char* w) {
  printf("✗ %s: %s\n", n, w);
  tests_failed++;
}

static void test_bound_empty(void) {
  if (nv_base64_encode_bound(0) != 1u) {
    fail("encode_bound(0)", "expected 1");
    return;
  }
  ok("encode_bound(0)");
}

static void test_known_vector(void) {
  static const uint8_t raw[] = {'f', 'o', 'o'};
  char buf[32];
  int n = nv_base64_encode(raw, sizeof(raw), buf, sizeof(buf));
  if (n < 0) {
    fail("encode known", "returned error");
    return;
  }
  if (strcmp(buf, "Zm9v") != 0) {
    fail("encode known", "unexpected string");
    return;
  }
  ok("encode known vector (foo)");
}

static void test_roundtrip(void) {
  uint8_t src[256];
  char enc[400];
  uint8_t dec[300];
  size_t k;
  size_t dlen = 0;
  int encn;
  for (k = 0; k < sizeof(src); k++) src[k] = (uint8_t)(k * 17u + 3u);
  encn = nv_base64_encode(src, sizeof(src), enc, sizeof(enc));
  if (encn < 0) {
    fail("roundtrip encode", "failed");
    return;
  }
  if (nv_base64_decode(enc, (size_t)encn, dec, sizeof(dec), &dlen) != 0) {
    fail("roundtrip decode", "failed");
    return;
  }
  if (dlen != sizeof(src) || memcmp(src, dec, sizeof(src)) != 0) {
    fail("roundtrip cmp", "mismatch");
    return;
  }
  ok("roundtrip 256 bytes");
}

static void test_decode_ws(void) {
  const char* s = " SGVsbG8= \n";
  uint8_t out[16];
  size_t dlen = 0;
  if (nv_base64_decode(s, strlen(s), out, sizeof(out), &dlen) != 0) {
    fail("decode ws", "decode error");
    return;
  }
  if (dlen != 5u || memcmp(out, "Hello", 5) != 0) {
    fail("decode ws", "payload");
    return;
  }
  ok("decode ignores whitespace");
}

static void test_null_safe(void) {
  char enc_dst[8];
  uint8_t dec_dst[4];
  if (nv_base64_encode(NULL, 1, enc_dst, sizeof(enc_dst)) != -1) {
    fail("encode null src", "expected -1");
    return;
  }
  ok("encode rejects null src with len>0");
  if (nv_base64_encode((const uint8_t*)"", 0, NULL, 4) != -1) {
    fail("encode null dst", "expected -1");
    return;
  }
  ok("encode rejects null dst");
  if (nv_base64_decode(NULL, 0, dec_dst, sizeof(dec_dst), NULL) != 0) {
    fail("decode null empty", "expected 0");
    return;
  }
  ok("decode null ptr zero len");
}

int main(void) {
  printf("=== nv-core base64 tests ===\n\n");
  test_bound_empty();
  test_known_vector();
  test_roundtrip();
  test_decode_ws();
  test_null_safe();
  printf("\nPassed: %d  Failed: %d\n", tests_passed, tests_failed);
  return tests_failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
