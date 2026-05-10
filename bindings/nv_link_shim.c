/*
 * nv_link_shim.c — translation unit for the nativeview_shared CMake target.
 *
 * The shared library embeds the full static archive chain via whole-archive
 * (ELF/MinGW) or /WHOLEARCHIVE (MSVC). This file anchors the target and keeps
 * a compile-time include check against the public API header.
 *
 * Apache 2.0 — See LICENSE for details.
 */

#include "nv.h"
