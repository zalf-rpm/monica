"""Drive cmd/monica-capnp-server with the Hohenfinow2 fixture and dump the result.

Builds exactly the env installer/Hohenfinow2/python/run-producer.py sends to the
ZMQ server (same monica_io.create_env_json_from_json_config, same
csvViaHeaderOptions/pathToClimateCSV), so the Cap'n Proto server's output can be
compared against monica-run's and the ZMQ server's for the same fixture.

Uses plain capnp.load() on the raw .capnp files the Odin server itself parses at
runtime, rather than the generated mas.schema.* stubs, so both sides are provably
talking about the same schema text.

  python run_client.py <zalfmas_capnp_schemas-dir> <host:port> [out.json]
"""

import json
import os
import sys
import asyncio

import capnp
from zalfmas_common.model import monica_io

SCHEMA_ROOT = sys.argv[1]
HOST, _, PORT = sys.argv[2].rpartition(":")
OUT_PATH = sys.argv[3] if len(sys.argv) > 3 else None

FIXTURE = os.path.abspath(
    os.path.join(os.path.dirname(__file__), "..", "..", "..", "installer", "Hohenfinow2")
)

capnp.remove_import_hook()
common = capnp.load(f"{SCHEMA_ROOT}/common/common.capnp", imports=[SCHEMA_ROOT])
model = capnp.load(f"{SCHEMA_ROOT}/model/model.capnp", imports=[SCHEMA_ROOT])


def build_env():
    with open(os.path.join(FIXTURE, "sim-min.json")) as f:
        sim_json = json.load(f)
    with open(os.path.join(FIXTURE, "site-min.json")) as f:
        site_json = json.load(f)
    with open(os.path.join(FIXTURE, "crop-min.json")) as f:
        crop_json = json.load(f)

    env = monica_io.create_env_json_from_json_config(
        {"crop": crop_json, "site": site_json, "sim": sim_json, "climate": ""}
    )
    # run-producer.py does exactly this: the server reads the CSV itself.
    env["csvViaHeaderOptions"] = sim_json["climate.csv-options"]
    env["pathToClimateCSV"] = os.path.join(FIXTURE, "climate-min.csv")
    env["customId"] = "capnp-fixture"
    return env


async def main():
    env = build_env()

    async with capnp.kj_loop():
        stream = await capnp.AsyncIoStream.create_connection(host=HOST, port=int(PORT))
        client = capnp.TwoPartyClient(stream)
        monica = client.bootstrap().cast_as(model.EnvInstance)

        info = await monica.info()
        print(f"info: id={info.id!r} name={info.name!r} description={info.description!r}")

        req = monica.run_request()
        rest = req.env.rest.as_struct(common.StructuredText)
        rest.value = json.dumps(env)
        rest.type = "json"

        res = await req.send()
        out = res.result.as_struct(common.StructuredText)
        print(f"result.type = {out.type}")

        result = json.loads(out.value)
        print(f"customId    = {result.get('customId')!r}")
        print(f"errors      = {result.get('errors')}")
        print(f"warnings    = {result.get('warnings')}")
        for d in result.get("data", []):
            n = len(d.get("results", []))
            first = len(d["results"][0]) if n else 0
            print(f"section {d.get('origSpec')!r}: {n} output id(s), {first} row(s)")

        if OUT_PATH:
            with open(OUT_PATH, "w") as f:
                json.dump(result, f, indent=1)
            print(f"wrote {OUT_PATH}")


asyncio.run(main())
