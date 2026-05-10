{
  Shared library exporting RunNativeView for the Darwin C host (nv_minimal_host.c).

  Export name must be _nv_pascal_minimal_run (leading underscore) so Clang can
  link the host executable against this dylib on arm64-apple-darwin.

  On Linux/Windows you can still use nv_minimal.lpr (pure program) when linking
  works for your toolchain.
}

library nv_minimal_lib;

{$mode objfpc}{$H+}

uses
  nv_minimal_app;

exports
  RunNativeView name '_nv_pascal_minimal_run';

begin
end.
