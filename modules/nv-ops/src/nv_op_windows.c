#include "nv_op_windows.h"
#include "nv_window_manager.h"
#include "nv_window_internal.h"
#include "nv_ipc_internal.h"
#include "nv_json.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// External helpers from nv_ipc.c (or similar)
NV_INTERNAL void nv_ipc_reply_ok(nv_window_t* w, int seq, nv_json_t* result, nv_arena_t* arena);
NV_INTERNAL void nv_ipc_reply_err(nv_window_t* w, int seq, const char* code, const char* message, nv_arena_t* arena);

// External helpers from nv_window.c (not in nv.h but available for linking)
int nv_window_get_size(nv_window_t* window, int* out_width, int* out_height);
int nv_window_get_position(nv_window_t* window, int* out_x, int* out_y);
const char* nv_window_get_title(nv_window_t* window);

// Helper to push event to all active windows
NV_INTERNAL void nv_op_windows_push_all(const char* event, nv_json_t* data, nv_arena_t* arena) {
    nv_wm_entry_t entries[NV_MAX_WINDOWS];
    size_t count = NV_MAX_WINDOWS;
    nv_wm_list(entries, &count);
    
    if (count == 0) return;

    const char* json_str = nv_json_serialize(data);
    if (!json_str) json_str = "null";

    for (size_t i = 0; i < count; i++) {
        if (entries[i].active && entries[i].window) {
            nv_send(entries[i].window, event, json_str);
        }
    }
}

// Close callback for windows opened via windows.open
static void on_created_window_close(nv_window_t* window, void* userdata) {
    (void)userdata;
    const char* id = nv_wm_get_id_by_window(window);
    if (id) {
        // Create a temporary arena for the push event
        nv_arena_t* arena = nv_arena_create(1024);
        
        // Notify beforeClose listeners
        nv_json_t* before = nv_json_object(arena);
        nv_json_str(before, "id", id);
        nv_op_windows_push_all("windows.beforeClose", before, arena);
        
        // Push windows.closed event
        nv_json_t* payload = nv_json_object(arena);
        nv_json_str(payload, "id", id);
        nv_op_windows_push_all("windows.closed", payload, arena);
        
        // Unregister from manager
        nv_wm_unregister(id);
        
        nv_arena_destroy(arena);
    }
}

NV_INTERNAL void nv_op_windows_open(nv_window_t* sender, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
    // 1. Extract args
    const char* id = nv_json_get_str(args, "id");
    const char* url = nv_json_get_str(args, "url");
    
    if (!id) {
        nv_ipc_reply_err(sender, seq, "ERR_INVALID_ARG", "missing 'id'", arena);
        return;
    }
    
    // 2. Validate ID
    if (!nv_wm_id_valid(id)) {
        nv_ipc_reply_err(sender, seq, "ERR_INVALID_ARG", "invalid 'id' format", arena);
        return;
    }
    
    // 3. Check if exists
    if (nv_wm_get_by_id(id) != NULL) {
        nv_ipc_reply_err(sender, seq, "ERR_ALREADY_EXISTS", "window id already exists", arena);
        return;
    }
    
    // 4. Build config
    nv_window_cfg_t cfg = {0};
    cfg.title = nv_json_get_str(args, "title");
    
    long long w = nv_json_get_int(args, "width");
    long long h = nv_json_get_int(args, "height");
    cfg.width = (w > 0) ? (int)w : 800;
    cfg.height = (h > 0) ? (int)h : 600;
    
    long long min_w = nv_json_get_int(args, "minWidth");
    long long min_h = nv_json_get_int(args, "minHeight");
    cfg.min_width = (int)min_w;
    cfg.min_height = (int)min_h;
    
    // Default resizable to 1
    cfg.resizable = 1;
    // If user provided frameless or devtools, set them
    cfg.frameless = nv_json_get_bool(args, "frameless");
    cfg.devtools = nv_json_get_bool(args, "devtools");
    cfg.modal = nv_json_get_bool(args, "modal");
    
    // 5. Create window
    nv_window_t* new_win = nv_window_create(sender->app, &cfg);
    if (!new_win) {
        nv_ipc_reply_err(sender, seq, "ERR_CREATE_FAILED", "failed to create window", arena);
        return;
    }
    
    // Remove auto-generated ID to prevent double registration
    const char* auto_id = nv_wm_get_id_by_window(new_win);
    if (auto_id) {
        nv_wm_unregister(auto_id);
    }
    
    // 6. Register
    if (nv_wm_register(id, new_win) != 0) {
        nv_window_destroy(new_win);
        nv_ipc_reply_err(sender, seq, "ERR_REGISTER_FAILED", "failed to register window", arena);
        return;
    }
    
    // 7. Load URL
    if (url) {
        nv_load_url(new_win, url);
    }
    
    // 7b. Apply modal if requested
    if (cfg.modal) {
        nv_window_platform_set_modal(new_win, 1);
    }
    
    // 7c. Show window
    nv_window_show(new_win);
    
    // 8. Register close callback
    nv_window_on_close(new_win, on_created_window_close, NULL);
    
    // 9. Push "windows.opened"
    nv_json_t* push_data = nv_json_object(arena);
    nv_json_str(push_data, "id", id);
    nv_op_windows_push_all("windows.opened", push_data, arena);
    
    // 10. Reply OK
    nv_json_t* res = nv_json_object(arena);
    nv_json_str(res, "id", id);
    nv_json_int(res, "created", 1);
    nv_ipc_reply_ok(sender, seq, res, arena);
}

NV_INTERNAL void nv_op_windows_close(nv_window_t* sender, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
    const char* id = nv_json_get_str(args, "id");
    if (!id) {
        nv_ipc_reply_err(sender, seq, "ERR_INVALID_ARG", "missing 'id'", arena);
        return;
    }
    
    nv_window_t* win = nv_wm_get_by_id(id);
    if (!win) {
        nv_ipc_reply_err(sender, seq, "ERR_NOT_FOUND", "window not found", arena);
        return;
    }
    
    // Before closing notify listeners
    nv_json_t* before = nv_json_object(arena);
    nv_json_str(before, "id", id);
    nv_op_windows_push_all("windows.beforeClose", before, arena);
    
    // Unregister first to prevent double-push if destroy triggers callback
    // Then push "windows.closed"
    nv_json_t* push_data = nv_json_object(arena);
    nv_json_str(push_data, "id", id);
    nv_op_windows_push_all("windows.closed", push_data, arena);
    
    nv_wm_unregister(id);
    nv_window_destroy(win);
    
    nv_ipc_reply_ok(sender, seq, nv_json_object(arena), arena);
}

NV_INTERNAL void nv_op_windows_focus(nv_window_t* sender, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
    const char* id = nv_json_get_str(args, "id");
    if (!id) {
        nv_ipc_reply_err(sender, seq, "ERR_INVALID_ARG", "missing 'id'", arena);
        return;
    }
    
    nv_window_t* win = nv_wm_get_by_id(id);
    if (win) {
        nv_window_focus(win);
    }
    
    nv_ipc_reply_ok(sender, seq, nv_json_object(arena), arena);
}

NV_INTERNAL void nv_op_windows_list(nv_window_t* sender, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
    (void)args;
    nv_wm_entry_t entries[NV_MAX_WINDOWS];
    size_t count = NV_MAX_WINDOWS;
    nv_wm_list(entries, &count);
    
    nv_json_t* res = nv_json_object(arena);
    nv_json_t* arr = nv_json_array(arena);
    
    for (size_t i = 0; i < count; i++) {
        if (entries[i].active && entries[i].window) {
            nv_json_t* item = nv_json_object(arena);
            nv_json_str(item, "id", entries[i].id);
            
            const char* title = nv_window_get_title(entries[i].window);
            nv_json_str(item, "title", title ? title : "");
            
            // Access internal struct for visibility
            nv_json_bool(item, "visible", entries[i].window->visible);
            
            nv_json_array_push(arr, item);
        }
    }
    
    nv_json_nest(res, "windows", arr);
    nv_ipc_reply_ok(sender, seq, res, arena);
}

NV_INTERNAL void nv_op_windows_get_info(nv_window_t* sender, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
    const char* id = nv_json_get_str(args, "id");
    if (!id) {
        nv_ipc_reply_err(sender, seq, "ERR_INVALID_ARG", "missing 'id'", arena);
        return;
    }
    
    nv_window_t* win = nv_wm_get_by_id(id);
    if (!win) {
        nv_ipc_reply_err(sender, seq, "ERR_NOT_FOUND", "window not found", arena);
        return;
    }
    
    int w=0, h=0, x=0, y=0;
    nv_window_get_size(win, &w, &h);
    nv_window_get_position(win, &x, &y);
    int focused = nv_window_is_focused(win);
    int visible = win->visible; // Access internal struct
    const char* title = nv_window_get_title(win);
    
    nv_json_t* res = nv_json_object(arena);
    nv_json_str(res, "id", id);
    nv_json_str(res, "title", title ? title : "");
    nv_json_int(res, "width", w);
    nv_json_int(res, "height", h);
    nv_json_int(res, "x", x);
    nv_json_int(res, "y", y);
    nv_json_bool(res, "visible", visible);
    nv_json_bool(res, "focused", focused);
    
    nv_ipc_reply_ok(sender, seq, res, arena);
}

NV_INTERNAL void nv_op_windows_set_title(nv_window_t* sender, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
    const char* id = nv_json_get_str(args, "id");
    const char* title = nv_json_get_str(args, "title");
    
    if (!id || !title) {
        nv_ipc_reply_err(sender, seq, "ERR_INVALID_ARG", "missing 'id' or 'title'", arena);
        return;
    }
    
    nv_window_t* win = nv_wm_get_by_id(id);
    if (win) {
        nv_window_set_title(win, title);
    }
    
    nv_ipc_reply_ok(sender, seq, nv_json_object(arena), arena);
}
