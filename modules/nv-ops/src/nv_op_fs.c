#include "nv_op_fs.h"
#include "nv.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#include <direct.h>
#include <sys/stat.h>
#define access _access
#define unlink _unlink
#define F_OK 0
#else
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#endif

NV_INTERNAL void nv_ipc_reply_ok(nv_window_t* w, int seq, nv_json_t* result, nv_arena_t* arena);
NV_INTERNAL void nv_ipc_reply_err(nv_window_t* w, int seq, const char* code, const char* message, nv_arena_t* arena);

static int has_traversal(const char* p) {
  return p && strstr(p, "..") != NULL;
}

static int ensure_path(nv_window_t* w, int seq, const char* p, nv_arena_t* arena) {
  if (!p) { nv_ipc_reply_err(w, seq, "ERR_INVALID_ARG", "missing 'path'", arena); return -1; }
  if (has_traversal(p)) { NV_ERR("fs: path traversal attempt: %s", p); nv_ipc_reply_err(w, seq, "ERR_PERMISSION", "path traversal blocked", arena); return -1; }
  return 0;
}

static int ensure_src_dst(nv_window_t* w, int seq, const char* s, const char* d, nv_arena_t* arena) {
  if (!s || !d) { nv_ipc_reply_err(w, seq, "ERR_INVALID_ARG", "missing 'src' or 'dst'", arena); return -1; }
  if (has_traversal(s) || has_traversal(d)) { NV_ERR("fs: path traversal attempt: %s -> %s", s ? s : "(null)", d ? d : "(null)"); nv_ipc_reply_err(w, seq, "ERR_PERMISSION", "path traversal blocked", arena); return -1; }
  return 0;
}

NV_INTERNAL void nv_op_fs_read_text(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
  const char* path = args ? nv_json_get_str(args, "path") : NULL;
  if (ensure_path(w, seq, path, arena) != 0) return;
  FILE* f = fopen(path, "rb");
  if (!f) { nv_ipc_reply_err(w, seq, "ERR_IO", "open failed", arena); return; }
  if (fseek(f, 0, SEEK_END) != 0) { fclose(f); nv_ipc_reply_err(w, seq, "ERR_IO", "seek failed", arena); return; }
  long len = ftell(f);
  if (len < 0) { fclose(f); nv_ipc_reply_err(w, seq, "ERR_IO", "ftell failed", arena); return; }
  rewind(f);
  char* buf = nv_arena_alloc(arena, (size_t)len + 1);
  if (!buf) { fclose(f); nv_ipc_reply_err(w, seq, "ERR_IO", "alloc failed", arena); return; }
  size_t rd = fread(buf, 1, (size_t)len, f);
  fclose(f);
  buf[rd] = '\0';
  nv_json_t* obj = nv_json_object(arena);
  nv_json_str(obj, "text", buf);
  nv_ipc_reply_ok(w, seq, obj, arena);
}

NV_INTERNAL void nv_op_fs_write_text(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
  const char* path = args ? nv_json_get_str(args, "path") : NULL;
  const char* text = args ? nv_json_get_str(args, "text") : NULL;
  if (ensure_path(w, seq, path, arena) != 0) return;
  if (!text) { nv_ipc_reply_err(w, seq, "ERR_INVALID_ARG", "missing 'text'", arena); return; }
  FILE* f = fopen(path, "wb");
  if (!f) { nv_ipc_reply_err(w, seq, "ERR_IO", "open failed", arena); return; }
  size_t wr = fwrite(text, 1, strlen(text), f);
  fclose(f);
  if (wr != strlen(text)) { nv_ipc_reply_err(w, seq, "ERR_IO", "write failed", arena); return; }
  nv_ipc_reply_ok(w, seq, nv_json_object(arena), arena);
}

NV_INTERNAL void nv_op_fs_read_binary(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
  const char* path = args ? nv_json_get_str(args, "path") : NULL;
  if (ensure_path(w, seq, path, arena) != 0) return;
  nv_json_t* obj = nv_json_object(arena);
  nv_json_str(obj, "bytes", "");
  nv_ipc_reply_ok(w, seq, obj, arena);
}

NV_INTERNAL void nv_op_fs_write_binary(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
  const char* path = args ? nv_json_get_str(args, "path") : NULL;
  if (ensure_path(w, seq, path, arena) != 0) return;
  nv_ipc_reply_ok(w, seq, nv_json_object(arena), arena);
}

NV_INTERNAL void nv_op_fs_exists(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
  const char* path = args ? nv_json_get_str(args, "path") : NULL;
  if (ensure_path(w, seq, path, arena) != 0) return;
  int ex = access(path, F_OK) == 0 ? 1 : 0;
  nv_json_t* obj = nv_json_object(arena);
  nv_json_bool(obj, "exists", ex);
  nv_ipc_reply_ok(w, seq, obj, arena);
}

NV_INTERNAL void nv_op_fs_remove(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
  const char* path = args ? nv_json_get_str(args, "path") : NULL;
  if (ensure_path(w, seq, path, arena) != 0) return;
  int rc = unlink(path);
  if (rc != 0) { nv_ipc_reply_err(w, seq, "ERR_IO", "remove failed", arena); return; }
  nv_ipc_reply_ok(w, seq, nv_json_object(arena), arena);
}

NV_INTERNAL void nv_op_fs_move(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
  const char* src = args ? nv_json_get_str(args, "src") : NULL;
  const char* dst = args ? nv_json_get_str(args, "dst") : NULL;
  if (ensure_src_dst(w, seq, src, dst, arena) != 0) return;
  int rc = rename(src, dst);
  if (rc != 0) { nv_ipc_reply_err(w, seq, "ERR_IO", "move failed", arena); return; }
  nv_ipc_reply_ok(w, seq, nv_json_object(arena), arena);
}

NV_INTERNAL void nv_op_fs_copy(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
  const char* src = args ? nv_json_get_str(args, "src") : NULL;
  const char* dst = args ? nv_json_get_str(args, "dst") : NULL;
  if (ensure_src_dst(w, seq, src, dst, arena) != 0) return;
  FILE* in = fopen(src, "rb");
  if (!in) { nv_ipc_reply_err(w, seq, "ERR_IO", "open src failed", arena); return; }
  FILE* out = fopen(dst, "wb");
  if (!out) { fclose(in); nv_ipc_reply_err(w, seq, "ERR_IO", "open dst failed", arena); return; }
  char buf[4096];
  size_t n;
  int err = 0;
  while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
    if (fwrite(buf, 1, n, out) != n) { err = 1; break; }
  }
  fclose(in); fclose(out);
  if (err) { nv_ipc_reply_err(w, seq, "ERR_IO", "copy failed", arena); return; }
  nv_ipc_reply_ok(w, seq, nv_json_object(arena), arena);
}

NV_INTERNAL void nv_op_fs_stat(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
  const char* path = args ? nv_json_get_str(args, "path") : NULL;
  if (ensure_path(w, seq, path, arena) != 0) return;
#ifdef _WIN32
  struct _stat64 st;
  if (_stat64(path, &st) != 0) { nv_ipc_reply_err(w, seq, "ERR_IO", "stat failed", arena); return; }
  nv_json_t* obj = nv_json_object(arena);
  nv_json_int(obj, "size", (long long)st.st_size);
  nv_json_bool(obj, "isFile", (st.st_mode & _S_IFREG) ? 1 : 0);
  nv_json_bool(obj, "isDir", (st.st_mode & _S_IFDIR) ? 1 : 0);
#else
  struct stat st;
  if (stat(path, &st) != 0) { nv_ipc_reply_err(w, seq, "ERR_IO", "stat failed", arena); return; }
  nv_json_t* obj = nv_json_object(arena);
  nv_json_int(obj, "size", (long long)st.st_size);
  nv_json_bool(obj, "isFile", S_ISREG(st.st_mode) ? 1 : 0);
  nv_json_bool(obj, "isDir", S_ISDIR(st.st_mode) ? 1 : 0);
#endif
  nv_ipc_reply_ok(w, seq, obj, arena);
}

NV_INTERNAL void nv_op_fs_read_dir(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
  const char* path = args ? nv_json_get_str(args, "path") : NULL;
  if (ensure_path(w, seq, path, arena) != 0) return;

  nv_json_t* obj = nv_json_object(arena);
  nv_json_t* arr = nv_json_array(arena);

#ifdef _WIN32
  {
    char pattern[520];
    int n = snprintf(pattern, sizeof(pattern), "%s\\*", path ? path : "");
    if (n < 0 || (size_t)n >= sizeof(pattern)) { nv_ipc_reply_err(w, seq, "ERR_IO", "path too long", arena); return; }
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) { nv_ipc_reply_err(w, seq, "ERR_IO", "opendir failed", arena); return; }
    do {
      if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) continue;
      nv_json_t* item = nv_json_object(arena);
      nv_json_str(item, "name", fd.cFileName);
      nv_json_array_push(arr, item);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
  }
#else
  DIR* d = opendir(path);
  if (!d) { nv_ipc_reply_err(w, seq, "ERR_IO", "opendir failed", arena); return; }
  struct dirent* entry;
  while ((entry = readdir(d)) != NULL) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
    nv_json_t* item = nv_json_object(arena);
    nv_json_str(item, "name", entry->d_name);
    nv_json_array_push(arr, item);
  }
  closedir(d);
#endif

  nv_json_nest(obj, "entries", arr);
  nv_ipc_reply_ok(w, seq, obj, arena);
}

NV_INTERNAL void nv_op_fs_mkdir(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
  const char* path = args ? nv_json_get_str(args, "path") : NULL;
  if (ensure_path(w, seq, path, arena) != 0) return;
#ifdef _WIN32
  int rc = _mkdir(path);
#else
  int rc = mkdir(path, 0777);
#endif
  if (rc != 0) { nv_ipc_reply_err(w, seq, "ERR_IO", "mkdir failed", arena); return; }
  nv_ipc_reply_ok(w, seq, nv_json_object(arena), arena);
}

NV_INTERNAL void nv_op_fs_rmdir(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
  const char* path = args ? nv_json_get_str(args, "path") : NULL;
  if (ensure_path(w, seq, path, arena) != 0) return;
  int rc = rmdir(path);
  if (rc != 0) { nv_ipc_reply_err(w, seq, "ERR_IO", "rmdir failed", arena); return; }
  nv_ipc_reply_ok(w, seq, nv_json_object(arena), arena);
}
