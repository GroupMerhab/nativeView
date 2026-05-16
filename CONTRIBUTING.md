# Contributing

Thank you for contributing to nativeview. Please read this guide before opening a pull request.

## Contributor License Agreement

All contributions require agreement to the [CLA](CLA.md). Sign by:

- Commenting on your PR: *"I have read the CLA and I sign it."* with your full legal name and date, or
- Using `git commit -s` (signed-off-by) on each commit.

## How to contribute

1. Fork the repository and create a branch from `main`.
2. Make your changes following the rules below.
3. Ensure the project configures, builds, and tests pass locally.
4. Open a pull request with a clear description and link any related issues.

## Issue reporting

When filing a bug, include:

- **OS and version** (e.g. Ubuntu 24.04, macOS 14, Windows 11)
- **nativeview version** or git commit hash
- **Steps to reproduce** (minimal example preferred)
- **Expected vs actual behavior**
- **Logs** (terminal output, WebView console if relevant)

For security issues, do **not** use public issues — see [SECURITY.md](SECURITY.md).

## Pull request checklist

- [ ] Builds with `cmake -S . -B build && cmake --build build` on your platform
- [ ] `ctest --test-dir build --output-on-failure` passes (desktop)
- [ ] No new compiler warnings (`-Wall -Wextra`)
- [ ] [CLA](CLA.md) signed (PR comment or `git commit -s`)
- [ ] File length limits respected (`NV_ENFORCE_FILE_LIMITS=ON`, default)
- [ ] New public C functions have unit tests
- [ ] New/changed source files include SPDX header (see below)

## Code style

Repository-wide rules live in [AGENTS.md](AGENTS.md). Module-specific rules: `modules/nv-*/AGENTS.md`.

Summary:

- C11, `-Wall -Wextra` clean
- No bare `malloc`/`free` in new code — use arena allocators
- No platform `#ifdef` outside `modules/nv-platform-*/`
- All public functions NULL-safe
- Max 800 lines per `.c` file (800 for `.m`/`.mm`/`.cpp`) unless allowlisted

### SPDX license header

New or substantially modified C/C++/Objective-C headers and sources should include:

```c
/* Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0 */
```

Or the full Apache boilerplate block used elsewhere in `nv-core`.

## File Length Limits (Configure-Time)

This repository enforces source file length limits during CMake configure by default.
If a file exceeds the limit, configuration fails with an error that mentions
`NV_ENFORCE_FILE_LIMITS`.

- The check runs when you configure with CMake (not at build time).
- It is controlled by the CMake option `NV_ENFORCE_FILE_LIMITS` (default: ON).
- The script is [`tools/check_file_lengths.py`](tools/check_file_lengths.py).

Current limits:

- `.c`: 800 lines
- `.m`, `.mm`, `.cpp`: 800 lines

Some legacy/exception files are allowlisted in the script; new additions to the
allowlist should be rare.

### How to Bypass (If You're Unblocking Yourself)

If you're just trying to get a local build going while you split/refactor, you
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
ctest --test-dir build --output-on-failure
```

## Code of Conduct

This project follows the [Contributor Covenant](CODE_OF_CONDUCT.md). Please be respectful and constructive in all interactions.
