"""MONICA MCP server (prototype).

Exposes the Odin MONICA build to AI agents as a small set of job-oriented
tools: discover crops, submit a crop-rotation simulation, poll it, and read
compact results (season summaries, yearly balances, daily/monthly series).

    pixi run serve        # stdio (Claude Code / Claude Desktop)
    pixi run serve-http   # Streamable HTTP on 127.0.0.1:8765/mcp (no auth!)

Prototype limits: one bundled climate (Hohenfinow2 1991-1997), local
executor only, no authentication or quotas.
"""

from __future__ import annotations

import sys
import time
from typing import Annotated, Any, Literal

import anyio
import monica_jobs as mj
from mcp.server.mcpserver import MCPServer
from mcp.types import ToolAnnotations
from pydantic import BaseModel, Field

INSTRUCTIONS = """\
MONICA is a process-based agro-ecosystem model (crop growth, soil water,
nitrogen and carbon dynamics) simulating crop rotations at daily time steps.

Typical workflow:
1. list_crops / get_defaults to see what can be simulated.
2. submit_simulation with a crop rotation (and optionally a site/soil). Runs
   take about a second locally; the tool waits briefly and usually returns a
   finished job. Otherwise poll get_job.
3. get_results for per-season summaries (yield, sowing/harvest, stress
   factors, water and N inputs) and yearly balances.
4. get_daily (aggregate='monthly' for overviews) to look at the course of the
   season, e.g. to explain a low yield via TraDef (water stress), NDef
   (nitrogen stress), HeatRed, FrostRed or OxRed (waterlogging).
5. compare_jobs to compare scenarios (e.g. N levels, irrigation, soils).

Stress factors are 1 = no stress and smaller = stronger stress. Dates in the
rotation are 'MM-DD'; the rotation repeats over the climate period.
Results come from a model: report them as simulated values, and mention the
limits (single site climate 1991-1997, default parameters) when relevant.
"""

server = MCPServer(
    name="monica", title="MONICA crop model", version="0.1.0", instructions=INSTRUCTIONS
)
jobs = mj.JobManager()

READ_ONLY = ToolAnnotations(readOnlyHint=True, idempotentHint=True, openWorldHint=False)


# ---------------------------------------------------------------------------
# input schemas
# ---------------------------------------------------------------------------


class Fertilization(BaseModel):
    date: Annotated[
        str, Field(description="application date 'MM-DD'", examples=["03-15"])
    ]
    kg_n_ha: Annotated[
        float,
        Field(gt=0, le=400, description="mineral N (ammonium nitrate) in kg N/ha"),
    ]


class CropSpec(BaseModel):
    species: Annotated[
        str, Field(description="species id from list_crops, e.g. 'wheat'")
    ]
    cultivar: Annotated[
        str, Field(description="cultivar id from list_crops, e.g. 'winter-wheat'")
    ]
    sowing_date: Annotated[
        str, Field(description="sowing date 'MM-DD'", examples=["09-22"])
    ]
    latest_harvest_date: Annotated[
        str,
        Field(
            description="harvest happens at maturity, at the latest on this 'MM-DD'; dates earlier in "
            "the calendar than sowing fall into the following year (winter crops)",
            examples=["08-20"],
        ),
    ]
    winter_crop: Annotated[
        bool | None,
        Field(description="defaults to true if the cultivar name contains 'winter'"),
    ] = None
    n_fertilization: Annotated[
        list[Fertilization], Field(description="mineral N applications for this crop")
    ] = []


class SoilLayer(BaseModel):
    thickness_m: Annotated[float, Field(gt=0, le=2.5)]
    soc_percent: Annotated[
        float, Field(ge=0, le=30, description="soil organic carbon [%]")
    ]
    ka5_texture: Annotated[
        str,
        Field(
            description="German KA5 texture class, e.g. 'Sl2' (slightly loamy sand), 'Ls3', 'Ut4', 'Tu3'"
        ),
    ]
    bulk_density_kg_m3: Annotated[float, Field(ge=800, le=2000)]


class SiteSpec(BaseModel):
    latitude: Annotated[float, Field(ge=-90, le=90)] = 52.8
    n_deposition_kg_ha_y: Annotated[float, Field(ge=0, le=100)] = 30
    soil_profile: Annotated[
        list[SoilLayer] | None,
        Field(
            description="layers from the surface down, 0.5-3 m in total; default: Hohenfinow loamy sand"
        ),
    ] = None


# ---------------------------------------------------------------------------
# tools
# ---------------------------------------------------------------------------


@server.tool(annotations=READ_ONLY)
def list_crops() -> list[dict[str, Any]]:
    """List the crop species and their cultivars that can be simulated."""
    return mj.list_crops()


@server.tool(annotations=READ_ONLY)
def get_defaults() -> dict[str, Any]:
    """Default site/soil, the available climate period, an example rotation and
    the meaning of all daily output columns."""
    return {
        "climate": {
            "source": "Hohenfinow2 (Germany) station data",
            "period": mj.CLIMATE_PERIOD,
        },
        "site": mj.default_site(),
        "example_rotation": [
            {
                "species": "wheat",
                "cultivar": "winter-wheat",
                "sowing_date": "09-22",
                "latest_harvest_date": "09-05",
                "n_fertilization": [
                    {"date": "03-15", "kg_n_ha": 40},
                    {"date": "04-15", "kg_n_ha": 80},
                    {"date": "05-15", "kg_n_ha": 40},
                ],
            },
        ],
        "daily_columns": mj.DAILY_OUTPUTS,
    }


async def _wait(job_id: str, wait_seconds: float) -> mj.Job:
    job, end = jobs.get(job_id), time.time() + max(0.0, min(wait_seconds, 120.0))
    while job.status in ("queued", "running") and time.time() < end:
        await anyio.sleep(0.2)
    return job


@server.tool(
    annotations=ToolAnnotations(
        readOnlyHint=False, destructiveHint=False, openWorldHint=False
    )
)
async def submit_simulation(
    rotation: Annotated[
        list[CropSpec],
        Field(description="crop rotation, repeated over the climate period"),
    ],
    site: SiteSpec | None = None,
    auto_irrigation: Annotated[
        bool,
        Field(
            description="irrigate 17 mm whenever the top 30 cm fall below 50% of available water"
        ),
    ] = False,
    extra_daily_outputs: Annotated[
        list[str],
        Field(
            description="additional MONICA output ids for the daily table, e.g. 'NO3', 'STemp', 'GPP'"
        ),
    ] = [],
    label: Annotated[
        str, Field(description="short name for this scenario, shown in list_jobs")
    ] = "",
    wait_seconds: Annotated[
        float, Field(ge=0, le=120, description="how long to wait for completion")
    ] = 30,
) -> dict[str, Any]:
    """Submit a MONICA simulation of a crop rotation on a site. Returns the job;
    if it finished within wait_seconds its status is 'succeeded' and get_results
    can be called right away."""
    job = jobs.submit(
        [c.model_dump() for c in rotation],
        site.model_dump(exclude_none=True) if site else None,
        auto_irrigation,
        extra_daily_outputs,
        label,
    )
    return (await _wait(job.job_id, wait_seconds)).public()


@server.tool(annotations=READ_ONLY)
async def get_job(
    job_id: str, wait_seconds: Annotated[float, Field(ge=0, le=120)] = 0
) -> dict[str, Any]:
    """Status of a job (queued, running, succeeded, failed), optionally waiting for it."""
    return (await _wait(job_id, wait_seconds)).public()


@server.tool(annotations=READ_ONLY)
def list_jobs(limit: Annotated[int, Field(ge=1, le=200)] = 20) -> list[dict[str, Any]]:
    """Most recent jobs first."""
    return [j.public() for j in jobs.list()[:limit]]


@server.tool(annotations=READ_ONLY)
def get_results(job_id: str) -> dict[str, Any]:
    """Per-season summaries (yield, sowing/harvest dates, max biomass and LAI,
    mean water/N stress factors, heat/frost reduction, water and N inputs) and
    yearly water and N balances of a finished job."""
    job = jobs.get(job_id)
    if job.status != "succeeded":
        raise mj.ScenarioError(
            f"job {job_id} is {job.status}" + (f": {job.error}" if job.error else "")
        )
    return mj.summarize(job)


@server.tool(annotations=READ_ONLY)
def get_daily(
    job_id: str,
    columns: Annotated[
        list[str], Field(description="daily columns, see get_defaults().daily_columns")
    ] = [],
    start_date: Annotated[str | None, Field(description="'YYYY-MM-DD'")] = None,
    end_date: Annotated[str | None, Field(description="'YYYY-MM-DD'")] = None,
    aggregate: Annotated[
        Literal["none", "monthly"],
        Field(
            description="'monthly': fluxes summed, states/stress averaged (crop states only over crop days)"
        ),
    ] = "none",
    max_rows: Annotated[int, Field(ge=1, le=1000)] = 200,
) -> dict[str, Any]:
    """Daily (or monthly aggregated) time series of selected output columns."""
    job = jobs.get(job_id)
    if job.status != "succeeded":
        raise mj.ScenarioError(f"job {job_id} is {job.status}")
    return mj.daily(job, columns, start_date, end_date, aggregate, max_rows)


@server.tool(annotations=READ_ONLY)
def compare_jobs(
    job_ids: Annotated[list[str], Field(min_length=2, max_length=20)],
) -> dict[str, Any]:
    """Compare finished jobs side by side: mean yield per crop and per-season
    yields with water/N stress, plus mean yearly water and N balances."""
    out = []
    for jid in job_ids:
        job = jobs.get(jid)
        if job.status != "succeeded":
            out.append({"job_id": jid, "label": job.label, "status": job.status})
            continue
        s = mj.summarize(job)
        years = s["years"]
        mean = lambda k: (
            round(sum(y[k] for y in years) / len(years), 1) if years else None
        )  # noqa: E731
        out.append(
            {
                "job_id": jid,
                "label": job.label,
                "mean_yield_by_crop_kg_ha": s["mean_yield_by_crop_kg_ha"],
                "seasons": [
                    {
                        k: x[k]
                        for k in (
                            "crop",
                            "harvest",
                            "yield_kg_ha",
                            "mean_water_stress_factor",
                            "mean_n_stress_factor",
                            "n_fertilizer_kg_ha",
                            "irrigation_mm",
                        )
                    }
                    for x in s["seasons"]
                    if x.get("harvest")
                ],
                "mean_yearly": {
                    k: mean(k)
                    for k in (
                        "precip_mm",
                        "actual_et_mm",
                        "irrigation_mm",
                        "recharge_mm",
                        "n_leaching_kg_ha",
                    )
                },
            }
        )
    return {"jobs": out}


# ---------------------------------------------------------------------------
# resources and prompts
# ---------------------------------------------------------------------------

JOB_FILES = {
    "sim.json",
    "crop.json",
    "site.json",
    "run.log",
    "out_section_daily.csv",
    "out_section_crop.csv",
    "out_section_yearly.csv",
}


@server.resource(
    "monica://jobs/{job_id}/{file_name}",
    mime_type="text/plain",
    description="Input JSON, run log or full output CSV of a job",
)
def job_file(job_id: str, file_name: str) -> str:
    if file_name not in JOB_FILES:
        raise mj.ScenarioError(
            f"unknown file {file_name!r}; available: {sorted(JOB_FILES)}"
        )
    path = jobs.get(job_id).dir / file_name
    if not path.exists():
        raise mj.ScenarioError(f"{file_name} does not exist (yet) for job {job_id}")
    return path.read_text(encoding="utf-8")


@server.resource(
    "monica://docs/outputs",
    mime_type="text/markdown",
    description="Meaning and units of the daily output columns",
)
def output_docs() -> str:
    lines = ["# MONICA daily output columns", ""]
    lines += [f"- **{k}**: {v}" for k, v in mj.DAILY_OUTPUTS.items()]
    lines += [
        "",
        "Stress/reduction factors are 1 = no stress; lower values reduce growth or yield.",
    ]
    return "\n".join(lines)


@server.resource(
    "monica://docs/components",
    mime_type="text/markdown",
    description="How MONICA's crop model is structured: its components, what each one reads "
    "(with units and sources, incl. previous-day values) and its state - generated from the code",
)
def component_docs() -> str:
    path = mj.REPO / "odin" / "components.md"
    if not path.exists():
        raise mj.ScenarioError("components.md not found - run `pixi run gen-components-doc` in odin/")
    return path.read_text(encoding="utf-8")


@server.prompt()
def explain_simulation(job_id: str) -> str:
    """Guide the model through explaining a finished simulation to a user."""
    return (
        f"Explain the MONICA simulation {job_id} to the user. First call get_results. For each "
        "harvested season, relate the yield to the mean water stress (TraDef) and nitrogen stress "
        "(NDef) factors and to heat/frost reductions. For the best and worst season, call get_daily "
        "with aggregate='monthly' and columns Crop, Stage, LAI, TraDef, NDef, Precip, Tavg over "
        "the season to show when stress occurred. Use plain language, give numbers with units, "
        "and point out that these are simulated values for one site and the years 1991-1997."
    )


def main() -> None:
    if "--http" in sys.argv:
        server.run("streamable-http", host="127.0.0.1", port=8765)
    else:
        server.run("stdio")


if __name__ == "__main__":
    main()
