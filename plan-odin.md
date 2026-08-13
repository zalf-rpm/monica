# Plan: porting MONICA to Odin

Goal: an Odin command-line MONICA equivalent to `src/run/monica-run-main.cpp`, producing
**byte-identical** `sim-min-out_section_*.csv` files for the Hohenfinow2 fixture.

Out of scope for this phase: ZeroMQ, Cap'n Proto RPC, Cap'n Proto serialization,
Intercropping, the `SaveMonicaState` workstep.

---

## 0. Feasibility summary (why this works)

The `plan.md` refactoring removed essentially every C++ construct that has no Odin equivalent:

| Construct | Count in `src` | Odin mapping |
| --- | --- | --- |
| `virtual` functions | 0 | — |
| polymorphic hierarchies | 0 | — |
| inheritance | 3, all data-only | `using base: T` (struct embedding) |
| templates | 4 (3 in dropped `build-output.cpp`) | parametric procs / drop |
| `Workstep` hierarchy | already `std::variant` + tag + dispatch | `union` + `switch v in ws.data` |
| module shape | already plain struct + free procs in lowercase namespace | 1:1 |

Cap'n Proto is quarantined: ~1,580 `builder.`/`reader.` lines, nearly all inside
`serialize`/`deserialize`. The simulation math never touches it.

### Line budget

| | lines |
| --- | ---: |
| `src` total | 32,570 |
| − capnp/zmq mains + rpc helpers | −2,720 |
| − `build-output.{h,cpp}` (replaced by ~300) | −1,840 |
| − capnp serde bodies | ~−2,000 |
| − Intercropping + save-monica-state | ~−800 |
| **`src` to port** | **~25,200 C++ → ~18,000 Odin** |
| + support layer ported from `mas_cpp_misc` | **~2,500 Odin** |
| **total** | **~20,500 Odin** |

Roughly 80% is mechanical transliteration (`crop-module.cpp` alone is ~4,750 lines of
agronomy arithmetic with no C++ trickery).

---

## 1. Validation harness (set this up first — everything depends on it)

**Fixture:** `installer/Hohenfinow2/` — `sim-min.json`, `crop-min.json`, `site-min.json`,
`climate-min.csv`.

**Parameter data:** `MONICA_PARAMETERS=~/GitHub/monica-parameters`
(`sim-min.json` sets `"include-file-base-path": "${MONICA_PARAMETERS}"`, and
`src/soil/soil.cpp` reads `${MONICA_PARAMETERS}/soil/{CapillaryRiseRates,
SoilCharacteristicData,SoilCharacteristicModifier}.json`).

**C++ reference run** (from repo root — `"path-to-output": "./"` puts the CSVs in the repo root):

```sh
cmake --build build --parallel
MONICA_PARAMETERS=~/GitHub/monica-parameters ./build/monica-run.exe -m installer/Hohenfinow2/sim-min.json
```

Produces `sim-min-out_section_{crop,daily,yearly,run,OrganicFertilization}.csv`.
The `_3.6.60`-suffixed copies in the repo root are the mainline-MONICA baseline; keep
diffing against those, not just against the current build.

**Odin run** must produce identical files. Add a `compare.sh` that diffs all five sections
and reports the first differing line.

### Trace diffing (the real debugging tool)

Comparing only the final CSVs finds *that* something diverged, not *where*. Add, from
Phase 4 onwards:

- **Odin side:** a `core:reflect`-based `dump_state(model, day)` that walks every field of
  `MonicaModel`, `SoilColumn` (per layer), `CropModule`, `SoilMoisture`, `SoilOrganic`,
  `SoilTemperature`, `SoilTransport` and emits `day;path.to.field;value`. This is ~150 lines
  in Odin and is the single highest-leverage piece of the whole port.
- **C++ side:** on divergence, add the corresponding output ids to a scratch `sim.json`
  `daily` section (`build-output.cpp` already exposes ~300 ids covering most model state) and
  diff those. Only widen when something actually diverges — don't pre-implement all 300 ids.

First divergent (day, field) pair pinpoints the bug. This is exactly how the
vernalisation-factor `Maybe<bool>` regression was found on `simplify_copilot`.

---

## 2. C++ prep passes (do these **before** writing Odin)

Each pass ends with `cmake --build build --parallel` + the reference run + an identical-CSV
check against `_3.6.60`. These are validated in C++ against a known-good baseline, which is
far safer than making the same judgement calls blind during translation.

### Prep 1 — split Cap'n Proto serde into `*-serde.cpp` files

Pure code motion, near-zero risk, highest value per effort. For each of
`crop-module`, `monica-parameters`, `monica-model`, `soilcolumn`, `soilmoisture`,
`soilorganic`, `soiltemperature`, `soiltransport`, `snow-component`, `frost-component`,
`voc-common`, `soil/soil.cpp`:

- move every `serialize` / `deserialize` body into `<module>-serde.cpp`
- keep the declarations where they are; add the new files to `CMakeLists.txt`

Afterwards each core `.cpp` is capnp-free and is a clean 1:1 translation unit — zero
ambiguity during the port about what to skip.

### Prep 2 — remove `std::function` from the model

372 lambdas exist, but 258 are in `build-output.cpp` (dropped). The ones that matter:

| Site | Current | Change to |
| --- | --- | --- |
| `CropModule` ctor: `fireEvent`, `addOrganicMatter`, `getSnowDepthAndCalcTempUnderSnow` (`crop-module.h:258-274`) | 3 `std::function` capturing `MonicaModel*` | store `MonicaModel* model` in `CropModule`; call `monicamodel::…` directly |
| `SiteParameters::calculateAndSetPwpFcSatFunctions` | `map<string, function>`, 5 entries, one captures `pathToSoilDir` | `enum PwpFcSatMethod` + `switch`; store `pathToSoilDir` in `SiteParameters` |
| `userSoilMoistureParameters.getCapillaryRiseRate` | `std::function`, captures nothing | plain function pointer |
| `supportedPatterns()` (`create-env-from-json-config.cpp:113`) | `map<string, function>`, captures nothing | plain function pointers |
| `Spec`'s 6 `std::function<bool(const MonicaModel&)>` (`run-monica.h:99-108`) | closures over parsed JSON | small `Expression` struct: `{op, left: OIdOrConst, right: OIdOrConst}` evaluated at call time |
| `SetValueData::getValue` (`set-value.h`) | closure | same `Expression` struct |
| `workstep::registerDailyFunction` | closure | explicit context struct |

The `Spec` change is the only one with real logic risk — do it as its own commit with its
own regression check.

### Prep 3 — remove KJ from `src/core`

- `kj::Own<T>` → `std::unique_ptr<T>`, `kj::heap<T>` → `std::make_unique<T>`
- `kj::Maybe<bool> __enable_vernalisation_factor_fix__` (`monica-parameters.h:247`) →
  `Tools::Maybe<bool>`. **Preserve the three-state `unset`/`true`/`false` semantics
  exactly** — collapsing `unset` into `false` is the bug that already bit us once.
- `kj::Tuple<int,int>` (`cropmodule::anthesisBetweenStages`) → `std::pair<int,int>`
- `mas::schema::climate::RCP` → plain `enum class RCP { RCP19, RCP26, RCP34, RCP45, RCP60, RCP70, RCP85 }`
- delete `seedCrop(MonicaModel*, CropSpec::Reader)` (only caller is the FBP main)

**Do not** flatten the 3 inheritances (`AutomaticIrrigationParameters`,
`OrganicFertilizerParameters`, `CropResidueParameters`) — Odin's `using` reproduces public
data inheritance exactly. **Do not** touch the `std::variant` worksteps — already ideal.

---

## 3. Odin project layout

```
odin/
  monica/            # port of src/core, src/soil, src/worksteps, src/run, src/io
    core/
    soil/
    worksteps/
    run/
    io/
  support/           # port of the needed mas_cpp_misc subset
    date/
    tools/           # Errors, EResult, file/string helpers, algorithms subset
    jsonx/           # forgiving json11-style accessors over core:encoding/json
    climate/
  cmd/monica-run/
```

---

## 4. JSON strategy: use `core:encoding/json`, not a json11 port

Odin's `json.Value` is `union{Null, Integer(i64), Float(f64), Boolean, String, Array, Object}`.
Four semantic gaps vs json11 must be closed by a `support/jsonx` layer (≈ a port of
`json11-helper.h`, 423 lines):

1. **Number split.** json11 stores *everything* as `f64`; Odin splits `Integer`/`Float`.
   This is the biggest silent-regression risk in the whole port — a parameter written `2`
   vs `2.0` must not take different paths. Every accessor (`num`, `int`, `f64`) must accept
   **both** variants. Parse with `spec = .JSON5` (superset of JSON, and MONICA's files use
   plain JSON).

2. **Unordered objects.** `Object` is `map[string]Value`; json11's is a sorted `std::map`.
   Only 7 sites iterate objects (`monica-parameters.cpp:987,1917,1923`,
   `create-env-from-json-config.cpp:98`, `cutting.cpp:45,82`, `harvest.cpp:57`) and all build
   maps, so behaviour is safe. But **sort keys when dumping** so `to_json` output is
   deterministic and diffable.

3. **Forgiving indexing.** json11's `j["a"]["b"]` on a missing/mismatched value returns a
   null `Json`, and the entire `merge()` codebase relies on it. Odin maps give zero-value+ok.
   Provide `jget(v: Value, key: string) -> Value` returning `Null` on any miss, chainable.

4. **Ownership.** json11 is `shared_ptr`; values are copied freely (see
   `findAndReplaceReferences` in `create-env-from-json-config.cpp`). Odin's `Value` owns a
   `[dynamic]`/`map`, so shallow copies invite double-frees.
   **Decision: parse the entire config into a single arena (`virtual.Arena`) and never free
   individual values.** Config JSON is small and lives for the whole run. Do not try to
   track ownership.

Also port `to_json` for the parameter structs — it is *not* Cap'n Proto serde, it costs
~1,100 lines, and it is the Phase 1 test oracle (see below). Worth every line.

---

## 5. Support layer (~2,500 Odin lines)

| Module | C++ source | Notes |
| --- | --- | --- |
| `date` | `tools/date.{h,cpp}` (959) | Custom calendar. **Must support relative dates** (year 0, e.g. `"0000-09-23"` in `crop-min.json`) and the `useLeapYears` toggle. Do not substitute `core:time`. Unit-test against C++ over a multi-year day-by-day sweep. |
| `tools` | `tools/helper.h` subset | `Errors`, `EResult(T)` (Odin parametric struct), `readFile`, `replaceEnvVars`, `fixSystemSeparator`, `splitPathToFile`, `ensureDirExists`, `isAbsolutePath`. Odin has a builtin `Maybe(T)`. |
| `tools/algorithms` | `tools/algorithms.{h,cpp}` subset | Only 13 procs are actually used: `bound`, `round`, `roundShiftedInt`, `minMax`, `median`, `splitString`, `hourlyRad`, `hourlyT`, `hourlyVaporPressureDeficit`, `solarElevation`, plus trivial wrappers. Skip the other ~700 lines. |
| `jsonx` | `json11/json11-helper.{h,cpp}` (806) | See §4. |
| `climate` | `climate/climate-common.{h,cpp}` (916) | `ACD` enum, `DataAccessor`. Drop the `Json11Serializable` base. |
| `climate/csv` | `climate/climate-file-io.{h,cpp}` (541) | `CSVViaHeaderOptions` + header-driven CSV reader. Drop the `kj::InputStream` overloads. |
Note: the soil library is **not** part of this support layer — `c419285` moved it out of the
`mas_cpp_misc` submodule into the main tree as `src/soil/{soil,conversion,constants}.{h,cpp}`, so
it is already counted in the `src` budget above and is ported in Phase 3.

---

## 6. Phased execution

Each phase ends with a diff against a C++-produced artefact. **Do not advance with a known
divergence.**

### Phase 0 — scaffolding + support layer
Arena strategy, `Errors`/`EResult`, file/string helpers, `Date`, `algorithms` subset.
**Oracle:** unit tests; a day-by-day `Date` sweep vs. a small C++ driver.

### Phase 1 — JSON config pipeline + all 26 parameter structs
`jsonx`, `create-env-from-json-config` (incl. `findAndReplaceReferences` and the
`include-from-file` / `ref` / `%` / KA5 patterns), all `merge` and `to_json` for the structs in
`monica-parameters.h`.
**Oracle — the most valuable checkpoint in the plan:** dump the fully-merged `Env` via
`env_to_json` from *both* implementations (with sorted keys) and diff. This validates ~2,700
lines of parameter merging plus the whole reference-resolution machinery in one shot, before
any simulation code exists.

### Phase 2 — climate
`DataAccessor`, `ACD`, the header-driven CSV reader.
**Oracle:** dump the loaded `DataAccessor` as a CSV of all ACDs × all steps; diff.

### Phase 3 — soil setup
`src/soil/soil.cpp` (incl. the three JSON readers replacing the capnp path — read
`${MONICA_PARAMETERS}/soil/*.json` directly with `core:encoding/json`; the `.sercapnp`
variants are ignored), `src/soil/conversion.cpp`, `src/soil/constants.cpp`, `soilcolumn.cpp`,
`SoilLayer`, `AOM_Properties`.

Note the current shape of these types (see `plan.md`, "Follow-up: the
`SoilColumn`/`SoilLayer`/`SoilParameters` refactor"): `SoilColumn` holds an explicit `layers`
member (no vector base), `SoilLayer` has the `SoilParameters` fields flattened directly onto it,
and the four raw/override fields (`vs_SoilRawDensity`, `vs_SoilBulkDensity`,
`vs_SoilOrganicCarbon`, `vs_SoilOrganicMatter`) use `-1` as "unset" with the resolved value
computed by `soillayer::soilXyz(...)`. **That `-1`-means-unset pattern is a
sentinel-with-fallback — the same shape as the `kj::Maybe<bool>` flag that caused the `ff0f0fc`
regression. Port the resolved-value procedures, never read the raw field directly unless the C++
does.** `Soil::SoilParameters` keeps `thickness` and `calculateAndSetPwpFcSat` and is still used
standalone for `site.json` horizon parsing, with its own `_vs_`-prefixed override fields.
**Oracle:** dump initial per-layer state after `createSoilPMs` + `SoilColumn` construction; diff.

### Phase 4 — soil physics
`soiltemperature`, `soilmoisture` (+ `snow-component`, `frost-component`), `soiltransport`,
`soilorganic`, `stics-nit-denit-n2o`.
**Oracle:** daily trace diff (§1). Build the Odin `dump_state` reflection dumper here.

### Phase 5 — crop
`crop-module.cpp` (largest single file), `photosynthesis-FvCB`, `voc-guenther`, `voc-jjv`,
`voc-common`, `O3-impact`.
**Oracle:** daily trace diff. Pay particular attention to the vernalisation-factor three-state
flag — see the resolved `ff0f0fc` regression in `plan.md`, whose two root causes (an
optional-with-fallback flattened to a plain default, and stale reads of removed duplicated state)
are exactly the two failure modes this port can reintroduce.

### Phase 6 — orchestration
`monica-model.cpp` (step/generalStep/cropStep, fertiliser/irrigation/tillage,
seeding/harvest/incorporation, CO2 + groundwater helpers), `workstep.cpp` +
`src/worksteps/*` (the `std::variant` → Odin `union` translation), `cultivation-method.cpp`.
**Oracle:** event-log diff plus daily trace diff.

### Phase 7 — run loop + output
`run-monica.cpp` (`runMonica`, `StoreData`, `setupStorage`, `Spec` evaluation),
`io/output.cpp` (`OId`, `Output`), `io/csv-format.cpp`, `cmd/monica-run`.

**`build-output` replacement.** `sim-min.json` needs exactly **21 distinct output ids**:

```
Date  Crop  CM-count  Year  Stage  Kc  Irrig  ETa/ETc  AbBiom  OrgBiom  Yield
LAI   Precip  Mois  SOC  N  Tavg  Globrad  RunOff  NLeach  Recharge
```

plus the event-qualified forms `Date|sowing` / `Date|harvest`, the layer forms
(`["Mois",[1,3]]`, `["SOC",[1,3]]`, `["N",[1,3,"AVG"]]`), the organ forms
(`["OrgBiom","Leaf"]`, `["OrgBiom","Fruit"]`) and the aggregations
(`FIRST`, `LAST`, `SUM`, `AVG`).

Write these 21 as a small table of `proc(^MonicaModel, OId) -> Value` — i.e. exactly what
`build-output.cpp` does, with 21 entries instead of ~300. Reflection alone cannot produce
these: the ids are computed expressions with layer aggregation, organ indexing and rounding,
not struct field names. (The reflection dumper from Phase 4 is a *separate*, complementary
debugging tool.)

`monica-run-main.cpp` shrinks from 518 to ~200 lines once the Intercropping `output2` half is
dropped.

### Phase 8 — regression close-out
All five `sim-min-out_section_*.csv` byte-identical to the `_3.6.60` baselines. Then widen:
run `sim.json` / `sim+.json` (the larger Hohenfinow2 configs) and add output ids as needed.

---

## 7. Explicitly dropped

| Dropped | Reason |
| --- | --- |
| `Intercropping` (`monica-parameters.h:997`, `env.ic`, `runMonicaIC`'s second output) | needs Cap'n Proto RPC; also removes the duplicated `output2` half of `monica-run-main.cpp` |
| `SaveMonicaState` workstep + `deserializeFullState` (`run-monica.cpp:547-607`) | inherently Cap'n Proto. `sim-min.json`'s `serializedMonicaState` has `load.atStart=false` / `save.atEnd=false`, so the fixture is unaffected |
| all `serialize` / `deserialize` | Cap'n Proto; isolated by Prep 1 |
| `run/monica-capnp-*`, `run/monica-zmq-*`, `run/serve-monica-zmq.cpp`, `run/run-monica-capnp.*`, `run/capnp-helper.*`, `run/daily-monica-fbp-component-main.cpp` | RPC/FBP entry points |
| ~280 of the ~300 `build-output.cpp` ids | add on demand |

---

## 8. Transliteration discipline (applies to every phase)

The single most effective rule for keeping a 20k-line port verifiable:

**Translate literally. Do not improve anything.**

- Keep C++ function names, argument order and *variable names* — including the `vc_` / `vs_` /
  `vw_` / `pc_` / `vm_` prefixes. A trace diff is only useful if both sides name the same thing.
- Keep statement order inside numeric routines. Reassociating floating-point arithmetic
  (`a*b + a*c` → `a*(b+c)`) changes the last bits and will fail the byte-identical CSV check.
- Port one C++ function at a time; do not merge, split or inline functions.
- **Watch mixed int/float arithmetic.** C++ implicitly promotes `int` to `double` in mixed
  expressions; Odin requires explicit conversion, and the natural "fix" of casting the wrong
  operand silently turns a float division into truncating integer division (or vice versa). For
  every arithmetic line containing an integer variable or literal, check the C++ promotion rules
  before writing the Odin. This class of bug already bit this branch once.
- Save cleanup for a *separate* pass, after the CSVs are byte-identical.

## 9. Who executes what

The port is ~75% high-volume mechanical transliteration and ~25% decisions that get baked into
every subsequent file. Split accordingly.

**Needs Opus (design-setting or hard-reasoning work):**

| Item | Why |
| --- | --- |
| Phase 0 conventions + `support/tools` | Fixes the Odin idiom for `Errors`/`EResult`/`Maybe`, module layout, allocator handling. Every later file copies these. Must land with worked examples before volume work starts. |
| `date` (Phase 0) | 959 lines of calendar logic with relative dates (year 0) and a leap-year toggle; highest bug-density-per-line in the port, and everything downstream depends on it. Needs an exhaustive differential test vs. C++, not spot checks. |
| `jsonx` accessors (Phase 1) | The `Integer`/`Float` split is the highest silent-regression risk in the whole port. Design it so the trap is *impossible*, not merely avoided. |
| Arena/ownership strategy | Wrong choice = rewrite. |
| `MonicaModel` ownership + back-pointer layout | Submodules hold pointers to `soilColumn`; getting this wrong is memory corruption, not a wrong number. |
| `Spec` expression evaluator (Prep 2 + Phase 7) | The only genuine logic redesign in the project. |
| 21-id output table + reflection trace dumper (Phases 4, 7) | Small volume, high design content. |
| **Any numeric divergence investigation** | Checkpoint bisection + hypothesis formation. The `ff0f0fc` hunt needed suspicion of a textually unchanged file — see `plan.md`. |

**Good for Sonnet 5 (high volume, tight verification loop):**

| Item | Volume |
| --- | ---: |
| Prep 1: serde split into `*-serde.cpp` | pure code motion, compiler-verified |
| Prep 3: KJ removal (with the `Maybe` three-state rule stated explicitly, per-commit regression check) | mechanical |
| Prep 2: `CropModule` callbacks → back-pointer, function-pointer swaps (**not** the `Spec` change) | mechanical |
| Phase 1: the 26 parameter structs' `merge` / `to_json` | ~2,700 lines, verified in one shot by the `env_to_json` diff |
| Phases 2–3: climate, CSV reader, soil setup | ~2,500 lines |
| Phases 4–5: soil physics + crop-module | ~12,000 lines — the bulk |
| Phase 6: worksteps `variant` → `union`, orchestration | ~2,000 lines |
| Phase 7: `csv-format`, `output` | ~500 lines |

**Hard sequencing constraint:** Phases 0–3 (including the trace dumper from §1) must be complete
and verified *before* Sonnet starts Phases 4–5. 12,000 lines of translated physics with no
day-by-day trace diff is not debuggable by anyone — the conventions and the oracle have to exist
first. This ordering matters more than the model choice.

## 10. Risk register

| Risk | Mitigation |
| --- | --- |
| `Integer` vs `Float` divergence in JSON accessors | accessors accept both; Phase 1 `env_to_json` diff catches it before any simulation runs |
| `json.Value` shallow-copy double-free | arena allocator, never free individually |
| `Date` relative-date / leap-year semantics | dedicated unit sweep in Phase 0 |
| Three-state `Maybe<bool>` flags collapsing to `false` | Prep 3 makes them `Tools::Maybe` in C++ first; Odin's builtin `Maybe(bool)` preserves it; already a known past regression |
| `MonicaModel` submodules hold back-pointers to `soilColumn` | heap-allocate `MonicaModel` once and never move it; take pointers after allocation |
| Floating-point drift (`round`, accumulation order) | port `algorithms::round` / `roundShiftedInt` bit-exactly; keep statement order identical in hot loops |
| 20k lines written before first end-to-end run | phase oracles (§6) mean nothing goes unverified for more than one module |
