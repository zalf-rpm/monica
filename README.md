[![Build and test](https://github.com/zalf-rpm/monica/actions/workflows/build.yml/badge.svg)](https://github.com/zalf-rpm/monica/actions/workflows/build.yml)

# MONICA - The Model for Nitrogen and Carbon in Agro-ecosystems

MONICA is a dynamic, process-based simulation model which describes transport and bio-chemical turn-over of carbon, nitrogen and water in agro-ecosystems. On daily time steps the most important processes in soil and plant are modelled mechanistically. They are linked in such way that feed-back relations of the single processes are reproduced as close to nature as possible. MONICA works one dimensional and represents a space of 1 m² surface area and 2 m depth.

The acronym MONICA is derived from „MOdel of Nitrogen and Carbon dynamics in Agro-ecosystems”.

**Documentation:** the [MONICA documentation](https://zalf-rpm.github.io/monica-documentation/)
covers the model, its parameters and how to use it; the
[wiki](https://github.com/zalf-rpm/monica/wiki) has further notes.

# Getting MONICA

* **Windows:** download `monica_win64_<version>.zip` from the
  [releases page](https://github.com/zalf-rpm/monica/releases). It contains the
  executables (`bin/`), the parameters (`monica-parameters/`) and the
  Hohenfinow2 examples (`projects/`).
* **Docker:** `zalfrpm/monica-cluster:<version>`, e.g.
  `zalfrpm/monica-cluster:3.6.63.ext_channel`. Only versioned tags are published
  (no `latest`), so a run is always tied to a known version; see
  [Docker Hub](https://hub.docker.com/r/zalfrpm/monica-cluster/tags) for the list.
* **Nix (Linux):** no installation needed, see below.
* **From source:** see [Building](#building).

# Running a simulation

A simulation is described by a `sim.json` (simulation settings and outputs),
which refers to a `crop.json` (crop rotation, cultivation), a `site.json`
(soil, location) and a climate CSV. These include shared base data from the
[monica-parameters](https://github.com/zalf-rpm/monica-parameters) repository,
which MONICA finds via the `MONICA_PARAMETERS` environment variable. It is a
git submodule of this repository, so each MONICA version is tied to the
parameters it was released with:

    git clone --recurse-submodules https://github.com/zalf-rpm/monica.git
    export MONICA_PARAMETERS=$PWD/monica/monica-parameters    # Windows: set MONICA_PARAMETERS=...

The Windows zip and the Docker image already contain them.

Then run `monica-run` on a `sim.json`, e.g. the Hohenfinow2 example in
`installer/Hohenfinow2` (`-o` overrides the output file set in `sim.json`):

    cd installer/Hohenfinow2
    monica-run sim-min.json
    monica-run -o out.csv sim-min.json

The same with Docker (mounting the current directory and the parameters):

    docker run --rm -v "$PWD":/data -w /data -v "$MONICA_PARAMETERS":/params:ro -e MONICA_PARAMETERS=/params \
      zalfrpm/monica-cluster:3.6.63.ext_channel /run/monica/monica-run sim-min.json

Or with Nix, straight from GitHub (the `git+https` form with `submodules=1` is
needed: `github:` URLs download a tarball without the git submodules):

    nix run 'git+https://github.com/zalf-rpm/monica?submodules=1' -- sim-min.json
    nix shell 'git+https://github.com/zalf-rpm/monica?submodules=1'   # all tools on PATH

## As a server, from Python

For many runs, MONICA runs as a server and gets its jobs over ZeroMQ: a
producer sends simulation setups, `monica-zmq-server` runs them, a consumer
collects the results. `installer/Hohenfinow2/python` has a minimal producer
(`run-producer.py`) and consumer (`run-consumer.py`), see its
[README_pipeline.md](installer/Hohenfinow2/python/README_pipeline.md). They need
`pyzmq` and `zalfmas-common` (`pip install pyzmq zalfmas-common`):

    monica-zmq-server -bi -i tcp://localhost:6666 -bo -o tcp://localhost:7777 &
    python run-consumer.py &
    python run-producer.py

Without arguments, the Docker image starts such a setup itself (under
supervisor): two `monica-zmq-proxy`s and `$monica_instances` (default 3)
workers, taking jobs on port 6666 and returning results on port 7777.

## Tools

* `monica-run` - runs a single simulation from the command line
* `monica-zmq-server` - runs simulations received via ZeroMQ
* `monica-zmq-proxy` - distributes ZeroMQ jobs to a pool of `monica-zmq-server` workers
* `monica-capnp-server`, `monica-capnp-proxy` - the same via Cap'n Proto RPC
* `monica-capnp-fbp-component`, `daily-monica-fbp-component` - MONICA as components in flow-based (FBP) pipelines
* `channel`, `gateway-service`, `host-port-resolver-service`, `storage-service` - supporting services for the Cap'n Proto setups

# Building

Clone with the git submodules:

    git clone --recurse-submodules https://github.com/zalf-rpm/monica.git
    # or, in an existing clone:
    git submodule update --init --recursive

## With pixi (Linux and Windows)

With [pixi](https://pixi.sh) installed (on Windows also Visual Studio with
"Desktop development with C++"), pixi provides CMake, Ninja, Python and, on
Linux, the compiler. vcpkg provides Cap'n Proto, ZeroMQ and libsodium: if
`VCPKG_ROOT` isn't set, the vcpkg version in `vcpkg_tag.txt` is cloned into
`.vcpkg/` on the first build.

    pixi run build              # Release build into _cmake_release/
    pixi run build -- --debug   # Debug build into _cmake_debug/

## With Nix (Linux)

    nix build                   # all executables in result/bin
    nix develop                 # shell with all dependencies, then e.g.:
    cmake -B _cmake_release -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build _cmake_release

With [direnv](https://direnv.net) (and nix-direnv), the dev shell activates
automatically in the checkout after a one-time `direnv allow`.

## Manually with vcpkg

Install CMake, a C++17 compiler and [vcpkg](https://vcpkg.io) (at the version in
`vcpkg_tag.txt`), then install `capnproto`, `zeromq` and `libsodium` with vcpkg
and configure with vcpkg's toolchain. On Windows, use the static triplet for
both (`vcpkg install ...:x64-windows-static`, and add
`-DVCPKG_TARGET_TRIPLET=x64-windows-static` to the first `cmake` line):

    cmake -B _cmake_release -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake -DCMAKE_BUILD_TYPE=Release
    cmake --build _cmake_release --config Release

## Docker image

    docker build -t zalfrpm/monica-cluster:<version> .

The `Dockerfile` builds your checkout (including its submodules) with the Nix
flake. Tagged releases build and push the image automatically
(`.github/workflows/on-tag.yml`).

## Math functions, and CPUs without FMA

MONICA calculates `exp`, `log`, `pow`, `sin`, `cos` etc. with the correctly
rounded functions of [CORE-MATH](https://core-math.gitlabpages.inria.fr/)
(`mas_cpp_misc/cr_math`), not with the platform's math library. Windows and
Linux math libraries round some results differently in the last bit, and the
model is sensitive enough for that to change yields by several percent over a
few decades. With CORE-MATH the results are the same on every platform.

CORE-MATH is built to use the CPU's FMA (fused multiply-add) instructions.
With them MONICA runs within about 10% of its speed with the platform's math
library; without them it is about 30% slower than that (muencheberg_v140 on
Linux, 2026). **The executables (Windows, Linux, Docker) therefore need an
x86-64 CPU with FMA**: Intel since Haswell (2013), AMD since Piledriver
(2012). On an older CPU, or in a virtual machine that hides FMA, MONICA stops
right away with an "illegal instruction" error. In that case build it with

    cmake ... -DCR_MATH_FMA=OFF

which gives exactly the same results, just slower. On Windows the CORE-MATH
sources are compiled with `clang-cl` (pixi provides it; otherwise install
LLVM or Visual Studio's "C++ Clang tools for Windows").

# Testing

`tests/` runs a set of simulation suites (Hohenfinow2, scenario repositories
like [muencheberg_v140](https://github.com/zalf-rpm/muencheberg_v140)) with a
MONICA build and compares the outputs of different builds, e.g. Windows
against Linux:

    pixi run test --monica _cmake_release/monica-run --out results/a
    pixi run compare results/a results/b

CI does this on every push for all builds. See [tests/README.md](tests/README.md).

## Step-by-step guides

The wiki has step-by-step instructions for
[Windows](https://github.com/zalf-rpm/monica/wiki/How-to-compile-MONICA-(Windows)) and
[Linux](https://github.com/zalf-rpm/monica/wiki/How-to-compile-MONICA-(Linux)).

# Contributing

Contributions are welcome.

# Authors

MONICA is a model of the [Research Platform "Data Analysis & Simulation"](https://www.zalf.de/en/struktur/fds/Pages/default.aspx) at the [Leibniz Centre for Agricultural Landscape Research (ZALF)](http://www.zalf.de/en), maintained by its authors:

* Claas Nendel (claas (dot) nendel [at] zalf (dot) de)
* Xenia Specka (xenia (dot) specka [at] zalf (dot) de)
* Michael Berg-Mohnicke (michael (dot) berg [at] zalf (dot) de)

# License

MONICA is distributed under the [Mozilla Public License, v. 2.0](http://mozilla.org/MPL/2.0/).
