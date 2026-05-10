{
  nv_minimal.lpr — entry point for the Free Pascal + nativeview sample.

  Logic lives in unit nv_minimal_app (counter UI, cdecl callbacks). See docs/Pascal.md.

  Build (from examples/pascal), shared library:
    fpc -dNATIVEVIEW_SHARED -Fu../../bindings/pascal nv_minimal.lpr
  plus linker flags so libnativeview resolves (see README.md).

  macOS + shared lib: prefer ./build_example.sh (Clang main + Pascal dylib); see README.

  Optional on Windows: hide the extra console with APPTYPE GUI (see docs/Pascal.md).
}

program nv_minimal;

{$mode objfpc}{$H+}

uses
  nv_minimal_app;

begin
  RunNativeView;
end.
