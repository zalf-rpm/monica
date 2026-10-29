# How to build the Odin MONICA

There are several ways to build the Odin port. They differ in where they build,
what they produce, and what they are meant for. This page explains which one
does what. For the C++ MONICA see `../how-to-build.md` on the C++ `master`.

In every case, clone with the git submodules first: `mas_cpp_misc` (CORE-MATH,
`cr_math`) and `odin/support/capnp` (the Cap'n Proto shim):

    git clone --recurse-submodules -b simplify_copilot https://github.com/zalf-rpm/monica.git
    # or, in an existing clone:
    git submodule update --init --recursive

## Overview

| OS | Way | Command | Builds in | Result | Meant for |
|---|---|---|---|---|---|
| Linux, Windows | pixi | `pixi run build` | `odin/build/` | there, unoptimized | day-to-day work, CI's pixi jobs and test suites |
| Linux, Windows | pixi, optimized | `pixi run build-release` | `odin/build/` | there, `-o:speed` | speed; not what the comparisons validate |
| Linux, Windows | pixi, single binary | `pixi run build-standalone` | `odin/build/` | a self-contained `monica-capnp-server` | releases without the shim library |
| Linux | Nix package | `nix build .#monica-odin` (= `nix build`) | a sandbox, result in `/nix/store` | `result/bin/` (link) | a reproducible build, CI's Nix job |
| Linux | Nix dev shell | `nix develop` | - | the tools for the pixi build and the editor | Odin language server, debugger, vcpkg's C++ needs |
| any | Docker | `docker build -f odin/Dockerfile .` | Docker | an image | `zalfrpm/monica-odin`, a drop-in for the C++ image |

All of them build the same programs from `odin/cmd/`:

| Program | What |
|---|---|
| `monica-run` | runs a sim.json, like the C++ `monica-run` |
| `monica-zmq-server`, `monica-zmq-proxy` | the ZeroMQ server and proxy (the Docker image runs them) |
| `monica-capnp-server`, `monica-capnp-fbp-component` | the Cap'n Proto service and FBP component - need the shim |
| `monica-module` | runs a swappable module (`plan-crop2ml-modules.md`) standalone |

CI (`.github/workflows/odin.yml`) builds with pixi on Linux and Windows, with
Nix and with Docker, runs the C++ MONICA's test suites with the pixi-built
`monica-run`, and checks that the results are identical, value by value, to the
C++ MONICA at the commit pinned in `versions.json` (`cpp_reference`, see
`UPDATING_VERSIONS.md`).

All commands below run in `odin/` unless they say otherwise.

## pixi (Linux and Windows)

Needs [pixi](https://pixi.sh). It brings Python, clang, CMake, Ninja and
zeromq (pinned in `pixi.lock`); the Odin compiler itself isn't on conda-forge
and is downloaded by `tools/odinw.py` at the version and SHA256 in
`versions.json`, into `.odin/`. `pixi run <task>` does that on first use.

A fresh checkout, the first time:

    pixi run capnp-shim       # the Cap'n Proto shim (needs vcpkg, see below), once
    pixi run build            # all programs into build/
    pixi run test             # the unit tests (odin/tests)

Without the shim, `pixi run build` stops at the first Cap'n Proto program and
says so. If you don't need those two, build the others by name:

    pixi run build monica-run monica-zmq-server monica-zmq-proxy monica-module

| Task | Does |
|---|---|
| `pixi run build [targets]` | builds into `build/`, unoptimized (the default, see below) |
| `pixi run build-release` | the same with `-o:speed` |
| `pixi run test` | the unit tests in `odin/tests` (run from the repo root, as they need to be) |
| `pixi run capnp-shim` | builds the shim library `support/capnp/shim_dynamic` (CMake + vcpkg's Cap'n Proto) |
| `pixi run capnp-shim-static` | the shim as one static library, for `build-standalone` |
| `pixi run build-standalone` | `monica-capnp-server` with the shim linked in statically |
| `pixi run clean` | removes `build/` and the shim's CMake build directories |
| `pixi run odin -- <args>` | the pinned Odin compiler, e.g. `pixi run odin -- version` |
| `pixi run gen-amei` | regenerates `amei/` from the pinned Crop2ML packages (`plan-crop2ml-modules.md`) |
| `pixi run python tools/odinw.py check-pins` | checks that every pin in `versions.json` exists everywhere |

What `tools/odinw.py` (behind the tasks) does:

- builds CORE-MATH (`mas_cpp_misc/cr_math`) into `build/cr_math/` on demand,
  with `mas_cpp_misc`'s own CMake flags, and links every program against it
  (`CONVENTIONS.md` §1);
- calls `odin build cmd/<program> -out:build/<program>` - **unoptimized by
  default**: the port's acceptance test is identical results to the C++, and
  that is what the comparisons validate. `--release` (`-o:speed`) is opt-in;
- copies the shim library next to the Cap'n Proto programs, which find it
  there at run time.

### Cap'n Proto shim: vcpkg

The two Cap'n Proto programs call Cap'n Proto's C++ library through a small C
API, the shim (`support/capnp/shim_dynamic`, a submodule), built with CMake.
Cap'n Proto comes from vcpkg, in manifest mode at the version and baseline in
`versions.json` (`odinw.py` generates the manifest):

    git clone https://github.com/microsoft/vcpkg ~/GitHub/vcpkg    # full history: the baseline must be reachable
    ~/GitHub/vcpkg/bootstrap-vcpkg.sh -disableMetrics               # bootstrap-vcpkg.bat on Windows
    export VCPKG_ROOT=~/GitHub/vcpkg                                 # the Nix dev shell sets this default
    pixi run capnp-shim

The first build compiles Cap'n Proto (a few minutes); vcpkg's binary cache
makes later ones quick.

### Windows

Additionally needs an MSVC toolchain: Odin can't link a Windows executable
without MSVC's and the Windows SDK's libraries, and their license forbids
redistributing them, so conda-forge can't provide them. `odinw.py` looks, in
this order, for an already active developer environment, `$VCVARS`, a portable
toolchain fetched with `setup-msvc`, and an installed Visual Studio (via
`vswhere`):

    pixi run check-msvc                            # which toolchain a build would use
    pixi run setup-msvc -- --accept-license        # fetch a portable one into odin/msvc/ (don't redistribute it)

With Visual Studio and "Desktop development with C++" installed nothing else
is needed. `clang` from pixi (`clang-cl`) builds CORE-MATH, which MSVC can't
compile. Programs get `.exe` and land in `build/` like on Linux; zeromq's DLL is
found through pixi's environment (`pixi run` puts it on `PATH`).

## Nix (Linux)

### Nix package: `nix build .#monica-odin`

    nix build .#monica-odin       # or just `nix build`: monica-odin is the default package

Run in the repository root. Like for the C++, Nix builds in a sandbox and
stores the package in `/nix/store`; `result` is a link to it, replaced by
every `nix build`. Odin, zeromq, Cap'n Proto, the shim and CORE-MATH are all
built from pinned sources (`flake.lock`, and `versions.json` for Odin and Cap'n
Proto). Unoptimized, like `pixi run build`. The Cap'n Proto programs are
wrapped to find their schemas in the store.

Other packages of the flake: `odin` (the compiler), `capnproto`,
`capnp-shim`, `cr-math`, and `monica-cpp-src` - the converted C++ in `src/`,
TEMPORARY until the move to Odin is complete (CI compares it against the C++
reference too).

### Nix dev shell

`nix develop` (in the repository root) gives the pinned Odin compiler, `ols`
(the Odin language server), CMake, Ninja, clangd, gdb and lldb, and sets
`VCPKG_ROOT` to `~/GitHub/vcpkg` if it isn't set. It is meant to go with the
pixi build: pixi supplies Python, clang and zeromq, the shell what pixi
doesn't (a C++ compiler for vcpkg's source build of Cap'n Proto, editor and
debugger tooling).

## Docker

    docker build -f odin/Dockerfile -t zalfrpm/monica-odin .

Run in the repository root (the build context needs `docker/` as well as
`odin/`). Unlike the C++ Dockerfile it builds the checked-out working tree,
including the submodules. The image is a drop-in for the C++ one: same Debian
release, `/run/monica` layout, environment variables and `supervisord.conf`
(two `monica-zmq-proxy` and `$monica_instances` `monica-zmq-server` workers).
Cap'n Proto is built from source in the image at the pinned version.

## Running

`monica-run` reads a sim.json; the parameter files are found through
`MONICA_PARAMETERS` (a checkout of
[monica-parameters](https://github.com/zalf-rpm/monica-parameters)):

    export MONICA_PARAMETERS=~/GitHub/monica-parameters
    cd ../installer/Hohenfinow2 && ../../odin/build/monica-run sim-min.json

## Checks

| Command | Checks |
|---|---|
| `pixi run test` | the unit tests |
| `bash tests/diff_outputs.sh --save`, then `bash tests/diff_outputs.sh` (from the repo root) | that a change keeps the Hohenfinow2 outputs byte-identical (snapshot, then compare) |
| `pixi run gen-amei`, then `pixi run bash tests/check_modules.sh` | the swappable modules: standalone against the Crop2ML Python reference, as drivers, integrated (needs `MONICA_PARAMETERS`) |
| `python <C++ checkout>/tests/run.py --monica build/monica-run --out <dir>` and `compare.py` | the C++ test suites, compared with a C++ build - what CI does |
| `bash tests/diff_vs_cpp.sh` (from the repo root) | `sim-min.json` with a C++ `monica-run` (default: the checkout `../monica`, built in its `build/`) and the Odin one, every output section diffed |

## Which one when

- **Working on the code** (Linux or Windows): pixi, `pixi run build` and
  `pixi run test`; on Linux in the Nix dev shell for `ols` and the debugger.
- **Only `monica-run`, no Cap'n Proto**: `pixi run build monica-run` - no
  vcpkg needed.
- **A reproducible Linux build**: `nix build .#monica-odin`.
- **The server image**: `docker build -f odin/Dockerfile .`.
- **Reproducing a CI job**: the same way as the job - pixi for the pixi jobs
  (`capnp-shim`, `build`, `test`), `nix build .#monica-odin` for the Nix job.

## Things to know

- **CPU with FMA**: CORE-MATH is built with FMA instructions, so the programs
  need an x86-64 CPU from 2012/2013 on (`CONVENTIONS.md` §1). The port's own
  code is built for generic x86-64, without FMA contraction - that is part of
  getting the same results as the C++.
- **Unoptimized by default**: only the default build is what CI compares
  with the C++; check `build-release` (`-o:speed`) the same way before relying
  on its results.
- **Pins**: the Odin compiler, zeromq, Cap'n Proto, vcpkg's baseline, the
  Crop2ML packages and the C++ reference are pinned in `versions.json`; how to
  move them is in `UPDATING_VERSIONS.md`.
- **Odin's conventions** (naming, memory, math, units): `CONVENTIONS.md`.
