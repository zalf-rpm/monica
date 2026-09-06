"""Shared connection helper for the Cap'n Proto test clients.

The server's bootstrap capability is a Restorer (as in the C++), so reaching
MONICA normally means connect -> restore(token) -> cast. It can also serve MONICA
directly (--serve-as-bootstrap), in which case the bootstrap IS the EnvInstance.
Both spellings are accepted here so the same test works either way:

    localhost:6789                      bootstrap is MONICA itself
    capnp://localhost:6789              same, spelled as a URL
    capnp://localhost:6789/<token>      bootstrap is the Restorer; restore first

Schemas are loaded with plain capnp.load() from the same raw .capnp files the
Odin server parses at runtime, rather than the generated mas.schema.* stubs, so
both sides are provably talking about the same schema text. (sturdyref_client.py
deliberately does the opposite - it uses the installed stubs and zalfmas_common's
own ConnectionManager, to prove a real client works unchanged. Do not mix the two
in one process: loading a schema twice through different loaders crashes pycapnp.)
"""

import capnp

capnp.remove_import_hook()

_cache = {}

# pycapnp tears the RPC system down when the TwoPartyClient is collected, and a
# capability obtained through it then fails with "RpcSystem was destroyed". The
# caller only wants the EnvInstance, so the client has to be kept alive here.
_keepalive = []


class Schemas:
    def __init__(self, root):
        self.common = capnp.load(f"{root}/common/common.capnp", imports=[root])
        self.model = capnp.load(f"{root}/model/model.capnp", imports=[root])
        self.persistence = capnp.load(f"{root}/persistence/persistence.capnp", imports=[root])
        self.climate = capnp.load(f"{root}/climate/climate.capnp", imports=[root])
        self.soil = capnp.load(f"{root}/soil/soil.capnp", imports=[root])
        self.fbp = capnp.load(f"{root}/fbp/fbp.capnp", imports=[root])


def schemas(root):
    if root not in _cache:
        _cache[root] = Schemas(root)
    return _cache[root]


def parse_target(target):
    """-> (host, port, token or None)"""
    rest = target[len("capnp://") :] if target.startswith("capnp://") else target
    token = None
    if "/" in rest:
        rest, _, token = rest.partition("/")
        token = token or None
    host, _, port = rest.rpartition(":")
    return host, int(port), token


async def open_monica(target, schema_root):
    """-> (schemas, EnvInstance client), restoring first if the target has a token."""
    s = schemas(schema_root)
    host, port, token = parse_target(target)

    stream = await capnp.AsyncIoStream.create_connection(host=host, port=port)
    client = capnp.TwoPartyClient(stream)
    _keepalive.append(client)
    bootstrap = client.bootstrap()

    if token is None:
        return s, bootstrap.cast_as(s.model.EnvInstance)

    restorer = bootstrap.cast_as(s.persistence.Restorer)
    cap = (await restorer.restore(localRef={"text": token})).cap
    if cap is None:
        raise RuntimeError(f"restore({token!r}) returned no capability")
    return s, cap.as_interface(s.model.EnvInstance)
