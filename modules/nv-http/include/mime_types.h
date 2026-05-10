/*
 * Copyright (c) 2026
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 */

/**
 * @file mime_types.h
 * @brief MIME type lookup from file extension.
 */

#ifndef MIME_TYPES_H
#define MIME_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Get MIME type from filename (uses extension, case-insensitive).
 * @return MIME type string (e.g. "text/html; charset=utf-8") or
 *         "application/octet-stream" if unknown
 */
const char* mime_type_from_filename(const char* filename);

#ifdef __cplusplus
}
#endif

#endif /* MIME_TYPES_H */
