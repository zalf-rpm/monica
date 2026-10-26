#!/usr/bin/env python3
"""The checks of the modules in MONICA (plan-crop2ml-modules.md, phases 3, 4).

    python tests/check_drivers.py --monica-run build/monica-run \\
        --monica-module build/monica-module --work build/check-drivers

1. Drivers change nothing: the diff_outputs.sh fixtures (sim-min, -events2,
   -setvalue) and sim, sim+ give byte-identical output with both soil
   temperature modules configured as drivers. Nor does Monica_SoilTemp
   integrated, MONICA's own algorithm on the same inputs. Every other module
   integrated runs all of them without NaN or inf.
2. On sim-min with 17-digit CSV output, through the modules' output paths:
   - the Monica_SoilTemp driver gives exactly MONICA's own soil temperature
     (same algorithm, same inputs);
   - a raw state path (module.Dssat_St.s.srftemp) gives the output's value;
   - both drivers give exactly what monica-module gives standalone on the
     inputs the drivers got: from-sim's per-run values plus the daily values
     reported through the drivers' input paths.
3. Integrated, every module together with itself as a driver (both get the
   same inputs every day): the soil column's temperatures - what the rest of
   MONICA reads - are exactly the driver's.

Needs MONICA_PARAMETERS. Standard library only.
"""
import argparse
import csv
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

ODIN = Path(__file__).resolve().parent.parent
H2 = ODIN.parent / "installer" / "Hohenfinow2"
FIXTURES = ["sim-min", "sim-min-events2", "sim-min-setvalue", "sim", "sim+"]
MODULES = ["Monica_SoilTemp", "DSSAT_ST_standalone"]
DRIVERS = "modules.soil_temperature_drivers"
DAILY_INPUTS = ["julian_day", "tmin", "tavg", "tmax", "globrad", "soil_coverage", "snow_depth",
                "temperature_under_snow"]

failures = []


def check(ok, what):
    print(("  ok    " if ok else "  FAIL  ") + what)
    if not ok:
        failures.append(what)


def run(cmd, cwd):
    r = subprocess.run([str(c) for c in cmd], cwd=cwd, capture_output=True, text=True)
    if r.returncode != 0:
        sys.exit(f"{' '.join(map(str, cmd))} failed:\n{r.stdout}{r.stderr}")
    return r.stdout


def run_variant(monica_run, work, variant, modules):
    """The fixtures with `modules` as sim.json's "Modules" (None: none), in a
    copy of Hohenfinow2; output files plus stdout, by name."""
    d = work / variant
    shutil.rmtree(d, ignore_errors=True)
    shutil.copytree(H2, d)
    before = set(p.name for p in d.iterdir())
    for sim in FIXTURES:
        if modules is not None:
            j = json.loads((d / f"{sim}.json").read_text())
            j["Modules"] = modules
            (d / f"{sim}.json").write_text(json.dumps(j, indent=1))
        (d / f"{sim}.stdout").write_text(run([monica_run, f"{sim}.json"], d))
    return {p.name: p.read_bytes() for p in d.iterdir() if p.name not in before}


def changes_nothing(monica_run, work):
    print("== drivers, and Monica_SoilTemp integrated, change nothing")
    plain = run_variant(monica_run, work, "plain", None)
    for variant, modules in (("drivers", {"SoilTemperatureDrivers": MODULES}),
                             ("integrated", {"SoilTemperature": {"impl": "Monica_SoilTemp"}})):
        outs = run_variant(monica_run, work, variant, modules)
        same = outs.keys() == plain.keys() and all(outs[k] == plain[k] for k in plain)
        check(same, f"{variant}: {len(plain)} outputs of {', '.join(FIXTURES)} byte-identical")
    print("== other modules integrated run everything")
    for m in MODULES[1:]:
        outs = run_variant(monica_run, work, f"integrated-{m}", {"SoilTemperature": {"impl": m}})
        bad = [k for k, v in outs.items() if re.search(rb"\b(-?nan|-?inf)\b", v, re.I)]
        check(outs.keys() == plain.keys() and not bad,
              f"{m} integrated: {', '.join(FIXTURES)} run, no NaN/inf" + (f" (in {bad})" if bad else ""))


def driver_sim(work, name="drivers", modules=None, drivers=MODULES):
    """sim-min with drivers and the columns the checks compare."""
    j = json.loads((H2 / "sim-min.json").read_text())
    for key in ("crop.json", "site.json", "climate.csv"):
        j[key] = str(H2 / j[key])
    j["Modules"] = {"SoilTemperatureDrivers": drivers, **(modules or {})}
    out = j["output"]
    out["file-name"] = f"{name}.csv"
    out["path-to-output"] = "./"
    out["csv-options"] = {**out.get("csv-options", {}), "precision": 17, "include-units-row": False}
    layers = [1, 20]
    cols = ["Date",
            {"path": "soil_column.layers.soil_temperature", "name": "STemp", "layers": layers},
            {"path": "soil_column.soil_surface_temperature", "name": "SurfTemp"}]
    for m in drivers:
        cols += [{"path": f"{DRIVERS}.{m}.output.layer_temperature", "name": f"{m}.T", "layers": layers},
                 {"path": f"{DRIVERS}.{m}.output.soil_surface_temperature", "name": f"{m}.Surf"},
                 {"path": f"{DRIVERS}.{m}.input.soil_moisture", "name": f"{m}.SM", "layers": layers}]
        cols += [{"path": f"{DRIVERS}.{m}.input.{n}", "name": f"{m}.{n}"} for n in DAILY_INPUTS]
    if "DSSAT_ST_standalone" in drivers:
        cols.append({"path": f"{DRIVERS}.DSSAT_ST_standalone.module.Dssat_St.s.srftemp", "name": "DSSAT.raw"})
        cols += [{"path": f"{DRIVERS}.DSSAT_ST_standalone.input.{n}", "name": f"input.{n}"}
                 for n in ("tav", "tamp")]
    out["events"] = ["daily", cols]
    (work / f"{name}.json").write_text(json.dumps(j, indent=1))


def read_section(path):
    rows = list(csv.reader(open(path, newline="")))
    header = rows[1]  # rows[0] is the section's name
    return [dict(zip(header, r)) for r in rows[2:] if r and r[0] and not r[0].startswith("[")]


def columns(row, name, n):
    return [row[f"{name}_{i}"] for i in range(1, n + 1)]


def drivers_against_monica_and_standalone(monica_run, monica_module, work):
    print("== drivers")
    driver_sim(work)
    run([monica_run, "drivers.json"], work)
    days = read_section(work / "drivers.csv")
    n = 20
    same = all(columns(r, "Monica_SoilTemp.T", n) == columns(r, "STemp", n)
               and r["Monica_SoilTemp.Surf"] == r["SurfTemp"] for r in days)
    check(same, f"Monica_SoilTemp driver == MONICA's own soil temperature, {len(days)} days")
    check(all(r["DSSAT.raw"] == r["DSSAT_ST_standalone.Surf"] for r in days),
          "raw state path module.Dssat_St.s.srftemp == output.soil_surface_temperature")

    # standalone on what the drivers got
    run([monica_module, "from-sim", H2 / "sim-min.json", "standalone.json"], work)
    inp = json.loads((work / "standalone.json").read_text())
    check(float(days[0]["input.tav"]) == inp["TAV"] and float(days[0]["input.tamp"]) == inp["TAMP"],
          "drivers' TAV/TAMP == from-sim's")
    for m in MODULES:
        p = f"{m}."
        with open(work / f"standalone-{m}.csv", "w", newline="") as f:
            f.write("date,tmin,tavg,tmax,globrad,soil_coverage,snow_depth,temperature_under_snow,"
                    + ",".join(f"soil_moisture_{i}" for i in range(1, n + 1)) + "\n")
            for r in days:
                f.write(",".join([r["Date"]] + [r[p + c] for c in DAILY_INPUTS[1:]]
                                 + columns(r, p + "SM", n)) + "\n")
        inp["Daily"] = f"standalone-{m}.csv"
        (work / f"standalone-{m}.json").write_text(json.dumps(inp, indent=1))
        run([monica_module, "run", "SoilTemperature", m, f"standalone-{m}.json", f"out-{m}.csv"], work)
        out = list(csv.reader(open(work / f"out-{m}.csv", newline="")))[1:]
        same = len(out) == len(days) and all(
            o[0] == r["Date"] and o[1] == r[p + "Surf"] and o[2:] == columns(r, p + "T", n)
            for o, r in zip(out, days))
        check(same, f"{m} driver == monica-module standalone on the drivers' inputs")


def integrated(monica_run, work):
    print("== integrated")
    n = 20
    for m in MODULES:
        name = f"integrated-{m}-driver"
        driver_sim(work, name, {"SoilTemperature": {"impl": m}}, [m])
        run([monica_run, f"{name}.json"], work)
        days = read_section(work / f"{name}.csv")
        same = len(days) > 0 and all(columns(r, f"{m}.T", n) == columns(r, "STemp", n)
                                     and r[f"{m}.Surf"] == r["SurfTemp"] for r in days)
        check(same, f"{m} integrated: soil column == its driver's output, {len(days)} days")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--monica-run", required=True, type=Path)
    ap.add_argument("--monica-module", required=True, type=Path)
    ap.add_argument("--work", required=True, type=Path)
    ns = ap.parse_args()
    if "MONICA_PARAMETERS" not in os.environ:
        sys.exit("MONICA_PARAMETERS is not set")
    work = ns.work.resolve()
    work.mkdir(parents=True, exist_ok=True)
    changes_nothing(ns.monica_run.resolve(), work)
    drivers_against_monica_and_standalone(ns.monica_run.resolve(), ns.monica_module.resolve(), work)
    integrated(ns.monica_run.resolve(), work)
    print("== FAIL" if failures else "== PASS")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
