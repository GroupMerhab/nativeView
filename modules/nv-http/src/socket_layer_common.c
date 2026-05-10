/*
 * Copyright (c) 2026
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 */

#include "socket_layer_internal.h"
#include <string.h>

#define ARENA_SERVER_SZ  512
#define ARENA_CONN_SZ    256

static int g_initialized;
static int g_init_error;

int socket_layer_init(void) {
  if (g_initialized)
    return g_init_error;
  g_init_error = socket_layer_platform_init();
  g_initialized = 1;
  return g_init_error;
}

int socket_layer_cleanup(void) {
  return socket_layer_platform_cleanup();
}

ServerSocket* socket_create_server(const char* host, int port, int backlog) {
  if (!g_initialized)
    (void)socket_layer_init();
  if (g_init_error != SOCKET_OK)
    return NULL;
  if (!host)
    host = "0.0.0.0";

  nv_arena_t* arena = nv_arena_create(ARENA_SERVER_SZ);
  if (!arena)
    return NULL;

  ServerSocket* s = socket_layer_platform_create_server(arena, host, port, backlog);
  if (!s) {
    nv_arena_destroy(arena);
    return NULL;
  }
  s->arena = arena;
  return s;
}

void socket_close_server(ServerSocket** server) {
  if (!server || !*server)
    return;
  ServerSocket* s = *server;
  nv_arena_t* arena = s->arena;
  socket_layer_platform_close_server(s);
  if (arena)
    nv_arena_destroy(arena);
  *server = NULL;
}

ConnectionSocket* socket_accept_connection(ServerSocket* server, int timeout_ms) {
  if (!server)
    return NULL;

  nv_arena_t* arena = nv_arena_create(ARENA_CONN_SZ);
  if (!arena)
    return NULL;

  ConnectionSocket* c = socket_layer_platform_accept_connection(server, timeout_ms, arena);
  if (!c) {
    nv_arena_destroy(arena);
    return NULL;
  }
  c->arena = arena;
  return c;
}

int socket_send(ConnectionSocket* conn, const void* data, size_t len) {
  if (!conn || !data)
    return SOCKET_ERROR_INVALID;
  return socket_layer_platform_send(conn, data, len);
}

int socket_recv(ConnectionSocket* conn, void* buf, size_t max_len) {
  if (!conn || !buf)
    return SOCKET_ERROR_INVALID;
  return socket_layer_platform_recv(conn, buf, max_len);
}

void socket_close_connection(ConnectionSocket** conn) {
  if (!conn || !*conn)
    return;
  ConnectionSocket* c = *conn;
  nv_arena_t* arena = c->arena;
  socket_layer_platform_close_connection(c);
  if (arena)
    nv_arena_destroy(arena);
  *conn = NULL;
}

void socket_set_send_timeout(ConnectionSocket* conn, int timeout_ms) {
  if (conn)
    conn->timeout_ms = timeout_ms;
}

void socket_set_recv_timeout(ConnectionSocket* conn, int timeout_ms) {
  if (conn)
    conn->timeout_ms = timeout_ms;
}
