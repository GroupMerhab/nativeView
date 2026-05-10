/*
 * Copyright (c) 2026
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 */

#ifndef NV_BASE64_H
#define NV_BASE64_H

#include "nv_util.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Bytes required for base64 text of `src_len` bytes, including trailing NUL. */
NV_API size_t nv_base64_encode_bound(size_t src_len);

/**
 * RFC 4648 base64-encode `src` into `dst`.
 *
 * @return length of encoded string (excluding NUL) on success, -1 if arguments
 *         are invalid or `dst_cap` is too small. Writes a terminating NUL.
 */
NV_API int nv_base64_encode(const uint8_t* src, size_t src_len, char* dst, size_t dst_cap);

/** Upper bound on decoded size for a base64 string of `b64_len` characters. */
NV_API size_t nv_base64_decode_max_out(size_t b64_len);

/**
 * Decode base64 from `src` (ASCII whitespace is ignored).
 *
 * @param out_len  if non-NULL, receives decoded byte count on success
 * @return 0 on success, -1 on invalid input or insufficient `dst_cap`
 */
NV_API int nv_base64_decode(const char* src, size_t src_len, uint8_t* dst, size_t dst_cap,
                            size_t* out_len);

#ifdef __cplusplus
}
#endif

#endif /* NV_BASE64_H */
