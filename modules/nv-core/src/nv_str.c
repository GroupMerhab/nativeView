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

/* Safe dynamic string builder backed by arena allocator */

#include "nv_str.h"
#include "nv_util.h"

#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#define NV_STR_INITIAL_CAPACITY 256

/* =============================================================================
 * String Builder Structure
 * =============================================================================
 */

struct nv_str {
  nv_arena_t* arena;      /* Arena for allocations */
  char*       buffer;     /* Dynamically allocated buffer */
  size_t      length;     /* Current string length (excluding null) */
  size_t      capacity;   /* Total buffer capacity */
};

/* =============================================================================
 * Helper Functions
 * =============================================================================
 */

/**
 * Ensure buffer has at least required_capacity bytes.
 * If buffer is too small, allocates a new larger buffer from arena.
 * Returns true on success, false on failure (logs error).
 */
NV_INTERNAL int nv_str_ensure_capacity(nv_str_t* s, size_t required_capacity) {
  if (s->capacity >= required_capacity) {
    return 1;  /* Already have enough */
  }
  
  /* Need more space; allocate larger buffer */
  size_t new_capacity = s->capacity * 2;
  if (new_capacity < required_capacity) {
    new_capacity = required_capacity;
  }
  
  char* new_buffer = (char*)nv_arena_alloc(s->arena, new_capacity);
  if (new_buffer == NULL) {
    NV_ERR("String: arena allocation failed for capacity %zu", new_capacity);
    return 0;
  }
  
  /* Copy existing data to new buffer */
  if (s->buffer != NULL && s->length > 0) {
    memcpy(new_buffer, s->buffer, s->length);
  }
  
  s->buffer = new_buffer;
  s->capacity = new_capacity;
  return 1;
}

/**
 * Compute strlen safely (max length check).
 */
NV_INTERNAL size_t nv_str_strlen(const char* s) {
  if (s == NULL) return 0;
  size_t len = 0;
  while (s[len] != '\0' && len < (1 << 20)) {  /* Max 1MB string */
    len++;
  }
  return len;
}

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

NV_API nv_str_t* nv_str_create(nv_arena_t* arena) {
  if (arena == NULL) {
    NV_ERR("String: creation from NULL arena");
    return NULL;
  }
  
  nv_str_t* s = (nv_str_t*)nv_arena_alloc(arena, sizeof(nv_str_t));
  if (s == NULL) {
    NV_ERR("String: arena allocation failed for structure");
    return NULL;
  }
  
  char* buffer = (char*)nv_arena_alloc(arena, NV_STR_INITIAL_CAPACITY);
  if (buffer == NULL) {
    NV_ERR("String: arena allocation failed for initial buffer");
    return NULL;
  }
  
  s->arena = arena;
  s->buffer = buffer;
  s->length = 0;
  s->capacity = NV_STR_INITIAL_CAPACITY;
  s->buffer[0] = '\0';  /* Initially empty */
  
  return s;
}

NV_API void nv_str_append(nv_str_t* s, const char* data) {
  if (s == NULL || data == NULL) {
    return;
  }
  
  size_t len = nv_str_strlen(data);
  nv_str_append_len(s, data, len);
}

NV_API void nv_str_append_len(nv_str_t* s, const char* data, size_t len) {
  if (s == NULL || data == NULL || len == 0) {
    return;
  }
  
  NV_ASSERT(s->buffer != NULL);
  NV_ASSERT(s->length <= s->capacity);
  
  /* Check if appending would overflow */
  size_t required_len = s->length + len + 1;  /* +1 for null terminator */
  
  if (required_len > s->capacity) {
    if (!nv_str_ensure_capacity(s, required_len)) {
      NV_ERR("String: overflow - cannot append %zu bytes (have %zu available)",
             len, s->capacity - s->length);
      return;
    }
  }
  
  /* Copy data */
  memcpy(&s->buffer[s->length], data, len);
  s->length += len;
  s->buffer[s->length] = '\0';  /* Always null-terminate */
}

NV_API void nv_str_appendf(nv_str_t* s, const char* fmt, ...) {
  if (s == NULL || fmt == NULL) {
    return;
  }
  
  /* Format into temporary buffer on stack */
  char temp[512];
  va_list args;
  va_start(args, fmt);
  int result = vsnprintf(temp, sizeof(temp), fmt, args);
  va_end(args);
  
  if (result < 0) {
    NV_ERR("String: vsnprintf failed");
    return;
  }
  
  size_t formatted_len = (size_t)result;
  if (formatted_len >= sizeof(temp)) {
    /* Formatted string was truncated by temp buffer */
    formatted_len = sizeof(temp) - 1;
  }
  
  nv_str_append_len(s, temp, formatted_len);
}

NV_API const char* nv_str_cstr(const nv_str_t* s) {
  if (s == NULL || s->buffer == NULL) {
    return "";
  }
  return s->buffer;
}

NV_API size_t nv_str_len(const nv_str_t* s) {
  if (s == NULL) {
    return 0;
  }
  return s->length;
}

NV_API void nv_str_reset(nv_str_t* s) {
  if (s == NULL) {
    return;
  }
  
  s->length = 0;
  if (s->buffer != NULL) {
    s->buffer[0] = '\0';
  }
}

