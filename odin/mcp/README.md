# MONICA MCP server (prototype)

Exposes the Odin MONICA build to AI agents (Claude Code, Claude Desktop, any
MCP client) as a small set of job-oriented tools. Step 1 of the plan: a local
executor around `monica-run`; an HPC/SLURM executor and remote hosting with
authentication come later behind the same tools.

## Setup

1. Build the Odin port (`cd odin && pixi run build`) - the server runs
   `odin/build/monica-run(.exe)`.
2. `monica-parameters` must be checked out (default `~/GitHub/monica-parameters`,
   override with `MONICA_PARAMETERS`).
3. `cd odin/mcp && pixi install`, then `pixi run smoke` for an end-to-end check.

Claude Code picks the server up automatically in this repo via the
project-level `.mcp.json` at the repo root (it asks for approval on first
use; tools are loaded when a session starts, so open a new session after
changes). To register it for use outside this repo:

```sh
claude mcp add monica -- pixi run --manifest-path /path/to/monica_odin/odin/mcp/pixi.toml serve
```

Claude Desktop (`claude_desktop_config.json`):

```json
{ "mcpServers": { "monica": {
    "command": "pixi",
    "args": ["run", "--manifest-path", "/path/to/monica_odin/odin/mcp/pixi.toml", "serve"] } } }
```

Remote-style test: `pixi run serve-http` serves Streamable HTTP on
`http://127.0.0.1:8765/mcp` - **no authentication, keep it on localhost.**

## What agents get

| Tool | Purpose |
| --- | --- |
| `list_crops` | species and cultivars from `monica-parameters/crops` |
| `get_defaults` | default site/soil, climate period, example rotation, meaning of daily columns |
| `submit_simulation` | crop rotation (+ optional site/soil, auto irrigation, extra output ids) -> job; waits up to `wait_seconds` |
| `get_job` / `list_jobs` | job status (queued, running, succeeded, failed) |
| `get_results` | per-season summary (yield, dates, max biomass/LAI, mean water/N stress, heat/frost, water and N inputs) + yearly balances |
| `get_daily` | daily or monthly-aggregated series of chosen columns, row-limited |
| `compare_jobs` | scenarios side by side |

Resources: `monica://jobs/{job_id}/{file}` (inputs, run log, full output
CSVs) and `monica://docs/outputs`. Prompt: `explain_simulation(job_id)`.

Input is deliberately high level (`species`, `cultivar`, `MM-DD` dates, kg N
per application, soil layers by KA5 texture) and is translated into MONICA's
sim/crop/site JSON per job (`monica_jobs.py`); jobs live in `odin/mcp/jobs/`
and survive a server restart.

## Layout

- `monica_jobs.py` - job layer, no MCP dependency: scenario -> MONICA JSON,
  `LocalExecutor` (subprocess pool), result parsing. A SLURM executor would
  implement the same `submit(job, on_done)` contract.
- `server.py` - MCP tools/resources/prompts on top (official `mcp` Python SDK 2.x, `MCPServer`).
- `smoke_test.py` - drives the server over stdio like a real client.

## Prototype limits

- One climate only: Hohenfinow2 station data 1991-1997 (`installer/Hohenfinow2/climate-min.csv`).
  A real service needs a server-side climate source selected by coordinates.
- Management is limited to sowing, maturity harvest (with a latest date),
  mineral N (ammonium nitrate) and optional automatic irrigation.
- No authentication, quotas or per-user isolation; local execution only.
