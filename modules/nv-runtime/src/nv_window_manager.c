#include "nv_window_manager.h"
#include <string.h>
#include <ctype.h>

static nv_wm_entry_t nv_wm_registry[NV_MAX_WINDOWS];
static int           nv_wm_initialized = 0;

void nv_wm_init(void) {
    memset(nv_wm_registry, 0, sizeof(nv_wm_registry));
    nv_wm_initialized = 1;
}

int nv_wm_id_valid(const char* id) {
    if (!id || !*id) return 0; // empty or null
    
    // Check for "main" which is reserved but valid?
    // User says: #define NV_WINDOW_ID_MAIN "main" // reserved id for first window
    // So "main" should be valid.
    
    // Check for "*" which is invalid
    if (strcmp(id, "*") == 0) return 0;
    
    size_t len = strlen(id);
    if (len >= NV_WINDOW_ID_MAX) return 0; // too long (MAX includes null)
    
    for (size_t i = 0; i < len; ++i) {
        char c = id[i];
        if (!isalnum(c) && c != '-' && c != '_') {
            return 0;
        }
    }
    return 1;
}

int nv_wm_register(const char* id, nv_window_t* w) {
    if (!nv_wm_initialized) nv_wm_init();
    
    if (!nv_wm_id_valid(id)) {
        NV_ERR("Invalid window ID: %s", id ? id : "NULL");
        return -1;
    }
    
    if (nv_wm_get_by_id(id) != NULL) {
        NV_ERR("Duplicate window ID: %s", id);
        return -1;
    }

    // Find free slot
    for (int i = 0; i < NV_MAX_WINDOWS; ++i) {
        if (!nv_wm_registry[i].active) {
            // Found free slot
            strncpy(nv_wm_registry[i].id, id, NV_WINDOW_ID_MAX - 1);
            nv_wm_registry[i].id[NV_WINDOW_ID_MAX - 1] = '\0';
            nv_wm_registry[i].window = w;
            nv_wm_registry[i].active = 1;
            return 0;
        }
    }

    NV_ERR("Window registry full (max %d)", NV_MAX_WINDOWS);
    return -1;
}

void nv_wm_unregister(const char* id) {
    if (!id) return;
    for (int i = 0; i < NV_MAX_WINDOWS; ++i) {
        if (nv_wm_registry[i].active && strcmp(nv_wm_registry[i].id, id) == 0) {
            nv_wm_registry[i].active = 0;
            nv_wm_registry[i].window = NULL;
            memset(nv_wm_registry[i].id, 0, NV_WINDOW_ID_MAX);
            return;
        }
    }
}

nv_window_t* nv_wm_get_by_id(const char* id) {
    if (!id) return NULL;
    for (int i = 0; i < NV_MAX_WINDOWS; ++i) {
        if (nv_wm_registry[i].active && strcmp(nv_wm_registry[i].id, id) == 0) {
            return nv_wm_registry[i].window;
        }
    }
    return NULL;
}

const char* nv_wm_get_id_by_window(const nv_window_t* w) {
    if (!w) return NULL;
    for (int i = 0; i < NV_MAX_WINDOWS; ++i) {
        if (nv_wm_registry[i].active && nv_wm_registry[i].window == w) {
            return nv_wm_registry[i].id;
        }
    }
    return NULL;
}

int nv_wm_list(nv_wm_entry_t* out, size_t* count) {
    if (!count) return -1;
    size_t max_count = *count;
    size_t found = 0;
    
    for (int i = 0; i < NV_MAX_WINDOWS; ++i) {
        if (nv_wm_registry[i].active) {
            if (out && found < max_count) {
                out[found] = nv_wm_registry[i];
            }
            found++;
        }
    }
    *count = found;
    return 0;
}

int nv_wm_count(void) {
    int count = 0;
    for (int i = 0; i < NV_MAX_WINDOWS; ++i) {
        if (nv_wm_registry[i].active) count++;
    }
    return count;
}
