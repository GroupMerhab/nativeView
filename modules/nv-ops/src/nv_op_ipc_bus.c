#include "nv_op_ipc_bus.h"
#include "nv_window_manager.h"
#include "util/nv_log.h"
#include <string.h>
#include <stdio.h>

// External helper from nv_ipc.c
NV_INTERNAL void nv_ipc_reply_ok(nv_window_t* w, int seq, nv_json_t* result, nv_arena_t* arena);
NV_INTERNAL void nv_ipc_reply_err(nv_window_t* w, int seq, const char* code, const char* message, nv_arena_t* arena);

// Helper to deliver message to a specific target
NV_INTERNAL void nv_ipc_bus_deliver(nv_window_t* target, const char* from_id, const char* event, const nv_json_val_t* data, nv_arena_t* arena) {
    if (!target || !from_id || !event) return;

    // Build payload: { from: sender_id, event: event, data: data }
    // Since we need to forward 'data' (nv_json_val_t*) as a nested object,
    // and nv_json_nest takes nv_json_t*, we have a gap.
    // However, nv_send takes a serialized string.
    // So we can build the JSON string manually or use a helper that serializes nv_json_val_t.
    
    // Approach: use nv_json_val_serialize to get string representation of 'data',
    // then build the full JSON string.
    // But nv_json API is a builder that produces nv_json_t*.
    // We don't have a "add raw json string as property" method in nv_json.h public API (checked).
    // But we can parse the serialized string back into an nv_json_t? No, that's inefficient.
    // Wait, nv_json_nest takes nv_json_t*.
    // If we have nv_json_val_t, we can't convert it to nv_json_t* easily without re-parsing or manual reconstruction.
    // BUT the requirement says "data field forwarded as-is (no re-parsing)".
    // This implies we should avoid parsing if possible, but if the API forces us...
    // Actually, "no re-parsing" might mean "don't parse the 'data' field inside the handler to inspect it, just forward it".
    // But we already have it as nv_json_val_t* (parsed).
    // So we need to put it into the outgoing JSON.
    
    // Efficient way:
    // 1. Serialize 'data' to string using nv_json_val_serialize.
    // 2. Construct the outer JSON string manually? Or use nv_json builder and add 'data' as... what?
    // We don't have 'nv_json_add_raw'.
    
    // Alternative:
    // Parse the serialized string back to nv_json_t*? That's re-parsing.
    
    // Let's look at nv_json.h again.
    // nv_json_val_serialize returns const char*.
    // nv_send takes const char*.
    
    // So we can construct the string manually using snprintf?
    // But 'data' can be large.
    
    // Let's use a trick:
    // Create a temporary nv_json_t for 'from' and 'event'.
    // Serialize it. It gives `{"from":"...","event":"..."}`.
    // Then we need to insert `,"data":...` before the closing brace.
    // This is hacky.
    
    // Better:
    // Use nv_json_val_serialize to get data_str.
    // Use nv_json_serialize to get metadata_str (from, event).
    // Combine them.
    
    const char* data_str = nv_json_val_serialize(arena, data);
    if (!data_str) data_str = "null";
    
    // We'll construct the final string manually to avoid re-parsing.
    // Format: {"from":"ID","event":"EVENT","data":DATA_JSON}
    // We need to escape ID and EVENT properly.
    // nv_json_serialize handles escaping.
    // Let's create a builder for metadata.
    
    nv_json_t* meta = nv_json_object(arena);
    nv_json_str(meta, "from", from_id);
    nv_json_str(meta, "event", event);
    const char* meta_str = nv_json_serialize(meta);
    // meta_str is {"from":"...","event":"..."}
    
    // We need to strip the closing '}' and append ',"data":' + data_str + '}'.
    size_t meta_len = strlen(meta_str);
    size_t data_len = strlen(data_str);
    size_t total_len = meta_len + 7 + data_len + 2; // -1 (}) + 8 (,"data":) + data + 1 (}) + null
    
    char* final_json = nv_arena_alloc(arena, total_len);
    if (final_json) {
        // Copy meta_str excluding last '}'
        memcpy(final_json, meta_str, meta_len - 1);
        // Append ,"data":
        memcpy(final_json + meta_len - 1, ",\"data\":", 8);
        // Append data_str
        memcpy(final_json + meta_len - 1 + 8, data_str, data_len);
        // Append closing }
        final_json[meta_len - 1 + 8 + data_len] = '}';
        final_json[meta_len - 1 + 8 + data_len + 1] = '\0';
        
        nv_send(target, "ipc_bus.message", final_json);
    }
}

NV_INTERNAL void nv_op_ipc_bus_send(nv_window_t* sender, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
    const char* to = nv_json_get_str(args, "to");
    const char* event = nv_json_get_str(args, "event");
    const nv_json_val_t* data = nv_json_get_obj(args, "data");
    
    if (!to || !event || !data) {
        nv_ipc_reply_err(sender, seq, "ERR_INVALID_ARG", "missing required args (to, event, data)", arena);
        return;
    }

    // Identify sender
    const char* from_id = nv_wm_get_id_by_window(sender);
    if (!from_id) {
        NV_LOG("IPC Bus: Unknown sender window, using 'unknown'");
        from_id = "unknown";
    }

    int delivered = 0;

    if (strcmp(to, "*") == 0) {
        // Broadcast
        nv_wm_entry_t entries[NV_MAX_WINDOWS];
        size_t count = NV_MAX_WINDOWS;
        nv_wm_list(entries, &count);

        for (size_t i = 0; i < count; i++) {
            if (entries[i].active && entries[i].window && entries[i].window != sender) {
                nv_ipc_bus_deliver(entries[i].window, from_id, event, data, arena);
                delivered++;
            }
        }
    } else {
        // Specific target
        nv_window_t* target = nv_wm_get_by_id(to);
        if (!target) {
            nv_ipc_reply_err(sender, seq, "ERR_NOT_FOUND", "target window not found", arena);
            return;
        }
        nv_ipc_bus_deliver(target, from_id, event, data, arena);
        delivered = 1;
    }

    // Reply OK
    nv_json_t* res = nv_json_object(arena);
    nv_json_int(res, "delivered", delivered);
    nv_ipc_reply_ok(sender, seq, res, arena);
}
