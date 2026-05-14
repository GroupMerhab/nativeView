/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <assert.h>
#include "nv.h"

int main(void) {
  assert(nv_is_process_main_thread() != 0);
  return 0;
}
