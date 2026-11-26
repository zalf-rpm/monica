#!/usr/bin/env python3
"""The checks of modules from plugin libraries (plan-module-plugins.md).

    python tools/odinw.py build monica-run monica-module
    python tools/odinw.py build-plugin test_modules test_broken
    python tests/check_plugins.py --build build --work build/check-plugins

tests/plugins/test_modules has two built-in modules again, under other names.
On Hohenfinow2's sim-min, with 17-digit output:

1. As drivers next to their built-in twins: the same outputs every day
   (Plugin_Monica_Reference_Evapotranspiration, Plugin_DSSAT_ST_standalone).
2. Integrated: Plugin_DSSAT_ST_standalone gives byte-identical output to
   DSSAT_ST_standalone (and not MONICA's own), loaded through sim.json's
   Modules.Plugins and through MONICA_MODULE_PLUGINS alike.
3. monica-module runs a plugin module standalone, with the built-in one's
   results.
4. tests/plugins/test_broken is refused module by module: another ABI version,
   an unknown process, another Input layout, a built-in module's name; a
   library that can't be loaded and an unknown plugin module are errors too.

Needs MONICA_PARAMETERS. Standard library only.
"""
import argparse
import csv
import json
import os
import subprocess
import sys
from pathlib import Path

ODIN = Path(__file__).resolve().parent.parent
H2 = ODIN.parent / "installer" / "Hohenfinow2"
WINDOWS = os.name == "nt"
EXE = ".exe" if WINDOWS else ""
LIB = ".dll" if WINDOWS else ".so"
ET_DRIVERS = "modules.evapotranspiration_drivers"
ST_DRIVERS = "modules.soil_temperature_drivers"

failures = []


def check(ok, what):
    print(("  ok    " if ok else "  FAIL  ") + what)
    if not ok:
        failures.append(what)


def sim(work, name, modules, columns):
    """A sim-min variant with these Modules and daily columns."""
    j = json.loads((H2 / "sim-min.json").read_text())
    for key in ("crop.json", "site.json", "climate.csv"):
        j[key] = str(H2 / j[key])
    j["Modules"] = modules
    out = j["output"]
    out["file-name"] = f"{name}.csv"
    out["path-to-output"] = "./"
    out["csv-options"] = {**out.get("csv-options", {}), "precision": 17, "include-units-row": False}
    out["events"] = ["daily", ["Date"] + columns]
    (work / f"{name}.json").write_text(json.dumps(j, indent=1))
    return f"{name}.json"


def run(cmd, work, env=None):
    return subprocess.run([str(c) for c in cmd], cwd=work, capture_output=True, text=True,
                          env={**os.environ, **(env or {})})


def days(path):
    rows = [r for r in csv.reader(open(path, newline="")) if r]
    header = next(r for r in rows if r[0] == "Date")
    return [dict(zip(header, r)) for r in rows if r[0][:1].isdigit()]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--build", required=True, type=Path, help="odin/build, with monica-run, monica-module, plugins/")
    ap.add_argument("--work", required=True, type=Path)
    ns = ap.parse_args()
    if "MONICA_PARAMETERS" not in os.environ:
        sys.exit("MONICA_PARAMETERS is not set")
    build, work = ns.build.resolve(), ns.work.resolve()
    work.mkdir(parents=True, exist_ok=True)
    monica_run, monica_module = build / f"monica-run{EXE}", build / f"monica-module{EXE}"
    good, broken = build / "plugins" / f"test_modules{LIB}", build / "plugins" / f"test_broken{LIB}"

    print("== drivers: plugin == built-in")
    pairs = [(ET_DRIVERS, "Monica_Reference_Evapotranspiration", "reference_evapotranspiration"),
             (ST_DRIVERS, "DSSAT_ST_standalone", "soil_surface_temperature")]
    cols = [{"path": f"{d}.{p}{m}.output.{v}", "name": f"{p}{m}"} for d, m, v in pairs for p in ("", "Plugin_")]
    cols += [{"path": f"{ST_DRIVERS}.{p}DSSAT_ST_standalone.output.layer_temperature", "name": f"{p}layers",
              "layers": [1, 20]} for p in ("", "Plugin_")]
    name = sim(work, "drivers", {
        "EvapotranspirationDrivers": ["Monica_Reference_Evapotranspiration", "Plugin_Monica_Reference_Evapotranspiration"],
        "SoilTemperatureDrivers": ["DSSAT_ST_standalone", "Plugin_DSSAT_ST_standalone"],
        "Plugins": [str(good)]}, cols)
    r = run([monica_run, name], work)
    check(r.returncode == 0, f"drivers run ({r.stdout[-300:]}{r.stderr[-300:]})".strip())
    if r.returncode == 0:
        ds = days(work / "drivers.csv")
        for _, m, v in pairs:
            check(len(ds) > 0 and all(row[m] == row[f"Plugin_{m}"] for row in ds),
                  f"{m} {v}: plugin == built-in, {len(ds)} days")
        layers = [k for k in ds[0] if k.startswith("layers_")] if ds else []
        check(len(layers) > 1 and all(row[k] == row[f"Plugin_{k}"] for row in ds for k in layers),
              f"DSSAT_ST_standalone layer temperatures: plugin == built-in, {len(layers)} layers")

    print("== integrated: plugin == built-in")
    st = ["SurfTemp", "STemp", {"path": "soil_moisture.reference_evapotranspiration", "name": "ET0"}]
    outs = {}
    for tag, modules, env in [
        ("internal", {}, None),
        ("builtin", {"SoilTemperature": {"impl": "DSSAT_ST_standalone"}}, None),
        ("plugin", {"SoilTemperature": {"impl": "Plugin_DSSAT_ST_standalone"}, "Plugins": [str(good)]}, None),
        ("plugin-env", {"SoilTemperature": {"impl": "Plugin_DSSAT_ST_standalone"}}, {"MONICA_MODULE_PLUGINS": str(good)}),
    ]:
        r = run([monica_run, sim(work, f"st-{tag}", modules, st)], work, env)
        check(r.returncode == 0, f"{tag} runs")
        outs[tag] = (work / f"st-{tag}.csv").read_bytes() if r.returncode == 0 else None
    check(outs["plugin"] is not None and outs["plugin"] == outs["builtin"], "Modules.Plugins: byte-identical")
    check(outs["plugin-env"] is not None and outs["plugin-env"] == outs["builtin"], "MONICA_MODULE_PLUGINS: byte-identical")
    check(outs["builtin"] != outs["internal"], "and not MONICA's own soil temperature")

    print("== monica-module standalone")
    inputs = ["tavg", "wind", "wind_speed_height", "vapor_pressure", "saturated_vapor_pressure",
              "stomata_resistance", "net_radiation"]
    cols = [{"path": f"{ET_DRIVERS}.Monica_Reference_Evapotranspiration.input.daily.{v}", "name": v} for v in inputs]
    r = run([monica_run, sim(work, "et-inputs", {"EvapotranspirationDrivers": ["Monica_Reference_Evapotranspiration"]},
                             cols)], work)
    ds = days(work / "et-inputs.csv")
    with open(work / "et-standalone.csv", "w", newline="") as f:
        f.write(",".join(["date"] + inputs) + "\n")
        for row in ds:
            f.write(",".join([row["Date"]] + [row[v] for v in inputs]) + "\n")
    (work / "et-standalone.json").write_text(json.dumps({"HeightNN": 50, "Daily": "et-standalone.csv"}))
    res = {}
    for m in ("Monica_Reference_Evapotranspiration", "Plugin_Monica_Reference_Evapotranspiration"):
        r = run([monica_module, "run", "Evapotranspiration", m, "et-standalone.json", f"{m}.csv"], work,
                {"MONICA_MODULE_PLUGINS": str(good)})
        res[m] = (work / f"{m}.csv").read_bytes() if r.returncode == 0 else None
        check(r.returncode == 0, f"monica-module runs {m}")
    check(None not in res.values() and len(set(res.values())) == 1 and len(ds) > 0,
          f"plugin == built-in standalone, {len(ds)} days")

    print("== refused")
    name = sim(work, "broken", {"SoilTemperatureDrivers": ["Broken_Plugin_Good_Module"], "Plugins": [str(broken)]}, st)
    r = run([monica_run, name], work)
    msg = r.stdout + r.stderr
    check(r.returncode != 0, "a run with a broken plugin fails")
    for what, text in [("another ABI version", "ABI version"), ("an unknown process", 'unknown process "NoSuchProcess"'),
                       ("another layout", '"Broken_Layout" was built against another SoilTemperature interface'),
                       ("a built-in's name", '"DSSAT_ST_standalone" is the name of a built-in module')]:
        check(text in msg, f"{what}: {text}")
    check("Broken_Plugin_Good_Module" not in msg, "the good module of that plugin isn't refused")
    name = sim(work, "missing", {"Plugins": [str(work / f"nonexistent{LIB}")]}, st)
    r = run([monica_run, name], work)
    check(r.returncode != 0 and "can't load it" in r.stdout + r.stderr, "a library that can't be loaded")
    name = sim(work, "unknown", {"SoilTemperatureDrivers": ["Plugin_DSSAT_ST_standalone"]}, st)
    r = run([monica_run, name], work)
    check(r.returncode != 0 and 'unknown module "Plugin_DSSAT_ST_standalone"' in r.stdout + r.stderr,
          "a plugin module without its plugin is unknown")

    print("== FAIL" if failures else "== PASS")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
