# Python minimal sample

**On macOS, static linking tends to crash before the window appears (AppKit event-loop ownership).** This example uses the **shared** library (`libnativeview.dylib` / `libnativeview.so` / `nativeview.dll`), which matches how **ctypes** loads nativeview.

## Quick run (Linux / macOS)

From **`examples/python/`**:

```bash
chmod +x build_shared.sh
./build_shared.sh
```

This configures CMake with **`-DNV_BUILD_SHARED=ON`**, builds **`nativeview_shared`**, sets **`NATIVEVIEW_LIB`** and **`PYTHONPATH`**, then runs **`minimal.py`**.

## Windows

In PowerShell from **`examples/python/`**:

```powershell
.\build_shared.ps1
```

## Manual

1. Build **`nativeview_shared`** (see root **`CMakeLists.txt`**).
2. **`export PYTHONPATH=/path/to/nativeView/bindings/python`** (or `pip install -e` that directory).
3. **`export NATIVEVIEW_LIB=/path/to/libnativeview.so`** (or `.dylib` / `.dll`).
4. **`python3 minimal.py`**

Full notes: **`docs/Python.md`**.
