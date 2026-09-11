# Plan: serialize/deserialize the whole simulation state

Goal: restore what the C++ MONICA could do and this port dropped — write the **complete
`MonicaModel` state** to a file mid-run and reconstruct a running model from it. JSON first,
Cap'n Proto second, one reflection-driven walker underneath both.

Replaces the two "dropped, Cap'n Proto" entries in `plan-odin.md`: `src/worksteps/
save-monica-state.cpp` (see `odin/monica/run/workstep.odin`'s header) and run-monica's
`loadSerializedMonicaStateAtStart` / `serializeMonicaStateAtEnd` (see
`odin/monica/run/run_monica.odin`'s header). Both parameter fields are already parsed by
`site_sim_parameters.odin` and currently do nothing.

Read `odin/CONVENTIONS.md` first; §9 (path-over-lambda) and `plan-reflective-outputs.md` are the
precedent this plan copies — same "compile once against a typeid, then walk pointers" shape as
`odin/support/reflectpath`.

---

## 0. The two things that decide the design

**(a) The field names are about to change.** Struct member names in the Odin tree will be renamed;
today's `monica_state.capnp` no longer matches them and will match even less. So:

- **JSON maps 1:1 to the Odin field names.** No name table, no tags, nothing to maintain — a rename
  propagates into the format for free. The cost is that JSON state files are tied to the current
  field names (§6).
- **Cap'n Proto cannot map 1:1.** capnp field names must be `lowerCamelCase` and, more importantly,
  the *ordinal* (`@7`) is the wire identity — a name is just a label for JSON and codegen. So the
  capnp side needs an explicit per-field mapping regardless of what the Odin names are.

**(b) Therefore: tag now, rename later.** Put the capnp name on the field *before* the renaming
pass, as a struct tag:

```odin
vm_SnowDensity: f64 `capnp:"snowDensity"`,
```

The tag travels with the field through any rename, so a rename can never silently repoint a wire
field. That single property is the reason step 1 comes before everything else, and it is cheap:
§1.3 shows **94% of the tags can be extracted mechanically from the C++ `serialize()` bodies**,
which are the authoritative pairing.

---

## 1. Ground truth and inventory

### 1.1 What the C++ actually did

| | |
| --- | --- |
| Schema | `mas_cpp_misc/mas_capnproto_schemas/zalfmas_capnp_schemas/model/monica/monica_state.capnp` — `RuntimeState` → `MonicaModelState` + 13 sibling structs |
| Write | `monicamodel::serialize` (`src/core/monica-model.cpp:237`), one hand-written `builder.setX(model->y)` per field, recursing into each submodule's `serialize` |
| Read | `monicamodel::deserialize` (`:87`), the mirror, **plus explicit re-wiring** — `soilcolumn::putCrop`, `soilMoisture->cropModule = ...`, `makeSoilTransport(soilColumn, ..., &model->sitePs, ...)` |
| File | `src/worksteps/save-monica-state.cpp` — `capnp::messageToFlatArray`, or `capnp::JsonCodec::encode` when `"toJson": true` |
| Restore | `run-monica.cpp:552 deserializeFullState` → `makeMonicaModel(reader)` |

Note what "JSON" meant there: **capnp's `JsonCodec` applied to the capnp message**, i.e. one schema,
two encodings. This plan deliberately breaks that tie (per §0a) and makes JSON its own, simpler
dialect — with an opt-in compat mode (§4, "Name modes") for reading files the C++ wrote.

Two C++ behaviours worth knowing before copying them:

- `Tools::Date::serialize` writes **only** `_d/_m/_y` (`mas_cpp_misc/tools/date.cpp:126`). The
  `useLeapYears` / `isRelative` flags — Odin's `_no_leap_years`, `_common_year_dim`, `_is_relative`
  — are silently lost. Ours can do better (§6).
- `runMonicaIC` restores the model but **not** the crop-rotation iterators; the C++ has the
  `while (critPos-- > 0 ...)` re-positioning commented out with `//!!! attention doesn't check
  currently if the env is the same as when the state had been serialized !!!`. §7 keeps the same
  limitation, explicitly.

### 1.2 What has to be walked, on the Odin side

Reachability from `Monica_Model` (measured, not estimated):

| | count |
| --- | ---: |
| struct types reachable | **37** |
| fields across them | **869** |
| back-pointers (not owned — skip + rewire) | 22 |
| owning pointers (serialized) | 2 — `Monica_Model.currentCropModule`, `Crop_Module.perennialCropParams` |
| `[dynamic]T` fields | 127 (mostly `[dynamic]f64`, one `[dynamic]Soil_Layer`, one `[dynamic]Aom_Properties`) |
| `map` fields | 9 in the reachable set — `map[string]bool` (a *set*), `map[int]f64`, `map[clim.ACD]f64` |
| `Maybe(T)` fields | 1 reachable (`Crop_Parameters.__enable_vernalisation_factor_fix__`) |
| proc-typed fields | 3, all in `Crop_Module` (`fireEvent`, `addOrganicMatter`, `getSnowDepthAndCalcTempUnderSnow`) |
| `rawptr` | 1 (`Crop_Module.intercropping`, inert) |

The heavy ones: `Crop_Module` 174 fields, `Species_Parameters` 57, `Soil_Moisture` 50,
`Soil_Organic` / `Soil_Organic_Module_Parameters` 43 each.

The proc fields are the easy case here and not the hard one they look like: this port already
resolved "Odin procs have no capture" with package-level callbacks bound to `g_current_model`
(`monica_model.odin:155-200`), so rewiring after a load is three assignments, not a closure problem.

### 1.3 Why `core:encoding/json` is not the answer

`core/encoding/json/marshal.odin:258` returns `.Unsupported_Type` for every `^T` that is not its own
`Null`. `Monica_Model` has 24 pointer fields; four of them (`Soil_Moisture.crop_module`,
`Soil_Organic.crop_module`, `Soil_Transport.cropModule`, `Soil_Column.cropModule`) plus
`Crop_Module.soilColumn` form **cycles**, which a naive walker would follow forever. Neither
marshal nor unmarshal has a skip mechanism, and neither can express "this `map[string]bool` is a
set" or "truncate this array to N entries". So: our own walker, in `support/`.

### 1.4 How much of the capnp tagging is mechanical

A throwaway regex pass over every `::serialize` body in `src/**/*.cpp` (three statement forms:
`builder.setX(v->y)`, `setCapnpList(v->y, builder.initX(..))`, `ns::serialize(&v->y,
builder.initX())`) pairs **565 of 604 fields — 94%**. The 39 left over are the recognisable
container forms, and a slightly better parser gets most of them too:

| Form | Where |
| --- | --- |
| `List(List(Float64))` loops | `CultivarParameters.pc_AssimilatePartitioningCoeff`, `pc_OrganSenescenceRate` |
| list-of-struct loops | `SiteParameters.vs_SoilParameters`, `SoilLayerState.vo_AOM_Pool`, `SoilColumnState._delayedNMinApplications` |
| map → list-of-pairs loops | `EnvironmentParameters.p_AtmosphericCO2s/O3s`, `MeasuredGroundwaterTableInformation.groundwaterInfo`, `MonicaModelState.climateData` |
| method-call serialize | `Date`, `Voc_Emissions.guentherEmissions/jjvEmissions`, `vocSpecies` |
| enum mapping | `AutomaticHarvestParameters._harvestTime` |

Per-struct coverage (auto / hand): `CropModuleState` 142/4, `SpeciesParameters` 55/0,
`SticsParameters` 39/0, `SoilOrganicModuleParameters` 37/0, `CultivarParameters` 33/10,
`MonicaModelState` 29/6, `SoilMoistureModuleParameters` 23/0, `SnowModuleState` 20/0.

---

## 2. Architecture

Three tiers, mirroring `support/reflectpath` → `monica/io/output_paths.odin` → `monica/io`:

```
odin/support/serde/          generic. no monica import. reflection + a value tree + JSON.
  plan.odin                  Type_Plan: compile once per typeid (offsets, kinds, names, flags)
  value.odin                 Value: null|bool|int|uint|float|text|list|struct — the intermediate
  walk.odin                  any -> Value   (encode)  /  Value -> any  (decode/bind)
  json.odin                  Value <-> JSON bytes
odin/support/capnpwire/      generic capnp wire format, driven by a layout table (phase 2)
odin/monica/state/           the monica glue
  schema_gen.odin            GENERATED capnp layout table (phase 2)
  state.odin                 save/load MonicaModel, the rewire pass, the custom hooks
```

### 2.1 `Type_Plan` — compile once

Same contract as `reflectpath`: everything that needs a *name* or a `Type_Info` happens once, at
plan time, cached in a `map[typeid]^Type_Plan`. Walking is offsets and stores.

```odin
Field_Plan :: struct {
    name:      string,      // Odin field name, or the json: tag override
    capnp:     string,      // capnp: tag, "" if none
    offset:    uintptr,
    kind:      Field_Kind,  // SCALAR|STRUCT|LIST|MAP_PAIRS|MAP_SET|MAYBE|OWNED_PTR|SKIP
    elem:      typeid,
    key_kind:  Key_Kind,    // MAP_*: STRING|INT|ENUM
    hook:      int,         // index into a per-root hook table, -1 if none
}
```

### 2.2 The value tree, not a streaming sink

`walk` produces a `Value` tree and the backends encode it, rather than each backend driving the
walk. Reasons: capnp needs list lengths and struct sizes up front anyway; the tree is
what the existing dynamic-shim C API already speaks (`capnp_dyn_value_new_struct` /
`capnp_dyn_struct_set`), so a third backend through the shim is nearly free; and a tree makes the
round-trip test (`encode → decode → encode`, compare trees) trivial and format-independent. The
whole tree lives in one `virtual.Arena` freed after the write, same as the config JSON
(CONVENTIONS §3).

Streaming is the optimisation to reach for only if a state file turns out to be large enough to
matter. A Hohenfinow2 state is ~1 MB of JSON; it does not.

### 2.3 Annotations

Only two tag keys, and neither is needed for the common case:

| Tag | Meaning |
| --- | --- |
| `capnp:"snowDensity"` | the capnp field name. **Frozen wire identity** — never edit it during a rename. |
| `serde:"-"` | not part of the state (back-pointers, proc fields, `intercropping`) |
| `serde:"own"` | owning pointer: nil → null; on load, allocate |
| `serde:"set"` | `map[K]bool` is a set → a sorted list of keys |
| `serde:"pairs:acd,value"` | `map[K]V` → list of two-field structs, with the capnp member names |
| `serde:"hook:climate_data"` | hand-written encode/decode for this field (§2.5) |
| `json:"name"` | override the JSON name; **absent means the Odin field name**, which is the point of §0a |

Everything with no tag at all — the overwhelming majority — is walked structurally.

### 2.4 Pointers: skip, then rewire

`serde:"-"` on all 22 back-pointers; after `decode`, one hand-written pass restores them. It is a
transcription of `make_monica_model`'s wiring block plus `monicamodel::deserialize`'s
`putCrop`/`cropModule =` lines — ~30 lines, in `monica/state/state.odin`, with the C++ line
references next to each:

```odin
state_rewire :: proc(m: ^Monica_Model) {
    g_current_model = m                       // the 3 callbacks (monica_model.odin:155)
    m.soilColumn.cropModule = m.currentCropModule
    m.soilMoisture.soil_column = &m.soilColumn
    ...
    if m.currentCropModule != nil {
        m.currentCropModule.fireEvent = monica_model_fire_event_cb
        ...
    }
}
```

A test asserts every `serde:"-"` pointer field in the reachable set is non-nil after a rewire (or
is on a documented "legitimately nil" list), so a field added later cannot be forgotten.

### 2.5 The hand-written hooks

Five, all of them things the C++ also special-cased:

1. `Monica_Model.climateData` — truncate to the last `simPs.noOfPreviousDaysSerializedClimateData`
   entries (`monica-model.cpp:308-317`), each `map[clim.ACD]f64` as a sorted pair list.
2. `currentEvents` / `previousDaysEvents` — `map[string]bool` as a sorted `[]string`.
3. `Date` — `{y,m,d}` only, if we keep C++ fidelity; see §6 for the alternative.
4. `Environment_Parameters.p_AtmosphericCO2s` / `p_AtmosphericO3s`,
   `Measured_Groundwater_Table_Information.groundwaterInfo` — sorted pair lists.
5. `Automatic_Harvest_Parameters._harvestTime` — enum ↔ name.

**Every map is emitted key-sorted.** Odin's map iteration order is unstable; without sorting, two
serializations of the same state differ, which would make the round-trip gate untestable. This is
the same reason `reflectpath` refuses a map as an open dimension.

---

## 3. Cap'n Proto (phase 2)

### 3.1 The schema has to be re-cut, and that is a decision

Since the Odin field names are being changed, `monica_state.capnp` no longer describes this model.
Two ways out, and they differ in exactly one thing — whether an existing C++-written **binary**
state file still loads:

| | (a) regenerate names, **keep ordinals** — recommended | (b) keep the schema as it is |
| --- | --- | --- |
| capnp field names | regenerated from the Odin names (lowerCamelCase) | unchanged, drift from the Odin names forever |
| `@N` ordinals | preserved for every field that already had one; new fields append | unchanged |
| binary compat with C++ MONICA | **yes** — ordinals are the wire identity | yes |
| capnp-JSON compat with C++ MONICA | no (names moved) | yes |
| maintenance | generator + drift test | 869 hand-maintained tags |

Recommend (a): it costs nothing (ordinals carry the compatibility, names carry the readability) and
it means the schema tracks the Odin structs instead of documenting a C++ tree that no longer exists.
Under (a) the `capnp:` tag still exists and is still frozen — it is what pins the *ordinal* lookup
across a rename.

Either way the schema stays a first-class artifact: a generator emits
`monica_state.capnp` from the annotated Odin structs, and a test fails if the checked-in schema and
the structs have drifted.

### 3.2 Encoding: pure Odin, using the offsets the schema repo already ships

`mas_cpp_misc/mas_capnproto_schemas/gen/capnp_offsets/model/monica/monica_state.capnp` is the
schema **with the computed layout in comments** — exactly what a wire encoder needs:

```capnp
struct AOMProperties @0xe3512e62df901c18 {  # 152 bytes, 0 ptrs
  aomSlow @0 :Float64;  # bits[0, 64)
  ...
struct CropState @0x8b008567c93f7c7d {  # 16 bytes, 11 ptrs
  seedDate @3 :import "/common/date.capnp".Date;  # ptr[3]
```

So `odin/tools/gen_capnp_layout.py` turns those files into an Odin table (struct id, data words,
pointer count, per-field bit offset / pointer index / type / default), and `support/capnpwire`
implements the wire format against that table: single-segment message, struct and list pointers,
`Text`, composite lists, default-value XOR. ~600 lines for the writer, ~600 for the reader. No
native dependency, so `monica-run` keeps working with no shim DLL.

**Alternative, if that is more than it is worth:** add two entry points to
`odin/support/capnp/shim_dynamic` — `capnp_dyn_value_to_message_bytes(schema, struct_name, value)`
and its inverse, plus `JsonCodec` encode/decode — and hand it the §2.2 value tree, which is already
the shape the shim's C API takes. ~150 lines of C++, byte-correct by construction, and it also
gives capnp-JSON for free. The cost is that saving state then requires the shim DLL and the
`.capnp` files at runtime. Worth doing *first* as an oracle for the pure-Odin encoder even if (a)
is the destination.

---

## 4. Steps

Each step ends at a gate. `bash odin/tests/diff_outputs.sh` must stay byte-identical throughout —
none of this may perturb the simulation.

### Step 1 — tags, before the renaming pass · gate: nothing changed

`odin/tools/gen_serde_tags.py`, modelled on `gen_output_aliases.py` (print, don't write; the rows
are hand-audited). Extracts the §1.3 pairs from the C++ `serialize()` bodies, maps C++ struct names
to Odin ones (`SnowComponent` → `Snow_Component`), and emits the tagged field lines. Report mode
lists what it could not pair, in both directions:

- Odin fields with no capnp counterpart (port-only, e.g. `Date._is_relative`, `intercropping`);
- capnp fields with no Odin counterpart (dropped features: `ICData`, `frostKillOn`, …).

Both lists get an explicit decision recorded in the plan, not a silent omission.

**Gate:** `pixi run build` and `pixi run test` pass; `diff_outputs.sh` byte-identical. Tags are
inert to the compiler, so this is provable by inspection *and* by the fixture.

### Step 2 — `support/serde`, standalone · gate: unit tests on synthetic types

`Type_Plan` + `Value` + walk + JSON, with no monica import (CONVENTIONS §6). Tests cover one
synthetic type per category: nested struct, `[dynamic]T` of scalar and of struct, fixed array,
`map` as set / as pairs with string, int and enum keys, `Maybe(T)` (three-state — CONVENTIONS §5:
`unset` must not collapse to the zero value), owning pointer nil and non-nil, `serde:"-"`, a hook.
Plus: encode → decode → encode is tree-identical, and decode of a truncated/garbage file returns an
error rather than trapping.

Number formatting is settled here, once: `%.17g`, matching `jsonx.dump` (CONVENTIONS §3a), so a
state file round-trips through `f64` exactly and diffs against a `jsonx`-dumped oracle.

### Step 3 — `monica/state` · gate: in-memory round trip on a real model

Annotate the 37 structs (mostly `serde:"-"` on 22 pointers + 3 procs), write the five hooks, write
`state_rewire`, and expose:

```odin
state_to_json  :: proc(m: ^Monica_Model, allocator) -> []byte
state_from_json:: proc(data: []byte, allocator) -> (^Monica_Model, tools.Errors)
```

**Gate:** run the Hohenfinow2 fixture to day *D*; `state_to_json` → `state_from_json` →
`state_to_json`; the two byte strings are identical, and a reflective deep-compare of the two models
differs only on `serde:"-"` fields. This gate needs no C++ build.

### Step 4 — the two entry points · gate: restart equivalence

Port `save-monica-state.cpp` as a real workstep (`Save_Monica_State_Data` in `worksteps.odin`,
including the `runAtStartOfDay = false` default its `merge` deliberately overrides), and honour
`loadSerializedMonicaStateAtStart` / `pathToLoadSerializationFile` in `run_monica.odin`.

**Gate — the real acceptance test:** run Hohenfinow2 straight through. Then run it again with a
`SaveMonicaState` at day *D*, and a third time starting from that file. The daily CSV rows from *D*
onward must be **byte-identical** to the straight-through run's. Under `-o:speed` too.

### Step 5 — capnp-JSON cross-check (optional, do it if C++ interop matters)

Add a `--compat` name mode (below) and a `cpp_ref` driver that dumps the same state through
`monicamodel::serialize` + `JsonCodec`, then diff. Expect to discover, and then either match or
document: whether `JsonCodec` omits default-valued fields, how it renders `UInt64` and `Data`, and
that `kj`'s shortest-round-trip double formatting is not `%.17g`. This is a compatibility exercise,
not a correctness one — step 4's gate already proves the state is complete.

### Step 6 — Cap'n Proto binary

`gen_capnp_layout.py` + `support/capnpwire` + `monica/state/schema_gen.odin` per §3, with the
`toJson: false` branch of the workstep and the schema-drift test. **Gate:** binary round trip
matches the step 3 gate; a C++-written `.capnp` state file loads into Odin and reproduces step 4's
restart equivalence; an Odin-written one loads in the C++ build.

### Name modes (used by steps 3, 5 and 6)

One switch on `Type_Plan`, three modes: `.odin` (field names — the default, §0a), `.capnp` (the
tag), `.compat` (the tag, for reading C++-written JSON). Costs one branch at plan time.

---

## 5. What this does *not* try to restore

| Item | Why |
| --- | --- |
| Crop-rotation / cultivation-method iterator position | the C++ has it commented out and warns the env is not checked; restoring into a *different* env is out of scope here too |
| `Env`, worksteps, `DataAccessor` climate | not in `MonicaModelState`; the restarted run re-reads them from `sim.json` exactly as the C++ does |
| Intercropping (`ICData`) | dropped from the port; `Crop_Module.intercropping` stays `serde:"-"` |
| `Output` / `OId` / `Path_Plan` | run artifacts, not state; `OId.plan` is a pointer into a compile-time table |
| Streaming encode | §2.2 |

---

## 6. Decisions to confirm

1. **Ordinals preserved or free?** §3.1 (a) vs (b). Recommend (a) — binary compat with the C++ is
   free, JSON compat is deliberately given up per §0a.
2. **`Date` fidelity.** The C++ drops `useLeapYears`/`isRelative`. We can carry all three flags in
   JSON at zero cost and add two capnp fields; that makes an Odin state file strictly better and
   still readable by the C++. Recommend carrying them, since a relative `Date` restored as absolute
   is a real (if latent) bug.
3. **JSON state files and renames.** With 1:1 names, renaming a field invalidates existing state
   files. Recommend a `{"formatVersion": N}` header plus an optional rename-alias table, added the
   first time it actually bites — not up front.
4. **When does the renaming pass happen?** Step 1 is cheapest before it (the C++ names still match,
   so the extractor works) and the tags are what make the rename safe. Everything from step 2 on is
   rename-agnostic.

---

## 7. Risk register

| Risk | Mitigation |
| --- | --- |
| Cycle through a back-pointer → infinite walk | `serde:"-"` is checked at *plan* time: a pointer field with no `serde` tag and no `own` is a compile-time-detected error in `Type_Plan`, not a hang |
| A field added later, never annotated → silently absent from the state | plan-time rule above + a test that walks all 37 reachable types and asserts every field is either walked, `-`, `own`, or hooked |
| A rewired pointer forgotten → null deref deep in `step()` | post-rewire non-nil assertion over every `serde:"-"` pointer (§2.4) |
| Restart diverges in the last bits | step 4's byte-identical gate on the daily CSV, at both `-o:none` and `-o:speed` |
| Unstable map order → non-reproducible files | every map emitted key-sorted (§2.5), asserted by the round-trip gate |
| `[dynamic]T` reallocated with the wrong allocator on load | decode takes the model allocator explicitly, as every allocating proc in this tree does (CONVENTIONS §3) |
| Rename silently repoints a capnp ordinal | the `capnp:` tag is frozen and the schema-drift test compares generated vs checked-in schema |
| capnp layout table drifting from the schema repo | regenerate from `gen/capnp_offsets` and diff in CI; the struct ids (`@0x…`) are in the table and checked |
| `Maybe(bool)` collapsing to `false` on load | CONVENTIONS §5; explicit three-state test in step 2 |
| Scope creep into `Env` serialization | §5 — the file is `MonicaModelState` and nothing else, same as the C++ |
