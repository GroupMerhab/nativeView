version       = "0.1.0"
author        = "nativeview"
description   = "Low-level Nim FFI to the nativeview public C API (nv.h, nv_hotkey.h, nv_util helpers)"
license       = "Apache-2.0"
srcDir        = "."

requires "nim >= 1.6"

task staticCheck, "Type-check nativeview.nim without linking (passL is your responsibility)":
  exec "nim check nativeview.nim"
