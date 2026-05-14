# Contributing

## File Length Limits (Configure-Time)

This repository enforces source file length limits during CMake configure by default.
If a file exceeds the limit, configuration fails with an error that mentions
`NV_ENFORCE_FILE_LIMITS`.

- The check runs when you configure with CMake (not at build time).
- It is controlled by the CMake option `NV_ENFORCE_FILE_LIMITS` (default: ON).
- The script is [`tools/check_file_lengths.py`](tools/check_file_lengths.py).

Current limits:
- `.c`: 400 lines
- `.m`, `.mm`, `.cpp`: 800 lines

Some legacy/exception files are allowlisted in the script; new additions to the
allowlist should be rare.

### How to Bypass (If You’re Unblocking Yourself)

If you’re just trying to get a local build going while you split/refactor, you
can disable the check:

```bash
cmake -S . -B build -DNV_ENFORCE_FILE_LIMITS=OFF
cmake --build build
```

Please do not rely on this for contributions meant to be merged.

### Preferred Fix

- Split the file into smaller compilation units (e.g. `*_core.c`, `*_platform.c`,
  `*_util.c`) and keep each within the limit.
- Move platform-specific code into the appropriate `modules/nv-platform-*/`
  backend rather than adding `#ifdef` blocks in shared code.

## Development Setup

- Configure and build:

```bash
cmake -S . -B build
cmake --build build
```

- Run tests (desktop builds only):

```bash
ctest --test-dir build
```

## Style & Project Rules

Repository-wide rules for modules and public APIs live in [`AGENTS.md`](AGENTS.md).
If you are contributing in a specific module, also check that module’s
`modules/nv-*/AGENTS.md`.
