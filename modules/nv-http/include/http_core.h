/*
 * Copyright (c) 2026
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 */

/**
 * @file http_core.h
 * @brief HTTP server core: routing, dispatch, request loop.
 *
 * Routes requests to handlers by method and path.
 * Manages server lifecycle and connection handling.
 *
 * IMPORTANT: The HTTP server is single-threaded and blocking by design. The
 * request loop processes one connection at a time and is intended for simple,
 * lightweight use (e.g. local/dev serving), not high-concurrency production
 * workloads.
 */

#ifndef HTTP_CORE_H
#define HTTP_CORE_H

#include "http_protocol.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum routes per `HttpCore` (fixed-size table). Excess registrations fail with HTTP_ERROR_INVALID. */
#define HTTP_CORE_MAX_ROUTES 32

/* Forward */
struct ServerSocket;
typedef struct ServerSocket ServerSocket;

/* =============================================================================
 * Types
 * ============================================================================= */

typedef struct HttpCore HttpCore;

/** Route handler: receives arena, request, response. Arena for body allocation. */
typedef void (*HttpHandler)(nv_arena_t* arena, const HttpRequest* req,
    HttpResponse* resp, void* user_data);

typedef struct {
  char path[512];
  char method[16];
  HttpHandler handler;
  void* user_data;
} HttpRoute;

typedef struct {
  uint64_t requests_handled;
  uint64_t requests_4xx;
  uint64_t requests_5xx;
  uint64_t bytes_sent;
  uint64_t bytes_received;
} HttpStats;

/* =============================================================================
 * Lifecycle
 * ============================================================================= */

/**
 * Create HTTP core. Allocates from arena.
 * @param arena  Arena for core and routes (long-lived)
 * @param port   Listen port (1-65535)
 * @param max_connections  Backlog (e.g. 5)
 * @return HttpCore* or NULL on failure
 */
HttpCore* http_core_create(nv_arena_t* arena, int port, int max_connections);

/**
 * Free core and close server. Sets *core to NULL.
 */
void http_core_free(HttpCore** core);

/* =============================================================================
 * Routing
 * ============================================================================= */

/**
 * Register route. Path may contain '*' for prefix match.
 * Method "*" matches any method.
 * @return HTTP_OK or HTTP_ERROR_INVALID (e.g. route table full, see HTTP_CORE_MAX_ROUTES)
 */
int http_core_register_route(HttpCore* core, const char* method,
    const char* path, HttpHandler handler, void* user_data);

/**
 * Set default (404) handler. If NULL, uses built-in JSON 404 response.
 */
void http_core_set_default_handler(HttpCore* core, HttpHandler handler,
    void* user_data);

/* =============================================================================
 * Server Control
 * ============================================================================= */

/**
 * Start server loop (blocking). Creates server, accepts connections,
 * parses requests, dispatches to routes, sends responses.
 * Returns when http_core_stop is called (e.g. from another thread).
 *
 * IMPORTANT: Single-threaded. Callers should run this on a dedicated thread if
 * the embedding application needs to remain responsive.
 * @return HTTP_OK or HTTP_ERROR_INVALID
 */
int http_core_start(HttpCore* core);

/**
 * Stop server loop. Safe to call from another thread or signal handler.
 */
void http_core_stop(HttpCore* core);

/* =============================================================================
 * Stats
 * ============================================================================= */

void http_core_get_stats(HttpCore* core, HttpStats* stats);

#ifdef __cplusplus
}
#endif

#endif /* HTTP_CORE_H */
