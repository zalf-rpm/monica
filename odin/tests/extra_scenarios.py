#!/usr/bin/env python3
"""Scenarios the diff_outputs.sh fixtures don't cover, byte-compared between a
reference binary and the current one (plan-component-interfaces.md, gate 6).

    pixi run python tests/extra_scenarios.py --save --reference <old monica-run>
        writes the scenarios (sim/crop/site/climate) under the work directory
        and runs the reference binary into <scenario>/base/
    pixi run python tests/extra_scenarios.py
        runs odin/build/monica-run into <scenario>/cur/ and compares every CSV
        with base/; exit status 1 if any differs

The scenarios exercise paths a refactoring can break without the fixtures
noticing: automatic irrigation (workstep and model-level), FAO-56 dual Kc,
output sections keyed to every crop event, N-min fertilising, the hourly FvCB
photosynthesis with O3 and VOC, a Transplant workstep, perennials with cuttings
(one reducing the max assimilation rate) - all on the Hohenfinow2 climate,
built with the MCP server's scenario builders (odin/mcp/monica_jobs.py).

Typical use: build the commit before a change once, keep its monica-run as
the reference, --save; then run without --save after every step. Needs
MONICA_PARAMETERS (default ~/GitHub/monica-parameters).
"""
import argparse
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

ODIN = Path(__file__).resolve().parent.parent
REPO = ODIN.parent
sys.path.insert(0, str(ODIN / "mcp"))
import monica_jobs as mj  # noqa: E402

EXE = ".exe" if os.name == "nt" else ""
EXTRA = ["Mois", "Irrig", "NLeach", "Recharge", "RunOff", "Kc", "LAI", "AbBiom", "PASW"]


def ww(kg):
    return {"species": "wheat", "cultivar": "winter-wheat", "sowing_date": "09-22",
            "latest_harvest_date": "09-05",
            "n_fertilization": [{"date": "03-15", "kg_n_ha": kg * 0.5}, {"date": "04-20", "kg_n_ha": kg * 0.5}]}


def maize():
    return {"species": "maize", "cultivar": "silage-maize", "sowing_date": "04-25",
            "latest_harvest_date": "10-10", "n_fertilization": [{"date": "05-10", "kg_n_ha": 120}]}


def transplanted(crop_spec):
    crop, _ = mj.build_crop_json([crop_spec], False)
    ws = crop["cropRotation"][0]["worksteps"]
    sow = next(w for w in ws if w["type"] == "Sowing")
    sow.update({"type": "Transplant", "initialStage": 2, "initialTemperatureSum": 200,
                "initialRootBiomass": 50, "initialLeafBiomass": 80, "initialShootBiomass": 60,
                "initialLAI": 0.4, "postTransplantDelay": 7})
    return crop


def perennial_crop(species, cut_max_assimilation=None):
    crop = {"species": ["include-from-file", f"crops/{species}.json"],
            "cultivar": ["include-from-file", f"crops/{species}/.json"]}
    residue = ["include-from-file", f"crop-residues/{species}.json"]
    crops = {"P": {"is-winter-crop": False, "is-perennial-crop": True, "cropParams": crop, "residueParams": residue}}
    ws = [{"date": "1991-04-15", "type": "Sowing", "crop": ["ref", "crops", "P"]}]
    for y in range(1991, 1997):
        for md in ("06-05", "08-20"):
            ws.append({"date": f"{y}-{md}", "type": "Cutting",
                       "organs": {"Leaf": [85, "%", "cut"], "Shoot": [85, "%", "cut"]}, "export": True,
                       **({"cut-max-assimilation-rate": [cut_max_assimilation, "%"]} if cut_max_assimilation else {})})
        ws.append({"date": f"{y}-03-20", "type": "MineralFertilization",
                   "amount": [60, "kg N"], "partition": ["include-from-file", "mineral-fertilisers/AN.json"]})
    ws.append({"date": "1996-10-15", "type": "Harvest"})
    ws.sort(key=lambda w: w["date"])
    return {"crops": crops, "cropRotation": [{"worksteps": ws}],
            "CropParameters": {"=": ["include-from-file", "general/crop.json"],
                               "__enable_vernalisation_factor_fix__": True}}


def scenarios():
    auto = {"amount": 20, "threshold": 0.5, "nitrateConcentration": 0, "sulfateConcentration": 0}
    return {
        # AutomaticIrrigation workstep (the MCP builder's auto_irrigation)
        "ww_irrig_workstep": dict(rotation=[ww(160)], auto_irrigation=True, sim={}),
        # model-level automatic irrigation in the crop step
        "ww_irrig_model": dict(rotation=[ww(160), maize()], auto_irrigation=False,
                               sim={"UseAutomaticIrrigation": True, "AutoIrrigationParams": auto}),
        # FAO-56 dual Kc
        "dual_kc": dict(rotation=[maize(), ww(120)], auto_irrigation=False,
                        sim={"evapotranspiration-method": "FAO-56-Dual"}),
        # output sections keyed to the crop step's events
        "crop_events": dict(rotation=[ww(160), maize()], auto_irrigation=False, sim={}, events=[
            ev for name in ("emergence", "anthesis", "maturity", "cereal-stem-elongation",
                            "Stage-1", "Stage-2", "Stage-3", "Stage-4", "Stage-5", "Stage-6")
            for ev in (name, ["Date", "Crop", "Stage", "LAI", "AbBiom"])]),
        # N-min fertilising (reads crop parameters in the general step)
        "nmin": dict(rotation=[dict(ww(100), n_fertilization=[])], auto_irrigation=False,
                     sim={"UseNMinMineralFertilisingMethod": True,
                          "NMinUserParams": {"min": 40, "max": 120, "delayInDays": 10},
                          "NMinFertiliserPartition": ["include-from-file", "mineral-fertilisers/AN.json"],
                          "JulianDayAutomaticFertilising": 89}),
        # hourly FvCB photosynthesis (C3 wheat) with O3 damage and VOC emissions
        "fvcb_o3_voc": dict(rotation=[ww(160), maize()], auto_irrigation=False, sim={},
                            crop_module_params={"__enable_hourly_FvCB_photosynthesis__": True},
                            environment={"AtmosphericO3": 60},
                            extra_daily=["O3-short-damage", "O3-long-damage", "O3-total-uptake",
                                         "O3-WS-gs-reduction", "guenther-isoprene-emission",
                                         "guenther-monoterpene-emission", "jjv-isoprene-emission",
                                         "jjv-monoterpene-emission"]),
        # transplanted maize (force_transplant_state, transplant shock)
        "transplant": dict(crop=transplanted(maize()), auto_irrigation=False, sim={}),
        # perennials with cuttings: perennial reset, cutting, multi-year development
        "perennial_clover": dict(crop=perennial_crop("clover-grass-ley"), auto_irrigation=False, sim={}),
        "perennial_alfalfa": dict(crop=perennial_crop("alfalfa", cut_max_assimilation=90),
                                  auto_irrigation=False, sim={}),
    }


def write(root, name, sc):
    d = root / name
    if d.exists():
        shutil.rmtree(d)
    d.mkdir(parents=True)
    crop = sc["crop"] if "crop" in sc else mj.build_crop_json(sc["rotation"], sc["auto_irrigation"])[0]
    crop["CropParameters"].update(sc.get("crop_module_params", {}))
    sim = mj.build_sim_json(EXTRA + sc.get("extra_daily", []))
    sim.update(sc["sim"])
    sim["output"]["events"] += sc.get("events", [])
    site = mj.build_site_json(None)
    site["EnvironmentParameters"].update(sc.get("environment", {}))
    (d / "crop.json").write_text(json.dumps(crop, indent=1))
    (d / "site.json").write_text(json.dumps(site, indent=1))
    (d / "sim.json").write_text(json.dumps(sim, indent=1))
    shutil.copyfile(mj.CLIMATE_FILE, d / "climate.csv")


def run(binary, d, sub):
    out = d / sub
    if out.exists():
        shutil.rmtree(out)
    out.mkdir()
    sim = json.loads((d / "sim.json").read_text())
    sim["output"]["path-to-output"] = f"./{sub}/"
    (d / f"sim-{sub}.json").write_text(json.dumps(sim, indent=1))
    env = dict(os.environ, MONICA_PARAMETERS=str(mj.MONICA_PARAMETERS))
    r = subprocess.run([str(binary), "-m", f"sim-{sub}.json"], cwd=d, env=env, capture_output=True, text=True)
    (out / "run.log").write_text(r.stdout + r.stderr)
    if r.returncode != 0:
        raise SystemExit(f"{d.name}: {binary} failed, see {out / 'run.log'}")
    return sorted(out.glob("*.csv"))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--save", action="store_true", help="write the scenarios and run the reference binary")
    ap.add_argument("--reference", type=Path, help="the reference monica-run (with --save)")
    ap.add_argument("--monica-run", type=Path, default=ODIN / "build" / f"monica-run{EXE}")
    ap.add_argument("--work", type=Path, default=REPO / "build" / "extra-scenarios")
    a = ap.parse_args()
    if a.save:
        if not a.reference:
            ap.error("--save needs --reference")
        for name, sc in scenarios().items():
            write(a.work, name, sc)
            print(f"saved {name}: {len(run(a.reference, a.work / name, 'base'))} CSVs")
        return
    bad = 0
    for name in scenarios():
        d = a.work / name
        if not (d / "base").is_dir():
            raise SystemExit(f"no reference outputs in {d / 'base'} - run with --save --reference <binary> first")
        for f in run(a.monica_run, d, "cur"):
            b = d / "base" / f.name
            same = b.exists() and b.read_bytes() == f.read_bytes()
            bad += not same
            print(f"{'ok     ' if same else 'DIFFERS'} {name}/{f.name}")
    print(f"{len(scenarios())} scenarios identical" if not bad else f"{bad} CSVs DIFFER")
    sys.exit(1 if bad else 0)


main()
