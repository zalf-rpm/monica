#!/usr/bin/env python3
"""Link the fields in the unit proposal lists to their declarations.

    python odin/tools/link_unit_proposals.py      # unit-proposals.md, unit-proposals-structs.md

Turns "`Struct.field`" in a table row into
"[`Struct.field`](monica/core/x.odin#L27)", the declaration's file and line
(GitHub and VS Code's Markdown preview jump to the line; other viewers open
the file). Line numbers go stale when the code changes: run it again, it
replaces existing links (and a "line" column, if the table has one). A field
whose declaration isn't found (or is found twice) keeps its plain name and is
reported.

In a "## Findings ..." section, every `name` in backticks is linked too:
`Struct.field` (also `Struct.prefix_*`: the first match), `Struct`, a bare
`field` (of the struct named last in the same bullet, else the only struct
that has it), a procedure, a legacy output name or JSON key (its first
occurrence in the code), a file (`x.odin`, `x.md`, `x.odin:83`, `x.cpp:96` -
the C++ in ../src). Names it can't resolve stay plain and are reported.
"""
import fnmatch
import os
import pathlib
import re
import sys

ODIN = pathlib.Path(__file__).resolve().parent.parent
FILES = ["unit-proposals.md", "unit-proposals-structs.md"]
CELL = re.compile(r"^(\|\s*\[[ xX]\]\s*\|\s*)\[?`(\w+)\.(\w+)`(?:\]\([^)]*\))?(\s*\|)(.*)$")


def declarations():
    """(struct, field) -> [(path relative to odin/, line)]"""
    decl = {}
    for f in sorted((ODIN / "monica").rglob("*.odin")):
        struct = None
        for i, l in enumerate(f.read_text(encoding="utf-8").split("\n"), 1):
            m = re.match(r"^(\w+) :: struct\b", l)
            if m:
                struct = m.group(1)
            elif l == "}":
                struct = None
            elif struct:
                m = re.match(r"^\t([\w, ]+):\s", l)
                if m:
                    for n in m.group(1).split(","):
                        decl.setdefault((struct, n.strip()), []).append((f.relative_to(ODIN).as_posix(), i))
    return decl


def code_index():
    """Structs, procedures and the code's lines, for the findings' names."""
    structs, procs, lines = {}, {}, []
    for f in sorted((ODIN / "monica").rglob("*.odin")):
        rel = f.relative_to(ODIN).as_posix()
        for i, l in enumerate(f.read_text(encoding="utf-8").split("\n"), 1):
            m = re.match(r"^(\w+)\s*(?::\s*[^:=]+?\s*)?::?\s*(struct|enum|union|bit_set|distinct)?(?=[\s({]|$)", l)
            if m:
                (structs if m.group(2) else procs).setdefault(m.group(1), (rel, i))  # procs: also constants
            lines.append((rel, i, l))
    return structs, procs, lines


TOKEN = re.compile(r"\[`([^`]+)`\]\([^)]*\)|`([^`]+)`")


def link_findings(lines, decl, index, name):
    """Link the names in the findings section(s) of *lines*, in place."""
    structs, procs, code = index
    by_field = {}
    for (st, fd), hits in decl.items():
        if len(hits) == 1:
            by_field.setdefault(fd, []).append(hits[0])

    def resolve(t, ctx):
        if t in structs:
            return structs[t], t
        m = re.fullmatch(r"(\w+)\.([\w*]+)", t)
        if m:
            st, fd = m.groups()
            hits = sorted(h for (s2, f2), hs in decl.items() if s2 == st and fnmatch.fnmatch(f2, fd) for h in hs)
            return (hits[0] if hits else None), st
        if re.fullmatch(r"[\w*]+", t):
            if ctx:
                hits = sorted(h for (s2, f2), hs in decl.items() if s2 == ctx and fnmatch.fnmatch(f2, t) for h in hs)
                if hits:
                    return hits[0], ctx
            if len(by_field.get(t, [])) == 1:
                return by_field[t][0], ctx
            if t in procs:
                return procs[t], ctx
        m = re.fullmatch(r"([\w.-]+\.(cpp|h|odin)):(\d+)", t)
        if m:
            where = ODIN / "monica" if m.group(2) == "odin" else ODIN.parent / "src"
            hits = sorted(where.rglob(m.group(1)))
            if hits:
                return (os.path.relpath(hits[0], ODIN).replace(os.sep, "/"), int(m.group(3))), ctx
            return None, ctx
        if re.fullmatch(r"[\w-]+\.odin", t):
            hits = sorted((ODIN / "monica").rglob(t))
            return ((hits[0].relative_to(ODIN).as_posix(), None) if hits else None), ctx
        if re.fullmatch(r"[\w-]+\.md", t) and (ODIN / t).exists():
            return (t, None), ctx
        if re.fullmatch(r"[\w-]+", t):  # an output name or JSON key: its first use as a string
            for rel, i, l in code:
                if f'"{t}"' in l:
                    return (rel, i), ctx
        return None, ctx

    inside, ctx = False, None
    for k, l in enumerate(lines):
        if l.startswith("## "):
            inside = l.startswith("## Findings")
            continue
        if not inside or l.startswith("|"):
            continue
        if l.startswith("- "):
            ctx = None

        def sub(m):
            nonlocal ctx
            t = m.group(1) or m.group(2)
            target, ctx = resolve(t, ctx)
            if target is None:
                if re.fullmatch(r"[\w.*/:-]+", t):
                    print(f"{name}: findings: `{t}` not found", file=sys.stderr)
                return f"`{t}`"
            path, line = target
            return f"[`{t}`]({path}{f'#L{line}' if line else ''})"

        lines[k] = TOKEN.sub(sub, l)


def main():
    decl = declarations()
    index = code_index()
    problems = 0
    for name in FILES:
        path = ODIN / name
        if not path.exists():
            continue
        lines = path.read_text(encoding="utf-8").split("\n")
        line_col = None  # index of a "line" column in the current table
        linked = 0
        for k, l in enumerate(lines):
            if l.startswith("| ok |"):
                cols = [c.strip() for c in l.strip("|").split("|")]
                line_col = cols.index("line") if "line" in cols else None
                continue
            m = CELL.match(l)
            if not m:
                continue
            struct, field = m.group(2), m.group(3)
            hits = decl.get((struct, field), [])
            if len(hits) != 1:
                print(f"{name}: {struct}.{field}: {len(hits)} declarations found", file=sys.stderr)
                problems += 1
                lines[k] = f"{m.group(1)}`{struct}.{field}`{m.group(4)}{m.group(5)}"
                continue
            file, line = hits[0]
            row = f"{m.group(1)}[`{struct}.{field}`]({file}#L{line}){m.group(4)}{m.group(5)}"
            if line_col is not None:
                cells = row.split("|")
                cells[line_col + 1] = f" {line} "
                row = "|".join(cells)
            lines[k] = row
            linked += 1
        link_findings(lines, decl, index, name)
        path.write_text("\n".join(lines), encoding="utf-8")
        print(f"{name}: {linked} fields linked")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
