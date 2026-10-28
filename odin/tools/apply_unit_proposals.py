#!/usr/bin/env python3
"""Write the ticked rows of odin/unit-proposals.md as `unit` struct tags.

    python odin/tools/apply_unit_proposals.py            # list what would be tagged
    python odin/tools/apply_unit_proposals.py --write    # tag the fields

A row is "| [x] | `Struct.field` | ... | `unit` | ..."; only ticked rows
([x] or [X]) are applied, each to the field's declaration in
odin/monica/params, after the type (CONVENTIONS.md §9a). A field that already
has a unit tag is skipped, so running it twice changes nothing.
"""
import argparse
import pathlib
import re
import sys

ODIN = pathlib.Path(__file__).resolve().parent.parent
ROW = re.compile(r"^\|\s*\[([ xX])\]\s*\|\s*`(\w+)\.(\w+)`\s*\|[^|]*\|[^|]*\|\s*`([^`]*)`\s*\|")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--write", action="store_true")
    ap.add_argument("--file", default=str(ODIN / "unit-proposals.md"))
    ns = ap.parse_args()

    ticked = []
    for line in open(ns.file, encoding="utf-8"):
        m = ROW.match(line)
        if m and m.group(1) in "xX":
            ticked.append(m.groups()[1:])
    files = {f: f.read_text(encoding="utf-8").split("\n") for f in (ODIN / "monica" / "params").glob("*.odin")}
    done, problems = 0, []
    for struct, field, unit in ticked:
        if '"' in unit or "`" in unit:
            problems.append(f"{struct}.{field}: a unit can't contain quotes or backticks")
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
        files[f][i] = f'{m.group(1)}{m.group(2)} `unit:"{unit}"`{m.group(3)}'
        print(f"tag    {struct}.{field} `unit:\"{unit}\"`")
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
