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
    odin/build/monica-capnp-server.exe --port 6789 --output_srs

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

## Static (single-binary) variant

Same tests, against a binary with no DLL next to it:

    pixi run capnp-shim-static
    pixi run build-standalone
    # copy just the .exe somewhere empty, then:
    MONICA_CAPNP_SCHEMA_DIR=$SCHEMAS ./monica-capnp-server.exe --port 6790 --output_srs

Note `MONICA_CAPNP_SCHEMA_DIR`: unlike the C++, which links generated code, this
server parses the `.capnp` files at runtime and defaults to a path relative to the
executable — which no longer resolves once the exe is moved out of `odin/build/`.
