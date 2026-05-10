/*
 * Copyright (c) 2026
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 */

#include "http_protocol.h"
#include "nv_arena.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int strcasecmp_c(const char* a, const char* b) {
  for (;;) {
    int ca = (unsigned char)*a++;
    int cb = (unsigned char)*b++;
    if (ca >= 'A' && ca <= 'Z') ca += 32;
    if (cb >= 'A' && cb <= 'Z') cb += 32;
    if (ca != cb || ca == '\0') return ca - cb;
  }
}

const char* http_status_text(int code) {
  switch (code) {
    case 200: return "OK";
    case 201: return "Created";
    case 204: return "No Content";
    case 301: return "Moved Permanently";
    case 302: return "Found";
    case 304: return "Not Modified";
    case 400: return "Bad Request";
    case 401: return "Unauthorized";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 500: return "Internal Server Error";
    case 501: return "Not Implemented";
    case 503: return "Service Unavailable";
    default:  return "Unknown";
  }
}

const char* http_request_get_header(const HttpRequest* req, const char* name) {
  if (!req || !name) return NULL;
  for (int i = 0; i < req->header_count; i++) {
    if (strcasecmp_c(req->headers[i].name, name) == 0)
      return req->headers[i].value;
  }
  return NULL;
}

int http_request_parse(nv_arena_t* arena, const char* buffer, size_t len,
    HttpRequest* req) {
  if (!buffer || !req || len == 0) return HTTP_ERROR_INVALID;
  memset(req, 0, sizeof(*req));

  const char* body_start = NULL;
  for (size_t i = 0; i + 4 <= len; i++) {
    if (buffer[i] == '\r' && buffer[i+1] == '\n' &&
        buffer[i+2] == '\r' && buffer[i+3] == '\n') {
      body_start = buffer + i;
      break;
    }
  }
  if (!body_start) return HTTP_ERROR_INCOMPLETE;
  size_t header_len = (size_t)(body_start - buffer);
  body_start += 4;

  const char* line_end = NULL;
  for (size_t i = 0; i + 1 < header_len; i++) {
    if (buffer[i] == '\r' && buffer[i+1] == '\n') { line_end = buffer + i; break; }
  }
  if (!line_end) return HTTP_ERROR_INVALID;

  char req_line[2080];
  size_t rl = (size_t)(line_end - buffer);
  if (rl >= sizeof(req_line)) return HTTP_ERROR_INVALID;
  memcpy(req_line, buffer, rl);
  req_line[rl] = '\0';

  char method[16], path[2048];
  int maj, min;
  if (sscanf(req_line, "%15s %2047s HTTP/%d.%d", method, path, &maj, &min) != 4)
    return HTTP_ERROR_INVALID;

  if (strcmp(method, "GET") != 0 && strcmp(method, "POST") != 0 &&
      strcmp(method, "PUT") != 0 && strcmp(method, "DELETE") != 0 &&
      strcmp(method, "HEAD") != 0)
    return HTTP_ERROR_INVALID;

  if (path[0] != '/') return HTTP_ERROR_INVALID;

  strncpy(req->method, method, sizeof(req->method) - 1);
  req->http_version_major = maj;
  req->http_version_minor = min;

  char* q = strchr(path, '?');
  if (q) {
    *q = '\0';
    strncpy(req->query, q + 1, sizeof(req->query) - 1);
  }
  strncpy(req->path, path, sizeof(req->path) - 1);

  const char* cur = line_end + 2;
  const char* hdr_end = buffer + header_len;
  while (cur < hdr_end && req->header_count < 32) {
    size_t rem = (size_t)(hdr_end - cur);
    if (rem < 2) break;
    /* Include the \r that ends this line (may be at hdr_end) */
    const char* nl = (const char*)memchr(cur, '\r', rem + 1);
    if (!nl || nl + 1 >= buffer + len || nl[1] != '\n') break;

    size_t line_len = (size_t)(nl - cur);
    if (line_len == 0) { cur = nl + 2; continue; }
    const char* colon = (const char*)memchr(cur, ':', line_len);
    if (!colon) { cur = nl + 2; continue; }

    const char* nstart = cur;
    while (nstart < colon && (*nstart == ' ' || *nstart == '\t')) nstart++;
    const char* nend = colon;
    while (nend > nstart && (nend[-1] == ' ' || nend[-1] == '\t')) nend--;
    size_t nlen = (size_t)(nend - nstart);
    const char* vstart = colon + 1;
    while (vstart < nl && (*vstart == ' ' || *vstart == '\t')) vstart++;
    const char* vend = nl;
    while (vend > vstart && (vend[-1] == ' ' || vend[-1] == '\t' ||
        vend[-1] == '\r' || vend[-1] == '\n')) vend--;
    size_t vlen = (size_t)(vend - vstart);

    if (nlen >= sizeof(req->headers[0].name) ||
        vlen >= sizeof(req->headers[0].value)) {
      cur = nl + 2;
      continue;
    }
    memcpy(req->headers[req->header_count].name, nstart, nlen);
    req->headers[req->header_count].name[nlen] = '\0';
    memcpy(req->headers[req->header_count].value, vstart, vlen);
    req->headers[req->header_count].value[vlen] = '\0';
    req->header_count++;
    cur = nl + 2;
  }

  const char* cl = http_request_get_header(req, "Content-Length");
  if (cl) req->parsed.content_length = (size_t)atoll(cl);

  size_t body_len = len - (size_t)(body_start - buffer);
  if (req->parsed.content_length > body_len) return HTTP_ERROR_INCOMPLETE;

  if (req->parsed.content_length > 0 && arena) {
    char* body = nv_arena_alloc(arena, req->parsed.content_length);
    if (body) {
      memcpy(body, body_start, req->parsed.content_length);
      req->body = body;
      req->body_len = req->parsed.content_length;
    }
  }

  const char* ct = http_request_get_header(req, "Content-Type");
  if (ct) strncpy(req->parsed.content_type, ct, sizeof(req->parsed.content_type) - 1);
  req->parsed.keep_alive = 1;
  return HTTP_OK;
}

int http_request_parse_query(const char* query, char* buf, size_t buf_size) {
  if (!query || !buf || buf_size == 0) return HTTP_ERROR_INVALID;
  size_t n = strlen(query);
  if (n >= buf_size) return HTTP_ERROR_INVALID;
  memcpy(buf, query, n + 1);
  return HTTP_OK;
}

HttpResponse* http_response_create(nv_arena_t* arena, int status) {
  if (!arena) return NULL;
  HttpResponse* r = nv_arena_alloc(arena, sizeof(HttpResponse));
  if (!r) return NULL;
  memset(r, 0, sizeof(*r));
  r->status_code = status;
  strcpy(r->metadata.content_type, "application/octet-stream");
  return r;
}

void http_response_set_header(HttpResponse* resp, const char* name,
    const char* value) {
  if (!resp || !name || !value) return;
  size_t cap = sizeof(resp->headers) - resp->headers_len;
  if (cap < 5) return;
  int n = snprintf(resp->headers + resp->headers_len, cap,
      "%s: %s\r\n", name, value);
  if (n > 0 && (size_t)n < cap) resp->headers_len += (size_t)n;
}

void http_response_set_body(nv_arena_t* arena, HttpResponse* resp,
    const void* body, size_t len, const char* content_type) {
  if (!resp) return;
  resp->body_is_file = 0;
  resp->body_file_path[0] = '\0';
  resp->body_len = len;
  if (len > 0 && arena && body) {
    char* buf = nv_arena_alloc(arena, len);
    if (buf) {
      memcpy(buf, body, len);
      resp->body = buf;
    }
  } else {
    resp->body = NULL;
  }
  if (content_type)
    strncpy(resp->metadata.content_type, content_type,
        sizeof(resp->metadata.content_type) - 1);
  else
    strcpy(resp->metadata.content_type, "application/octet-stream");
}

void http_response_set_body_ref(HttpResponse* resp, const void* body,
    size_t len, const char* content_type) {
  if (!resp) return;
  resp->body_is_file = 0;
  resp->body_file_path[0] = '\0';
  resp->body_len = len;
  resp->body = len > 0 && body ? (char*)body : NULL;
  if (content_type)
    strncpy(resp->metadata.content_type, content_type,
        sizeof(resp->metadata.content_type) - 1);
  else
    strcpy(resp->metadata.content_type, "application/octet-stream");
}

int http_response_set_body_file(HttpResponse* resp, const char* file_path,
    size_t len, const char* content_type) {
  if (!resp || !file_path || !file_path[0]) return HTTP_ERROR_INVALID;
  size_t n = strlen(file_path);
  if (n >= sizeof(resp->body_file_path)) return HTTP_ERROR_INVALID;
  resp->body_is_file = 1;
  memcpy(resp->body_file_path, file_path, n + 1);
  resp->body = NULL;
  resp->body_len = len;
  if (content_type)
    strncpy(resp->metadata.content_type, content_type,
        sizeof(resp->metadata.content_type) - 1);
  else
    strcpy(resp->metadata.content_type, "application/octet-stream");
  return HTTP_OK;
}

int http_response_serialize(const HttpResponse* resp, char* buf, size_t buf_size) {
  if (!resp || !buf || buf_size < 64) return -1;

  const char* status_text = http_status_text(resp->status_code);
  int n = snprintf(buf, buf_size,
      "HTTP/1.1 %d %s\r\n"
      "Content-Type: %s\r\n"
      "Content-Length: %zu\r\n"
      "Connection: close\r\n"
      "Server: nv-http/1.0\r\n",
      resp->status_code, status_text,
      resp->metadata.content_type,
      resp->body_len);

  if (n < 0 || (size_t)n >= buf_size) return -1;
  size_t written = (size_t)n;

  if (resp->headers_len > 0 && written + resp->headers_len < buf_size) {
    memcpy(buf + written, resp->headers, resp->headers_len);
    written += resp->headers_len;
  }

  if (written + 2 >= buf_size) return -1;
  memcpy(buf + written, "\r\n", 2);
  written += 2;

  if (resp->body_len > 0) {
    if (!resp->body) return -1;
    if (written + resp->body_len > buf_size) return -1;
    memcpy(buf + written, resp->body, resp->body_len);
    written += resp->body_len;
  }
  return (int)written;
}

int http_response_serialize_headers(const HttpResponse* resp, char* buf,
    size_t buf_size) {
  if (!resp || !buf || buf_size < 64) return -1;
  const char* status_text = http_status_text(resp->status_code);
  int n = snprintf(buf, buf_size,
      "HTTP/1.1 %d %s\r\n"
      "Content-Type: %s\r\n"
      "Content-Length: %zu\r\n"
      "Connection: close\r\n"
      "Server: nv-http/1.0\r\n",
      resp->status_code, status_text,
      resp->metadata.content_type,
      resp->body_len);
  if (n < 0 || (size_t)n >= buf_size) return -1;
  size_t written = (size_t)n;
  if (resp->headers_len > 0 && written + resp->headers_len < buf_size) {
    memcpy(buf + written, resp->headers, resp->headers_len);
    written += resp->headers_len;
  }
  if (written + 2 >= buf_size) return -1;
  memcpy(buf + written, "\r\n", 2);
  return (int)(written + 2);
}

void http_response_free(HttpResponse** resp) {
  if (resp) *resp = NULL;
}
