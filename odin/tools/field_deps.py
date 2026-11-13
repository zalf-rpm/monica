#!/usr/bin/env python3
"""Dependency scans for the Odin MONICA (odin/architecture.md).

A text scan, not a parser: good enough to see the structure, but check what
matters by hand (a field used as an index on the left of `=` isn't a write, a
write through a pointer passed to another package isn't seen).

  field_deps.py files <package dir>
      which file of a package uses which other file's top-level declarations

  field_deps.py fields <file.odin> <Struct> <var> [--soil <expr>...] [--json out.json]
      per procedure in <file>: the fields of <Struct> (reached through <var>.)
      it writes and reads, the soil column values it reads (through
      <var>.soil_column or the given expressions, e.g. sm.soil_column),
      the parameter structs it writes, and the procedures of the file it calls.
      Reads and writes include those of the procedures it calls.

  field_deps.py lag <file.odin> <Struct> <var> <step proc>
      walks <step proc> in order and lists the fields read before they are
      written that day but written later in it: the previous day's values.

  field_deps.py writers <file.odin> <Struct> <dir> [--only <field>...]
      for each field of <Struct>: the files and procedures under <dir> that
      assign it (by field name, so a field of another struct with the same
      name shows up too - check)

Run with the pixi Python: pixi run python tools/field_deps.py ...
"""

import argparse
import collections
import glob
import json
import os
import re

IDENT = re.compile(r"\b[A-Za-z_][A-Za-z0-9_]*\b")
ASSIGN = re.compile(r"^\s*(?!if\b|for\b|return\b|case\b|when\b)([^:=!<>]+?)\s*(\+=|-=|\*=|/=|=)(?!=)")
PARAMS = {"crop_params", "site_params", "sim_params", "mod_params", "perennial_crop_params", "residue_params",
          "params", "organic_params", "env_params"}


def strip_comments(s):
    return re.sub(r"//.*", "", s)


def files_graph(pkg):
    files = sorted(glob.glob(os.path.join(pkg, "*.odin")))
    decl = {}
    for f in files:
        for m in re.finditer(r"^([A-Za-z_][A-Za-z0-9_]*)\s*::", open(f).read(), re.M):
            decl.setdefault(m.group(1), f)
    name = lambda f: os.path.basename(f)[:-5]
    for f in files:
        c = collections.Counter()
        which = collections.defaultdict(collections.Counter)
        for tok in IDENT.findall(strip_comments(open(f).read())):
            g = decl.get(tok)
            if g and g != f:
                c[name(g)] += 1
                which[name(g)][tok] += 1
        print(f"{name(f)} -> " + ", ".join(f"{g}({n})" for g, n in c.most_common()))
        for g, _ in c.most_common():
            print(f"    {g}: " + " ".join(t for t, _ in which[g].most_common(8)))


def procs_of(lines):
    procs, cur = [], None
    for i, l in enumerate(lines):
        m = re.match(r"^([a-z_0-9]+) :: proc", l)
        if m:
            cur = [m.group(1), i, None]
            procs.append(cur)
        elif cur and l.startswith("}") and cur[2] is None:
            cur[2] = i
    return {n: (a, b) for n, a, b in procs if b is not None}


def struct_fields(src, struct):
    i = src.find(f"{struct} :: struct")
    if i < 0:
        raise SystemExit(f"no struct {struct}")
    body = src[i:src.index("\n}", i)]
    return {n.strip() for m in re.findall(r"^\t([a-z_0-9, ]+?)\s*:", body, re.M) for n in m.split(",")}


class Line_Scan:
    """The reads and writes of one line, given the aliases known so far."""

    def __init__(self, var, soil_exprs):
        self.var = var
        self.soil = set(soil_exprs) | {f"{var}.soil_column"}
        self.alias = {}  # local -> field it points into

    def scan(self, l):
        v = re.escape(self.var)
        for m in re.finditer(r"([a-z_0-9]+)\s*:=\s*&?" + v + r"\.([a-z_0-9]+)", l):
            if m.group(2) == "soil_column":
                self.soil.add(m.group(1))
            else:
                self.alias[m.group(1)] = m.group(2)
        R, W, SR, SW = set(), set(), set(), set()
        m = ASSIGN.match(l)
        for t in [t.strip() for t in m.group(1).split(",")] if m else []:
            mm = re.match(v + r"\.([a-z_0-9]+)", t)
            if mm and mm.group(1) != "soil_column":
                W.add(mm.group(1))
            root = re.match(r"([a-z_0-9]+)", t)
            if root and root.group(1) in self.alias and t != root.group(1):
                W.add(self.alias[root.group(1)])
            for s in self.soil:
                mm = re.match(re.escape(s) + r"\.(?:layers\[[^\]]*\]\.)?([a-z_0-9]+)", t)
                if mm:
                    SW.add(mm.group(1))
        for mm in re.finditer(r"[(,]\s*&" + v + r"\.([a-z_0-9]+)", l):
            if mm.group(1) != "soil_column":
                W.add(mm.group(1))
        for f in re.findall(v + r"\.([a-z_0-9]+)", l):
            if f != "soil_column":
                R.add(f)
        for s in self.soil:
            for f in re.findall(r"(?<![a-z_.])" + re.escape(s) + r"\.(?:layers\[[^\]]*\]\.)?([a-z_0-9]+)", l):
                SR.add(f)
        return R, W, SR - {"layers"}, SW


def scan_procs(path, struct, var, soil_exprs):
    src = open(path).read()
    lines = [strip_comments(l) for l in src.split("\n")]
    fields = struct_fields(src, struct)
    procs = procs_of(lines)
    res = {}
    for name, (a, b) in procs.items():
        ls = Line_Scan(var, soil_exprs)
        R, W, SR, SW = set(), set(), set(), set()
        for l in lines[a:b + 1]:
            r, w, sr, sw = ls.scan(l)
            R |= r; W |= w; SR |= sr; SW |= sw
        calls = sorted(set(re.findall(r"\b([a-z_0-9]+)\(", "\n".join(lines[a:b + 1]))) & set(procs) - {name})
        res[name] = dict(lines=b - a + 1, R=R & fields, W=W & fields, SR=SR, SW=SW, calls=calls)
    # include what the called procedures read and write
    def closure(n, seen):
        r = res[n]
        R, W, SR, SW = set(r["R"]), set(r["W"]), set(r["SR"]), set(r["SW"])
        for c in r["calls"]:
            if c not in seen:
                cr, cw, csr, csw = closure(c, seen | {c})
                R |= cr; W |= cw; SR |= csr; SW |= csw
        return R, W, SR, SW
    for n in res:
        R, W, SR, SW = closure(n, {n})
        res[n].update(R=R, W=W, SR=SR, SW=SW)
    return res, lines, procs, fields


def cmd_fields(a):
    res, _, _, _ = scan_procs(a.file, a.struct, a.var, a.soil)
    out = {}
    for n, r in res.items():
        o = dict(lines=r["lines"],
                 write=sorted(r["W"] - PARAMS), read=sorted(r["R"] - r["W"] - PARAMS),
                 params_written=sorted(r["W"] & PARAMS),
                 soil_read=sorted(r["SR"] - r["SW"]), soil_write=sorted(r["SW"]), calls=r["calls"])
        out[n] = o
        print(f"## {n} ({o['lines']} lines)")
        for k in ("write", "read", "params_written", "soil_write", "soil_read", "calls"):
            if o[k]:
                print(f"  {k}: {' '.join(o[k])}")
    if a.json:
        json.dump(out, open(a.json, "w"), indent=1)


def cmd_lag(a):
    res, lines, procs, fields = scan_procs(a.file, a.struct, a.var, [])
    if a.step not in procs:
        raise SystemExit(f"no procedure {a.step}")
    s, e = procs[a.step]
    # the step's events in order: (line, what, reads, writes)
    ls = Line_Scan(a.var, [])
    events = []
    for i in range(s + 1, e + 1):
        l = lines[i]
        called = [c for c in re.findall(r"\b([a-z_0-9]+)\(", l) if c in res and c != a.step]
        r, w, _, _ = ls.scan(l)
        for c in called:
            events.append((i + 1, c, res[c]["R"], res[c]["W"]))
        if (r | w) & fields:
            events.append((i + 1, a.step, r & fields, w & fields))
    # assignments of a call's result (x = f(...)) count as written after the call
    lag = collections.defaultdict(set)
    written = set()
    for k, (ln, who, R, W) in enumerate(events):
        later = set().union(*[ev[3] for ev in events[k + 1:]]) if k + 1 < len(events) else set()
        for f in sorted(R - written - W - PARAMS):
            if f in later:
                writers = sorted({ev[1] for ev in events[k + 1:] if f in ev[3]})
                lag[(who, ln)].add(f"{f} (written later by {', '.join(writers)})")
        written |= W
    for (who, ln), fs in lag.items():
        print(f"{who} (line {ln}): yesterday's")
        for f in sorted(fs):
            print(f"    {f}")


def cmd_writers(a):
    fields = struct_fields(open(a.file).read(), a.struct)
    if a.only:
        fields &= set(a.only)
    W = collections.defaultdict(lambda: collections.defaultdict(set))
    for f in sorted(glob.glob(os.path.join(a.dir, "**", "*.odin"), recursive=True)):
        proc = "?"
        for l in open(f).read().split("\n"):
            m = re.match(r"^([a-z_0-9]+) :: proc", l)
            if m:
                proc = m.group(1)
            for m in re.finditer(r"\.([a-z_0-9]+)(?:\[[^\]]*\])?\s*(\+=|-=|\*=|/=|=)(?!=)", strip_comments(l)):
                if m.group(1) in fields:
                    W[m.group(1)][os.path.basename(f)[:-5]].add(proc)
    for fld in sorted(W):
        print(f"{fld}:")
        for k, v in sorted(W[fld].items()):
            print(f"    {k}: {', '.join(sorted(v))}")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("files"); p.add_argument("pkg")
    p = sub.add_parser("fields"); p.add_argument("file"); p.add_argument("struct"); p.add_argument("var")
    p.add_argument("--soil", nargs="*", default=[]); p.add_argument("--json")
    p = sub.add_parser("lag"); p.add_argument("file"); p.add_argument("struct"); p.add_argument("var"); p.add_argument("step")
    p = sub.add_parser("writers"); p.add_argument("file"); p.add_argument("struct"); p.add_argument("dir")
    p.add_argument("--only", nargs="*")
    a = ap.parse_args()
    {"files": lambda: files_graph(a.pkg), "fields": lambda: cmd_fields(a), "lag": lambda: cmd_lag(a),
     "writers": lambda: cmd_writers(a)}[a.cmd]()


if __name__ == "__main__":
    main()
