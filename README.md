# z-core

**The central infrastructure for the z-libs ecosystem.**

This repository contains the shared memory definitions, return codes, and build scripts used by all libraries in the z-libs collection.

It is designed to be included as a **Git Submodule** in your specific library repositories.

## Contents

### 1. `zcommon.h` (Shared Definitions)
A lightweight header that standardizes:
* **Return Codes:** `Z_OK`, `Z_ERR`, `Z_FOUND`.
* **Memory Management:** Wrappers for `malloc`, `calloc`, `realloc`, and `free`.
  * **Default:** Uses `<stdlib.h>`.
  * **Override:** Users can define `Z_MALLOC`, `Z_CALLOC`, `Z_REALLOC`, and `Z_FREE` before including any z-lib.
  * **Note:** It is strongly recommended to override **all four** macros together to avoid mixing memory allocators (e.g., allocating with an arena but resizing with standard realloc).

### `zbundler.py` (The Builder)
A Python build script that generates single-header libraries.
* Takes a source implementation (e.g., `src/zvec.c`) and injects `zcommon.h` directly into it.
* Ensures the final output file (e.g., `dist/zvec.h`) is truly standalone with zero external dependencies.
* Wraps the common code in a unique guard (`Z_COMMON_BUNDLED`) so users can include multiple z-libs in the same project without redefinition errors.

## Usage

### As a Dependency in a Library Repo

To use `z-core` in a library like `zvec.h`:

```bash
git submodule add https://github.com/z-libs/z-core.git z-core
```

### Building a Single-Header Library

Use the bundler script in your `Makefile` to generate the release file:

```Makefile
BUNDLER = z-core/zbundler.py
SRC = src/mylib.c
DIST = mylib.h

bundle:
    python3 $(BUNDLER) $(SRC) $(DIST)
```

## Contributing

Updates to `zcommon.h` or `zbundler.py` should be made here.
After pushing changes, update the submodule pointer in the dependent libraries:

```bash
# In the dependent repo.
git submodule update --remote
git commit -am "Update z-core tooling"
```
