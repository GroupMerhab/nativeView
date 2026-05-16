/* SPDX-License-Identifier: Apache-2.0 */
#include "NativeviewBridge.h"

/* Translation unit so SwiftPM always compiles a C object for CNativeview. */
void nativeview_swift_shim_marker(void) {}
