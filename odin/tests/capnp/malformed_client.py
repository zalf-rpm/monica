"""Send envs the server cannot run and check it answers instead of dying.

Odin has no exceptions, so the C++'s try/catch around runMonica cannot be ported:
anything that panics inside the model takes the whole process down, and a server
is reachable by any client. The concrete case was an env with no soil profile -
every soil module indexes the soil column unconditionally, so an empty one is an
out-of-bounds access (undefined behaviour in the C++, a process-aborting bounds
check in Odin). run_monica.odin now refuses such a run up front.

Each case must come back as a normal result carrying errors, and the server must
still answer afterwards - checked by running a real fixture last.

  python malformed_client.py <zalfmas_capnp_schemas-dir> <host:port or monicaSR>
"""

import asyncio
import json
import os
import sys

import capnp
from zalfmas_common.model import monica_io

from _target import open_monica, schemas

SCHEMA_ROOT = sys.argv[1]
TARGET = sys.argv[2]

FIXTURE = os.path.abspath(
    os.path.join(os.path.dirname(__file__), "..", "..", "..", "installer", "Hohenfinow2")
)

_schemas = schemas(SCHEMA_ROOT)
common = _schemas.common
model = _schemas.model


def good_env():
    with open(os.path.join(FIXTURE, "sim-min.json")) as f:
        sim = json.load(f)
    with open(os.path.join(FIXTURE, "site-min.json")) as f:
        site = json.load(f)
    with open(os.path.join(FIXTURE, "crop-min.json")) as f:
        crop = json.load(f)
    env = monica_io.create_env_json_from_json_config(
        {"crop": crop, "site": site, "sim": sim, "climate": ""}
    )
    env["csvViaHeaderOptions"] = sim["climate.csv-options"]
    env["pathToClimateCSV"] = os.path.join(FIXTURE, "climate-min.csv")
    return env


def no_soil_env():
    env = good_env()
    del env["params"]["siteParameters"]["SoilProfileParameters"]
    return env


CASES = [
    ("empty object", "{}"),
    ("customId only", '{"customId": "x"}'),
    ("no soil profile", json.dumps(no_soil_env())),
    ("empty soil profile list", json.dumps({**no_soil_env(), "customId": "empty-list"})),
]


async def run_rest(monica, text, type_="json"):
    req = monica.run_request()
    rest = req.env.rest.as_struct(common.StructuredText)
    rest.value = text
    rest.type = type_
    res = await req.send()
    return json.loads(res.result.as_struct(common.StructuredText).value)


async def main():
    failures = []
    async with capnp.kj_loop():
        _, monica = await open_monica(TARGET, SCHEMA_ROOT)

        for name, text in CASES:
            try:
                result = await run_rest(monica, text)
            except Exception as e:
                print(f"  {name}: SERVER DIED ({type(e).__name__})")
                failures.append(name)
                break
            errors = result.get("errors") or []
            if errors:
                print(f"  {name}: refused with {errors[0]!r}")
            else:
                print(f"  {name}: NO ERROR REPORTED (result had {len(result.get('data', []))} sections)")
                failures.append(name)

        # non-JSON rest, which the C++ rejects explicitly
        try:
            result = await run_rest(monica, "not json at all", "unstructured")
            errors = result.get("errors") or []
            print(f"  non-JSON rest: refused with {errors[0]!r}" if errors else "  non-JSON rest: NO ERROR")
            if not errors:
                failures.append("non-JSON rest")
        except Exception as e:
            print(f"  non-JSON rest: SERVER DIED ({type(e).__name__})")
            failures.append("non-JSON rest")

        # ...and the server must still be usable afterwards
        try:
            result = await run_rest(monica, json.dumps(good_env()))
            rows = len(result["data"][1]["results"][0])
            print(f"  still alive: real fixture ran, {rows} daily rows")
            if rows != 2557:
                failures.append("fixture row count")
        except Exception as e:
            print(f"  still alive: SERVER DIED ({type(e).__name__})")
            failures.append("post-malformed fixture")

    if failures:
        print(f"\nFAIL: {failures}")
        sys.exit(1)
    print("\nOK - every malformed env was refused and the server survived")


asyncio.run(main())
