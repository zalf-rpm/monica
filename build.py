#!/usr/bin/env python3
"""Build MONICA on Linux or Windows without any manual setup - see pixi.toml.

pixi supplies the tools (CMake, Ninja, Python, git, on Linux a GCC, on
Windows clang-cl; MSVC itself is found via vswhere);
vcpkg supplies the libraries (Cap'n Proto, ZeroMQ, libsodium) in manifest
mode, from cmake/vcpkg/vcpkg.json. If $VCPKG_ROOT is not set, a vcpkg
checkout at the tag in vcpkg_tag.txt is cloned into .vcpkg/ and bootstrapped
on first use.

    pixi run build                 # Release, into _cmake_release/
    pixi run build -- --debug      # Debug, into _cmake_debug/
    pixi run build -- --reconfigure  # also rerun CMake's configure step
    pixi run clean

Why vcpkg rather than conda-forge for the libraries: CMakeLists.txt forces
MSVC's static runtime (/MT) on Windows, which only links against static
libraries built the same way - vcpkg's x64-windows-static, the triplet the
Windows CI and release builds use. conda-forge's are DLL (/MD) builds.
"""
from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
LOCAL_VCPKG = ROOT / ".vcpkg"
MANIFEST_DIR = ROOT / "cmake" / "vcpkg"
IS_WINDOWS = sys.platform == "win32"


def run(cmd: list[str], **kw) -> None:
    print("==> " + " ".join(cmd), flush=True)
    if (rc := subprocess.call(cmd, **kw)) != 0:
        sys.exit(rc)


def vcpkg_root() -> Path:
    """$VCPKG_ROOT if set (an existing checkout), else .vcpkg/, cloned and
    bootstrapped on first use at the tag the rest of the repo pins."""
    if (env := os.environ.get("VCPKG_ROOT")):
        root = Path(env).expanduser()
        if not (root / "scripts" / "buildsystems" / "vcpkg.cmake").is_file():
            sys.exit(f"error: VCPKG_ROOT={env!r} is not a vcpkg checkout")
        return root

    tag = (ROOT / "vcpkg_tag.txt").read_text().strip()
    exe = LOCAL_VCPKG / ("vcpkg.exe" if IS_WINDOWS else "vcpkg")
    if not (LOCAL_VCPKG / ".git").is_dir():
        # Shallow is enough: the manifest's builtin-baseline is this very tag.
        run(["git", "clone", "--depth", "1", "--branch", tag,
             "https://github.com/microsoft/vcpkg.git", str(LOCAL_VCPKG)])
    else:
        have = subprocess.run(["git", "-C", str(LOCAL_VCPKG), "describe", "--tags", "--exact-match"],
                              capture_output=True, text=True).stdout.strip()
        if have != tag:
            # vcpkg_tag.txt was bumped: move the clone along, and re-bootstrap,
            # since a newer tag can require a newer vcpkg executable.
            print(f"==> .vcpkg is at {have or 'an untagged commit'}, vcpkg_tag.txt says {tag}; updating",
                  flush=True)
            run(["git", "-C", str(LOCAL_VCPKG), "fetch", "--depth", "1", "origin", "tag", tag])
            run(["git", "-C", str(LOCAL_VCPKG), "checkout", "--detach", tag])
            exe.unlink(missing_ok=True)
    if not exe.is_file():
        script = "bootstrap-vcpkg.bat" if IS_WINDOWS else "bootstrap-vcpkg.sh"
        run([str(LOCAL_VCPKG / script), "-disableMetrics"], cwd=LOCAL_VCPKG)
    return LOCAL_VCPKG


def msvc_env() -> dict[str, str]:
    """The environment of an x64 developer prompt, for Ninja to find MSVC.
    The current one if cl.exe is already on PATH, else vcvars64.bat's of the
    newest Visual Studio with the C++ tools, as vswhere reports it."""
    if shutil.which("cl"):
        return dict(os.environ)
    vswhere = (Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)"))
               / "Microsoft Visual Studio" / "Installer" / "vswhere.exe")
    vs = vswhere.is_file() and subprocess.run(
        [str(vswhere), "-latest", "-products", "*", "-property", "installationPath",
         "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64"],
        capture_output=True, text=True).stdout.strip()
    if not vs:
        sys.exit('error: no Visual Studio with "Desktop development with C++" found')
    vcvars = Path(vs) / "VC" / "Auxiliary" / "Build" / "vcvars64.bat"
    print(f"==> MSVC environment from {vcvars}", flush=True)
    out = subprocess.run(f'"{vcvars}" >nul && set', shell=True, capture_output=True, text=True)
    if out.returncode != 0:
        sys.exit(f"error: {vcvars} failed:\n{out.stdout}{out.stderr}")
    return dict(line.split("=", 1) for line in out.stdout.splitlines() if "=" in line)


def cmd_build(ns: argparse.Namespace) -> int:
    config = "Debug" if ns.debug else "Release"
    build_dir = ROOT / ("_cmake_debug" if ns.debug else "_cmake_release")
    toolchain = vcpkg_root() / "scripts" / "buildsystems" / "vcpkg.cmake"
    # resolved before vcvars puts Visual Studio's own CMake and Ninja on PATH
    cmake = shutil.which("cmake") or "cmake"
    ninja = shutil.which("ninja") or "ninja"
    # Ninja on Windows too: the Visual Studio generator compiles the files of a
    # project one after another, and writes no compile_commands.json
    env = msvc_env() if IS_WINDOWS else None

    configure = [
        cmake, "-S", str(ROOT), "-B", str(build_dir), "-G", "Ninja",
        f"-DCMAKE_MAKE_PROGRAM={ninja}",
        f"-DCMAKE_BUILD_TYPE={config}",
        f"-DCMAKE_TOOLCHAIN_FILE={toolchain}",
        f"-DVCPKG_MANIFEST_DIR={MANIFEST_DIR}",
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
    ]
    if IS_WINDOWS:
        configure += ["-DVCPKG_TARGET_TRIPLET=x64-windows-static"]
    cache = build_dir / "CMakeCache.txt"
    ninja_cache = cache.is_file() and "CMAKE_GENERATOR:INTERNAL=Ninja\n" in cache.read_text()
    if cache.is_file() and not ninja_cache:
        # a build dir of the former Visual Studio generator; keeps vcpkg_installed/
        configure += ["--fresh"]
    # An existing build dir reconfigures itself when a CMakeLists.txt or the
    # vcpkg manifest changes; configuring every time costs half a minute of
    # vcpkg checks for a rebuild that may have nothing to do.
    if ns.reconfigure or not ninja_cache:
        run(configure, env=env)
    run([cmake, "--build", str(build_dir), "--parallel"], env=env)
    print(f"==> binaries in {build_dir}")
    return 0


def cmd_clean(ns: argparse.Namespace) -> int:
    for d in ("_cmake_release", "_cmake_debug"):
        if (ROOT / d).exists():
            print(f"==> removing {d}")
            shutil.rmtree(ROOT / d, ignore_errors=True)
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(prog="build.py", description="build MONICA")
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("build", help="configure and build (Release by default)")
    p.add_argument("--debug", action="store_true", help="Debug build into _cmake_debug/")
    p.add_argument("--reconfigure", action="store_true",
                   help="run the CMake configure step even if the build dir is configured already")
    p.set_defaults(fn=cmd_build)
    p = sub.add_parser("clean", help="remove _cmake_release/ and _cmake_debug/")
    p.set_defaults(fn=cmd_clean)
    ns = ap.parse_args()
    return ns.fn(ns)


if __name__ == "__main__":
    sys.exit(main())
