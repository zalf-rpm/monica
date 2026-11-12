#!/usr/bin/env python3
"""Compare simulated with measured data for the suites that have measurements.

    python tests/validate.py results/linux --out validation
    python tests/validate.py results/linux --out validation --baseline old/validation.json

Reads a result directory written by tests/run.py. For each suite whose
monica-test.toml has [[observed]] entries, it matches the measurements to the
simulation outputs and calculates goodness-of-fit statistics: n, bias, MAE,
RMSE, nRMSE, R², Willmott's index of agreement d and the Nash-Sutcliffe
efficiency. Writes to --out:

  report.md        statistics as Markdown (also appended to $GITHUB_STEP_SUMMARY
                   in GitHub Actions unless --summary '' is given)
  report.html      the same plus plots (simulation as lines, measurements as
                   dots), one self-contained file
  validation.json  the statistics, for --baseline in a later run

With --baseline (validation.json of an earlier run, e.g. the previous master
build) the tables also show how the RMSE changed. The statistics are only
information: the exit code is 0 unless something can't be read.

An [[observed]] entry in monica-test.toml:

  [[observed]]
  name = "N leaching, cumulative"  # title in the report, key for --baseline
  file = "nleach_measured.csv"     # the measurements, CSV with a header row
  sim = "*/{site}/sim-min"         # simulation of a row: {column} from the row,
                                   #   glob over the simulation names, or instead
  # sim-map = "sim-map.csv"        #   a CSV with a "sim" column and columns
                                   #   matched against the measurement row
  date = "date"                    # column with the date (YYYY-MM-DD), or
  # key = "year"                   #   a column matched against output.key
  value = "cumulative"             # measured value
  sd = "sd"                        # optional: standard deviation (error bars)
  unit = "kg N/ha"
  group = "site"                   # optional: statistics per value of this column
  label = "{site}"                 # optional: plot title per simulation
  output = { section = "daily", column = "NLeach", accumulate = "since-first" }

output: section and column of MONICA's CSV output; optional
  key        column matched against the measurement's key (e.g. "harv-year")
  scale      factor applied to the simulated values (e.g. 100 for kg/kg -> %)
  accumulate "since-first": sum of the daily values after the simulation's
             first measurement date up to the measurement date (cumulative
             measurements); "since-previous": sum since the previous
             measurement date (measurements of a period)
"""
from __future__ import annotations

import argparse
import base64
import csv
import fnmatch
import html
import io
import json
import math
import os
import re
import sys
import tomllib
from dataclasses import dataclass, field
from pathlib import Path

TESTS = Path(__file__).resolve().parent
sys.path.insert(0, str(TESTS))
import compare  # noqa: E402  (read_sections)
import run  # noqa: E402  (suite checkouts)

SIM_COLOR, OBS_COLOR = "#2a78d6", "#eb6834"
STATS = ["n", "obs_mean", "sim_mean", "bias", "mae", "rmse", "nrmse", "r2", "d", "nse"]


# --- statistics -------------------------------------------------------------

def statistics(obs: list[float], sim: list[float]) -> dict:
    n = len(obs)
    if not n:
        return {"n": 0}
    mo, ms = sum(obs) / n, sum(sim) / n
    err = [s - o for o, s in zip(obs, sim)]
    sse = sum(e * e for e in err)
    var_o = sum((o - mo) ** 2 for o in obs)
    var_s = sum((s - ms) ** 2 for s in sim)
    cov = sum((o - mo) * (s - ms) for o, s in zip(obs, sim))
    pot = sum((abs(s - mo) + abs(o - mo)) ** 2 for o, s in zip(obs, sim))
    rmse = math.sqrt(sse / n)
    return {
        "n": n,
        "obs_mean": mo,
        "sim_mean": ms,
        "bias": ms - mo,
        "mae": sum(abs(e) for e in err) / n,
        "rmse": rmse,
        "nrmse": 100 * rmse / abs(mo) if mo else None,
        "r2": cov * cov / (var_o * var_s) if var_o and var_s else None,
        "d": 1 - sse / pot if pot else None,
        "nse": 1 - sse / var_o if var_o else None,
    }


# --- reading ----------------------------------------------------------------

@dataclass
class Point:
    sim: str
    label: str
    group: str
    x: str  # date or key
    obs: float
    sim_value: float
    sd: float | None


@dataclass
class Series:
    """Simulated values of one simulation, as plotted."""
    x: list[str] = field(default_factory=list)
    y: list[float] = field(default_factory=list)


@dataclass
class ObservedSet:
    spec: dict
    points: list[Point] = field(default_factory=list)
    series: dict[str, Series] = field(default_factory=dict)  # by label
    unmatched: list[str] = field(default_factory=list)

    @property
    def name(self) -> str:
        return self.spec["name"]

    @property
    def is_date(self) -> bool:
        return "date" in self.spec


def read_csv(path: Path) -> list[dict]:
    with path.open(encoding="utf-8-sig", newline="") as f:
        return list(csv.DictReader(f))


def to_float(s: str | None) -> float | None:
    try:
        v = float(s)
    except (TypeError, ValueError):
        return None
    return v if math.isfinite(v) else None


class Outputs:
    """Sections of the simulation outputs, read once."""

    def __init__(self, results: Path, suite: str):
        self.dir = results / suite
        self.cache: dict[str, dict] = {}

    def column(self, sim: str, section: str, column: str, key: str) -> list[tuple[str, float]]:
        """(key, value) rows of a column, in output order."""
        sections = self.cache.get(sim)
        if sections is None:
            sections = self.cache[sim] = compare.read_sections(self.dir / sim / "out.csv")
        s = sections.get(section)
        if s is None:
            raise KeyError(f"{sim}: no section {section!r} in the output")
        names = s.split(s.header)
        for wanted in (column, key):
            if wanted not in names:
                raise KeyError(f"{sim} [{section}]: no column {wanted!r} "
                               f"(has {', '.join(names)})")
        ci, ki = names.index(column), names.index(key)
        rows = []
        for line in s.rows:
            cells = s.split(line)
            v = to_float(cells[ci]) if ci < len(cells) else None
            k = cells[ki] if ki < len(cells) else ""
            if v is not None and k:
                rows.append((k, v))
        return rows


def sim_resolver(spec: dict, directory: Path, sims: list[str]):
    """Function from a measurement row to its simulation name (or None)."""
    if "sim-map" in spec:
        mapping = read_csv(directory / spec["sim-map"])
        cols = [c for c in mapping[0] if c != "sim"] if mapping else []
        index = {tuple(m[c].strip() for c in cols): m["sim"] for m in mapping}

        def from_map(row: dict) -> str | None:
            return index.get(tuple((row.get(c) or "").strip() for c in cols))
        return from_map

    def from_pattern(row: dict) -> str | None:
        pattern = spec["sim"].format(**row)
        found = [s for s in sims if fnmatch.fnmatchcase(s, pattern)]
        return found[0] if len(found) == 1 else None
    return from_pattern


def accumulate(daily: list[tuple[str, float]], dates: list[str], mode: str
               ) -> tuple[dict[str, float], Series]:
    """Simulated sums at the measurement dates and the cumulative series
    from the first measurement date on."""
    dates = sorted(set(dates))
    at: dict[str, float] = {}
    series = Series()
    total, last_total = 0.0, 0.0
    wanted = iter(dates)
    next_date = next(wanted, None)
    for date, v in daily:
        if date <= dates[0]:
            if date == dates[0]:
                series.x.append(date)
                series.y.append(0.0)
            continue
        if date > dates[-1]:
            break
        total += v
        series.x.append(date)
        series.y.append(total)
        while next_date is not None and next_date <= date:
            if next_date == date:
                at[date] = total if mode == "since-first" else total - last_total
                last_total = total
            next_date = next(wanted, None)
    if mode == "since-previous":
        # a period's own sum is the plotted quantity, the cumulative line
        # would not fit the dots
        series = Series([dates[0], *at], [0.0, *(at[d] for d in at)])
    return at, series


def collect(spec: dict, directory: Path, outputs: Outputs, sims: list[str]) -> ObservedSet:
    result = ObservedSet(spec)
    rows = read_csv(directory / spec["file"])
    resolve = sim_resolver(spec, directory, sims)
    out = spec["output"]
    scale = out.get("scale", 1.0)
    xcol = spec.get("date") or spec["key"]
    okey = "Date" if "date" in spec else out["key"]
    mode = out.get("accumulate")
    label_t, group_c = spec.get("label"), spec.get("group")

    by_sim: dict[str, list[dict]] = {}
    for row in rows:
        if to_float(row.get(spec["value"])) is None:
            continue
        sim = resolve(row)
        if sim is None or sim not in sims:
            result.unmatched.append(f"no simulation for {dict(row)}")
            continue
        by_sim.setdefault(sim, []).append(row)

    for sim, sim_rows in sorted(by_sim.items()):
        label = label_t.format(**sim_rows[0]) if label_t else sim
        data = outputs.column(sim, out["section"], out["column"], okey)
        if mode:
            values, series = accumulate(data, [r[xcol].strip() for r in sim_rows], mode)
            series.y = [y * scale for y in series.y]
        else:
            values = dict(data)  # a key appearing twice: the last one
            series = Series([k for k, _ in data], [v * scale for _, v in data])
        result.series[label] = series
        first = min(r[xcol].strip() for r in sim_rows)
        for row in sim_rows:
            x = row[xcol].strip()
            if mode and x == first:
                continue  # the baseline of the sums, 0 by definition
            if x not in values:
                result.unmatched.append(f"{label}: no simulated {out['column']} at {x}")
                continue
            result.points.append(Point(
                sim, label, row[group_c].strip() if group_c else "", x,
                to_float(row[spec["value"]]), values[x] * scale,
                to_float(row.get(spec["sd"])) if "sd" in spec else None))
    return result


# --- reporting --------------------------------------------------------------

def natural(key: str) -> list:
    """GE2 before GE10."""
    return [int(t) if t.isdigit() else t for t in re.split(r"(\d+)", key)]


def set_statistics(s: ObservedSet) -> dict:
    def of(points: list[Point]) -> dict:
        return statistics([p.obs for p in points], [p.sim_value for p in points])
    groups: dict[str, list[Point]] = {}
    sims: dict[str, list[Point]] = {}
    for p in s.points:
        groups.setdefault(p.group, []).append(p)
        sims.setdefault(p.label, []).append(p)
    return {"unit": s.spec.get("unit", ""),
            "group": s.spec.get("group", ""),
            "all": of(s.points),
            "groups": {g: of(groups[g]) for g in sorted(groups, key=natural)} if s.spec.get("group") else {},
            "sims": {k: of(sims[k]) for k in sorted(sims, key=natural)},
            "unmatched": len(s.unmatched)}


def fmt(v, digits: int = 3) -> str:
    if v is None:
        return "-"
    if isinstance(v, int):
        return str(v)
    if v == 0:
        return "0"
    mag = math.floor(math.log10(abs(v)))
    return f"{v:.{max(0, digits - 1 - mag)}f}" if -3 <= mag < digits + 2 else f"{v:.{digits}g}"


def delta(stats: dict, base: dict | None) -> str:
    if not base or stats.get("rmse") is None or base.get("rmse") is None:
        return "-"
    d = stats["rmse"] - base["rmse"]
    if abs(d) <= 1e-9 * max(1.0, abs(base["rmse"])):
        return "±0"
    return f"{'+' if d > 0 else '−'}{fmt(abs(d))} ({'worse' if d > 0 else 'better'})"


def stats_rows(first: str, items: list[tuple[str, dict]], base: dict | None,
               base_key: str) -> list[str]:
    head = (f"| {first} | n | obs mean | sim mean | bias | MAE | RMSE | nRMSE % "
            f"| R² | d | NSE |{' Δ RMSE |' if base is not None else ''}")
    sep = "|---|" + "--:|" * (len(STATS) + (base is not None))
    rows = [head, sep]
    for name, st in items:
        cells = [fmt(st.get(k)) for k in STATS]
        b = None
        if base is not None:
            b = base.get("all") if name == "**all**" else base.get(base_key, {}).get(name)
        rows.append(f"| {name} | " + " | ".join(cells) +
                    (f" | {delta(st, b)} |" if base is not None else " |"))
    return rows


def inputs(meta: dict, validated: dict) -> dict[str, str | None]:
    """Commits of what the statistics depend on besides MONICA: the parameters
    and the suites with measurements."""
    out = {"monica-parameters": meta.get("parameters", {}).get("commit")}
    out.update({f"suite {s['name']}": s.get("commit") for s in meta["suites"]
                if s["name"] in validated})
    return out


def markdown(suites: list[tuple[str, list[ObservedSet]]], stats: dict, baseline: dict | None,
             meta: dict) -> str:
    out = ["## Simulated vs measured", ""]
    src = meta["monica"].get("source") or {}
    line = (f"{meta['monica']['version']} ({meta['engine']}"
            f"{', ' + src['commit'][:10] if src.get('commit') else ''})")
    if baseline is not None:
        b = baseline.get("monica", {})
        line += (f"; Δ RMSE vs {b.get('version', '?')} "
                 f"({(b.get('commit') or '?')[:10]}): negative is better")
    out.append(line)
    if baseline is not None and "inputs" in baseline:
        changed = [f"{k} {(baseline['inputs'].get(k) or '-')[:10]} → {(v or '-')[:10]}"
                   for k, v in inputs(meta, stats).items() if v != baseline["inputs"].get(k)]
        if changed:
            out += ["", "> **Note:** different inputs than the baseline, so changes may come "
                    "from them: " + "; ".join(changed)]
    out += ["", "Bias = mean(sim − obs); nRMSE = RMSE / obs mean; d = Willmott's index "
            "of agreement; NSE = Nash-Sutcliffe efficiency (1 is perfect, < 0 is worse "
            "than the measured mean).", ""]
    if not suites:
        out.append("No suite has measurements (`[[observed]]` in its monica-test.toml).")
    for suite, sets in suites:
        for s in sets:
            st = stats[suite][s.name]
            base = (baseline or {}).get("suites", {}).get(suite, {}).get(s.name)
            if baseline is not None and base is None:
                base = {}
            unit = f" ({st['unit']})" if st["unit"] else ""
            out += [f"### {suite}: {s.name}{unit}", ""]
            items = [("**all**", st["all"])]
            items += list(st["groups"].items())
            out += stats_rows(st["group"] or "", items, base, "groups")
            if s.unmatched:
                out += ["", f"{len(s.unmatched)} measurements without a simulated value "
                        "(see report.html)."]
            out.append("")
    return "\n".join(out) + "\n"


def figures(s: ObservedSet) -> list[tuple[str, str]]:
    """(title, base64 PNG) of the time series and the scatter plot."""
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.dates as mdates
    import matplotlib.pyplot as plt
    import numpy as np

    plt.rcParams.update({"font.size": 8, "axes.edgecolor": "#888", "axes.linewidth": 0.6,
                         "xtick.color": "#555", "ytick.color": "#555",
                         "axes.titlesize": 9, "figure.facecolor": "#fcfcfb",
                         "axes.facecolor": "#fcfcfb"})
    unit = s.spec.get("unit", "")
    ylabel = f"{s.name} ({unit})" if unit else s.name

    def x_of(values: list[str]):
        if s.is_date:
            return list(mdates.date2num(np.array(values, dtype="datetime64[D]")))
        return [to_float(v) for v in values]

    def png(fig) -> str:
        buf = io.BytesIO()
        fig.savefig(buf, format="png", dpi=110)
        plt.close(fig)
        return base64.b64encode(buf.getvalue()).decode()

    result = []
    labels = sorted(s.series, key=natural)
    by_label: dict[str, list[Point]] = {}
    for p in s.points:
        by_label.setdefault(p.label, []).append(p)
    ncols = min(len(labels), 4 if len(labels) <= 12 else 6)
    nrows = math.ceil(len(labels) / ncols)
    fig, axes = plt.subplots(nrows, ncols, figsize=(3.2 * ncols, 2.3 * nrows + 0.6),
                             squeeze=False, layout="constrained")
    accumulated = s.spec["output"].get("accumulate")
    for ax, label in zip(axes.flat, labels):
        ser = by_label.get(label, [])
        line = s.series[label]
        if accumulated == "since-previous":
            # each value is the sum over the period ending at its date
            ax.plot(x_of(line.x), line.y, "-", lw=1.0, color=SIM_COLOR, label="simulated",
                    drawstyle="steps-pre")
        elif s.is_date:
            ax.plot(x_of(line.x), line.y, "-", lw=1.2, color=SIM_COLOR, label="simulated")
        else:
            ax.plot(x_of(line.x), line.y, "-", lw=0.8, color=SIM_COLOR, label="simulated",
                    marker="s", ms=2.5)
        if ser:
            xs, ys = x_of([p.x for p in ser]), [p.obs for p in ser]
            sds = [p.sd or 0.0 for p in ser]
            ax.errorbar(xs, ys, yerr=sds if any(sds) else None, fmt="o", ms=4,
                        color=OBS_COLOR, mec="#fcfcfb", mew=0.6, elinewidth=0.7,
                        label="measured")
            if not accumulated and s.is_date:
                # daily outputs run over the whole simulation, zoom to the measurements
                pad = 0.03 * ((max(xs) - min(xs)) or 30)
                ax.set_xlim(min(xs) - pad, max(xs) + pad)
        ax.set_title(label)
        ax.grid(alpha=0.25, lw=0.5)
        if s.is_date:
            loc = mdates.AutoDateLocator(minticks=3, maxticks=5)
            ax.xaxis.set_major_locator(loc)
            ax.xaxis.set_major_formatter(mdates.ConciseDateFormatter(loc))
    for ax in axes.flat[len(labels):]:
        ax.set_visible(False)
    handles, names = axes.flat[0].get_legend_handles_labels()
    fig.legend(handles, names, loc="outside upper right", ncols=2, frameon=False)
    fig.supylabel(ylabel)
    result.append(("Time series per simulation", png(fig)))

    groups: dict[str, list[Point]] = {}
    for p in s.points:
        groups.setdefault(p.group or "all", []).append(p)
    if s.points:
        ncols = min(len(groups), 4 if len(groups) <= 8 else 6)
        nrows = math.ceil(len(groups) / ncols)
        fig, axes = plt.subplots(nrows, ncols, figsize=(2.9 * ncols, 2.9 * nrows + 0.5),
                                 squeeze=False, layout="constrained")
        for ax, (g, ps) in zip(axes.flat, sorted(groups.items(), key=lambda kv: natural(kv[0]))):
            # each panel its own range, so groups with small values stay readable
            values = [v for p in ps for v in (p.obs, p.sim_value)]
            lo, hi = min(values), max(values)
            pad = (hi - lo) * 0.05 or 1
            lim = (lo - pad, hi + pad)
            ax.plot(lim, lim, "--", color="#666", lw=0.8)
            ax.plot([p.obs for p in ps], [p.sim_value for p in ps], "o", ms=3.5,
                    alpha=0.6, color=SIM_COLOR, mec="#fcfcfb", mew=0.4)
            st = statistics([p.obs for p in ps], [p.sim_value for p in ps])
            ax.set_title(f"{g} (n={st['n']}, RMSE {fmt(st['rmse'])})")
            ax.set(xlim=lim, ylim=lim, aspect="equal")
            ax.grid(alpha=0.25, lw=0.5)
        for ax in axes.flat[len(groups):]:
            ax.set_visible(False)
        fig.supxlabel(f"measured ({unit})" if unit else "measured")
        fig.supylabel(f"simulated ({unit})" if unit else "simulated")
        result.append(("Simulated vs measured (dashed: 1:1)", png(fig)))
    return result


def html_report(md_meta: str, suites: list[tuple[str, list[ObservedSet]]], stats: dict,
                baseline: dict | None, plots: bool) -> str:
    def table(first: str, items: list[tuple[str, dict]], base: dict | None, key: str) -> str:
        rows = stats_rows(first, items, base, key)
        head = [c.strip() for c in rows[0].strip("|").split("|")]
        body = ["<tr>" + "".join(
            f"<td>{html.escape(c.strip().strip('*'))}</td>"
            for c in r.strip("|").split("|")) + "</tr>" for r in rows[2:]]
        return ("<table><thead><tr>" + "".join(f"<th>{html.escape(h)}</th>" for h in head)
                + "</tr></thead><tbody>" + "".join(body) + "</tbody></table>")

    parts = ["<!doctype html><html><head><meta charset='utf-8'>",
             "<title>MONICA: simulated vs measured</title><style>",
             "body{font:14px system-ui,sans-serif;margin:24px;background:#fcfcfb;color:#222}"
             "table{border-collapse:collapse;margin:8px 0 16px}"
             "td,th{padding:3px 8px;border-bottom:1px solid #ddd;text-align:right}"
             "td:first-child,th:first-child{text-align:left}"
             "th{color:#555;font-weight:600}img{max-width:100%}"
             "h2{margin-top:40px;border-top:1px solid #ccc;padding-top:16px}"
             "details{margin:8px 0}.note{color:#555}",
             "</style></head><body><h1>MONICA: simulated vs measured</h1>",
             f"<p class='note'>{html.escape(md_meta)}</p>",
             "<p class='note'>Blue: simulated, orange dots: measured (error bars: sd). "
             "Bias = mean(sim − obs); nRMSE = RMSE / obs mean; d = Willmott's index of "
             "agreement; NSE = Nash-Sutcliffe efficiency.</p>"]
    for suite, sets in suites:
        for s in sets:
            st = stats[suite][s.name]
            base = (baseline or {}).get("suites", {}).get(suite, {}).get(s.name)
            if baseline is not None and base is None:
                base = {}
            unit = f" ({st['unit']})" if st["unit"] else ""
            parts.append(f"<h2>{html.escape(suite)}: {html.escape(s.name)}{html.escape(unit)}</h2>")
            parts.append(table(st["group"] or "", [("all", st["all"]), *st["groups"].items()],
                               base, "groups"))
            if plots:
                for title, data in figures(s):
                    parts.append(f"<h3>{html.escape(title)}</h3>"
                                 f"<img alt='{html.escape(title)}' src='data:image/png;base64,{data}'>")
            parts.append("<details><summary>Statistics per simulation</summary>"
                         + table("simulation", list(st["sims"].items()), base, "sims")
                         + "</details>")
            if s.unmatched:
                parts.append(f"<details><summary>{len(s.unmatched)} measurements without a "
                             "simulated value</summary><ul>"
                             + "".join(f"<li>{html.escape(u)}</li>" for u in s.unmatched[:500])
                             + "</ul></details>")
    parts.append("</body></html>")
    return "\n".join(parts)


# --- main -------------------------------------------------------------------

def suite_directory(entry: dict, defs: dict[str, dict], overrides: dict[str, Path],
                    cache: Path) -> Path | None:
    """The suite's files at the commit the results were made with."""
    name = entry["name"]
    if name in overrides:
        return overrides[name]
    suite = defs.get(name)
    if suite is None:
        source = Path(entry.get("source", ""))
        return source if source.is_dir() else None
    if "git" in suite and entry.get("commit"):
        suite = {**suite, "rev": entry["commit"]}
    return run.suite_dir(suite, {}, cache)


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    p.add_argument("results", type=Path, help="result directory written by tests/run.py")
    p.add_argument("--out", type=Path, default=Path("validation"), help="report directory")
    p.add_argument("--baseline", type=Path, help="validation.json of an earlier run")
    p.add_argument("--summary", default=os.environ.get("GITHUB_STEP_SUMMARY"),
                   help="also append report.md to this file (default: $GITHUB_STEP_SUMMARY)")
    p.add_argument("--no-plots", action="store_true", help="no plots (no matplotlib needed)")
    p.add_argument("--suites", type=Path, default=TESTS / "suites.toml")
    p.add_argument("--use", action="append", default=[], metavar="SUITE=DIR",
                   help="read a suite's measurements and manifest from a local directory")
    p.add_argument("--cache", type=Path, default=TESTS / ".suites")
    a = p.parse_args()

    meta = json.loads((a.results / "metadata.json").read_text(encoding="utf-8"))
    defs = {s["name"]: s for s in
            tomllib.loads(a.suites.read_text(encoding="utf-8"))["suite"]}
    overrides = {n: Path(d).resolve() for n, _, d in (u.partition("=") for u in a.use)}
    baseline = None
    if a.baseline:
        if a.baseline.is_file():
            baseline = json.loads(a.baseline.read_text(encoding="utf-8"))
        else:
            print(f"note: no baseline {a.baseline}, Δ RMSE not shown", file=sys.stderr)

    suites: list[tuple[str, list[ObservedSet]]] = []
    stats: dict = {}
    for entry in meta["suites"]:
        directory = suite_directory(entry, defs, overrides, a.cache.resolve())
        if directory is None:
            print(f"note: suite {entry['name']}: source not found, skipped", file=sys.stderr)
            continue
        if not (directory / run.MANIFEST).is_file():
            print(f"note: suite {entry['name']}: no {directory / run.MANIFEST}, skipped",
                  file=sys.stderr)
            continue
        manifest = tomllib.loads((directory / run.MANIFEST).read_text(encoding="utf-8"))
        specs = manifest.get("observed", [])
        if not specs:
            continue
        ok = [sim for sim, r in entry["sims"].items() if r["returncode"] == 0]
        outputs = Outputs(a.results, entry["name"])
        sets = []
        for spec in specs:
            try:
                sets.append(collect(spec, directory, outputs, ok))
            except (KeyError, OSError, ValueError) as e:
                sys.exit(f"error: suite {entry['name']}, observed {spec.get('name')!r}: {e}")
        suites.append((entry["name"], sets))
        stats[entry["name"]] = {s.name: set_statistics(s) for s in sets}

    src = meta["monica"].get("source") or {}
    data = {"monica": {"version": meta["monica"]["version"], "commit": src.get("commit")},
            "engine": meta["engine"], "suites": stats, "inputs": inputs(meta, stats)}
    text = markdown(suites, stats, baseline, meta)
    a.out.mkdir(parents=True, exist_ok=True)
    (a.out / "validation.json").write_text(json.dumps(data, indent=1) + "\n", encoding="utf-8")
    (a.out / "report.md").write_text(text, encoding="utf-8")
    header = text.split("\n")[2]
    (a.out / "report.html").write_text(
        html_report(header, suites, stats, baseline, not a.no_plots), encoding="utf-8")
    print(text)
    if a.summary:
        with open(a.summary, "a", encoding="utf-8") as f:
            f.write(text + "\n")


if __name__ == "__main__":
    main()
