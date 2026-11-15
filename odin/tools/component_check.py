#!/usr/bin/env python3
"""The crop components' declared inputs (CONVENTIONS.md 9b, plan-component-interfaces.md H).

A text scan of monica/core/crop_*.odin, not a parser - like field_deps.py.

  component_check.py check
      findings, exit status 1 if there are any:
      - unused: an *_In field the component's procedures never read
      - lag:    a lag:"1d" field filled from something that isn't the carry-over
                (prev.x, or a local/input derived from it)
      - order:  in crop_module_step, a same-day input from a component that the
                step hasn't written yet when the input is filled
      - src:    a field filled from another component's state than its src says

  component_check.py graph [--all]
      the component graph (mermaid) from the src/lag tags: solid arrows for
      same-day values, dashed for the previous day's; --all adds the weather,
      soil, site and parameter inputs

  component_check.py doc [-o components.md] [--check]
      a generated reference: the graph, and per component its state (with
      units) and its inputs (with unit, src, lag)

Run with the pixi Python from odin/: pixi run python tools/component_check.py check
"""

import argparse
import glob
import os
import re
import sys

CORE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "monica", "core")

# file -> component (the state struct is Crop_<component>)
FILE_COMPONENT = {
    "crop_radiation": "Radiation", "crop_phenology": "Phenology", "crop_canopy": "Canopy",
    "crop_photosynthesis": "Photosynthesis", "crop_o3": "O3", "crop_voc": "Voc", "crop_stress": "Stress",
    "crop_n_status": "N_Status", "crop_growth": "Growth", "crop_root_system": "Root_System", "crop_et0": "Et0",
    "crop_water_uptake": "Water_Uptake", "crop_n_uptake": "N_Uptake", "crop_carbon": "Carbon",
    "crop_management": "Management",
}
FIELD = re.compile(r"^\t(\w+):\s+(.+?)\s*(?:`([^`]*)`)?,\s*(?://\s*(.*))?$")
TAG = re.compile(r'(\w+):"([^"]*)"')


def strip_comments(s):
    return re.sub(r"//[^\n]*", "", s)


def read(path):
    with open(path, encoding="utf-8") as f:
        return f.read().replace("\r\n", "\n")


def structs(src):
    """name -> list of (field, type, tags, comment)"""
    out = {}
    for m in re.finditer(r"^(\w+) :: struct \{\n(.*?)\n\}", src, re.M | re.S):
        fields = []
        for line in m.group(2).split("\n"):
            f = FIELD.match(line)
            if f and not line.strip().startswith("//"):
                fields.append((f.group(1), f.group(2), dict(TAG.findall(f.group(3) or "")), f.group(4) or ""))
        out[m.group(1)] = fields
    return out


def procs(src):
    """name -> (signature, body) for top-level procedures"""
    out = {}
    for m in re.finditer(r"^(\w+) :: proc\(", src, re.M):
        end = src.find("\n}\n", m.start())
        text = src[m.start():end + 2]
        brace = re.search(r"\{\n", text)
        out[m.group(1)] = (text[:brace.start()], text[brace.end():])
    return out


def call_args(text, i):
    """text[i] == '(' -> list of argument strings"""
    depth, k, start, args = 1, i + 1, i + 1, []
    while depth:
        c = text[k]
        if c in "([{":
            depth += 1
        elif c in ")]}":
            depth -= 1
            if depth == 0:
                args.append(text[start:k])
        elif c == "," and depth == 1:
            args.append(text[start:k])
            start = k + 1
        k += 1
    return [a.strip() for a in args if a.strip()], k


class Model:
    def __init__(self):
        self.inputs = {}       # In name -> fields
        self.input_comp = {}   # In name -> reader component
        self.states = {}       # component -> fields
        self.field_comp = {}   # state field -> component
        self.procs = {}        # proc name -> (sig, body, file)
        self.embed = {}        # component -> embedded field name in Crop_Module
        for path in sorted(glob.glob(os.path.join(CORE, "crop_*.odin"))):
            base = os.path.splitext(os.path.basename(path))[0]
            src = read(path)
            for name, fields in structs(src).items():
                if name.endswith("_In"):
                    self.inputs[name] = fields
                    self.input_comp[name] = FILE_COMPONENT.get(base, base)
                elif name.startswith("Crop_") and name[5:] in FILE_COMPONENT.values():
                    self.states[name[5:]] = fields
                    for f in fields:
                        self.field_comp[f[0]] = name[5:]
            for name, (sig, body) in procs(src).items():
                self.procs[name] = (sig, strip_comments(body), base)
            if base == "crop_module":
                for m in re.finditer(r"^\tusing (\w+):\s+Crop_(\w+),", src, re.M):
                    self.embed[m.group(2)] = m.group(1)
        # the fill helpers: x_in :: proc(...) -> X_In { return {field = expr, ...} }
        self.helpers = {}
        for name, (sig, body, _) in self.procs.items():
            m = re.search(r"->\s*(\w+_In)\s*$", sig.strip())
            if m:
                params = [p.split(":")[0].strip() for p in re.sub(r"\s+", " ", sig[sig.index("(") + 1:sig.rindex(")")]).split(",")]
                # "a, b: f64" -> a, b ; keep order
                names = []
                for p in re.sub(r"\s+", " ", sig[sig.index("(") + 1:sig.rindex(")")]).split(","):
                    p = p.strip()
                    if p:
                        names.append(p.split(":")[0].strip())
                self.helpers[name] = (m.group(1), names, dict(re.findall(r"^\t+(\w+)\s*=\s*(.+?),?$", body, re.M)))

    def lag_fields(self, in_name):
        return {f[0] for f in self.inputs.get(in_name, []) if f[2].get("lag")}


def derived_from_lag(body, expr, lag_inputs):
    """expr is prev.x, input.<lag field>, or a local defined from one of those."""
    expr = expr.strip()
    if "prev." in expr:
        return True
    m = re.fullmatch(r"input\.(\w+)(?:\[[^\]]*\])?", expr)
    if m and m.group(1) in lag_inputs:
        return True
    if re.fullmatch(r"\w+", expr):
        d = re.search(rf"^\t+{expr}\s*:=\s*(.+)$", body, re.M)
        if d and ("prev." in d.group(1) or any(f"input.{f}" in d.group(1) for f in lag_inputs)):
            return True
    return False


def fills(model):
    """every place an *_In is filled: (proc, line no in body, In name, {field: expr})"""
    out = []
    for pname, (sig, body, _) in model.procs.items():
        if pname in model.helpers:
            continue
        for in_name in model.inputs:
            for m in re.finditer(rf"\b{in_name}\s*\{{", body):
                _, k = call_args(body, m.end() - 1)
                lit = body[m.end():k - 1]
                out.append((pname, body.count("\n", 0, m.start()), in_name,
                            dict(re.findall(r"^\s*(\w+)\s*=\s*(.+?),?\s*$", lit, re.M))))
        for hname, (in_name, params, mapping) in model.helpers.items():
            for m in re.finditer(rf"(?<![\w.]){hname}\(", body):
                args, _ = call_args(body, m.end() - 1)
                fieldmap = {}
                for f, expr in mapping.items():
                    expr = expr.strip()
                    if expr in params:
                        idx = params.index(expr)
                        fieldmap[f] = args[idx] if idx < len(args) else expr
                    else:
                        fieldmap[f] = expr
                out.append((pname, body.count("\n", 0, m.start()), in_name, fieldmap))
    return out


def written_before(model, step_lines, line, comp):
    """does crop_module_step write component `comp`'s state before body line `line`?"""
    emb = model.embed.get(comp, "")
    comp_fields = {f[0] for f in model.states.get(comp, [])}
    for ln in step_lines[:line + 1]:
        if (emb and (f"&cm.{emb}" in ln or re.search(rf"\bcm\.{emb}\s*=", ln))) or \
                any(re.search(rf"\bcm\.{cf}\b[^=\n]*=[^=]", ln) for cf in comp_fields if f"cm.{cf}" in ln):
            return True
    return False


def cmd_check(model):
    findings = []
    # unused
    for in_name, fields in model.inputs.items():
        bodies = [b for (sig, b, _) in model.procs.values() if re.search(rf"\binput:\s*{in_name}\b", sig)]
        if not bodies:
            findings.append(f"unused: {in_name} is no procedure's `input`")
            continue
        text = "\n".join(bodies)
        for f in fields:
            if not re.search(rf"\binput\.{f[0]}\b", text):
                findings.append(f"unused: {in_name}.{f[0]}")
    step_sig, step_body, _ = model.procs["crop_module_step"]
    step_lines = step_body.split("\n")
    for pname, line, in_name, fieldmap in fills(model):
        sig, body, _ = model.procs[pname]
        enclosing_in = re.search(r"\binput:\s*(\w+_In)\b", sig)
        lag_inputs = model.lag_fields(enclosing_in.group(1)) if enclosing_in else set()
        fields = {f[0]: f for f in model.inputs[in_name]}
        for fname, expr in fieldmap.items():
            if fname not in fields:
                continue
            tags = fields[fname][2]
            src = tags.get("src", "")
            m = re.fullmatch(r"(?:cm\.)?(\w+)(?:\[.*\])?(?:\[:\])?", expr)
            state_field = m.group(1) if m and expr.startswith("cm.") else None
            # lag: from the carry-over, or read in place from state the step
            # hasn't written yet today (arrays, in/out values - CONVENTIONS.md 9b)
            if tags.get("lag") and not derived_from_lag(body, expr, lag_inputs):
                in_place_ok = (pname == "crop_module_step" and state_field in model.field_comp
                               and not written_before(model, step_lines, line, model.field_comp[state_field]))
                if not in_place_ok:
                    findings.append(f"lag: {in_name}.{fname} = {expr} in {pname} is neither the carry-over nor "
                                    f"state not yet written today")
            if not tags.get("lag") and "prev." in expr:
                findings.append(f"lag: {in_name}.{fname} = {expr} in {pname} is yesterday's but has no lag tag")
            # src: filled from another component's state
            if state_field and state_field in model.field_comp and src in model.states \
                    and model.field_comp[state_field] != src:
                findings.append(f"src: {in_name}.{fname} = {expr} is {model.field_comp[state_field]}'s, src says {src}")
            # order (in the step only): the src component written before this fill
            if pname == "crop_module_step" and src in model.states and not tags.get("lag"):
                if not written_before(model, step_lines, line, src):
                    findings.append(f"order: {in_name}.{fname} (src {src}) is filled in the step before {src} runs - "
                                    f"yesterday's value without a lag tag?")
    for f in findings:
        print(f)
    print(f"{len(model.inputs)} inputs checked, {len(findings)} findings")
    return 1 if findings else 0


EXTERNAL = ["weather", "site", "soil", "soil moisture", "species", "cultivar", "crop parameters",
            "crop module parameters", "simulation parameters"]


def edges(model):
    out = {}
    for in_name, fields in model.inputs.items():
        reader = model.input_comp[in_name]
        for f in fields:
            src = f[2].get("src")
            if not src or src == reader and not f[2].get("lag"):
                continue
            key = (src, reader, bool(f[2].get("lag")))
            out.setdefault(key, []).append(f[0])
    return out


def node_id(name):
    return re.sub(r"\W", "_", name)


def graph(model, external=False):
    lines = ["flowchart LR"]
    es = {k: v for k, v in edges(model).items() if external or k[0] not in EXTERNAL}
    used = {s for (s, r, _) in es} | {r for (s, r, _) in es}
    for ext in EXTERNAL:
        if ext in used:
            lines.append(f"    {node_id(ext)}([{ext}])")
    for comp in FILE_COMPONENT.values():
        if comp in used:
            lines.append(f"    {node_id(comp)}[{comp}]")
    for (src, reader, lag), fields in sorted(es.items()):
        label = ", ".join(sorted(set(fields))) if len(set(fields)) <= 3 else f"{len(set(fields))} values"
        arrow = "-.->" if lag else "-->"
        lines.append(f"    {node_id(src)} {arrow}|\"{label}\"| {node_id(reader)}")
    return "\n".join(lines)


def doc(model):
    out = ["<!-- generated by `pixi run python tools/component_check.py doc` - don't edit -->",
           "# The crop components",
           "",
           "Generated from the state structs and the tagged inputs in `monica/core/crop_*.odin`",
           "(`CONVENTIONS.md` 9b, `plan-component-interfaces.md`). Solid arrows: the same day's value;",
           "dashed: the previous day's (`lag:\"1d\"`, from `Crop_Carry_Over`, or read in place before",
           "the source runs). Same-day edges from a component to itself are left out; the inputs from the",
           "weather, the soil, the site and the parameters are in the tables. What the crop gives the soil",
           "is `Crop_To_Soil` (`crop_to_soil.odin`).",
           "",
           "```mermaid", graph(model), "```", ""]
    by_comp = {}
    for in_name, comp in model.input_comp.items():
        by_comp.setdefault(comp, []).append(in_name)
    file_of = {v: k for k, v in FILE_COMPONENT.items()}
    for comp in FILE_COMPONENT.values():
        out += [f"## {comp}", "", f"`monica/core/{file_of[comp]}.odin`", ""]
        if comp in model.states:
            out += [f"State `Crop_{comp}`:", "", "| Field | Type | Unit |", "|---|---|---|"]
            for f, t, tags, _ in model.states[comp]:
                out.append(f"| `{f}` | `{t}` | {tags.get('unit', '')} |")
            out.append("")
        for in_name in sorted(by_comp.get(comp, [])):
            out += [f"Input `{in_name}`:", "", "| Field | Type | Unit | From | Previous day | Note |",
                    "|---|---|---|---|---|---|"]
            for f, t, tags, comment in model.inputs[in_name]:
                out.append(f"| `{f}` | `{t}` | {tags.get('unit', '')} | {tags.get('src', '')} | "
                           f"{'yes' if tags.get('lag') else ''} | {comment} |")
            out.append("")
    return "\n".join(out)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("check")
    g = sub.add_parser("graph")
    g.add_argument("--all", action="store_true")
    d = sub.add_parser("doc")
    d.add_argument("-o", "--output")
    d.add_argument("--check", action="store_true", help="exit 1 if --output differs from what would be generated")
    a = ap.parse_args()
    model = Model()
    if a.cmd == "check":
        sys.exit(cmd_check(model))
    if a.cmd == "graph":
        print(graph(model, a.all))
    if a.cmd == "doc":
        text = doc(model)
        if a.check:
            current = read(a.output) if a.output and os.path.exists(a.output) else ""
            if current != text + "\n":
                print(f"{a.output} is out of date - run: pixi run gen-components-doc")
                sys.exit(1)
            print(f"{a.output} is up to date")
            return
        if a.output:
            with open(a.output, "w", encoding="utf-8", newline="\n") as f:
                f.write(text + "\n")
        else:
            print(text)


main()
