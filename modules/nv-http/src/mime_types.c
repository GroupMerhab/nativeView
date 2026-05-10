/*
 * Copyright (c) 2026
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 */

#include "mime_types.h"
#include <string.h>

static int ext_match(const char* ext, const char* pattern) {
  while (*ext && *pattern) {
    int a = (unsigned char)*ext;
    int b = (unsigned char)*pattern;
    if (a >= 'A' && a <= 'Z') a += 32;
    if (b >= 'A' && b <= 'Z') b += 32;
    if (a != b) return 0;
    ext++;
    pattern++;
  }
  return *ext == '\0' && *pattern == '\0';
}

static const struct {
  const char* ext;
  const char* mime;
} TABLE[] = {
  {".html", "text/html; charset=utf-8"},
  {".htm", "text/html; charset=utf-8"},
  {".css", "text/css; charset=utf-8"},
  {".js", "application/javascript; charset=utf-8"},
  {".json", "application/json"},
  {".xml", "application/xml"},
  {".txt", "text/plain; charset=utf-8"},
  {".png", "image/png"},
  {".jpg", "image/jpeg"},
  {".jpeg", "image/jpeg"},
  {".gif", "image/gif"},
  {".svg", "image/svg+xml"},
  {".ico", "image/x-icon"},
  {".webp", "image/webp"},
  {".woff", "font/woff"},
  {".woff2", "font/woff2"},
  {".ttf", "font/ttf"},
  {".otf", "font/otf"},
  {".pdf", "application/pdf"},
  {".zip", "application/zip"},
  {".gz", "application/gzip"},
  {".gzip", "application/gzip"},
  {NULL, "application/octet-stream"},
};

const char* mime_type_from_filename(const char* filename) {
  if (!filename) return "application/octet-stream";
  const char* ext = strrchr(filename, '.');
  if (!ext) return "application/octet-stream";
  for (size_t i = 0; TABLE[i].ext; i++) {
    if (ext_match(ext, TABLE[i].ext)) return TABLE[i].mime;
  }
  return "application/octet-stream";
}
