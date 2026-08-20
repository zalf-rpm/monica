#!/usr/bin/env python3
"""Generate monica/io/output_paths.odin's alias table from src/io/build-output.cpp.

The C++ registers ~181 output ids, each as a lambda. Most of those lambdas are
nothing but "read this field, round to N digits" - which the reflection path
engine (support/reflectpath) can do from a path string, with no code per id.
This script does the mechanical sweep that separates those from the ones whose
value is genuinely *computed* (a proc call, arithmetic, a Date method, an
organ-count guard), and emits the alias rows for the former.

    python odin/tools/gen_output_aliases.py            # print the table rows
    python odin/tools/gen_output_aliases.py --report   # + why each id was skipped

The output is pasted into odin/monica/io/output_paths.odin, NOT written there:
the rows are hand-audited afterwards, and odin/tests/output_paths_test.odin
then proves every row compiles against Monica_Model and lands on a numeric
leaf. See plan-reflective-outputs.md §§1, 6.
"""
import argparse
import os
import re
import sys

CPP = os.path.join(os.path.dirname(__file__), "..", "..", "src", "io", "build-output.cpp")

ORGAN = {"OId::ROOT": "0", "OId::LEAF": "1", "OId::SHOOT": "2",
         "OId::FRUIT": "3", "OId::STRUCT": "4", "OId::SUGAR": "5"}

LAM = r"\[\]\(const MonicaModel &monica, OId oid\) \{ "

# Ordered: the first pattern that matches wins, so the guarded/rounded forms
# must come before the bare ones they are a special case of.
PATTERNS = [
    ("c_int", re.compile(LAM + r"return monica\.currentCropModule\.get\(\) \? int\((.+?)\) : 0; \}$")),
    ("c_round", re.compile(LAM + r"return monica\.currentCropModule\.get\(\) \? (?:Tools::)?round\( ?(.+?), (\d+)\) : 0\.0; \}$")),
    ("c_raw", re.compile(LAM + r"return monica\.currentCropModule\.get\(\) \? (.+?) : 0\.0; \}$")),
    ("s_round", re.compile(LAM + r"return (?:Tools::)?round\((.+?), (\d+)\); \}$")),
    ("len", re.compile(LAM + r"return int\((monica\.[A-Za-z0-9_.>\-]+(?:\.at\(\d+\))?[A-Za-z0-9_.]*)\.size\(\)\); \}$")),
    ("s_raw", re.compile(LAM + r"return (monica\.[A-Za-z0-9_.>\-]+); \}$")),
    ("gcv_crop", re.compile(LAM + r"return getComplexValues<double>\( oid, \[&\]\(int i\) \{ return monica\.currentCropModule\.get\(\) \? (.+?) : 0\.0; \}, (\d+)\); \}$")),
    ("gcv_pool", re.compile(LAM + r"return getComplexValues<double>\( oid, \[&\]\(int i\) \{ const auto &layer = (.+?); return layer\.vo_AOM_Pool\.empty\(\) \? 0\.0 : layer\.vo_AOM_Pool\.at\(0\)\.(\w+); \}, (\d+)\); \}$")),
    ("gcv", re.compile(LAM + r"return getComplexValues<double>\( oid, \[&\]\(int i\) \{ return (.+?); \}, (\d+)\); \}$")),
    ("climate", re.compile(LAM + r"const auto &cd = monica\.climateData\.back\(\); auto ci = cd\.find\(Climate::(\w+)\); return ci == cd\.end\(\) \? 0\.0 : round\(ci->second, (\d+)\); \}$")),
]


def split_top_level(s):
    """Split build()'s argument list on commas that are not inside brackets."""
    out, depth, cur = [], 0, []
    for ch in s:
        if ch in "([{":
            depth += 1
        elif ch in ")]}":
            depth -= 1
        if ch == "," and depth == 0:
            out.append("".join(cur))
            cur = []
        else:
            cur.append(ch)
    out.append("".join(cur))
    return [x.strip() for x in out]


def entries(src):
    """Yield (metadata, of-lambda) for every build({id++, ...}, of[, setf]) call."""
    i = 0
    while True:
        m = re.search(r"\bbuild\(\s*\{\s*id\+\+,\s*", src[i:])
        if not m:
            return
        p = i + m.end()
        depth, j = 1, p
        while depth:
            depth += (src[j] == "{") - (src[j] == "}")
            j += 1
        meta = " ".join(src[p:j - 1].split())
        depth, k = 1, j
        while depth:
            depth += (src[k] == "(") - (src[k] == ")")
            k += 1
        args = split_top_level(" ".join(src[j:k - 1].split()))
        of = next((a for a in args if a.startswith("[]")), "")
        yield meta, of
        i = k


def to_path(expr, open_dim):
    """A C++ member expression -> an Odin path, or None if it is not a pure
    field path (a call, an index expression or arithmetic anywhere makes it
    computed, which is exactly the tier this script must NOT alias)."""
    e = expr.strip()
    for k, v in ORGAN.items():
        e = e.replace("[%s]" % k, "." + v)
    if not e.startswith("monica."):
        return None
    e = e[len("monica."):]
    for a, b in (("currentCropModule.get()->", "currentCropModule."),
                 ("currentCropModule->", "currentCropModule."),
                 ("soilColumn.get()->", "soilColumn."),
                 ("soilOrganic.get()->", "soilOrganic.")):
        e = e.replace(a, b)
    e = e.replace("->", ".")
    if open_dim:
        # the unindexed array becomes the open dimension the OId drives
        e = e.replace(".at(i)", "").replace("[i]", "")
    e = re.sub(r"\.at\((\d+)\)", r".\1", e)
    if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*(\.[A-Za-z0-9_]+)*", e):
        return None
    return e


def classify(of):
    """-> (path, round, cast) or None if this id belongs to the computed tier."""
    for kind, rx in PATTERNS:
        m = rx.match(of)
        if not m:
            continue
        if kind == "c_int":
            p = to_path(m.group(1), False)
            return (p, -1, "INT") if p else None
        if kind in ("c_round", "s_round"):
            p = to_path(m.group(1), False)
            return (p, int(m.group(2)), "NONE") if p else None
        if kind == "c_raw":
            p = to_path(m.group(1), False)
            return (p, -1, "NONE") if p else None
        if kind == "len":
            p = to_path(m.group(1), False)
            return (p + ".#len", -1, "INT") if p else None
        if kind == "s_raw":
            p = to_path(m.group(1), False)
            return (p, -1, "NONE") if p else None
        if kind in ("gcv", "gcv_crop"):
            p = to_path(m.group(1), True)
            return (p, int(m.group(2)), "NONE") if p else None
        if kind == "gcv_pool":
            base = to_path(m.group(1), True)
            return (base + ".vo_AOM_Pool.0." + m.group(2), int(m.group(3)), "NONE") if base else None
        if kind == "climate":
            return ("climateData.#last." + m.group(1), int(m.group(2)), "NONE")
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--report", action="store_true", help="also list the skipped ids on stderr")
    ap.add_argument("--cpp", default=CPP)
    args = ap.parse_args()

    raw = open(args.cpp, encoding="utf-8").read()
    # Strip // comments first: build-output.cpp has a fully commented-out
    # build() call (Act_ET2) that would otherwise be extracted as a real id.
    src = "\n".join(re.sub(r"//.*$", "", ln) for ln in raw.split("\n"))
    src = src[src.index("BOTRes &monica::buildOutputTable()"):]

    rows, skipped, total = {}, [], 0
    for meta, of in entries(src):
        total += 1
        names = re.findall(r'"((?:[^"\\]|\\.)*)"', meta)
        name, unit = names[0], (names[1] if len(names) > 1 else "")
        got = classify(of)
        if got:
            rows[name] = (unit, got)  # a re-registered name: last one wins, as in the C++
        else:
            skipped.append(name)

    for name, (unit, (path, rnd, cast)) in sorted(rows.items(), key=lambda kv: kv[0].lower()):
        print('\t{"%s", {"%s", %d, .%s, "%s"}},' % (name, path, rnd, cast, unit))

    print("// %d of %d ids aliased; %d stay computed" % (len(rows), total, len(skipped)),
          file=sys.stderr)
    if args.report:
        print("// skipped: " + ", ".join(sorted(skipped)), file=sys.stderr)


if __name__ == "__main__":
    main()
