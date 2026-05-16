# Swift examples

- **`minimal/`** — SwiftPM executable that mirrors **`examples/nim/minimal.nim`** (`nv_on_ready`, **`nv_eval_js_batch`**, **`nv_send`**, **`nv_window_on_close`**).
- **`build_static.sh`** — configures CMake (static archives), then **`swift build -c release`** in **`minimal/`** with **`swift build`’s `-Xlinker`** flags for macOS or Linux.

See **`docs/Swift.md`** for path dependencies (**`.package(name: "Nativeview", …)`**), the **`nv_pub`** header symlink, and callback rules.
