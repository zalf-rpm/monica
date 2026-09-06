"""Stand in for an FBP runtime around monica-capnp-fbp-component.

Hosts the two channel endpoints the component connects to, each as the bootstrap
capability of its own loopback port (so the sturdy refs need no token):

  env    a Channel(IP).Reader serving, in order, an openBracket IP, N Env IPs,
         a closeBracket IP, then `done`
  result a Channel(IP).Writer collecting whatever comes back, plus `close`

Then it checks the component did what monica-capnp-fbp-component-main.cpp does:
brackets forwarded through untouched, one result IP per Env IP, IP attributes
carried over, and the result JSON matching what the fixture should produce.

Run the component against it with:

    monica-capnp-fbp-component --env_in_sr capnp://localhost:<envPort> \
                               --result_out_sr capnp://localhost:<resPort>

  python fbp_harness.py <zalfmas_capnp_schemas-dir> <envPort> <resPort> [--to-attr NAME]
"""

import asyncio
import contextlib
import json
import os
import sys

import capnp
from zalfmas_common.model import monica_io

from _target import schemas

SCHEMA_ROOT = sys.argv[1]
ENV_PORT = int(sys.argv[2])
RES_PORT = int(sys.argv[3])
def opt(name):
    return sys.argv[sys.argv.index(name) + 1] if name in sys.argv else None


TO_ATTR = opt("--to-attr")
CONF_PORT = int(opt("--conf-port")) if opt("--conf-port") else None
MONICA_SR = opt("--monica-sr")

FIXTURE = os.path.abspath(
    os.path.join(os.path.dirname(__file__), "..", "..", "..", "installer", "Hohenfinow2")
)

s = schemas(SCHEMA_ROOT)
N_ENVS = 2


def build_env_json(custom_id):
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
    env["customId"] = custom_id
    return env


class EnvReader(s.fbp.Channel.Reader.Server):
    """The component's `env` IN port."""

    def __init__(self):
        # openBracket, N envs, closeBracket, then done
        self.script = ["openBracket"] + [f"env{i}" for i in range(N_ENVS)] + ["closeBracket"]
        self.sent = 0

    async def read_context(self, context):
        if self.sent >= len(self.script):
            context.results.done = None
            return
        item = self.script[self.sent]
        self.sent += 1

        # Channel(V) is generic and unbound here, so `value` is an AnyPointer.
        ip = context.results.value.as_struct(s.fbp.IP)
        if item in ("openBracket", "closeBracket"):
            ip.type = item
            ip.init("attributes", 1)
            ip.attributes[0].key = "marker"
            ip.attributes[0].value.set_as_text(item)
            return

        ip.type = "standard"
        env = ip.content.as_struct(s.model.Env)
        rest = env.rest.as_struct(s.common.StructuredText)
        rest.value = json.dumps(build_env_json(item))
        rest.type = "json"
        # An attribute the component must carry over to the result IP.
        ip.init("attributes", 1)
        ip.attributes[0].key = "trace"
        ip.attributes[0].value.set_as_text(item)

    async def close_context(self, context):
        pass


class ConfReader(s.fbp.Channel.Reader.Server):
    """The component's `conf` IN port: one StructuredText IIP, then done.

    This is how the C++ gets `monica_sr` (use a remote MONICA rather than an
    inline one) and `to_attr` - see its DEFAULT_CONFIG.
    """

    def __init__(self, config):
        self.config = config
        self.sent = False

    async def read_context(self, context):
        if self.sent:
            context.results.done = None
            return
        self.sent = True
        ip = context.results.value.as_struct(s.fbp.IP)
        ip.type = "standard"
        st = ip.content.as_struct(s.common.StructuredText)
        st.value = json.dumps(self.config)
        st.type = "json"

    async def close_context(self, context):
        pass


class ResultWriter(s.fbp.Channel.Writer.Server):
    """The component's `result` OUT port."""

    def __init__(self):
        self.received = []
        self.closed = False
        self.done = asyncio.get_event_loop().create_future()

    async def write_context(self, context):
        msg = context.params
        ip = msg.value.as_struct(s.fbp.IP)
        entry = {
            "type": str(ip.type),
            "attributes": {kv.key: kv.value.as_text() for kv in ip.attributes},
        }
        # A standard IP carries the result JSON, unless it went to an attribute.
        if entry["type"] == "standard" and TO_ATTR is None:
            entry["content"] = ip.content.as_text()
        self.received.append(entry)
        if len(self.received) == N_ENVS + 2 and not self.done.done():
            self.done.set_result(True)

    async def close_context(self, context):
        self.closed = True
        if not self.done.done():
            self.done.set_result(True)


async def main():
    async with capnp.kj_loop():
        reader = EnvReader()
        writer = ResultWriter()

        # The callback has to stay awaited for the life of the connection: a
        # TwoPartyServer that is constructed and dropped is collected straight
        # away, and the peer just sees "Peer disconnected".
        async def serve(stream, bootstrap):
            await capnp.TwoPartyServer(stream, bootstrap=bootstrap).on_disconnect()

        env_server = await capnp.AsyncIoStream.create_server(
            lambda stream: serve(stream, reader), "localhost", ENV_PORT
        )
        res_server = await capnp.AsyncIoStream.create_server(
            lambda stream: serve(stream, writer), "localhost", RES_PORT
        )
        servers = [env_server, res_server]

        conf_server = None
        if CONF_PORT is not None:
            config = {}
            if MONICA_SR:
                config["monica_sr"] = MONICA_SR
            if TO_ATTR:
                config["to_attr"] = TO_ATTR
            conf = ConfReader(config)
            conf_server = await capnp.AsyncIoStream.create_server(
                lambda stream: serve(stream, conf), "localhost", CONF_PORT
            )
            servers.append(conf_server)
            print(f"conf reader on localhost:{CONF_PORT} serving {config}", flush=True)

        async with contextlib.AsyncExitStack() as stack:
            for srv in servers:
                await stack.enter_async_context(srv)
            print(
                f"env reader on localhost:{ENV_PORT}, result writer on localhost:{RES_PORT}",
                flush=True,
            )
            try:
                await asyncio.wait_for(writer.done, timeout=900)
            except asyncio.TimeoutError:
                print("TIMEOUT waiting for results")
                return 1
            # Give the component a moment to close the out port.
            await asyncio.sleep(0.5)

        return check(reader, writer)


def check(reader, writer):
    failures = []
    print(f"read {reader.sent} IP(s), received {len(writer.received)} back, closed={writer.closed}")
    for i, e in enumerate(writer.received):
        summary = e.get("content", "")
        if summary:
            try:
                d = json.loads(summary)
                summary = f"customId={d.get('customId')!r} errors={d.get('errors')} sections={len(d.get('data', []))}"
            except json.JSONDecodeError:
                summary = f"<unparseable, {len(summary)} chars>"
        print(f"  [{i}] type={e['type']} attrs={e['attributes']} {summary}")

    types = [e["type"] for e in writer.received]
    expected = ["openBracket"] + ["standard"] * N_ENVS + ["closeBracket"]
    if types != expected:
        failures.append(f"IP types {types} != {expected}")

    # Brackets must come through with their attributes intact.
    for e in writer.received:
        if e["type"] in ("openBracket", "closeBracket"):
            if e["attributes"].get("marker") != e["type"]:
                failures.append(f"{e['type']} lost its marker attribute: {e['attributes']}")

    # Each result must carry the originating IP's attribute and a real run.
    for i, e in enumerate(e for e in writer.received if e["type"] == "standard"):
        if e["attributes"].get("trace") != f"env{i}":
            failures.append(f"result {i} lost the trace attribute: {e['attributes']}")
        payload = e["attributes"].get(TO_ATTR) if TO_ATTR else e.get("content")
        if not payload:
            failures.append(f"result {i} has no payload")
            continue
        d = json.loads(payload)
        if d.get("customId") != f"env{i}":
            failures.append(f"result {i} customId={d.get('customId')!r}")
        if d.get("errors"):
            failures.append(f"result {i} errors={d['errors']}")
        rows = len(d["data"][1]["results"][0]) if d.get("data") else 0
        if rows != 2557:
            failures.append(f"result {i} has {rows} daily rows, expected 2557")

    if not writer.closed:
        failures.append("the component never closed the result port")

    if failures:
        print("\nFAIL:")
        for f in failures:
            print(f"  {f}")
        return 1
    print("\nOK - brackets forwarded, attributes carried over, every Env produced a full run")
    return 0


sys.exit(asyncio.run(main()))
