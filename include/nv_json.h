/* Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0 */

/* =============================================================================
 * nv_json.h - JSON Builder API
 *
 * Write-only JSON builder for constructing C→JS IPC payloads.
 * Uses arena allocation, optimized for compact serialization.
 *
 * Apache 2.0 - See LICENSE for details
 * =============================================================================
 */

#ifndef NV_JSON_H
#define NV_JSON_H

#include "nv_arena.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Opaque JSON Value Type
 * =============================================================================
 */

/**
 * Opaque JSON value type for building JSON structures.
 * Values are immutable once created and backed by arena allocator.
 */
typedef struct nv_json_value nv_json_t;

/* =============================================================================
 * Object and Array Creation
 * =============================================================================
 */

/**
 * Create an empty JSON object.
 *
 * @param arena     Arena for allocation (required)
 * @return          New JSON object, or NULL on allocation failure
 *
 * Object can be populated with nv_json_str(), nv_json_int(), etc.
 * Serialize with nv_json_serialize().
 */
NV_API nv_json_t* nv_json_object(nv_arena_t* arena);

/**
 * Create an empty JSON array.
 *
 * @param arena     Arena for allocation (required)
 * @return          New JSON array, or NULL on allocation failure
 *
 * Array can be populated with nv_json_array_push().
 * Serialize with nv_json_serialize().
 */
NV_API nv_json_t* nv_json_array(nv_arena_t* arena);

/* =============================================================================
 * Object Property Methods (add key:value pairs)
 * =============================================================================
 */

/**
 * Add string property to JSON object.
 *
 * @param j         JSON object (NULL-safe)
 * @param key       Property name (must not be NULL for objects)
 * @param val       String value to store (copied, NULL becomes "null")
 *
 * Handles proper escaping: quotes, backslash, newlines, tabs, control chars.
 */
NV_API void nv_json_str(nv_json_t* j, const char* key, const char* val);

/**
 * Add integer property to JSON object.
 *
 * @param j         JSON object (NULL-safe)
 * @param key       Property name (must not be NULL for objects)
 * @param val       Integer value
 */
NV_API void nv_json_int(nv_json_t* j, const char* key, long long val);

/**
 * Add double property to JSON object.
 *
 * @param j         JSON object (NULL-safe)
 * @param key       Property name (must not be NULL for objects)
 * @param val       Double-precision value
 */
NV_API void nv_json_double(nv_json_t* j, const char* key, double val);

/**
 * Add boolean property to JSON object.
 *
 * @param j         JSON object (NULL-safe)
 * @param key       Property name (must not be NULL for objects)
 * @param val       Boolean (0 = false, non-zero = true)
 */
NV_API void nv_json_bool(nv_json_t* j, const char* key, int val);

/**
 * Add null property to JSON object.
 *
 * @param j         JSON object (NULL-safe)
 * @param key       Property name (must not be NULL for objects)
 */
NV_API void nv_json_null(nv_json_t* j, const char* key);

/**
 * Add nested object or array to JSON object.
 *
 * @param parent    Parent JSON object (NULL-safe)
 * @param key       Property name (must not be NULL for objects)
 * @param child     Child object or array to nest (takes ownership)
 *
 * Child becomes owned by parent and will be serialized as nested structure.
 */
NV_API void nv_json_nest(nv_json_t* parent, const char* key, nv_json_t* child);

/* =============================================================================
 * Array Element Methods
 * =============================================================================
 */

/**
 * Push element to JSON array.
 *
 * @param arr       JSON array (NULL-safe)
 * @param item      Element to push (can be any type including nested objects/arrays)
 *
 * Array takes ownership of element.
 * Elements are serialized in push order.
 */
NV_API void nv_json_array_push(nv_json_t* arr, nv_json_t* item);

/* =============================================================================
 * Serialization
 * =============================================================================
 */

/**
 * Serialize JSON to compact string.
 *
 * @param j         JSON object or array (NULL-safe)
 * @return          Null-terminated JSON string, or NULL on failure
 *
 * Output is compact (no whitespace). String is valid as long as arena exists.
 * Handles proper escaping of special characters in strings.
 */
NV_API const char* nv_json_serialize(nv_json_t* j);

/* =============================================================================
 * Parser API - Parse incoming JSON strings (JS→C IPC messages)
 * =============================================================================
 */

/**
 * Opaque JSON parsed value type.
 * Represents a parsed JSON value from input string.
 * Separate from builder nv_json_t - this is for reading, not writing.
 */
typedef struct nv_json_parsed nv_json_val_t;

/**
 * Parse JSON string from IPC input.
 *
 * @param arena     Arena for allocation (required, null results in error)
 * @param input     JSON input string (must be null-terminated)
 * @return          Parsed JSON value (NULL on parse error)
 *
 * Supports: string, integer, double, bool, null, object, array
 * On error: returns NULL and logs error with byte position via NV_ERR()
 * Never crashes on malformed input - validates gracefully.
 * Result is valid as long as arena exists.
 */
NV_API nv_json_val_t* nv_json_parse(nv_arena_t* arena, const char* input);

/* =============================================================================
 * Parsed Value Type Checking
 * =============================================================================
 */

/**
 * Check if parsed value is a JSON object.
 */
NV_API int nv_json_is_object(const nv_json_val_t* v);

/**
 * Check if parsed value is a JSON array.
 */
NV_API int nv_json_is_array(const nv_json_val_t* v);

/**
 * Check if parsed value is a string.
 */
NV_API int nv_json_is_string(const nv_json_val_t* v);

/**
 * Check if parsed value is a number (int or double).
 */
NV_API int nv_json_is_number(const nv_json_val_t* v);

/**
 * Serialize parsed JSON value to string.
 * Useful for forwarding parsed data.
 */
NV_API const char* nv_json_val_serialize(nv_arena_t* arena, const nv_json_val_t* val);

/* =============================================================================
 * Object Value Accessors (get property from object by key)
 * =============================================================================
 */

/**
 * Get string property from object.
 *
 * @param obj       Parsed object (NULL-safe)
 * @param key       Property name (NULL-safe)
 * @return          String value, or NULL if key not found or wrong type
 */
NV_API const char* nv_json_get_str(const nv_json_val_t* obj, const char* key);

/**
 * Get integer property from object.
 *
 * @param obj       Parsed object (NULL-safe)
 * @param key       Property name (NULL-safe)
 * @return          Integer value, or 0 if key not found or wrong type
 */
NV_API long long nv_json_get_int(const nv_json_val_t* obj, const char* key);

/**
 * Get double property from object.
 *
 * @param obj       Parsed object (NULL-safe)
 * @param key       Property name (NULL-safe)
 * @return          Double value, or 0.0 if key not found or wrong type
 */
NV_API double nv_json_get_double(const nv_json_val_t* obj, const char* key);

/**
 * Get boolean property from object.
 *
 * @param obj       Parsed object (NULL-safe)
 * @param key       Property name (NULL-safe)
 * @return          Boolean (0/1), or 0 if key not found or wrong type
 */
NV_API int nv_json_get_bool(const nv_json_val_t* obj, const char* key);

/**
 * Get nested object property from object.
 *
 * @param obj       Parsed object (NULL-safe)
 * @param key       Property name (NULL-safe)
 * @return          Nested object value, or NULL if key not found or wrong type
 */
NV_API nv_json_val_t* nv_json_get_obj(const nv_json_val_t* obj, const char* key);

/* =============================================================================
 * Array Accessors
 * =============================================================================
 */

/**
 * Get array length.
 *
 * @param arr       Parsed array (NULL-safe)
 * @return          Number of elements, or 0 if not an array or NULL
 */
NV_API size_t nv_json_array_len(const nv_json_val_t* arr);

/**
 * Get array element by index.
 *
 * @param arr       Parsed array (NULL-safe)
 * @param index     Element index (0-based)
 * @return          Array element value, or NULL if out of bounds or not an array
 */
NV_API nv_json_val_t* nv_json_array_get(const nv_json_val_t* arr, size_t index);

#ifdef __cplusplus
}
#endif

#endif /* NV_JSON_H */


