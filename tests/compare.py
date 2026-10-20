#!/usr/bin/env python3
"""Compare two result directories written by tests/run.py.

    python tests/compare.py results/linux results/windows

Every CSV of every simulation is compared section by section ("daily",
"crop", ...): data values numerically with the tolerances from the suite's
monica-test.toml ([compare]; exact by default). Columns are matched by name,
so if two builds write different column sets (one lacks some outputs, say),
that is reported, and the columns both write are still compared value by
value; a column whose units differ is reported but not compared. Prints a
Markdown report - also appended to $GITHUB_STEP_SUMMARY in GitHub Actions -
and exits with 1 if anything differs.
"""
from __future__ import annotations

import argparse
import csv
import json
import math
import os
import sys
from dataclasses import dataclass, field
from pathlib import Path

MAX_TABLE_ROWS = 50


@dataclass
class Section:
    name: str
    header: str
    meta: list[str]  # units and aggregation rows
    rows: list[str]

    @property
    def delimiter(self) -> str:
        return ";" if self.header.count(";") > self.header.count(",") else ","

    def split(self, line: str) -> list[str]:
        return next(csv.reader([line], delimiter=self.delimiter))


def read_sections(path: Path) -> dict[str, Section]:
    """MONICA's CSV output: blocks separated by blank lines, each a quoted
    name, the header, the units and aggregation rows, then the data."""
    sections: dict[str, Section] = {}
    block: list[str] = []
    # surrogateescape: compare whatever bytes a build wrote, valid UTF-8 or not;
    # splitlines() also takes care of Windows' CRLF line ends
    text = path.read_text(encoding="utf-8", errors="surrogateescape")
    for line in [*text.splitlines(), ""]:
        if line.strip():
            block.append(line)
            continue
        if not block:
            continue
        name = f"#{len(sections) + 1}"
        if block[0].startswith('"') and block[0].endswith('"') and len(block[0]) > 1:
            name = block.pop(0)[1:-1]
        header, rest = (block[0], block[1:]) if block else ("", [])
        n = 0
        while n < len(rest) and rest[n].startswith(("[", "m:", "j:")):
            n += 1
        sections[name] = Section(name, header, rest[:n], rest[n:])
        block = []
    return sections


@dataclass
class ColumnDiff:
    count: int = 0
    max_abs: float = 0.0
    max_rel: float = 0.0
    first: tuple[str, str, str] | None = None  # row key, value A, value B


@dataclass
class Report:
    structural: list[str] = field(default_factory=list)
    columns: dict[tuple[str, str, str, str], ColumnDiff] = field(default_factory=dict)
    files: int = 0
    ignored: list[str] = field(default_factory=list)  # simulations skipped on request

    @property
    def identical(self) -> bool:
        return not self.structural and not self.columns


def to_float(s: str) -> float | None:
    try:
        return float(s)
    except ValueError:
        return None


def names_list(names: list[str], limit: int = 10) -> str:
    shown = ", ".join(f"`{n}`" for n in names[:limit])
    return shown + (f" (+{len(names) - limit} more)" if len(names) > limit else "")


def keyed_columns(s: Section) -> dict[tuple[str, int], tuple[int, list[str]]]:
    """(name, occurrence) -> (column index, its units/aggregation cells).
    Columns are matched by name, so two builds writing different column sets
    are still compared on the columns they share."""
    meta = [s.split(m) for m in s.meta]
    out: dict[tuple[str, int], tuple[int, list[str]]] = {}
    seen: dict[str, int] = {}
    for i, name in enumerate(s.split(s.header)):
        seen[name] = seen.get(name, -1) + 1
        out[(name, seen[name])] = (i, [m[i] if i < len(m) else "" for m in meta])
    return out


def compare_sections(where: str, file: str, a: Section, b: Section,
                     tolerances: dict, report: Report) -> None:
    label = f"`{where}` {file} [{a.name}]"
    names_a = a.split(a.header)
    same_layout = a.header == b.header and a.meta == b.meta
    if same_layout:
        columns = None  # positions match: compare cell by cell
    else:
        # Different column sets (e.g. one build lacks some outputs): report
        # that - it still counts as a difference, so the exit code stays 1 -
        # and compare the values of the columns both builds write.
        cols_a, cols_b = keyed_columns(a), keyed_columns(b)
        only_a = [n for (n, k) in cols_a if (n, k) not in cols_b]
        only_b = [n for (n, k) in cols_b if (n, k) not in cols_a]
        shared = [key for key in cols_a if key in cols_b]
        units = [key for key in shared if cols_a[key][1] != cols_b[key][1]]
        if only_a:
            report.structural.append(f"{label}: columns only in A: {names_list(only_a)}")
        if only_b:
            report.structural.append(f"{label}: columns only in B: {names_list(only_b)}")
        if units:
            report.structural.append(f"{label}: units differ, not compared: "
                                     f"{names_list([n for n, _ in units])}")
        if not (only_a or only_b or units):
            report.structural.append(f"{label}: same columns in a different order")
        columns = [(n, cols_a[(n, k)][0], cols_b[(n, k)][0])
                   for n, k in shared if (n, k) not in units]
    if len(a.rows) != len(b.rows):
        report.structural.append(f"{label}: {len(a.rows)} vs {len(b.rows)} rows")
    date = names_a.index("Date") if "Date" in names_a else None
    default = tolerances.get("default", {})
    for i, (row_a, row_b) in enumerate(zip(a.rows, b.rows)):
        if columns is None and row_a == row_b:
            continue
        cells_a, cells_b = a.split(row_a), b.split(row_b)
        key = cells_a[date] if date is not None and date < len(cells_a) else f"row {i + 1}"
        if columns is None:
            # (column name, index in A, index in B); also covers cells
            # beyond the header
            pairs = [(names_a[c] if c < len(names_a) else f"#{c + 1}", c, c)
                     for c in range(max(len(cells_a), len(cells_b)))]
        else:
            pairs = columns
        for column, ca, cb in pairs:
            va = cells_a[ca] if ca < len(cells_a) else ""
            vb = cells_b[cb] if cb < len(cells_b) else ""
            if va == vb:
                continue
            fa, fb = to_float(va), to_float(vb)
            if fa is not None and fb is not None:
                if math.isnan(fa) and math.isnan(fb):
                    continue
                tol = tolerances.get(column, default)
                diff = abs(fa - fb)
                scale = max(abs(fa), abs(fb))
                if diff <= tol.get("atol", 0.0) + tol.get("rtol", 0.0) * scale:
                    continue
                rel = diff / scale if scale else math.inf
            else:
                diff = rel = math.nan  # text, e.g. a crop name
            d = report.columns.setdefault((where, file, a.name, column), ColumnDiff())
            d.count += 1
            if not math.isnan(diff):
                d.max_abs = max(d.max_abs, diff)
                d.max_rel = max(d.max_rel, rel)
            if d.first is None:
                d.first = (key, va, vb)


def compare_runs(a: Path, b: Path, meta_a: dict, meta_b: dict,
                 ignore: frozenset[str] = frozenset()) -> Report:
    report = Report()
    suites_b = {s["name"]: s for s in meta_b["suites"]}
    for suite in meta_a["suites"]:
        other = suites_b.pop(suite["name"], None)
        if other is None:
            report.structural.append(f"suite `{suite['name']}` only in A")
            continue
        sims_b = dict(other["sims"])
        for sim, result in suite["sims"].items():
            where = f"{suite['name']}/{sim}"
            result_b = sims_b.pop(sim, None)
            if where in ignore:
                report.ignored.append(where)
                continue
            if result_b is None:
                report.structural.append(f"`{where}` only in A")
                continue
            if result["returncode"] or result_b["returncode"]:
                report.structural.append(f"`{where}` failed: return codes "
                                         f"{result['returncode']} vs {result_b['returncode']}")
                continue
            dir_a, dir_b = a / suite["name"] / sim, b / suite["name"] / sim
            files_a = {p.name for p in dir_a.glob("*.csv")}
            files_b = {p.name for p in dir_b.glob("*.csv")}
            for f in sorted(files_a ^ files_b):
                report.structural.append(f"`{where}` {f} only in {'A' if f in files_a else 'B'}")
            for f in sorted(files_a & files_b):
                report.files += 1
                if (dir_a / f).read_bytes() == (dir_b / f).read_bytes():
                    continue
                secs_a, secs_b = read_sections(dir_a / f), read_sections(dir_b / f)
                for s in secs_a.keys() ^ secs_b.keys():
                    report.structural.append(f"`{where}` {f}: section [{s}] only in "
                                             f"{'A' if s in secs_a else 'B'}")
                for s in secs_a.keys() & secs_b.keys():
                    compare_sections(where, f, secs_a[s], secs_b[s],
                                     suite.get("compare", {}), report)
        for sim in sims_b:
            if f"{suite['name']}/{sim}" in ignore:
                report.ignored.append(f"{suite['name']}/{sim}")
                continue
            report.structural.append(f"`{suite['name']}/{sim}` only in B")
    for name in suites_b:
        report.structural.append(f"suite `{name}` only in B")
    return report


def short(state: dict | None) -> str:
    if not state or "commit" not in state:
        return "-"
    return state["commit"][:10] + (" (dirty)" if state.get("dirty") else "")


def markdown(meta_a: dict, meta_b: dict, report: Report) -> str:
    out = [f"## {meta_a['engine']} vs {meta_b['engine']}", "",
           "| | A | B |", "|---|---|---|"]
    rows = [("engine", meta_a["engine"], meta_b["engine"]),
            ("MONICA", meta_a["monica"]["version"], meta_b["monica"]["version"]),
            ("MONICA source", short(meta_a["monica"]["source"]),
             short(meta_b["monica"]["source"])),
            ("platform", f"{meta_a['platform']['system']} {meta_a['platform']['machine']}",
             f"{meta_b['platform']['system']} {meta_b['platform']['machine']}"),
            ("parameters", short(meta_a["parameters"]), short(meta_b["parameters"]))]
    suites_b = {s["name"]: s for s in meta_b["suites"]}
    for s in meta_a["suites"]:
        rows.append((f"suite {s['name']}", short(s), short(suites_b.get(s["name"]))))
    out += [f"| {k} | {va} | {vb} |" for k, va, vb in rows]
    out.append("")
    differing_inputs = [k for k, va, vb in rows[1:] if va != vb and k != "platform"]
    if differing_inputs:
        out += [f"> **Note:** different inputs ({', '.join(differing_inputs)}), "
                "so differences may be expected.", ""]

    if report.ignored:
        out += [f"Not compared (--ignore): {', '.join(f'`{w}`' for w in report.ignored)}", ""]
    if report.identical:
        out.append(f"**Identical** ({report.files} output files compared).")
        return "\n".join(out) + "\n"

    out.append(f"**Differences** ({report.files} output files compared):")
    out.append("")
    out += [f"- {s}" for s in report.structural]
    if report.columns:
        out += ["", "| simulation | file | section | column | values | max abs | max rel "
                "| first at | A | B |", "|---|---|---|---|--:|--:|--:|---|---|---|"]
        items = sorted(report.columns.items(), key=lambda kv: -kv[1].max_rel)
        for (where, f, sec, col), d in items[:MAX_TABLE_ROWS]:
            key, va, vb = d.first
            out.append(f"| {where} | {f} | {sec} | {col} | {d.count} | {d.max_abs:.3g} "
                       f"| {d.max_rel:.3g} | {key} | {va} | {vb} |")
        if len(items) > MAX_TABLE_ROWS:
            out.append(f"\n... and {len(items) - MAX_TABLE_ROWS} more columns")
    return "\n".join(out) + "\n"


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    p.add_argument("a", type=Path, help="result directory A (e.g. the reference)")
    p.add_argument("b", type=Path, help="result directory B")
    p.add_argument("--summary", type=Path, default=os.environ.get("GITHUB_STEP_SUMMARY"),
                   help="also append the report to this file "
                        "(default: $GITHUB_STEP_SUMMARY)")
    p.add_argument("--ignore", nargs="+", default=[], metavar="SUITE/SIM",
                   help="don't compare these simulations, e.g. hohenfinow2/sim-min-ic for a "
                        "build known to lack a feature; they are listed in the report")
    a = p.parse_args()

    meta_a = json.loads((a.a / "metadata.json").read_text(encoding="utf-8"))
    meta_b = json.loads((a.b / "metadata.json").read_text(encoding="utf-8"))
    report = compare_runs(a.a, a.b, meta_a, meta_b, frozenset(a.ignore))
    text = markdown(meta_a, meta_b, report)
    print(text)
    if a.summary:
        with open(a.summary, "a", encoding="utf-8") as f:
            f.write(text + "\n")
    sys.exit(0 if report.identical else 1)


if __name__ == "__main__":
    main()
