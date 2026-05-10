/*
 * Copyright (c) 2026
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 */

/**
 * @file http_protocol.h
 * @brief HTTP request parsing and response building.
 *
 * State-machine style parsing, arena-based allocation.
 * Zero-copy where possible.
 */

#ifndef HTTP_PROTOCOL_H
#define HTTP_PROTOCOL_H

#include <stddef.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration for arena */
struct nv_arena;
typedef struct nv_arena nv_arena_t;

/* =============================================================================
 * Error Codes
 * ============================================================================= */

#define HTTP_OK              0
#define HTTP_ERROR_INVALID   -1
#define HTTP_ERROR_INCOMPLETE -2

/* =============================================================================
 * HTTP Header
 * ============================================================================= */

typedef struct {
  char name[256];
  char value[4096];
} HttpHeader;

/* =============================================================================
 * HTTP Request
 * ============================================================================= */

typedef struct {
  char method[16];
  char path[2048];
  char query[4096];
  int http_version_major;
  int http_version_minor;

  HttpHeader headers[32];
  int header_count;

  char* body;
  size_t body_len;

  struct {
    char content_type[256];
    size_t content_length;
    int keep_alive;
  } parsed;
} HttpRequest;

/* =============================================================================
 * HTTP Response
 * ============================================================================= */

typedef struct {
  int status_code;
  char headers[4096];
  size_t headers_len;
  char* body;
  size_t body_len;
  int body_is_file;
  char body_file_path[1024];
  struct {
    char content_type[256];
    time_t last_modified;
    int cache_seconds;
  } metadata;
} HttpResponse;

/* =============================================================================
 * Request Parsing
 * ============================================================================= */

/**
 * Parse HTTP request from buffer.
 * Body is allocated from arena. req can be stack-allocated.
 *
 * @param arena  Arena for body allocation (can be NULL if no body)
 * @param buffer Input buffer (not necessarily NUL-terminated)
 * @param len    Buffer length
 * @param req    Output request (must not be NULL)
 * @return HTTP_OK, HTTP_ERROR_INVALID, or HTTP_ERROR_INCOMPLETE
 */
int http_request_parse(nv_arena_t* arena, const char* buffer, size_t len,
    HttpRequest* req);

/**
 * Get header value by name (case-insensitive).
 * @return Pointer to value or NULL if not found
 */
const char* http_request_get_header(const HttpRequest* req, const char* name);

/**
 * Parse query string into buf as "key=value" pairs (for simple lookups).
 * @return HTTP_OK or HTTP_ERROR_INVALID
 */
int http_request_parse_query(const char* query, char* buf, size_t buf_size);

/* =============================================================================
 * Response Building
 * ============================================================================= */

/**
 * Create HTTP response (allocated from arena).
 * @return HttpResponse* or NULL on failure
 */
HttpResponse* http_response_create(nv_arena_t* arena, int status);

/**
 * Add header to response.
 */
void http_response_set_header(HttpResponse* resp, const char* name,
    const char* value);

/**
 * Set response body. Allocates from arena and copies body.
 */
void http_response_set_body(nv_arena_t* arena, HttpResponse* resp,
    const void* body, size_t len, const char* content_type);

/**
 * Set response body from arena-allocated buffer (no copy).
 * Caller guarantees body stays valid for response lifetime.
 */
void http_response_set_body_ref(HttpResponse* resp, const void* body,
    size_t len, const char* content_type);

int http_response_set_body_file(HttpResponse* resp, const char* file_path,
    size_t len, const char* content_type);

/**
 * Serialize full response to wire format (headers + body).
 * Use for small responses only.
 * @return Bytes written, or -1 on error (buffer too small)
 */
int http_response_serialize(const HttpResponse* resp, char* buf, size_t buf_size);

/**
 * Serialize response headers only (status, Content-Type, Content-Length, etc.).
 * Caller then sends body separately if body_len > 0.
 * @return Bytes written, or -1 on error (buffer too small)
 */
int http_response_serialize_headers(const HttpResponse* resp, char* buf,
    size_t buf_size);

/**
 * Clear response pointer (memory is in arena; caller destroys arena).
 */
void http_response_free(HttpResponse** resp);

/**
 * Get status line text for status code.
 */
const char* http_status_text(int code);

#ifdef __cplusplus
}
#endif

#endif /* HTTP_PROTOCOL_H */
