#include "nv_window_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h> // for uintptr_t

int test_count = 0;
int test_passed = 0;

void test_ok(const char* name) {
  test_count++;
  test_passed++;
  printf("✓ %s\n", name);
}

void test_fail(const char* name, const char* reason) {
  test_count++;
  printf("✗ %s: %s\n", name, reason);
}

void test_roundtrip(void) {
    nv_window_t* w = (nv_window_t*)(uintptr_t)0x1234;
    nv_wm_init(); // Reset state
    
    if (nv_wm_register("win1", w) != 0) {
        test_fail("roundtrip", "register failed");
        return;
    }
    
    if (nv_wm_get_by_id("win1") != w) {
        test_fail("roundtrip", "get_by_id mismatch");
        return;
    }
    
    test_ok("roundtrip");
}

void test_duplicate(void) {
    nv_window_t* w1 = (nv_window_t*)(uintptr_t)0x1111;
    nv_window_t* w2 = (nv_window_t*)(uintptr_t)0x2222;
    nv_wm_init();
    
    nv_wm_register("dup", w1);
    if (nv_wm_register("dup", w2) != -1) {
        test_fail("duplicate", "allowed duplicate id");
        return;
    }
    
    test_ok("duplicate");
}

void test_max_windows(void) {
    nv_wm_init();
    char id[16];
    
    for (int i = 0; i < NV_MAX_WINDOWS; ++i) {
        sprintf(id, "w%d", i);
        if (nv_wm_register(id, (nv_window_t*)(uintptr_t)(i+1)) != 0) {
            test_fail("max_windows", "failed to register within limit");
            return;
        }
    }
    
    if (nv_wm_register("overflow", (nv_window_t*)(uintptr_t)0x9999) != -1) {
        test_fail("max_windows", "allowed register beyond limit");
        return;
    }
    
    test_ok("max_windows");
}

void test_unregister(void) {
    nv_wm_init();
    nv_window_t* w = (nv_window_t*)(uintptr_t)0xABCD;
    nv_wm_register("unreg", w);
    
    if (nv_wm_get_by_id("unreg") != w) {
        test_fail("unregister", "setup failed");
        return;
    }
    
    nv_wm_unregister("unreg");
    
    if (nv_wm_get_by_id("unreg") != NULL) {
        test_fail("unregister", "still found after unregister");
        return;
    }
    
    test_ok("unregister");
}

void test_get_id_by_window(void) {
    nv_wm_init();
    nv_window_t* w = (nv_window_t*)(uintptr_t)0xCAFE;
    nv_wm_register("cafe", w);
    
    const char* id = nv_wm_get_id_by_window(w);
    if (!id || strcmp(id, "cafe") != 0) {
        test_fail("get_id_by_window", "wrong id returned");
        return;
    }
    
    test_ok("get_id_by_window");
}

void test_list(void) {
    nv_wm_init();
    nv_wm_register("a", (nv_window_t*)(uintptr_t)1);
    nv_wm_register("b", (nv_window_t*)(uintptr_t)2);
    nv_wm_register("c", (nv_window_t*)(uintptr_t)3);
    nv_wm_unregister("b");
    
    nv_wm_entry_t entries[10];
    size_t count = 10;
    
    if (nv_wm_list(entries, &count) != 0) {
        test_fail("list", "function failed");
        return;
    }
    
    if (count != 2) {
        test_fail("list", "wrong count");
        return;
    }
    
    // Check content (order not guaranteed but likely insertion order)
    int found_a = 0, found_c = 0;
    for (size_t i = 0; i < count; ++i) {
        if (strcmp(entries[i].id, "a") == 0) found_a = 1;
        if (strcmp(entries[i].id, "c") == 0) found_c = 1;
    }
    
    if (!found_a || !found_c) {
        test_fail("list", "missing entries");
        return;
    }
    
    test_ok("list");
}

void test_id_valid(void) {
    if (!nv_wm_id_valid("valid-id_123")) test_fail("id_valid", "rejected valid id");
    if (!nv_wm_id_valid("main")) test_fail("id_valid", "rejected main");
    
    if (nv_wm_id_valid(NULL)) test_fail("id_valid", "accepted NULL");
    if (nv_wm_id_valid("")) test_fail("id_valid", "accepted empty");
    if (nv_wm_id_valid("*")) test_fail("id_valid", "accepted *");
    if (nv_wm_id_valid("invalid space")) test_fail("id_valid", "accepted space");
    if (nv_wm_id_valid("invalid/slash")) test_fail("id_valid", "accepted slash");
    
    char long_id[100];
    memset(long_id, 'a', 99);
    long_id[99] = 0;
    if (nv_wm_id_valid(long_id)) test_fail("id_valid", "accepted too long");
    
    test_ok("id_valid");
}

void test_count_fn(void) {
    nv_wm_init();
    if (nv_wm_count() != 0) { test_fail("count", "initial not 0"); return; }
    
    nv_wm_register("1", (nv_window_t*)(uintptr_t)1);
    if (nv_wm_count() != 1) { test_fail("count", "after 1 reg not 1"); return; }
    
    nv_wm_register("2", (nv_window_t*)(uintptr_t)2);
    if (nv_wm_count() != 2) { test_fail("count", "after 2 reg not 2"); return; }
    
    nv_wm_unregister("1");
    if (nv_wm_count() != 1) { test_fail("count", "after unreg not 1"); return; }
    
    test_ok("count");
}

int main(void) {
    printf("Running nv_window_manager tests...\n");
    
    test_roundtrip();
    test_duplicate();
    test_max_windows();
    test_unregister();
    test_get_id_by_window();
    test_list();
    test_id_valid();
    test_count_fn();
    
    printf("Tests passed: %d/%d\n", test_passed, test_count);
    return (test_passed == test_count) ? 0 : 1;
}
