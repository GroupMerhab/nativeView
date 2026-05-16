# Zig binding — nativeview

FFI module: **`nativeview.zig`** (no `std` dependency in the binding itself).

**Full guide:** [docs/Zig.md](../../docs/Zig.md)

**Examples:** [examples/zig/](../../examples/zig/), [examples/todo_app/zig_todo/](../../examples/todo_app/zig_todo/)

```bash
zig build-exe --dep nativeview \
  -Mroot=app.zig \
  -Mnativeview=bindings/zig/nativeview.zig
```
