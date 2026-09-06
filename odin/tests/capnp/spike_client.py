"""Stage 0 spike client: drives cmd/monica-capnp-server over real RPC.

Takes either a plain host:port (--serve-as-bootstrap) or a full sturdy ref; see
_target.py, which also explains the raw-schema loading.

  python spike_client.py <zalfmas_capnp_schemas-dir> <host:port or monicaSR>
"""
import asyncio
import sys

import capnp
from _target import open_monica

ROOT = sys.argv[1]
TARGET = sys.argv[2] if len(sys.argv) > 2 else "localhost:6789"


async def main():
    async with capnp.kj_loop():
        s, env_instance = await open_monica(TARGET, ROOT)
        common = s.common

        print("== info() ==")
        info = await env_instance.info()
        print(f"  id          = {info.id!r}")
        print(f"  name        = {info.name!r}")
        print(f"  description = {info.description!r}")

        print("== run() ==")
        req = env_instance.run_request()
        rest = req.env.rest.as_struct(common.StructuredText)
        rest.value = '{"customId": "spike"}'
        rest.type = "json"
        res = await req.send()
        out = res.result.as_struct(common.StructuredText)
        print(f"  result.value = {out.value!r}")
        print(f"  result.type  = {out.type!r}")

        print("OK")


asyncio.run(main())
