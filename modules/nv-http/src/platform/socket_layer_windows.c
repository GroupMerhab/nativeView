/*
 * Copyright (c) 2026
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 */

#include "socket_layer_internal.h"
#include "nv_arena.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <string.h>

typedef struct {
  SOCKET sock;
  int is_closed;
} WindowsHandle;

static int map_wsa_error(int err) {
  if (err == WSAEWOULDBLOCK || err == WSAETIMEDOUT)
    return SOCKET_ERROR_TIMEOUT;
  if (err == WSAENOTSOCK || err == WSAECONNRESET)
    return SOCKET_ERROR_ALREADY_CLOSED;
  return SOCKET_ERROR_RECV;
}

int socket_layer_platform_init(void) {
  WSADATA wsa;
  if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    return SOCKET_ERROR_CREATE;
  return SOCKET_OK;
}

int socket_layer_platform_cleanup(void) {
  WSACleanup();
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
  hints.ai_protocol = IPPROTO_TCP;
  hints.ai_flags = (strcmp(host, "0.0.0.0") == 0) ? AI_PASSIVE : 0;
  /* Avoid some getaddrinfo failures on Windows when the host is a numeric IPv4 string. */
  if ((hints.ai_flags & AI_PASSIVE) == 0 && strcmp(host, "127.0.0.1") == 0)
    hints.ai_flags |= AI_NUMERICHOST;

  char port_str[16];
  snprintf(port_str, sizeof(port_str), "%d", port);
  if (getaddrinfo(
        (hints.ai_flags & AI_PASSIVE) ? NULL : host,
        port_str, &hints, &res) != 0) {
    return NULL;
  }

  SOCKET listen_sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  if (listen_sock == INVALID_SOCKET) {
    freeaddrinfo(res);
    return NULL;
  }

  BOOL reuse = TRUE;
  if (setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR,
        (const char*)&reuse, sizeof(reuse)) == SOCKET_ERROR) {
    closesocket(listen_sock);
    freeaddrinfo(res);
    return NULL;
  }

  if (bind(listen_sock, res->ai_addr, (int)res->ai_addrlen) == SOCKET_ERROR) {
    closesocket(listen_sock);
    freeaddrinfo(res);
    return NULL;
  }

  if (listen(listen_sock, backlog) == SOCKET_ERROR) {
    closesocket(listen_sock);
    freeaddrinfo(res);
    return NULL;
  }

  ServerSocket* s = nv_arena_alloc(arena, sizeof(ServerSocket));
  WindowsHandle* h = nv_arena_alloc(arena, sizeof(WindowsHandle));
  if (!s || !h) {
    closesocket(listen_sock);
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
  WindowsHandle* h = (WindowsHandle*)server->handle;
  if (h && !h->is_closed && h->sock != INVALID_SOCKET) {
    shutdown(h->sock, SD_BOTH);
    closesocket(h->sock);
    h->is_closed = 1;
  }
}

ConnectionSocket* socket_layer_platform_accept_connection(ServerSocket* server,
    int timeout_ms, nv_arena_t* arena) {
  if (!server || !arena)
    return NULL;

  WindowsHandle* sh = (WindowsHandle*)server->handle;
  if (!sh || sh->is_closed || sh->sock == INVALID_SOCKET)
    return NULL;

  fd_set rd;
  FD_ZERO(&rd);
  FD_SET(sh->sock, &rd);
  struct timeval tv;
  tv.tv_sec = timeout_ms / 1000;
  tv.tv_usec = (timeout_ms % 1000) * 1000;

  if (select(0, &rd, NULL, NULL, &tv) <= 0)
    return NULL;

  struct sockaddr_in client_addr;
  int addr_len = (int)sizeof(client_addr);
  SOCKET client_sock = accept(sh->sock,
      (struct sockaddr*)&client_addr, &addr_len);
  if (client_sock == INVALID_SOCKET)
    return NULL;

  ConnectionSocket* c = nv_arena_alloc(arena, sizeof(ConnectionSocket));
  WindowsHandle* ch = nv_arena_alloc(arena, sizeof(WindowsHandle));
  if (!c || !ch) {
    closesocket(client_sock);
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
  WindowsHandle* h = (WindowsHandle*)conn->handle;
  if (!h || h->is_closed || h->sock == INVALID_SOCKET)
    return SOCKET_ERROR_ALREADY_CLOSED;

  BOOL no_delay = TRUE;
  setsockopt(h->sock, IPPROTO_TCP, TCP_NODELAY,
      (const char*)&no_delay, sizeof(no_delay));

  int n = send(h->sock, (const char*)data, (int)len, 0);
  if (n == SOCKET_ERROR) {
    int err = WSAGetLastError();
    return (err == WSAEWOULDBLOCK || err == WSAETIMEDOUT)
        ? SOCKET_ERROR_TIMEOUT : SOCKET_ERROR_SEND;
  }
  return n;
}

int socket_layer_platform_recv(ConnectionSocket* conn,
    void* buf, size_t max_len) {
  if (!conn || !buf)
    return SOCKET_ERROR_INVALID;
  WindowsHandle* h = (WindowsHandle*)conn->handle;
  if (!h || h->is_closed || h->sock == INVALID_SOCKET)
    return SOCKET_ERROR_ALREADY_CLOSED;

  int n = recv(h->sock, (char*)buf, (int)max_len, 0);
  if (n == SOCKET_ERROR) {
    int err = WSAGetLastError();
    return (err == WSAEWOULDBLOCK || err == WSAETIMEDOUT)
        ? SOCKET_ERROR_TIMEOUT : map_wsa_error(err);
  }
  return n;
}

void socket_layer_platform_close_connection(ConnectionSocket* conn) {
  if (!conn)
    return;
  WindowsHandle* h = (WindowsHandle*)conn->handle;
  if (h && !h->is_closed && h->sock != INVALID_SOCKET) {
    shutdown(h->sock, SD_BOTH);
    closesocket(h->sock);
    h->is_closed = 1;
  }
}
