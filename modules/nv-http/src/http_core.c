/*
 * Copyright (c) 2026
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 */

#include "http_core.h"
#include "http_protocol.h"
#include "nv_arena.h"
#include "socket_layer.h"
#include "util/nv_log.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>

#define REQUEST_BUF_SZ 8192
#define RESPONSE_BUF_SZ 16384
#define ARENA_PER_REQUEST_SZ (2 * 1024 * 1024)
#define ERR_JSON_BAD "{\"error\":\"Bad Request\"}"
#define ERR_JSON_404 "{\"error\":\"Not Found\"}"

struct HttpCore {
  ServerSocket* server;
  HttpRoute routes[HTTP_CORE_MAX_ROUTES];
  int route_count;
  HttpHandler default_handler;
  void* default_handler_data;
  HttpStats stats;
  int running;
  int port;
  int max_connections;
};

/* Best-effort full write for headers/body; socket_send may return partial bytes. */
static int socket_send_all(ConnectionSocket* conn, const void* data, size_t len) {
  if (!conn || (!data && len > 0)) return SOCKET_ERROR_INVALID;
  const char* p = (const char*)data;
  size_t sent_total = 0;
  while (sent_total < len) {
    int n = socket_send(conn, p + sent_total, len - sent_total);
    if (n <= 0) return n == 0 ? SOCKET_ERROR_SEND : n;
    sent_total += (size_t)n;
  }
  if (sent_total > (size_t)INT_MAX)
    return INT_MAX;
  return (int)sent_total;
}

static int http_send_file_body(ConnectionSocket* conn, const char* path, size_t expected_len,
    uint64_t* bytes_sent) {
  if (!conn || !path || !path[0]) return SOCKET_ERROR_INVALID;
  FILE* f = fopen(path, "rb");
  if (!f) return SOCKET_ERROR_SEND;
  unsigned char buf[64 * 1024];
  size_t total = 0;
  for (;;) {
    size_t want = sizeof(buf);
    if (expected_len > 0) {
      size_t remaining = expected_len > total ? (expected_len - total) : 0;
      if (remaining == 0) break;
      if (remaining < want) want = remaining;
    }
    size_t n = fread(buf, 1, want, f);
    if (n == 0) break;
    int s = socket_send_all(conn, buf, n);
    if (s <= 0) {
      fclose(f);
      return s;
    }
    total += (size_t)s;
    if (bytes_sent) *bytes_sent += (uint64_t)s;
  }
  fclose(f);
  return (expected_len == 0 || total == expected_len) ? (int)(total > (size_t)INT_MAX ? INT_MAX : total) : SOCKET_ERROR_SEND;
}

static HttpRoute* find_route(HttpCore* core, const HttpRequest* req) {
  if (!core || !req) return NULL;
  HttpRoute* best = NULL;
  int best_priority = -1;
  for (int i = 0; i < core->route_count; i++) {
    HttpRoute* r = &core->routes[i];
    if (strcmp(r->method, "*") != 0 && strcmp(r->method, req->method) != 0)
      continue;
    int priority = 0;
    if (strcmp(r->path, req->path) == 0)
      priority = 3;
    else {
      char* star = strchr(r->path, '*');
      if (star) {
        size_t n = (size_t)(star - r->path);
        if (strncmp(r->path, req->path, n) == 0) priority = 2;
      } else if (strcmp(r->path, "*") == 0)
        priority = 1;
    }
    if (priority > best_priority) { best = r; best_priority = priority; }
  }
  return best;
}

HttpCore* http_core_create(nv_arena_t* arena, int port, int max_connections) {
  if (!arena || port <= 0 || port > 65535 || max_connections <= 0) return NULL;
  HttpCore* c = nv_arena_alloc(arena, sizeof(HttpCore));
  if (!c) return NULL;
  memset(c, 0, sizeof(*c));
  c->port = port;
  c->max_connections = max_connections > 128 ? 128 : max_connections;
  return c;
}

void http_core_free(HttpCore** core) {
  if (!core || !*core) return;
  HttpCore* c = *core;
  if (c->server) {
    socket_close_server(&c->server);
  }
  *core = NULL;
}

int http_core_register_route(HttpCore* core, const char* method,
    const char* path, HttpHandler handler, void* user_data) {
  if (!core || !method || !path || !handler)
    return HTTP_ERROR_INVALID;
  if (core->route_count >= HTTP_CORE_MAX_ROUTES) {
    NV_ERR("http_core: route table full (max %d); refusing registration for %s %s",
        HTTP_CORE_MAX_ROUTES, method, path);
    return HTTP_ERROR_INVALID;
  }
  HttpRoute* r = &core->routes[core->route_count++];
  strncpy(r->method, method, sizeof(r->method) - 1);
  r->method[sizeof(r->method) - 1] = '\0';
  strncpy(r->path, path, sizeof(r->path) - 1);
  r->path[sizeof(r->path) - 1] = '\0';
  r->handler = handler;
  r->user_data = user_data;
  return HTTP_OK;
}

void http_core_set_default_handler(HttpCore* core, HttpHandler handler,
    void* user_data) {
  if (!core) return;
  core->default_handler = handler;
  core->default_handler_data = user_data;
}

void http_core_stop(HttpCore* core) {
  if (core) core->running = 0;
}

void http_core_get_stats(HttpCore* core, HttpStats* stats) {
  if (!core || !stats) return;
  memcpy(stats, &core->stats, sizeof(HttpStats));
}

int http_core_start(HttpCore* core) {
  if (!core) return HTTP_ERROR_INVALID;
  if (socket_layer_init() != SOCKET_OK) return HTTP_ERROR_INVALID;

  core->server = socket_create_server("0.0.0.0", core->port,
      core->max_connections);
  if (!core->server) return HTTP_ERROR_INVALID;

  core->running = 1;
  while (core->running) {
    ConnectionSocket* conn = socket_accept_connection(core->server, 5000);
    if (!conn) continue;

    char req_buf[REQUEST_BUF_SZ];
    int n = socket_recv(conn, req_buf, sizeof(req_buf));
    if (n <= 0) {
      socket_close_connection(&conn);
      continue;
    }

    core->stats.bytes_received += (uint64_t)n;

    nv_arena_t* arena = nv_arena_create(ARENA_PER_REQUEST_SZ);
    if (!arena) {
      socket_close_connection(&conn);
      continue;
    }

    HttpRequest req;
    int parse_err = http_request_parse(arena, req_buf, (size_t)n, &req);

    HttpResponse* resp = NULL;
    if (parse_err != HTTP_OK) {
      resp = http_response_create(arena, 400);
      if (resp)
        http_response_set_body(arena, resp, ERR_JSON_BAD,
            sizeof(ERR_JSON_BAD) - 1, "application/json");
    } else {
      HttpRoute* route = find_route(core, &req);
      if (!route) {
        resp = http_response_create(arena, 404);
        if (resp) {
          if (core->default_handler)
            core->default_handler(arena, &req, resp, core->default_handler_data);
          else
            http_response_set_body(arena, resp, ERR_JSON_404,
                sizeof(ERR_JSON_404) - 1, "application/json");
        }
      } else {
        resp = http_response_create(arena, 200);
        if (resp) route->handler(arena, &req, resp, route->user_data);
      }
    }

    if (resp) {
      char hdr_buf[RESPONSE_BUF_SZ];
      int hdr_len = http_response_serialize_headers(resp, hdr_buf,
          sizeof(hdr_buf));
      if (hdr_len > 0) {
        int s = socket_send_all(conn, hdr_buf, (size_t)hdr_len);
        if (s > 0) {
          core->stats.bytes_sent += (uint64_t)s;
          if (resp->body_is_file && resp->body_file_path[0]) {
            (void)http_send_file_body(conn, resp->body_file_path, resp->body_len, &core->stats.bytes_sent);
          } else if (resp->body_len > 0 && resp->body) {
            int s2 = socket_send_all(conn, resp->body, resp->body_len);
            if (s2 > 0) core->stats.bytes_sent += (uint64_t)s2;
          }
        }
      }
      core->stats.requests_handled++;
      if (resp->status_code >= 400 && resp->status_code < 500)
        core->stats.requests_4xx++;
      else if (resp->status_code >= 500)
        core->stats.requests_5xx++;
    }

    nv_arena_destroy(arena);
    socket_close_connection(&conn);
  }

  socket_close_server(&core->server);
  return HTTP_OK;
}
