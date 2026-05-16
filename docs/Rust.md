# Rust — nativeview

Low-level Rust FFI to the public C API in `include/nv.h`, `include/nv_hotkey.h`, and version helpers from `include/nv_util.h`.

**Crate:** `bindings/rust/` (`nativeview` on crates.io path only — **`publish = false`**).

---

## What you need

- **Rust** 2021 edition (`edition = "2021"` in `Cargo.toml`)
- A **built** nativeview library for your platform (CMake; see [getting-started.md](getting-started.md))
- **Windows:** WebView2 runtime installed for running GUI apps

---

## Crate features

Enable **exactly one** link mode:

| Feature | Description |
|---------|-------------|
| `link-shared` (default) | Link `libnativeview.so` / `.dylib` / `nativeview.dll` |
| `link-static` | Link `libnativeview.a` / `nativeview.lib` |
| `link-none` | Symbols only; you pass archives via `RUSTFLAGS` / `build.rs` |

```bash
# Shared (default)
cargo build

# Static
cargo build --no-default-features --features link-static

# External static chain (todo sample style)
cargo build --no-default-features --features link-none
```

---

## Linking

### Shared library

```bash
cmake -S . -B build -DNV_BUILD_SHARED=ON
cmake --build build
export RUSTFLAGS="-L native=/path/to/nativeView/build"
cargo build --manifest-path bindings/rust/Cargo.toml
```

Ensure `libnativeview` is on the loader path at runtime (`LD_LIBRARY_PATH`, `DYLD_LIBRARY_PATH`, or next to the `.exe` on Windows).

### Static library

```bash
cmake -S . -B build
cmake --build build
export RUSTFLAGS="-L native=/path/to/nativeView/build"
cargo build --manifest-path bindings/rust/Cargo.toml \
  --no-default-features --features link-static
```

On **Linux**, you may need the same WebKitGTK and helper libraries as the C examples (`pkg-config --libs webkit2gtk-4.1`, etc.) — mirror [Nim.md](Nim.md) or `examples/todo_app/rust_todo/build_static.sh`.

### `link-none` + CMake archive chain

Used by **`examples/todo_app/rust_todo/`** when linking the full static module graph (`--start-group` on ELF). Pass linker flags via `RUSTFLAGS` or a `build.rs` in your app crate.

---

## Usage

Add a path dependency:

```toml
[dependencies]
nativeview = { path = "../bindings/rust" }
```

```rust
// Opaque types and extern fns — see bindings/rust/src/nativeview.rs
// Match C calling conventions; callbacks must be `unsafe extern "C" fn` with stable addresses.
```

### Callbacks and lifetimes

- `nv_on_message`: `event` and `json` pointers are valid **only inside the callback** — copy if needed.
- `NvWindowCfg::title` must outlive `nv_window_create` (same as C).
- `nv_eval_js_batch`: pass a pointer to the first element of a contiguous array of `*const c_char`; keep storage valid until the call returns.

---

## Layout

| Path | Role |
|------|------|
| `src/nativeview.rs` | `extern "C"` declarations + types (`#![no_std]`) |
| `Cargo.toml` | Features and metadata |

Short README: [`bindings/rust/README.md`](../bindings/rust/README.md).

---

## Examples

- **Todo app (static link):** [examples/todo_app/rust_todo/](../examples/todo_app/rust_todo/) — Vue UI + rusqlite, uses `link-none` + `build_static.sh`

There is no minimal `examples/rust/` yet; use the todo sample or C `examples/hello/` with Rust linked via the steps above.

---

## Related bindings

Same C surface as [Nim.md](Nim.md), [Zig.md](Zig.md), [Pascal.md](Pascal.md). For JVM/Python shared-library workflows see [Java.md](Java.md) and [Python.md](Python.md).

Overview: [bindings.md](bindings.md).
