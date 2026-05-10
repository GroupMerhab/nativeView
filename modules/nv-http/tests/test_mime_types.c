/*
 * Copyright (c) 2026
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 */

#include "mime_types.h"
#include <stdio.h>
#include <string.h>

static int g_count, g_passed;

static void ok(const char* name) { g_count++; g_passed++; printf("  [OK] %s\n", name); }
static void fail(const char* name, const char* r) {
  g_count++; printf("  [FAIL] %s: %s\n", name, r);
}

static void check(const char* name, const char* got, const char* want) {
  if (strcmp(got, want) == 0) ok(name);
  else { char buf[256]; snprintf(buf, sizeof(buf), "got %s", got); fail(name, buf); }
}

static void test_html(void) {
  check("html", mime_type_from_filename("index.html"),
      "text/html; charset=utf-8");
  check("htm", mime_type_from_filename("page.htm"),
      "text/html; charset=utf-8");
}

static void test_css_js(void) {
  check("css", mime_type_from_filename("/path/style.css"),
      "text/css; charset=utf-8");
  check("js", mime_type_from_filename("app.js"), "application/javascript; charset=utf-8");
}

static void test_images(void) {
  check("png", mime_type_from_filename("img.PNG"), "image/png");
  check("jpg", mime_type_from_filename("photo.jpg"), "image/jpeg");
  check("jpeg", mime_type_from_filename("photo.jpeg"), "image/jpeg");
  check("webp", mime_type_from_filename("x.webp"), "image/webp");
}

static void test_fonts(void) {
  check("woff2", mime_type_from_filename("font.woff2"), "font/woff2");
  check("ttf", mime_type_from_filename("font.ttf"), "font/ttf");
}

static void test_unknown(void) {
  check("no_ext", mime_type_from_filename("README"),
      "application/octet-stream");
  check("unknown_ext", mime_type_from_filename("file.xyz"),
      "application/octet-stream");
  check("null", mime_type_from_filename(NULL),
      "application/octet-stream");
}

int main(void) {
  printf("test_mime_types\n");
  printf("===============\n");

  test_html();
  test_css_js();
  test_images();
  test_fonts();
  test_unknown();

  printf("===============\n");
  printf("%d/%d passed\n", g_passed, g_count);
  return (g_passed == g_count) ? 0 : 1;
}
