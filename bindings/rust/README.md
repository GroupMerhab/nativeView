# Rust binding — nativeview

Low-level `extern "C"` crate mirroring `include/nv.h`. **`#![no_std]`**.

**Full guide:** [docs/Rust.md](../../docs/Rust.md)

**Not on crates.io** (`publish = false`). Use a path dependency:

```toml
nativeview = { path = "../bindings/rust" }
```

**Features:** `link-shared` (default), `link-static`, `link-none`

**Example:** [examples/todo_app/rust_todo/](../../examples/todo_app/rust_todo/)
