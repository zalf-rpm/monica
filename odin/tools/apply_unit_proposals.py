#!/usr/bin/env python3
"""Write the ticked rows of odin/unit-proposals.md as `unit` struct tags.

    python odin/tools/apply_unit_proposals.py            # list what would be tagged
    python odin/tools/apply_unit_proposals.py --write    # tag the fields
    python odin/tools/apply_unit_proposals.py --all ...  # every row, ticked or not

A row is "| [x] | `Struct.field` | ... | `unit` | confidence | ..."; only ticked
rows ([x] or [X]) are applied (all with --all), each to the field's
declaration in odin/monica (unit-proposals.md: monica/params,
unit-proposals-structs.md: the other structs), after the type (CONVENTIONS.md §9a), with
the row's confidence as `unit_guess:"high|medium|low"`: the unit is a guess,
not documentation. A field that already has a unit tag is skipped, so running
it twice changes nothing.
"""
import argparse
import pathlib
import re
import sys

ODIN = pathlib.Path(__file__).resolve().parent.parent
ROW = re.compile(r"^\|\s*\[([ xX])\]\s*\|\s*\[?`(\w+)\.(\w+)`(?:\]\([^)]*\))?\s*\|[^|]*\|[^|]*\|\s*`([^`]*)`\s*\|\s*(\w+)\s*\|")
CONFIDENCE = ("high", "medium", "low")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--write", action="store_true")
    ap.add_argument("--all", action="store_true", help="apply every row, not only the ticked ones")
    ap.add_argument("--file", default=str(ODIN / "unit-proposals.md"))
    ns = ap.parse_args()

    ticked = []
    for line in open(ns.file, encoding="utf-8"):
        m = ROW.match(line)
        if m and (ns.all or m.group(1) in "xX"):
            ticked.append(m.groups()[1:])
    files = {f: f.read_text(encoding="utf-8").split("\n") for f in (ODIN / "monica").rglob("*.odin")}
    done, problems = 0, []
    for struct, field, unit, confidence in ticked:
        if '"' in unit or "`" in unit:
            problems.append(f"{struct}.{field}: a unit can't contain quotes or backticks")
            continue
        if confidence not in CONFIDENCE:
            problems.append(f"{struct}.{field}: confidence {confidence!r} is none of {', '.join(CONFIDENCE)}")
            continue
        hits = []
        for f, lines in files.items():
            try:
                start = next(i for i, l in enumerate(lines) if re.match(rf"^{struct} :: struct\b", l))
            except StopIteration:
                continue
            end = next(i for i in range(start, len(lines)) if lines[i] == "}")
            hits += [(f, i) for i in range(start, end)
                     if re.match(rf"^\t{re.escape(field)}:\s", lines[i])]
        if len(hits) != 1:
            problems.append(f"{struct}.{field}: {len(hits)} declarations found")
            continue
        f, i = hits[0]
        line = files[f][i]
        if "unit:" in line:
            print(f"skip   {struct}.{field} (already tagged)")
            continue
        m = re.match(r"^(\t\w+:\s+)([^,/`]+?)(,.*)$", line)
        tag = f'unit:"{unit}" unit_guess:"{confidence}"'
        files[f][i] = f'{m.group(1)}{m.group(2)} `{tag}`{m.group(3)}'
        print(f"tag    {struct}.{field} `{tag}`")
        done += 1
    for p in problems:
        print("ERROR  " + p, file=sys.stderr)
    if ns.write:
        for f, lines in files.items():
            f.write_text("\n".join(lines), encoding="utf-8")
    print(f"{done} of {len(ticked)} ticked rows {'tagged' if ns.write else 'would be tagged'}")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
