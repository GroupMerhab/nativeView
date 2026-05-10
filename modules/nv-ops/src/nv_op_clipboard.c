#include "nv_op_clipboard.h"
#include "nv_base64.h"
#include "nv.h"
#include <stdlib.h>
#include <string.h>

NV_INTERNAL void nv_ipc_reply_ok(nv_window_t* w, int seq, nv_json_t* result, nv_arena_t* arena);
NV_INTERNAL void nv_ipc_reply_err(nv_window_t* w, int seq, const char* code, const char* message, nv_arena_t* arena);

/* macOS platform hooks (implemented in nv_mac.m) */
NV_INTERNAL char* nv_mac_clipboard_read_text(void);
NV_INTERNAL int   nv_mac_clipboard_write_text(const char* text);
NV_INTERNAL void  nv_mac_clipboard_clear(void);
NV_INTERNAL int   nv_mac_clipboard_has_text(void);
NV_INTERNAL char* nv_mac_clipboard_read_image(int* out_w, int* out_h);
NV_INTERNAL int   nv_mac_clipboard_write_image(const char* base64_png);
NV_INTERNAL int   nv_mac_clipboard_has_image(void);

/* Windows platform hooks (implemented in nv_win.c / nv_win_clipboard_image.cpp) */
#if defined(_WIN32)
NV_INTERNAL char* nv_win_clipboard_read_text(void);
NV_INTERNAL int   nv_win_clipboard_write_text(const char* utf8);
NV_INTERNAL void  nv_win_clipboard_clear(void);
NV_INTERNAL int   nv_win_clipboard_has_text(void);
NV_INTERNAL char* nv_win_clipboard_read_image(int* out_w, int* out_h);
NV_INTERNAL int   nv_win_clipboard_write_image(const char* base64_png);
NV_INTERNAL int   nv_win_clipboard_has_image(void);
#endif
#if defined(__linux__) && !defined(__APPLE__)
NV_INTERNAL char* nv_linux_clipboard_read_text(void);
NV_INTERNAL int   nv_linux_clipboard_write_text(const char* utf8);
NV_INTERNAL void  nv_linux_clipboard_clear(void);
NV_INTERNAL int   nv_linux_clipboard_has_text(void);
NV_INTERNAL char* nv_linux_clipboard_read_image(int* out_w, int* out_h);
NV_INTERNAL int   nv_linux_clipboard_write_image(const char* base64_png);
NV_INTERNAL int   nv_linux_clipboard_has_image(void);
#endif

static int nv_clipboard_png_ihdr_dims(const uint8_t* p, size_t n, int* w, int* h) {
  static const uint8_t sig[8] = {137u, 80u, 78u, 71u, 13u, 10u, 26u, 10u};
  if (!w || !h || !p) return -1;
  *w = *h = 0;
  if (n < 24u) return -1;
  if (memcmp(p, sig, 8u) != 0) return -1;
  if (memcmp(p + 12u, "IHDR", 4u) != 0) return -1;
  *w = (int)(((unsigned)p[16] << 24) | ((unsigned)p[17] << 16) | ((unsigned)p[18] << 8) |
             (unsigned)p[19]);
  *h = (int)(((unsigned)p[20] << 24) | ((unsigned)p[21] << 16) | ((unsigned)p[22] << 8) |
             (unsigned)p[23]);
  if (*w <= 0 || *h <= 0) return -1;
  return 0;
}

NV_INTERNAL void nv_op_clipboard_read_text(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
  (void)args;
  char* heap = NULL;
#ifdef __APPLE__
  heap = nv_mac_clipboard_read_text();
#elif defined(_WIN32)
  heap = nv_win_clipboard_read_text();
#elif defined(__linux__) && !defined(__APPLE__)
  heap = nv_linux_clipboard_read_text();
#endif
  const char* text = heap ? heap : "";
  nv_json_t* obj = nv_json_object(arena);
  nv_json_str(obj, "text", text);
  nv_ipc_reply_ok(w, seq, obj, arena);
  if (heap) free(heap);
}

NV_INTERNAL void nv_op_clipboard_write_text(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
  const char* text = args ? nv_json_get_str(args, "text") : NULL;
  if (!text) { nv_ipc_reply_err(w, seq, "ERR_INVALID_ARG", "missing 'text'", arena); return; }
  int rc = 0;
#ifdef __APPLE__
  rc = nv_mac_clipboard_write_text(text);
#elif defined(_WIN32)
  rc = nv_win_clipboard_write_text(text);
#elif defined(__linux__) && !defined(__APPLE__)
  rc = nv_linux_clipboard_write_text(text);
#endif
  if (rc != 0) { nv_ipc_reply_err(w, seq, "ERR_IO", "clipboard write failed", arena); return; }
  nv_ipc_reply_ok(w, seq, nv_json_object(arena), arena);
}

NV_INTERNAL void nv_op_clipboard_read_image(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
  (void)args;
  int iw = 0, ih = 0;
  char* b64 = NULL;
#ifdef __APPLE__
  b64 = nv_mac_clipboard_read_image(&iw, &ih);
#elif defined(_WIN32)
  b64 = nv_win_clipboard_read_image(&iw, &ih);
#elif defined(__linux__) && !defined(__APPLE__)
  b64 = nv_linux_clipboard_read_image(&iw, &ih);
#endif
  if (!b64 || !b64[0]) {
    if (b64) free(b64);
    nv_ipc_reply_err(w, seq, "ERR_IO", "clipboard has no PNG image", arena);
    return;
  }
  if (iw <= 0 || ih <= 0) {
    free(b64);
    nv_ipc_reply_err(w, seq, "ERR_IO", "clipboard image has invalid dimensions", arena);
    return;
  }
  {
    nv_json_t* obj = nv_json_object(arena);
    nv_json_int(obj, "width", (long long)iw);
    nv_json_int(obj, "height", (long long)ih);
    nv_json_str(obj, "format", "png");
    nv_json_str(obj, "data", b64);
    nv_ipc_reply_ok(w, seq, obj, arena);
  }
  free(b64);
}

NV_INTERNAL void nv_op_clipboard_write_image(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
  const char* fmt = args ? nv_json_get_str(args, "format") : NULL;
  const char* data = args ? nv_json_get_str(args, "data") : NULL;
  if (!data) data = args ? nv_json_get_str(args, "bytes") : NULL;
  if (!data) data = args ? nv_json_get_str(args, "b64") : NULL;
  if (!data) { nv_ipc_reply_err(w, seq, "ERR_INVALID_ARG", "missing 'data' (base64 PNG)", arena); return; }
  if (!fmt || !fmt[0]) fmt = "png";
  if (strcmp(fmt, "png") != 0) {
    nv_ipc_reply_err(w, seq, "ERR_INVALID_ARG", "only format 'png' is supported", arena);
    return;
  }
  {
    size_t blen = strlen(data);
    size_t maxdec = nv_base64_decode_max_out(blen);
    uint8_t* raw = (uint8_t*)malloc(maxdec ? maxdec : 1u);
    size_t dlen = 0;
    int pw = 0, ph = 0;
    if (!raw || nv_base64_decode(data, blen, raw, maxdec, &dlen) != 0 ||
        nv_clipboard_png_ihdr_dims(raw, dlen, &pw, &ph) != 0) {
      free(raw);
      nv_ipc_reply_err(w, seq, "ERR_INVALID_ARG", "invalid base64 PNG payload", arena);
      return;
    }
    free(raw);
  }
  {
    int rc = -1;
#ifdef __APPLE__
    rc = nv_mac_clipboard_write_image(data);
#elif defined(_WIN32)
    rc = nv_win_clipboard_write_image(data);
#elif defined(__linux__) && !defined(__APPLE__)
    rc = nv_linux_clipboard_write_image(data);
#endif
    if (rc != 0) { nv_ipc_reply_err(w, seq, "ERR_IO", "clipboard image write failed", arena); return; }
  }
  nv_ipc_reply_ok(w, seq, nv_json_object(arena), arena);
}

NV_INTERNAL void nv_op_clipboard_clear(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
  (void)args;
#ifdef __APPLE__
  nv_mac_clipboard_clear();
#elif defined(_WIN32)
  nv_win_clipboard_clear();
#elif defined(__linux__) && !defined(__APPLE__)
  nv_linux_clipboard_clear();
#endif
  nv_ipc_reply_ok(w, seq, nv_json_object(arena), arena);
}

NV_INTERNAL void nv_op_clipboard_has_text(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
  (void)args;
  int has = 0;
#ifdef __APPLE__
  has = nv_mac_clipboard_has_text();
#elif defined(_WIN32)
  has = nv_win_clipboard_has_text();
#elif defined(__linux__) && !defined(__APPLE__)
  has = nv_linux_clipboard_has_text();
#endif
  nv_json_t* obj = nv_json_object(arena);
  nv_json_bool(obj, "value", has ? 1 : 0);
  nv_ipc_reply_ok(w, seq, obj, arena);
}

NV_INTERNAL void nv_op_clipboard_has_image(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
  (void)args;
  int has = 0;
#ifdef __APPLE__
  has = nv_mac_clipboard_has_image();
#elif defined(_WIN32)
  has = nv_win_clipboard_has_image();
#elif defined(__linux__) && !defined(__APPLE__)
  has = nv_linux_clipboard_has_image();
#endif
  nv_json_t* obj = nv_json_object(arena);
  nv_json_bool(obj, "value", has ? 1 : 0);
  nv_ipc_reply_ok(w, seq, obj, arena);
}
