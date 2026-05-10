/*
 * Copyright (c) 2026
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 */

/**
 * @file url_parser.h
 * @brief URL path and query string parsing, URL-encode/decode.
 */

#ifndef URL_PARSER_H
#define URL_PARSER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Error Codes
 * ============================================================================= */

#define URL_OK                0
#define URL_ERROR_INVALID    -1
#define URL_ERROR_PATH_TOO_LONG -2

/* =============================================================================
 * Types
 * ============================================================================= */

typedef struct {
  char key[256];
  char value[4096];
} QueryParam;

typedef struct {
  char path[2048];
  QueryParam params[32];
  int param_count;
  char fragment[256];
} ParsedUrl;

/* =============================================================================
 * Parsing
 * ============================================================================= */

/**
 * Parse URL into path, query params, and fragment.
 * Caller provides ParsedUrl; no heap allocation.
 * @return URL_OK, URL_ERROR_INVALID, or URL_ERROR_PATH_TOO_LONG
 */
int url_parse(const char* url, ParsedUrl* parsed);

/**
 * Get query parameter value by key (case-sensitive).
 * @return Pointer to value or NULL if not found
 */
const char* url_get_param(const ParsedUrl* parsed, const char* key);

/**
 * Get query parameter by index.
 * @return 1 if index valid, 0 otherwise. Sets *out_key and *out_value.
 */
int url_get_param_by_index(const ParsedUrl* parsed, int index,
    const char** out_key, const char** out_value);

/* =============================================================================
 * Encode / Decode
 * ============================================================================= */

/**
 * Decode percent-encoded string. Handles %HH and + as space.
 * @return Decoded length, or -1 on error (null inputs)
 */
int url_decode(const char* encoded, char* decoded, size_t max_len);

/**
 * Encode string for URL. Unreserved chars (A-Za-z0-9-._~) pass through.
 * @return Encoded length, or -1 on error (null inputs)
 */
int url_encode(const char* plain, char* encoded, size_t max_len);

#ifdef __cplusplus
}
#endif

#endif /* URL_PARSER_H */
