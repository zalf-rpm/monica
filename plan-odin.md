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

### Phase 0 — scaffolding + support layer — **DONE**
Arena strategy, `Errors`/`EResult`, file/string helpers, `Date`, `algorithms` subset.

Delivered: `odin/CONVENTIONS.md` (read this first), `odin/support/tools/{errors,strings,files,algorithms}.odin`,
`odin/support/date/date.odin`, `odin/tests/`.

**Oracle — both green:**
- `odin test odin/tests` — 27 tests, including 1:1 ports of the C++ `Tools::testDate()` and
  `Tools::testRoundFloorCeil()` assertions.
- `bash odin/tests/cpp_ref/run.sh` — builds a C++ driver against the real
  `mas_cpp_misc/tools/date.cpp`, runs both implementations over 8,283 cases (a 2,192-day sweep,
  arithmetic at 11 offsets × 6 bases both directions, 546 construction edge cases × 3 variants,
  relative dates + `toAbsoluteDate`, ISO parsing, 1,830 `julianDate` round-trips, the setter
  quirks, 64 comparison pairs) and diffs. **Currently identical.**

C++ quirks found and deliberately reproduced (all marked `NOTE(c++-quirk)` in the source):
`toAbsoluteDate` passes only 5 of 6 constructor arguments so `useLeapYears` lands on
`createValidDate`; `setYear` does not reselect the days-in-month table; `setUseLeapYears` ignores
`isLeapYear()`; the `Date(string)` constructor drops the parsed relative flag; `readFile`
concatenates lines without newlines. Also: `daysInMonth(month > 12)` *aborts* the C++ process
(confirmed empirically while building the driver), so the Odin guard returning 0 is unobservable.
`Tools::testDate()`'s relative-date assertions are dead code that would fail if enabled — see
`odin/tests/date_test.odin` for the worked-through explanation.

### Phase 1a — `jsonx` (the forgiving json11-compatible layer) — **DONE**
`odin/support/jsonx/{value,accessors}.odin`: arena strategy, parse/dump, forgiving indexing, the
full `json11-helper` accessor set, unit transforms, `iso_date_value`.

**Oracle — green:** `bash odin/tests/cpp_ref/run_json.sh` parses and re-dumps **455 real MONICA
parameter files** through both json11 and jsonx and diffs. Identical apart from one documented
divergence. Plus 13 jsonx unit tests in `odin/tests/jsonx_test.odin`.

Key findings:
- json11's own header claims "all numbers are double", but `parse_number` returns a **`JsonInt`**
  when the token has no `.`/exponent and ≤ 9 digits. Reading is transparent (`JsonInt::number_value`
  widens, `JsonDouble::int_value` truncates), so the accessors accept both — this is the
  Integer/Float trap, now closed and unit-tested.
- Odin's `strconv` `'g'`/precision-17 emits **exactly** MSVC's `%.17g` digits (verified against a C
  driver on 0.1, 1/3, 1e17, 1e20, 9.9999999999999995e-08, …); only a leading `+` needs stripping.
  That is what makes the byte-identical dump — and hence the Phase 1b oracle — possible.
- Two `core:encoding/json` divergences from json11, both accepted and pinned by tests: empty object
  keys are silently dropped, and duplicate keys are an error rather than last-wins. See
  `odin/CONVENTIONS.md` §3a.

### Phase 1b — `create-env-from-json-config` — **DONE**
`odin/monica/run/{create_env,create_env_json}.odin` (reference resolution + Env assembly) and
`odin/monica/soil/conversion.odin` (the conversion subset the patterns reach; the rest of
`src/soil/conversion.cpp` stays for phase 3).

**Oracle — green:** `bash odin/tests/cpp_ref/run_env.sh` runs both fixtures through both
implementations and diffs the three resolved documents plus the assembled Env:
`sim-min.json` (50,468 B) and `sim+.json` (158,189 B) — **identical**. Between them they exercise
recursive `include-from-file`, `ref` (incl. its cache), `%`, `KA5-texture-class->clay`,
`bulk-density-class->raw-density` and `sand-and-clay->lambda`. `humus-class->corg` and
`->sand` are reached by no config in the repo and are unit-tested instead.

The C++ driver links against the already-built `monica_lib`/`monica_run_lib` and takes its compiler
flags from `build/compile_commands.json`, so it always compiles the way the real build does — it
therefore needs a **configured and built** CMake tree.

`climateData` is stripped from the C++ Env before dumping, since reading climate CSV is phase 2;
remove that strip in `env_ref_main.cpp` once phase 2 lands.

### Phase 1c — the 26 parameter structs — **DONE**

Tranche 1 (**done**): the five module-parameter structs in `odin/monica/params/module_parameters.odin` —
`SoilMoistureModuleParameters`, `SoilTemperatureModuleParameters`,
`SoilTransportModuleParameters`, `SticsParameters`, `SoilOrganicModuleParameters` — with `merge`,
`to_json` and the C++ in-class initialisers. These are the structs fed by
`monica-parameters/general/*.json`.

**Oracle — green:** `bash odin/tests/cpp_ref/run_params.sh` merges the real `general/*.json` into
each struct and diffs the `to_json` dumps (8,764 B, identical). Each struct is dumped twice: once
default-constructed, which pins the C++ in-class initialisers, and once after the merge.

`jsonx` gained `build.odin` (`f`/`i`/`b`/`s`/`sl`/`arr`/`vu`/`obj`) so `to_json` reads close to the
C++ `json11::Json::object{...}` while keeping the Integer/Float choice explicit at each call site —
`dump` formats them differently (`%d` vs `%.17g`).

Tranche 2 (**done**): `odin/monica/params/site_sim_parameters.odin` — `MineralFertilizerParameters`,
`NMinApplicationParameters`, `IrrigationParameters`, `AutomaticIrrigationParameters`,
`MeasuredGroundwaterTableInformation`, `SiteParameters`, `SimulationParameters`,
`CropModuleParameters`, `EnvironmentParameters`, `CentralParameterProvider`.

`run_params.sh` grew to 15,283 B, still identical. It now also covers the rcp parser across all
11 accepted spellings (`"85"`, `"8.5"`, `"rcp85"`, `"rcp8.5"`, `"19"`, `"nonsense"`, and the
numeric forms), and synthetic merges for the structs no `general/*.json` feeds.

**PHASE SCOPE — `SiteParameters.vs_SoilParameters` is not built.** The C++ `siteparameters::merge`
calls `Soil::createEqualSizedSoilPMs`, which reaches `soilparameters::merge` ->
`fcSatPwpFromKA5textureClass` -> the three `${MONICA_PARAMETERS}/soil/*.json` tables — i.e. most of
phase 3's `src/soil/soil.cpp`. Only the raw `initSoilProfileSpec` array is captured, and
`to_json` emits an empty `SoilProfileParameters`. Phase 3 fills this in; the merge signature does
not need to change.

Tranche 3a (**done**): `odin/monica/params/organic_parameters.odin` — `YieldComponent`,
`AutomaticHarvestParameters`, `NMinCropParameters`, `OrganicMatterParameters`,
`OrganicFertilizerParameters`, `CropResidueParameters`. `run_params.sh` now at 22,668 B,
identical. Merges are exercised against the real `crop-residues/wheat.json` and
`organic-fertilisers/CAM.json`.

Two latent C++ bugs found and reproduced (see the `NOTE(c++-quirk)` blocks):
- `organicmatterparameters::to_json` lists the key `"AOM_NO3Content"` **twice**; the second entry
  is described as "Carbamide content" and was plainly meant to be `AOM_CarbamidContent`, but it
  repeats both the key and the value. Since `json11::Json::object` is a `std::map` and its
  initializer-list constructor inserts rather than assigns, the first entry wins — so
  `vo_AOM_CarbamidContent` is **never emitted**, and a `to_json`/`merge` round trip zeroes it.
  No parameter file in `monica-parameters` sets that field, so the impact today is nil.
- `automaticharvestparameters::to_json` emits `"latestHavestDOY"` (missing the `r`) while `merge`
  reads `"latestHarvestDOY"`, so that value does not survive a round trip either.

Tranche 3b (**done**): `odin/monica/params/crop_parameters.odin` — `SpeciesParameters`,
`CultivarParameters`, `CropParameters` (both `merge` overloads, incl. the split
`merge(cp, speciesJson, cultivarJson)` form), plus the trivial `cropName`/`numberOfXxx`
wrappers. `run_params.sh` now at 40,593 B, identical. Merges are exercised against real fixtures
for the first time in phase 1c — `crops/wheat.json` (species) and `crops/wheat/winter-wheat.json`
(cultivar) — rather than only `general/*.json`.

Two `jsonx` additions were needed and went into `accessors.odin`, not this file: `set_bool_vector`
(the key-form wrapper `bool_vector_d` was missing) and `double_vector` (the no-key overload of
`double_vectorD`, needed for `AssimilatePartitioningCoeff`/`OrganSenescenceRate`,
`vector<vector<double>>` fields merged element-by-element).

`CropParameters::__enable_vernalisation_factor_fix__` is the one `kj::Maybe<bool>` in this whole
struct family (CONVENTIONS §5) — mapped to Odin's builtin `Maybe(bool)`. `run_params.sh` pins the
tri-state explicitly: one synthetic merge sets the flag true, another omits the key entirely, and
the Odin side reads it back with `v, ok := x.?` (never `x.? or_else`) to confirm "unset" survives
rather than collapsing to `false`. Odin's builtin `Maybe(T)` is `runtime.Maybe`, from
`core_builtin.odin` — no import needed, matching the CONVENTIONS claim it's builtin.

**Real-fixture gotcha caught while writing the driver, not the port:** `wheat.json` and
`winter-wheat.json` don't set every key (e.g. `EF_MONO`, `VCMAX25`, `AEKC`, `LightExtinctionCoefficient`,
`EarlyRefLeafExp` are absent from both). The C++ driver's `SpeciesParameters p;` default-constructs
with the in-class initialisers *before* `merge` runs, so those omitted fields keep their non-zero
defaults. The first Odin driver draft started merges from a zero-valued struct instead of
`make_species_parameters()`/`make_cultivar_parameters()`, which would have silently zeroed those
fields — caught by re-deriving the oracle's intent before running it, not by the diff (a
zero-vs-0.5 mismatch would have failed loudly anyway, but the fix belongs in the test driver, not
the port itself).

**Phase 1c is now complete** — all 26 parameter structs in `monica-parameters.h` have `merge` and
`to_json` ported, verified. `create-env-from-json-config` incl. `findAndReplaceReferences` and the
`include-from-file` / `ref` / `%` / KA5 patterns was already done (phase 1b). With the capstone
below also green, **phase 1 as a whole is done** — phase 2 (climate) is next.

### Phase 1 capstone — the `CentralParameterProvider` checkpoint — **DONE**

**Scope correction (important).** A *full* `env_to_json` diff is NOT achievable at this point and
should not be attempted: `Env::to_json` emits `cropRotation` (-> `CultivationMethod` -> `Workstep`,
phase 6) and `climateData` (-> `DataAccessor`, phase 2). Neither exists yet.

What was built instead — the valuable checkpoint the plan meant:
`odin/tests/cpp_ref/central_params_ref_main.cpp` + `odin/tests/central_params_ref/main.odin`, run by
the new `odin/tests/cpp_ref/run_central_params.sh` (`run_env.sh` itself is untouched):

1. Take the `params` sub-object of the assembled Env JSON (same `createEnvJsonFromJsonObjects` /
   `create_env_json_from_json_objects` machinery `run_env.sh` already proves identical).
2. Merge it into `CentralParameterProvider` on both sides.
3. Diff `centralparameterprovider::to_json`.

**Oracle — green:** both Hohenfinow2 fixtures, `sim-min.json` (6,949 B) and `sim+.json` (6,872 B),
identical — 13,821 B total. That wires all 26 tranche-1c structs together against real fixture data
in one shot, which the per-struct tests (`run_params.sh`) cannot catch: a field read under the
wrong key, or a sub-struct never reached because its parent key is misspelled. None turned up.

One known gap needed active normalisation, one needed none:
- `SiteParameters.vs_SoilParameters` (-> `SoilProfileParameters`) is real on the C++ side (phase 3
  is done there) but always an empty array on the Odin side (phase 3 isn't ported yet — see
  `site_parameters_to_json`'s "PHASE SCOPE" comment). The C++ driver force-overwrites
  `SoilProfileParameters` to `[]` after computing `to_json`, matching what the Odin side already
  always emits, so this expected gap doesn't mask a real regression in the other 2,700 lines. The
  Odin driver needs no equivalent step.
- `groundwaterInformation`: `centralparameterprovider::to_json` doesn't emit it on *either* side (a
  pre-existing, already-reproduced quirk — commented out in the C++), so it's simply untested here,
  not a source of mismatch requiring normalisation.

### Phase 2 — climate — **DONE**
`DataAccessor`, `ACD`, the header-driven CSV reader: `odin/support/climate/{climate_common,
climate_file_io}.odin`.

**Oracle — green, and it subsumes the planned one.** Rather than a dedicated CSV-dump diff, phase
2 wires `create_env_json_from_json_objects` (phase 1b) up to actually produce `env["climateData"]`
(previously a documented gap), removes the corresponding strip in `env_ref_main.cpp` / `run_env.sh`,
and lets the existing phase-1b oracle carry the load: `run_env.sh` now diffs the **whole** assembled
Env, climate data included, for both Hohenfinow2 fixtures — **667,957 B identical** (was 208,657 B
pre-phase-2). sim-min.json exercises the plain iso-date path; sim+.json's `climate.csv-options`
turned out to exercise nearly everything else in one real fixture: the `header-to-acd-names` rename
branch, the 3-element convert-tuple branch (`"globrad": ["globrad", "/", 100]`, i.e. `convertFn`),
the `de-date` (`DD.MM.YYYY`) column format, an unmapped column falling back to `.skip`
("Julian-day"), and a `start-date`/`end-date` window (exercising the date-validity filters and the
strict-date-checking completeness check). `run_params.sh` and `run_central_params.sh` were re-run
clean afterwards - no regressions in phase 1c from the `jsonx` accessor additions below.

**The one real bug found, and the general lesson it leaves behind.** `read_climate_data_from_csv_lines`
originally used `map[d.Date]map[ACD]f64` to bucket parsed rows by date (mirroring the C++
`map<Date, map<ACD,double>> data`) - and it silently dropped ~85% of sim+.json's rows (2,526 days
of data collapsed to 732). Root cause: **C++'s `Date::operator==` compares only
`year()/month()/day()/isRelativeDate()`** - explicitly *not* the leap-year-table-selection flag
(`date.h:121-124`). Odin's default map-key equality for a struct compares *every* field. Two
`Date` values built through different paths (arithmetic via `d.add` vs field-by-field parsing via
`set_day`/`set_month`/`set_year`) can represent the identical logical date while carrying different
internal flag bits, so `map[d.Date]` silently treats them as different keys. Fixed by keying on a
normalised `Date_Key{y, m, dy, is_relative}` tuple instead (`date_key()` in `climate_file_io.odin`)
everywhere a `Date` is used as a map key or hashed/compared for equality outside the `date` package's
own `d.eq`/`d.lt`/... procs (which already implement the correct C++-matching comparison and were
never the problem). **Generalised risk, not fully resolved package-wide**: any future `map[d.Date]...`
anywhere in this port has the same latent bug; grep for `map[d.Date]` / `map[Date]` before adding
one, or route through a `Date_Key`-shaped normalisation the way this file does. This is exactly the
class of bug the plan's risk register warns about for sentinel/derived-state fields, just surfacing
in a new place (map key semantics) rather than the `Maybe<bool>` case it was originally written for.

Two smaller decisions:
- `CSVViaHeaderOptions.convertFn` (`std::map<ACD, std::function<double(double)>>`) is ported as
  `map[ACD]Csv_Convert` (`{op: Convert_Op, value: f64}` + `apply_csv_convert`), the same enum+apply
  treatment `jsonx.Transform` already uses for `%`/unit conversions, per prep 2.
- `DataAccessor._data` (C++ `shared_ptr<vector<vector<double>>>`, so copies alias the same backing
  storage) is ported as a plain `[dynamic][dynamic]f64` value field - each copy gets independent
  storage. `cloneForRange` and the non-splice branches of `mergeClimateData` therefore don't share
  mutations the way the C++ does. Nothing in phases 1-2 exercises that aliasing (a single
  `climate.csv` per fixture, never cloned or merged with a second `DataAccessor`); flagged with a
  `NOTE(simplification)` in `climate_common.odin` to revisit if a later phase's trace-diff oracle
  needs genuine sharing semantics.

Dropped (confirmed zero references anywhere under `src/`, so nothing to diff against and no
oracle would exercise them): the CLM/Werex/WettReg/Carbiocial/UserSqlite/Star DB-column-name
helpers, `availableClimateData2Name`/`2unit`, `YearRange`/`snapToRaster`, gzip (`.gz`) climate file
support (needs `kj::GzipInputStream`), and the two `extern "C"` DLL exports.

`jsonx` gained `set_string_vector`/`set_string_vector_d` in `accessors.odin` (the keyed-setter
wrapper existed for `double`/`bool` but not `string`, needed for `CSVViaHeaderOptions.header`), and
`tools` gained `trim` and `sunshine2global_radiation` in `algorithms.odin` (the latter not in the
plan's "13 procs actually used" list, since that audit was scoped to `src/`, not
`mas_cpp_misc/climate`) - none of them new patterns.

### Phase 3 — soil setup — **DONE**
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

Also in scope, because phase 1c deliberately deferred it: `siteparameters::merge` must start
building `vs_SoilParameters` via `createEqualSizedSoilPMs`, and `siteparameters::to_json` must emit
the real `SoilProfileParameters` array. `Site_Parameters.initSoilProfileSpec` already captures the
raw spec, so the merge signature does not change. Once done, the harness must compare a
**populated** profile — a diff over two empty arrays proves nothing.

Highest-risk part of this phase is `fcSatPwpFromKA5textureClass` (~200 lines of bounded
interpolation between soil-raw-density and organic-matter breakpoints). It is dense mixed
int/float arithmetic, i.e. exactly where the reassociation and integer-division traps of
CONVENTIONS §1 bite. Transliterate term for term and do not fold constants.

**Required second oracle: an interpolation sweep.** The per-layer state diff only exercises the
soil types and densities the Hohenfinow2 fixtures happen to contain — a handful of paths through a
function with many branches. That is not enough: a wrong interpolation in an unexercised branch
would pass phase 3 silently and only surface in phase 4/5 as an unexplained soil-moisture
divergence, which is the most expensive kind of bug to chase (see the `ff0f0fc` history in
`plan.md`).

So add a sweep driver, in the same shape as `odin/tests/cpp_ref/run.sh` does for `Date`: call
`fcSatPwpFromKA5textureClass` (and the other `fcSatPwpFrom*` entry points, plus
`updateUnsetPwpFcSatFrom*`) from both implementations over a grid — every KA5 texture class × raw
densities spanning and straddling the table breakpoints (e.g. 900 … 2100 in steps of 50, so
interpolation *and* clamping are hit) × organic-matter values around the 1.5/3/6/11.5 breakpoints
— and diff the resulting sat/fc/pwp triples. Include the unset/`-1` sentinel combinations
explicitly. A few thousand rows costs nothing to run and converts "the fixture happens to pass"
into "the function agrees across its domain".

**Checkpoint 3a (done) — the pure-function half: `SoilParameters`, the three JSON table readers,
the interpolation functions, `createSoilPMs`/`createEqualSizedSoilPMs`, and the required sweep
oracle.** `odin/monica/soil/{constants,conversion,soil_parameters,soil_tables,soil_pwp_fc_sat}.odin`.
`conversion.odin` existed already (phase 1b ported the 5 functions its reference patterns reach);
this checkpoint adds the 6th, `sandAndClay2KA5texture`, completing the file.

**Oracle — green:** `odin/tests/cpp_ref/run_soil_pwp_fc_sat.sh`, **15,978 rows identical** — every
KA5 texture class in `SoilCharacteristicData.json` (39, plus one deliberately-unknown "XX" to
exercise the error path) × 25 raw densities (900…2100 step 50) × 7 organic-matter values straddling
the 0/1.5/3/6/11.5 breakpoints, 3 `soilRawDensity`/`soilOrganicMatter` fallback-resolution combos,
plus a grid over `updateUnsetPwpFcSatFrom{VanGenuchtenVereecken,VanGenuchtenToth,Toth}` (sand × clay
× bulk density × organic carbon × stone content × layer number, ~8,000 rows exercising
`VanGenuchtenToth`'s `isTopSoil` branch both ways). Reached through the public
`updateUnsetPwpFcSatFrom*` entry points, not the private `fcSatPwpFrom*` ones directly — see below.

Two deliberate deviations from the plan's literal wording, both because the redesign this phase
implements (prep 2's `std::function` removal) makes the literal C++ shape moot:

- **`pathToSoilDir` is a parameter, not a `SiteParameters` field.** The plan's prep-2 table says
  "store `pathToSoilDir` in `SiteParameters`". `SoilParameters.calculateAndSetPwpFcSat` (a
  `std::function<Errors(SoilParameters*)>`, one per soil layer) is replaced by a
  `Pwp_Fc_Sat_Method` enum (`NONE`/`WESSOLEK2009`/`VAN_GENUCHTEN_VEREECKEN`/`VAN_GENUCHTEN_TOTH`/
  `TOTH` — mirroring the 5-entry `calculateAndSetPwpFcSatFunctions` map built once in
  `monica-run-main.cpp:240-249`) dispatched by `apply_pwp_fc_sat_method`. `path_to_soil_dir` and
  `layer_no` are threaded through as explicit parameters of `soil_parameters_merge` instead of a
  stored/mutated field, since they're needed only at merge time and this keeps the merge call
  self-contained and testable without a side-channel setup step. Zero observable difference — the
  field is never serialized by any `to_json`. Site_Parameters wiring (checkpoint 3c below) will pass
  `${MONICA_PARAMETERS}/soil/` through this parameter.
- **The private `fcSatPwpFrom*` functions and `updateUnsetPwpFcSatFromKA5textureClass` are exported
  in Odin**, unlike the C++ where they're anonymous-namespace/file-local (only reachable in the C++
  test driver through the public `updateUnsetPwpFcSatFrom{VanGenuchtenVereecken,VanGenuchtenToth,
  Toth}` and `getInitializedUpdateUnsetPwpFcSatfromKA5textureClassFunction` wrappers, which is what
  the sweep driver actually calls on the C++ side). Odin has no file-local-across-package-files
  restriction matching C++'s anonymous namespace, and later phases need these directly.

Caching: the three JSON table readers cache their parsed table in a package-level Odin global
guarded by a `bool`, matching the C++'s function-local `static` + `if (!initialized)` pattern —
minus the C++'s mutex, which existed for the RPC/zmq server mains this port drops (see plan-odin.md
§7); `monica-run` is single-threaded.

**Checkpoint 3b (done) — `AOM_Properties`/`SoilLayer`/`SoilColumn` construction.**
`odin/monica/core/soil_column.odin` (a new package, `odin/monica/core/`, mirroring `src/core/`).
Ports `AOM_Properties` (data only), `SoilLayer` + `makeSoilLayer` + the `soillayer::` resolved
getters (`soilMoisturePF`, `soilNmin`, `soilSiltContent`, `soilRawDensity`, `soilBulkDensity`,
`soilOrganicCarbon`, `soilOrganicMatter`), `SoilColumn` + `makeSoilColumn` (the `Soil::SoilPMs`
overload only), and the handful of `soilcolumn::` getters that don't depend on a running
CropModule/worksteps (`calculateNumberOfOrganicLayers`, `numberOfLayers`, `numberOfOrganicLayers`,
`layerThickness`, `dailyCropNUptake`, `getLayerNumberForDepth`, `sumSoilTemperature`).

**Not ported** (all `soilcolumn.h`-declared but phase 6, not phase 3 — they run against a live
`MonicaModel`/worksteps, not at construction time): `applyMineralFertiliser*`, `applyIrrigation*`,
`applyTillage`, the delayed-N-min machinery (including the `DelayedNMinApplicationParams` struct
and `_delayedNMinApplications` list — always empty at construction, so dropping them changes
nothing the phase-3 oracle observes), `putCrop`/`removeCrop`, `clearTopDressingParams`,
`deleteAOMPool`. `SoilColumn.cropModule` is kept as a `rawptr` placeholder (never dereferenced by
anything ported so far) until `CropModule` exists in phase 5.

**Oracle — green:** `odin/tests/cpp_ref/run_soil_column.sh`, both `site-min.json` and `site.json` -
**18,972 B identical**. Builds the real 20-layer soil column via `createEqualSizedSoilPMs` +
`makeSoilColumn` from each fixture's inline `SoilProfileParameters` (no `include-from-file`/`ref`
resolution needed for either — see below) and dumps all 36 fields of every layer plus 5
`getLayerNumberForDepth` probes, `sumSoilTemperature` and `calculateNumberOfOrganicLayers`.
`site+.json` is deliberately not used: its profile uses `"bulk-density-class->raw-density"`/
`"sand-and-clay->lambda"` reference patterns that need the phase-1b resolution pipeline this
driver doesn't run - checkpoint 3c (which does go through that pipeline) exercises it instead.

**Finding with consequences for phases 4-6: use `core:c/libc`, not `core:math`, for
`pow`/`log`/`exp`/...** `soilMoisturePF`'s `pow(pow(x, 1/m) - 1, 1/n)` chain initially diverged
from the C++ reference in the last 2 ULP on one layer of one fixture, while all ~1,440 other
values in this oracle (and the unrelated 15,978-row interpolation sweep, which also leans on
`pow`/`exp`) matched exactly — `core:math`'s pure-Odin implementation is not always bit-identical
to the MSVC CRT `<cmath>` the C++ build calls. Fixed by switching to `core:c/libc`'s `pow`/`log10`
(FFI bindings to the platform CRT, so they call the literal same function on Windows) — now
byte-identical. Documented as a general rule in `CONVENTIONS.md` §1: use `core:c/libc` for every
transcendental function from phase 4 onward, not just where a diff happens to catch it, since
`crop-module.cpp` alone leans on `exp`/`pow` for ~4,750 lines of agronomy arithmetic and a
differential test on finitely many inputs cannot prove a pure-Odin implementation matches
everywhere. The already-passing `calcVanGenuchtenVereeckenParams`/`calcVanGenuchtenTothParams`
(checkpoint 3a) were deliberately left on `core:math` rather than churned without a failing test
to justify it; worth an opportunistic swap if either is touched again.

**Checkpoint 3c (done) — `siteparameters::merge`/`to_json` build/emit the real
`vs_SoilParameters`.** `odin/monica/params/site_sim_parameters.odin`. `Site_Parameters` gains the
`vs_SoilParameters: [dynamic]soil.Soil_Parameters` field deferred since phase 1c tranche 2.
`site_parameters_merge` resolves `pwpFcSatFunction` to a `Pwp_Fc_Sat_Method` via
`soil.pwp_fc_sat_method_from_name` (warning on an unresolved name, exactly like the C++ map-miss
branch) and calls `soil.create_equal_sized_soil_pms`; `site_parameters_to_json` emits one
`soil.soil_parameters_to_json` per layer instead of the placeholder empty array. Needed an extra
parameter (`path_to_soil_dir`), so — like `environment_parameters_merge` before it — the generic
`default_merge` helper doesn't fit and the `DEFAULT`/`=` unwrap is inlined by hand.
`central_parameter_provider_merge` threads `path_to_soil_dir` one level further down; its C++-side
test-driver counterpart now pre-populates `calculateAndSetPwpFcSatFunctions` before merging, exactly
as `monica-run-main.cpp:239-249` does before `env_merge`.

**Oracle — green, no new driver needed.** `run_central_params.sh` (the phase 1 capstone) already
merges real fixture data into `CentralParameterProvider` and diffs its `to_json` — it was only ever
passing because both sides normalised `SoilProfileParameters` to `[]`. Removed that normalisation
(both the "Odin always emits empty" comment and the C++ driver's forced overwrite) and it stays
green with the *real* 20-layer profile now on both sides: **44,809 B identical** across
`sim-min.json` and `sim+.json` (up from 13,821 B). `run_params.sh` (40,593 B) and `run_env.sh`
(667,957 B) re-verified clean afterwards — this phase touched shared code (`site_sim_parameters.odin`)
without breaking either.

**Phase 3 is now complete.**

### Phase 4 — soil physics
`soiltemperature`, `soilmoisture` (+ `snow-component`, `frost-component`), `soiltransport`,
`soilorganic`, `stics-nit-denit-n2o`.
**Oracle:** daily trace diff (§1). Build the Odin `dump_state` reflection dumper here.

**Trace dumper — done.** `odin/monica/trace/trace.odin` (the `core:reflect` walker) and
`odin/tests/cpp_ref/trace_common.h` (the C++ counterpart), validated by
`odin/tests/cpp_ref/run_trace.sh` against the phase-3 `SoilColumn` (600 lines/fixture, 1,200
total, identical). See the `6d0b203` commit message for the design notes (Maybe-unset handling,
pointer/map policy, the `defer delete` temp-allocator bug found bringing it up). Phase 4 proper
extends this with dump functions for `SoilMoisture`, `SoilTemperature`, `SoilTransport`,
`SoilOrganic`, `SnowComponent`, `FrostComponent` on both sides, called once per simulated day.

**Prep, done before the module work below — the libc sweep + soil sweep sentinel gap.**
Two follow-ups flagged while closing out phase 3, done first since both touch code every phase-4
module will lean on:

1. **libc sweep.** Checkpoint 3a's finding (`soillayer::soilMoisturePF`, §6 above and
   `CONVENTIONS.md` §1) — `core:math`'s `pow`/`exp`/... aren't always bit-identical to the C++
   reference build's MSVC-CRT `<cmath>` calls — was fixed at the one site a diff happened to catch,
   with two more sites (`calcVanGenuchtenVereeckenParams`/`calcVanGenuchtenTothParams`) deliberately
   left on `core:math` pending "an opportunistic swap if either is touched again." Rather than wait
   for phase 4/5's ~12,000 lines to hit the same trap piecemeal, swept the whole tree onto
   `core:c/libc` for the full C `<math.h>` §7.12 family (trig/hyperbolic/exp/log/power/gamma — not
   just the five named in the checkpoint-3a note): `support/tools/algorithms.odin`
   (`round_to_digits`'s `pow`, `sunshine2global_radiation`'s trig chain), `support/date/date.odin`
   (`day_lengths`'s trig chain — phase 0, already-`DONE`, reopened for this), and
   `monica/soil/soil_pwp_fc_sat.odin` (the two deferred Van Genuchten functions). Added
   `odin/tests/check_libc_transcendentals.sh`, a grep-based guard over every `*.odin` file for the
   full function-name list, so the rule is enforced on every future file rather than relying on a
   diff to catch a regression. All eight `cpp_ref` oracles (`run.sh`, `run_json.sh`, `run_env.sh`,
   `run_params.sh`, `run_central_params.sh`, `run_soil_column.sh`, `run_soil_pwp_fc_sat.sh`,
   `run_trace.sh`) plus the 44 `odin test odin/tests` unit tests re-ran clean afterwards — this
   touched shared code without breaking anything phases 0-3 already proved.
2. **`-1` sentinel rows in the phase-3 interpolation sweep.** Every existing row in
   `soil_pwp_fc_sat_ref_main.cpp`/`soil_pwp_fc_sat_ref/main.odin` left
   `vs_FieldCapacity`/`vs_Saturation`/`vs_PermanentWiltingPoint` **all** unset (`-1`) before calling
   an `updateUnsetPwpFcSatFrom*` entry point. But the C++ (and the Odin port of it) checks each of
   the three independently after the disjunctive "is anything unset" guard
   (`if (sp->vs_FieldCapacity < 0) sp->vs_FieldCapacity = res.fc;`, same for the other two) — the
   same sentinel-with-fallback shape as the `ff0f0fc` `Maybe<bool>` regression (`plan.md`), and a
   caller that already knows e.g. field capacity from a `site.json` horizon override and wants only
   saturation/wilting point computed exercises a per-field branch none of the existing ~16,000 rows
   ever reached. Added a `*SENT` section sweeping all 8 combinations of which of the three are
   preset (to fixed representative values) vs `-1`, through all four entry points (`KA5SENT`,
   `VGVSENT`, `VGTSENT`, `TOTHSENT`) — 32 new rows, oracle now at 16,010 total, still identical.

**Module 1 — soiltemperature — done.** `odin/monica/core/soil_temperature.odin`: `Soil_Temperature`,
`make_soil_temperature`, `step`, `calc_soil_surface_temperature`, the file-local
`soil_temperature_layer_at` helper.

**Deliberate deviation, made explicit because the plan flags it as consequential
("MonicaModel ownership + back-pointer layout... wrong choice = rewrite").** The C++ struct carries
a `monica: *MonicaModel` back-pointer, read in exactly three places: the constructor's
`monica.envPs.p_timeStep`, and `calcSoilSurfaceTemperature`'s `monica.currentCropModule ?
->vc_SoilCoverage : 0.0` and `monica.soilMoisture->{snowComponent->vm_SnowDepth,
frostComponent->vm_TemperatureUnderSnow}`. Neither `MonicaModel` (phase 6) nor `CropModule` (phase
5) exist yet, and `SoilMoisture` is a phase-4 *sibling* not yet ported either — so there is no
back-pointer target to take literally. Rather than invent a placeholder `MonicaModel` now (a
decision every later phase-4/5/6 file would then copy), the Odin port drops the `monica` field
entirely and threads the three values it would have supplied as explicit parameters on
`make_soil_temperature`/`step`/`calc_soil_surface_temperature` instead — the same move checkpoint
3a made for `pathToSoilDir` ("needed only at merge time... keeps merge self-contained and testable
without a side-channel setup step"). Phase 6's real orchestration call site becomes a mechanical
substitution (`model.currentCropModule != nil ? model.currentCropModule.vc_SoilCoverage : 0`, etc.
passed positionally), not a redesign. Full rationale is in the file's package comment.

**Oracle — green.** `odin/tests/cpp_ref/soil_temperature_ref_main.cpp` +
`odin/tests/soil_temperature_ref/main.odin`, run by `run_soil_temperature.sh`: **23,220 trace
lines identical** over 60 simulated days. Both drivers build a *real* `MonicaModel` from
`sim-min.json` via the unmodified `makeMonicaModel` (reusing the exact CentralParameterProvider
pipeline checkpoint 3c already proved identical), so `soilColumn`/`soilTemperature`/`soilMoisture`
(incl. real `snowComponent`/`frostComponent`) are all constructed by production code — only
`soiltemperature::step` itself is driven directly rather than through `generalStep`. Each day reads
real `tmin`/`tmax`/`globrad` from `climate-min.csv` (2,557 available days, capped at 60) and pokes
`snowComponent.vm_SnowDepth`/`frostComponent.vm_TemperatureUnderSnow` to a synthetic, deterministic
sequence identical on both sides (`(day%30)<10 ? 50.0 : 0.0`, exercising both branches of the snow
check repeatedly) rather than running `soilmoisture::step` — `currentCropModule` stays null
throughout (bare soil, `vc_SoilCoverage` always 0), which is the scope this checkpoint proves;
widening it to a live crop is phase 5's job once `CropModule` exists, the same kind of documented,
symmetric gap as the phase-1-capstone's `SoilProfileParameters` normalisation.

Three bugs caught before/while bringing the oracle up, all worth recording:
- **A real port bug, not a harness bug: `dampingFactor` never got its C++ in-class initialiser.**
  `struct SoilTemperature { ... double dampingFactor{0.8}; ... }` — `makeSoilTemperature`'s body
  never assigns it explicitly, so a zero-initialised Odin struct silently kept `0.0`. Since
  `calcSoilSurfaceTemperature`'s shading-coefficient formula divides through `dampingFactor` on the
  `soilCoverage == 0` (bare-soil) path this oracle exercises, the bug would have shown up
  immediately as a divergent `soilSurfaceTemperature` on day 0 — caught by re-deriving the
  constructor from the header before running anything, the same "re-derive the oracle's intent"
  discipline noted in phase 1c tranche 3b. Fixed with an explicit `st.dampingFactor = 0.8` and a
  comment naming the in-class initialiser it stands in for.
- **`CSVViaHeaderOptions`/`Csv_Via_Header_Options` must go through `merge()`, not hand-set fields,
  on both sides.** Constructing the options with `separator`/`noOfHeaderLines` assigned directly
  (skipping `.merge(json)`) leaves `lineNoOfDataStart` at its raw in-class default (`-2`) instead of
  the value `merge()` derives (`lineNoOfHeaderLine + noOfHeaderLines`) — and the stale default
  crashed the CSV parser outright (C++ abort, exit code 3) rather than misreading data. Fixed by
  building the options from a small JSON object (`{"no-of-climate-file-header-lines": 2,
  "csv-separator": ","}`) through the real constructor/`csv_via_header_options_merge` path on both
  sides, mirroring `sim-min.json`'s actual `"climate.csv-options"` keys instead of hand-picking
  struct fields. General lesson for phases 4-5: any C++ struct whose `merge()` *derives* a field
  from others is unsafe to partially hand-construct in a driver; go through `merge()`.
- **`createEnvJsonFromJsonObjects`/`create_env_json_from_json_objects` need a `climate.csv` path
  fixup, not just `crop.json`/`site.json`.** Copied the `central_params_ref_main.cpp` fixup block,
  which (because that driver strips `climateData` before dumping) never needed one — missing it
  produced a silently-swallowed "could not open climate file" from the *internal* resolution
  attempt (a second, separate read of the same file, done directly by this driver with the correct
  path, still worked, but the misleading stray stderr line cost real time to trace). `env_ref_main.cpp` /
  `env_ref/main.odin` already had the right fixup; copied from there instead.

**Module 2a — snow-component + frost-component — done.**
`odin/monica/core/{snow_component,frost_component}.odin`: `Snow_Component`/`Frost_Component` and
every `snowcomponent::`/`frostcomponent::` proc. Split out of soilmoisture as their own checkpoint
before tackling `soilmoisture.cpp` itself: reading the whole of both C++ files first showed neither
touches `MonicaModel`/`CropModule` at all (unlike `soiltemperature.cpp` and, much more heavily,
`soilmoisture.cpp` itself — see Module 2b below), so both ported as direct 1:1 translations with
none of `soil_temperature.odin`'s back-pointer-to-parameter deviation.

**Oracle — green, first try.** `odin/tests/cpp_ref/snow_frost_ref_main.cpp` +
`odin/tests/snow_frost_ref/main.odin`, run by `run_snow_frost.sh`: **18,980 trace lines identical**
over a full 365-day year (long enough to guarantee a real winter, unlike soiltemperature's synthetic
snow injection — here the snow is real, computed from real `tavg`/`precip`). Both drivers reuse the
real, already-constructed `model->soilMoisture->{snowComponent,frostComponent}` (built by the
unmodified `makeMonicaModel -> makeSoilMoisture -> initializeFromParams` chain — `initializeFromParams`
itself doesn't touch `CropModule` either, only the snow/frost setup lines are exercised here) and
drive `snowcomponent::calcSnowLayer`/`frostcomponent::calcSoilFrost` directly, in the same order and
with the same bare-soil simplification (`vc_NetPrecipitation == precipitation`, no live crop)
`soil_temperature_ref_main.cpp` established. The Odin driver constructs `Snow_Component`/
`Frost_Component` directly via `initialize_snow_component`/`initialize_frost_component` (mirroring
`initializeFromParams`'s two `initialize` calls) since `Soil_Moisture` itself isn't ported yet.

**Module 2b — soilmoisture.cpp — done.** `odin/monica/core/soil_moisture.odin` (~700 lines):
`Soil_Moisture`, `make_soil_moisture`, `initialize_from_params`, `soil_moisture_step`, and every
other `soilmoisture::` proc (`infiltration`, `capillary_rise`, `percolation_with_groundwater`,
`groundwater_replenishment`, `percolation_without_groundwater`, `backwater_replenishment`,
`dual_kc_precomputation`, `evapotranspiration`, `reference_evapotranspiration`, `get_e_reducer_1`,
`get_deprivation_factor`, `mean_water_content`/`mean_water_content_to_depth`,
`get_snow_depth_and_calc_temperature_under_snow`).

**`CropModule` entanglement, and how it's scoped.** Unlike snow/frost, `soilmoisture.cpp` reads
`CropModule` state at 10 call sites (rooting depth, Kc/Kcb factors, crop height, transpiration
per layer, remaining/reference evapotranspiration, evaporated-from-intercept) across
`step`/`evapotranspiration`/`dualKcPrecomputation`/`capillaryRise`. `CropModule` itself is phase 5's
~4,750-line struct, not ported yet. Two decisions handle this, both following
`soil_temperature.odin`'s no-`monica`-field precedent:
- `Crop_Module` (new file `crop_module_stub.odin`) is a **phase-5 stub**: a real (not `rawptr`)
  struct carrying only the 10 fields this file reads, each with its final C++ name so phase 5's
  real struct is a drop-in replacement, not a rename.
- The C++ struct already carries its own `CropModule *cropModule` field (kept in sync with
  `monica.currentCropModule.get()` by monica-model.cpp at every plant/harvest event) alongside the
  `monica`-mediated reads — so unlike `soil_temperature.odin`, this isn't inventing a new field to
  read through, just unifying two provably-equal C++ pointers onto the one Odin already has.
  `dualKcMethod`/`dailySumIrrigationWater` (the two genuinely `monica`-only reads, not mirrored onto
  any `SoilMoisture` field) become explicit parameters on `soil_moisture_step`/`evapotranspiration`/
  `dual_kc_precomputation`, the same move used for `p_timeStep` in `make_soil_temperature`.

**Real C++-quirk reproduced, not fixed:** `dualKcPrecomputation`'s KA5-texture REW lookup compares
against mixed-case strings (`"Ss"`, `"Su2"`, ...) while every real KA5 texture in this port's
fixtures is uppercase (`"SS"`, `"SU2"`, ...) — so on real data every branch misses and `REW` always
falls through to the `FC0`-based fallback. Reproduced with a `NOTE(c++-quirk)` rather than
case-folded to what was "obviously" intended, since fixing it would change simulated evaporation on
every future crop-planted run. Dormant in this checkpoint's oracle regardless (the whole `useDualKc`
branch requires `cropModule != nil`, so it doesn't execute at all yet).

**A same-package procedure-name collision, and a `CONVENTIONS.md` correction it exposed.**
`soiltemperature::step` and `soilmoisture::step` both map to `core.step` under the "one Odin package
per C++ *directory*" rule checkpoint 3b actually established (`odin/monica/core/` mirrors
`src/core/`) — a different rule than `CONVENTIONS.md §2`'s naming table literally showed
(`monica::soilmoisture::step -> soilmoisture.step`, implying one package per C++ *namespace*). Fixed
by renaming both to `soil_temperature_step`/`soil_moisture_step` (disambiguating by owning-struct
prefix, not leaving one bare) and correcting `CONVENTIONS.md §2` to document the directory-based rule
that's actually in effect, with an explicit "check for a collision before adding a new free
procedure to `core`" note — `soiltransport`/`soilorganic` (later phase-4 modules) almost certainly
have their own `step` too.

**Oracle — green.** `odin/tests/cpp_ref/soil_moisture_ref_main.cpp` +
`odin/tests/soil_moisture_ref/main.odin`, run by `run_soil_moisture.sh`: **189,070 trace lines
identical** over a full 365-day year. Same "real `MonicaModel`, bare soil" approach as the
soiltemperature and snow/frost checkpoints (`model->soilMoisture`, incl. its real
`snowComponent`/`frostComponent`, built by the unmodified `makeMonicaModel`), with one more
synthesized input: `vs_GroundwaterDepth` isn't produced by anything ported yet
(`monicamodel::groundwaterDepthForDate` is phase-6 orchestration), so both drivers use a
deterministic alternating shallow/deep sequence (`(day % 40) < 15 ? 3.0 : 15.0`) to exercise both
`percolationWithGroundwater` and `percolationWithoutGroundwater` repeatedly over the run. `et0` is
`-1.0` throughout, matching `climate-min.csv`'s actual shape (no `et0` column).

One real port bug caught by the oracle, same class as `soil_temperature.odin`'s `dampingFactor`
miss: **`vm_irrigFwEvent` never got its C++ in-class initialiser** (`double vm_irrigFwEvent{1.0};`).
First diff was a clean, single-field divergence (`vm_irrigFwEvent`: C++ `1`, Odin `0`) on day 0,
immediately localizing the bug. Fixed alongside the two initialisers already caught in `crop_module_stub.odin`'s
sibling struct (`vc_KcFactor{0.6}`, `vm_ReferenceEvapotranspiration{6.0}`) — all three now set
explicitly in `make_default_soil_moisture`. Worth noting for the remaining phase-4/5 modules: grep
each header for `{[0-9]` before considering a struct port complete: Odin's zero-value default
silently swallows every non-zero C++ in-class initialiser, and the trace-diff oracle only catches it
if the affected field is actually reached by the day range and code path under test.

**Module 3 — soiltransport — done.** `odin/monica/core/soil_transport.odin`: `Soil_Transport`,
`make_soil_transport`, `soil_transport_step`, `n_deposition`, `n_uptake`, `n_transport`,
`soil_transport_put_crop`, `soil_transport_remove_crop`. Simpler than soiltemperature/soilmoisture:
the C++ struct already takes `siteParams`/`envParams`/`cropModParams` as raw pointers passed
directly into `makeSoilTransport`, never through a `MonicaModel&`, so no back-pointer-to-parameter
deviation was needed at all — a straight 1:1 translation, like snow/frost. `cropModule` reads one
field (`vc_NUptakeFromLayer`) added to the phase-5 `Crop_Module` stub from module 2b.
`putCrop`/`removeCrop` pre-emptively got the owning-struct-prefix treatment (`soil_transport_put_crop`
/`soil_transport_remove_crop`) even though nothing collides with them yet — `soilorganic.cpp` (next)
almost certainly declares its own `putCrop`/`removeCrop` too, per the `CONVENTIONS.md §2` correction
from module 2b.

**Oracle — green, first try.** `odin/tests/cpp_ref/soil_transport_ref_main.cpp` +
`odin/tests/soil_transport_ref/main.odin`, run by `run_soil_transport.sh`: **77,015 trace lines
identical** over a full 365-day year. Unlike the bare-inputs approach elsewhere, this driver chains
the already-verified `soilmoisture::step`/`soil_moisture_step` before
`soiltransport::step`/`soil_transport_step` each day — real production data flow, since `soiltransport`
reads soil-layer NO3/water-flux state that only `soilmoisture` (not yet `soilorganic`, which isn't
ported) actually produces. `soilorganic::step` normally runs between them and updates `vs_SoilNO3`
via mineralisation; without it, `vs_SoilNO3` evolves purely through `soiltransport`'s own
mass-conservation math starting from the fixture's initial value — a valid, self-contained test of
`soiltransport` in isolation, matching the same "prove what's provable now, defer the rest with a
documented gap" pattern used throughout phase 4.

**Module 4 — stics-nit-denit-n2o + soilorganic — done. Phase 4 is now complete.**

**Prerequisite folded in: `stics-nit-denit-n2o.cpp` (233 lines).** `odin/monica/core/stics.odin`:
`stics_vnit`, `stics_vdenit`, `stics_n2o` (5-arg), `stics_n2o_full` (9-arg convenience overload), and
the file-local `nit::`/`denit::`/`n2o::` C++ namespaces as `@(private)` procs prefixed accordingly
(`nit_fNH4`, `denit_fT`, `n2o_rcor`, ...). The plan lists this as its own phase-4 module, but
`soilorganic::step` calls `stics::vnit`/`vdenit`/`N2O` directly when `SticsParameters.use_nit`/
`use_denit`/`use_n2o` are enabled, so `soilorganic.odin` would not compile without it — folded in now
rather than left as a stub. Fully self-contained (only depends on the already-ported
`p.Stics_Parameters`), no oracle-scope caveats needed. Real fixtures and the C++ in-class defaults
both set `use_nit`/`use_denit`/`use_n2o` to `false`, so `soilorganic`'s own oracle (below) exercises
the non-STICS `foNitrification`/`foDenitrification`/`foN2OProduction` path; the STICS branch is
ported and compiles correctly but is dead code in every fixture in this repo, the same "correct but
currently unexercised" status as `soilmoisture.odin`'s `dualKcPrecomputation`.

**`odin/monica/core/soil_organic.odin` (~1,500 lines): `Soil_Organic`, `make_soil_organic`,
`soil_organic_initialize_from_params`, `soil_organic_step`, and every other `soilorganic::` proc**
(`fo_urea`, `fo_mit`, `fo_volatilisation`, `fo_nitrification`/`fo_stics_nitrification`,
`fo_denitrification`/`fo_stics_denitrification`, `fo_n2o_production`/`fo_stics_n2o_production`,
`fo_pool_update`, the eleven `fo_*_on_*` rate-modifier helpers, `add_organic_matter` (both C++
overloads), and the `get*` accessors). No `monica`-back-pointer deviation needed — like
`soiltransport`, the C++ struct takes `SoilColumn&` and `SoilOrganicModuleParameters` directly.
`cropModule` reads one more field (`vc_NetPrimaryProduction`) added to the phase-5 `Crop_Module` stub.

**Two real C++ copy-vs-reference bugs found by reading the source, reproduced exactly (not fixed) —
the highest-value find of this checkpoint.** `foUrea`'s `auto layer0 = so->soilColumn.layers.at(0);`
and `foVolatilisation`'s `auto lay0 = so->soilColumn.layers.at(0);` are BY-VALUE copies (every other
loop variable in this ~1,900-line file is `auto &layi`). Mutations made through them —
`foUrea`'s `vs_SoilNH4 -=` after ammonia volatilisation, and `foVolatilisation`'s `vs_SoilNH4 -=`
*and* the `vo_DaysAfterApplication++` on every pool in `lay0.vo_AOM_Pool` (bound as a reference into
the copy) — never reach the real soil column. This is a genuine, confirmed upstream bug, not a
translation hazard; CONVENTIONS discipline says reproduce it, not "fix" it. Reproducing it exactly
was itself non-trivial: Odin's `[dynamic]T` struct-field copy is a *shallow header copy* (same
backing array), unlike `std::vector`'s deep-copying copy constructor, so a naive `lay0 :=
sc.layers[0]` would have made `lay0.vo_AOM_Pool` mutations *visible* on the real layer — the exact
opposite of the C++ bug being reproduced. Fixed by adding `clone_aom_pool`, an explicit deep-copy
helper, used only where `foVolatilisation` needs it (`foUrea`'s copy never touches `vo_AOM_Pool`, so
no clone needed there). Dormant in this checkpoint's oracle regardless — `vo_AOM_Pool` is empty for
every layer all year, since nothing in the bare-soil/no-workstep scope ever calls
`addOrganicMatter` — but real once phase 6 wires up fertiliser/residue worksteps, so this needed to
be gotten right now rather than deferred.

**A second, unrelated shadowing quirk, confirmed harmless.** `foMIT`'s local `vo_AOM_FastDeltaSum`/
`vo_AOM_SlowDeltaSum` (declared partway through the C++ function body) share names with
`SoilOrganic` struct fields but are genuinely fresh function-local vectors, not aliases — the real
struct fields of the same name are populated later, by `foPoolUpdate`. Reproduced as true Odin
locals, never touching `so.vo_AOM_FastDeltaSum`/`so.vo_AOM_SlowDeltaSum` inside `soil_organic_fo_mit`.

**Oracle — green.** `odin/tests/cpp_ref/soil_organic_ref_main.cpp` + `odin/tests/soil_organic_ref/main.odin`,
run by `run_soil_organic.sh`: **68,255 trace lines identical** over a full 365-day year. Chains all
three modules that run before `soilorganic` in the real `monicamodel::generalStep` order —
`soiltemperature::step` → `soilmoisture::step` → `soilorganic::step` — rather than isolating
`soilorganic` behind synthetic inputs. A side benefit: since `soilmoisture` is now real and verified
(module 2b), `soiltemperature` no longer needs `soil_temperature_ref_main.cpp`'s original synthetic
snow-depth injection here — it reads the real, live `model->soilMoisture->snowComponent`/
`frostComponent` state through the unmodified `st->monica` back-pointer, exactly like production.
One small oracle bug of its own: the C++ dump function initially missed four real `SticsParameters`
fields (`code_hourly_wfps_nit`, `code_hourly_wfps_denit`, `k_desat`, `profdenit`) that exist on the
C++ struct but aren't read by `stics-nit-denit-n2o.cpp` — caught immediately as an Odin-only extra
line in the diff (not a value mismatch), fixed by adding them to the C++ dump.

**Phase 4 (soil physics) is now complete: soiltemperature, snow-component, frost-component,
soilmoisture, soiltransport, stics-nit-denit-n2o, soilorganic all ported and independently verified
against real, chained-where-possible daily trace-diff oracles.** Next: phase 5 (crop-module and
friends) or phase 6 (orchestration / `variant`→`union` worksteps) — crop-module unblocks widening
every phase-4 oracle's bare-soil scope to a live crop, per each module's package-comment notes above.

### Phase 5 — crop

Scoped into checkpoints at the user's request, since `crop-module.cpp` alone is ~5,500 lines — the
largest single file in the port by a wide margin. Checkpoint order: (1) the four satellite modules
`crop-module.cpp` depends on, (2) `CropModule` scaffolding, (3) phenology + canopy, (4)
photosynthesis, (5) biomass/dry-matter + stress, (6) water + nitrogen, (7) `step()` orchestration
(which also widens every phase-4 module's bare-soil oracle to a live crop).

**Checkpoint 1 — satellite modules — done.** `photosynthesis-FvCB` (698 lines, the FvCB C3
photosynthesis + Yin/Struik stomatal-conductance model), `O3-impact` (207 lines, hourly ozone
damage to assimilation + FAO-56 water-stress stomatal closure), `voc-guenther` (131 lines, Guenther
et al. biogenic VOC emissions), `voc-jjv` (323 lines, the Grote et al. 2014 JJV VOC model),
`voc-common` (532-line header, pure data structs, no `.cpp`). Confirmed by reading every header
before starting: **all four are fully self-contained** — plain struct-in/struct-out functions, zero
`CropModule`/`MonicaModel` coupling — exactly the snow/frost situation from phase 4, and a genuine
first checkpoint rather than an artificial one.

`odin/monica/core/{photosynthesis_fvcb,o3_impact,voc_common,voc_guenther,voc_jjv}.odin`. C++'s
`std::map<FvCB_Model_Consts,double>` static globals (`c_bernacchi`/`deltaH_bernacchi`, 7 entries
each, populated once and never mutated) became switch-accessor procs instead of package-level map
globals — same values, no mutable-global-map-init-order question to reason about. Added
`tl.flt_equal_eps`/`tl.flt_equal_zero` (`mas_cpp_misc/tools/helper.h`, used by 3 C++ files including
this checkpoint's `voc-guenther.cpp`) to `support/tools/algorithms.odin`, the first genuinely new
`tools` addition since phase 0.

**Two real C++ quirks found and reproduced:**
- `FvCB_canopy_hourly_C3`'s shaded branch has a copy-paste bug — `out.shaded.cc = get<0>(sh_ci_cc_gs)`
  uses index 0 (`Ci`) instead of index 1 (`Cc`), unlike the sunlit branch three lines above which
  correctly uses `get<1>` for `.cc`. So `out.shaded.cc` always duplicates `out.shaded.ci` in the real
  C++. Reproduced exactly.
- `calculateJJVVOCEmissionsMultipleSpecies`'s own `calculateParTempTerm` parameter is never passed
  through to its internal `calcLeafEmission` call — the C++ call site omits the argument entirely,
  so the callee always uses its default (`false`) regardless of what the outer function's caller
  passed in. Reproduced by hardcoding `false` at that one call site, not threading the parameter
  through.

**A genuine, not-reproducible gap: uninitialised memory, not a C++ quirk.** `FvCB_leaf_fraction`'s
`ci`/`cc` fields have no in-class initialiser, and `FvCB_canopy_hourly_C3`'s "no photosynthesis can
occur" branch (`global_rad <= 0`) never assigns them — so a local, non-value-initialised
`FvCB_canopy_hourly_out out;` leaves `out.{sunlit,shaded}.{ci,cc}` as indeterminate stack garbage in
that branch, which showed up in the oracle as wildly different numbers between runs
(`-9.2559631349317831e+61` one build, presumably something else another). This is honest undefined
behaviour in the reference build itself, not a value any translation could or should reproduce —
normalized both sides to `0` (what Odin's zero-initialised local naturally produces) when
`global_rad <= 0`, the same "documented gap, symmetric normalisation" move used for
`SoilProfileParameters` in the phase 1 capstone, rather than chasing a compiler-dependent garbage
value.

**Oracle — green.** `odin/tests/cpp_ref/phase5_satellite_ref_main.cpp` +
`odin/tests/phase5_satellite_ref/main.odin`, run by `run_phase5_satellite.sh`: **3,012 rows
identical**. Unlike every phase-4 driver, none of these four modules take a `SoilColumn`/climate/
`MonicaModel` — every function is a pure calculation really called *hourly* from inside
`crop-module.cpp`'s not-yet-ported photosynthesis loop, so "daily trace diff" doesn't apply. This is
a parameter-sweep oracle instead, the same shape as phase 3's `fcSatPwpFromKA5textureClass` sweep: a
full 7-dimension grid (864 rows) for FvCB, curated scenario sets for O3-impact/voc-guenther/voc-jjv.

**Two bugs caught bringing the oracle up, both in the test driver, not the port:**
- A raw-`printf` format-string/argument-count mismatch (one extra `out.sunlit.jv` argument with no
  matching `%.17g` slot silently shifted every subsequent field by one, corrupting the whole
  `shaded.*` column group without any compiler warning under Windows' varargs printf). Fixed by
  rewriting the C++ driver's output to build a `vector<string>` and tab-join it - mirroring the Odin
  driver's `row()` helper - which makes a field-count mismatch a compile-time-obvious list-length
  difference instead of a silently-shifted format string. Worth adopting for future multi-field-sweep
  drivers over raw `printf`.
- `NaN` formats differently across the two runtimes for the *same* underlying IEEE754 value - MSVC's
  `printf` renders it `nan` or `-nan(ind)` depending on how it arose, Odin's `strconv` renders it
  `NaN` - genuine, deterministic floating-point NaN propagation (a couple of `LAI=0` FvCB grid points
  drive a real division by zero), not a translation divergence. Both drivers' number-formatting
  helpers now special-case `is_nan` to the literal `"NAN"` rather than diffing an arbitrary NaN
  payload's text rendering.
- Also caught, while investigating the JJV-section-only divergence: the Odin driver's own
  `Voc_Species_Data` sweep values were built from a bare zero-valued struct instead of
  `make_voc_species_data()`, so `SCALE_I`/`SCALE_M` (C++ in-class default `1.0`) were `0` -
  `voc_jjv_calc_leaf_emission` divides by `species.SCALE_I`, so this produced spurious `NaN`s the
  real C++ driver's properly-defaulted struct never hit. The same "re-derive the oracle's default
  construction path, don't zero-init" lesson from phase 1c tranche 3b's `SpeciesParameters` mistake,
  recurring in a new driver.

**Checkpoint 2 — CropModule scaffolding — done.** `crop-module.h`'s `CropModule` struct (~175
fields) and `crop-module.cpp`'s `makeCropModule` constructor (the first overload only - the
second, `CropModuleState::Reader`-based, is Cap'n Proto deserialize and dropped).
`odin/monica/core/crop_module.odin` replaces `crop_module_stub.odin`'s 11-field placeholder
wholesale; every field keeps its exact C++ name, so the phase-4 modules already holding a
`cropModule: ^Crop_Module` pointer (soilmoisture, soiltransport, soilorganic) compile unchanged
against the drop-in. Not yet ported: any of the ~40 `cropmodule::` step functions (`fcRadiation`,
`step`, ...) - those are checkpoints 3-7.

**Two design calls, not direct transliteration:**
- `Intercropping *intercropping` becomes a typed-but-inert `rawptr`, always nil. `Intercropping`
  itself is dropped (Cap'n Proto RPC, see the "Explicitly dropped" table); every real use of this
  field in `crop-module.cpp` is behind `if (isIntercropping)`, false in every fixture in this repo.
- `fireEvent` / `addOrganicMatter` / `getSnowDepthAndCalcTempUnderSnow` (C++ `std::function`
  members capturing a `MonicaModel*`, prep-2's flagged std::function-removal site) become plain
  Odin `proc` fields with matching signatures (`proc(_: string)`, `proc(_: map[int]f64, _: f64)`,
  `proc(_: f64) -> (f64, f64)`). Odin's `proc` type has no capture, so this defers - not solves -
  prep 2's actual design question (`store a MonicaModel* model in CropModule and call
  monicamodel::... directly`); `MonicaModel` doesn't exist yet (phase 6), and this checkpoint never
  calls these fields, only assigns them, so the field-shape decision was enough for now. Revisit at
  phase 6 when `step()` actually needs to fire them.

**One real bug caught and fixed, not a C++ quirk: shallow- vs deep-copy of `cropParams`.**
`cm->cropParams = *cropParams;` in C++ deep-copies via `CropParameters`' implicit copy constructor
(every `std::vector` member copies). Odin's plain struct assignment only copies `[dynamic]T`
headers, aliasing the backing storage - the same class of bug as phase 4's `clone_aom_pool`
(`soilorganic.odin`), but this one is not hypothetical: `cropParams` typically points at a Sowing
workstep's own, long-lived `CropParameters` (`src/worksteps/sowing.cpp`), reused every time that
workstep fires again across a multi-year crop rotation. Without a deep clone, a later checkpoint
mutating `cm.cropParams` (perennial-crop handling, cutting) would corrupt the workstep's source
object for the next season instead of only this crop's own copy. Added `clone_crop_parameters`
(+ private `clone_species_parameters`/`clone_cultivar_parameters`/`clone_f64_array`/
`clone_f64_2d_array`/`clone_bool_array`/`clone_yield_component_array` helpers) to `crop_module.odin`,
deep-cloning every `[dynamic]T` reachable from `CropParameters`, including the two
`[dynamic][dynamic]f64` fields (`pc_AssimilatePartitioningCoeff`/`pc_OrganSenescenceRate`).
`residueParams` needs no such clone - `CropResidueParameters`/`OrganicMatterParameters` are all
scalar fields, nothing to alias.

**Oracle - green, no daily loop needed.** `odin/tests/cpp_ref/crop_module_ref_main.cpp` +
`odin/tests/crop_module_ref/main.odin`, run by `run_crop_module.sh`: **3,540 lines identical**
across 4 construction scenarios (indexed as trace "days" 0-3, since `makeCropModule` is a single
construction, not a step function, and `makeMonicaModel` only needs a `CentralParameterProvider` -
no climate data or day loop at all). Real wheat `CropParameters`/`CropResidueParameters` loaded the
same way `params_ref_main.cpp` loads them; a real, live `SoilColumn` from the same
`CentralParameterProvider`-merge pipeline every phase-4 oracle uses. The 4 scenarios exercise every
branch in the constructor: (0) baseline (`pc_AdjustRootDepthForSoilProps=true`,
`vs_ImpenetrableLayerDepth=-1` -> no clamp), (1) `pc_AdjustRootDepthForSoilProps` forced false
(skips the soil-adjusted rooting-depth branch), (2) `vs_ImpenetrableLayerDepth` forced positive and
below the computed max rooting depth (clamp branch), (3) a synthetic cultivar with
`pc_StageKcFactor` scaled to peak below 1.0 (the Kcb "low-coverage crop" branch - real wheat peaks
at 1.1, so scenario 0 alone already covers the "high-coverage" side of that `if`).

Deliberately **not** dumped by the oracle, and so not re-verified here: `cropParams`/
`residueParams`/`perennialCropParams` (already round-tripped through `cropparameters::merge`/
`to_json` in the phase-1 params oracle - only the deep-clone itself is new here, and the byte-exact
match across all vector fields on 4 differently-shaped inputs is enough to confirm it works);
`soilColumn`/`siteParams`/`simParams`/`cropModParams`/`intercropping` (pointers, dumped as set/nil
only); `guentherEmissions`/`jjvEmissions`/`vocSpecies`/`cropPhotosynthesisResults`/
`perennialCropDormancyPeriodEndDate` (default-constructed, never touched by this constructor - the
right checkpoint to verify them is whichever one actually populates them);
`fireEvent`/`addOrganicMatter`/`getSnowDepthAndCalcTempUnderSnow` (function values, not dumpable).

**Checkpoint 3 — phenology + canopy geometry — done.** The `cropmodule::` functions that turn
accumulated heat units into developmental stage, crop height/diameter, leaf area index and soil
coverage: `fcRadiation`, `fcDaylengthFactor`, `fcVernalisationFactor`, `fcOxygenDeficiency`,
`fcCropDevelopmentalStage` (+ its `fcUpdateCropParametersForPerennial` dependency, and the
file-scope `WangEngelTemperatureResponse` helper), `fcKcFactor`, `fcCropSize`, `fcCropGreenArea`,
`fcSoilCoverage`, `setStage`, the anthesis/maturity query functions
(`isAnthesisDay`/`anthesisBetweenStages`/`isMaturityDay`/`getAnthesisDay`/`getMaturityDay`/
`maturityReached`), and the small pure getters (`sunlitAndShadedLAI`,
`getFractionOfInterceptedRadiation1`/`2`, `setOtherCropHeightAndLAIt`,
`getCurrentTotalTemperatureSum`, `getCurrentStageTemperatureSum`, `getTotalTemperatureSum`,
`sumStageTemperatureSums`) - all appended to `odin/monica/core/crop_module.odin`. Not yet ported:
`fcCropPhotosynthesis` onward (checkpoint 4+), `step()` itself and every `fireEvent`-driven bit of
orchestration inline in it (checkpoint 7), `forceTransplantState` (fired by a Transplant workstep,
checkpoint 7), `setPerennialCropParameters` (fired by Sowing, checkpoint 7),
`organIdsForPrimaryYield` (yield, checkpoint 5), `getEffectiveRootingDepth` (root/water, checkpoint 6).

**`fcRadiation` is a second, independent day-length/declination/radiation implementation** - not
the same code as `soilmoisture.odin`'s own copy (different consumer, same HERMES-derived formulas).
Not deduplicated, matching the C++ (`soilmoisture.cpp` and `crop-module.cpp` each have their own).

**The `cropParams`/`perennialCropParams` deep-copy discipline established in checkpoint 2 recurs
here for real, not just defensively.** `fcUpdateCropParametersForPerennial` does
`cm->cropParams = *cm->perennialCropParams;` - the same C++ deep-copy-via-copy-constructor
semantics as `makeCropModule`'s `cropParams = *cropParams`, so it reuses `clone_crop_parameters`
rather than a plain assignment. The oracle actually exercises this: scenario B's `perennialCropParams`
is a second, distinctly-named synthetic `CropParameters`, and the trace shows
`cropParams.cultivarParams.pc_CultivarId` switching from `"synthetic-season-1"` to
`"synthetic-next-season"` at the exact reset day, confirming the swap-by-value (not by-reference)
semantics on both sides.

**One design call: the size_t/int unsigned-wraparound edge case in `sumStageTemperatureSums`
was not reproduced bit-for-bit.** C++ computes `endAtInclStage2` as
`cm->noOfDevStages + endAtInclStage + 1` in mixed `size_t`/`int` arithmetic - when `endAtInclStage`
is negative, the signed operand converts to a huge unsigned value and wraps back around modulo
2^64 to the numerically-intended result for any realistic (small-magnitude) negative offset like
`-1`. Reproduced with plain `int`/`f64` arithmetic instead of chasing the wraparound literally,
since the two only diverge for pathological inputs (offsets more negative than `-noOfDevStages`)
that no caller in this codebase ever passes.

**Oracle - two scenarios, no full `step()` replication.** `fcRadiation` through `fcSoilCoverage`
aren't `step()` itself, so the oracle drivers (`odin/tests/cpp_ref/crop_module_phenology_ref_main.cpp`
+ `odin/tests/crop_module_phenology_ref/main.odin`, run by `run_crop_module_phenology.sh`) replicate
only the step()-body excerpt this checkpoint needs (matching its call order exactly) - not the
FAO-56 inline Kcb block (not a `cropmodule::` function) and not any `fireEvent` call, both left for
checkpoint 7. **15,960 lines identical:**
- **Scenario A** - real wheat `CropParameters` against `NUM_DAYS_A=400` days of the real
  Hohenfinow2 `climate-min.csv` record, started past germination via `setStage(cm, 1)` (germination
  itself needs `soiltemperature::step`, checkpoint 7's job to chain in; `EmergenceMoistureControlOn`/
  `EmergenceFloodingControlOn` are both `false` in `sim-min.json`, so `soilMoisture_m3`/
  `fieldCapacity`/`permanentWiltingPoint` are provably unused once past stage 0 regardless). Exercises
  real long-day `fcDaylengthFactor`, real vernalisation dynamics across a genuine winter/summer
  temperature swing, and whatever N-/water-stress developmental acceleration wheat's own
  `pc_AssimilatePartitioningCoeff` triggers near maturity.
- **Scenario B** - a synthetic cultivar (cloned from wheat, then overridden: `pc_Perennial=true`,
  a `dormancyStartDoy` reset trigger, negative `pc_DaylengthRequirement` for the short-day branch,
  sane `pc_MinTempDev_WE`/`pc_OptTempDev_WE`/`pc_MaxTempDev_WE` bounds plus
  `__enable_Phenology_WangEngelTemperatureResponse__` forced on, an explicit
  `CropParameters`-level `__enable_vernalisation_factor_fix__` override (`Some(false)`, overriding
  the `CropModuleParameters` default of `true`), a `perennialCropParams` set to a second,
  distinctly-named synthetic instance) run against a small hand-written 20-day weather sequence
  (identical on both sides) with a rising `soilColumn.layers[0].vs_SoilTemperature` crossing
  `pc_BaseTemperature[0]` partway through (stage-0 germination) and one day with
  `globalRadiation<=0` and nonzero `sunshineHours` (`fcRadiation`'s other branch). The trace confirms
  the perennial reset actually fires - `vc_DevelopmentalStage` drops back to 0 and
  `cropParams.cultivarParams.pc_CultivarId` switches to the second synthetic instance's id on the
  same day `dormancyStartDoy` is crossed.

No bugs found bringing this oracle up - the only issue was a build error (missing
`using namespace monica::cropmodule;` in the C++ driver, `fcRadiation` et al. live in that
sub-namespace, unlike `makeCropModule`/`setStage`'s parent `monica::` namespace), not a translation
divergence.

**Checkpoint 4 — photosynthesis + assimilation — done.** `fcCropPhotosynthesis` (~1100 lines, the
largest function in the port), plus `fcGrossPrimaryProduction`, `fcNetPrimaryProduction` -
appended to `odin/monica/core/crop_module.odin`. This is where the phase-5-checkpoint-1 satellite
modules (`photosynthesis-FvCB`, `O3-impact`, `voc-guenther`, `voc-jjv`) actually get wired into the
crop module. Also added the 4 `Tools::` hourly-weather helper functions this needed
(`hourlyT`/`hourlyVaporPressureDeficit`/`solarElevation`/`hourlyRad`, plus their shared
`solarDeclination` dependency) to `odin/support/tools/algorithms.odin` - `mas_cpp_misc/tools/
algorithms.{h,cpp}` functions never needed until now.

**Two departures from a literal transliteration, both because the eliminated code is provably dead
or undefined, not inconvenience:**
- The Intercropping branch of `fcCropPhotosynthesis` (a second, alternate call to its `code` lambda
  with a different fraction-of-intercepted-radiation function, gated on
  `intercroppingOtherCropHeight > zeroHeightEps`) is unreachable in this port: `Intercropping`
  itself is a dropped feature (Cap'n Proto RPC), and `intercroppingOtherCropHeight` starts at -1
  and nothing in this port ever sets it positive. With only one surviving call to `code`, its body
  is inlined directly at that call site rather than reproduced as a `std::function`-taking closure
  - Odin's `proc` type has no capture, and building a capture-workaround for a lambda with one
    caller would be pure overhead.
- The hourly sunrise-detection check reads C++'s `hourlyGlobrads.back()` on a still-empty, freshly-
  constructed `std::vector` at hour 0 - undefined behaviour (likely a null-pointer dereference),
  not a reproducible quirk. Reimplemented as an explicit "previous hour" tracker seeded at 0.0,
  matching what the logic clearly intends ("is this the first hour with positive radiation").

**One quirk reproduced exactly, flagged `NOTE(c++-quirk)`:** the dark-growth-respiration term uses
`vc_PhotoTemperature`, not `vc_NightTemperature` - asymmetric with the maintenance-respiration split
immediately above it (which correctly uses Photo/Night respectively) and with the photo-growth-
respiration term right next to it (which is *supposed* to use Photo). Reproduced exactly, not fixed.

**A second, wider instance of checkpoint 1's `FvCB_leaf_fraction.ci/cc` gap, found by the oracle,
not anticipated going in.** `FvCB_canopy_hourly_out`'s "no photosynthesis can occur" branch
(`global_rad<=0`, `photosynthesis-FvCB.cpp:597-602`) only sets `canopy_gross_photos`/
`canopy_net_photos`/`sunlit.gs`/`shaded.gs` - every other per-leaf-fraction field (`kc`, `ko`, `oi`,
`ci`, `comp`, `vcMax`, `jMax`, `rad`, `jj`, `jj1000`, `jv`) is left as indeterminate stack garbage,
not just `ci`/`cc` as checkpoint 1's own grid happened to surface. Because the hourly loop always
ends at h=23 (11pm - virtually always night for any real latitude/date), `cropPhotosynthesisResults`'
end-of-day snapshot is built from this genuinely undefined data on essentially every real day, not
an edge case. The oracle first caught this as a live divergence (`cropPhotosynthesisResults.ci`
showing `-9.26e+61` in C++ against Odin's well-defined `0`, then cascading into `jjvEmissions`
showing real numbers in C++ against `NaN` in Odin, since JJV consumes `cropPhotosynthesisResults` as
its `CPData` input). Fix: stopped dumping `cropPhotosynthesisResults`' kc/ko/oi/ci/comp/vcMax/jMax/
jj/jj1000/jv fields and `jjvEmissions` entirely (documented in both drivers) - genuinely
unverifiable, not a translation bug. `vc_sunlitLeafAreaIndex`/`vc_shadedLeafAreaIndex` (from `LAI`,
well-defined in both branches), the O3-impact chain (consumes `.gs`, also well-defined), and
`guentherEmissions` (built from LAI/radiation/temperature aggregates, never touches the tainted
fields) all stay in the oracle and are still fully verified.

**Oracle - four scenarios, extending checkpoint 3's day-step driver.**
`odin/tests/cpp_ref/crop_module_photosynthesis_ref_main.cpp` +
`odin/tests/crop_module_photosynthesis_ref/main.odin`, run by `run_crop_module_photosynthesis.sh`:
**11,005 lines identical.** The driver's `day_step` is checkpoint 3's `phenology_day_step` extended
with `fcCropPhotosynthesis`/`fcGrossPrimaryProduction`/`fcNetPrimaryProduction` in the
`vc_DevelopmentalStage>0` block, matching `step()`'s real call order exactly. Still not ported
(checkpoints 5-7, and so not replicated): `fcHeatStressImpact`, `fcFrostKill`,
`fcDroughtImpactOnFertility`, `fcCropNitrogen`, `fcCropDryMatter`, `fcCropWaterUptake`,
`fcCropNUptake`, every `fireEvent` call - so `vc_CropNRedux`/`vc_TranspirationDeficit`/
`vc_OrganGreenBiomass` stay at their checkpoint-2 construction-time values for the whole run
(nothing in checkpoints 2-4's scope mutates them), the same "degenerate but correct" situation as
checkpoint 3's `fcCropGreenArea` test - the photosynthesis math itself still varies meaningfully day
to day from real weather and real phenology-driven Kc/LAI/height progression.
- **Scenario A** - real wheat, default flags (`pc_CO2Method=3`, the Long/Mitchell CO2 response;
  hourly FvCB off, so the daily Penning De Vries/HERMES radiation-interception path), past
  germination via `setStage(1)`, `NUM_DAYS_A=100` real Hohenfinow2 climate days.
- **Scenario B** - real wheat with `__enable_hourly_FvCB_photosynthesis__` forced true and
  `vc_RootingDepth` manually forced to `3` (root distribution is checkpoint 6, not yet ported, so
  it stays `0` by default - which would skip the O3 block entirely), 15 real climate days -
  exercises the entire hourly FvCB/O3-impact/VOC-guenther/VOC-jjv wiring, the main point of this
  checkpoint. Confirmed genuinely exercised: `vc_O3_sumUptake` accumulates across days, and
  `guentherEmissions.monoterpene_emission` is nonzero every day.
- **Scenario C** - real wheat with `cm->pc_CO2Method` forced to `2`, the Hoffmann 1995 CO2 response
  branch (`pc_CO2Method` is a plain `CropModule` member, never set from any parameter file in the
  real system either - scenario A's default of `3` is the only value any fixture in this repo ever
  produces).
- **Scenario D** - a synthetic cultivar with `speciesParams.pc_CarboxylationPathway` forced to `2`,
  the non-C3 branch (real wheat is `CarboxylationPathway=1`, so scenario A never reaches this).

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
