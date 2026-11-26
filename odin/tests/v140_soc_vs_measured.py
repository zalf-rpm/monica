#!/usr/bin/env python3
"""Simulated topsoil SOC of the Muencheberg V140 suite against the measured
values, for one or more result directories written by ../monica/tests/run.py.

    pixi run python tests/v140_soc_vs_measured.py ../build/phase-i/off ../build/phase-i/on

For model changes that can't be checked by comparing outputs exactly (plan
phase I): besides how far a change moves the results (../monica/tests/
deviation.py), whether it moves them towards or away from the measurements.
SOC is the measured quantity the suite carries; the N cycle reaches it through
immobilisation, which limits decomposition when mineral N is short.

Each simulation's site file site_<n>.json is plot 12000 + n of
soil_corg_measured.csv; the daily SOC_0_30_kgkg (0-30 cm mean, kg C kg-1) on
the sampling date is compared with obs_corg_% / 100. Prints a Markdown table
per run: number of pairs, mean bias, RMSE, Pearson r - overall and per
treatment.
"""
from __future__ import annotations

import argparse
import csv
import json
import math
import re
import statistics
import sys
from collections import defaultdict
from pathlib import Path

ODIN = Path(__file__).resolve().parent.parent
MONICA_TESTS = ODIN.parent.parent / "monica" / "tests"


def read_measured(suite: Path) -> dict[int, list[tuple[str, int, float]]]:
    """plot -> [(sampling date, treatment, SOC kg C kg-1)]"""
    by_plot: dict[int, list[tuple[str, int, float]]] = defaultdict(list)
    with open(suite / "soil_corg_measured.csv", newline="") as f:
        for row in csv.DictReader(f):
            by_plot[int(row["plot_no"])].append(
                (row["sampling_date"], int(row["treatment"]), float(row["obs_corg_%"]) / 100.0))
    return by_plot


def plot_of(sim_json: Path) -> int:
    site = json.loads(sim_json.read_text())["site.json"]
    m = re.search(r"site_(\d+)\.json$", site)
    if not m:
        raise SystemExit(f"{sim_json}: no site_<n>.json in {site!r}")
    return 12000 + int(m.group(1))


def simulated_soc(out_csv: Path, read_sections) -> dict[str, float]:
    """Date -> SOC_0_30_kgkg of the daily section"""
    sections = read_sections(out_csv)
    daily = sections.get("daily")
    if daily is None:
        return {}
    cols = daily.split(daily.header)
    i_date, i_soc = cols.index("Date"), cols.index("SOC_0_30_kgkg")
    res = {}
    for line in daily.rows:
        v = daily.split(line)
        try:
            res[v[i_date]] = float(v[i_soc])
        except ValueError:
            pass
    return res


def stats(pairs: list[tuple[float, float]]) -> str:
    n = len(pairs)
    if n == 0:
        return "| 0 | | | |"
    diffs = [s - o for s, o in pairs]
    bias = statistics.fmean(diffs)
    rmse = math.sqrt(statistics.fmean(d * d for d in diffs))
    r = statistics.correlation([s for s, _ in pairs], [o for _, o in pairs]) if n > 2 else float("nan")
    # in % SOC (as measured) for readability
    return f"| {n} | {bias * 100:+.4f} | {rmse * 100:.4f} | {r:.3f} |"


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("runs", nargs="+", type=Path, help="result directories of tests/run.py")
    p.add_argument("--suite", type=Path, default=MONICA_TESTS / ".suites" / "muencheberg_v140",
                   help="the muencheberg_v140 checkout (default: run.py's cache)")
    a = p.parse_args()

    sys.path.insert(0, str(MONICA_TESTS))
    from compare import read_sections  # noqa: E402  (../monica/tests/compare.py)

    measured = read_measured(a.suite)
    print("SOC 0-30 cm, simulated - measured, in % SOC (measured mean "
          f"{statistics.fmean(v for obs in measured.values() for _, _, v in obs) * 100:.3f} %)\n")
    for run in a.runs:
        overall: list[tuple[float, float]] = []
        per_treatment: dict[int, list[tuple[float, float]]] = defaultdict(list)
        for sim_dir in sorted((run / "muencheberg_v140" / "sim_files").iterdir()):
            plot = plot_of(a.suite / "sim_files" / f"{sim_dir.name}.json")
            soc = simulated_soc(sim_dir / "out.csv", read_sections)
            for date, treatment, obs in measured.get(plot, []):
                if date in soc:
                    overall.append((soc[date], obs))
                    per_treatment[treatment].append((soc[date], obs))
        print(f"### {run}\n")
        print("| treatment | n | bias | RMSE | r |\n|---|---:|---:|---:|---:|")
        print("| all " + stats(overall))
        for t in sorted(per_treatment):
            print(f"| {t} " + stats(per_treatment[t]))
        print()


if __name__ == "__main__":
    main()
