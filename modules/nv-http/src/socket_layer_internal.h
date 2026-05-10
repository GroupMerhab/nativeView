/*
 * Copyright (c) 2026
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 */

#ifndef SOCKET_LAYER_INTERNAL_H
#define SOCKET_LAYER_INTERNAL_H

#include "socket_layer.h"
#include "nv_arena.h"
#include <stdint.h>

/** Internal: server socket layout. Platform uses arena for allocations. */
struct ServerSocket {
  void* handle;
  int port;
  int backlog;
  int timeout_ms;
  nv_arena_t* arena;
};

/** Internal: connection socket layout. */
struct ConnectionSocket {
  void* handle;
  uint32_t remote_ip;
  uint16_t remote_port;
  int timeout_ms;
  nv_arena_t* arena;
};

/* Platform-specific implementation (one of windows/posix compiled) */
int socket_layer_platform_init(void);
int socket_layer_platform_cleanup(void);
ServerSocket* socket_layer_platform_create_server(nv_arena_t* arena,
  const char* host, int port, int backlog);
void socket_layer_platform_close_server(ServerSocket* server);
ConnectionSocket* socket_layer_platform_accept_connection(ServerSocket* server,
  int timeout_ms, nv_arena_t* arena);
int socket_layer_platform_send(ConnectionSocket* conn,
  const void* data, size_t len);
int socket_layer_platform_recv(ConnectionSocket* conn,
  void* buf, size_t max_len);
void socket_layer_platform_close_connection(ConnectionSocket* conn);

#endif /* SOCKET_LAYER_INTERNAL_H */
