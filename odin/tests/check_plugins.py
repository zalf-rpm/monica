#!/usr/bin/env python3
"""The checks of modules from plugin libraries (plan-module-plugins.md).

    python tools/odinw.py build monica-run monica-module
    python tools/odinw.py build-plugin test_modules test_broken amei
    python tests/check_plugins.py --build build --work build/check-plugins

tests/plugins/test_modules has MONICA's two built-in modules again, under
other names (Plugin_Monica_SoilTemp, Plugin_Monica_Evapotranspiration). On
Hohenfinow2's sim-min, with 17-digit output:

1. As drivers next to their built-in twins: the same outputs every day.
2. Integrated: byte-identical output to the built-in twin integrated, loaded
   through sim.json's Modules.Plugins and through MONICA_MODULE_PLUGINS alike.
3. Paths into a module's own state (module.Monica_Soil_Temp.s...., a layer
   dimension, an int, the unit from the plugin's tag) read the same in the
   plugin module as in the built-in one; a wrong field is the plugin's error.
4. monica-module runs a plugin module standalone, with the built-in one's
   results.
5. plugins/amei: its modules run (integrated and as drivers) with the plugin
   and are unknown without it; a path without extension finds the library; the
   same library under two paths (environment and sim.json) is fine.
6. tests/plugins/test_broken is refused module by module: another ABI version,
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


def sim(work, name, modules, columns, units=False):
    """A sim-min variant with these Modules and daily columns."""
    j = json.loads((H2 / "sim-min.json").read_text())
    for key in ("crop.json", "site.json", "climate.csv"):
        j[key] = str(H2 / j[key])
    j["Modules"] = modules
    out = j["output"]
    out["file-name"] = f"{name}.csv"
    out["path-to-output"] = "./"
    out["csv-options"] = {**out.get("csv-options", {}), "precision": 17, "include-units-row": units}
    out["events"] = ["daily", ["Date"] + columns]
    (work / f"{name}.json").write_text(json.dumps(j, indent=1))
    return f"{name}.json"


def run(cmd, work, env=None):
    return subprocess.run([str(c) for c in cmd], cwd=work, capture_output=True, text=True,
                          env={**os.environ, **(env or {})})


def table(path):
    """Header, units row (or None) and the daily rows as dicts."""
    rows = [r for r in csv.reader(open(path, newline="", encoding="utf-8")) if r]
    hi = next(i for i, r in enumerate(rows) if r[0] == "Date")
    header = rows[hi]
    units = rows[hi + 1] if hi + 1 < len(rows) and not rows[hi + 1][0][:1].isdigit() else None
    return header, units, [dict(zip(header, r)) for r in rows[hi + 1:] if r[0][:1].isdigit()]


def same_twins(ds, names):
    return len(ds) > 0 and all(row[k] == row[f"Plugin_{k}"] for row in ds for k in names)


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
    plugins = build / "plugins"
    good, broken, amei = plugins / f"test_modules{LIB}", plugins / f"test_broken{LIB}", plugins / f"amei{LIB}"

    print("== drivers: plugin == built-in")
    st, et = "Monica_SoilTemp", "Monica_Evapotranspiration"
    cols = []
    for p in ("", "Plugin_"):
        cols += [{"path": f"{ST_DRIVERS}.{p}{st}.output.soil_surface_temperature", "name": f"{p}surface"},
                 {"path": f"{ST_DRIVERS}.{p}{st}.output.layer_temperature", "name": f"{p}T", "layers": [1, 20]},
                 {"path": f"{ET_DRIVERS}.{p}{et}.output.reference_evapotranspiration", "name": f"{p}ET0"},
                 {"path": f"{ET_DRIVERS}.{p}{et}.output.actual_evapotranspiration", "name": f"{p}ETa"},
                 {"path": f"{ET_DRIVERS}.{p}{et}.output.soil_moisture", "name": f"{p}SM", "layers": [1, 20]}]
    name = sim(work, "drivers", {"SoilTemperatureDrivers": [st, f"Plugin_{st}"],
                                 "EvapotranspirationDrivers": [et, f"Plugin_{et}"], "Plugins": [str(good)]}, cols)
    r = run([monica_run, name], work)
    check(r.returncode == 0, f"drivers run ({r.stdout[-300:]}{r.stderr[-300:]})".strip())
    if r.returncode == 0:
        header, _, ds = table(work / "drivers.csv")
        names = [k for k in header if k != "Date" and not k.startswith("Plugin_")]
        check(len(names) == 43 and same_twins(ds, names),
              f"plugin == built-in: {len(names)} columns (surface, 20 layer temperatures, ET0, ETa, 20 soil "
              f"moistures), {len(ds)} days")

    print("== integrated: plugin == built-in")
    out_cols = ["SurfTemp", "STemp", "Mois", "ETa/ETc",
                {"path": "soil_moisture.reference_evapotranspiration", "name": "ET0"}]
    outs = {}
    for tag, modules, env in [
        ("builtin", {"SoilTemperature": {"impl": st}, "Evapotranspiration": {"impl": et}}, None),
        ("plugin", {"SoilTemperature": {"impl": f"Plugin_{st}"}, "Evapotranspiration": {"impl": f"Plugin_{et}"},
                    "Plugins": [str(good)]}, None),
        ("plugin-env", {"SoilTemperature": {"impl": f"Plugin_{st}"}, "Evapotranspiration": {"impl": f"Plugin_{et}"}},
         {"MONICA_MODULE_PLUGINS": str(good)}),
    ]:
        r = run([monica_run, sim(work, f"integrated-{tag}", modules, out_cols)], work, env)
        check(r.returncode == 0, f"{tag} runs")
        outs[tag] = (work / f"integrated-{tag}.csv").read_bytes() if r.returncode == 0 else None
    check(outs["plugin"] is not None and outs["plugin"] == outs["builtin"], "Modules.Plugins: byte-identical")
    check(outs["plugin-env"] is not None and outs["plugin-env"] == outs["builtin"],
          "MONICA_MODULE_PLUGINS: byte-identical")

    print("== paths into a module's state")
    state = [("s.soil_surface_temperature", None), ("s.soil_temperature", [1, 10]), ("no_of_layers", None)]
    cols = []
    for p in ("", "Plugin_"):
        for f, layers in state:
            c = {"path": f"{ST_DRIVERS}.{p}{st}.module.Monica_Soil_Temp.{f}", "name": f"{p}{f.split('.')[-1]}"}
            if layers:
                c["layers"] = layers
            cols.append(c)
    name = sim(work, "state", {"SoilTemperatureDrivers": [st, f"Plugin_{st}"], "Plugins": [str(good)]}, cols,
               units=True)
    r = run([monica_run, name], work)
    check(r.returncode == 0, f"state paths run ({r.stdout[-300:]}{r.stderr[-300:]})".strip())
    if r.returncode == 0:
        header, units, ds = table(work / "state.csv")
        names = [k for k in header if k != "Date" and not k.startswith("Plugin_")]
        check(len(names) == 12 and same_twins(ds, names),
              f"plugin == built-in: {len(names)} state columns (surface, 10 layers, an int), {len(ds)} days")
        check(any(float(row["soil_surface_temperature"]) != 0 for row in ds), "the state values aren't all 0")
        unit = dict(zip(header, units or []))
        check(unit.get("Plugin_soil_surface_temperature") == "[°C]" == unit.get("soil_surface_temperature"),
              "the plugin's unit ([°C])")
    name = sim(work, "state-bad", {"SoilTemperatureDrivers": [f"Plugin_{st}"], "Plugins": [str(good)]},
               [{"path": f"{ST_DRIVERS}.Plugin_{st}.module.Monica_Soil_Temp.s.no_such_field", "name": "x"}])
    r = run([monica_run, name], work)
    msg = r.stdout + r.stderr
    check("no_such_field" in msg and "Soil_Temperature_Comp_State" in msg, "a wrong field is reported (by the plugin)")

    print("== monica-module standalone")
    r = run([monica_module, "from-sim", H2 / "sim-min.json", "st-input.json"], work)
    check(r.returncode == 0, f"monica-module from-sim ({r.stdout[-300:]}{r.stderr[-300:]})".strip())
    res = {}
    for m in (st, f"Plugin_{st}"):
        r = run([monica_module, "run", "SoilTemperature", m, "st-input.json", f"{m}.csv"], work,
                {"MONICA_MODULE_PLUGINS": str(good)})
        res[m] = (work / f"{m}.csv").read_bytes() if r.returncode == 0 else None
        check(r.returncode == 0, f"monica-module runs {m}")
    check(None not in res.values() and len(set(res.values())) == 1, "plugin == built-in standalone")

    print("== plugins/amei")
    amei_noext = str(amei)[: -len(LIB)]
    name = sim(work, "amei", {"SoilTemperature": {"impl": "DSSAT_ST_standalone"},
                              "SoilTemperatureDrivers": ["ApsimCampbell", "Stics_soil_temperature"],
                              "EvapotranspirationDrivers": ["Monica_Reference_Evapotranspiration"],
                              "Plugins": [amei_noext]},
               ["SurfTemp", {"path": f"{ET_DRIVERS}.Monica_Reference_Evapotranspiration.output."
                                     "reference_evapotranspiration", "name": "ET0"}])
    r = run([monica_run, name], work)
    check(r.returncode == 0, f"AMEI modules run from the plugin, path without extension "
                             f"({r.stdout[-300:]}{r.stderr[-300:]})".strip())
    r = run([monica_run, "amei.json"], work, {"MONICA_MODULE_PLUGINS": str(plugins / ".." / "plugins" / f"amei{LIB}")})
    check(r.returncode == 0, "the same library through MONICA_MODULE_PLUGINS and sim.json, under other paths")
    name = sim(work, "amei-missing", {"SoilTemperature": {"impl": "DSSAT_ST_standalone"}}, ["SurfTemp"])
    r = run([monica_run, name], work)
    check(r.returncode != 0 and 'unknown impl "DSSAT_ST_standalone"' in r.stdout + r.stderr,
          "without the plugin, its modules are unknown")

    print("== refused")
    name = sim(work, "broken", {"SoilTemperatureDrivers": ["Broken_Plugin_Good_Module"], "Plugins": [str(broken)]},
               ["SurfTemp"])
    r = run([monica_run, name], work)
    msg = r.stdout + r.stderr
    check(r.returncode != 0, "a run with a broken plugin fails")
    for what, text in [("another ABI version", "ABI version"), ("an unknown process", 'unknown process "NoSuchProcess"'),
                       ("another layout", '"Broken_Layout" was built against another SoilTemperature interface'),
                       ("a built-in's name", '"Monica_SoilTemp" is the name of a built-in module')]:
        check(text in msg, f"{what}: {text}")
    check("Broken_Plugin_Good_Module" not in msg, "the good module of that plugin isn't refused")
    name = sim(work, "missing", {"Plugins": [str(work / f"nonexistent{LIB}")]}, ["SurfTemp"])
    r = run([monica_run, name], work)
    check(r.returncode != 0 and "can't load it" in r.stdout + r.stderr, "a library that can't be loaded")
    name = sim(work, "unknown", {"SoilTemperatureDrivers": [f"Plugin_{st}"]}, ["SurfTemp"])
    r = run([monica_run, name], work)
    check(r.returncode != 0 and f'unknown module "Plugin_{st}"' in r.stdout + r.stderr,
          "a plugin module without its plugin is unknown")

    print("== FAIL" if failures else "== PASS")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
