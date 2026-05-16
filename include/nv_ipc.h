/* Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0 */

/* =============================================================================
 * nv_ipc.h - Inter-Process Communication Module
 *
 * JSON-RPC 2.0 based messaging between C and JavaScript runtimes.
 * Compact wire-format for minimal payload size.
 *
 * Apache 2.0 - See LICENSE for details
 * =============================================================================
 */

#ifndef NV_IPC_H
#define NV_IPC_H

#include "nv_arena.h"
#include "nv_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Wire protocol version for C↔JS messages */
#ifndef NV_WIRE_VERSION
#define NV_WIRE_VERSION 1
#endif

/* =============================================================================
 * Types
 * =============================================================================
 */

/* Opaque IPC message handle */
typedef struct nv_ipc_message nv_ipc_message_t;

/* IPC message type */
typedef enum {
  NV_IPC_REQUEST,      /* Has id, expects response */
  NV_IPC_RESPONSE,     /* Response with result or error */
  NV_IPC_NOTIFICATION  /* No id, one-way message */
} nv_ipc_message_type_t;

/* =============================================================================
 * Message Construction
 * =============================================================================
 */

/**
 * Create a JSON-RPC 2.0 request (expects response).
 *
 * @param arena Memory arena for allocation
 * @param method Method name (e.g., "window.open", "app.getData")
 * @param id Request ID (>0), used to match responses
 * @return request message, NULL on error
 *
 * wire format: {"jsonrpc":"2.0","method":"...","params":{...},"id":N}
 * compact: {"j":"2.0","m":"...","p":{...},"i":N}
 */
NV_API nv_ipc_message_t* nv_ipc_request(nv_arena_t* arena, const char* method, int id);

/**
 * Create a JSON-RPC 2.0 response.
 *
 * @param arena Memory arena for allocation
 * @param request_id ID from corresponding request
 * @return response message, NULL on error
 *
 * wire format: {"jsonrpc":"2.0","result":{...},"id":N}
 * compact: {"j":"2.0","r":{...},"i":N}
 */
NV_API nv_ipc_message_t* nv_ipc_response(nv_arena_t* arena, int request_id);

/**
 * Create a JSON-RPC 2.0 error response.
 *
 * @param arena Memory arena for allocation
 * @param request_id ID from corresponding request
 * @param error_code Error code (standard: -32600 to -32000, custom: -32000+)
 * @param error_message Error description (optional)
 * @return error response message, NULL on error
 *
 * wire format: {"jsonrpc":"2.0","error":{"code":...,"message":"..."},"id":N}
 * compact: {"j":"2.0","e":{"c":...,"m":"..."},"i":N}
 */
NV_API nv_ipc_message_t* nv_ipc_error_response(nv_arena_t* arena, int request_id,
                                               int error_code, const char* error_message);

/**
 * Create a JSON-RPC 2.0 notification (one-way).
 *
 * @param arena Memory arena for allocation
 * @param method Method name
 * @return notification message, NULL on error
 *
 * wire format: {"jsonrpc":"2.0","method":"...","params":{...}}
 * custom: {"e":"event.name","d":{...}}  (for nativeview events)
 */
NV_API nv_ipc_message_t* nv_ipc_notify(nv_arena_t* arena, const char* method);

/* =============================================================================
 * Message Configuration
 * =============================================================================
 */

/**
 * Set message parameters/result as JSON object.
 *
 * @param msg Message to configure
 * @param content JSON object with parameters or result
 * @return 0 on success, -1 on error
 */
NV_API int nv_ipc_set_params(nv_ipc_message_t* msg, nv_json_t* content);

/**
 * Set message result (response only).
 *
 * @param msg Response message
 * @param content JSON object with result
 * @return 0 on success, -1 on error
 */
NV_API int nv_ipc_set_result(nv_ipc_message_t* msg, nv_json_t* content);

/**
 * Add event data (notification shorthand).
 * Sets "d" field for compact event messages.
 *
 * @param msg Notification message
 * @param data JSON object with event data
 * @return 0 on success, -1 on error
 */
NV_API int nv_ipc_set_event_data(nv_ipc_message_t* msg, nv_json_t* data);

/* =============================================================================
 * Serialization
 * =============================================================================
 */

/**
 * Serialize message to JSON string.
 *
 * @param msg Message to serialize
 * @return Compact JSON string (owned by message), NULL on error
 */
NV_API const char* nv_ipc_serialize(nv_ipc_message_t* msg);

/**
 * Get message size in bytes.
 *
 * @param msg Message
 * @return Size of serialized message, 0 if not yet serialized
 */
NV_API size_t nv_ipc_get_size(nv_ipc_message_t* msg);

/* =============================================================================
 * Parsing
 * =============================================================================
 */

/**
 * Parse JSON-RPC 2.0 message from string.
 *
 * @param arena Memory arena for allocation
 * @param json JSON string to parse
 * @param json_len Length of JSON string
 * @return Parsed message, NULL on parse error
 */
NV_API nv_ipc_message_t* nv_ipc_parse(nv_arena_t* arena, const char* json, size_t json_len);

/**
 * Get message type.
 *
 * @param msg Parsed message
 * @return Message type (REQUEST, RESPONSE, NOTIFICATION)
 */
NV_API nv_ipc_message_type_t nv_ipc_get_type(nv_ipc_message_t* msg);

/**
 * Get method name (for requests/notifications).
 *
 * @param msg Message
 * @return Method name, NULL if not applicable
 */
NV_API const char* nv_ipc_get_method(nv_ipc_message_t* msg);

/**
 * Get request ID (for requests/responses).
 *
 * @param msg Message
 * @return Request ID, -1 if not applicable
 */
NV_API int nv_ipc_get_id(nv_ipc_message_t* msg);

/**
 * Get parameters/result as JSON object.
 *
 * @param msg Message
 * @return JSON object (owned by message), NULL if not present
 */
NV_API nv_json_t* nv_ipc_get_params(nv_ipc_message_t* msg);

/**
 * Get error information.
 *
 * @param msg Error response message
 * @param out_code Pointer to receive error code
 * @param out_message Pointer to receive error message
 * @return 0 if valid error response, -1 otherwise
 */
NV_API int nv_ipc_get_error(nv_ipc_message_t* msg, int* out_code, const char** out_message);

/* =============================================================================
 * Utilities
 * =============================================================================
 */

/**
 * Destroy message and free associated resources.
 * Automatically called when arena is destroyed.
 *
 * @param msg Message to destroy
 */
NV_API void nv_ipc_destroy(nv_ipc_message_t* msg);

#ifdef __cplusplus
}
#endif

#endif /* NV_IPC_H */
