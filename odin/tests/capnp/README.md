# Cap'n Proto server tests

Integration tests for `cmd/monica-capnp-server` (the port of
`src/run/monica-capnp-server-main.cpp` + `src/run/run-monica-capnp.cpp`). They are
plain scripts rather than `odin test` cases because they need a *client* speaking
real Cap'n Proto RPC over a socket, which is the whole point — a self-test inside
the same process would not prove the wire format.

The client is pycapnp, from the environment that already exists for the ZMQ
producer/consumer scripts:

    installer/Hohenfinow2/python/.pixi/envs/default/python.exe

Both scripts deliberately `capnp.load()` the raw `.capnp` files the Odin server
itself parses at runtime, rather than the generated `mas.schema.*` stubs, so both
sides provably agree on the schema text.

## Prerequisites

    export MONICA_PARAMETERS=<...>/monica-parameters     # server AND client need it
    SCHEMAS=<repo>/odin/support/capnp/shim/external/mas_capnproto_schemas/zalfmas_capnp_schemas
    PY=<repo>/installer/Hohenfinow2/python/.pixi/envs/default/python.exe

Build and start the server (loopback only — binding `*` trips a Windows Firewall
prompt that needs admin rights):

    pixi run capnp-shim          # once: the C++ shim (needs cmake + $VCPKG_ROOT)
    pixi run build monica-capnp-server
    odin/build/monica-capnp-server.exe --port 6789 --srt monica-test-token --output_srs
    # prints: monicaSR=capnp://localhost:6789/monica-test-token

The bootstrap capability is a **Restorer**, as in the C++, so reaching MONICA
means connect → `restore(token)` → cast. `--srt` pins the token so the tests know
it up front instead of having to scrape the server's stdout.

Every client below takes either spelling of the target and does the right thing
(see `_target.py`):

    capnp://localhost:6789/monica-test-token   restore first (the default server)
    localhost:6789                             bootstrap is MONICA itself
                                               (server started --serve-as-bootstrap)

## `spike_client.py` — the plumbing check

    $PY spike_client.py $SCHEMAS localhost:6789

Calls `info()` and a trivial `run()`. This is what established that the dynamic
API can host `EnvInstance` at all, which was the open question: it is a *generic*
interface (`EnvInstance(RestInput, Output)`), so the shim gets the unbound schema
in which both parameters degrade to AnyPointer. Checks, in order:

1. `capnp_dyn_server_new` accepts an unbound generic `InterfaceSchema`;
2. an inherited method (`info`, from `Identifiable`) dispatches;
3. `env.rest` arrives as AnyPointer and reads back as a `common.capnp`
   `StructuredText`;
4. `result :Output` — a bare unbound generic — can be written back.

## `run_client.py` + `compare_with_monica_run.py` — the real acceptance test

Runs the Hohenfinow2 fixture through the server and diffs the result against
`monica-run`'s own CSV for the same fixture:

    cd installer/Hohenfinow2
    ../../odin/build/monica-run.exe -o ./capnp_check/ref.csv sim-min.json
    cd ../..
    $PY odin/tests/capnp/run_client.py $SCHEMAS localhost:6789 /tmp/capnp.json
    $PY odin/tests/capnp/compare_with_monica_run.py \
        installer/Hohenfinow2/capnp_check/ref.csv /tmp/capnp.json

`run_client.py` builds the env exactly the way
`installer/Hohenfinow2/python/run-producer.py` builds it for the ZMQ server (same
`monica_io.create_env_json_from_json_config`, same `csvViaHeaderOptions` /
`pathToClimateCSV`), so the two servers are being fed the same thing.

Expected: **53798 cells, all identical**. The CSV rounds and the JSON does not, so
the comparison renders each JSON value at the precision the CSV printed; it also
flattens layer-range outputs, where one output id (`SOC`) spans several CSV
columns (`SOC_1`, `SOC_2`, `SOC_3`).

## `capability_client.py` — the env.timeSeries / env.soilProfile paths

    $PY capability_client.py $SCHEMAS localhost:6789

The other two scripts let the server read the climate CSV itself. This one hosts
a `climate.capnp` TimeSeries and a `soil.capnp` Profile **in the client** and
passes them as call parameters, so the server has to call back into capabilities
belonging to the very connection it is dispatching on — the case that forces the
async handler (see `monica/capnp/run_monica_capnp.odin`'s header).

It runs the same fixture twice against the same server: once with the data inline
(`climateCSV` + `SoilProfileParameters`), once with neither, forcing the RPC path.
Both must come back byte-identical. Every value is first rounded through float32,
because `TimeSeries.dataT` is `List(List(Float32))` and `Layer.size`/`f32Value`
are `Float32` — otherwise the two runs would differ in the last bits for no
interesting reason. The script also asserts the capabilities were actually called
(`range`/`header`/`dataT` and `data`), so it cannot pass by silently skipping them.

## `malformed_client.py` — envs the server cannot run

    $PY malformed_client.py $SCHEMAS localhost:6789

Odin has no exceptions, so the C++'s `try/catch` around `runMonica` cannot be
ported: anything that panics inside the model aborts the whole process, and a
server is reachable by any client. The concrete case was an env with **no soil
profile** — every soil module indexes the soil column unconditionally, so an
empty one is an out-of-bounds access (undefined behaviour in the C++, a
process-aborting bounds check in Odin). `run_monica.odin` now refuses such a run
up front and reports it through `Output.errors`.

This sends an empty object, a `customId`-only env, an env with the soil profile
removed, and a non-JSON `rest`, requires each to come back as a normal result
carrying an error, and then runs the real fixture to prove the server is still
alive. `monica-run` prints the same error and exits 1 rather than writing an
unexplained empty CSV.

## `sturdyref_client.py` — an existing ZALF client, unmodified

    $PY sturdyref_client.py capnp://localhost:6789/monica-test-token

The odd one out, deliberately. Every other client here loads the raw `.capnp`
files so both sides provably share the same schema text; this one uses the
**installed `mas.schema.*` stubs and `zalfmas_common`'s own `ConnectionManager`**,
none of which this repo controls. It hands the `monicaSR=` line straight to
`conman.connect(sr, cast_as=model_capnp.EnvInstance)` — so if it passes, a real
MONICA client reaches this server without changes. It also checks that an unknown
token yields a null capability that only fails when called, which is what the C++
Restorer does (`KJ_IF_MAYBE(cap, maybeCap)` leaves the result unset).

**Do not mix the two styles in one process.** Loading a schema through both
`capnp.load()` and the generated bundle crashes pycapnp hard (`0xC0000409`,
no traceback) — which is why this client does not import `_target`.

## `fbp_harness.py` — monica-capnp-fbp-component

Stands in for an FBP runtime: hosts the channel endpoints the component connects
to, each as the bootstrap of its own loopback port (so the sturdy refs need no
token), then checks what came back.

    # inline MONICA (no monica_sr configured)
    $PY fbp_harness.py $SCHEMAS 6900 6901 &
    odin/build/monica-capnp-fbp-component.exe \
        --env_in_sr capnp://localhost:6900 --result_out_sr capnp://localhost:6901

    # remote MONICA, result into an attribute instead of the IP content
    odin/build/monica-capnp-server.exe --port 6910 --srt fbp-token --output_srs &
    $PY fbp_harness.py $SCHEMAS 6911 6912 --conf-port 6913 \
        --monica-sr capnp://localhost:6910/fbp-token --to-attr monica_result &
    odin/build/monica-capnp-fbp-component.exe \
        --env_in_sr capnp://localhost:6911 --result_out_sr capnp://localhost:6912 \
        --config_in_sr capnp://localhost:6913

The `env` reader serves an openBracket IP, two Env IPs and a closeBracket IP,
then `done`. Expected: brackets forwarded through with their attributes intact,
one result IP per Env IP carrying the originating IP's attribute and a full
five-section run with the right `customId`, and the out port closed at the end.
Both MONICA paths are covered — inline (the component runs the model itself) and
remote (it calls an EnvInstance over a sturdy ref).

Two pycapnp things this took to get right, worth knowing before writing another
harness: a `create_server` callback must stay awaited for the life of the
connection (`await TwoPartyServer(...).on_disconnect()`), or the peer just sees
"Peer disconnected"; and server methods that receive a struct with a **union**
must be defined as `<name>_context(self, context)`, because the plain
`<name>(self, _context, **kwargs)` form eagerly reads every field and throws on
the inactive union members (`Channel.Msg` is exactly that shape).

## Multiple requests

Worth doing explicitly after any change to allocator handling: send three runs
over one connection and check all three come back. Until the module-level
parameter caches were moved onto `tools.process_cache_allocator`, request 2 took
the server down — the caches were built from request 1's arena and read after it
was destroyed. `monica-zmq-server` had the identical bug on its second message.

## Static (single-binary) variant

Same tests, against a binary with no DLL next to it:

    pixi run capnp-shim-static
    pixi run build-standalone
    # copy just the .exe somewhere empty, then:
    MONICA_CAPNP_SCHEMA_DIR=$SCHEMAS ./monica-capnp-server.exe --port 6790 --output_srs

Note `MONICA_CAPNP_SCHEMA_DIR`: unlike the C++, which links generated code, this
server parses the `.capnp` files at runtime and defaults to a path relative to the
executable — which no longer resolves once the exe is moved out of `odin/build/`.
