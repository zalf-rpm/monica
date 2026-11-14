"""Job layer for the MONICA MCP prototype - independent of MCP itself.

Turns a high-level scenario (crop rotation + site) into MONICA's sim/crop/
site JSON files, runs the Odin `monica-run` binary as a job and parses the
CSV output sections into compact summaries.

The executor is the only part that knows *where* a job runs. `LocalExecutor`
runs monica-run as a subprocess on this machine; an HPC executor (sbatch /
slurmrestd + shared storage) would implement the same `submit`/`poll`
contract and the MCP tools on top would not change.
"""

from __future__ import annotations

import concurrent.futures
import csv
import io
import json
import os
import re
import shutil
import subprocess
import threading
import time
import uuid
from dataclasses import asdict, dataclass, field
from pathlib import Path
from typing import Any

HERE = Path(__file__).resolve().parent
REPO = HERE.parent.parent  # monica_odin/
FIXTURE_DIR = REPO / "installer" / "Hohenfinow2"


def _default_binary() -> Path:
    exe = REPO / "odin" / "build" / ("monica-run.exe" if os.name == "nt" else "monica-run")
    return Path(os.environ.get("MONICA_RUN", exe))


MONICA_PARAMETERS = Path(
    os.environ.get("MONICA_PARAMETERS", Path.home() / "GitHub" / "monica-parameters")
)
JOBS_DIR = Path(os.environ.get("MONICA_MCP_JOBS", HERE / "jobs"))

# The only climate data the prototype ships with: Hohenfinow2 station data,
# 1991-01-01 .. 1997-12-31. A real service needs a server-side climate source
# (station/gridded/scenario data selected by coordinates).
CLIMATE_FILE = FIXTURE_DIR / "climate-min.csv"
CLIMATE_PERIOD = ("1991-01-01", "1997-12-31")

# ---------------------------------------------------------------------------
# Output configuration: one fixed, explanation-friendly set per section.
# ---------------------------------------------------------------------------

# Daily columns (MONICA output id -> description). The stress factors are
# what lets an agent explain *why* a yield came out the way it did.
DAILY_OUTPUTS: dict[str, str] = {
    "Date": "date",
    "Crop": "current crop (empty between crops)",
    "Stage": "developmental stage (1..6/7, 0 = none)",
    "Yield": "primary yield [kg DM/ha]",
    "AbBiom": "aboveground biomass [kg DM/ha]",
    "LAI": "leaf area index [m2/m2]",
    "Tavg": "mean air temperature [degC]",
    "Tmax": "max air temperature [degC]",
    "Precip": "precipitation [mm]",
    "Irrig": "irrigation [mm]",
    "NFert": "mineral N fertilizer applied [kg N/ha]",
    "Act_ET": "actual evapotranspiration [mm]",
    "ET0": "reference evapotranspiration [mm]",
    "Mois": "soil moisture, mean of top 3 layers (0-30 cm) [m3/m3]",
    "TraDef": "water stress factor: transpiration deficit (1 = none, 0 = full stress)",
    "NDef": "nitrogen stress factor (1 = none, 0 = full stress)",
    "HeatRed": "heat stress yield reduction at flowering (1 = none)",
    "FrostRed": "frost damage reduction factor (1 = none)",
    "OxRed": "oxygen deficiency / waterlogging factor (1 = none)",
    "RunOff": "surface runoff [mm]",
    "Recharge": "groundwater recharge [mm]",
    "NLeach": "N leaching below the leaching depth [kg N/ha]",
}
# columns aggregated by summing (fluxes); all others are averaged
FLUX_COLUMNS = {"Precip", "Irrig", "NFert", "Act_ET", "ET0", "RunOff", "Recharge", "NLeach"}
# crop state/stress columns: monthly means only over days with a crop
# (between crops they are 0 or meaningless and would dilute the mean)
CROP_ONLY_COLUMNS = {"Stage", "Yield", "AbBiom", "LAI", "TraDef", "NDef", "HeatRed", "FrostRed", "OxRed"}


def _daily_spec(extra: list[str]) -> list[Any]:
    spec: list[Any] = []
    for oid in DAILY_OUTPUTS:
        spec.append(["Mois", [1, 3, "AVG"]] if oid == "Mois" else oid)
    for oid in extra:
        if oid not in DAILY_OUTPUTS:
            spec.append(oid)
    return spec


CROP_OUTPUTS: list[Any] = [
    "CM-count",
    "Crop",
    ["Yield", "LAST"],
    ["Date|sowing", "FIRST"],
    ["Date|harvest", "LAST"],
    ["AbBiom", "MAX"],
    ["LAI", "MAX"],
    ["TraDef", "AVG"],
    ["NDef", "AVG"],
    ["HeatRed", "LAST"],
    ["FrostRed", "LAST"],
    ["OxRed", "AVG"],
    ["Precip", "SUM"],
    ["Act_ET", "SUM"],
    ["Irrig", "SUM"],
    ["NFert", "SUM"],
]
CROP_COLUMN_NAMES = [
    "cm_count", "crop", "yield_kg_ha", "sowing", "harvest", "max_aboveground_biomass_kg_ha",
    "max_lai", "mean_water_stress_factor", "mean_n_stress_factor", "heat_reduction_factor",
    "frost_reduction_factor", "mean_oxygen_factor", "precip_mm", "actual_et_mm",
    "irrigation_mm", "n_fertilizer_kg_ha",
]

YEARLY_OUTPUTS: list[Any] = [
    "Year",
    ["Precip", "SUM"],
    ["Act_ET", "SUM"],
    ["Irrig", "SUM"],
    ["NFert", "SUM"],
    ["RunOff", "SUM"],
    ["Recharge", "SUM"],
    ["NLeach", "SUM"],
]
YEARLY_COLUMN_NAMES = [
    "year", "precip_mm", "actual_et_mm", "irrigation_mm", "n_fertilizer_kg_ha",
    "runoff_mm", "recharge_mm", "n_leaching_kg_ha",
]

# ---------------------------------------------------------------------------
# Scenario -> MONICA JSON
# ---------------------------------------------------------------------------

DATE_MMDD = re.compile(r"^(0[1-9]|1[0-2])-(0[1-9]|[12][0-9]|3[01])$")


try:  # under the MCP SDK these reach the model verbatim instead of a generic "Error executing tool"
    from mcp.server.mcpserver.exceptions import ResourceError as _ResourceError, ToolError as _ToolError
    _ERROR_BASES: tuple[type, ...] = (_ToolError, _ResourceError)
except ImportError:  # job layer used without MCP
    _ERROR_BASES = (ValueError,)


class ScenarioError(*_ERROR_BASES):
    """Invalid scenario input - the message is meant for the agent/user."""


def list_crops() -> list[dict[str, Any]]:
    """species -> cultivars available in monica-parameters/crops."""
    crops_dir = MONICA_PARAMETERS / "crops"
    out = []
    for species_file in sorted(crops_dir.glob("*.json")):
        species = species_file.stem
        cultivar_dir = crops_dir / species
        if not cultivar_dir.is_dir():
            continue
        cultivars = sorted(p.stem for p in cultivar_dir.glob("*.json"))
        if cultivars:
            out.append({"species": species, "cultivars": cultivars})
    return out


def _check_mmdd(value: str, what: str) -> str:
    if not DATE_MMDD.match(value):
        raise ScenarioError(f"{what} must be 'MM-DD' (e.g. '09-22'), got {value!r}")
    return value


def _rel_date(mmdd: str, sowing_mmdd: str) -> str:
    """MONICA relative date: year 0000 = sowing year, 0001 = the year after.

    Any event that falls earlier in the calendar than sowing is taken to
    happen in the following year (e.g. winter wheat sown 09-22 and fertilized
    03-15 -> 0001-03-15).
    """
    return ("0001-" if mmdd < sowing_mmdd else "0000-") + mmdd


def _residue_file(species: str) -> str:
    residue = MONICA_PARAMETERS / "crop-residues" / f"{species}.json"
    if residue.exists():
        return f"crop-residues/{species}.json"
    return "crop-residues/wheat.json"  # generic fallback, reported in the job's notes


def build_crop_json(rotation: list[dict[str, Any]], auto_irrigation: bool) -> tuple[dict, list[str]]:
    if not rotation:
        raise ScenarioError("rotation must contain at least one crop")
    notes: list[str] = []
    crops: dict[str, Any] = {}
    cms = []
    for i, c in enumerate(rotation):
        species, cultivar = c["species"], c["cultivar"]
        if not (MONICA_PARAMETERS / "crops" / f"{species}.json").exists():
            raise ScenarioError(f"unknown species {species!r} - see list_crops")
        if not (MONICA_PARAMETERS / "crops" / species / f"{cultivar}.json").exists():
            raise ScenarioError(f"unknown cultivar {cultivar!r} for {species!r} - see list_crops")
        sowing = _check_mmdd(c["sowing_date"], f"rotation[{i}].sowing_date")
        latest_harvest = _check_mmdd(c["latest_harvest_date"], f"rotation[{i}].latest_harvest_date")
        winter = c.get("winter_crop")
        if winter is None:
            winter = "winter" in cultivar
        key = f"crop{i}_{species}_{cultivar}"
        residue = _residue_file(species)
        if residue.endswith("/wheat.json") and species != "wheat":
            notes.append(f"no residue parameters for {species}, used wheat residues")
        crops[key] = {
            "is-winter-crop": bool(winter),
            "cropParams": {
                "species": ["include-from-file", f"crops/{species}.json"],
                "cultivar": ["include-from-file", f"crops/{species}/{cultivar}.json"],
            },
            "residueParams": ["include-from-file", residue],
        }
        ws: list[dict[str, Any]] = []
        if auto_irrigation:
            ws.append({
                "type": "AutomaticIrrigation",
                "irrigateCrop": True,
                "parameters": {
                    "irrigationParameters": {"nitrateConcentration": [0, "mg dm-3"]},
                    "amount": [17, "mm"],
                    "trigger_if_nFC_below_%": [50, "%"],
                    "calc_nFC_until_depth_m": [0.3, "m"],
                },
            })
        ws.append({"date": "0000-" + sowing, "type": "Sowing", "crop": ["ref", "crops", key]})
        for j, f in enumerate(c.get("n_fertilization") or []):
            d = _check_mmdd(f["date"], f"rotation[{i}].n_fertilization[{j}].date")
            kg = float(f["kg_n_ha"])
            if not 0 < kg <= 400:
                raise ScenarioError(f"rotation[{i}].n_fertilization[{j}].kg_n_ha must be in (0, 400]")
            ws.append({
                "type": "MineralFertilization",
                "date": _rel_date(d, sowing),
                "amount": [kg, "kg N"],
                "partition": ["include-from-file", "mineral-fertilisers/AN.json"],
            })
        ws.append({
            "type": "AutomaticHarvest",
            "latest-date": _rel_date(latest_harvest, sowing),
            "min-%-asw": 0,
            "max-%-asw": 100,
            "max-3d-precip-sum": 999,
            "max-curr-day-precip": 999,
            "harvest-time": "maturity",
        })
        cms.append({"worksteps": ws})
    crop_json = {
        "crops": crops,
        "cropRotation": cms,
        "CropParameters": {
            "=": ["include-from-file", "general/crop.json"],
            "__enable_vernalisation_factor_fix__": True,
        },
    }
    return crop_json, notes


def default_site() -> dict[str, Any]:
    return {
        "latitude": 52.8,
        "n_deposition_kg_ha_y": 30,
        "soil_profile": [
            {"thickness_m": 0.3, "soc_percent": 0.8, "ka5_texture": "Sl2", "bulk_density_kg_m3": 1446},
            {"thickness_m": 0.1, "soc_percent": 0.15, "ka5_texture": "Sl2", "bulk_density_kg_m3": 1446},
            {"thickness_m": 1.6, "soc_percent": 0.05, "ka5_texture": "Sl2", "bulk_density_kg_m3": 1446},
        ],
    }


def build_site_json(site: dict[str, Any] | None) -> dict:
    s = {**default_site(), **(site or {})}
    lat = float(s["latitude"])
    if not -90 <= lat <= 90:
        raise ScenarioError("latitude must be within [-90, 90]")
    layers = []
    total = 0.0
    for i, l in enumerate(s["soil_profile"]):
        t = float(l["thickness_m"])
        if t <= 0:
            raise ScenarioError(f"soil_profile[{i}].thickness_m must be > 0")
        total += t
        layers.append({
            "Thickness": [t, "m"],
            "SoilOrganicCarbon": [float(l["soc_percent"]), "%"],
            "KA5TextureClass": str(l["ka5_texture"]),
            "SoilRawDensity": [float(l["bulk_density_kg_m3"]), "kg m-3"],
        })
    if not 0.5 <= total <= 3.0:
        raise ScenarioError(f"soil profile must be 0.5-3.0 m deep in total, got {total:.2f} m")
    return {
        "SiteParameters": {
            "Latitude": lat,
            "Slope": 0,
            "HeightNN": [0, "m"],
            "NDeposition": [float(s["n_deposition_kg_ha_y"]), "kg N ha-1 y-1"],
            "pwpFcSatFunction": "Wessolek2009",
            "SoilProfileParameters": layers,
        },
        "SoilTemperatureParameters": ["include-from-file", "general/soil-temperature.json"],
        "EnvironmentParameters": {
            "=": ["include-from-file", "general/environment.json"],
            "LeachingDepth": 2.0,
            "WindSpeedHeight": 2.5,
        },
        "SoilOrganicParameters": ["include-from-file", "general/soil-organic.json"],
        "SoilTransportParameters": ["include-from-file", "general/soil-transport.json"],
        "SoilMoistureParameters": ["include-from-file", "general/soil-moisture.json"],
    }


def build_sim_json(extra_daily: list[str]) -> dict:
    return {
        "crop.json": "crop.json",
        "site.json": "site.json",
        "climate.csv": "climate.csv",
        "climate.csv-options": {"no-of-climate-file-header-lines": 2, "csv-separator": ","},
        "debug?": False,
        "include-file-base-path": "${MONICA_PARAMETERS}",
        "output": {
            "write-file?": True,
            "path-to-output": "./",
            "file-name": "out.csv",
            "csv-options": {
                "include-header-row": True,
                "include-units-row": True,
                "include-aggregation-rows": False,
                "csv-separator": ",",
            },
            "events": [
                "daily", _daily_spec(extra_daily),
                "crop", CROP_OUTPUTS,
                "yearly", YEARLY_OUTPUTS,
            ],
        },
        "UseSecondaryYields": True,
        "NitrogenResponseOn": True,
        "WaterDeficitResponseOn": True,
        "EmergenceMoistureControlOn": False,
        "EmergenceFloodingControlOn": False,
        "UseNMinMineralFertilisingMethod": False,
        "UseAutomaticIrrigation": False,
    }


# ---------------------------------------------------------------------------
# Jobs and executors
# ---------------------------------------------------------------------------


@dataclass
class Job:
    job_id: str
    label: str
    status: str  # queued | running | succeeded | failed
    created: float
    started: float | None = None
    finished: float | None = None
    error: str | None = None
    notes: list[str] = field(default_factory=list)
    scenario: dict[str, Any] = field(default_factory=dict)

    @property
    def dir(self) -> Path:
        return JOBS_DIR / self.job_id

    def save(self) -> None:
        (self.dir / "job.json").write_text(json.dumps(asdict(self), indent=2), encoding="utf-8")

    def public(self) -> dict[str, Any]:
        d = {k: v for k, v in asdict(self).items() if k != "scenario"}
        if self.started and self.finished:
            d["runtime_s"] = round(self.finished - self.started, 2)
        return d


class LocalExecutor:
    """Runs monica-run as a local subprocess, a few jobs in parallel."""

    def __init__(self, binary: Path | None = None, max_parallel: int | None = None, timeout_s: int = 900):
        self.binary = binary or _default_binary()
        self.timeout_s = timeout_s
        self.pool = concurrent.futures.ThreadPoolExecutor(
            max_workers=max_parallel or max(1, (os.cpu_count() or 2) // 2)
        )

    def submit(self, job: Job, on_done) -> None:
        self.pool.submit(self._run, job, on_done)

    def _run(self, job: Job, on_done) -> None:
        job.status, job.started = "running", time.time()
        job.save()
        env = {**os.environ, "MONICA_PARAMETERS": str(MONICA_PARAMETERS)}
        try:
            p = subprocess.run(
                [str(self.binary), "-m", "sim.json"],
                cwd=job.dir, env=env, capture_output=True, text=True, timeout=self.timeout_s,
            )
            (job.dir / "run.log").write_text(p.stdout + p.stderr, encoding="utf-8")
            if p.returncode != 0:
                job.status, job.error = "failed", f"monica-run exited with {p.returncode}: {(p.stdout + p.stderr)[-800:]}"
            elif not (job.dir / "out_section_crop.csv").exists():
                job.status, job.error = "failed", "monica-run produced no output (see run.log)"
            else:
                job.status = "succeeded"
        except subprocess.TimeoutExpired:
            job.status, job.error = "failed", f"timed out after {self.timeout_s}s"
        except Exception as e:  # noqa: BLE001 - report anything to the agent
            job.status, job.error = "failed", f"{type(e).__name__}: {e}"
        job.finished = time.time()
        job.save()
        on_done(job)


class JobManager:
    def __init__(self, executor: LocalExecutor | None = None):
        JOBS_DIR.mkdir(parents=True, exist_ok=True)
        self.executor = executor or LocalExecutor()
        self.lock = threading.Lock()
        self.jobs: dict[str, Job] = {}
        for jf in JOBS_DIR.glob("*/job.json"):  # jobs survive a server restart
            try:
                j = Job(**json.loads(jf.read_text(encoding="utf-8")))
                if j.status in ("queued", "running"):
                    j.status, j.error = "failed", "server restarted while the job was pending"
                self.jobs[j.job_id] = j
            except Exception:  # noqa: BLE001
                pass

    def submit(self, rotation, site, auto_irrigation, extra_daily, label) -> Job:
        if not self.executor.binary.exists():
            raise ScenarioError(f"monica-run binary not found at {self.executor.binary} - build the Odin port first")
        crop_json, notes = build_crop_json(rotation, auto_irrigation)
        site_json = build_site_json(site)
        for oid in extra_daily:
            if not re.fullmatch(r"[A-Za-z0-9_|/.\-]+", oid):
                raise ScenarioError(f"invalid output id {oid!r}")
        job = Job(
            job_id=uuid.uuid4().hex[:12], label=label, status="queued", created=time.time(), notes=notes,
            scenario={"rotation": rotation, "site": site or default_site(),
                      "auto_irrigation": auto_irrigation, "extra_daily_outputs": extra_daily},
        )
        job.dir.mkdir(parents=True)
        (job.dir / "crop.json").write_text(json.dumps(crop_json, indent=2), encoding="utf-8")
        (job.dir / "site.json").write_text(json.dumps(site_json, indent=2), encoding="utf-8")
        (job.dir / "sim.json").write_text(json.dumps(build_sim_json(extra_daily), indent=2), encoding="utf-8")
        shutil.copyfile(CLIMATE_FILE, job.dir / "climate.csv")
        job.save()
        with self.lock:
            self.jobs[job.job_id] = job
        self.executor.submit(job, lambda j: None)
        return job

    def get(self, job_id: str) -> Job:
        job = self.jobs.get(job_id)
        if job is None:
            raise ScenarioError(f"unknown job_id {job_id!r} - see list_jobs")
        return job

    def list(self) -> list[Job]:
        return sorted(self.jobs.values(), key=lambda j: j.created, reverse=True)

    def wait(self, job_id: str, timeout_s: float) -> Job:
        job, end = self.get(job_id), time.time() + timeout_s
        while job.status in ("queued", "running") and time.time() < end:
            time.sleep(0.2)
        return job


# ---------------------------------------------------------------------------
# Results
# ---------------------------------------------------------------------------


def read_section(job: Job, section: str) -> tuple[list[str], list[str], list[list[str]]]:
    """(header, units, rows) of out_section_<section>.csv"""
    path = job.dir / f"out_section_{section}.csv"
    if not path.exists():
        raise ScenarioError(f"job {job.job_id} has no {section} output (status: {job.status})")
    rows = list(csv.reader(io.StringIO(path.read_text(encoding="utf-8"))))
    rows = [r[:-1] if r and r[-1] == "" else r for r in rows]  # MONICA writes a trailing separator
    return rows[0], rows[1], rows[2:]


def _num(v: str) -> float | str:
    try:
        return float(v)
    except ValueError:
        return v


def summarize(job: Job) -> dict[str, Any]:
    _, _, crop_rows = read_section(job, "crop")
    seasons = []
    for r in crop_rows:
        rec = dict(zip(CROP_COLUMN_NAMES, (_num(v) for v in r)))
        if rec.get("crop") in ("", None):
            continue
        seasons.append(rec)
    _, _, year_rows = read_section(job, "yearly")
    years = [dict(zip(YEARLY_COLUMN_NAMES, (_num(v) for v in r))) for r in year_rows]
    by_crop: dict[str, list[float]] = {}
    for s in seasons:
        if isinstance(s.get("yield_kg_ha"), float) and s.get("harvest"):
            by_crop.setdefault(str(s["crop"]), []).append(s["yield_kg_ha"])
    return {
        "job_id": job.job_id,
        "label": job.label,
        "climate": f"Hohenfinow2 station data {CLIMATE_PERIOD[0]}..{CLIMATE_PERIOD[1]}",
        "seasons": seasons,
        "years": years,
        "mean_yield_by_crop_kg_ha": {k: round(sum(v) / len(v), 1) for k, v in by_crop.items()},
        "notes": job.notes + [
            "stress factors are 1 = no stress, lower = stronger stress; a season's last row may be "
            "an unfinished crop at the end of the climate period (no harvest date)",
        ],
    }


def daily(job: Job, columns: list[str], start: str | None, end: str | None,
          aggregate: str, max_rows: int) -> dict[str, Any]:
    header, units, rows = read_section(job, "daily")
    cols = columns or ["Date", "Crop", "Stage", "Yield", "LAI", "TraDef", "NDef"]
    missing = [c for c in cols if c not in header]
    if missing:
        raise ScenarioError(f"unknown daily columns {missing}; available: {header}")
    if "Date" not in cols:
        cols = ["Date"] + cols
    idx = [header.index(c) for c in cols]
    sel = [r for r in rows if (not start or r[0] >= start) and (not end or r[0] <= end)]
    if aggregate == "monthly":
        groups: dict[str, list[list[str]]] = {}
        for r in sel:
            groups.setdefault(r[0][:7], []).append(r)
        out_rows = []
        for month, rs in groups.items():
            row: list[Any] = [month]
            crop_i = header.index("Crop")
            for c, i in zip(cols[1:], idx[1:]):
                src = [r for r in rs if r[crop_i]] if c in CROP_ONLY_COLUMNS else rs
                vals = [float(r[i]) for r in src if r[i] not in ("", None) and _is_float(r[i])]
                if not vals:
                    # no crop that month -> None; non-numeric (e.g. Crop) -> last value
                    row.append(None if c in CROP_ONLY_COLUMNS else rs[-1][i])
                elif c in FLUX_COLUMNS:
                    row.append(round(sum(vals), 3))
                else:
                    row.append(round(sum(vals) / len(vals), 4))
            out_rows.append(row)
        cols = ["Month"] + cols[1:]
    elif aggregate == "none":
        out_rows = [[_num(r[i]) for i in idx] for r in sel]
    else:
        raise ScenarioError("aggregate must be 'none' or 'monthly'")
    truncated = len(out_rows) > max_rows
    return {
        "columns": cols,
        "units": [units[header.index(c)] if c in header else "" for c in cols],
        "rows": out_rows[:max_rows],
        "truncated": truncated,
        "total_rows": len(out_rows),
        "hint": "narrow start/end or use aggregate='monthly'" if truncated else None,
    }


def _is_float(v: str) -> bool:
    try:
        float(v)
        return True
    except ValueError:
        return False
