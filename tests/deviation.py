#!/usr/bin/env python3
"""Show how much two result directories written by tests/run.py differ.

    python tests/deviation.py results/master results/fix

compare.py answers whether two runs differ; this answers by how much, for
changes that are meant to change results (a bug fix, a parameter change, a
new model version). Comparing daily values row by row mostly measures timing:
a crop reaching maturity a day earlier shows its whole biomass as the
difference on that day. So the sections are compared in two ways:

- daily time series (a Date column with one row per day, e.g. "daily"): per
  simulation the column's mean over the run (equivalent to its total) and its
  end value (the meaningful one for cumulative columns and pools)
- all other sections (one row per harvest, year, month, event: "crop",
  "yearly", ...): row by row, grouped by crop where the section has a Crop
  column

Changes are relative to A, in percent. Values that are 0 in A can't be
expressed relatively; they are counted separately ("from 0"). Columns of
layers (Mois_1, Mois_2, ...) are pooled into one row (Mois_*) unless
--per-layer is given. Prints a Markdown report - also appended to
$GITHUB_STEP_SUMMARY in GitHub Actions. Always exits with 0.
"""
from __future__ import annotations

import argparse
import datetime
import json
import math
import os
import re
import statistics
from dataclasses import dataclass, field
from pathlib import Path

from compare import Section, inputs_table, keyed_columns, read_sections, to_float

# columns that identify a row of an event section, the first one present wins
ROW_KEYS = ("Date", "harvest", "harv-year", "Year", "CM-count", "crop-counter")
LAYER = re.compile(r"^(.+)_(\d+)$")


@dataclass
class Changes:
    """Relative changes of one column (in one group), collected over rows or
    simulations."""
    rel: list[float] = field(default_factory=list)
    sum_a: float = 0.0
    sum_b: float = 0.0
    from_zero: int = 0  # A is 0, B isn't
    largest: tuple[float, str] = (0.0, "")  # relative change, where

    def add(self, a: float, b: float, where: str) -> None:
        self.sum_a += a
        self.sum_b += b
        if a == b:
            self.rel.append(0.0)
        elif a == 0:
            self.from_zero += 1
        else:
            r = (b - a) / abs(a)
            self.rel.append(r)
            if abs(r) > abs(self.largest[0]):
                self.largest = (r, where)

    @property
    def count(self) -> int:
        return len(self.rel) + self.from_zero

    @property
    def changed(self) -> int:
        return sum(r != 0 for r in self.rel) + self.from_zero

    @property
    def mean_abs(self) -> float:
        return statistics.fmean(abs(r) for r in self.rel) if self.rel else 0.0


@dataclass
class Report:
    events: dict[tuple[str, str, str, str], Changes] = field(default_factory=dict)
    means: dict[tuple[str, str, str], Changes] = field(default_factory=dict)
    ends: dict[tuple[str, str, str], Changes] = field(default_factory=dict)
    notes: list[str] = field(default_factory=list)
    files: int = 0
    ignored: list[str] = field(default_factory=list)


def column_label(name: str, occurrence: int, per_layer: bool) -> str:
    """Pools layer columns (Mois_3 -> Mois_*); a name written more than once
    (SnowD, SnowD) is told apart by its occurrence."""
    if not per_layer and (m := LAYER.match(name)):
        name = m.group(1) + "_*"
    return name if occurrence == 0 else f"{name} ({occurrence + 1})"


def is_daily(s: Section, date_col: int | None) -> bool:
    """A Date column with one row per consecutive day."""
    if date_col is None or len(s.rows) < 2:
        return False
    try:
        first = datetime.date.fromisoformat(s.split(s.rows[0])[date_col])
        second = datetime.date.fromisoformat(s.split(s.rows[1])[date_col])
        last = datetime.date.fromisoformat(s.split(s.rows[-1])[date_col])
    except (ValueError, IndexError):
        return False
    return (second - first).days == 1 and (last - first).days == len(s.rows) - 1


def compare_sections(suite: str, where: str, a: Section, b: Section, per_layer: bool,
                     group_by: str, report: Report) -> None:
    label = f"`{where}` [{a.name}]"
    cols_a, cols_b = keyed_columns(a), keyed_columns(b)
    # columns both runs write with the same units/aggregation
    shared = [key for key in cols_a if key in cols_b and cols_a[key][1] == cols_b[key][1]]
    skipped = [n for (n, k) in cols_a if (n, k) not in shared]
    if skipped:
        report.notes.append(f"{label}: not in both runs or different units: "
                            + ", ".join(f"`{n}`" for n in skipped[:10]))
    if len(a.rows) != len(b.rows):
        report.notes.append(f"{label}: not compared, {len(a.rows)} vs {len(b.rows)} rows")
        return
    rows_a = [a.split(r) for r in a.rows]
    rows_b = [b.split(r) for r in b.rows]
    names = a.split(a.header)
    date_col = names.index("Date") if "Date" in names else None

    def column(rows: list[list[str]], i: int) -> list[float | None]:
        return [to_float(r[i]) if i < len(r) else None for r in rows]

    if is_daily(a, date_col):
        for (name, k) in shared:
            va, vb = column(rows_a, cols_a[(name, k)][0]), column(rows_b, cols_b[(name, k)][0])
            if any(v is None or math.isnan(v) for v in va + vb):
                continue  # text (Crop, Date) or gaps
            key = (suite, a.name, column_label(name, k, per_layer))
            report.means.setdefault(key, Changes()).add(
                statistics.fmean(va), statistics.fmean(vb), where)
            report.ends.setdefault(key, Changes()).add(va[-1], vb[-1], where)
        return

    key_col = next((names.index(n) for n in ROW_KEYS if n in names), None)
    group_col = names.index(group_by) if group_by in names else None
    for i, (ra, rb) in enumerate(zip(rows_a, rows_b)):
        row_key = ra[key_col] if key_col is not None and key_col < len(ra) else f"row {i + 1}"
        group = ra[group_col] if group_col is not None and group_col < len(ra) else ""
        for (name, k) in shared:
            ca, cb = cols_a[(name, k)][0], cols_b[(name, k)][0]
            fa = to_float(ra[ca]) if ca < len(ra) else None
            fb = to_float(rb[cb]) if cb < len(rb) else None
            if fa is None or fb is None or math.isnan(fa) or math.isnan(fb) or name in ROW_KEYS:
                continue
            report.events.setdefault((suite, a.name, column_label(name, k, per_layer), group),
                                     Changes()).add(fa, fb, f"{where} {row_key}")


def compare_runs(a: Path, b: Path, meta_a: dict, meta_b: dict, ignore: frozenset[str],
                 per_layer: bool, group_by: str) -> Report:
    report = Report()
    suites_b = {s["name"]: s for s in meta_b["suites"]}
    for suite in meta_a["suites"]:
        other = suites_b.get(suite["name"])
        if other is None:
            report.notes.append(f"suite `{suite['name']}` only in A")
            continue
        for sim, result in suite["sims"].items():
            where = f"{suite['name']}/{sim}"
            result_b = other["sims"].get(sim)
            if where in ignore:
                report.ignored.append(where)
                continue
            if result_b is None or result["returncode"] or result_b["returncode"]:
                report.notes.append(f"`{where}` not compared: missing or failed in a run")
                continue
            dir_a, dir_b = a / suite["name"] / sim, b / suite["name"] / sim
            for f in sorted(p.name for p in dir_a.glob("*.csv")):
                if not (dir_b / f).exists():
                    continue
                report.files += 1
                secs_a, secs_b = read_sections(dir_a / f), read_sections(dir_b / f)
                for s in secs_a.keys() & secs_b.keys():
                    compare_sections(suite["name"], where, secs_a[s], secs_b[s], per_layer,
                                     group_by, report)
    return report


def pct(r: float) -> str:
    return f"{100 * r:+.2f}%"


def change_of_sums(c: Changes) -> str:
    if c.sum_a == 0:
        return "-" if c.sum_b == 0 else "from 0"
    return pct((c.sum_b - c.sum_a) / abs(c.sum_a))


def markdown(meta_a: dict, meta_b: dict, report: Report, top: int, show_all: bool) -> str:
    out = inputs_table(meta_a, meta_b)
    if report.ignored:
        out += [f"Not compared (--ignore): {', '.join(f'`{w}`' for w in report.ignored)}", ""]
    out.append(f"{report.files} output files compared. Changes relative to A.")

    def limited(items: list) -> tuple[list, str]:
        if top and len(items) > top:
            return items[:top], f"\n... and {len(items) - top} more (--top 0 shows all)"
        return items, ""

    def rng(c: Changes) -> str:
        return f"{pct(min(c.rel))} ... {pct(max(c.rel))}" if c.rel else "-"

    out += ["", "*Per harvest, year, event* (sections without daily rows): each value "
            "compared with its counterpart; *mean change* and *mean abs* average the "
            "relative changes of the single values, *total* is the change of their sum.",
            "", "*Daily time series*: per simulation the change of the column's mean over "
            "the run (equivalent to its total) and of its end value; *mean abs* averages "
            "over the simulations."]
    for suite in dict.fromkeys(k[0] for k in [*report.events, *report.means]):
        events = [(k, c) for k, c in report.events.items()
                  if k[0] == suite and (show_all or c.changed)]
        events.sort(key=lambda kc: (-kc[1].mean_abs, -kc[1].changed))
        unchanged = sum(1 for k, c in report.events.items() if k[0] == suite and not c.changed)
        out += ["", f"### {suite}: per harvest, year, event", ""]
        if events:
            shown, more = limited(events)
            out += ["| section | column | crop | values | changed | from 0 | mean change "
                    "| mean abs | range | total | largest at |",
                    "|---|---|---|--:|--:|--:|--:|--:|---|--:|---|"]
            for (_, sec, col, group), c in shown:
                mean = pct(statistics.fmean(c.rel)) if c.rel else "-"
                out.append(f"| {sec} | {col} | {group or '-'} | {c.count} | {c.changed} "
                           f"| {c.from_zero} | {mean} | {100 * c.mean_abs:.2f}% | {rng(c)} "
                           f"| {change_of_sums(c)} | {c.largest[1] or '-'} |")
            out.append(more)
        elif not unchanged:
            out.append("No such sections.")
        if unchanged and not show_all:
            out.append(f"{unchanged} unchanged columns not listed (--all lists them).")

        keys = [k for k in report.means if k[0] == suite]
        changed = [k for k in keys if report.means[k].changed or report.ends[k].changed]
        listed = keys if show_all else changed
        listed.sort(key=lambda k: -max(report.means[k].mean_abs, report.ends[k].mean_abs))
        out += ["", f"### {suite}: daily time series", ""]
        if listed:
            shown, more = limited(listed)
            out += ["| section | column | sims | mean: mean abs | mean: range "
                    "| end: mean abs | end: range | largest at |",
                    "|---|---|--:|--:|---|--:|---|---|"]
            for k in shown:
                m, e = report.means[k], report.ends[k]
                largest = max((m.largest, e.largest), key=lambda x: abs(x[0]))[1] or "-"
                out.append(f"| {k[1]} | {k[2]} | {m.count} | {100 * m.mean_abs:.2f}% "
                           f"| {rng(m)} | {100 * e.mean_abs:.2f}% | {rng(e)} | {largest} |")
            out.append(more)
        elif not keys:
            out.append("No such sections.")
        if len(keys) > len(changed) and not show_all:
            out.append(f"{len(keys) - len(changed)} unchanged columns not listed "
                       "(--all lists them).")

    if report.notes:
        out += ["", "### Not compared", ""] + [f"- {n}" for n in report.notes]
    return "\n".join(out) + "\n"


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    p.add_argument("a", type=Path, help="result directory A (the reference)")
    p.add_argument("b", type=Path, help="result directory B")
    p.add_argument("--top", type=int, default=40,
                   help="rows per table, largest changes first (0: all; default 40)")
    p.add_argument("--all", action="store_true", help="also list unchanged columns")
    p.add_argument("--per-layer", action="store_true",
                   help="one row per layer column (Mois_1, Mois_2, ...) instead of Mois_*")
    p.add_argument("--group-by", default="Crop", metavar="COLUMN",
                   help="group the rows of event sections by this column (default: Crop)")
    p.add_argument("--summary", type=Path, default=os.environ.get("GITHUB_STEP_SUMMARY"),
                   help="also append the report to this file (default: $GITHUB_STEP_SUMMARY)")
    p.add_argument("--ignore", nargs="+", default=[], metavar="SUITE/SIM",
                   help="don't compare these simulations; they are listed in the report")
    a = p.parse_args()

    meta_a = json.loads((a.a / "metadata.json").read_text(encoding="utf-8"))
    meta_b = json.loads((a.b / "metadata.json").read_text(encoding="utf-8"))
    report = compare_runs(a.a, a.b, meta_a, meta_b, frozenset(a.ignore), a.per_layer,
                          a.group_by)
    text = markdown(meta_a, meta_b, report, a.top, a.all)
    print(text)
    if a.summary:
        with open(a.summary, "a", encoding="utf-8") as f:
            f.write(text + "\n")


if __name__ == "__main__":
    main()
