/*
 * Copyright (c) 2026
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 */

#define _POSIX_C_SOURCE 200809L

#include "socket_layer_internal.h"
#include "nv_arena.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#if defined(SO_NOSIGPIPE)
#define NV_USE_SO_NOSIGPIPE 1
#else
#define NV_USE_SO_NOSIGPIPE 0
#endif

#if defined(MSG_NOSIGNAL)
#define NV_MSG_NOSIGNAL MSG_NOSIGNAL
#else
#define NV_MSG_NOSIGNAL 0
#endif

typedef struct {
  int sock;
  int is_closed;
} PosixHandle;

int socket_layer_platform_init(void) {
  signal(SIGPIPE, SIG_IGN);
  return SOCKET_OK;
}

int socket_layer_platform_cleanup(void) {
  return SOCKET_OK;
}

ServerSocket* socket_layer_platform_create_server(nv_arena_t* arena,
    const char* host, int port, int backlog) {
  if (!arena || !host)
    return NULL;

  struct addrinfo hints, *res = NULL;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = 0;
  hints.ai_flags = (strcmp(host, "0.0.0.0") == 0) ? AI_PASSIVE : 0;

  char port_str[16];
  snprintf(port_str, sizeof(port_str), "%d", port);
  const char* node = (hints.ai_flags & AI_PASSIVE) ? NULL : host;
  if (getaddrinfo(node, port_str, &hints, &res) != 0)
    return NULL;

  int listen_sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  if (listen_sock < 0) {
    freeaddrinfo(res);
    return NULL;
  }

  int reuse = 1;
  if (setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR,
        &reuse, sizeof(reuse)) < 0) {
    close(listen_sock);
    freeaddrinfo(res);
    return NULL;
  }

  if (bind(listen_sock, res->ai_addr, res->ai_addrlen) < 0) {
    close(listen_sock);
    freeaddrinfo(res);
    return NULL;
  }

  if (listen(listen_sock, backlog) < 0) {
    close(listen_sock);
    freeaddrinfo(res);
    return NULL;
  }

  ServerSocket* s = nv_arena_alloc(arena, sizeof(ServerSocket));
  PosixHandle* h = nv_arena_alloc(arena, sizeof(PosixHandle));
  if (!s || !h) {
    close(listen_sock);
    freeaddrinfo(res);
    return NULL;
  }

  h->sock = listen_sock;
  h->is_closed = 0;
  s->handle = h;
  s->port = port;
  s->backlog = backlog;
  s->timeout_ms = 5000;
  s->arena = arena;

  freeaddrinfo(res);
  return s;
}

void socket_layer_platform_close_server(ServerSocket* server) {
  if (!server)
    return;
  PosixHandle* h = (PosixHandle*)server->handle;
  if (h && !h->is_closed && h->sock >= 0) {
    shutdown(h->sock, SHUT_RDWR);
    close(h->sock);
    h->is_closed = 1;
  }
}

ConnectionSocket* socket_layer_platform_accept_connection(ServerSocket* server,
    int timeout_ms, nv_arena_t* arena) {
  if (!server || !arena)
    return NULL;

  PosixHandle* sh = (PosixHandle*)server->handle;
  if (!sh || sh->is_closed || sh->sock < 0)
    return NULL;

  fd_set rd;
  FD_ZERO(&rd);
  FD_SET(sh->sock, &rd);
  struct timeval tv;
  tv.tv_sec = timeout_ms / 1000;
  tv.tv_usec = (timeout_ms % 1000) * 1000;

  int nfds = sh->sock + 1;
  if (select(nfds, &rd, NULL, NULL, &tv) <= 0)
    return NULL;

  struct sockaddr_in client_addr;
  socklen_t addr_len = sizeof(client_addr);
  int client_sock = accept(sh->sock,
      (struct sockaddr*)&client_addr, &addr_len);
  if (client_sock < 0)
    return NULL;

#if NV_USE_SO_NOSIGPIPE
  {
    int nosig = 1;
    setsockopt(client_sock, SOL_SOCKET, SO_NOSIGPIPE, &nosig, sizeof(nosig));
  }
#endif

  ConnectionSocket* c = nv_arena_alloc(arena, sizeof(ConnectionSocket));
  PosixHandle* ch = nv_arena_alloc(arena, sizeof(PosixHandle));
  if (!c || !ch) {
    close(client_sock);
    return NULL;
  }

  ch->sock = client_sock;
  ch->is_closed = 0;
  c->handle = ch;
  c->remote_ip = ntohl(client_addr.sin_addr.s_addr);
  c->remote_port = ntohs(client_addr.sin_port);
  c->timeout_ms = 5000;
  c->arena = arena;

  return c;
}

int socket_layer_platform_send(ConnectionSocket* conn,
    const void* data, size_t len) {
  if (!conn || !data)
    return SOCKET_ERROR_INVALID;
  PosixHandle* h = (PosixHandle*)conn->handle;
  if (!h || h->is_closed || h->sock < 0)
    return SOCKET_ERROR_ALREADY_CLOSED;

  int no_delay = 1;
  setsockopt(h->sock, IPPROTO_TCP, TCP_NODELAY, &no_delay, sizeof(no_delay));

  ssize_t n = send(h->sock, data, len, NV_MSG_NOSIGNAL);
  if (n < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK)
      return SOCKET_ERROR_TIMEOUT;
    return SOCKET_ERROR_SEND;
  }
  return (int)n;
}

int socket_layer_platform_recv(ConnectionSocket* conn,
    void* buf, size_t max_len) {
  if (!conn || !buf)
    return SOCKET_ERROR_INVALID;
  PosixHandle* h = (PosixHandle*)conn->handle;
  if (!h || h->is_closed || h->sock < 0)
    return SOCKET_ERROR_ALREADY_CLOSED;

  ssize_t n = recv(h->sock, buf, max_len, 0);
  if (n < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK)
      return SOCKET_ERROR_TIMEOUT;
    return SOCKET_ERROR_RECV;
  }
  return (int)n;
}

void socket_layer_platform_close_connection(ConnectionSocket* conn) {
  if (!conn)
    return;
  PosixHandle* h = (PosixHandle*)conn->handle;
  if (h && !h->is_closed && h->sock >= 0) {
    shutdown(h->sock, SHUT_RDWR);
    close(h->sock);
    h->is_closed = 1;
  }
}
