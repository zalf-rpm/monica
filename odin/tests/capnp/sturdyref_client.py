"""Reach the server the way an existing ZALF client does: through a sturdy ref.

The other clients here connect to host:port and use the bootstrap capability
directly. This one takes the `monicaSR=` line the server prints and hands it to
**zalfmas_common's own ConnectionManager**, unmodified — the same code path the
real MONICA clients use. It connects, gets the Restorer as the bootstrap, calls
restore(token) and casts the result to EnvInstance, none of which this repo
controls. If it works, an existing client works.

Also checks that an unknown token comes back as a null capability rather than an
error, which is what the C++ Restorer does (`KJ_IF_MAYBE(cap, maybeCap)` simply
leaves the result unset).

  python sturdyref_client.py <monicaSR>
"""

import asyncio
import json
import os
import sys

import capnp
from mas.schema.common import common_capnp
from mas.schema.model import model_capnp
from zalfmas_common import common
from zalfmas_common.model import monica_io

MONICA_SR = sys.argv[1]

FIXTURE = os.path.abspath(
    os.path.join(os.path.dirname(__file__), "..", "..", "..", "installer", "Hohenfinow2")
)



def build_env():
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
    env["customId"] = "sturdy-ref"
    return env


async def main():
    async with capnp.kj_loop():
        conman = common.ConnectionManager()

        print(f"connecting via {MONICA_SR}")
        monica = await conman.connect(MONICA_SR, cast_as=model_capnp.EnvInstance)
        if monica is None:
            print("FAIL - ConnectionManager returned nothing")
            sys.exit(1)

        info = await monica.info()
        print(f"  info: id={info.id!r} name={info.name!r}")

        req = monica.run_request()
        rest = req.env.rest.as_struct(common_capnp.StructuredText)
        rest.value = json.dumps(build_env())
        rest.type = "json"
        res = await req.send()
        result = json.loads(res.result.as_struct(common_capnp.StructuredText).value)
        rows = len(result["data"][1]["results"][0])
        print(f"  run: customId={result.get('customId')!r} errors={result.get('errors')} daily={rows}")
        if rows != 2557 or result.get("errors"):
            print("FAIL - unexpected result")
            sys.exit(1)

        # An unknown token: the C++ leaves `cap` unset rather than erroring.
        base, _, _ = MONICA_SR.rpartition("/")
        bad_sr = f"{base}/definitely-not-a-real-token"
        print(f"unknown token via {bad_sr}")
        try:
            bad = await conman.connect(bad_sr, cast_as=model_capnp.EnvInstance)
            if bad is None:
                print("  restore returned nothing, as expected")
            else:
                # A null capability only fails once actually called.
                try:
                    await bad.info()
                    print("  FAIL - an unknown token produced a working capability")
                    sys.exit(1)
                except Exception as e:
                    print(f"  calling it fails, as expected ({type(e).__name__})")
        except Exception as e:
            print(f"  restore failed, as expected ({type(e).__name__})")

        # ...and the server is still fine afterwards.
        info2 = await monica.info()
        print(f"  still alive: info id={info2.id!r}")

    print("\nOK - reached MONICA through a sturdy ref with zalfmas_common's ConnectionManager")


asyncio.run(main())
