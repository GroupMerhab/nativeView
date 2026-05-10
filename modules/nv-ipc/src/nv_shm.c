/* =============================================================================
 * nv_shm.c - Shared Memory for Fast C↔JS Data Path
 *
 * POSIX: shm_open + mmap
 * Windows: CreateFileMapping + MapViewOfFile
 *
 * Apache 2.0 - See LICENSE for details
 * =============================================================================
 */

#include "nv_shm.h"
#include "util/nv_log.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#if defined(_WIN32) || defined(_WIN64)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

struct nv_shm {
  char name[64];
  size_t size;
  void* ptr;
#if defined(_WIN32) || defined(_WIN64)
  HANDLE hMapping;
  HANDLE hFile;
#else
  int fd;
#endif
};

NV_API nv_shm_t* nv_shm_create(const char* name, size_t size) {
  if (!name || name[0] == '\0' || size == 0 || (size % 4) != 0) {
    NV_ERR("nv_shm: invalid name/size");
    return NULL;
  }

  nv_shm_t* shm = (nv_shm_t*)malloc(sizeof(nv_shm_t));
  if (!shm) return NULL;
  memset(shm, 0, sizeof(nv_shm_t));

  size_t nlen = strlen(name);
  if (nlen >= sizeof(shm->name)) nlen = sizeof(shm->name) - 1;
  memcpy(shm->name, name, nlen);
  shm->name[nlen] = '\0';
  shm->size = size;
  shm->ptr = NULL;

#if defined(_WIN32) || defined(_WIN64)
  shm->hFile = INVALID_HANDLE_VALUE;
  shm->hMapping = NULL;

  wchar_t wname[128];
  size_t i;
  for (i = 0; i < nlen && i < 127; i++)
    wname[i] = (wchar_t)(unsigned char)name[i];
  wname[i] = L'\0';

  shm->hMapping = CreateFileMappingW(
    INVALID_HANDLE_VALUE,
    NULL,
    PAGE_READWRITE,
    (DWORD)((size >> 32) & 0xFFFFFFFF),
    (DWORD)(size & 0xFFFFFFFF),
    wname);
  if (!shm->hMapping) {
    NV_ERR("nv_shm: CreateFileMapping failed");
    free(shm);
    return NULL;
  }

  shm->ptr = MapViewOfFile(shm->hMapping, FILE_MAP_ALL_ACCESS, 0, 0, size);
  if (!shm->ptr) {
    NV_ERR("nv_shm: MapViewOfFile failed");
    CloseHandle(shm->hMapping);
    free(shm);
    return NULL;
  }
#else
  shm->fd = -1;

  /* POSIX shm_open name: must start with /, no further slashes; sanitize */
  char path[128];
  size_t j = 0;
  path[j++] = '/';
  for (size_t i = 0; name[i] && j < sizeof(path) - 1; i++) {
    char c = name[i];
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')
      path[j++] = c;
  }
  path[j] = '\0';
  if (j <= 1) { path[1] = 'x'; path[2] = '\0'; }

  shm->fd = shm_open(path, O_CREAT | O_RDWR, 0600);
  if (shm->fd < 0) {
    NV_ERR("nv_shm: shm_open failed");
    free(shm);
    return NULL;
  }

  if (ftruncate(shm->fd, (off_t)size) != 0) {
    NV_ERR("nv_shm: ftruncate failed");
    close(shm->fd);
    shm_unlink(path);
    free(shm);
    return NULL;
  }

  shm->ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, shm->fd, 0);
  if (shm->ptr == MAP_FAILED) {
    NV_ERR("nv_shm: mmap failed");
    close(shm->fd);
    shm_unlink(path);
    free(shm);
    return NULL;
  }
#endif

  return shm;
}

NV_API void nv_shm_destroy(nv_shm_t* shm) {
  if (!shm) return;

#if defined(_WIN32) || defined(_WIN64)
  if (shm->ptr) UnmapViewOfFile(shm->ptr);
  if (shm->hMapping) CloseHandle(shm->hMapping);
  if (shm->hFile != INVALID_HANDLE_VALUE) CloseHandle(shm->hFile);
#else
  if (shm->ptr && shm->ptr != MAP_FAILED)
    munmap(shm->ptr, shm->size);
  if (shm->fd >= 0) {
    close(shm->fd);
    char path[128];
    size_t j = 0;
    path[j++] = '/';
    for (size_t i = 0; shm->name[i] && j < sizeof(path) - 1; i++) {
      char c = shm->name[i];
      if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')
        path[j++] = c;
    }
    path[j] = '\0';
    if (j > 1) shm_unlink(path);
  }
#endif

  free(shm);
}

NV_API void* nv_shm_ptr(nv_shm_t* shm) {
  return shm ? shm->ptr : NULL;
}

NV_API size_t nv_shm_size(nv_shm_t* shm) {
  return shm ? shm->size : 0;
}

NV_API int nv_shm_write_f32(nv_shm_t* shm, size_t offset, float value) {
  if (!shm || !shm->ptr || (offset % 4) != 0 || offset + 4 > shm->size)
    return -1;
  *(float*)((char*)shm->ptr + offset) = value;
  return 0;
}

NV_API const char* nv_shm_name(nv_shm_t* shm) {
  return shm ? shm->name : NULL;
}
