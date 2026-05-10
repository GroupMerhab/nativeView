/*
 * Darwin + shared nativeview: use this C main and link libnv_minimal_lib.dylib.
 *
 * Free Pascal's normal program entry interacts badly with AppKit when the
 * nativeview stack is in a separate dylib (EXC_BAD_INSTRUCTION in
 * _NSGetCGFloatAppConfig). A tiny Clang main avoids FPC's PASCALMAIN path;
 * all UI logic stays in Pascal (nv_minimal_lib.lpr + nv_minimal_app.pas).
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#if defined(__APPLE__)

extern void nv_pascal_minimal_run(void);

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;
  nv_pascal_minimal_run();
  return 0;
}

#else

int main(void) {
  return 0;
}

#endif
