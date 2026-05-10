/*
 * Copyright (c) 2026
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 */

#include "nv_base64.h"
#include <string.h>

static const char g_enc[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int8_t g_dec[256];

static void nv_base64_init_dec(void) {
  static int inited;
  size_t i;
  if (inited) return;
  memset(g_dec, -1, sizeof(g_dec));
  for (i = 0; i < 64u; i++) {
    unsigned char c = (unsigned char)g_enc[i];
    g_dec[c] = (int8_t)i;
  }
  inited = 1;
}

size_t nv_base64_encode_bound(size_t src_len) {
  if (src_len == 0) return 1u;
  return 4u * ((src_len + 2u) / 3u) + 1u;
}

int nv_base64_encode(const uint8_t* src, size_t src_len, char* dst, size_t dst_cap) {
  size_t o = 0;
  size_t i;
  if (!dst || dst_cap == 0) return -1;
  if (!src && src_len > 0) return -1;
  if (nv_base64_encode_bound(src_len) > dst_cap) return -1;
  if (src_len == 0) {
    dst[0] = '\0';
    return 0;
  }
  for (i = 0; i + 2u < src_len; i += 3u) {
    uint32_t v = ((uint32_t)src[i] << 16) | ((uint32_t)src[i + 1u] << 8) | (uint32_t)src[i + 2u];
    dst[o++] = g_enc[(v >> 18) & 63u];
    dst[o++] = g_enc[(v >> 12) & 63u];
    dst[o++] = g_enc[(v >> 6) & 63u];
    dst[o++] = g_enc[v & 63u];
  }
  {
    size_t rem = src_len - i;
    if (rem == 1u) {
      uint32_t v = (uint32_t)src[i] << 16;
      dst[o++] = g_enc[(v >> 18) & 63u];
      dst[o++] = g_enc[(v >> 12) & 63u];
      dst[o++] = '=';
      dst[o++] = '=';
    } else if (rem == 2u) {
      uint32_t v = ((uint32_t)src[i] << 16) | ((uint32_t)src[i + 1u] << 8);
      dst[o++] = g_enc[(v >> 18) & 63u];
      dst[o++] = g_enc[(v >> 12) & 63u];
      dst[o++] = g_enc[(v >> 6) & 63u];
      dst[o++] = '=';
    }
  }
  dst[o] = '\0';
  return (int)o;
}

size_t nv_base64_decode_max_out(size_t b64_len) {
  return (b64_len / 4u) * 3u + 3u;
}

int nv_base64_decode(const char* src, size_t src_len, uint8_t* dst, size_t dst_cap,
                     size_t* out_len) {
  int qv[4];
  int nq = 0;
  size_t ri = 0;
  size_t wo = 0;
  if (!dst && dst_cap > 0) return -1;
  if (dst_cap > 0 && !dst) return -1;
  nv_base64_init_dec();
  if (!src) {
    if (src_len > 0) return -1;
    src = "";
  }
  for (;;) {
    while (nq < 4 && ri < src_len) {
      unsigned char c = (unsigned char)src[ri++];
      int v;
      if (c == ' ' || c == '\t' || c == '\r' || c == '\n') continue;
      if (c == '=') {
        qv[nq++] = -1;
        continue;
      }
      v = g_dec[c];
      if (v < 0) return -1;
      qv[nq++] = v;
    }
    if (nq == 0) break;
    if (nq < 4) return -1;
    {
      uint32_t a = (uint32_t)(uint8_t)qv[0];
      uint32_t b = (uint32_t)(uint8_t)qv[1];
      uint32_t c = qv[2] < 0 ? 0u : (uint32_t)(uint8_t)qv[2];
      uint32_t d = qv[3] < 0 ? 0u : (uint32_t)(uint8_t)qv[3];
      uint32_t pack = (a << 18) | (b << 12) | (c << 6) | d;
      if (qv[2] < 0 && qv[3] < 0) {
        if (wo + 1u > dst_cap) return -1;
        dst[wo++] = (uint8_t)((pack >> 16) & 0xFFu);
      } else if (qv[3] < 0) {
        if (qv[2] < 0) return -1;
        if (wo + 2u > dst_cap) return -1;
        dst[wo++] = (uint8_t)((pack >> 16) & 0xFFu);
        dst[wo++] = (uint8_t)((pack >> 8) & 0xFFu);
      } else {
        if (wo + 3u > dst_cap) return -1;
        dst[wo++] = (uint8_t)((pack >> 16) & 0xFFu);
        dst[wo++] = (uint8_t)((pack >> 8) & 0xFFu);
        dst[wo++] = (uint8_t)(pack & 0xFFu);
      }
    }
    nq = 0;
    if (qv[2] < 0 || qv[3] < 0) break;
  }
  while (ri < src_len) {
    unsigned char c = (unsigned char)src[ri++];
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n') continue;
    return -1;
  }
  if (out_len) *out_len = wo;
  return 0;
}
