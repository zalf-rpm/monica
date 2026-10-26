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
import re
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


def daily_value(j, day, name):
    """A daily value as monica-module reads it: the column, or its default."""
    if name in day:
        return day[name]
    if name == "air_pressure":
        return 1010.0
    if name == "kc_factor":
        return j.get("KcFactor", 0.0)
    if name == "day_length":
        raise KeyError("day_length must be a column (from-sim writes it)")
    return 0.0


def thickness_cm(m):
    """soil_temperature.odin's thickness_cm"""
    return float(int(m * 100.0 + 0.5))


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

    return params, daily, lambda env, n: (env["soilSurfaceTemperature"], env["soilTemperature"][:n])


def dssat_st_standalone(j, layers):
    """monica/modules/soil_temperature/dssat_st.odin"""
    n = len(layers)
    dlayr = [thickness_cm(l["Thickness"]) for l in layers]
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

    return params, daily, lambda env, n: (env["SRFTEMP"], env["ST"][:n])


def dssat_epicst_standalone(j, layers):
    """monica/modules/soil_temperature/dssat_epicst.odin"""
    n = len(layers)
    dlayr = [thickness_cm(l["Thickness"]) for l in layers]
    ds, depth = [], 0.0
    for x in dlayr:
        depth += x
        ds.append(depth)
    params = {
        "ISWWAT": "Y", "NL": n, "NLAYR": n,
        "DLAYR": array("d", dlayr), "DS": array("d", ds),
        "BD": array("d", [l["BulkDensity"] / 1000.0 for l in layers]),
        "LL": array("d", [l["PermanentWiltingPoint"] for l in layers]),
        "DUL": array("d", [l["FieldCapacity"] for l in layers]),
        "SW": array("d", [l["SoilMoisture"] for l in layers]),
    }

    def daily(day):
        v = lambda name: daily_value(j, day, name)
        return {
            "TMIN": day["tmin"], "TAVG": day["tavg"], "TMAX": day["tmax"], "RAIN": v("precip"),
            "SNOW": v("snow_depth"), "DEPIR": v("irrigation"), "MULCHMASS": 0.0,
            "BIOMAS": v("above_ground_biomass"), "TAV": j["TAV"], "TAMP": j["TAMP"],
            "SW": array("d", daily_soil_moisture(day, layers)),
        }

    return params, daily, lambda env, n: (env["SRFTEMP"], env["ST"][:n])


def apsim_campbell(j, layers):
    """monica/modules/soil_temperature/apsim_campbell.odin"""
    params = {
        "ps": 2.63, "pom": 1.3,
        "soilConstituentNames": ["Rocks", "OrganicMatter", "Sand", "Silt", "Clay", "Water", "Ice", "Air"],
        "weather_Latitude": j["Latitude"],
        "physical_Thickness": array("d", [l["Thickness"] * 1000.0 for l in layers]),
        "physical_BD": array("d", [l["BulkDensity"] / 1000.0 for l in layers]),
        "physical_ParticleSizeSand": array("d", [l["Sand"] * 100.0 for l in layers]),
        "physical_ParticleSizeSilt": array("d", [l["Silt"] * 100.0 for l in layers]),
        "physical_ParticleSizeClay": array("d", [l["Clay"] * 100.0 for l in layers]),
        "physical_Rocks": array("d", [l["Stones"] * 100.0 for l in layers]),
        "organic_Carbon": array("d", [l["OrganicCarbon"] * 100.0 for l in layers]),
    }

    def daily(day):
        v = lambda name: daily_value(j, day, name)
        potential_et = v("et0") * v("kc_factor")
        return {
            "weather_MinT": day["tmin"], "weather_MaxT": day["tmax"], "weather_MeanT": day["tavg"],
            "weather_Radn": day["globrad"], "clock_Today_DayOfYear": julian_day(day["date"]),
            "weather_Wind": v("wind"), "weather_AirPressure": v("air_pressure"),
            "waterBalance_Eo": potential_et, "waterBalance_Eos": potential_et,
            "waterBalance_Es": v("actual_evaporation"), "waterBalance_Salb": j["Albedo"],
            "weather_Tav": j["TAV"], "weather_Amp": j["TAMP"],
            "microClimate_CanopyHeight": v("crop_height") * 1000.0,
            "waterBalance_SW": array("d", daily_soil_moisture(day, layers)),
        }

    return params, daily, lambda env, n: (env["aveSoilTemp"][1], env["aveSoilTemp"][2:2 + n])


def bioma(j, layers, swat_surface):
    """monica/modules/soil_temperature/bioma_parton_swat.odin, bioma_swat_swat.odin"""
    depth = 0.0
    for l in layers:
        depth += l["Thickness"]
    params = {
        "LayerThickness": array("d", [l["Thickness"] for l in layers]),
        "BulkDensity": array("d", [l["BulkDensity"] / 1000.0 for l in layers]),
        "SoilProfileDepth": depth, "LagCoefficient": 0.8, "AirTemperatureAnnualAverage": j["TAV"],
    }

    def daily(day):
        v = lambda name: daily_value(j, day, name)
        d = {
            "AirTemperatureMinimum": day["tmin"], "AirTemperatureMaximum": day["tmax"],
            "GlobalSolarRadiation": day["globrad"], "AboveGroundBiomass": v("above_ground_biomass"),
            "VolumetricWaterContent": array("d", [sm - l["PermanentWiltingPoint"]
                                                  for sm, l in zip(daily_soil_moisture(day, layers), layers)]),
        }
        if swat_surface:
            d.update({"WaterEquivalentOfSnowPack": v("snow_water_equivalent"), "Albedo": j["Albedo"]})
        else:
            d["DayLength"] = v("day_length")
        return d

    return params, daily, lambda env, n: (env["SurfaceSoilTemperature"], env["SoilTemperatureByLayers"][:n])


def simplace(j, layers, first_day):
    """monica/modules/soil_temperature/simplace.odin"""
    n = len(layers)
    depths, depth, bd = [], 0.0, 0.0
    for l in layers:
        depth += l["Thickness"]
        depths.append(depth)
        bd += l["BulkDensity"] / 1000.0
    params = {
        "cInitialAgeOfSnow": 0, "cSnowIsolationFactorA": 2.3, "cSnowIsolationFactorB": 0.22,
        "cInitialSnowWaterContent": daily_value(j, first_day, "snow_water_equivalent"),
        "cAlbedo": j["Albedo"], "cDampingDepth": 6.0, "cCarbonContent": layers[0]["OrganicCarbon"] * 100.0,
        "cAverageBulkDensity": bd / float(n), "cSoilLayerDepth": array("d", depths),
        "cAverageGroundTemperature": j["TAV"], "cFirstDayMeanTemp": first_day["tavg"],
    }

    def daily(day):
        v = lambda name: daily_value(j, day, name)
        water = 0.0
        for sm, l in zip(daily_soil_moisture(day, layers), layers):
            water += sm * 10 * l["Thickness"] * 100
        return {
            "iAirTemperatureMin": day["tmin"], "iAirTemperatureMax": day["tmax"],
            "iGlobalSolarRadiation": day["globrad"], "iRAIN": v("precip"), "iLeafAreaIndex": v("lai"),
            "iPotentialSoilEvaporation": v("et0") * j.get("KcFactor", 0.0),
            "iSoilWaterContent": water, "iCropResidues": 0.0,
        }

    return params, daily, lambda env, n: (env["SoilSurfaceTemperature"], env["SoilTempArray"][:n])


def sq(j, layers):
    """monica/modules/soil_temperature/sq.odin"""
    params = {"a": 0.5, "b": 1.81, "c": 0.49, "lambda_": 2.454}

    def daily(day):
        return {"dayLength": daily_value(j, day, "day_length"), "maxTAir": day["tmax"], "minTAir": day["tmin"],
                "meanTAir": day["tavg"], "meanAnnualAirTemp": j["TAV"], "heatFlux": 0.0}

    def extract(env, n):
        avg = (env["minTSoil"] + env["maxTSoil"]) / 2.0
        return avg, [avg] * n

    return params, daily, extract


def stics(j, layers, first_day):
    """monica/modules/soil_temperature/stics.odin"""
    params = {"layer_thick": [int(thickness_cm(l["Thickness"])) for l in layers],
              "air_temp_day1": first_day["tavg"]}

    def daily(day):
        return {"min_temp": day["tmin"], "max_temp": day["tmax"], "min_canopy_temp": day["tmin"],
                "max_canopy_temp": day["tmax"], "min_air_temp": day["tmin"]}

    return params, daily, lambda env, n: (env["temp_profile"][0], env["layer_temp"][:n])


# name -> (source repository, mapping(j, layers, first day))
MODULES = {
    "Monica_SoilTemp": ("SoilTemperatureModels", lambda j, ls, d0: monica_soiltemp(j, ls)),
    "DSSAT_ST_standalone": ("SoilTemperatureModels", lambda j, ls, d0: dssat_st_standalone(j, ls)),
    "DSSAT_EPICST_standalone": ("SoilTemperatureModels", lambda j, ls, d0: dssat_epicst_standalone(j, ls)),
    "ApsimCampbell": ("SoilTemperatureModels", lambda j, ls, d0: apsim_campbell(j, ls)),
    "BiomaSurfacePartonSoilSWATC": ("SoilTemperatureModels", lambda j, ls, d0: bioma(j, ls, False)),
    "BiomaSurfaceSWATSoilSWATC": ("SoilTemperatureModels", lambda j, ls, d0: bioma(j, ls, True)),
    "Simplace_Soil_Temperature": ("SoilTemperatureModels", simplace),
    "SQ_Soil_Temperature": ("SoilTemperatureModels", lambda j, ls, d0: sq(j, ls)),
    "Stics_soil_temperature": ("SoilTemperatureModels", stics),
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
    params, daily, extract = mapping(j, layers, days[0])

    class MappedInputs(Inputs):
        """PyCrop2ML's deterministic inputs, overridden with the mapped ones.
        A parameter the binding doesn't set gets what the generated Odin's
        make_* gives it: the metadata's default, also where that is an
        arithmetic expression like ApsimCampbell's timestep "24.0 * 60.0 *
        60.0", and zero, empty or "" without one. PyCrop2ML's Inputs would
        invent a value for both."""

        def __init__(self, comp, values):
            self.mapped = values
            super().__init__(comp)
            self.values.update(values)

        def value(self, var, step):
            if var.name in self.mapped:
                return self.mapped[var.name]
            default = str(getattr(var, "default", "") or "").strip()
            if hasattr(var, "parametercategory") and default and var.datatype in ("DOUBLE", "INT"):
                try:
                    float(default)
                except ValueError:
                    if re.fullmatch(r"[0-9.eE+\-*/() ]+", default):
                        v = eval(default, {"__builtins__": {}})
                        return int(v) if var.datatype == "INT" else float(v)
            if hasattr(var, "parametercategory") and not default:
                dt = var.datatype
                if dt.endswith("ARRAY") or dt.endswith("LIST"):
                    return array("d") if dt.startswith("DOUBLE") else []
                return {"DOUBLE": 0.0, "INT": 0, "BOOLEAN": False}.get(dt, "")
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
                surface, layer_temps = extract(env, n)
                values = [surface] + list(layer_temps)
                f.write(day["date"] + "".join("," + format(float(v), ".17g") for v in values) + "\n")
    finally:
        reference.unload()


def dynamic(in_path, out_path):
    """A copy of a monica-module input whose days exercise what a bare-soil
    from-sim input doesn't: a crop (cover, biomass, height, LAI), snow (on 10
    of every 30 days, as tests/soil_temperature_ref does), ET0, evaporation,
    irrigation, and soil moisture changing between the wilting point and
    field capacity. Deterministic."""
    in_path, out_path = Path(in_path), Path(out_path)
    j, layers, days = read_input(in_path)
    csv_path = out_path.with_suffix(".csv")
    j["Daily"] = csv_path.name
    out_path.write_text(json.dumps(j, indent=1))
    kept = [c for c in days[0] if c != "date" and not c.startswith("soil_moisture_")]
    synthetic = ["soil_coverage", "above_ground_biomass", "crop_height", "lai", "snow_depth",
                 "snow_water_equivalent", "temperature_under_snow", "et0", "actual_evaporation", "irrigation"]
    kept = [c for c in kept if c not in synthetic]
    cols = ["date"] + kept + synthetic + [f"soil_moisture_{k + 1}" for k in range(len(layers))]
    g = lambda x: format(x, ".17g")
    with open(csv_path, "w", newline="") as f:
        f.write(",".join(cols) + "\n")
        for i, day in enumerate(days):
            season = 0.5 * (1.0 - math.cos(2.0 * math.pi * julian_day(day["date"]) / 365.0))
            snow = 50.0 if i % 30 < 10 else 0.0
            et0 = 1.0 + 3.0 * season
            row = [day["date"]] + [g(day[c]) for c in kept]
            row += [g(0.9 * season), g(8000.0 * season), g(season), g(5.0 * season), g(snow), g(0.3 * snow),
                    g(-2.0 - i % 5), g(et0), g(0.5 * et0 * (1.0 - 0.9 * season)),
                    "10" if i % 15 == 0 and season > 0.5 else "0"]
            for k, l in enumerate(layers):
                pwp, fc = l["PermanentWiltingPoint"], l["FieldCapacity"]
                row.append(g(pwp + (fc - pwp) * (0.5 + 0.4 * math.sin(0.05 * i + k))))
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
