#!/usr/bin/env python3
"""Run a Crop2ML package's generated *Python* code on a monica-module input.

The reference for cmd/monica-module (plan-crop2ml-modules.md, phase 2): the
same input file, the same mapping to the module's variables as the Odin
binding (monica/modules/soil_temperature/<module>.odin), and the same CSV
format, so the two outputs can be compared number by number.

The generated Python is driven through PyCrop2ML's own test reference
(test/cyml/odin_reference.py, PythonReference), which works around the Python
target's known bugs - see its docstring. Both come from `pixi run gen-amei`:

    pixi run gen-amei
    pixi run --manifest-path build/crop2ml/PyCrop2ML/pyproject.toml \\
        python tools/module_reference.py Monica_SoilTemp input.json out.csv

    tools/module_reference.py --compare a.csv b.csv   # any Python 3
    tools/module_reference.py --dynamic in.json out.json
"""
import argparse
import csv
import json
import math
import sys
from array import array
from datetime import date
from pathlib import Path

ODIN = Path(__file__).resolve().parent.parent
CROP2ML = ODIN / "build" / "crop2ml"


def read_input(path):
    path = Path(path)
    j = json.loads(path.read_text())
    layers = j["Layers"]
    days = []
    with open(path.parent / j["Daily"], newline="") as f:
        for row in csv.DictReader(f):
            day = {k: float(v) for k, v in row.items() if k != "date"}
            day["date"] = row["date"]
            days.append(day)
    return j, layers, days


def daily_soil_moisture(day, layers):
    if "soil_moisture_1" in day:
        return [day[f"soil_moisture_{i + 1}"] for i in range(len(layers))]
    return [l["SoilMoisture"] for l in layers]


def julian_day(iso):
    return date.fromisoformat(iso).timetuple().tm_yday


# --- the mappings, one per module: as in the Odin bindings -------------------

def monica_soiltemp(j, layers):
    """monica/modules/soil_temperature/monica_soiltemp.odin"""
    ps = {"NTau": 0, "InitialSurfaceTemperature": 0, "BaseTemperature": 0, "QuartzRawDensity": 0,
          "DensityAir": 0, "DensityWater": 0, "DensityHumus": 0, "SpecificHeatCapacityAir": 0,
          "SpecificHeatCapacityQuartz": 0, "SpecificHeatCapacityWater": 0,
          "SpecificHeatCapacityHumus": 0, "SoilMoisture": 0.25}
    ps.update(j.get("SoilTemperatureParameters", {}))
    n = len(layers)
    lts = [l["Thickness"] for l in layers]
    params = {
        "timeStep": float(j.get("TimeStep", 1.0)),
        "baseTemp": ps["BaseTemperature"],
        "initialSurfaceTemp": ps["InitialSurfaceTemperature"],
        "densityAir": ps["DensityAir"],
        "specificHeatCapacityAir": ps["SpecificHeatCapacityAir"],
        "densityHumus": ps["DensityHumus"],
        "specificHeatCapacityHumus": ps["SpecificHeatCapacityHumus"],
        "densityWater": ps["DensityWater"],
        "specificHeatCapacityWater": ps["SpecificHeatCapacityWater"],
        "quartzRawDensity": ps["QuartzRawDensity"],
        "specificHeatCapacityQuartz": ps["SpecificHeatCapacityQuartz"],
        "nTau": ps["NTau"],
        "dampingFactor": 0.8,
        "noOfSoilLayers": n,
        "noOfTempLayers": n + 2,
        "noOfTempLayersPlus1": n + 3,
        "layerThickness": array("d", lts + [lts[-1], lts[-1]]),
        "soilMoistureConst": array("d", [ps["SoilMoisture"]] * n),
        "soilBulkDensity": array("d", [l["BulkDensity"] for l in layers]),
        "saturation": array("d", [l["Saturation"] for l in layers]),
        "soilOrganicMatter": array("d", [l["OrganicMatter"] for l in layers]),
    }

    def daily(day):
        return {
            "tmin": day["tmin"], "tmax": day["tmax"], "globrad": day["globrad"],
            "soilCoverage": day.get("soil_coverage", 0.0),
            "hasSnowCover": day.get("snow_depth", 0.0) > 0.0,
            "soilSurfaceTemperatureBelowSnow": day.get("temperature_under_snow", 0.0),
        }

    return params, daily, ("soilSurfaceTemperature", "soilTemperature")


def dssat_st_standalone(j, layers):
    """monica/modules/soil_temperature/dssat_st.odin"""
    n = len(layers)
    dlayr = [float(round(l["Thickness"] * 100.0)) for l in layers]
    ds, depth = [], 0.0
    for x in dlayr:
        depth += x
        ds.append(depth)
    params = {
        "ISWWAT": "Y", "NL": n, "NLAYR": n, "XLAT": j["Latitude"], "MSALB": j["Albedo"],
        "DLAYR": array("d", dlayr), "DS": array("d", ds),
        "BD": array("d", [l["BulkDensity"] / 1000.0 for l in layers]),
        "LL": array("d", [l["PermanentWiltingPoint"] for l in layers]),
        "DUL": array("d", [l["FieldCapacity"] for l in layers]),
        "SW": array("d", [l["SoilMoisture"] for l in layers]),
    }

    def daily(day):
        return {
            "DOY": julian_day(day["date"]), "SRAD": day["globrad"], "TAVG": day["tavg"],
            "TMAX": day["tmax"], "TAV": j["TAV"], "TAMP": j["TAMP"],
            "SW": array("d", daily_soil_moisture(day, layers)),
        }

    return params, daily, ("SRFTEMP", "ST")


MODULES = {
    "Monica_SoilTemp": ("SoilTemperatureModels", monica_soiltemp),
    "DSSAT_ST_standalone": ("SoilTemperatureModels", dssat_st_standalone),
}


def run(module, input_path, out_path):
    sys.path.insert(0, str(CROP2ML / "PyCrop2ML" / "test" / "cyml"))
    from odin_reference import Inputs, PythonReference
    from pycropml.topology import Topology

    repo, mapping = MODULES[module]
    package = CROP2ML / "work" / repo / module
    if not (package / "src" / "py" / module).is_dir():
        sys.exit(f"{package}/src/py not found - run `pixi run gen-amei` first")
    comp = Topology(module, package).model

    j, layers, days = read_input(input_path)
    params, daily, (surface, layer_temps) = mapping(j, layers)

    class MappedInputs(Inputs):
        """PyCrop2ML's deterministic inputs, overridden with the mapped ones."""

        def __init__(self, comp, values):
            self.mapped = values
            super().__init__(comp)
            self.values.update(values)

        def value(self, var, step):
            if var.name in self.mapped:
                return self.mapped[var.name]
            return super().value(var, step)

    first = {**params, **daily(days[0])}
    reference = PythonReference(package, comp, MappedInputs(comp, first), [])
    try:
        env = reference.env
        if reference.init_body is not None:
            reference.run_body(reference.init_body)
        n = len(layers)
        with open(out_path, "w", newline="") as f:
            f.write("date,soil_surface_temperature" + "".join(f",layer_{i + 1}" for i in range(n)) + "\n")
            for day in days:
                env.update(daily(day))
                reference.run_body(reference.model_body)
                values = [env[surface]] + list(env[layer_temps])[:n]
                f.write(day["date"] + "".join("," + format(float(v), ".17g") for v in values) + "\n")
    finally:
        reference.unload()


def dynamic(in_path, out_path):
    """A copy of a monica-module input whose days exercise what a bare-soil
    from-sim input doesn't: a crop cover, snow (on 10 of every 30 days, as
    tests/soil_temperature_ref does) and soil moisture changing between the
    wilting point and field capacity. Deterministic."""
    in_path, out_path = Path(in_path), Path(out_path)
    j, layers, days = read_input(in_path)
    csv_path = out_path.with_suffix(".csv")
    j["Daily"] = csv_path.name
    out_path.write_text(json.dumps(j, indent=1))
    cols = ["date", "tmin", "tavg", "tmax", "globrad", "soil_coverage", "snow_depth",
            "temperature_under_snow"] + [f"soil_moisture_{k + 1}" for k in range(len(layers))]
    with open(csv_path, "w", newline="") as f:
        f.write(",".join(cols) + "\n")
        for i, day in enumerate(days):
            doy = julian_day(day["date"])
            row = [day["date"]] + [format(day[c], ".17g") for c in ("tmin", "tavg", "tmax", "globrad")]
            row += [format(0.45 * (1.0 - math.cos(2.0 * math.pi * doy / 365.0)), ".17g"),
                    "50" if i % 30 < 10 else "0", format(-2.0 - i % 5, ".17g")]
            for k, l in enumerate(layers):
                pwp, fc = l["PermanentWiltingPoint"], l["FieldCapacity"]
                row.append(format(pwp + (fc - pwp) * (0.5 + 0.4 * math.sin(0.05 * i + k)), ".17g"))
            f.write(",".join(row) + "\n")


def compare(a_path, b_path, tolerance):
    """Number by number: identical, within `tolerance` (relative), or not."""
    with open(a_path) as fa, open(b_path) as fb:
        a, b = list(csv.reader(fa)), list(csv.reader(fb))
    if a[0] != b[0] or len(a) != len(b):
        print(f"different shape: {len(a)} vs {len(b)} rows, header {a[0] == b[0]}")
        return 1
    identical = close = 0
    worst = (0.0, None)
    for ra, rb in zip(a[1:], b[1:]):
        if ra[0] != rb[0]:
            print(f"different dates: {ra[0]} vs {rb[0]}")
            return 1
        for col, x, y in zip(a[0][1:], ra[1:], rb[1:]):
            x, y = float(x), float(y)
            if x == y or (math.isnan(x) and math.isnan(y)):
                identical += 1
                continue
            rel = abs(x - y) / max(abs(x), abs(y))
            close += rel <= tolerance
            if rel > worst[0]:
                worst = (rel, f"{ra[0]} {col}: {x!r} vs {y!r}")
    total = (len(a) - 1) * (len(a[0]) - 1)
    differ = total - identical - close
    print(f"{total} values: {identical} identical, {close} within {tolerance:g}, {differ} differ"
          + (f"; largest relative difference {worst[0]:.3g} ({worst[1]})" if worst[1] else ""))
    return 1 if differ else 0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--compare", nargs=2, metavar="CSV", help="compare two outputs instead")
    ap.add_argument("--dynamic", nargs=2, metavar="JSON", help="write a dynamic variant of an input")
    ap.add_argument("--tolerance", type=float, default=1e-12, help="relative, for --compare")
    ap.add_argument("args", nargs="*", help="MODULE INPUT.json OUT.csv")
    ns = ap.parse_args()
    if ns.compare:
        return compare(*ns.compare, ns.tolerance)
    if ns.dynamic:
        return dynamic(*ns.dynamic)
    if len(ns.args) != 3 or ns.args[0] not in MODULES:
        ap.error(f"expected MODULE INPUT.json OUT.csv, MODULE one of {', '.join(MODULES)}")
    run(*ns.args)
    return 0


if __name__ == "__main__":
    sys.exit(main())
