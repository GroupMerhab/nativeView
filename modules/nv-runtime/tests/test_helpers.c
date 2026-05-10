#include "test_helpers.h"
#include "nv_window_internal.h"
#include "nv_ipc_internal.h"
#include "nv_json.h"
#include "nv_arena.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* Global test state */
static int g_last_ok = 0;
static int g_last_seq = 0;
static char* g_last_json = NULL;
static char g_last_err[64];

/* Reset state */
void test_reset_replies(void) {
    g_last_ok = 0;
    g_last_seq = 0;
    if (g_last_json) {
        free(g_last_json);
        g_last_json = NULL;
    }
    g_last_err[0] = '\0';
}

/* Getters */
int test_last_reply_ok(void) { return g_last_ok; }
const char* test_last_reply_json(void) { return g_last_json; }
const char* test_last_reply_error_code(void) { return g_last_err[0] ? g_last_err : NULL; }

/* Override nv_ipc_reply_ok (strong symbol overrides weak library symbol) */
void nv_ipc_reply_ok(nv_window_t* w, int seq, nv_json_t* result, nv_arena_t* arena) {
    (void)w;
    (void)arena;
    g_last_ok = 1;
    g_last_seq = seq;
    g_last_err[0] = '\0';
    
    char* s = result ? nv_json_serialize(result) : NULL;
    if (g_last_json) {
        free(g_last_json);
        g_last_json = NULL;
    }
    if (s) {
        g_last_json = strdup(s);
    }
}

/* Override nv_ipc_reply_err */
void nv_ipc_reply_err(nv_window_t* w, int seq, const char* code, const char* message, nv_arena_t* arena) {
    (void)w;
    (void)message;
    (void)arena;
    g_last_ok = 0;
    g_last_seq = seq;
    strncpy(g_last_err, code ? code : "ERR_UNKNOWN", sizeof(g_last_err)-1);
    g_last_err[sizeof(g_last_err)-1] = '\0';
    if (g_last_json) {
        free(g_last_json);
        g_last_json = NULL;
    }
}

/* Window creation helper */
nv_window_t* test_make_window(const char* title) {
    (void)title;
    nv_window_t* w = calloc(1, sizeof(nv_window_t));
    if (!w) return NULL;
    w->arena = nv_arena_create(16384);
    // Initialize IPC state if needed?
    // Ops might use nv_window_get_ipc(w)
    // nv_window_get_ipc returns &w->ipc.
    // w is zeroed, so ipc is zeroed.
    return w;
}

/* JSON parse helper */
nv_json_val_t* test_parse(nv_arena_t* arena, const char* s) {
    return nv_json_parse(arena, s);
}
