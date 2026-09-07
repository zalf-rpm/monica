#!/usr/bin/env python3
"""Pixi driver for the Odin port: bootstraps a pinned Odin compiler, resolves the
Windows MSVC toolchain, and drives `odin build` / `odin test`.

Why a Python driver instead of plain pixi tasks:

- The Odin compiler is NOT on conda-forge (checked: no package matches `odin`),
  so it has to come from a GitHub release. Pinning tag + SHA256 here is what
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
import os
import shutil
import subprocess
import sys
import tarfile
import tempfile
import urllib.request
import zipfile
from pathlib import Path

# --- pinned Odin compiler -----------------------------------------------------
# Bump all three fields together. Verify a new SHA256 with:
#   curl -sL <url> | sha256sum
ODIN_TAG = "dev-2026-08"
ODIN_ASSETS = {
    # sys.platform -> (asset filename, sha256)
    "linux": (
        f"odin-linux-amd64-{ODIN_TAG}.tar.gz",
        "d858c0a182bb28d7b04b04dbb8aed592a9c96c84e4400ee917c74b45848a4d87",
    ),
    "win32": (
        f"odin-windows-amd64-{ODIN_TAG}.zip",
        "4fa5bfcc1f51d748705a7b9bfe09de6ba71459429ac6c89fa6d2218a0976ba23",
    ),
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

IS_WINDOWS = sys.platform == "win32"
ODIN_EXE = DIST_DIR / ("odin.exe" if IS_WINDOWS else "odin")

TARGETS = {
    "monica-run": "cmd/monica-run",
    "monica-zmq-server": "cmd/monica-zmq-server",
    "monica-capnp-server": "cmd/monica-capnp-server",
    "monica-capnp-fbp-component": "cmd/monica-capnp-fbp-component",
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
        hit = next(
            (d for d in lib_dirs if str(d).lower().rstrip("\\/").endswith(suffix)),
            None,
        )
        if hit is None:
            return f"no Windows SDK {suffix} directory on %LIB%"
        if not (hit / probe).is_file():
            return f"incomplete Windows SDK ({hit / probe} is missing)"
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


def cmd_build(ns: argparse.Namespace) -> int:
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
        if name in CAPNP_TARGETS and static_capnp:
            args.append("-define:MAS_CAPNP_DYN_STATIC_LINK=true")
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
    tc = Path(root) / "scripts" / "buildsystems" / "vcpkg.cmake"
    if not tc.exists():
        die(f"{tc} not found - VCPKG_ROOT={root!r} does not look like a vcpkg checkout")
    return tc


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


def _drop_stale_build_dir(build_dir: Path, env: dict) -> None:
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
    `pixi run setup-msvc`. Reconfiguring from scratch is the only fix (the cache
    entry is not meant to be edited), so do it automatically rather than leaving
    a confusing compiler error.
    """
    cached = _cached_cxx_compiler(build_dir)
    if cached is None:
        return
    current = shutil.which("cl" if IS_WINDOWS else "c++", path=env.get("PATH", ""))
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
    env = odin_env()
    variants = ["static"] if ns.static else ["dll"]
    if ns.all:
        variants = ["dll", "static"]

    for variant in variants:
        # Release for the static variant, matching capnp_dynamic.odin's foreign
        # import path and its reason: a Debug (/MTd) combined lib fails to link
        # into an Odin exe with unresolved _CrtDbgReport.
        build_dir = SHIM_DIR / ("build-static" if variant == "static" else "build")
        config = "Release" if variant == "static" else "Debug"
        _drop_stale_build_dir(build_dir, env)
        configure = [
            "cmake", "-S", str(SHIM_DIR), "-B", str(build_dir),
            f"-DCMAKE_TOOLCHAIN_FILE={toolchain}",
            f"-DCMAKE_BUILD_TYPE={config}",
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

def cmd_test(ns: argparse.Namespace) -> int:
    BUILD_DIR.mkdir(parents=True, exist_ok=True)
    # cwd MUST be the repo root - see this file's module docstring.
    out = (BUILD_DIR / "tests.bin").relative_to(REPO).as_posix()
    return run_odin(["test", "odin/tests", f"-out:{out}", *ns.odin_args], REPO)


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
