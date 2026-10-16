#!/usr/bin/env python3
"""Build MONICA on Linux or Windows without any manual setup - see pixi.toml.

pixi supplies the tools (CMake, Ninja, Python, git, and on Linux a GCC);
vcpkg supplies the libraries (Cap'n Proto, ZeroMQ, libsodium) in manifest
mode, from cmake/vcpkg/vcpkg.json. If $VCPKG_ROOT is not set, a vcpkg
checkout at the tag in vcpkg_tag.txt is cloned into .vcpkg/ and bootstrapped
on first use.

    pixi run build                 # Release, into _cmake_release/
    pixi run build -- --debug      # Debug, into _cmake_debug/
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
            print(f"warning: .vcpkg is at {have or 'an untagged commit'}, vcpkg_tag.txt says {tag}; "
                  "delete .vcpkg/ to re-clone", flush=True)
    if not exe.is_file():
        script = "bootstrap-vcpkg.bat" if IS_WINDOWS else "bootstrap-vcpkg.sh"
        run([str(LOCAL_VCPKG / script), "-disableMetrics"], cwd=LOCAL_VCPKG)
    return LOCAL_VCPKG


def cmd_build(ns: argparse.Namespace) -> int:
    config = "Debug" if ns.debug else "Release"
    build_dir = ROOT / ("_cmake_debug" if ns.debug else "_cmake_release")
    toolchain = vcpkg_root() / "scripts" / "buildsystems" / "vcpkg.cmake"

    configure = [
        "cmake", "-S", str(ROOT), "-B", str(build_dir),
        f"-DCMAKE_TOOLCHAIN_FILE={toolchain}",
        f"-DVCPKG_MANIFEST_DIR={MANIFEST_DIR}",
    ]
    if IS_WINDOWS:
        # CMake's default generator here is the newest Visual Studio it finds,
        # which also locates MSVC itself - no developer prompt needed.
        configure += ["-A", "x64", "-DVCPKG_TARGET_TRIPLET=x64-windows-static"]
    else:
        configure += ["-G", "Ninja", f"-DCMAKE_BUILD_TYPE={config}"]
    run(configure)
    run(["cmake", "--build", str(build_dir), "--config", config, "--parallel"])
    print(f"==> binaries in {build_dir / config if IS_WINDOWS else build_dir}")
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
    p.set_defaults(fn=cmd_build)
    p = sub.add_parser("clean", help="remove _cmake_release/ and _cmake_debug/")
    p.set_defaults(fn=cmd_clean)
    ns = ap.parse_args()
    return ns.fn(ns)


if __name__ == "__main__":
    sys.exit(main())
