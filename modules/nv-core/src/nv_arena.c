/* =============================================================================
 * nv_arena.c - Arena Allocator
 *
 * Fast bump-pointer arena allocator for IPC and message processing.
 * Apache 2.0 - See LICENSE for details
 * =============================================================================
 */

/* Fast bump-pointer arena allocator for IPC and message processing */

#include "nv_arena.h"
#include "nv_util.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define NV_ARENA_ALIGN 8

/* =============================================================================
 * Fixed Arena Implementation
 * =============================================================================
 */

struct nv_arena {
  enum {
    NV_ARENA_FIXED,
    NV_ARENA_POOL
  } type;
  
  union {
    struct {
      char*  buffer;        /* Pre-allocated buffer */
      size_t capacity;      /* Total size of buffer */
      size_t cursor;        /* Current allocation offset */
    } fixed;
    
    struct {
      char** chunks;        /* Array of chunk pointers */
      size_t chunk_size;    /* Size of each chunk */
      size_t max_chunks;    /* Capacity of chunks array */
      size_t num_chunks;    /* Currently allocated chunks */
      size_t chunk_cursor;  /* Current chunk index */
      size_t cursor;        /* Offset within current chunk */
      int growable;         /* 1 = realloc chunks when full */
      /* Overflow: allocations > chunk_size */
      char** overflow_blocks;
      size_t num_overflow;
      size_t max_overflow;
    } pool;
  } state;
};

/**
 * Round up size to next multiple of NV_ARENA_ALIGN.
 */
NV_INTERNAL size_t nv_arena_align_size(size_t size) {
  return (size + NV_ARENA_ALIGN - 1) & ~(NV_ARENA_ALIGN - 1);
}

/**
 * Allocate from fixed arena.
 */
NV_INTERNAL void* nv_arena_alloc_fixed(nv_arena_t* arena, size_t size) {
  NV_ASSERT(arena != NULL);
  
  size_t aligned_size = nv_arena_align_size(size);
  size_t remaining = arena->state.fixed.capacity - arena->state.fixed.cursor;
  
  if (aligned_size > remaining) {
    NV_ERR("Arena overflow: need %zu bytes, have %zu available",
           aligned_size, remaining);
    return NULL;
  }
  
  void* ptr = &arena->state.fixed.buffer[arena->state.fixed.cursor];
  arena->state.fixed.cursor += aligned_size;
  
  return ptr;
}

/**
 * Allocate from pool arena, allocating new chunks as needed.
 */
NV_INTERNAL void* nv_arena_alloc_pool(nv_arena_t* arena, size_t size) {
  NV_ASSERT(arena != NULL);
  
  size_t aligned_size = nv_arena_align_size(size);
  size_t chunk_size = arena->state.pool.chunk_size;

  /* Overflow: allocation larger than one chunk; use malloc and track for reset/destroy */
  if (aligned_size > chunk_size) {
    char* ptr = (char*)malloc(aligned_size);
    if (ptr == NULL) {
      NV_ERR("Arena pool overflow: malloc failed for %zu bytes", aligned_size);
      return NULL;
    }
    /* Grow overflow array if needed */
    if (arena->state.pool.num_overflow >= arena->state.pool.max_overflow) {
      size_t new_max = arena->state.pool.max_overflow == 0 ? 8 : arena->state.pool.max_overflow * 2;
      char** grown = (char**)realloc(arena->state.pool.overflow_blocks,
                                     sizeof(char*) * new_max);
      if (!grown) {
        NV_ERR("Arena pool overflow: realloc failed for overflow list");
        free(ptr);
        return NULL;
      }
      arena->state.pool.overflow_blocks = grown;
      arena->state.pool.max_overflow = new_max;
    }
    arena->state.pool.overflow_blocks[arena->state.pool.num_overflow++] = ptr;
    return ptr;
  }

  while (1) {
    if (arena->state.pool.chunk_cursor >= arena->state.pool.num_chunks) {
      /* Need to allocate a new chunk */
      if (arena->state.pool.num_chunks >= arena->state.pool.max_chunks) {
        if (!arena->state.pool.growable) {
          NV_ERR("Arena pool exhausted: max_chunks=%zu reached",
                 arena->state.pool.max_chunks);
          return NULL;
        }
        /* Grow chunks array: double capacity */
        size_t new_max = arena->state.pool.max_chunks * 2;
        if (new_max < 16) new_max = 16;
        char** grown = (char**)realloc(arena->state.pool.chunks,
                                       sizeof(char*) * new_max);
        if (!grown) {
          NV_ERR("Arena pool: realloc failed for %zu chunks", new_max);
          return NULL;
        }
        arena->state.pool.chunks = grown;
        arena->state.pool.max_chunks = new_max;
      }
      
      char* new_chunk = (char*)malloc(arena->state.pool.chunk_size);
      if (new_chunk == NULL) {
        NV_ERR("Arena pool: malloc failed for chunk_size=%zu",
               arena->state.pool.chunk_size);
        return NULL;
      }
      
      arena->state.pool.chunks[arena->state.pool.num_chunks] = new_chunk;
      arena->state.pool.num_chunks++;
      arena->state.pool.cursor = 0;
    }
    
    char* current_chunk = arena->state.pool.chunks[arena->state.pool.chunk_cursor];
    size_t remaining = arena->state.pool.chunk_size - arena->state.pool.cursor;
    
    if (aligned_size <= remaining) {
      /* Allocation fits in current chunk */
      void* ptr = &current_chunk[arena->state.pool.cursor];
      arena->state.pool.cursor += aligned_size;
      return ptr;
    } else {
      /* Move to next chunk */
      arena->state.pool.chunk_cursor++;
      arena->state.pool.cursor = 0;
    }
  }
}

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

NV_API nv_arena_t* nv_arena_create(size_t capacity) {
  if (capacity == 0) {
    NV_ERR("Arena: capacity must be > 0");
    return NULL;
  }
  
  nv_arena_t* arena = (nv_arena_t*)malloc(sizeof(nv_arena_t));
  if (arena == NULL) {
    NV_ERR("Arena: malloc failed for arena structure");
    return NULL;
  }
  
  char* buffer = (char*)malloc(capacity);
  if (buffer == NULL) {
    NV_ERR("Arena: malloc failed for buffer size %zu", capacity);
    free(arena);
    return NULL;
  }
  
  arena->type = NV_ARENA_FIXED;
  arena->state.fixed.buffer = buffer;
  arena->state.fixed.capacity = capacity;
  arena->state.fixed.cursor = 0;
  
  return arena;
}

NV_API void* nv_arena_alloc(nv_arena_t* arena, size_t size) {
  if (arena == NULL) {
    NV_ERR("Arena: allocation from NULL arena");
    return NULL;
  }
  
  if (size == 0) {
    NV_ERR("Arena: alloc size must be > 0");
    return NULL;
  }
  
  if (arena->type == NV_ARENA_FIXED) {
    return nv_arena_alloc_fixed(arena, size);
  } else {
    return nv_arena_alloc_pool(arena, size);
  }
}

NV_API char* nv_arena_str_dup(nv_arena_t* arena, const char* str) {
  const char* s = str ? str : "";
  size_t n;
  char* copy;

  if (!arena) {
    NV_ERR("Arena: str_dup from NULL arena");
    return NULL;
  }
  n = strlen(s) + 1;
  copy = (char*)nv_arena_alloc(arena, n);
  if (!copy) return NULL;
  memcpy(copy, s, n);
  return copy;
}

NV_API void nv_arena_reset(nv_arena_t* arena) {
  if (arena == NULL) {
    return;
  }
  
  if (arena->type == NV_ARENA_FIXED) {
    arena->state.fixed.cursor = 0;
  } else {
    arena->state.pool.chunk_cursor = 0;
    arena->state.pool.cursor = 0;
    /* Free all overflow blocks */
    for (size_t i = 0; i < arena->state.pool.num_overflow; i++) {
      free(arena->state.pool.overflow_blocks[i]);
    }
    arena->state.pool.num_overflow = 0;
  }
}

NV_API void nv_arena_destroy(nv_arena_t* arena) {
  if (arena == NULL) {
    return;
  }
  
  if (arena->type == NV_ARENA_FIXED) {
    free(arena->state.fixed.buffer);
  } else {
    /* Free overflow blocks first */
    for (size_t i = 0; i < arena->state.pool.num_overflow; i++) {
      free(arena->state.pool.overflow_blocks[i]);
    }
    free(arena->state.pool.overflow_blocks);
    for (size_t i = 0; i < arena->state.pool.num_chunks; i++) {
      free(arena->state.pool.chunks[i]);
    }
    free(arena->state.pool.chunks);
  }
  
  free(arena);
}

NV_API nv_arena_t* nv_arena_create_pool_growable(size_t chunk_size, size_t initial_max_chunks) {
  size_t max_chunks = initial_max_chunks > 0 ? initial_max_chunks : 4;
  if (chunk_size == 0) {
    NV_ERR("Arena pool: chunk_size must be > 0");
    return NULL;
  }
  if (max_chunks == 0) {
    NV_ERR("Arena pool: max_chunks must be > 0");
    return NULL;
  }
  nv_arena_t* arena = (nv_arena_t*)malloc(sizeof(nv_arena_t));
  if (arena == NULL) {
    NV_ERR("Arena pool: malloc failed for arena structure");
    return NULL;
  }
  char** chunks = (char**)malloc(sizeof(char*) * max_chunks);
  if (chunks == NULL) {
    NV_ERR("Arena pool: malloc failed for chunks array");
    free(arena);
    return NULL;
  }
  arena->type = NV_ARENA_POOL;
  arena->state.pool.chunks = chunks;
  arena->state.pool.chunk_size = chunk_size;
  arena->state.pool.max_chunks = max_chunks;
  arena->state.pool.num_chunks = 0;
  arena->state.pool.chunk_cursor = 0;
  arena->state.pool.cursor = 0;
  arena->state.pool.growable = 1;
  arena->state.pool.overflow_blocks = NULL;
  arena->state.pool.num_overflow = 0;
  arena->state.pool.max_overflow = 0;
  return arena;
}

