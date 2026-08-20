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

**Checkpoint 5 — biomass/dry matter + stress — done.** `fcHeatStressImpact`, `fcFrostKill`,
`fcDroughtImpactOnFertility`, `fcCropNitrogen`, `fcCropDryMatter` (~635 lines), plus the root-growth
support trio `fcMoveDeadRootBiomassToSoil`, `addAndDistributeRootBiomassInSoil`,
`calcRootDensityFactorAndSum` - all appended to `odin/monica/core/crop_module.odin`.

**One scope call: `fcCropNitrogen` was pulled forward from the "water + nitrogen" bucket into this
checkpoint.** Despite its name it's mostly the crop's root-growth-rate machinery (`vc_RootingDepth`/
`vc_RootingZone`/`vc_TotalRootLength`/`vc_MaxNUptake`) plus the N-stress redux factor
(`vc_CropNRedux`/`rootNRedux`) - `fcCropDryMatter`'s root distribution and organ-growth stress
terms both genuinely depend on its output to be worth testing, and it sits directly upstream of
`fcCropDryMatter` in `step()`'s real call order. `fcCropNUptake` (the actual N uptake *amounts*
extracted from soil layers - a separate, larger concern) stays in checkpoint 6 as planned.
Deliberately **not** ported here either: the ~15 yield/N-content getters
(`getFruitBiomassNContent`, `getPrimaryCropYield`, `getResiduesNConcentration`, ...) and
`numberOfAbovegroundOrgans`/`organIdsForPrimaryYield` - grepped and confirmed none of them are
called anywhere inside `crop-module.cpp` itself; they're pure output-API surface for
`build-output.cpp`, so whichever checkpoint actually needs them (phase 7's output table) can port
them then.

**One quirk reproduced exactly, flagged `NOTE(c++-quirk)`:** `calcRootDensityFactorAndSum`'s
`(i_Layer - vc_RootingDepth) / (vc_RootingZone - vc_RootingDepth)` term is genuine integer division
(all `size_t` operands) - the C++ source has a commented-out double-cast "fix" for this deliberately
left unapplied, with a comment reading "changes the outputs enough to talk about it first". Reproduced
with plain `int`/`int` division, not the double-cast fix.

**Two genuine dead stores kept for fidelity** (computed in the C++ but never read again afterward,
confirmed by re-scanning the rest of `fcCropDryMatter`): `vc_NConcentrationOptimum` and
`vc_RootNIncrement`. Odin errors on unused locals, so both keep an explicit `_ = x` discard with a
comment explaining they're genuine C++ dead stores, not omitted-by-mistake logic.

**Real callback wiring, not noop stubs, for the first time this phase.** `fcFrostKill` calls
`cm->getSnowDepthAndCalcTempUnderSnow` and `fcMoveDeadRootBiomassToSoil` (via `fcCropDryMatter`)
calls `cm->addOrganicMatter` - both dead ends in checkpoints 2-4's noop stubs, but real, exercised
code paths here. Since Odin `proc` values can't capture (no closures, unlike C++'s lambdas), the
Odin driver stores the callback's needed state in file-level globals (`g_soil_moisture`,
`g_last_organic_matter_total`/`_nconc`/`g_organic_matter_call_count`) with a plain wrapper `proc`
reading them - a test-driver-only pattern, not something the port itself needed.
`getSnowDepthAndCalcTempUnderSnow` is wired to a real, daily-stepped `SoilTemperature`+
`SoilMoisture` pair (same `soiltemperature::step` -> `soilmoisture::step` chaining phase 4's
soilorganic checkpoint used) via the already-ported
`get_snow_depth_and_calc_temperature_under_snow` - needed for `fcFrostKill`'s snow-depth branch to
be more than dead code. `addOrganicMatter` is **not** wired to a real `SoilOrganic` (already
verified in phase 4; re-verifying it here would be scope creep) - instead a recording callback sums
whatever map is passed and remembers the last `nConcentration` and a running call count, enough to
confirm `fcMoveDeadRootBiomassToSoil` computes and passes sensible values.
`model->soilMoisture->cropModule` stays `nullptr` throughout (bare soil, matching every phase-4
driver) - this checkpoint verifies `CropModule`'s own functions, not full crop/soil coupling
(checkpoint 7's job).

**Oracle - two scenarios, extending checkpoint 3/4's day-step driver.**
`odin/tests/cpp_ref/crop_module_biomass_ref_main.cpp` + `odin/tests/crop_module_biomass_ref/main.odin`,
run by `run_crop_module_biomass.sh`: **32,725 lines identical** (also spot-checked at 1,000 days -
2.7 years - to confirm long-run stability, still identical).
- **Scenario A** - real wheat, past germination via `setStage(1)`, the real Hohenfinow2
  `climate-min.csv` record for a full year (`NUM_DAYS=365`) - long enough for real winter frost
  dynamics (`pc_FrostKillOn=true` by default and in `sim-min.json`; confirmed genuinely exercised,
  `vc_LT50` hardens from -5.7 to about -24 over the winter) and a real growing season's dry-matter
  accumulation (confirmed genuinely exercised, `vc_TotalBiomass` climbs from ~106 to a ~23,000
  kg/ha plateau). Real wheat's `pc_OrganSenescenceRate` for the root organ is exactly `0` at every
  stage, so `dailyDeadRootBiomassIncrement` is genuinely, correctly always `0` here and
  `addOrganicMatter` never fires in this scenario - confirmed by checking the real fixture data, not
  assumed.
- **Scenario B** - a synthetic cultivar (cloned from wheat) with the root organ's
  `pc_OrganSenescenceRate` forced to `0.01` at every stage, 60 real climate days - exercises
  `fcMoveDeadRootBiomassToSoil`'s real `addOrganicMatter` call path scenario A cannot reach.
  Confirmed genuinely exercised: the callback fires every day once past stage 0 (60/60 days), with a
  realistic, monotonically growing total (0.57 to 0.68 kg by day 59-60).

**Checkpoint 6 — water + nitrogen uptake — done.** `fcReferenceEvapotranspiration` (FAO-56
Penman-Monteith), `fcCropWaterUptake`, `fcCropNUptake`, and `getEffectiveRootingDepth` (deferred
here from checkpoint 3 - it reads `vc_RootEffectivity`, populated by `fcCropWaterUptake`, so this is
where testing it is actually meaningful) - all appended to `odin/monica/core/crop_module.odin`.
Together with checkpoint 5's `fcCropNitrogen`/`fcCropDryMatter`, this closes out every
`cropmodule::` function `step()` calls in its `vc_DevelopmentalStage>0` block except the
`fireEvent`-driven bookkeeping itself (checkpoint 7's job).

**Several genuine C++ dead stores kept for fidelity**, same pattern as checkpoint 5:
`vc_CropWaterUptakeFromGroundwater` in `fcCropWaterUptake`, `vc_ConvectiveNUptake_1`/
`vc_DiffusiveNUptake_1` in `fcCropNUptake` - all computed but never read again afterward in the C++
either. Kept with explicit `_ = x` discards (Odin errors on unused locals). Also noted, not flagged
as a quirk since it has zero behavioural effect: `fcReferenceEvapotranspiration`'s
`pc_CarboxylationPathway == 1` branch and its `else` branch compute the exact same formula -
harmless C++ redundancy, reproduced as-is (both branches present) rather than collapsed, since
collapsing would be an improvement, not a translation.

**Oracle - one scenario, extending checkpoint 5's day-step driver to the real position of
GPP/NPP.** `odin/tests/cpp_ref/crop_module_water_nitrogen_ref_main.cpp` +
`odin/tests/crop_module_water_nitrogen_ref/main.odin`, run by `run_crop_module_water_nitrogen.sh`:
**44,165 lines identical.** Real wheat, past germination via `setStage(1)`, a full year
(`NUM_DAYS=365`) of the real Hohenfinow2 `climate-min.csv` record, same real daily-stepped
`SoilTemperature`+`SoilMoisture` pair and real `getSnowDepthAndCalcTempUnderSnow` wiring checkpoint
5 established. `climate-min.csv` has no `et0` column, so `fcReferenceEvapotranspiration` is always
the live branch, never step()'s "use the climate file's et0" pass-through. Confirmed genuinely
exercised: `vc_ActualTranspiration` varies with the season, `vc_SumTotalNUptake` accumulates to a
~441 kg N/ha plateau by day 362, and `getEffectiveRootingDepth` swings between 0.1m and 1.3m across
the year.

**One real bug caught and fixed, not in the port - in checkpoint 5's oracle driver.** Re-running
checkpoint 5's oracle as a regression check after checkpoint 6 landed turned up a divergence in
`cropModuleB.recording.lastOrganicMatterTotal` at day 55, differing in the last couple of digits
(`...492427` vs `...492438`). Root cause: `recording_add_organic_matter` (the checkpoint-5 driver's
test-only stand-in for a real `SoilOrganic`, see checkpoint 5's writeup) summed
`layer2amount`'s values via a raw `for _, v in layer2amount` loop - Odin map iteration order is
unspecified, unlike C++'s `std::map` (sorted ascending by key), and IEEE754 addition is commutative
but not associative, so summing 3+ values in a different order can round differently in the last
bit or two. This is exactly the class of gotcha `odin/monica/trace/trace.odin`'s own
`dump_map_int_f64` comment already warns about, recurring in a new driver. Fixed by sorting keys
first (the same insertion-sort technique `dump_map_int_f64` uses) before summing. Not a bug in
`fc_move_dead_root_biomass_to_soil` or any ported code - purely a test-driver artifact, caught only
because checkpoint 6's regression sweep happened to re-run checkpoint 5's oracle in a process with a
different map hash layout than whatever run originally reported PASS.

**Checkpoint 7 — step() orchestration — done. Phase 5 (crop-module.cpp) is now complete.**
`step()` itself (as `crop_module_step` - `step` collides with every other `core/*.odin` file's own
step function, so it gets the same owning-struct prefix `soil_temperature_step`/
`soil_moisture_step`/`soil_organic_step` already established), the FAO-56 dual-Kc block (never a
separate `cropmodule::` function in C++ - inline in `step()` there too, ported the same way here),
`forceTransplantState`, `setPerennialCropParameters` - all appended to
`odin/monica/core/crop_module.odin`. Every function ported in checkpoints 3-6 is finally wired
together into one real, callable daily entry point, and `fireEvent` stops being a no-op stub in the
oracle for the first time.

**`applyCutting` is deliberately not ported.** It takes a `map<int, CuttingData::Value>` -
`CuttingData` is declared in `src/worksteps/cutting.h`, which this port hasn't reached yet
(worksteps are phase 6, a separate later phase from this phase-5 checkpoint, not to be confused with
this phase's checkpoint numbering). Deferred to whichever phase-6 checkpoint ports the Cutting
workstep itself.

**One quirk reproduced exactly:** the `cereal-stem-elongation` `fireEvent` call is not guarded by a
nil check, unlike every other call site in `step()` - harmless in practice since `fireEvent` is a
required, always-set constructor parameter, but reproduced exactly rather than "fixed" to match the
others.

**Oracle - the biggest integration test in the port so far, and genuinely new: real callbacks, a
full four-module soil chain, and natural germination.** Unlike every earlier checkpoint's driver,
which hand-copied `step()`'s body into a local `day_step` (since `step()` itself wasn't ported yet),
`odin/tests/cpp_ref/crop_module_step_ref_main.cpp` + `odin/tests/crop_module_step_ref/main.odin`
(run by `run_crop_module_step.sh`) call the real, now-ported `crop_module_step` directly - there is
nothing left to hand-replicate. **79,510 lines identical** over 500 real Hohenfinow2 climate days
(more than a full year).

- **Full soil-module chain** for the first time since phase 4 itself: `soiltemperature::step` ->
  `soilmoisture::step` -> `cropmodule::step` -> `soilorganic::step` -> `soiltransport::step`
  (soilorganic before soiltransport matches the real `monicamodel::generalStep` order per
  `soil_organic_ref_main.cpp`'s file comment; crop step sits between soilmoisture and soilorganic so
  today's dead root biomass, added via the real `addOrganicMatter` callback, gets incorporated by
  soilorganic the same day - the true canonical position relative to `monica-model.cpp`'s real step
  is that file's job, not this checkpoint's).
- **All three `CropModule` callbacks are real for the first time:** `fireEvent` traces every fired
  event as its own line, so the C++/Odin event *sequence* is diffed exactly like every other field;
  `addOrganicMatter` is wired to the real, now-shared `SoilOrganic` (not checkpoint 5's recording
  stand-in); `getSnowDepthAndCalcTempUnderSnow` is wired to the real, now-shared `SoilMoisture`.
- **`soilMoisture`/`soilOrganic`/`soilTransport`'s `cropModule` pointers are all set to the real
  `CropModule`** (matching production `soilcolumn::putCrop`/`soiltransport::putCrop` wiring) - this
  is what actually widens phase 4's bare-soil coverage to a live crop: soilmoisture's
  Kc/soil-coverage/height/devStage read paths, soilorganic's NPP read (for NEP/NEE), and
  soiltransport's N-uptake read all activate for the first time since being ported, dormant, in
  phase 4.
- **No `setStage(1)` forcing, unlike every earlier checkpoint:** with a real, daily-stepped
  `SoilTemperature` now driving `soilColumn->layers[0]`'s temperature, germination is real and
  exercised, not skipped. Confirmed genuinely exercised via the event log: sown "Stage-1" on day 0,
  germinates ("emergence"/"Stage-2") on day 61, "cereal-stem-elongation" day 146, "anthesis"/"Stage-5"
  day 175, "maturity"/"Stage-6" day 201 - a fully realistic winter wheat phenology sequence, and one
  no earlier checkpoint could produce (they all started already past germination).

**Two real bugs found and fixed by this oracle - both in the test drivers, not the port - and both
exactly the kind of thing a "widen to a live crop" integration test is supposed to catch, since
neither's code path was ever live before this checkpoint:**
- **C++ driver:** `soiltemperature.cpp`'s and `soilmoisture.cpp`'s crop-coupling "outer gate" reads
  `sm->monica.currentCropModule`/`st->monica->currentCropModule` - a `MonicaModel`-level pointer,
  *separate* from `SoilMoisture`'s own `cropModule` field that this driver was already setting. The
  Odin port deliberately unifies the two onto one field (`soil_moisture.odin`'s package comment
  already documents this), so only the C++ driver needed both set - and it only set one. Every
  earlier checkpoint's C++ driver left `model->currentCropModule` at its default `nullptr` because
  none of them needed it (bare soil throughout); this checkpoint needed it and initially missed it,
  silently leaving C++ in bare-soil mode for this one read path while Odin correctly took the
  crop-coupled branch. Found via a `vc_KcFactor` diagnostic dump showing C++ reading the bare-soil
  `pm_KcFactor` parameter (0.75, from the real fixture) instead of the crop's `vc_KcFactor` (0.6).
  Fixed by moving the `CropModule`'s ownership directly into `model->currentCropModule` instead of a
  separate local `kj::Own<CropModule>`, so both read paths point at the same instance.
- **Odin driver:** `calcSoilSurfaceTemperature` reads
  `st->monica->currentCropModule ? ->vc_SoilCoverage : 0.0` in the real C++; the Odin port's
  `soil_temperature_step` exposes this as an explicit `soil_coverage` parameter instead (no
  `monica` back-pointer at all, an established phase-4 design decision). Every earlier checkpoint's
  driver correctly passed `0.0` there, since every earlier checkpoint ran genuinely bare soil. This
  driver kept the same hardcoded `0.0` by copy-paste inertia instead of updating it to the live
  crop's real `vc_SoilCoverage` now that one exists. Found by bisecting the first divergent day (62,
  one day after germination) down to `vc_LT50` in `fcFrostKill`, which reads
  `soilColumn->vt_SoilSurfaceTemperature` - itself downstream of the coverage-dependent shading
  coefficient in `calcSoilSurfaceTemperature`.

### Phase 6 — orchestration — **DONE**

`monica-model.cpp` (`MonicaModel` scaffolding, `step`/`generalStep`/`cropStep`,
fertiliser/irrigation/tillage, harvest/incorporation, CO2 + groundwater helpers),
`workstep.cpp` + `src/worksteps/*` (the `std::variant` → Odin `union` translation, all
12 concrete worksteps), `cultivation-method.cpp`.

**Discovered mid-phase: Odin's `union` forces a different checkpoint split than
planned.** The plan originally scoped this phase as "workstep infra + 4 simple
worksteps" / "crop-lifecycle worksteps" / "conditional worksteps + `applyCutting`" as
three separate checkpoints. Odin's `union` (the direct translation of
`std::variant`) requires every member's *full type* to exist at the point the union
itself is declared - unlike C++, where each payload struct and its functions can live
in separate translation units compiled independently against a forward-declared
variant alternative. So all 12 workstep data structs had to be declared together
before any dispatch function could compile, which made porting all 12 concrete
worksteps in one pass more natural than the planned three-way split. The actual
checkpoints that emerged instead:

**Checkpoint 1 — `MonicaModel` scaffolding + standalone helpers — done.** The
`MonicaModel` struct and every non-`step()` function in `monica-model.cpp`:
`makeMonicaModel`, `CO2ForDate` (both overloads), `groundwaterDepthForDate`,
`clearEvents`, the daily-sum accumulators, `dailyReset`, and the fertiliser/
irrigation/tillage apply wrappers. Also fills in the `soilcolumn.h` mutators these
wrap that phase 3 deliberately deferred (its own package comment already named this
checkpoint as the destination): `applyMineralFertiliser*`, `applyIrrigation*`,
`applyTillage`, the delayed-N-min queue, `putCrop`/`removeCrop`,
`clearTopDressingParams`, `deleteAOMPool`. `SoilColumn.cropModule` is upgraded from
phase 3's placeholder `rawptr` to a real `^Crop_Module`, matching every other
phase-4 module's own `cropModule` field since phase 5 checkpoint 2.

Two real bugs found and fixed by the oracle:
- `applyTillage`'s "merge aom pool" block is dead code in the C++: its write-back
  loop uses `for (auto aomp : layer.vo_AOM_Pool)` - a by-value range-for copy, not
  `auto &aomp` - so every `aomp.vo_AOM_Slow = ...` mutates only the loop-local copy,
  never `layer.vo_AOM_Pool[pool_index]` itself. The whole block computes per-pool
  averages and then discards them without touching any `SoilColumn` state (its own
  debug `cout` lines are all commented out too, so there's no observable side effect
  at all). Omitted here rather than reproduced as unreachable computation with a
  provably-dead write-back.
- `applyPossibleDelayedFerilizer`'s C++ bounds its drain loop on a *copy* of the
  delayed-application queue (`auto delayedApps = sc->_delayedNMinApplications;`), not
  the live one - necessary because `applyMineralFertiliserViaNMinMethod` can itself
  re-append to the live queue (a still-too-wet re-delay). A literal "loop while the
  live queue's length is nonzero" translation infinite-loops the moment a re-delay
  happens (each iteration removes one entry and, if still too wet, adds one back, net
  zero) - caught only because the oracle itself hung. Fixed by snapshotting the
  original length up front instead of copying the list.

Also resolves, for real production code, the CropModule-callback capture question
phase 5 checkpoint 2 deliberately deferred ("Odin's `proc` type has no capture...
revisit at phase 6 when `step()` actually needs to fire them"): `fireEvent`/
`addOrganicMatter`/`getSnowDepthAndCalcTempUnderSnow` need a `MonicaModel*` that
Odin `proc` values cannot close over. Since monica-run is a single-simulation-per-
process CLI tool - genuinely one live `MonicaModel` at a time, not just a convenient
shortcut - a package-level `g_current_model` pointer (set once by `makeMonicaModel`,
matching the heap-allocate-once-never-move invariant the risk register already
required) plus three plain wrapper procs reading it is behaviourally identical to the
C++ lambda captures for every caller in this port.

**Oracle - 752 lines identical, 12 scenarios** (construction, then each fertiliser/
irrigation/tillage function exercised directly against a real, live `SoilColumn`,
plus the delayed-fertiliser/top-dressing drain sequences and a `CO2ForDate`/
`groundwaterDepthForDate` parameter sweep). `odin/tests/cpp_ref/
monica_model_ref_main.cpp` + `odin/tests/monica_model_ref/main.odin`, run by
`run_monica_model.sh`.

**Prerequisite — crop-module yield getters + `applyCutting` — done.** `harvestCurrentCrop`
and the `Harvest`/`Cutting` worksteps all need `cropmodule::` functions phase 5
checkpoints 5 and 7 explicitly deferred to "whichever checkpoint actually needs
them": the ~10 yield/N-content getters (`getPrimaryCropYield`, `getSecondaryCropYield`,
`getResidueBiomass`, `getResiduesNConcentration`, `getPrimaryYieldNConcentration`,
`getResiduesNContent`, `getPrimaryYieldNContent`, `getSecondaryYieldNContent`,
`getAbovegroundBiomassNContent`, `organIdsForPrimaryYield`) and `applyCutting` itself
(~150 lines, the largest single function still unported after phase 5).
`getRawProteinConcentration` is not ported - grepped and confirmed it has no callers
anywhere in this port's scope, pure `build-output.cpp` (phase 7) API surface.

`CuttingData::Value`'s `Unit`/`CL` enums are hoisted into `core` as `Cutting_Value`/
`Cutting_Unit`/`Cutting_Cl` - the same circular-dependency break used repeatedly this
phase (see below): the Cutting workstep lives in the `run` package, which imports
`core`, so `core` cannot import it back to reach a workstep-owned payload type.

**Oracle - 124 lines identical, 5 scenarios** (the getters read-only on a real
300-day-grown wheat crop, then four chained `applyCutting` calls exercising the
biomass/percentage/LAI unit branches and the empty-`organs` auto-fill branch).
`odin/tests/cpp_ref/crop_module_yield_ref_main.cpp` + `odin/tests/
crop_module_yield_ref/main.odin`, run by `run_crop_module_yield.sh`.

**Prerequisite — `harvestCurrentCrop` + `incorporateCurrentCrop` — done.** The other
half of checkpoint 1's deferral: needs `HarvestData::Spec`/`OptCarbonManagementData`
(workstep payload types) and the yield getters above. `Harvest_Spec`/
`Harvest_Opt_Carbon_Management_Data` (+ `Harvest_Spec_Value`, `Harvest_Crop_Usage`)
are hoisted into `monica_model.odin` for the same reason `Cutting_Value` was hoisted
into `crop_module.odin`. Iterates `spec.organ2specVal` in sorted key order to match
C++ `std::map`'s iteration - the accumulators are order-sensitive floating-point
sums, the same precaution `applyCutting` and several earlier checkpoints' oracles
already needed.

**Oracle - 42 lines identical, 6 scenarios** (both `harvestCurrentCrop` branches -
old default and `optCarbonConservation`, plus its `greenManure`/detailed-spec/
`exported=false` sub-branches - then `incorporateCurrentCrop`, all on a real
300-day-grown wheat crop). `odin/tests/cpp_ref/monica_model_harvest_ref_main.cpp` +
`odin/tests/monica_model_harvest_ref/main.odin`, run by `run_monica_model_harvest.sh`.

**Checkpoint 2 — `workstep.odin`, all 12 concrete worksteps — done.** `WorkstepType`,
`WorkstepData` (the union), `Workstep`, the shared helpers (`makeInitAbsDate`,
`organIdFromName`/`organNameFromId`, `isSoilMoistureOk`/`isPrecipitationOk`),
`mergeCommon`/`applyCommon`/`conditionCommon`/`reinitCommon`, and the full central
dispatch, plus every concrete workstep's own `merge`/`apply`/`condition`/`reinit`
(normally spread across `src/worksteps/*.{h,cpp}`): Sowing, Transplant,
AutomaticSowing, Harvest, AutomaticHarvest, Cutting, MineralFertilization,
NDemandFertilization, OrganicFertilization, Tillage, Irrigation,
AutomaticIrrigation.

**Dropped:** `SaveMonicaState` (Cap'n Proto, per the "Explicitly dropped" table).
**Deferred:** `SetValue` - needs `OId`/`build-output.cpp`'s `Spec` expression
evaluator, none of which exist yet (phase 7); `to_json` for all 12 types - its only
real callers are workflow-dump/env-round-trip features this port's regression
fixture never exercises, the same "port on demand" call already made for
`getRawProteinConcentration`.

**One genuine C++ inheritance quirk that doesn't survive translation cleanly:**
`TransplantData` declares its own `initialKcb`, which *shadows* (not shares)
`SowingData`'s - two distinct storage locations in C++, both parsed from the same
JSON `"initialKcb"` key during merge (kept in sync by redundant parsing, not
aliasing), but only the derived one is actually read in `apply()` (C++ unqualified-
name lookup prefers the derived member). Odin's `using` field promotion does **not**
allow an outer field to share a name with a promoted one - a hard redeclaration
error, not a shadow - so `Transplant_Data` has no second field at all; since both
C++ storage locations always held the same value anyway, reusing the single
promoted `sowing.initialKcb` for both merge and apply is behaviourally identical,
not an approximation.

**Two real bugs found and fixed by the oracle**, both missing-default-value bugs of
the same shape phase 1c tranche 3b and phase 5 checkpoint 1 already hit once each:
- `Workstep`'s C++ in-class defaults (`isActive{true}`, `runAtStartOfDay{true}`)
  weren't applied by Odin's zero-initialising `new()` - every `make_*_workstep`
  factory now goes through a `new_workstep()` helper that sets both explicitly.
- Sowing/Transplant/AutomaticSowing's `initialKcb` (0.15) and `plantDensity` (-1)
  C++ in-class defaults were similarly missing from their Odin struct literals -
  caught because a synthetic Sowing scenario in the oracle didn't set `"initialKcb"`
  in its JSON (real `sim-min.json` doesn't either), so the gap was silent until the
  oracle compared the resulting `vc_Kcb_ini` against the C++ side.

A third apparent divergence (`applyCutting`'s "before/after" LAI differing by ~5x in
an early oracle draft) turned out not to be a port bug at all: the draft scenario
re-sowed a second crop immediately after harvest and regrew it for 30 days without
ever calling `dailyReset` (which is what actually clears `currentCropModule` after a
harvest - not yet ported at that point in the session) or re-stepping soil
temperature/moisture during the regrowth window. Once the scenario was simplified to
apply `Cutting` mid-season to the still-growing primary crop instead - the realistic
use case, and one that doesn't depend on any of that missing machinery - both sides
matched exactly. Recorded as a caution for future oracle authors: an oracle scenario
that exercises a code path the *real* system never exercises in that shape can
manufacture its own divergence.

**Oracle - 226 lines identical.** `odin/tests/cpp_ref/workstep_ref_main.cpp` +
`odin/tests/workstep_ref/main.odin`, run by `run_workstep.sh`. Reuses the real
Hohenfinow2 `crop-min.json` rotation (`AutomaticIrrigation`/`Sowing`/
`NDemandFertilization` x3/`AutomaticHarvest`/`OrganicFertilization`, with its
`"include-from-file"`/`"ref"` JSON patterns pre-resolved by hand - embedding the
already-loaded species/cultivar/residue/fertiliser JSON objects directly rather than
routing through `create-env-from-json-config`, since the full date-matching/
`unfinishedDynamicWorksteps` dispatch is cultivation-method's job, not this
checkpoint's) end to end over a real 500-day Hohenfinow2 climate run: sowing, three
N-demand fertiliser applications on their exact dates, automatic-irrigation trigger
logic, automatic-harvest maturity detection - plus synthetic JSON exercising the five
workstep types the fixture doesn't use, including `Cutting` applied mid-season to the
live, still-growing crop.

**Checkpoint 5 — `cultivation_method.odin` — done.** `CultivationMethod`,
`makeCultivationMethod`, `merge`, the three `apply()` overloads (exact-date static
dispatch, the absolute-date variant, and the `unfinishedDynamicWorksteps`/
`applyWithPossibleCondition` dispatch), `workstepsAt`/`absWorkstepsAt`,
`areOnlyAbsoluteWorksteps`, `staticWorksteps`, `allDynamicWorksteps(Finished)`,
`startDate`/`absStartDate`/`absLatestSowingDate`/`endDate`/`absEndDate`, `reinit`.
`to_json`/`toString` are not ported, matching checkpoint 2's own `to_json` deferral.

**Oracle - 96 lines identical, first oracle in the port to drive worksteps through
the real production dispatcher** (`cultivationmethod::apply`'s three overloads)
rather than a hand-rolled per-type sequence: builds one real `CultivationMethod`
from the exact `crop-min.json` rotation and runs the full real Hohenfinow2 season
through it end to end. `odin/tests/cpp_ref/cultivation_method_ref_main.cpp` +
`odin/tests/cultivation_method_ref/main.odin`, run by `run_cultivation_method.sh`.

**Checkpoint 6 — `step`/`generalStep`/`cropStep` — done, phase 6 complete.** The
daily orchestration entry points that wire every earlier phase-6 checkpoint together.
`step()` drops the Intercropping-async branch (Cap'n Proto RPC). `generalStep`
resolves groundwater depth (measured, falling back to the `groundwaterDepthForDate`
sine-curve model) and atmospheric CO2 (climate data, then yearly
`UserEnvironmentParameters`, then the logistic `CO2ForDate` model, then a flat
fallback) before stepping soil temperature/moisture/organic/transport; `cropStep`
does the same for atmospheric O3 before stepping the crop and checking
`simParams`-level automatic irrigation - a *separate* mechanism from the
`AutomaticIrrigation` **workstep**: `simPs.p_AutoIrrigationParams` +
`applyIrrigationViaTrigger` called directly, gated on its own start/end dates, not
routed through any workstep at all.

`soil_temperature_step`'s `soil_coverage`/`snow_depth`/`temp_under_snow` parameters
(phase 4's explicit-parameter design instead of a `monica` back-pointer) are finally
supplied by real production code here, not just test drivers, for the first time.

Added `measuredgroundwatertableinformation::getGroundwaterInformation`
(`params/site_sim_parameters.odin`) as a small prerequisite - `generalStep` needed it
and it hadn't been ported yet (an ISO-date-string-keyed map lookup, matching how
`groundwaterInfo`'s `Date` keys are already stored as canonicalised ISO strings from
its own `merge`).

**Oracle - 206 lines identical, and the first oracle in the port that doesn't
hand-step the four soil modules, doesn't supply a synthetic groundwater depth, and
doesn't hardcode atmospheric CO2/O3** - `monicamodel::step`'s `generalStep`/
`cropStep` halves resolve all of that internally now, for real. The daily loop is
exactly the sequence `run-monica.cpp`'s `runMonica` (phase 7, not yet ported) will
use: `cultivationmethod::apply(date)` → `cultivationmethod::apply(model, true)` →
`monicamodel::step(model)` → `cultivationmethod::apply(model, false)` →
`monicamodel::dailyReset(model)` - which drives the full real Hohenfinow2 season
through the real `crop-min.json` rotation end to end, including the real
`currentCropModule` clear-to-null after automatic harvest fires (via `dailyReset`,
never exercised in this form by any earlier phase-6 oracle, all of which left the
harvested crop module stale rather than actually cleared). `odin/tests/cpp_ref/
monica_model_step_ref_main.cpp` + `odin/tests/monica_model_step_ref/main.odin`, run
by `run_monica_model_step.sh`.

### Phase 7 — run loop + output — **DONE, phase 8's regression goal reached along the way**
`run-monica.cpp` (`runMonica`, `StoreData`, `setupStorage`, `Spec` evaluation),
`io/output.cpp` (`OId`, `Output`), `io/csv-format.cpp`, `cmd/monica-run`.

**Checkpoint 1 — `io/output.odin` — done.** `OId` (+ its two enums, `isRange`/`isOrgan`/
`toString`/`outputName`) and the `Output`/`Output::Data` shapes the CSV write path builds and
consumes. `oid::merge`/`to_json` and `Output::merge`/`to_json`/`customId`/`errors`/`warnings`
are not ported - RPC/env-round-trip-only callers this port drops, same "port on demand" already
used throughout phase 6. `resultsObj` (the `"obj-outputs?"` path) is dropped too: `sim-min.json`
never sets that flag. Pure data structure / pure functions - verified via `odin check` only, no
oracle needed.

**Checkpoint 2 — `io/build_output.odin` — done.** `applyOIdOP`, `getComplexValues`, and
`parseOutputIds`, plus a 21-entry `buildOutputTable` replacement covering exactly the ids
`sim-min.json`'s `output.events` needs (CM-count, Date, Year, Crop, Stage, AbBiom, OrgBiom,
Yield, LAI, Mois, Irrig, RunOff, Kc, Recharge, NLeach, SOC, Tavg, Precip, Globrad, N, ETa/ETc)
instead of the full ~300-entry C++ table. Each entry's `id` is purely an internal map key in
both languages (csv-format.cpp writes `name`/`displayName`/`unit`, never `id`), so this port's
own 0..20 numbering doesn't need to match C++'s registration-order numbering - only internal
self-consistency (`parseOutputIds` resolves a name to an id, the table resolves that id to a
function) matters. Drops the `setfs`/`SetValue`-workstep machinery and the
`getCompareOp`/`buildCompareExpression` machinery: grepped `sim-min.json`'s entire
`output.events`/`_events` section (the `_events`/`__events` siblings are underscore-disabled and
never read) and confirmed every spec is a shortcut string, a workstep-event-name string, or a
plain output-id array - never the `["while"|"at", [oid, "op", value]]` comparison-expression
array syntax that machinery exists for.

**Checkpoint 3 — `io/csv_format.odin` — done.** `writeOutputHeaderRows` and `writeOutput`.
`writeOutputObj` is not ported (unreachable, same reason as `resultsObj` above). Two format
details needed empirical verification, not just reading the source:
- **Number formatting.** C++'s `ostream << double` with no precision/flags set anywhere in
  `csv-format.cpp` uses the stream's default - 6 significant digits, `%g`-style (scientific
  below 1e-4 or at/above 1e6, trailing zeros trimmed, `e+06` not `e+006`, `-0` preserved).
  Verified empirically (a throwaway C++ probe vs a throwaway Odin probe over representative
  magnitudes) that Odin's `fmt` `"%.6g"` verb reproduces this byte-for-byte. That's the
  formatting `write_json_scalar` uses for every NUMBER value.
- **Line endings.** The checked-in `sim-min-out_section_*.csv` baselines use `\r\n`: the C++
  side writes through a `std::ofstream` opened without `ios::binary`, and Windows text-mode
  translates `endl`/`'\n'` to `\r\n`. `io.Writer` does no such translation, so every line in
  `csv_format.odin` ends with an explicit `"\r\n"` rather than relying on the OS.

**Oracle - byte-identical**, a synthetic set of `OId`s (organ-indexed, layer-range, layer-
aggregate, plain scalar, string-valued) and values chosen to exercise both format details above
plus the escaping paths (a display name containing commas, a JSON-input string containing
quotes). `odin/tests/cpp_ref/csv_format_ref_main.cpp` + `odin/tests/csv_format_ref/main.odin`,
run by `run_csv_format.sh`.

**Checkpoint 4 — `run/run_monica.odin` — done.** `CropRotation`, `Env` (+ `env_merge`), `Spec`
(+ `setupStorage`), `StoreData` (+ `store_data_aggregate_results`,
`store_data_store_results_if_spec_applies`), and a genuinely single-model `run_monica` written
directly rather than as an `isIC=false` branch of a dual-model function. `Spec`'s six
`std::function<bool(const MonicaModel&)>` fields become a `Spec_Expr` (data: a kind tag +
day/month/year/eventName) plus a single `spec_expr_eval` dispatcher, not function pointers -
Odin procs can't capture, and unlike the "one live model" global-pointer workaround used
elsewhere in this port, here many differently-parameterised expressions (every shortcut's date
pattern, every workstep event name) coexist at once, so there's no single global to redirect
calls through; the captured data has to live somewhere, and a plain struct is the direct Odin
equivalent. Drops the second `MonicaModel`/all Intercropping sync branches, Cap'n Proto state
load/save, and the daily-function registration loop (`workstep::registerDailyFunction` - the
only workstep needing it, `AutomaticSowing`, is unused by `crop-min.json`'s rotation).

**This oracle is the first thing in the whole port to exercise the production
`absApply`/`nextAbsoluteCMApplicationDate` crop-rotation-cycling path end to end** - every
earlier crop-growth oracle, including phase 6 checkpoint 6's, drove worksteps through the
simpler literal-date `apply()` overload instead as a shortcut, which (being matched against a
genuinely *relative* `Tools::Date`, whose `operator==` compares `isRelativeDate()` too) never
actually fired a relative-dated static workstep like `Sowing` at all - meaning no earlier oracle
had actually validated real crop growth past its initial state. This one caught a real bug:
`workstep.odin`'s `sowing_merge` merged species/cultivar JSON straight into a zero-value
`Crop_Parameters` instead of one built via `make_crop_parameters()` first. C++'s
`CropParameters` carries in-class defaults (the Farquhar-model constants KC25/KO25/AEKC/AEKO/
AEVC), and species/cultivar files routinely omit fields meant to just take those defaults;
without the `make_` call every such field silently read back as 0. For wheat this zeroed
KC25/KO25, turning the CO2Method==3 assimilation formula's `Mkc*Oi/Mko` term into a 0/0 NaN as
soon as the crop reached its second developmental stage - `max(0.1, NaN)` returns the non-NaN
operand, so this failed silently (no crash) as a hard floor on `vc_AssimilationRate`, freezing
biomass growth solid from then on. Fixed by calling `make_crop_parameters()` before merging in
`sowing_merge` (shared by `Sowing`/`Transplant`/`AutomaticSowing`). See
`[[monica-odin-port-make-before-merge-bug]]` - worth a grep pass for the same shape elsewhere.

**Oracle - byte-identical, 2593 lines, 5 sections, the full ~7-year Hohenfinow2 crop rotation.**
`odin/tests/cpp_ref/run_monica_ref_main.cpp` (calls the real C++ `runMonica`) vs
`odin/tests/monica_run_ref` (calls the real `run_monica`), both loading the actual
`installer/Hohenfinow2/sim-min.json`+`crop-min.json`+`site-min.json` and dumping every output
section through the checkpoint-3-verified CSV writer. `run_monica_run.sh`.

**Checkpoint 5 — `cmd/monica-run` — done.** Full CLI arg parsing
(`-d`/`-sd`/`-ed`/`-m`/`-op`/`-o`/`-c`/`-s`/`-w`/`-h`/`-v`), sim.json loading and crop/site/
climate path resolution, env construction via `create_env_json_from_json_objects` + `env_merge`,
and both output-writing modes (`-m`: one CSV per output section; default: all sections in one
file). Dropped, all "port on demand": Cap'n Proto/ZeroMQ RPC (`-icrsr`/`-icwsr` and the sturdy-
ref soil/climate/intercropping connections), the entire `output2`/`-o2` Intercropping half, and
the `getCapillaryRiseRate` closure wiring - already baked directly into
`soil_moisture.odin`'s `read_capillary_rise_rates`, no per-run wiring needed (same pattern
already used for `calculateAndSetPwpFcSatFunctions` in `central_parameter_provider_merge`).

Also fixed a second latent bug, found the same way as checkpoint 4's: a heap-corrupted output-
directory string traced back to `support/tools/files.odin`'s `fix_system_separator`. `strings.
replace_all` returns its input string unallocated (aliased, not copied) when there's nothing to
replace (e.g. `path=="."`, no `/` to turn into `\`), but `fix_system_separator`'s `defer
delete(step1, allocator)` deleted it unconditionally - freeing the *caller's own* string out
from under it whenever the path needed no separator substitution. Nothing before this checkpoint
had happened to both call `fix_system_separator` (or `ensure_dir_exists`, which calls it) *and*
keep using the original argument string afterward. Fixed by only deleting `step1` when
`strings.replace_all`'s `was_allocation` return says it actually allocated.

**Phase 8's regression goal reached here, via the real CLI rather than just an in-process
oracle:** `build/monica-run.exe -m` (C++) and the new Odin `monica-run.exe -m` (checkpoint 5),
run independently (separate working directories - `-op` turns out to be a no-op in the real
C++ too, see the CLI's own header comment) over the real `sim-min.json`, produce byte-identical
output in **both** CLI modes:
- `-m` (one file per section): all 5 `sim-min-out_section_*.csv` files identical between the two
  binaries, and `daily`/`crop` are additionally byte-identical to the checked-in
  `sim-min-out_section_{daily,crop}_3.6.60.csv` baselines.
- default (single combined file): identical between the two binaries.

### Phase 8 — regression close-out — **DONE, folded into phase 7 checkpoint 5**
Widening beyond `sim-min.json` (the larger `sim.json`/`sim+.json` Hohenfinow2 configs, more
output ids) is future "port on demand" work, not required for this port's original scope.

### Phase 9 — ZeroMQ server — **DONE**
`src/run/serve-monica-zmq.{h,cpp}` (`serveZmqMonicaFull`), `src/run/monica-zmq-server-main.cpp`,
`src/run/monica-zmq-defaults.h`, and the `Msg`/`receiveMsg`/`s_send`/`s_sendmore` pieces of
`mas_cpp_misc/zeromq/zmq-helper.{h,cpp}` it depends on. Originally table-1 "explicitly dropped"
as an "RPC entry point"; revisited because the server itself has no Cap'n Proto dependency once
`INCLUDE_SR_SUPPORT` (sturdy-ref soil/climate lookups) and Intercropping are dropped, same as
`cmd/monica-run` already drops them.

**Odin ZMQ bindings.** No Odin standard-library ZMQ support exists; vendored
`github.com/qxuken/odin-zmq-bindings` (libzmq 4.3.5 C API) into `odin/support/zeromq`, with two
changes from upstream:
- The Windows `foreign import` pointed at an ambient `libzmq.lib` on the linker's `LIB` path;
  replaced with a vendored import lib (`windows/libzmq.lib`, built from vcpkg's
  `zeromq:x64-windows` dynamic-triplet port) so the build doesn't depend on machine-local linker
  configuration. The matching runtime DLL (`windows/libzmq-mt-4_3_5.dll`) has to sit next to any
  built `.exe` that imports this package - Windows' default DLL search order includes the exe's
  own directory.
- **Fixed a real upstream bug**: `zmq_pollitem_t.events`/`.revents` are `short` (2 bytes) in
  `zmq.h` on every platform, and `.fd` is the platform socket handle (`SOCKET`, 8 bytes, on
  Windows). The upstream binding declared both as `c.int`, which happens to keep `sizeof(Poll_Item)`
  right (padding absorbs the difference) but shifts every field after `fd` to the wrong byte
  offset - `zmq_poll()` (compiled against the real 2-byte-`short` layout) then reads garbage
  `revents`, so `POLLIN` is never observed even though the message already arrived. Silent hang,
  not a crash: `serve_zmq_monica_full` blocked forever in its poll loop despite the peer having
  already sent and the socket having real data queued. Fixed in the vendored copy
  (`bindings.odin`): `FD` is `uintptr` on Windows / `c.int` elsewhere, and `Poll_Item`/
  `Poller_Event`'s event fields are `i16`.

**Dropped**, matching `run_monica.odin`'s own file header and this port's established
"port on demand" pattern: the `INCLUDE_SR_SUPPORT` Cap'n Proto sturdy-ref branches
(`kj::setupAsyncIo`/`ConnectionManager`, soil-profile and climate-timeseries sturdy-ref lookups)
and Intercropping (`isIC`, the second `MonicaModel`/`out2` half, the `"1"`/`"2"` object reply).
`debug()` trace calls are dropped too, consistent with `cmd/monica-run` (no debug-trace facility
is ported); `env.debugMode`'s assignment is kept for structural parity even though nothing reads
it.

**`Env`'s dropped `climateCSV`/`pathsToClimateCSV`/`csvViaHeaderOptions` fields.** `run_monica.odin`
dropped these from `Env` because `cmd/monica-run`'s CLI path always resolves climate data into
`env["climateData"]` before `env_merge` runs (`create_env_json_from_json_objects` reads the CSV
itself). The ZMQ server doesn't have that luxury - real producers
(`installer/Hohenfinow2/python/run-producer.py`) send `pathToClimateCSV`/`csvViaHeaderOptions`
over the wire and expect the server to read the CSV. Rather than re-adding the fields to `Env`
(which `run_monica`'s 12,000 lines of phase-4/5 code never needed), `serve_zmq.odin` reads those
three keys directly off the incoming `jx.Value` message and resolves the `Data_Accessor` itself,
mirroring what `env_merge` used to do with them before this port's `Env` was slimmed down.

**`output::to_json`/`oid::to_json`**, dropped by phase 7 checkpoint 1 ("nothing in the CSV write
path reads them"), were the demand this phase supplied: added to `io/output.odin` along with the
`customId`/`errors`/`warnings` fields `Output` had also dropped, since `serveZmqMonicaFull`
round-trips `customId` and reports env-merge/climate-read errors back over the wire.

**Regression - byte-identical**, via the real producer/consumer pipeline rather than an in-process
oracle: `installer/Hohenfinow2/python`'s `pixi run run_prod_cons_pipeline` (see that directory's
`run_producer_consumer_pipeline.cmd` - swap the `monica-zmq-server`/`monica-zmq-server-odin` path
there to switch backends) drives the real `run-producer.py`/`run-consumer.py` against
`sim-min.json` over live ZMQ sockets. All 5 output sections
(`crop`/`daily`/`run`/`yearly`/`OrganicFertilization`) came back byte-identical between the C++
and Odin servers.

### Phase 10 — reflection-driven output access — **read side DONE**
See **`plan-reflective-outputs.md`** for the full design; the short version of what it changes
here.

Phase 7 checkpoint 2 deliberately scoped `buildOutputTable` down to the 21 ids `sim-min.json`
needs, out of the C++'s 181. That scoping is what this phase removes. `support/reflectpath`
compiles a field *path* (`"soilColumn.layers.vs_SoilNH4"`, `"climateData.#last.tavg"`) against
`Monica_Model` into flat pointer arithmetic, and `OId` gains a `plan` pointer that
`store_results` dispatches on. A `name -> path` alias table then restores **125 of the 181**
legacy output names for one table row each - `python odin/tools/gen_output_aliases.py`
regenerates it from `build-output.cpp`. The remaining 56 are genuinely computed (proc calls,
arithmetic, `Date` methods, organ-count guards, the `numberOfOrganicLayers` clamp) and stay as
lambdas; `output_paths.odin` lists them and says why for each group.

Unknown output names now warn at setup instead of silently costing you a column, and a raw path
or a `{"path": ..., "unit": ..., "round": ...}` object works in `sim.json` with no table entry at
all.

**Regression - byte-identical.** `bash odin/tests/diff_outputs.sh` self-diffs both fixtures; the
12 ids that moved onto the engine on the pre-existing `sim-min.json` baseline (CM-count, Kc,
Irrig, AbBiom, LAI, Mois, RunOff, NLeach, Recharge + the Tavg/Precip/Globrad map paths) did not
move a byte, and neither did the 9 computed ones still on lambdas. `"use-legacy-output-fns?":
true` in `sim.json`'s `output` section forces the lambda tier for A/B bisection; that A/B is also
byte-identical, and un-measurable in wall clock (0.31s either way).

**The SetValue workstep writes through the same engine.** Phase 7 could only set the two ids the
C++ registers a `setf` for (`Stage`, `Mois`); any field a path reaches is now settable, via one
pair of tier-agnostic entry points (`oid_get_value`/`oid_set_value`) that both the read and write
sides share. `#len` is refused - its `resolve` returns the plan's scratch slot, so a write would
silently change nothing.

**`buildPrimitiveCalcExpression` is ported** (the `["=", a, op, b]` form in a SetValue's `value`),
which phase 7 checkpoint 2 had deferred alongside `buildCompareExpression`. Its operands run
through `parse_output_ids`, so they reach the path tier too. Three C++ behaviours kept and pinned
by tests - an unknown operator evaluates to `0.0` forever, two literal operands do not build, and
the array/array case returns booleans because the C++ accumulates into a `vector<bool>`.
`buildCompareExpression` itself stays unported: the `Spec` evaluator has its own.

`installer/Hohenfinow2/sim-min-setvalue.json` is the third fixture in `diff_outputs.sh` and the
only coverage SetValue has ever had here.

---

## 7. Explicitly dropped

| Dropped | Reason |
| --- | --- |
| `Intercropping` (`monica-parameters.h:997`, `env.ic`, `runMonicaIC`'s second output) | needs Cap'n Proto RPC; also removes the duplicated `output2` half of `monica-run-main.cpp` |
| `SaveMonicaState` workstep + `deserializeFullState` (`run-monica.cpp:547-607`) | inherently Cap'n Proto. `sim-min.json`'s `serializedMonicaState` has `load.atStart=false` / `save.atEnd=false`, so the fixture is unaffected |
| all `serialize` / `deserialize` | Cap'n Proto; isolated by Prep 1 |
| `run/monica-capnp-*`, `run/run-monica-capnp.*`, `run/capnp-helper.*`, `run/daily-monica-fbp-component-main.cpp` | Cap'n Proto RPC/FBP entry points |
| `run/monica-zmq-*`, `run/serve-monica-zmq.cpp` | ~~RPC entry points~~ **ported, see phase 9** - the ZMQ server itself needs no Cap'n Proto; only its `INCLUDE_SR_SUPPORT` sturdy-ref branches (still Cap'n Proto) and Intercropping stayed dropped |
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
