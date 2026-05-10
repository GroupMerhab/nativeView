/*
 * Copyright (c) 2026
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 */

/**
 * @file file_server.h
 * @brief Static file serving with path validation and MIME types.
 *
 * Serves files from a root directory. Validates paths to prevent
 * directory traversal. Uses arena for response body allocation.
 */

#ifndef FILE_SERVER_H
#define FILE_SERVER_H

#include "http_protocol.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Types
 * ============================================================================= */

typedef struct FileServer FileServer;

/* Result codes */
#define FILE_OK            0
#define FILE_ERROR_INVALID -1
#define FILE_ERROR_ACCESS_DENIED -2
#define FILE_ERROR_NOT_FOUND -3
#define FILE_ERROR_TOO_LARGE -4
#define FILE_ERROR_READ -5

/* =============================================================================
 * Lifecycle
 * ============================================================================= */

/**
 * Create file server. Allocates from arena.
 * @param arena  Long-lived arena for FileServer
 * @param root   Root directory path (e.g. "/var/www")
 * @param max_file_size  Max bytes per file (prevents abuse)
 * @param enable_caching  If non-zero, adds Cache-Control and ETag headers
 */
FileServer* file_server_create(nv_arena_t* arena, const char* root,
    size_t max_file_size, int enable_caching);

/**
 * Free file server. Sets *server to NULL.
 * No-op when using arena allocation (provided for API symmetry).
 */
void file_server_free(FileServer** server);

/* =============================================================================
 * Request Handling
 * ============================================================================= */

/**
 * Handle HTTP request: validate path, read file, set response.
 * Uses arena for body allocation. Sets resp->status_code and body on success.
 * @return FILE_OK or error code
 */
int file_server_handle_request(nv_arena_t* arena, FileServer* server,
    const HttpRequest* req, HttpResponse* resp);

/**
 * HttpHandler wrapper. Pass as handler with user_data = FileServer*.
 * Example: register route GET /static/ with path prefix match.
 */
void file_server_handler(nv_arena_t* arena, const HttpRequest* req,
    HttpResponse* resp, void* user_data);

#ifdef __cplusplus
}
#endif

#endif /* FILE_SERVER_H */
