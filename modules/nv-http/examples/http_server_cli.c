/*
 * CLI to run the nv-http static file server on a directory and port.
 * Usage: http_server_cli <directory> [port]
 * Example: http_server_cli ./ui 8080
 * Serves directory at http://127.0.0.1:<port>/  (GET / -> index.html, GET * -> files)
 */

#include "file_server.h"
#include "http_core.h"
#include "http_protocol.h"
#include "nv_arena.h"
#include "socket_layer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ARENA_SIZE (256 * 1024)
#define DEFAULT_PORT 8080

static void handler_root(nv_arena_t* arena, const HttpRequest* req, HttpResponse* resp,
    void* user_data) {
  (void)req;
  FileServer* fs = (FileServer*)user_data;
  HttpRequest mod = {0};
  mod.method[0] = 'G';
  mod.method[1] = 'E';
  mod.method[2] = 'T';
  mod.method[3] = '\0';
  strcpy(mod.path, "/index.html");
  mod.query[0] = '\0';
  file_server_handle_request(arena, fs, &mod, resp);
}

static void handler_static(nv_arena_t* arena, const HttpRequest* req, HttpResponse* resp,
    void* user_data) {
  FileServer* fs = (FileServer*)user_data;
  file_server_handle_request(arena, fs, req, resp);
}

static void handler_404(nv_arena_t* arena, const HttpRequest* req, HttpResponse* resp,
    void* user_data) {
  (void)req;
  (void)user_data;
  resp->status_code = 404;
  http_response_set_body(arena, resp, "{\"error\":\"Not Found\"}", 21, "application/json");
}

static void usage(const char* prog) {
  fprintf(stderr, "Usage: %s <directory> [port]\n", prog);
  fprintf(stderr, "  directory  Root folder to serve (e.g. . or ./ui)\n");
  fprintf(stderr, "  port       Listen port (default %d)\n", DEFAULT_PORT);
  fprintf(stderr, "Example: %s . 8080\n", prog);
}

int main(int argc, char** argv) {
  if (argc < 2) {
    usage(argv[0] ? argv[0] : "http_server_cli");
    return 1;
  }

  const char* dir = argv[1];
  int port = (argc >= 3) ? atoi(argv[2]) : DEFAULT_PORT;
  if (port <= 0 || port > 65535) {
    fprintf(stderr, "Invalid port. Use 1-65535.\n");
    return 1;
  }

  if (socket_layer_init() != SOCKET_OK) {
    fprintf(stderr, "Socket init failed.\n");
    return 1;
  }

  nv_arena_t* arena = nv_arena_create(ARENA_SIZE);
  if (!arena) {
    fprintf(stderr, "Arena create failed.\n");
    socket_layer_cleanup();
    return 1;
  }

  FileServer* fs = file_server_create(arena, dir, 2 * 1024 * 1024, 1);
  if (!fs) {
    fprintf(stderr, "File server create failed.\n");
    nv_arena_destroy(arena);
    socket_layer_cleanup();
    return 1;
  }

  HttpCore* core = http_core_create(arena, port, 5);
  if (!core) {
    fprintf(stderr, "HTTP core create failed.\n");
    file_server_free(&fs);
    nv_arena_destroy(arena);
    socket_layer_cleanup();
    return 1;
  }

  if (http_core_register_route(core, "GET", "/", handler_root, fs) != HTTP_OK ||
      http_core_register_route(core, "GET", "*", handler_static, fs) != HTTP_OK) {
    fprintf(stderr, "Route register failed.\n");
    http_core_free(&core);
    file_server_free(&fs);
    nv_arena_destroy(arena);
    socket_layer_cleanup();
    return 1;
  }
  http_core_set_default_handler(core, handler_404, NULL);

  printf("Serving %s at http://127.0.0.1:%d/\n", dir, port);
  printf("Press Ctrl+C to stop.\n");

  int r = http_core_start(core);
  http_core_free(&core);
  file_server_free(&fs);
  nv_arena_destroy(arena);
  socket_layer_cleanup();
  return (r == HTTP_OK) ? 0 : 1;
}
