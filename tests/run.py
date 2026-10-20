#!/usr/bin/env python3
"""Run the MONICA test suites with one monica-run and record the results.

    python tests/run.py --monica _cmake_release/monica-run --out results/linux

For every suite in tests/suites.toml this fetches the suite (scenario repos at
their pinned commit), runs each simulation listed in its monica-test.toml and
writes, per simulation, a directory <out>/<suite>/<sim>/ with out.csv (plus any
further files the simulation writes) and monica.log. <out>/metadata.json
records what was run: engine, platform, MONICA version, parameters and suite
commits, return codes and run times. tests/compare.py compares two such
result directories.

--monica can be anything taking monica-run's arguments, e.g. a wrapper running
monica-run inside a Docker image (tests/docker-monica-run.sh); it gets the
suite's directory in $MONICA_TEST_SUITE. Only the
standard library is used, so any Python >= 3.11 will do.
"""
from __future__ import annotations

import argparse
import concurrent.futures
import glob
import json
import os
import platform
import shutil
import subprocess
import sys
import time
import tomllib
from pathlib import Path

TESTS = Path(__file__).resolve().parent
ROOT = TESTS.parent
MANIFEST = "monica-test.toml"


def git(*args: str, cwd: Path | None = None) -> str:
    return subprocess.run(["git", *args], cwd=cwd, check=True,
                          capture_output=True, text=True).stdout.strip()


def git_state(path: Path) -> dict | None:
    """Commit and dirtiness of the git checkout at path, if it is one."""
    try:
        return {"commit": git("rev-parse", "HEAD", cwd=path),
                "dirty": bool(git("status", "--porcelain", "--", ".", cwd=path))}
    except (subprocess.CalledProcessError, FileNotFoundError):
        return None


def fetch(suite: dict, cache: Path) -> Path:
    """Checkout of a git suite at its pinned rev, cached in cache/<name>."""
    name, url, rev = suite["name"], suite["git"], suite["rev"]
    dest = cache / name
    if not (dest / ".git").is_dir():
        shutil.rmtree(dest, ignore_errors=True)
        dest.mkdir(parents=True)
        git("init", "--quiet", cwd=dest)
        git("remote", "add", "origin", url, cwd=dest)
    if git_state(dest) != {"commit": rev, "dirty": False}:
        print(f"==> fetching {name} at {rev}", flush=True)
        git("fetch", "--quiet", "--depth", "1", "origin", rev, cwd=dest)
        git("checkout", "--quiet", "--force", "FETCH_HEAD", cwd=dest)
        git("clean", "--quiet", "-dfx", cwd=dest)
    return dest


def suite_dir(suite: dict, overrides: dict[str, Path], cache: Path) -> Path:
    if suite["name"] in overrides:
        return overrides[suite["name"]]
    if "path" in suite:
        return ROOT / suite["path"]
    return fetch(suite, cache)


def sim_files(directory: Path, patterns: list[str]) -> list[Path]:
    sims: list[Path] = []
    for pattern in patterns:
        found = sorted(Path(p) for p in glob.glob(str(directory / pattern)))
        if not found:
            sys.exit(f"error: {directory / MANIFEST}: no simulation matches {pattern!r}")
        sims += [s for s in found if s not in sims]
    return sims


def run_sim(monica: list[str], sim: Path, out: Path, env: dict) -> dict:
    # Its own working directory, as some simulations write further files
    # (e.g. sim-min-ic.json's second crop output) relative to it.
    shutil.rmtree(out, ignore_errors=True)
    out.mkdir(parents=True)
    start = time.perf_counter()
    with open(out / "monica.log", "w") as log:
        rc = subprocess.call([*monica, "-o", str(out / "out.csv"), str(sim)],
                             cwd=out, env=env, stdout=log, stderr=subprocess.STDOUT)
    return {"returncode": rc, "seconds": round(time.perf_counter() - start, 3)}


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    p.add_argument("--monica", required=True,
                   help="monica-run executable, or a wrapper taking its arguments")
    p.add_argument("--out", required=True, type=Path, help="result directory")
    p.add_argument("--engine", help="label for this build in reports, e.g. linux-nix "
                                    "(default: <os>-<machine>)")
    p.add_argument("--params", type=Path, default=ROOT / "monica-parameters",
                   help="monica-parameters directory (default: the submodule)")
    p.add_argument("--suites", type=Path, default=TESTS / "suites.toml")
    p.add_argument("--only", nargs="+", metavar="SUITE", help="run just these suites")
    p.add_argument("--use", action="append", default=[], metavar="SUITE=DIR",
                   help="use a local directory instead of the suite's pinned "
                        "source, e.g. to try out scenario changes")
    p.add_argument("--cache", type=Path, default=TESTS / ".suites",
                   help="where scenario repositories are checked out")
    p.add_argument("-j", "--jobs", type=int, default=os.cpu_count())
    a = p.parse_args()

    # an executable path is resolved, a command on PATH is used as is
    monica = [str(Path(a.monica).resolve()) if Path(a.monica).exists() else a.monica]
    params = a.params.resolve()
    if not (params / "general").is_dir():
        sys.exit(f"error: {params} doesn't look like monica-parameters "
                 "(git submodule update --init?)")
    overrides = {}
    for use in a.use:
        name, _, directory = use.partition("=")
        overrides[name] = Path(directory).resolve()

    suites = tomllib.loads(a.suites.read_text(encoding="utf-8"))["suite"]
    if a.only:
        unknown = set(a.only) - {s["name"] for s in suites}
        if unknown:
            sys.exit(f"error: unknown suite(s): {', '.join(sorted(unknown))}")
        suites = [s for s in suites if s["name"] in a.only]

    out = a.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    env = {**os.environ, "MONICA_PARAMETERS": str(params)}
    try:
        version = subprocess.run([*monica, "--version"], capture_output=True, text=True,
                                 env=env).stdout.strip()
    except OSError as e:
        sys.exit(f"error: can't run --monica {a.monica}: {e}")
    meta = {
        "engine": a.engine or f"{platform.system()}-{platform.machine()}".lower(),
        "monica": {"command": monica, "version": version, "source": git_state(ROOT)},
        "platform": {"system": platform.system(), "release": platform.release(),
                     "machine": platform.machine(), "python": platform.python_version()},
        "parameters": {"path": str(params), **(git_state(params) or {})},
        "suites": [],
    }
    print(f"==> {version} as {meta['engine']}, parameters {params}", flush=True)

    jobs = []  # (suite metadata, sim name, future)
    with concurrent.futures.ThreadPoolExecutor(a.jobs) as pool:
        for suite in suites:
            directory = suite_dir(suite, overrides, a.cache.resolve())
            if not (directory / MANIFEST).is_file():
                sys.exit(f"error: suite {suite['name']}: no {MANIFEST} in {directory}")
            manifest = tomllib.loads((directory / MANIFEST).read_text(encoding="utf-8"))
            entry = {"name": suite["name"], "source": str(directory),
                     **(git_state(directory) or {}),
                     "compare": manifest.get("compare", {}), "sims": {}}
            if suite["name"] in overrides:
                entry["override"] = True
            meta["suites"].append(entry)
            for sim in sim_files(directory, manifest["sims"]):
                name = sim.relative_to(directory).with_suffix("").as_posix()
                jobs.append((entry, name, pool.submit(
                    run_sim, monica, sim, out / suite["name"] / name,
                    {**env, "MONICA_TEST_SUITE": str(directory)})))

        failed = []
        for entry, name, future in jobs:
            result = entry["sims"][name] = future.result()
            ok = result["returncode"] == 0
            print(f"  {'ok  ' if ok else 'FAIL'} {entry['name']}/{name} "
                  f"({result['seconds']:.1f}s)", flush=True)
            if not ok:
                failed.append(f"{entry['name']}/{name}")

    (out / "metadata.json").write_text(json.dumps(meta, indent=2) + "\n", encoding="utf-8")
    print(f"==> {len(jobs) - len(failed)}/{len(jobs)} simulations ok, results in {out}")
    if failed:
        sys.exit("error: failed (see their monica.log): " + ", ".join(failed))


if __name__ == "__main__":
    main()
