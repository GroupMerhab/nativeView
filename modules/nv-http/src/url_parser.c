/*
 * Copyright (c) 2026
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 */

#include "url_parser.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define QUERY_BUF_SZ 4096
#define MAX_PARAMS 32

static void parse_query_string(const char* query, QueryParam* params,
    int* param_count, int max_params) {
  *param_count = 0;
  if (!query || !params || max_params <= 0) return;

  char buf[QUERY_BUF_SZ];
  size_t qlen = strlen(query);
  if (qlen >= sizeof(buf)) qlen = sizeof(buf) - 1;
  memcpy(buf, query, qlen);
  buf[qlen] = '\0';

  char* cur = buf;
  char* end = buf + qlen;
  while (cur < end && *param_count < max_params) {
    char* amp = strchr(cur, '&');
    if (amp && amp >= end) amp = NULL;
    if (amp) *amp = '\0';

    char* eq = strchr(cur, '=');
    char* key = cur;
    char* val = "";
    if (eq) {
      *eq = '\0';
      val = eq + 1;
    }

    char dkey[256], dval[4096];
    url_decode(key, dkey, sizeof(dkey));
    url_decode(val, dval, sizeof(dval));

    strncpy(params[*param_count].key, dkey, sizeof(params[0].key) - 1);
    params[*param_count].key[sizeof(params[0].key) - 1] = '\0';
    strncpy(params[*param_count].value, dval, sizeof(params[0].value) - 1);
    params[*param_count].value[sizeof(params[0].value) - 1] = '\0';

    (*param_count)++;
    cur = amp ? amp + 1 : end;
  }
}

int url_parse(const char* url, ParsedUrl* parsed) {
  if (!url || !parsed) return URL_ERROR_INVALID;

  memset(parsed, 0, sizeof(*parsed));

  const char* q = strchr(url, '?');
  const char* f = strchr(url, '#');

  size_t path_len;
  if (q) path_len = (size_t)(q - url);
  else if (f) path_len = (size_t)(f - url);
  else path_len = strlen(url);

  if (path_len >= sizeof(parsed->path)) return URL_ERROR_PATH_TOO_LONG;

  memcpy(parsed->path, url, path_len);
  parsed->path[path_len] = '\0';

  if (q) {
    const char* q_start = q + 1;
    const char* q_end = f ? f : url + strlen(url);
    size_t qlen = (size_t)(q_end - q_start);
    if (qlen > 0 && qlen < QUERY_BUF_SZ) {
      char qbuf[QUERY_BUF_SZ];
      memcpy(qbuf, q_start, qlen);
      qbuf[qlen] = '\0';
      parse_query_string(qbuf, parsed->params, &parsed->param_count,
          (int)(sizeof(parsed->params) / sizeof(parsed->params[0])));
    }
  }

  if (f) {
    size_t flen = strlen(f + 1);
    if (flen >= sizeof(parsed->fragment)) flen = sizeof(parsed->fragment) - 1;
    memcpy(parsed->fragment, f + 1, flen);
    parsed->fragment[flen] = '\0';
  }

  return URL_OK;
}

const char* url_get_param(const ParsedUrl* parsed, const char* key) {
  if (!parsed || !key) return NULL;
  for (int i = 0; i < parsed->param_count; i++) {
    if (strcmp(parsed->params[i].key, key) == 0)
      return parsed->params[i].value;
  }
  return NULL;
}

int url_get_param_by_index(const ParsedUrl* parsed, int index,
    const char** out_key, const char** out_value) {
  if (!parsed || index < 0 || index >= parsed->param_count) return 0;
  if (out_key) *out_key = parsed->params[index].key;
  if (out_value) *out_value = parsed->params[index].value;
  return 1;
}

int url_decode(const char* encoded, char* decoded, size_t max_len) {
  if (!encoded || !decoded || max_len == 0) return -1;

  size_t len = 0;
  for (const char* p = encoded; *p && len < max_len - 1; p++) {
    if (*p == '%' && p[1] && p[2]) {
      char hex[3] = { p[1], p[2], '\0' };
      char* end = NULL;
      long v = strtol(hex, &end, 16);
      if (end && *end == '\0' && v >= 0 && v <= 255) {
        decoded[len++] = (char)(unsigned char)v;
        p += 2;
      } else {
        decoded[len++] = *p;
      }
    } else if (*p == '+') {
      decoded[len++] = ' ';
    } else {
      decoded[len++] = *p;
    }
  }
  decoded[len] = '\0';
  return (int)len;
}

int url_encode(const char* plain, char* encoded, size_t max_len) {
  if (!plain || !encoded || max_len == 0) return -1;

  size_t len = 0;
  for (const char* p = plain; *p && len < max_len - 1; p++) {
    unsigned char c = (unsigned char)*p;
    if (isalnum(c) || c == '-' || c == '.' || c == '_' || c == '~') {
      encoded[len++] = (char)c;
    } else {
      if (len + 3 > max_len) break;
      int n = snprintf(encoded + len, max_len - len, "%%%02X", c);
      if (n > 0) len += (size_t)n;
    }
  }
  encoded[len] = '\0';
  return (int)len;
}
