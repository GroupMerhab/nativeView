/*
 * Copyright (c) 2026
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @file socket_layer.h
 * @brief Platform-neutral TCP socket operations.
 *
 * Abstracts Windows WinSock2, POSIX sockets, and macOS differences.
 * Caller never sees platform-specific code or #ifdef.
 *
 * Usage:
 *   1. socket_layer_init() (optional, auto-called on first use)
 *   2. ServerSocket* s = socket_create_server("127.0.0.1", 8080, 5);
 *   3. ConnectionSocket* c = socket_accept_connection(s, 5000);
 *   4. int n = socket_recv(c, buf, sizeof(buf));
 *   5. socket_close_connection(&c);
 *   6. socket_close_server(&s);
 *   7. socket_layer_cleanup() (optional)
 */

#ifndef SOCKET_LAYER_H
#define SOCKET_LAYER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Opaque Types
 * ============================================================================= */

/** Opaque server (listening) socket. */
typedef struct ServerSocket ServerSocket;

/** Opaque connection (accepted client) socket. */
typedef struct ConnectionSocket ConnectionSocket;

/* =============================================================================
 * Error Codes
 * ============================================================================= */

typedef enum {
  SOCKET_OK = 0,
  SOCKET_ERROR_CREATE = -1,
  SOCKET_ERROR_BIND = -2,
  SOCKET_ERROR_LISTEN = -3,
  SOCKET_ERROR_ACCEPT = -4,
  SOCKET_ERROR_SEND = -5,
  SOCKET_ERROR_RECV = -6,
  SOCKET_ERROR_TIMEOUT = -7,
  SOCKET_ERROR_INVALID = -8,
  SOCKET_ERROR_ALREADY_CLOSED = -9,
} SocketError;

/* =============================================================================
 * Lifecycle
 * ============================================================================= */

/**
 * Initialize platform socket layer (e.g. WSAStartup on Windows, SIGPIPE ignore on POSIX).
 * Safe to call multiple times. Auto-invoked on first socket operation if not called.
 * @return SOCKET_OK or SOCKET_ERROR_*
 */
int socket_layer_init(void);

/**
 * Shutdown platform socket layer (e.g. WSACleanup on Windows).
 * Safe to call after all sockets are closed.
 * @return SOCKET_OK
 */
int socket_layer_cleanup(void);

/* =============================================================================
 * Server API
 * ============================================================================= */

/**
 * Create a TCP server socket bound to host:port.
 * @param host   Bind address ("127.0.0.1", "0.0.0.0", or NULL for all interfaces)
 * @param port   Port number (1-65535)
 * @param backlog Maximum pending connections (e.g. 5)
 * @return ServerSocket* or NULL on failure
 */
ServerSocket* socket_create_server(const char* host, int port, int backlog);

/**
 * Close server socket and free resources.
 * Sets *server to NULL.
 * @param server  Pointer to server handle (may be NULL)
 */
void socket_close_server(ServerSocket** server);

/* =============================================================================
 * Connection API
 * ============================================================================= */

/**
 * Accept a pending connection with optional timeout.
 * @param server    Listening server socket
 * @param timeout_ms Max wait in milliseconds (0 = block indefinitely)
 * @return ConnectionSocket* or NULL on timeout/error
 */
ConnectionSocket* socket_accept_connection(ServerSocket* server, int timeout_ms);

/**
 * Send data on a connection.
 * @param conn  Connection socket
 * @param data  Data to send
 * @param len   Number of bytes
 * @return Bytes sent (>0), or SocketError (<0)
 */
int socket_send(ConnectionSocket* conn, const void* data, size_t len);

/**
 * Receive data from a connection.
 * @param conn   Connection socket
 * @param buf    Buffer to fill
 * @param max_len Maximum bytes to read
 * @return Bytes received (>0), 0 if peer closed, or SocketError (<0)
 */
int socket_recv(ConnectionSocket* conn, void* buf, size_t max_len);

/**
 * Close connection and free resources.
 * Sets *conn to NULL.
 * @param conn  Pointer to connection handle (may be NULL)
 */
void socket_close_connection(ConnectionSocket** conn);

/* =============================================================================
 * Timeouts (optional)
 * ============================================================================= */

/**
 * Set send timeout for a connection (affects future send calls).
 * @param conn       Connection socket
 * @param timeout_ms Timeout in milliseconds
 */
void socket_set_send_timeout(ConnectionSocket* conn, int timeout_ms);

/**
 * Set receive timeout for a connection (affects future recv calls).
 * @param conn       Connection socket
 * @param timeout_ms Timeout in milliseconds
 */
void socket_set_recv_timeout(ConnectionSocket* conn, int timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* SOCKET_LAYER_H */
