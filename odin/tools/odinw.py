#!/usr/bin/env python3
"""Pixi driver for the Odin port: bootstraps a pinned Odin compiler, resolves the
Windows MSVC toolchain, and drives `odin build` / `odin test`.

Why a Python driver instead of plain pixi tasks:

- The Odin compiler is NOT on conda-forge (checked: no package matches `odin`),
  so it has to come from a GitHub release. Pinning tag + SHA256 (in
  versions.json, shared with ../flake.nix) is what
  makes builds repeatable; `pixi.lock` only covers the conda half.
- The two release archives normalize differently: the Linux tarball unpacks to
  `odin-linux-amd64-nightly+2026-08-06/` (a name that changes every release)
  while the Windows zip unpacks to `dist/`. Both are a single top-level
  directory, so that is what `bootstrap` keys on.
- Odin on Windows cannot link without MSVC's `lib\\x64` plus the Windows SDK
  `um\\x64` / `ucrt\\x64` import libraries. conda-forge ships none of those and
  never will - the Windows SDK EULA permits redistributing only "the results of
  running such Distributable Code through a linker", not the `.lib` files
  themselves. `-linker:lld` does not help: Odin ships its own `bin/lld-link.exe`
  and still hard-fails with "VS library path not found", a check that is not
  gated on the linker choice. So Windows needs a real toolchain located at
  build time, which is what `windows_msvc_env` does, in this order: an
  already-active developer environment, `$VCVARS`, a portable toolchain under
  `odin/msvc`, then a locally installed Visual Studio. The portable toolchain
  outranks the system one because it only exists if someone ran `setup-msvc`
  on purpose; each candidate is rejected with a reason rather than silently.
- Every candidate is a vcvars-style .bat whose environment has to be captured
  by running it. That subprocess call must NOT use subprocess's list form -
  see `_env_from_bat`. Getting this wrong breaks all four sources at once.
- `odin test odin/tests` must run from the REPO ROOT, not from `odin/`:
  `tests/conventions_test.odin` walks the literal relative path "odin" to grep
  for stray `core:math` transcendentals, so running it from `odin/` fails with
  ENOENT. Hence `test` sets cwd to the repo root.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile
import urllib.request
import zipfile
from pathlib import Path

# --- pinned versions ----------------------------------------------------------
# All third-party pins live in versions.json, which ../flake.nix reads too, so
# the pixi and Nix builds cannot drift apart. Bump the tag and both SHA256s
# together there.
VERSIONS = json.loads((Path(__file__).resolve().parent.parent / "versions.json").read_text())
ODIN_TAG = VERSIONS["odin"]["tag"]
ODIN_ASSETS = {
    # sys.platform -> (asset filename, sha256)
    "linux": (f"odin-linux-amd64-{ODIN_TAG}.tar.gz", VERSIONS["odin"]["sha256"]["linux"]),
    "win32": (f"odin-windows-amd64-{ODIN_TAG}.zip", VERSIONS["odin"]["sha256"]["win32"]),
}
ODIN_URL = "https://github.com/odin-lang/Odin/releases/download/{tag}/{asset}"

# --- pinned portable-MSVC fetcher (Windows fallback only) ---------------------
# mmozeiko's script, which Odin's own install docs link to. Pinned by gist
# revision AND content hash: a gist's "raw/<file>" URL is mutable, a
# "raw/<revision>/<file>" URL is not.
PMSVC_REV = "2ee035a22b3d8e1976ae725c8fc8b45638779910"
PMSVC_URL = (
    "https://gist.githubusercontent.com/mmozeiko/7f3162ec2988e81e56d5c4e22cde9977"
    f"/raw/{PMSVC_REV}/portable-msvc.py"
)
PMSVC_SHA256 = "84bc7fa28a45081f50b1144d14471264bef22b248816387de3d614b09ce10b59"

ROOT = Path(__file__).resolve().parent.parent      # <repo>/odin
REPO = ROOT.parent                                 # <repo>
ODIN_DIR = ROOT / ".odin"
CACHE_DIR = ODIN_DIR / "cache"
DIST_DIR = ODIN_DIR / "dist"
STAMP = ODIN_DIR / "installed.txt"
BUILD_DIR = ROOT / "build"
MSVC_DIR = ROOT / "msvc"                           # portable-msvc output
DOWNLOADS_DIR = ROOT / "downloads"                 # portable-msvc scratch/cache
SHIM_DIR = ROOT / "support" / "capnp" / "shim_dynamic"  # Cap'n Proto C++ shim (submodule)
VCPKG_MANIFEST_DIR = BUILD_DIR / "vcpkg-manifest"        # generated, see _write_vcpkg_manifest
CR_MATH_SRC = REPO / "mas_cpp_misc" / "cr_math"           # CORE-MATH (submodule), see ensure_cr_math
CR_MATH_DIR = BUILD_DIR / "cr_math"                       # where support/cr_math imports it from
CROP2ML_DIR = BUILD_DIR / "crop2ml"   # pinned PyCrop2ML + Crop2ML sources, see cmd_gen_amei
AMEI_DIR = ROOT / "amei"              # the generated Crop2ML packages, committed

IS_WINDOWS = sys.platform == "win32"
ODIN_EXE = DIST_DIR / ("odin.exe" if IS_WINDOWS else "odin")

TARGETS = {
    "monica-run": "cmd/monica-run",
    "monica-zmq-server": "cmd/monica-zmq-server",
    "monica-zmq-proxy": "cmd/monica-zmq-proxy",
    "monica-capnp-server": "cmd/monica-capnp-server",
    "monica-capnp-fbp-component": "cmd/monica-capnp-fbp-component",
    "monica-module": "cmd/monica-module",
}

# Targets that link the Cap'n Proto dynamic-API shim
# (support/capnp/shim_dynamic). Unlike libzmq, which conda-forge supplies, this
# one is built from the submodule's own CMake project - see cmd_build.
CAPNP_TARGETS = {"monica-capnp-server", "monica-capnp-fbp-component"}


def die(msg: str) -> "NoReturn":  # type: ignore[name-defined]
    sys.exit(f"error: {msg}")


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def download(url: str, dest: Path) -> None:
    dest.parent.mkdir(parents=True, exist_ok=True)
    tmp = dest.with_suffix(dest.suffix + ".part")
    print(f"  downloading {url}")
    with urllib.request.urlopen(url) as r, tmp.open("wb") as out:
        shutil.copyfileobj(r, out)
    tmp.replace(dest)


def fetch_verified(url: str, dest: Path, want_sha: str, what: str) -> Path:
    """Download `url` to `dest` unless already present with the right hash."""
    if dest.exists():
        if sha256_file(dest) == want_sha:
            return dest
        print(f"  cached {what} has the wrong hash, re-downloading")
        dest.unlink()
    download(url, dest)
    got = sha256_file(dest)
    if got != want_sha:
        dest.unlink(missing_ok=True)
        die(
            f"{what} SHA256 mismatch\n"
            f"  expected {want_sha}\n"
            f"  got      {got}\n"
            f"  url      {url}"
        )
    return dest


# --- bootstrap ----------------------------------------------------------------


def _extract(archive: Path, into: Path) -> None:
    if archive.name.endswith(".zip"):
        with zipfile.ZipFile(archive) as z:
            z.extractall(into)
    else:
        with tarfile.open(archive) as t:
            try:
                t.extractall(into, filter="data")  # Python 3.12+
            except TypeError:
                t.extractall(into)


def bootstrap(force: bool = False) -> Path:
    if not force and ODIN_EXE.exists() and STAMP.exists():
        if STAMP.read_text().strip() == ODIN_TAG:
            return ODIN_EXE

    try:
        asset, sha = ODIN_ASSETS[sys.platform]
    except KeyError:
        die(
            f"unsupported platform {sys.platform!r}; this setup pins Odin for "
            f"{', '.join(sorted(ODIN_ASSETS))} only"
        )

    print(f"==> bootstrapping Odin {ODIN_TAG} ({asset})")
    archive = fetch_verified(
        ODIN_URL.format(tag=ODIN_TAG, asset=asset),
        CACHE_DIR / asset,
        sha,
        "Odin release archive",
    )

    ODIN_DIR.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=ODIN_DIR) as td:
        tmp = Path(td)
        _extract(archive, tmp)
        # Both archives contain exactly one top-level directory, but with
        # different names (dated on Linux, "dist" on Windows) - normalize.
        entries = [p for p in tmp.iterdir()]
        inner = entries[0] if len(entries) == 1 and entries[0].is_dir() else tmp
        if DIST_DIR.exists():
            shutil.rmtree(DIST_DIR)
        shutil.move(str(inner), str(DIST_DIR))

    if not ODIN_EXE.exists():
        die(f"extraction produced no {ODIN_EXE.name} under {DIST_DIR}")
    if not (DIST_DIR / "core").is_dir():
        die(f"extraction produced no core/ library under {DIST_DIR}")
    if not IS_WINDOWS:
        ODIN_EXE.chmod(ODIN_EXE.stat().st_mode | 0o755)

    STAMP.write_text(ODIN_TAG + "\n")
    print(f"==> Odin ready at {ODIN_EXE}")
    return ODIN_EXE


# --- Windows MSVC discovery ---------------------------------------------------


# One import library per directory Odin's linker resolves against, probed by
# name. Checking for actual FILES rather than just the %LIB% path strings is
# what catches a toolchain whose unpack was interrupted: on Windows an
# antivirus or file-sync client (Sophos, Tresorit, OneDrive, ...) will happily
# lock or quarantine files mid-extraction, leaving a directory tree that looks
# complete but links nothing.
_LIB_PROBES = (
    ("um\\x64", "kernel32.lib"),
    ("ucrt\\x64", "ucrt.lib"),
)


def _msvc_env_problem(env: dict) -> str | None:
    """Mirror what Odin itself requires: MSVC tools plus SDK um/ucrt import libs.

    Returns None when the environment is usable, else a one-line reason.
    """
    tools = env.get("VCToolsInstallDir")
    if not tools:
        return "VCToolsInstallDir is not set"

    vc_lib = Path(tools) / "lib" / "x64"
    if not (vc_lib / "libcmt.lib").is_file():
        return f"missing MSVC import libraries ({vc_lib / 'libcmt.lib'})"

    lib_dirs = [Path(p) for p in env.get("LIB", "").split(";") if p.strip()]
    for suffix, probe in _LIB_PROBES:
        # ALL matching directories, like the linker searches all of %LIB%: the
        # .NET Framework SDK's NETFXSDK\<ver>\lib\um\x64 also ends in um\x64,
        # holds no kernel32.lib, and comes before the Windows SDK's on e.g.
        # GitHub's windows-latest runners.
        candidates = [d for d in lib_dirs if str(d).lower().rstrip("\\/").endswith(suffix)]
        if not candidates:
            return f"no Windows SDK {suffix} directory on %LIB%"
        if not any((d / probe).is_file() for d in candidates):
            return f"incomplete Windows SDK ({probe} is in none of: {', '.join(map(str, candidates))})"
    return None


def _msvc_env_ok(env: dict) -> bool:
    return _msvc_env_problem(env) is None


def _env_from_bat(bat: Path) -> dict | None:
    """Run a vcvars-style batch file and capture the environment it exports.

    The command line is assembled by hand and handed to CreateProcess as ONE
    string. It cannot go through subprocess's list form: that runs the args
    through `list2cmdline`, which escapes the quotes around `bat` as \\" - a
    convention cmd.exe does not implement. cmd then reads \\"C:\\... as the
    program name, and every toolchain fails identically with "the system cannot
    find the path specified". `/s` is the documented escape hatch: it strips
    exactly the outer quote pair and leaves the rest of the line alone.

    Decoding is lenient because `set` dumps the whole environment, which on a
    non-English Windows can hold bytes that are not valid in the ANSI codepage
    Python decodes with.
    """
    line = f'cmd.exe /s /c "call "{bat}" >nul 2>&1 && set"'
    try:
        out = subprocess.run(line, capture_output=True, text=True, errors="replace")
    except OSError:
        return None
    if out.returncode != 0:
        return None
    env = {}
    for entry in out.stdout.splitlines():
        if "=" in entry:
            k, v = entry.split("=", 1)
            env[k] = v
    return env or None


def _find_vcvars() -> Path | None:
    pf86 = os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")
    vswhere = Path(pf86) / "Microsoft Visual Studio" / "Installer" / "vswhere.exe"
    if vswhere.exists():
        try:
            out = subprocess.run(
                [
                    str(vswhere), "-nologo", "-latest", "-products", "*",
                    "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
                    "-property", "installationPath",
                ],
                capture_output=True,
                text=True,
            )
            for line in out.stdout.splitlines():
                bat = Path(line.strip()) / "VC" / "Auxiliary" / "Build" / "vcvars64.bat"
                if bat.exists():
                    return bat
        except OSError:
            pass
    # vswhere missing (older/partial installs) - probe the conventional layouts.
    for base in (os.environ.get("ProgramFiles", r"C:\Program Files"), pf86):
        for year in ("2022", "2019"):
            for ed in ("Enterprise", "Professional", "Community", "BuildTools"):
                bat = (
                    Path(base) / "Microsoft Visual Studio" / year / ed
                    / "VC" / "Auxiliary" / "Build" / "vcvars64.bat"
                )
                if bat.exists():
                    return bat
    return None


def _try_bat(bat: Path, label: str, notes: list[str]) -> dict | None:
    """Evaluate one candidate vcvars-style script, recording why it was rejected."""
    if not bat.exists():
        notes.append(f"{label}: not present ({bat})")
        return None
    env = _env_from_bat(bat)
    if env is None:
        notes.append(f"{label}: {bat} could not be run")
    elif (why := _msvc_env_problem(env)):
        notes.append(f"{label}: {why}")
    else:
        print(f"==> MSVC from {label} ({bat})")
        return env
    # The script exists, so someone meant to use it - say so even if a later
    # candidate ends up working, otherwise a half-unpacked msvc/ stays silent.
    print(f"warning: {notes[-1]}", file=sys.stderr)
    return None


def windows_msvc_env() -> dict:
    """Resolve an MSVC-capable environment, most explicit source first."""
    notes: list[str] = []

    # 1. Already configured - a VS developer prompt, or an outer vcvars call.
    if _msvc_env_ok(dict(os.environ)):
        print("==> MSVC from the ambient environment")
        return dict(os.environ)

    # 2. Explicit override. Same VCVARS knob odin/tests/cpp_ref/run*.sh use.
    if (vc := os.environ.get("VCVARS")):
        bat = Path(vc)
        if not bat.exists():
            die(f"VCVARS points at a missing file: {bat}")
        if (env := _try_bat(bat, "$VCVARS", notes)):
            return env
        die(f"VCVARS did not yield a usable MSVC environment\n  {notes[-1]}")

    # 3. Portable MSVC fetched by `pixi run setup-msvc`. Probed BEFORE the
    #    system Visual Studio: msvc/ exists only because someone deliberately
    #    fetched it, which makes it the stronger signal of intent - and it is
    #    the only way to exercise the portable path on a machine that also has
    #    VS installed. An incomplete tree warns and falls through to VS.
    if (env := _try_bat(MSVC_DIR / "setup_x64.bat", "portable toolchain", notes)):
        return env

    # 4. Locally installed Visual Studio.
    if (bat := _find_vcvars()):
        if (env := _try_bat(bat, "installed Visual Studio", notes)):
            return env
    else:
        notes.append(
            "installed Visual Studio: not found by vswhere or in the "
            "conventional install locations"
        )

    die(
        "no usable MSVC toolchain found, and Odin cannot link on Windows without\n"
        "  one. Odin needs MSVC's lib\\x64 plus the Windows SDK um\\x64 / ucrt\\x64\n"
        "  import libraries; conda-forge cannot ship these for licensing reasons.\n"
        "  Candidates tried:\n"
        + "\n".join(f"    - {n}" for n in notes)
        + "\n  Pick one:\n"
        "    - install VS 2022 Build Tools with 'Desktop development with C++'\n"
        "    - set VCVARS=<path to vcvars64.bat>\n"
        "    - fetch a portable toolchain:  pixi run setup-msvc -- --accept-license\n"
        "      (if it was fetched but is reported incomplete above, an antivirus or\n"
        "      file-sync client likely interfered - re-run it, with odin/msvc and\n"
        "      odin/downloads excluded from on-access scanning and file sync)"
    )


def odin_env() -> dict:
    return windows_msvc_env() if IS_WINDOWS else dict(os.environ)


# --- commands -----------------------------------------------------------------


def run_odin(args: list[str], cwd: Path) -> int:
    odin = bootstrap()
    cmd = [str(odin), *args]
    print(f"==> {' '.join(cmd)}   (cwd={cwd})")
    return subprocess.call(cmd, cwd=str(cwd), env=odin_env())


def _capnp_shim_artifacts() -> dict:
    """Where shim_dynamic's CMake build puts what the Odin side links against.

    Mirrors support/capnp/odin/capnp_dynamic/capnp_dynamic.odin's own foreign
    import paths - and its reason for two build directories: the combined static
    lib must be a Release (/MT) build, because Odin's linking does not pull in
    MSVC's debug static CRT the way a CMake/MSVC Debug build does.
    """
    if IS_WINDOWS:
        return {
            "dll_lib": SHIM_DIR / "build" / "mas_capnp_dynamic_shim.lib",
            "dll": SHIM_DIR / "build" / "mas_capnp_dynamic_shim.dll",
            "static": SHIM_DIR / "build-static" / "mas_capnp_dynamic_shim_combined.lib",
        }
    return {
        "dll_lib": SHIM_DIR / "build" / "libmas_capnp_dynamic_shim.so",
        "dll": SHIM_DIR / "build" / "libmas_capnp_dynamic_shim.so",
        "static": SHIM_DIR / "build-static" / "libmas_capnp_dynamic_shim_combined.a",
    }


def _cr_math_artifact() -> Path:
    """Where support/cr_math/cr_math.odin's foreign import expects the library."""
    return CR_MATH_DIR / ("cr_math_lib.lib" if IS_WINDOWS else "libcr_math_lib.a")


def _cr_math_linker_flags() -> list[str]:
    """support/cr_math imports the library as "system:cr_math_lib" off Windows:
    a path import becomes `-l:/abs/path.a`, which not every GNU ld accepts
    (nixpkgs' binutils doesn't). So the linker needs its directory."""
    return [] if IS_WINDOWS else [f"-L{CR_MATH_DIR.resolve()}"]


def ensure_cr_math(force: bool = False) -> None:
    """Build mas_cpp_misc/cr_math (CORE-MATH) into build/cr_math/, unless it is
    already there and newer than its sources.

    Every Odin target links it - all transcendental functions go through it
    (CONVENTIONS.md §1) - so `build` and `test` call this; unlike the Cap'n
    Proto shim it is small and needs no vcpkg. It uses mas_cpp_misc's own
    CMakeLists.txt, so the flags (-ffp-contract=off, CR_MATH_FMA) are exactly
    the C++ build's. On Linux it is compiled with the same conda clang +
    glibc-2.28 sysroot Odin links with (see _capnp_shim_toolchain for why
    that matters); on Windows in the MSVC environment Odin links in, where
    that CMakeLists.txt switches to clang-cl by itself (pixi supplies clang).
    """
    artifact = _cr_math_artifact()
    if not (CR_MATH_SRC / "CMakeLists.txt").is_file():
        die(
            f"{CR_MATH_SRC} not found - cr_math comes with the mas_cpp_misc submodule.\n"
            "  Run: git submodule update --init --recursive"
        )
    if not force and artifact.is_file():
        newest_src = max(f.stat().st_mtime for f in CR_MATH_SRC.rglob("*") if f.is_file())
        if artifact.stat().st_mtime >= newest_src:
            return
    if shutil.which("cmake") is None:
        die("cmake not found on PATH (needed to build mas_cpp_misc/cr_math)")

    env = odin_env()
    cmake_dir = BUILD_DIR / "cr_math-cmake"
    configure = [
        "cmake", "-S", str(CR_MATH_SRC), "-B", str(cmake_dir),
        "-DCMAKE_BUILD_TYPE=Release",
        "-DCMAKE_POSITION_INDEPENDENT_CODE=ON",
    ]
    if shutil.which("ninja", path=env.get("PATH")):
        configure += ["-G", "Ninja"]
    if not IS_WINDOWS and (tc := _capnp_shim_toolchain(env)) is not None:
        cxx, flags = tc
        cc = Path(cxx).with_name(Path(cxx).name.replace("clang++-", "clang-"))
        sysroot = " ".join(f for f in flags.split() if f.startswith("--sysroot"))
        configure += [f"-DCMAKE_C_COMPILER={cc}", f"-DCMAKE_C_FLAGS={sysroot}"]
    print(f"==> building cr_math (CORE-MATH) from {CR_MATH_SRC}", flush=True)
    for cmd in (configure, ["cmake", "--build", str(cmake_dir), "--config", "Release"]):
        print(f"==> {' '.join(cmd)}", flush=True)
        if subprocess.call(cmd, env=env) != 0:
            die("building cr_math failed")
    # Single- vs multi-config generators put it in different places.
    built = next(cmake_dir.rglob(artifact.name), None)
    if built is None:
        die(f"cr_math built, but no {artifact.name} under {cmake_dir}")
    CR_MATH_DIR.mkdir(parents=True, exist_ok=True)
    shutil.copy2(built, artifact)
    os.utime(artifact)  # newer than the sources even if copy2 kept an older mtime
    print(f"==> {artifact}", flush=True)


def cmd_cr_math(ns: argparse.Namespace) -> int:
    ensure_cr_math(force=True)
    return 0


def _pinned_checkout(name: str, url: str, commit: str) -> Path:
    """A checkout of `url` at `commit` in build/crop2ml/<name>, fetched (only
    that commit, shallow) on first use and moved when the pin changes."""
    d = CROP2ML_DIR / name
    def git(*a: str, check: bool = True) -> subprocess.CompletedProcess:
        return subprocess.run(["git", "-C", str(d), *a], check=check, capture_output=True, text=True)
    if not (d / ".git").is_dir():
        d.mkdir(parents=True, exist_ok=True)
        git("init", "-q")
        git("remote", "add", "origin", url)
    have = git("rev-parse", "HEAD", check=False).stdout.strip()
    if have != commit:
        print(f"==> fetching {name} at {commit[:10]}", flush=True)
        if subprocess.call(["git", "-C", str(d), "fetch", "-q", "--depth", "1", "origin", commit]) != 0:
            die(f"fetching {url} at {commit} failed")
        git("checkout", "-q", "--detach", "--force", "FETCH_HEAD")
    return d


def cmd_gen_amei(ns: argparse.Namespace) -> int:
    """Regenerate amei/<package>/ from the Crop2ML packages pinned in
    versions.json, with the pinned PyCrop2ML's Odin target.

    PyCrop2ML runs in its own pixi environment (its pyproject.toml), so its
    dependencies stay out of this one. The generator writes into the package
    directory (src/<target>/, doc images), so it runs on a fresh copy, never on
    the checkout. It also generates the Python code, which stays in
    build/crop2ml/work/ for tools/module_reference.py.
    """
    pins = VERSIONS["crop2ml"]
    pixi = os.environ.get("PIXI_EXE") or shutil.which("pixi")
    if pixi is None:
        die("pixi not found - PyCrop2ML runs in its own pixi environment")
    pycropml = _pinned_checkout("PyCrop2ML", pins["pycropml"]["url"], pins["pycropml"]["commit"])
    env = dict(os.environ)
    # as seen from amei/<package>/
    env["CYML_ODIN_MATH"] = "../../support/cr_math"
    for repo, src in pins["sources"].items():
        checkout = _pinned_checkout(repo, src["url"], src["commit"])
        work = CROP2ML_DIR / "work" / repo
        for pkg in src["packages"]:
            shutil.rmtree(work / pkg, ignore_errors=True)
            shutil.copytree(checkout / src["path"] / pkg, work / pkg)
            # the repository's own generated code: it may be stale, and a file
            # the generator doesn't write anymore would survive next to its output
            for target in ("odin", "py"):
                shutil.rmtree(work / pkg / "src" / target, ignore_errors=True)
            cmd = [pixi, "run", "--manifest-path", str(pycropml / "pyproject.toml"),
                   "python", str(pycropml / "src" / "pycropml" / "main.py"), "-p", pkg, "odin", "py"]
            print(f"==> {repo}/{pkg}: {' '.join(cmd[-4:])}", flush=True)
            if subprocess.call(cmd, cwd=work, env=env) != 0:
                die(f"generating {pkg} failed")
            generated = work / pkg / "src" / "odin" / pkg
            if not any(generated.glob("*.odin")):
                die(f"no Odin code in {generated}")
            dest = AMEI_DIR / pkg
            shutil.rmtree(dest, ignore_errors=True)
            dest.mkdir(parents=True)
            for f in sorted(generated.glob("*.odin")):
                shutil.copy2(f, dest / f.name)
            print(f"==> {dest.relative_to(ROOT)}", flush=True)
    return 0


def cmd_build(ns: argparse.Namespace) -> int:
    _check_pixi_zeromq()
    ensure_cr_math()
    BUILD_DIR.mkdir(parents=True, exist_ok=True)
    ext = ".exe" if IS_WINDOWS else ""
    wanted = ns.targets or list(TARGETS)
    for name in wanted:
        if name not in TARGETS:
            die(f"unknown target {name!r}; known: {', '.join(TARGETS)}")

    # Cap'n Proto targets need the shim built first - it is a CMake/vcpkg project
    # (support/capnp/shim_dynamic), not something Odin can produce. `capnp-shim`
    # builds it; check here so the failure names the fix instead of surfacing as
    # an unresolved-externals link error.
    art = _capnp_shim_artifacts()
    static_capnp = ns.capnp_link == "static"
    if any(n in CAPNP_TARGETS for n in wanted):
        needed = art["static"] if static_capnp else art["dll_lib"]
        if not needed.exists():
            die(
                f"{needed} not found - the Cap'n Proto shim has not been built.\n"
                f"  Run: pixi run capnp-shim{' -- --static' if static_capnp else ''}"
            )

    for name in wanted:
        args = [
            "build", TARGETS[name],
            f"-out:{(BUILD_DIR / (name + ext)).relative_to(ROOT).as_posix()}",
        ]
        # Default to an unoptimized build: the port's acceptance test is
        # byte-identical CSV against the C++ reference (CONVENTIONS.md §1), and
        # that is what every regression run so far has validated. -o:speed is
        # opt-in so any numeric drift it might introduce is a deliberate choice.
        if ns.release:
            args.append("-o:speed")
        linker_flags = _cr_math_linker_flags()
        if name in CAPNP_TARGETS and static_capnp:
            args.append("-define:MAS_CAPNP_DYN_STATIC_LINK=true")
        if name in CAPNP_TARGETS and not static_capnp and not IS_WINDOWS:
            # capnp_dynamic.odin's non-Windows dynamic branch imports the shim
            # as "system:mas_capnp_dynamic_shim" (Odin has no relative-path
            # .so import like Windows' relative .lib one), so the linker needs
            # an explicit -L to find libmas_capnp_dynamic_shim.so in
            # shim_dynamic/build/ - it's a project-local CMake artifact, not
            # something on the system/conda library path the way libzmq is.
            # -rpath,$ORIGIN makes the resulting binary find that same .so
            # next to itself at runtime, matching the copy done below.
            # Odin's own default rpath flag (visible via -print-linker-flags)
            # spells this \$ORIGIN, not $ORIGIN - it shells out to invoke the
            # linker, and an unescaped $ORIGIN gets consumed as a shell
            # variable reference (empty, since nothing sets $ORIGIN) before
            # ever reaching ld. Matching Odin's own escaping here is what
            # actually survives that.
            #
            # No --sysroot override needed here (there used to be one): as
            # of _capnp_shim_toolchain, cmd_capnp_shim builds the shim with
            # the same conda clang+sysroot Odin already links with, instead
            # of cmake's default (system) compiler - so both sides agree on
            # the same, deliberately old and portable glibc baseline.
            shim_dir = art["dll_lib"].parent.resolve()
            linker_flags += [f"-L{shim_dir}", "-Wl,-rpath,\\$ORIGIN"]
        if linker_flags:
            args.append(f"-extra-linker-flags:{' '.join(linker_flags)}")
        args += ns.odin_args
        if (rc := run_odin(args, ROOT)) != 0:
            return rc
        # The DLL variant resolves the shim at load time, so it has to sit next
        # to the exe. The static variant is self-contained - that is the whole
        # point of it (single-binary deployment).
        if name in CAPNP_TARGETS and not static_capnp:
            shutil.copy2(art["dll"], BUILD_DIR / art["dll"].name)
            print(f"==> copied {art['dll'].name} next to the binaries")

    print(f"==> binaries in {BUILD_DIR}")
    return 0


def _vcpkg_toolchain() -> Path:
    """vcpkg supplies Cap'n Proto for the shim. Located the same way the rest of
    this script locates things: an explicit env var first, never a guess."""
    root = os.environ.get("VCPKG_ROOT")
    if not root:
        die(
            "VCPKG_ROOT is not set - the Cap'n Proto shim needs vcpkg's capnproto.\n"
            "  Set VCPKG_ROOT to your vcpkg checkout (the one with scripts/buildsystems/)."
        )
    # expanduser: a literal "~/..." is easy to end up with (e.g. set from a Nix
    # attribute, where no shell expands it).
    tc = Path(root).expanduser() / "scripts" / "buildsystems" / "vcpkg.cmake"
    if not tc.exists():
        die(f"{tc} not found - VCPKG_ROOT={root!r} does not look like a vcpkg checkout")
    return tc


def _write_vcpkg_manifest() -> Path:
    """Generate the vcpkg manifest the shim's CMake configure installs from.

    Manifest mode makes the configure step install Cap'n Proto itself (into the
    shim's build dir), instead of relying on a manual `vcpkg install` into the
    checkout. It is generated rather than committed so the Cap'n Proto version
    and baseline come from versions.json - the same pins the Nix build uses -
    and it lives outside SHIM_DIR because that is a submodule of another repo.
    The baseline alone would already fix the version; the override makes it
    explicit and fails loudly if the baseline doesn't know that version.
    """
    manifest = {
        "name": "mas-capnp-dynamic-shim",
        "dependencies": ["capnproto"],
        "builtin-baseline": VERSIONS["vcpkg_baseline"],
        "overrides": [{"name": "capnproto", "version": VERSIONS["capnproto"]["version"]}],
    }
    VCPKG_MANIFEST_DIR.mkdir(parents=True, exist_ok=True)
    path = VCPKG_MANIFEST_DIR / "vcpkg.json"
    text = json.dumps(manifest, indent=2) + "\n"
    if not path.exists() or path.read_text() != text:
        path.write_text(text)
    return VCPKG_MANIFEST_DIR


def _pixi_zeromq_mismatch() -> str | None:
    try:
        import tomllib
    except ImportError:  # Python 3.10 and older; pixi.toml requires 3.11+
        return None
    spec = tomllib.loads((ROOT / "pixi.toml").read_text())["dependencies"]["zeromq"]
    want = VERSIONS["zeromq"]
    if spec.removesuffix(".*") != want:
        return f"pixi.toml pins zeromq {spec!r} but versions.json says {want!r}; make them agree"
    return None


def _check_pixi_zeromq() -> None:
    """pixi.toml can't read versions.json, so at least catch the two drifting."""
    if (msg := _pixi_zeromq_mismatch()) is not None:
        die(msg)


def _cached_cxx_compiler(build_dir: Path) -> str | None:
    """CMAKE_CXX_COMPILER as recorded in an existing CMakeCache.txt, if any."""
    cache = build_dir / "CMakeCache.txt"
    if not cache.is_file():
        return None
    for line in cache.read_text(errors="replace").splitlines():
        if line.startswith("CMAKE_CXX_COMPILER:"):
            return line.split("=", 1)[1].strip() if "=" in line else None
    return None


def _same_path(a: str | None, b: str | None) -> bool:
    if not a or not b:
        return False
    try:
        pa, pb = Path(a).resolve(), Path(b).resolve()
    except OSError:
        return False
    return (str(pa).lower() == str(pb).lower()) if IS_WINDOWS else (pa == pb)


def _capnp_shim_toolchain(env: dict) -> tuple[str, str] | None:
    """The (CXX compiler, extra CXX flags) cmd_capnp_shim pins the shim's
    CMake configure to on non-Windows - or None if pixi's conda clang isn't
    on PATH (e.g. running outside `pixi run capnp-shim`), in which case
    CMake's own default (system) compiler detection is used instead, same as
    before this existed.

    Why pin it at all: CMake's default detection finds whatever `c++` is on
    PATH - the system compiler (e.g. Ubuntu 24.04's GCC 13). Odin's own
    build links through pixi's conda `clang` package (see pixi.toml), whose
    bundled sysroot_linux-64 only declares glibc symbol versions up to 2.28
    (a deliberately old, portable baseline). A shim .so linked with the
    system's newer glibc (2.34+ on a current Ubuntu) then fails Odin's link
    with "undefined reference to `pthread_create@GLIBC_2.34`" and similar -
    a toolchain mismatch, not a stale-cache problem. Pinning only the shim's
    own final link is NOT enough: glibc's headers (2.38+) redirect e.g.
    strtoll to __isoc23_strtoll, and fortified builds call __inet_pton_chk -
    both baked into the .o files at COMPILE time, and absent from the 2.28
    sysroot. So vcpkg's capnproto/kj (and zlib) static libs must be built with
    this same clang+sysroot too, which _vcpkg_pixi_triplet arranges.

    Why all the manual flags instead of just -DCMAKE_CXX_COMPILER=clang++:
    conda's `clang` package ships the runtime (libstdc++.so) but not the
    C++ standard library HEADERS - pixi.toml additionally depends on
    libstdcxx-devel_linux-64 for those, installed under
    lib/gcc/<triple>/<ver>/include/c++. Clang's own GCC-installation
    auto-detection is, as of clang 22, known-buggy for exactly this split
    (confirmed empirically, and clang says so itself - the warning reads
    "future releases of the clang compiler will prefer GCC installations
    containing libstdc++ include directories" - implying current releases
    do not): with both the older GCC version directory bundled by the
    `clang` package itself (headers-less) and the newer one from
    libstdcxx-devel_linux-64 (has the headers) present, it silently PICKS
    THE HEADERS-LESS ONE, so plain `--gcc-toolchain=<prefix>` still fails
    with 'vector' file not found. Passing -isystem directly, for whichever
    installed GCC version dir actually HAS an include/c++ subdirectory,
    sidesteps that detection entirely. And `clang++-<ver>` (the C++ driver
    that auto-links libstdc++ - plain `x86_64-conda-linux-gnu-clang`
    compiles C++ syntax fine but does NOT auto-add -lstdc++/exception
    runtime support) has no matching .cfg of its own, so -L/-rpath for
    libstdc++.so and --sysroot are also passed explicitly rather than
    relying on any auto-loaded config file.
    """
    if IS_WINDOWS:
        return None
    cc = shutil.which("x86_64-conda-linux-gnu-clang", path=env.get("PATH", ""))
    if cc is None:
        return None
    bindir = Path(cc).parent
    prefix = bindir.parent
    cxx_candidates = sorted(bindir.glob("clang++-*"))
    if not cxx_candidates:
        return None
    cxx = str(cxx_candidates[-1])

    gcc_base = prefix / "lib" / "gcc" / "x86_64-conda-linux-gnu"
    cxx_header_dir = None
    for d in sorted(gcc_base.iterdir()) if gcc_base.is_dir() else []:
        if (d / "include" / "c++").is_dir():
            cxx_header_dir = d / "include" / "c++"  # last (newest) match wins
    if cxx_header_dir is None:
        return None

    sysroot = prefix / "x86_64-conda-linux-gnu" / "sysroot"
    libdir = prefix / "lib"
    flags = " ".join(
        [
            f"--sysroot={sysroot}",
            f"-isystem{cxx_header_dir}",
            f"-isystem{cxx_header_dir / 'x86_64-conda-linux-gnu'}",
            f"-isystem{cxx_header_dir / 'backward'}",
            f"-L{libdir}",
            f"-Wl,-rpath,{libdir}",
        ]
    )
    return cxx, flags


VCPKG_TRIPLET_DIR = BUILD_DIR / "vcpkg-triplets"  # generated, see _vcpkg_pixi_triplet
VCPKG_PIXI_TRIPLET = "x64-linux-pixi"


def _vcpkg_pixi_triplet(cxx: str, flags: str) -> Path:
    """Generate an overlay triplet that builds vcpkg's ports with the shim's
    clang+sysroot (see _capnp_shim_toolchain for why), instead of whatever
    compiler vcpkg would detect on PATH. Returns the overlay directory.

    Static libraries, like vcpkg's default x64-linux, so the shim .so absorbs
    Cap'n Proto and needs nothing else at runtime. The C compiler (for zlib) is
    the clang-<ver> next to clang++-<ver>; the C++-only -isystem flags are
    harmless for it but the sysroot and -L/-rpath are what matter.
    """
    cc = Path(cxx).with_name(Path(cxx).name.replace("clang++-", "clang-"))
    if not cc.exists():
        die(f"{cc} not found next to {cxx}")
    sysroot_flags = " ".join(f for f in flags.split() if not f.startswith("-isystem"))
    VCPKG_TRIPLET_DIR.mkdir(parents=True, exist_ok=True)
    toolchain = VCPKG_TRIPLET_DIR / "pixi-toolchain.cmake"
    files = {
        toolchain: (
            "# Generated by tools/odinw.py - see _vcpkg_pixi_triplet.\n"
            f'set(CMAKE_C_COMPILER "{cc.as_posix()}")\n'
            f'set(CMAKE_CXX_COMPILER "{Path(cxx).as_posix()}")\n'
            f'set(CMAKE_C_FLAGS_INIT "-fPIC {sysroot_flags}")\n'
            f'set(CMAKE_CXX_FLAGS_INIT "-fPIC {flags}")\n'
        ),
        VCPKG_TRIPLET_DIR / f"{VCPKG_PIXI_TRIPLET}.cmake": (
            "# Generated by tools/odinw.py - see _vcpkg_pixi_triplet.\n"
            "set(VCPKG_TARGET_ARCHITECTURE x64)\n"
            "set(VCPKG_CRT_LINKAGE dynamic)\n"
            "set(VCPKG_LIBRARY_LINKAGE static)\n"
            "set(VCPKG_CMAKE_SYSTEM_NAME Linux)\n"
            f'set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "{toolchain.as_posix()}")\n'
        ),
    }
    # Only rewrite on change: vcpkg hashes these into its ABI key, and an
    # unchanged mtime keeps CMake from reconfiguring for nothing.
    for path, text in files.items():
        if not path.exists() or path.read_text() != text:
            path.write_text(text)
    return VCPKG_TRIPLET_DIR


def _drop_stale_build_dir(build_dir: Path, env: dict, target_cxx: str | None) -> None:
    """Wipe build_dir if it was configured with a DIFFERENT compiler than the one
    this environment resolves to.

    CMake records an absolute compiler path in CMakeCache.txt and reuses it
    forever, while the INCLUDE/LIB this script sets come from whichever toolchain
    `windows_msvc_env` picked THIS time. Configure a directory with one and build
    it with the other and you get, e.g., system MSVC 14.44 compiling portable
    14.51's headers, which the STL rejects outright:

        yvals_core.h(921): error C2338: static_assert failed: 'error STL1001:
        Unexpected compiler version, expected MSVC Compiler 19.50 or newer.'

    That is the shape of it whenever the two entry points disagree - this script
    versus the shim's own vcbuild.bat, or a machine that gained/lost
    `pixi run setup-msvc`; or, on non-Windows, a build_dir configured before
    this script started pinning CMAKE_CXX_COMPILER to pixi's own clang++ (see
    _capnp_shim_toolchain) rather than leaving it to CMake's default
    detection. Reconfiguring from scratch is the only fix (the cache entry is
    not meant to be edited), so do it automatically rather than leaving a
    confusing compiler error.

    `target_cxx` is what the *upcoming* configure will pass as
    CMAKE_CXX_COMPILER - None means "whatever CMake defaults to", in which
    case this falls back to comparing against `c++`/`cl` on PATH, same as
    before.
    """
    cached = _cached_cxx_compiler(build_dir)
    if cached is None:
        return
    current = target_cxx or shutil.which("cl" if IS_WINDOWS else "c++", path=env.get("PATH", ""))
    if current is None or _same_path(cached, current):
        return
    print(f"==> {build_dir.name} was configured with a different compiler, reconfiguring", flush=True)
    print(f"      cached:  {cached}", flush=True)
    print(f"      current: {current}", flush=True)
    shutil.rmtree(build_dir, ignore_errors=True)


def cmd_capnp_shim(ns: argparse.Namespace) -> int:
    """Build support/capnp/shim_dynamic - the C++ side of the Cap'n Proto bindings.

    Deliberately NOT part of `build`: it needs CMake, a C++ compiler and a vcpkg
    checkout, none of which the Odin build otherwise requires, and it changes far
    less often than the Odin code.

    This is the supported entry point for building the shim from this repo, and
    the one CI should call to produce the static variant for single-binary
    releases. The shim submodule also carries its own vcbuild.bat, for using that
    repo standalone; the two share build directories, which is why
    _drop_stale_build_dir exists.
    """
    if not SHIM_DIR.exists():
        die(
            f"{SHIM_DIR} not found - the Cap'n Proto shim is a git submodule.\n"
            "  Run: git submodule update --init --recursive"
        )
    if shutil.which("cmake") is None:
        die("cmake not found on PATH")

    toolchain = _vcpkg_toolchain()
    manifest_dir = _write_vcpkg_manifest()
    env = odin_env()
    shim_toolchain = _capnp_shim_toolchain(env)
    if not IS_WINDOWS and shim_toolchain is None:
        die(
            "x86_64-conda-linux-gnu-clang (or a clang++-<ver> next to it) not "
            "found on PATH - are you running this via `pixi run capnp-shim`? "
            "The shim must be built with the same clang+sysroot Odin links "
            "with (see _capnp_shim_toolchain), or the final link against it "
            "fails with undefined GLIBC_2.3x references."
        )
    target_cxx = shim_toolchain[0] if shim_toolchain else None
    variants = ["static"] if ns.static else ["dll"]
    if ns.all:
        variants = ["dll", "static"]

    for variant in variants:
        # Release for the static variant, matching capnp_dynamic.odin's foreign
        # import path and its reason: a Debug (/MTd) combined lib fails to link
        # into an Odin exe with unresolved _CrtDbgReport.
        build_dir = SHIM_DIR / ("build-static" if variant == "static" else "build")
        config = "Release" if variant == "static" else "Debug"
        _drop_stale_build_dir(build_dir, env, target_cxx)
        configure = [
            "cmake", "-S", str(SHIM_DIR), "-B", str(build_dir),
            f"-DCMAKE_TOOLCHAIN_FILE={toolchain}",
            f"-DVCPKG_MANIFEST_DIR={manifest_dir}",
            f"-DCMAKE_BUILD_TYPE={config}",
        ]
        if shim_toolchain is not None:
            cxx, flags = shim_toolchain
            configure += [
                f"-DCMAKE_CXX_COMPILER={cxx}",
                f"-DCMAKE_CXX_FLAGS={flags}",
                f"-DVCPKG_OVERLAY_TRIPLETS={_vcpkg_pixi_triplet(cxx, flags)}",
                f"-DVCPKG_TARGET_TRIPLET={VCPKG_PIXI_TRIPLET}",
            ]
        if shutil.which("ninja"):
            configure += ["-G", "Ninja"]
        if IS_WINDOWS:
            configure.append("-DVCPKG_TARGET_TRIPLET=x64-windows-static")

        print(f"==> {' '.join(configure)}")
        if (rc := subprocess.call(configure, env=env)) != 0:
            return rc

        build = ["cmake", "--build", str(build_dir), "--config", config]
        if variant == "static":
            build += ["--target", "mas_capnp_dynamic_shim_combined"]
        print(f"==> {' '.join(build)}")
        if (rc := subprocess.call(build, env=env)) != 0:
            return rc

    art = _capnp_shim_artifacts()
    for variant in variants:
        produced = art["static"] if variant == "static" else art["dll"]
        print(f"==> {variant}: {produced}{'' if produced.exists() else '  (MISSING)'}")
    return 0


# Directories a from-scratch rebuild should never need to inspect. Not
# .odin/ (the bootstrapped Odin compiler itself - large download, never the
# source of a stale-artifact problem) and not odin/msvc or downloads/ (the
# fetched Windows toolchain, gated behind its own license-acceptance step -
# see cmd_setup_msvc - so cleaning it should be that same deliberate choice,
# not a side effect of a generic clean).
CLEAN_DIRS = [
    BUILD_DIR,
    SHIM_DIR / "build",
    SHIM_DIR / "build-static",
]


def cmd_clean(ns: argparse.Namespace) -> int:
    for d in CLEAN_DIRS:
        if d.exists():
            print(f"==> removing {d}")
            shutil.rmtree(d, ignore_errors=True)
        else:
            print(f"==> {d} already absent")
    return 0


def cmd_test(ns: argparse.Namespace) -> int:
    BUILD_DIR.mkdir(parents=True, exist_ok=True)
    ensure_cr_math()
    # cwd MUST be the repo root - see this file's module docstring.
    out = (BUILD_DIR / "tests.bin").relative_to(REPO).as_posix()
    args = ["test", "odin/tests", f"-out:{out}"]
    if (flags := _cr_math_linker_flags()):
        args.append(f"-extra-linker-flags:{' '.join(flags)}")
    return run_odin([*args, *ns.odin_args], REPO)


def cmd_exec(ns: argparse.Namespace) -> int:
    if not ns.odin_args:
        die("nothing to run; try: pixi run odin -- version")
    return run_odin(ns.odin_args, ROOT)


def cmd_setup_msvc(ns: argparse.Namespace) -> int:
    if not IS_WINDOWS:
        die("setup-msvc only applies to Windows builds")
    if "--accept-license" not in ns.odin_args:
        die(
            "this downloads Microsoft's MSVC compiler and Windows SDK, which are\n"
            "  covered by the Visual Studio license - the fetched msvc/ directory is\n"
            "  NOT redistributable and must not be committed. Accepting that license\n"
            "  has to be your explicit choice, so re-run as:\n"
            "    pixi run setup-msvc -- --accept-license"
        )
    # Ours, not portable-msvc.py's - strip it before forwarding.
    keep_downloads = "--keep-downloads" in ns.odin_args
    forwarded = [a for a in ns.odin_args if a != "--keep-downloads"]

    script = fetch_verified(
        PMSVC_URL, CACHE_DIR / "portable-msvc.py", PMSVC_SHA256, "portable-msvc.py"
    )
    print(f"==> fetching portable MSVC into {MSVC_DIR}")
    # The script hardcodes its output to ./msvc, so run it from odin/.
    rc = subprocess.call(
        [sys.executable, str(script), "--target", "x64", *forwarded], cwd=str(ROOT)
    )
    if rc != 0:
        return rc
    setup = MSVC_DIR / "setup_x64.bat"
    if not setup.exists():
        die(f"portable-msvc.py finished but {setup} is missing")

    # Verify here rather than leaving it to the next build. Unpacking ~1.3 GB of
    # .cab/.vsix trips on-access scanners and file-sync clients, which can lock
    # or quarantine individual files without failing the script - so confirm the
    # import libraries Odin links against actually landed.
    env = _env_from_bat(setup)
    if env is None:
        die(f"{setup} exists but could not be run")
    if (why := _msvc_env_problem(env)):
        die(
            f"the fetched toolchain is not usable: {why}\n"
            "  Unpacking was most likely interrupted by antivirus or a file-sync\n"
            "  client. Exclude odin/msvc and odin/downloads from on-access scanning\n"
            "  and file sync, then re-run:\n"
            "    pixi run setup-msvc -- --accept-license"
        )

    if not keep_downloads and DOWNLOADS_DIR.is_dir():
        # ~0.5 GB of .cab/.vsix that portable-msvc.py keeps as a re-run cache.
        print(f"==> removing {DOWNLOADS_DIR} (pass --keep-downloads to keep it)")
        shutil.rmtree(DOWNLOADS_DIR, ignore_errors=True)

    print(f"==> portable MSVC ready ({setup})")
    return 0


def cmd_check_msvc(ns: argparse.Namespace) -> int:
    """Report which toolchain a build would use, without building anything."""
    if not IS_WINDOWS:
        print("==> not Windows; no MSVC toolchain needed")
        return 0
    env = windows_msvc_env()  # dies with a per-candidate diagnostic if none work
    print(f"    VCToolsInstallDir = {env.get('VCToolsInstallDir')}")
    print(f"    WindowsSDKVersion = {env.get('WindowsSDKVersion')}")
    for entry in env.get("LIB", "").split(";"):
        if entry.strip():
            print(f"    LIB               = {entry}")
    return 0


def cmd_check_pins(ns: argparse.Namespace) -> int:
    """Check, from any OS, that everything versions.json pins actually exists for
    every path that consumes it - see UPDATING_VERSIONS.md. Availability only:
    whether the Windows build then compiles and links is CI's job.

    Parts whose tool is missing (no $VCPKG_ROOT, no nix) are reported as SKIP,
    not failures, so this runs anywhere; CI runs it with everything present.
    """
    results: list[tuple[str, str, str]] = []

    def report(status: str, what: str, detail: str = "") -> None:
        results.append((status, what, detail))
        print(f"  {status:<4} {what}{': ' + detail if detail else ''}", flush=True)

    print(f"==> checking pins in {ROOT / 'versions.json'}")

    # Odin: both release assets exist and match their SHA256 - downloaded into
    # the same cache bootstrap uses, so a later bootstrap reuses them.
    for plat, (asset, want) in sorted(ODIN_ASSETS.items()):
        url = ODIN_URL.format(tag=ODIN_TAG, asset=asset)
        dest = CACHE_DIR / asset
        try:
            if not dest.exists() or sha256_file(dest) != want:
                download(url, dest)
        except OSError as e:
            report("FAIL", f"odin {plat}", f"cannot download {url} ({e})")
            continue
        got = sha256_file(dest)
        if got == want:
            report("OK", f"odin {plat}", asset)
        else:
            dest.unlink(missing_ok=True)
            report("FAIL", f"odin {plat}", f"SHA256 is {got}, versions.json says {want}")

    # zeromq: pixi.toml, the lock (both platforms - this is what proves the
    # conda-forge package exists for Windows too), the vendored Windows import
    # lib, and the version the Odin bindings were written against.
    zmq = VERSIONS["zeromq"]
    if (msg := _pixi_zeromq_mismatch()) is not None:
        report("FAIL", "zeromq pixi.toml", msg)
    else:
        report("OK", "zeromq pixi.toml", zmq)
    locked = dict(
        re.findall(r"conda-forge/(linux-64|win-64)/zeromq-([0-9.]+)-", (ROOT / "pixi.lock").read_text())
    )
    for plat in ("linux-64", "win-64"):
        got = locked.get(plat)
        if got == zmq:
            report("OK", f"zeromq pixi.lock {plat}", got)
        else:
            report("FAIL", f"zeromq pixi.lock {plat}", f"locked {got!r}; run `pixi update zeromq`")
    zmq_lib = ROOT / "support" / "zeromq" / "windows" / "libzmq.lib"
    dll = f"libzmq-mt-{zmq.replace('.', '_')}.dll"
    if dll.encode() in zmq_lib.read_bytes():
        report("OK", "zeromq windows/libzmq.lib", f"imports {dll}")
    else:
        report("FAIL", "zeromq windows/libzmq.lib", f"does not import {dll}; replace it (see UPDATING_VERSIONS.md)")
    bindings = (ROOT / "support" / "zeromq" / "bindings.odin").read_text()
    parts = [re.search(rf"VERSION_{k}\s*::\s*(\d+)", bindings) for k in ("MAJOR", "MINOR", "PATCH")]
    bound = ".".join(m.group(1) for m in parts if m)
    if bound == zmq:
        report("OK", "zeromq bindings.odin", bound)
    else:
        report("WARN", "zeromq bindings.odin", f"VERSION_* say {bound}, pin is {zmq} (compile-time constants only)")

    # Cap'n Proto via vcpkg. Manifest mode resolves the override against the
    # versions database of the CHECKOUT, so the version must exist there; the
    # baseline commit must exist too (shallow clones lack it).
    capnp = VERSIONS["capnproto"]["version"]
    baseline = VERSIONS["vcpkg_baseline"]
    vcpkg_root = os.environ.get("VCPKG_ROOT")
    if not vcpkg_root or shutil.which("git") is None:
        report("SKIP", "capnproto vcpkg", "VCPKG_ROOT not set (or no git)")
    else:
        root = Path(vcpkg_root).expanduser()

        def git(*a: str) -> subprocess.CompletedProcess:
            return subprocess.run(["git", "-C", str(root), *a], capture_output=True, text=True)

        if git("cat-file", "-e", f"{baseline}^{{commit}}").returncode != 0:
            report("FAIL", "capnproto vcpkg baseline", f"{baseline} not in {root}; `git -C $VCPKG_ROOT fetch` (full history)")
        else:
            report("OK", "capnproto vcpkg baseline", baseline[:12])
            db = root / "versions" / "c-" / "capnproto.json"
            known = {v.get("version") for v in json.loads(db.read_text())["versions"]} if db.exists() else set()
            if capnp in known:
                report("OK", "capnproto vcpkg version", capnp)
            else:
                report("FAIL", "capnproto vcpkg version", f"{capnp} not in {db}; update the vcpkg checkout")
            r = git("show", f"{baseline}:versions/baseline.json")
            default = json.loads(r.stdout)["default"].get("capnproto", {}).get("baseline") if r.returncode == 0 else None
            if default != capnp:
                report("WARN", "capnproto vcpkg baseline default", f"{default}; only the override selects {capnp}")

    # Cap'n Proto + zeromq via Nix: the source hash, and the flake evaluating
    # at all (its zeromq assertion fires during evaluation).
    if shutil.which("nix") is None:
        report("SKIP", "nix", "nix not on PATH")
    else:
        r = subprocess.run(
            ["nix", "flake", "prefetch", "--json", f"github:capnproto/capnproto/v{capnp}"],
            capture_output=True, text=True,
        )
        want = VERSIONS["capnproto"]["nix_src_hash"]
        got = json.loads(r.stdout)["hash"] if r.returncode == 0 else None
        if got == want:
            report("OK", "capnproto nix_src_hash", got)
        else:
            report("FAIL", "capnproto nix_src_hash", f"is {got}, versions.json says {want}" if got else (r.stderr.strip().splitlines() or ["prefetch failed"])[-1])
        r = subprocess.run(
            ["nix", "eval", "--raw", f"{REPO}#packages.x86_64-linux.monica-odin.drvPath"],
            capture_output=True, text=True,
        )
        if r.returncode == 0:
            report("OK", "nix flake evaluates", "monica-odin")
        else:
            report("FAIL", "nix flake evaluates", r.stderr.strip().splitlines()[-1] if r.stderr.strip() else "")

    failed = [r for r in results if r[0] == "FAIL"]
    print(f"==> {len(failed)} failed, {sum(r[0] == 'WARN' for r in results)} warnings")
    return 1 if failed else 0


def main() -> int:
    ap = argparse.ArgumentParser(
        prog="odinw", description="build the Odin port of MONICA"
    )
    sub = ap.add_subparsers(dest="cmd", required=True)

    p = sub.add_parser("bootstrap", help="download the pinned Odin compiler")
    p.add_argument("--force", action="store_true", help="re-download even if present")
    p.set_defaults(fn=lambda ns: (bootstrap(ns.force), 0)[1])

    p = sub.add_parser("build", help="build the CLI binaries")
    p.add_argument("targets", nargs="*", help=f"default: {' '.join(TARGETS)}")
    p.add_argument("--release", action="store_true", help="build with -o:speed")
    p.add_argument(
        "--capnp-link",
        choices=("dll", "static"),
        default="dll",
        help=(
            "how monica-capnp-server links the Cap'n Proto shim: 'dll' (default,"
            " what you want while developing - the .dll is copied next to the exe)"
            " or 'static', for a single self-contained binary. Both need"
            " `capnp-shim` to have built the matching variant."
        ),
    )
    p.set_defaults(fn=cmd_build)


    p = sub.add_parser(
        "capnp-shim",
        help="build the Cap'n Proto C++ shim (support/capnp/shim_dynamic)",
    )
    p.add_argument(
        "--static",
        action="store_true",
        help="build the combined static lib (Release) instead of the DLL (Debug)",
    )
    p.add_argument("--all", action="store_true", help="build both variants")
    p.set_defaults(fn=cmd_capnp_shim)

    p = sub.add_parser(
        "clean",
        help="remove build/ and the Cap'n Proto shim's CMake build dirs",
    )
    p.set_defaults(fn=cmd_clean)

    p = sub.add_parser(
        "check-pins",
        help="check that every version in versions.json exists for pixi, vcpkg, Nix and Windows",
    )
    p.set_defaults(fn=cmd_check_pins)

    p = sub.add_parser(
        "cr-math",
        help="(re)build the CORE-MATH library from mas_cpp_misc/cr_math (build/test do it on demand)",
    )
    p.set_defaults(fn=cmd_cr_math)

    p = sub.add_parser(
        "gen-amei",
        help="regenerate amei/<package>/ from the Crop2ML packages pinned in versions.json",
    )
    p.set_defaults(fn=cmd_gen_amei)

    p = sub.add_parser("test", help="run the odin/tests unit suite")
    p.set_defaults(fn=cmd_test)

    p = sub.add_parser("exec", help="run the pinned odin with arbitrary arguments")
    p.set_defaults(fn=cmd_exec)

    p = sub.add_parser("setup-msvc", help="[Windows] fetch a portable MSVC toolchain")
    p.set_defaults(fn=cmd_setup_msvc)

    p = sub.add_parser(
        "check-msvc", help="[Windows] report which MSVC toolchain a build would use"
    )
    p.set_defaults(fn=cmd_check_msvc)

    # Anything this parser doesn't recognise is forwarded to odin (or to
    # portable-msvc.py). Both spellings have to work: `odinw exec -- version`
    # when called directly, and `pixi run odin -- version`, where pixi consumes
    # the `--` itself and appends a bare `version` to the task's command line.
    argv = sys.argv[1:]
    passthrough: list[str] = []
    if "--" in argv:
        i = argv.index("--")
        argv, passthrough = argv[:i], argv[i + 1 :]
    ns, extra = ap.parse_known_args(argv)
    ns.odin_args = extra + passthrough
    return ns.fn(ns)


if __name__ == "__main__":
    sys.exit(main())
