"""Compare the Cap'n Proto server's JSON result against monica-run's CSV.

Both run the same fixture (installer/Hohenfinow2/sim-min.json) through the same
Odin run_monica, so any difference is in the Cap'n Proto plumbing - the env that
reached the server, or the output serialisation - not in the model.

The CSV rounds; the JSON does not. So each JSON value is compared against the CSV
value rounded to the number of decimals the CSV actually printed.

  python compare_with_monica_run.py <ref.csv> <capnp-result.json>
"""

import json
import sys


def read_csv_sections(path):
    """-> {section_name: (header, [row, ...])}, mirroring monica-run's layout:
    a bare quoted section name, then header, then units, then data rows."""
    sections = {}
    name = None
    header = None
    rows = None
    pending_units = False
    with open(path, encoding="utf-8") as f:
        for raw in f:
            line = raw.rstrip("\r\n")
            if not line:
                continue
            parts = line.split(",")
            if len(parts) == 1 and line.startswith('"') and line.endswith('"'):
                if name is not None:
                    sections[name] = (header, rows)
                name = line.strip('"')
                header, rows, pending_units = None, [], False
                continue
            if header is None:
                header = parts
                pending_units = True
                continue
            if pending_units:
                pending_units = False  # units row
                continue
            rows.append(parts)
    if name is not None:
        sections[name] = (header, rows)
    return sections


def decimals(s):
    return len(s.split(".")[1]) if "." in s else 0


def as_csv_text(v, ref):
    """Render a JSON value the way the CSV would have, given the reference cell."""
    if v is None:
        return ""
    if isinstance(v, bool):
        return "1" if v else "0"
    if isinstance(v, (int, float)) and not isinstance(v, bool):
        try:
            float(ref)
        except ValueError:
            return str(v)
        return f"{float(v):.{decimals(ref)}f}"
    return str(v)


def main():
    ref_csv, capnp_json = sys.argv[1], sys.argv[2]
    sections = read_csv_sections(ref_csv)
    result = json.load(open(capnp_json, encoding="utf-8"))

    total_cells = 0
    mismatches = []

    for d in result.get("data", []):
        name = json.loads(d["origSpec"]) if d.get("origSpec", "").startswith('"') else d.get("origSpec")
        if name not in sections:
            mismatches.append(f"section {name!r} present in JSON but not in CSV")
            continue
        header, rows = sections[name]
        cols = d["results"]  # column-oriented: cols[output][row]
        n_rows = len(cols[0]) if cols else 0

        if n_rows != len(rows):
            mismatches.append(f"section {name!r}: {n_rows} JSON rows vs {len(rows)} CSV rows")
            continue

        bad_here = 0
        for r in range(n_rows):
            # One output id can span several CSV columns - a layer range (SOC_1,
            # SOC_2, SOC_3 for a single "SOC" id) shows up as a list in the JSON.
            flat = []
            for c in range(len(cols)):
                v = cols[c][r]
                flat.extend(v if isinstance(v, list) else [v])
            if len(flat) != len(header):
                mismatches.append(
                    f"section {name!r} row {r}: {len(flat)} flattened JSON values"
                    f" vs {len(header)} CSV columns"
                )
                break
            for c, v in enumerate(flat):
                ref = rows[r][c]
                got = as_csv_text(v, ref)
                total_cells += 1
                if got != ref:
                    bad_here += 1
                    if bad_here <= 3:
                        mismatches.append(
                            f"section {name!r} row {r} col {header[c]!r}: CSV {ref!r} vs capnp {got!r}"
                        )
        if bad_here:
            mismatches.append(f"section {name!r}: {bad_here} differing cells")
        else:
            print(f"section {name!r}: {n_rows} rows x {len(header)} cols all match")

    csv_only = set(sections) - {
        json.loads(d["origSpec"]) if d.get("origSpec", "").startswith('"') else d.get("origSpec")
        for d in result.get("data", [])
    }
    for name in sorted(csv_only):
        mismatches.append(f"section {name!r} present in CSV but not in JSON")

    print(f"\ncompared {total_cells} cells")
    if mismatches:
        print("MISMATCHES:")
        for m in mismatches:
            print(f"  {m}")
        sys.exit(1)
    print("OK - Cap'n Proto result is identical to monica-run's output")


main()
