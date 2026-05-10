/*
 * Copyright (c) 2026
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 */

#include "file_server.h"
#include "mime_types.h"
#include "nv_arena.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#define ROOT_MAX 512
#define FULL_PATH_MAX 1024

struct FileServer {
  char root_path[ROOT_MAX];
  size_t max_file_size;
  int enable_caching;
};

#if defined(_WIN32)
/**
 * @brief Make @p path use `/` separators in place.
 *
 * @ref path_normalize maps both `/` and `\\` to `/` in the combined root+URL path.
 * If @ref FileServer::root_path keeps backslashes, @ref validate_path's `strncmp` of
 * `norm` against `root` fails at the first separator and every static file returns 403.
 * `fopen` / `stat` accept forward slashes on Windows.
 */
static void file_server_normalize_root_slashes(char* path) {
  if (!path) return;
  for (char* p = path; *p; p++) {
    if (*p == '\\')
      *p = '/';
  }
}
#endif

static int path_normalize(const char* path, char* out, size_t out_size) {
  if (!path || !out || out_size == 0) return 0;
  const char* src = path;
  char* dst = out;
  char* end = out + out_size - 1;

  while (*src && dst < end) {
    if (*src == '/' || *src == '\\') {
      if (dst == out || dst[-1] != '/') *dst++ = '/';
      while (*src == '/' || *src == '\\') src++;
    } else if (src[0] == '.' && (src[1] == '/' || src[1] == '\\' || !src[1])) {
      if (!src[1]) break;
      src += 2;
    } else if (src[0] == '.' && src[1] == '.' &&
               (src[2] == '/' || src[2] == '\\' || !src[2])) {
      return 0;
    } else {
      *dst++ = *src++;
    }
  }
  *dst = '\0';
  return 1;
}

static int validate_path(const char* root, const char* requested) {
  if (!root || !requested || requested[0] != '/') return 0;
  char full[FULL_PATH_MAX];
  int n = snprintf(full, sizeof(full), "%s%s", root, requested);
  if (n < 0 || (size_t)n >= sizeof(full)) return 0;
  char norm[FULL_PATH_MAX];
  if (!path_normalize(full, norm, sizeof(norm))) return 0;
  size_t rlen = strlen(root);
  if (strncmp(norm, root, rlen) != 0) return 0;
  char next = norm[rlen];
  if (next != '\0' && next != '/') return 0;
  return 1;
}

FileServer* file_server_create(nv_arena_t* arena, const char* root,
    size_t max_file_size, int enable_caching) {
  if (!arena || !root) return NULL;
  FileServer* fs = nv_arena_alloc(arena, sizeof(FileServer));
  if (!fs) return NULL;
  memset(fs, 0, sizeof(*fs));
  strncpy(fs->root_path, root, ROOT_MAX - 1);
  fs->root_path[ROOT_MAX - 1] = '\0';
#if defined(_WIN32)
  file_server_normalize_root_slashes(fs->root_path);
#endif
  fs->max_file_size = max_file_size > 0 ? max_file_size : 1024 * 1024;
  fs->enable_caching = enable_caching ? 1 : 0;
  return fs;
}

void file_server_free(FileServer** server) {
  if (server) *server = NULL;
}

int file_server_handle_request(nv_arena_t* arena, FileServer* server,
    const HttpRequest* req, HttpResponse* resp) {
  if (!server || !req || !resp) return FILE_ERROR_INVALID;

  if (!validate_path(server->root_path, req->path)) {
    resp->status_code = 403;
    http_response_set_body(arena, resp, "{\"error\":\"Access Denied\"}", 22,
        "application/json");
    return FILE_ERROR_ACCESS_DENIED;
  }

  char full_path[FULL_PATH_MAX];
  int n = snprintf(full_path, sizeof(full_path), "%s%s",
      server->root_path, req->path);
  if (n < 0 || (size_t)n >= sizeof(full_path))
    return FILE_ERROR_INVALID;

  struct stat st;
  if (stat(full_path, &st) < 0) {
    resp->status_code = 404;
    http_response_set_body(arena, resp, "{\"error\":\"Not Found\"}", 19,
        "application/json");
    return FILE_ERROR_NOT_FOUND;
  }

  if (S_ISDIR(st.st_mode)) {
    resp->status_code = 404;
    http_response_set_body(arena, resp, "{\"error\":\"Not Found\"}", 19,
        "application/json");
    return FILE_ERROR_NOT_FOUND;
  }

  if ((size_t)st.st_size > server->max_file_size) {
    resp->status_code = 413;
    http_response_set_body(arena, resp,
        "{\"error\":\"Payload Too Large\"}", 28, "application/json");
    return FILE_ERROR_TOO_LARGE;
  }

  FILE* f = fopen(full_path, "rb");
  if (!f) {
    resp->status_code = 403;
    http_response_set_body(arena, resp, "{\"error\":\"Access Denied\"}", 22,
        "application/json");
    return FILE_ERROR_ACCESS_DENIED;
  }
  fclose(f);

  const char* mime = mime_type_from_filename(full_path);
  resp->status_code = 200;
  if (http_response_set_body_file(resp, full_path, (size_t)st.st_size, mime) != HTTP_OK) {
    resp->status_code = 500;
    return FILE_ERROR_READ;
  }

  if (server->enable_caching) {
    http_response_set_header(resp, "Cache-Control", "max-age=3600");
    char etag[64];
    snprintf(etag, sizeof(etag), "\"%lx-%lx\"",
        (long)st.st_mtime, (long)st.st_size);
    http_response_set_header(resp, "ETag", etag);
  }

  return FILE_OK;
}

void file_server_handler(nv_arena_t* arena, const HttpRequest* req,
    HttpResponse* resp, void* user_data) {
  FileServer* fs = (FileServer*)user_data;
  if (fs)
    file_server_handle_request(arena, fs, req, resp);
}
