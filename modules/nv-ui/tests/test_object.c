/*
 * Tests for composite component as single node (type, name, init, destroy).
 * "Object" is folded into nv_component_t; this tests one component.
 */

#include "nv_ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int test_count;
static int test_passed;

static void ok(const char* name) {
  test_count++;
  test_passed++;
  printf("OK %s\n", name);
}

static void fail(const char* name, const char* reason) {
  test_count++;
  printf("FAIL %s: %s\n", name, reason);
}

int main(void) {
  test_count = 0;
  test_passed = 0;

  /* Alloc with valid type and name */
  nv_component_t* comp = (nv_component_t*)malloc(sizeof(nv_component_t));
  if (!comp) {
    printf("FAIL alloc: malloc\n");
    return 1;
  }
  nv_component_init(comp, NV_COMP_BUTTON, NULL, NULL, NULL, "btn1");
  if (comp->type != NV_COMP_BUTTON) fail("init_type", "type not set");
  else ok("init_type");
  if (strcmp(comp->name, "btn1") != 0) fail("init_name", "name not copied");
  else ok("init_name");
  nv_component_destroy(comp);

  /* Name truncation */
  comp = (nv_component_t*)malloc(sizeof(nv_component_t));
  if (!comp) {
    printf("FAIL alloc2: malloc\n");
    return 1;
  }
  char long_name[128];
  memset(long_name, 'x', 127);
  long_name[127] = '\0';
  nv_component_init(comp, NV_COMP_LABEL, NULL, NULL, NULL, long_name);
  if (strlen(comp->name) >= NV_OBJECT_NAME_MAX) fail("name_truncate", "name not truncated");
  else ok("name_truncate");
  nv_component_destroy(comp);

  /* destroy NULL */
  nv_component_destroy(NULL);
  ok("destroy_null");

  printf("%d/%d passed\n", test_passed, test_count);
  return test_passed == test_count ? 0 : 1;
}
