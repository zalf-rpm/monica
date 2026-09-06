"""Stage 0 spike client: drives cmd/monica-capnp-server over real RPC.

Deliberately uses plain capnp.load() on the same raw .capnp files the Odin
server reads at runtime, rather than the generated mas.schema.* stubs, so both
sides are provably talking about the same schema text.
"""
import asyncio
import sys

import capnp

ROOT = sys.argv[1]
HOST, _, PORT = (sys.argv[2] if len(sys.argv) > 2 else "localhost:6789").rpartition(":")

capnp.remove_import_hook()
common = capnp.load(f"{ROOT}/common/common.capnp", imports=[ROOT])
model = capnp.load(f"{ROOT}/model/model.capnp", imports=[ROOT])


async def main():
    async with capnp.kj_loop():
        stream = await capnp.AsyncIoStream.create_connection(host=HOST, port=int(PORT))
        client = capnp.TwoPartyClient(stream)
        env_instance = client.bootstrap().cast_as(model.EnvInstance)

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
