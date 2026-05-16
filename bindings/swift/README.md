# Swift binding (SwiftPM)

Low-level Swift access to the nativeview **C** API: SwiftPM target **`CNativeview`** (umbrella header + shim) and library **`Nativeview`** that re-exports it.

- **Build the Swift modules only:** `cd bindings/swift && swift build` (no nativeview native link required).
- **End-to-end sample:** `examples/swift/build_static.sh` then run `examples/swift/minimal/.build/release/minimal`.
- **Full guide:** `docs/Swift.md`.
