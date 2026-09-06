"""Exercise the two capability paths of monica-capnp-server's run(): env.timeSeries
and env.soilProfile.

Unlike run_client.py (which lets the server read the climate CSV itself), this
hosts a climate.capnp TimeSeries and a soil.capnp Profile *in the client* and
passes them as call parameters — so the server has to call back into capabilities
belonging to the very connection it is dispatching on. That is the case the
shim only permits from an async handler, and it is what
run_monica_capnp.odin's Run_Call state machine exists for.

The check is a differential one, run twice against the same server:

  baseline     env carries the climate as an inline climateCSV and the soil as
               ordinary SoilProfileParameters JSON — the paths already known to
               match monica-run (see run_client.py).
  capabilities env carries neither; the server has to fetch both over RPC.

Both must produce byte-identical output. To make that achievable, every value is
first rounded through float32 and the *rounded* values are what both runs use:
capnp's TimeSeries.dataT is List(List(Float32)) and Layer.size/f32Value are
Float32, so an un-rounded baseline would differ in the last bits for no
interesting reason.

  python capability_client.py <zalfmas_capnp_schemas-dir> <host:port or monicaSR>
"""

import asyncio
import json
import os
import struct
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
climate = _schemas.climate
soil = _schemas.soil
model = _schemas.model


def f32(x):
    """Round a float through float32, the width capnp carries these values at."""
    return struct.unpack("f", struct.pack("f", float(x)))[0]


# --- the fixture, in both shapes ---------------------------------------------


def read_climate():
    """-> (header_elements, rows) from climate-min.csv, all values float32-rounded.

    Header line 1 names the columns, line 2 the units (the csvViaHeaderOptions
    the fixture uses declares 2 header lines).
    """
    path = os.path.join(FIXTURE, "climate-min.csv")
    with open(path, encoding="utf-8") as f:
        lines = [ln.strip() for ln in f if ln.strip()]
    names = [c for c in lines[0].split(",") if c]
    elements = names[1:]  # everything after iso-date
    rows = []
    for ln in lines[2:]:
        cells = ln.split(",")
        rows.append((cells[0], [f32(c) for c in cells[1 : 1 + len(elements)]]))
    return elements, rows


# site-min.json's SoilProfileParameters, as the soil.capnp Layer properties the
# server maps back (see capnp_helper.odin's soil_property_to_layer_json):
#   size            -> Thickness
#   organicCarbon   -> SoilOrganicCarbon   (no scaling)
#   rawDensity      -> SoilRawDensity      (no scaling)
#   soilType        -> KA5TextureClass
LAYERS = [
    {"thickness": 0.3, "soc": 0.8, "raw_density": 1446, "ka5": "Sl2"},
    {"thickness": 0.1, "soc": 0.15, "raw_density": 1446, "ka5": "Sl2"},
    {"thickness": 1.6, "soc": 0.05, "raw_density": 1446, "ka5": "Sl2"},
]


def layers_as_json():
    return [
        {
            "Thickness": f32(l["thickness"]),
            "SoilOrganicCarbon": f32(l["soc"]),
            "SoilRawDensity": f32(l["raw_density"]),
            "KA5TextureClass": l["ka5"],
        }
        for l in LAYERS
    ]


def base_env():
    with open(os.path.join(FIXTURE, "sim-min.json")) as f:
        sim_json = json.load(f)
    with open(os.path.join(FIXTURE, "site-min.json")) as f:
        site_json = json.load(f)
    with open(os.path.join(FIXTURE, "crop-min.json")) as f:
        crop_json = json.load(f)
    site_json["SiteParameters"]["SoilProfileParameters"] = layers_as_json()
    env = monica_io.create_env_json_from_json_config(
        {"crop": crop_json, "site": site_json, "sim": sim_json, "climate": ""}
    )
    env["customId"] = "capability-test"
    return env, sim_json


def climate_csv_text(elements, rows):
    """The same data as an inline CSV, for the baseline run. repr() of a
    float32-rounded double round-trips exactly through the server's parser."""
    out = ["iso-date," + ",".join(elements), "," + ",".join("" for _ in elements)]
    for date, values in rows:
        out.append(date + "," + ",".join(repr(v) for v in values))
    return "\n".join(out)


# --- hosted capabilities ------------------------------------------------------


class TimeSeriesImpl(climate.TimeSeries.Server):
    """Only range/header/dataT are implemented - the three run-monica-capnp.cpp's
    dataAccessorFromTimeSeries actually sends."""

    def __init__(self, elements, rows):
        self.elements = elements
        self.rows = rows
        self.calls = []

    async def range(self, _context, **kwargs):
        self.calls.append("range")
        first, last = self.rows[0][0], self.rows[-1][0]
        for name, iso in (("startDate", first), ("endDate", last)):
            y, m, d = (int(p) for p in iso.split("-"))
            getattr(_context.results, name).year = y
            getattr(_context.results, name).month = m
            getattr(_context.results, name).day = d

    async def header(self, _context, **kwargs):
        self.calls.append("header")
        _context.results.header = self.elements

    async def dataT(self, _context, **kwargs):
        self.calls.append("dataT")
        # transposed: one inner list per element
        _context.results.data = [
            [row[1][i] for row in self.rows] for i in range(len(self.elements))
        ]


class ProfileImpl(soil.Profile.Server):
    def __init__(self):
        self.calls = []

    async def data(self, _context, **kwargs):
        self.calls.append("data")
        _context.results.layers = [
            {
                "size": l["thickness"],
                "properties": [
                    {"name": "organicCarbon", "f32Value": l["soc"]},
                    {"name": "rawDensity", "f32Value": l["raw_density"]},
                    {"name": "soilType", "type": l["ka5"]},
                ],
            }
            for l in LAYERS
        ]


# --- the two runs -------------------------------------------------------------


async def run_once(monica, env, time_series=None, profile=None):
    req = monica.run_request()
    rest = req.env.rest.as_struct(common.StructuredText)
    rest.value = json.dumps(env)
    rest.type = "json"
    if time_series is not None:
        req.env.timeSeries = time_series
    if profile is not None:
        req.env.soilProfile = profile
    res = await req.send()
    return json.loads(res.result.as_struct(common.StructuredText).value)


async def main():
    env, sim_json = base_env()
    elements, rows = read_climate()

    baseline_env = dict(env)
    baseline_env["climateCSV"] = climate_csv_text(elements, rows)
    baseline_env["csvViaHeaderOptions"] = sim_json["climate.csv-options"]

    # The capability run supplies neither the CSV nor the soil profile inline.
    cap_env = dict(env)
    cap_env["params"] = json.loads(json.dumps(env["params"]))
    del cap_env["params"]["siteParameters"]["SoilProfileParameters"]

    async with capnp.kj_loop():
        _, monica = await open_monica(TARGET, SCHEMA_ROOT)

        print("== baseline (inline CSV + inline soil profile) ==")
        baseline = await run_once(monica, baseline_env)
        report(baseline)

        print("== capabilities (TimeSeries + Profile over RPC) ==")
        ts, prof = TimeSeriesImpl(elements, rows), ProfileImpl()
        got = await run_once(monica, cap_env, time_series=ts, profile=prof)
        report(got)
        print(f"  TimeSeries methods called: {sorted(ts.calls)}")
        print(f"  Profile methods called:    {sorted(prof.calls)}")

        if not ts.calls:
            print("FAIL - the server never called the TimeSeries capability")
            sys.exit(1)
        if not prof.calls:
            print("FAIL - the server never called the Profile capability")
            sys.exit(1)

        if json.dumps(baseline, sort_keys=True) == json.dumps(got, sort_keys=True):
            print("\nOK - capability run is byte-identical to the inline-data run")
            return
        print("\nMISMATCH between the two runs:")
        diff(baseline, got)
        sys.exit(1)


def report(result):
    print(f"  customId = {result.get('customId')!r}")
    print(f"  errors   = {result.get('errors')}")
    print(f"  warnings = {result.get('warnings')}")
    for d in result.get("data", []):
        n = len(d.get("results", []))
        print(f"  section {d.get('origSpec')!r}: {n} ids, {len(d['results'][0]) if n else 0} rows")


def diff(a, b):
    if a.get("errors") != b.get("errors"):
        print(f"  errors: {a.get('errors')} vs {b.get('errors')}")
    for da, db in zip(a.get("data", []), b.get("data", [])):
        name = da.get("origSpec")
        ca, cb = da["results"], db["results"]
        if len(ca) != len(cb):
            print(f"  {name}: {len(ca)} vs {len(cb)} output ids")
            continue
        shown = 0
        for i, (colA, colB) in enumerate(zip(ca, cb)):
            for r, (va, vb) in enumerate(zip(colA, colB)):
                if va != vb and shown < 5:
                    print(f"  {name} col {i} row {r}: {va!r} vs {vb!r}")
                    shown += 1


asyncio.run(main())
