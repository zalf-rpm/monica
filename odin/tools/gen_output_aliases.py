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
NAME_MAP = os.path.join(os.path.dirname(__file__), "..", "NAME_MAP.md")

ORGAN = {"OId::ROOT": "0", "OId::LEAF": "1", "OId::SHOOT": "2",
         "OId::FRUIT": "3", "OId::STRUCT": "4", "OId::SUGAR": "5"}

ODIN_SRC = os.path.join(os.path.dirname(__file__), "..", "monica")

# Hungarian-style prefixes of C++ member names (vm_ActualEvaporation,
# vc_KcFactor, vo_AOM_Pool, pc_...), which the Odin port drops.
CPP_PREFIX = re.compile(r"^(?:v[a-z]?|p[a-z]?)_")


def load_name_map(path=NAME_MAP):
    """Parse odin/NAME_MAP.md into {Odin_Struct_Name: {cpp_field: odin_field}}.

    Each struct gets its own "## ... -> `pkg.Struct_Name` (...)" heading
    followed by a "| `cpp` | `odin` |" table; this reads every such table
    regardless of how many structs the file grows to cover.
    """
    if not os.path.exists(path):
        return {}
    tables, current = {}, None
    heading = re.compile(r"^##\s.*->\s*`[\w.]+\.(\w+)`")
    row = re.compile(r"^\|\s*`([^`]+)`\s*\|\s*`([^`]+)`\s*\|")
    for line in open(path, encoding="utf-8"):
        m = heading.match(line)
        if m:
            current = m.group(1)
            tables[current] = {}
            continue
        m = row.match(line)
        if m and current:
            tables[current][m.group(1)] = m.group(2)
    return tables


def load_odin_structs(root=ODIN_SRC):
    """{Struct_Name: {field: type}} for every `Name :: struct {` under
    odin/monica, so a path can be walked field by field with each segment
    looked up in the struct its parent field actually is."""
    structs = {}
    head = re.compile(r"^(\w+)\s*::\s*struct\b.*\{\s*$")
    field = re.compile(r"^\t(?:using\s+)?([\w, ]+?)\s*:\s*([^=/]+?)\s*,?\s*(?://.*)?$")
    for dirpath, _, files in os.walk(root):
        for fn in files:
            if not fn.endswith(".odin"):
                continue
            current = None
            for line in open(os.path.join(dirpath, fn), encoding="utf-8"):
                m = head.match(line)
                if m:
                    current = structs.setdefault(m.group(1), {})
                    continue
                if current is None:
                    continue
                if line.startswith("}"):
                    current = None
                    continue
                m = field.match(line.rstrip("\n"))
                if m:
                    for name in m.group(1).split(","):
                        current[name.strip()] = m.group(2).strip()
    return structs


def base_struct(odin_type):
    """`^[dynamic]p.Soil_Layer` -> `Soil_Layer`: the struct a path continues
    in after this field (indexing an array/pointer keeps the element type)."""
    t = odin_type.strip()
    while True:
        m = re.match(r"^(?:\^|\[[^\]]*\]|#soa\s*)(.*)$", t) or re.match(r"^Maybe\((.*)\)$", t)
        if not m:
            break
        t = m.group(1).strip()
    return t.split(".")[-1]


def snake(name):
    """camelCase -> snake_case. A digit splits from a following capital only
    when a word starts there: AtmosphericCO2Concentration ->
    atmospheric_co2_concentration, but N2O_Produced -> n2o_produced."""
    s1 = re.sub(r"([A-Z]+)([A-Z][a-z])", r"\1_\2", name)
    s1 = re.sub(r"([a-z])([A-Z])", r"\1_\2", s1)
    return re.sub(r"([0-9])([A-Z][a-z])", r"\1_\2", s1).lower()


# C++ members whose Odin counterpart lives in a different struct, which a
# per-struct rename (NAME_MAP.md) can't express. The targets are Odin paths
# and are still walked, so a later rename of them is caught, not emitted.
MOVED = {
    "soilMoisture.vm_SurfaceWaterStorage": "soil_column.surface_water_storage",
    "soilMoisture.vm_FluxAtLowerBoundary": "soil_column.flux_at_lower_boundary",
    # per-layer in the C++ too (vm_WaterFlux[i]); Odin keeps it on the layer
    "soilMoisture.vm_WaterFlux": "soil_column.layers.soil_water_flux",
    # Monica_Model.soil_temperature is a union (MONICA's own or a module,
    # plan-crop2ml-modules.md §1.2); both write the soil column, which is
    # what the C++ reads through soilTemperature's back-pointer (STemp) or
    # its own copy (SurfTemp)
    "soilTemperature.soilColumn": "soil_column",
    "soilTemperature.soilSurfaceTemperature": "soil_column.soil_surface_temperature",
}


def odin_field(struct, seg, structs, name_map):
    """The Odin name of C++ member `seg` of `struct`, or None. In order:
    NAME_MAP.md (the recorded renames), the name as-is (never renamed, or
    already the Odin spelling), then the port's convention - Hungarian
    prefix dropped, snake_cased - which covers structs that were ported in
    snake_case from the start and so never got a NAME_MAP table. Every
    candidate must be a real field, so a guess can't slip through."""
    fields = structs.get(struct, {})
    for cand in (name_map.get(struct, {}).get(seg), seg,
                 snake(CPP_PREFIX.sub("", seg)), snake(seg)):
        if cand and cand in fields:
            return cand
    return None


def odin_path(cpp_path, structs, name_map, unresolved):
    """Walk a C++-shaped path ("currentCropModule.vc_KcFactor") from
    Monica_Model, renaming every segment to its Odin field. Index and `#`
    segments keep the current struct. None if any segment doesn't resolve -
    recorded in `unresolved` for --report."""
    for cpp, odin in MOVED.items():
        if cpp_path == cpp or cpp_path.startswith(cpp + "."):
            cpp_path = odin + cpp_path[len(cpp):]
            break
    struct, out = "Monica_Model", []
    for seg in cpp_path.split("."):
        if seg.isdigit() or seg.startswith("#"):
            out.append(seg)
            continue
        name = odin_field(struct, seg, structs, name_map)
        if name is None:
            unresolved.append("%s (no field %r in %s)" % (cpp_path, seg, struct))
            return None
        out.append(name)
        struct = base_struct(structs[struct][name])
    return ".".join(out)


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


_NAME_MAP = load_name_map()
_STRUCTS = load_odin_structs()


def to_path(expr, open_dim):
    """A C++ member expression -> a C++-shaped path (still the C++ member
    names; odin_path renames them), or None if it is not a pure
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
            # the last segment is a Climate::ACD key into a map, not a field
            return ("climateData.#last", int(m.group(2)), "NONE", m.group(1))
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
    m = re.search(r"BOTRes\s*&\s*monica::buildOutputTable\(\)", src)
    if not m:
        sys.exit("gen_output_aliases: can't find buildOutputTable() in %s" % args.cpp)
    src = src[m.start():]

    rows, skipped, unresolved, total = {}, [], [], 0
    for meta, of in entries(src):
        total += 1
        names = re.findall(r'"((?:[^"\\]|\\.)*)"', meta)
        name, unit = names[0], (names[1] if len(names) > 1 else "")
        got = classify(of)
        if got:
            cpp_path, rnd, cast = got[:3]
            path = odin_path(cpp_path, _STRUCTS, _NAME_MAP, unresolved)
            if path is None:
                skipped.append(name)
                continue
            if len(got) > 3:
                path += "." + got[3]
            rows[name] = (unit, (path, rnd, cast))  # a re-registered name: last one wins, as in the C++
        else:
            skipped.append(name)

    for name, (unit, (path, rnd, cast)) in sorted(rows.items(), key=lambda kv: kv[0].lower()):
        print('\t{"%s", {"%s", %d, .%s, "%s"}},' % (name, path, rnd, cast, unit))

    print("// %d of %d ids aliased; %d stay computed" % (len(rows), total, len(skipped)),
          file=sys.stderr)
    if args.report:
        print("// skipped: " + ", ".join(sorted(skipped)), file=sys.stderr)
    # Always shown: a path that no longer resolves is a rename this script
    # can't follow - fix NAME_MAP.md (or the field) rather than the table.
    for u in unresolved:
        print("// UNRESOLVED: " + u, file=sys.stderr)
    if unresolved:
        sys.exit(1)


if __name__ == "__main__":
    main()
