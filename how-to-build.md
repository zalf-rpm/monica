# How to build MONICA

There are several ways to build MONICA. They differ in where they build, what
they produce, and what they are meant for. This page explains which one does
what; the short commands are also in the README's "Building" section.

In every case, clone with the git submodules first (`mas_cpp_misc`,
`monica-parameters`):

    git clone --recurse-submodules https://github.com/zalf-rpm/monica.git
    # or, in an existing clone:
    git submodule update --init --recursive

## Overview

| OS | Way | Command | Builds in | Result | Meant for |
|---|---|---|---|---|---|
| Linux | Nix package | `nix build` | a sandbox, result in `/nix/store` | `result/bin/` (link), Release | the "official" Linux build: Docker image, CI |
| Linux | Nix package, debug | `nix build .#monica-debug` | same | `result/bin/` (link), Debug | debugging a Nix build without a dev shell |
| Linux | Nix dev shell + CMake | `cmake --build build` | `build/` in the checkout | `build/`, Debug | day-to-day work: Zed tasks, debugging, clangd |
| Linux, Windows | pixi | `pixi run build` (`-- --debug`) | `_cmake_release/` (`_cmake_debug/`) | there | Windows development, CI's pixi jobs |
| Windows | Visual Studio + vcpkg | see the release workflows | `_cmake_win64/` | `_cmake_win64/Release/` | the Windows release zip (CI) |
| any | Docker | `docker build .` | Docker (Nix inside) | an image | the published `zalfrpm/monica-cluster` image |

CI checks with the test suites (`tests/README.md`) that the Nix build, the
Docker image and the pixi builds on Linux and Windows give bit-identical
simulation results. The others use the same code; a dev shell Debug build in
`build/` also gave identical results when checked by hand.

## Linux

### Nix package: `nix build`

    nix build                         # Release
    nix build .#monica-debug          # Debug, not stripped, sources found by gdb

Nix doesn't build in your checkout. It copies the source into a sandbox,
builds there (the directory is temporary and gone afterwards) and stores the
finished package in `/nix/store/...`. `result` in the checkout is only a
symbolic link to that package, and **every `nix build` replaces it**: after
`nix build .#monica-debug`, `result/bin` is the Debug build, after the next
plain `nix build` the Release build again. To keep both:

    nix build -o result-debug .#monica-debug

The compiler and all libraries are pinned by `flake.lock`; this is exactly
what goes into the Docker image. `monica-debug` is meant for debugging a Nix
build where there is no dev shell; for working on the code, the dev shell
below is quicker (incremental builds).

You can also run MONICA straight from GitHub, without a checkout:

    nix run 'git+https://github.com/zalf-rpm/monica?submodules=1' -- sim-min.json

### Nix dev shell + CMake: `build/`

The dev shell (`flake.nix`, `devShells.default`) has the same compiler and
libraries as the Nix package, plus `clangd`, `clang-format` and `gdb`. With
[direnv](https://direnv.net) it is loaded automatically when you enter the
checkout (once: `direnv allow`), otherwise with `nix develop`.

The name `build/` doesn't come from the flake. It comes from:

- `.envrc`: when the dev shell is loaded and `build/` has no
  `compile_commands.json` yet, it configures a Debug build there (takes a few
  seconds; it doesn't compile anything). That's what clangd needs.
- the Linux tasks in `.zed/tasks.json` and the Linux configurations in
  `.zed/debug.json`, which build and debug `build/`.

By hand:

    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug   # done by .envrc
    cmake --build build                                     # executables in build/

In Zed (Linux):

| Task / debug configuration | Does |
|---|---|
| `[Linux] CMake Build` | `cmake --build build` |
| `[Linux] CMake Clean + Build` | clean, then build |
| `[Linux] CMake Reconfigure + Build` | `cmake --fresh ...`, then build - after CMake changes that confuse the cache |
| `[Linux] Test suites with build/monica-run` | runs `tests/` with `build/monica-run` into `build/test-results` |
| `[Linux] Debug monica-run (Hohenfinow2 sim-min.json)` | builds, then GDB on `sim-min.json` |
| `[Linux] Debug monica-zmq-server` | builds, then GDB |

The tasks run through `direnv exec .`, so they get the dev shell even if Zed
didn't pass it on. Notes:

- The dev shell switches off Nix's "fortify" hardening: it would add `-O2`
  to CMake's Debug builds (which only pass `-g`), making them silently
  optimized. The Nix package keeps it.
- clangd finds `build/compile_commands.json` on its own (it looks in each
  parent directory and its `build/` subdirectory). A `compile_commands.json`
  in the repository root is found *first* - don't leave one there on Linux
  (the Windows tasks create one, see below).
- If Zed uses its own downloaded clangd instead of the dev shell's, it can't
  find the standard library on NixOS. The Zed log (`zed: open log`, file
  `~/.local/share/zed/logs/Zed.log`) shows which one runs (`binary path:`);
  restarting Zed in the checkout usually fixes it.
- GDB: `skip -rfu ^(std|__gnu_cxx|kj|capnp)::` in `~/.config/gdb/gdbinit`
  keeps `step` out of the standard library and Cap'n Proto/kj.

### pixi on Linux

    pixi run build              # Release into _cmake_release/
    pixi run build -- --debug   # Debug into _cmake_debug/

Uses conda-forge's GCC and vcpkg's libraries instead of Nix (see Windows
below for how). It's what CI's `pixi (ubuntu-latest)` job builds; the Zed
Linux tasks don't use it.

## Windows

### pixi (recommended)

Needs [pixi](https://pixi.sh) and Visual Studio with "Desktop development
with C++" (MSVC can't come from conda-forge). Everything else comes from pixi
or is fetched on the first build:

    pixi run build                     # Release into _cmake_release/
    pixi run build -- --debug          # Debug into _cmake_debug/
    pixi run build -- --reconfigure    # also rerun CMake's configure step
    pixi run clean                     # remove _cmake_release/ and _cmake_debug/

What `build.py` (behind `pixi run build`) does:

- pixi provides CMake, Ninja, Python, git and `clang-cl` (for CORE-MATH,
  `mas_cpp_misc/cr_math`, which MSVC can't compile).
- MSVC is found with `vswhere` and its environment set up like a developer
  prompt - no need to start one.
- vcpkg provides Cap'n Proto, ZeroMQ and libsodium (static,
  `x64-windows-static`, matching MONICA's static runtime `/MT`). If
  `VCPKG_ROOT` isn't set, vcpkg at the tag in `vcpkg_tag.txt` is cloned into
  `.vcpkg/` on the first build (takes a while; later builds reuse it and
  vcpkg's binary cache).
- Generator Ninja (also on Windows), so the executables are directly in
  `_cmake_debug/` / `_cmake_release/` (no `Debug/`/`Release/` subfolder), and
  a `compile_commands.json` is written.
- Only the first build of a directory configures it; after that it
  reconfigures itself when a `CMakeLists.txt` or the vcpkg manifest changes.

In Zed (Windows):

| Task / debug configuration | Does |
|---|---|
| `[Windows] Build Debug (pixi)` | `pixi run build -- --debug`, then copies `_cmake_debug/compile_commands.json` to the root for clangd |
| `[Windows] Reconfigure + Build Debug (pixi)` | the same with `--reconfigure` |
| `[Windows] Build Release (pixi)` | `pixi run build` |
| `[Windows] Clean Debug (pixi)` | `cmake --build _cmake_debug --target clean` (keeps `vcpkg_installed/`) |
| `[Windows] Debug monica-run` (task) | runs `_cmake_debug/monica-run.exe` in raddbg |
| `[Windows] Debug monica-run` / `monica-zmq-server` / `monica-capnp-server` (debug configurations) | build, then CodeLLDB on `_cmake_debug/...` |

### Visual Studio + vcpkg (the release zip)

The Windows release zip is built by `.github/workflows/on-tag.yml` (and
`windows_build_install_package.yml` on pushes to `master`) with Visual
Studio's own generator and vcpkg in classic mode:

    vcpkg install zeromq:x64-windows-static capnproto:x64-windows-static libsodium:x64-windows-static
    cmake -G "Visual Studio 18 2026" -B _cmake_win64 -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static
    msbuild _cmake_win64/Monica.slnx /p:Configuration=Release /p:Platform=x64

`clang-cl` comes from the runner's LLVM installation here; locally, install
LLVM or Visual Studio's "C++ Clang tools for Windows". Executables end up in
`_cmake_win64/Release/`.

## Docker

    docker build -t zalfrpm/monica-cluster:<version> .

The `Dockerfile` runs `nix build` on your checkout (incl. submodules) and
puts the result into a Debian image with supervisor. Tagged releases build
and push it automatically (`on-tag.yml`).

## Which one when

- **Working on the code on Linux** (Zed, debugging): the dev shell build in
  `build/`.
- **Working on the code on Windows**: pixi, `_cmake_debug/`.
- **What the Docker image / HPC runs**: `nix build` (or the image itself).
- **Debugging a Nix build without a dev shell**: `nix build -o result-debug .#monica-debug`.
- **Reproducing a CI job**: the same way as the job - pixi for the pixi jobs,
  `nix build` for the Nix and Docker jobs.

## Things to know

- **CPU with FMA**: all builds use FMA instructions for CORE-MATH by default
  and then need an x86-64 CPU from 2012/2013 on. For older CPUs build with
  `-DCR_MATH_FMA=OFF` (same results, slower) - see the README section "Math
  functions, and CPUs without FMA".
- **Windows and Linux in the same checkout**: the Windows tasks copy a
  `compile_commands.json` into the root. Delete it before working on Linux,
  otherwise clangd uses it instead of `build/`'s.
