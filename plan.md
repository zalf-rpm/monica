# Plan

## Refactoring goals (important)

1. Convert `_simple` modules to **plain struct + explicit free procedures**.
2. Keep converted procedures **declared in the corresponding module's `.h` interface**, even if currently only used internally.
3. Prefer direct named procedures over hidden anonymous-namespace orchestration.
4. Optimize for readability/simple structure/modular testability over encapsulation in this phase.
5. Continue stepwise (small safe conversions), preserving behavior and keeping build green.
6. Once a module's struct + free procedures are stable, move *only the free procedures* into their
   own lower-case namespace (e.g. `monica::cropmodule`) so they don't need a type-name prefix. The
   struct itself and its `makeXYZ(...)` constructor function(s) stay declared directly in `monica`
   (not nested in the module namespace, no `using X = x::X;` alias) — the struct's name is already
   unique project-wide and `makeXYZ` already bakes in the module name, so nesting them adds an
   indirection (the alias) without solving any real name collision. External call sites therefore
   write `makeXYZ(...)` unqualified but `x::someProcedure(...)` qualified for the free procedures.
   (This was reached after initially nesting struct+`makeXYZ` too for several modules; see
   `soilmoisture`/`soiltransport`/`soiltemperature`/`soilorganic`/`soilcolumn`/`soillayer` below —
   all were converged onto the leaner pattern that `cropmodule`/`snowcomponent`/`frostcomponent`
   used from the start.)

## File renames (done)

The originally `_simple`-suffixed core files are now the sole implementations of their modules, so
the suffix was dropped and all `#include`s / `CMakeLists.txt` entries were updated accordingly:

- `crop-module_simple.h/.cpp` -> `crop-module.h/.cpp`
- `soilcolumn_simple.h/.cpp` -> `soilcolumn.h/.cpp`
- `soilmoisture_simple.h/.cpp` -> `soilmoisture.h/.cpp`
- `soilorganic_simple.h/.cpp` -> `soilorganic.h/.cpp`
- `soiltemperature_simple.h/.cpp` -> `soiltemperature.h/.cpp`
- `soiltransport_simple.h/.cpp` -> `soiltransport.h/.cpp`

Each rename was followed by a full build + `monica-run` regression check against
`sim-min-out_section_crop_orig.csv` with no differences found.

## Current state summary

### `soilorganic` (status: complete)

1. Constructors and member `serialize`/`deserialize` are removed; `makeSoilOrganic(...)` + free
   `soilOrganicSerialize/Deserialize` are used.
2. Main internal process methods were converted to free procedures and rewired:
   - `soilOrganicFoUrea`
   - `soilOrganicFoMIT`
   - `soilOrganicFoVolatilisation`
   - `soilOrganicFoNitrification`
   - `soilOrganicFoSticsNitrification`
   - `soilOrganicFoDenitrification`
   - `soilOrganicFoSticsDenitrification`
   - `soilOrganicFoN2OProduction`
   - `soilOrganicFoSticsN2OProduction`
   - `soilOrganicFoPoolUpdate`
   - `soilOrganicFoNetEcosystemProduction`
   - `soilOrganicFoNetEcosystemExchange`
   - decomposition/nitrification helper procedures (kaiteew + non-kaiteew + hydrolysis/nitrification/denitrification helpers)
3. Trivial setter/getter wrappers were inlined at call sites and removed where safe.
4. External wiring updated (notably `monica-model.cpp`, `build-output.cpp`, `cultivation-method.cpp`).
5. Remaining public getters in `soilorganic.h` intentionally kept where they still provide non-trivial conversions or are still used as interface.
6. The free procedures were moved into a `monica::soilorganic` namespace with the `soilOrganic`
   prefix dropped (e.g. `soilOrganicFoUrea` -> `soilorganic::foUrea`). `struct SoilOrganic` and
   `makeSoilOrganic(...)` stay directly in `monica` (leaner pattern, see goal #6 above) — no
   `using SoilOrganic = ...` alias. Call sites (`monica-model.cpp`, `cultivation-method.cpp`,
   `build-output.cpp`) were rewired: `makeSoilOrganic(...)` unqualified, the free procedures
   qualified as `soilorganic::...`. Build + `monica-run` output comparison against
   `sim-min-out_section_crop_3.6.60.csv` and `sim-min-out_section_daily_3.6.60.csv` (mainline
   MONICA without the refactorings) remained identical.

### `soilmoisture` (status: complete)

1. Member constructors and member `serialize`/`deserialize` are removed from `soilmoisture.h/.cpp`.
2. Procedural initializer added: `initializeFromParams(SoilMoisture* sm)`.
3. `makeSoilMoisture(...)` constructs the plain struct and initializes/deserializes procedurally.
4. `deserialize(...)` / `serialize(...)` contain full logic directly (no forwarding to removed members).
5. All former `SoilMoisture` member getters/setters were removed; direct field access or free
   procedures now cover the former API surface.
6. The free-procedure API lives in `monica::soilmoisture`; `struct SoilMoisture` and
   `makeSoilMoisture(...)` stay directly in `monica` (leaner pattern, see goal #6 above), no alias.

### `soiltransport` (status: complete)

1. The flat `soilTransport...` free procedures were moved under `monica::soiltransport` and renamed
   without the prefix; `struct SoilTransport` and `makeSoilTransport(...)` stay directly in `monica`
   (leaner pattern, see goal #6 above), no alias.
2. Remaining trivial getters were removed and their call sites were inlined to direct field access.
3. File renamed from `soiltransport_simple.*` to `soiltransport.*`.

### `soiltemperature` (status: complete)

1. The free procedures were moved under `monica::soiltemperature`; `struct SoilTemperature` and
   `makeSoilTemperature(...)` stay directly in `monica` (leaner pattern, see goal #6 above), no alias.
2. Remaining trivial accessors were inlined and removed after the namespace move.
3. File renamed from `soiltemperature_simple.*` to `soiltemperature.*`.

### `crop-module` (status: proceduralization pass completed)

1. Class-style methods were removed from `CropModule` and replaced by free procedures.
2. Remaining trivial accessors/setters were inlined to direct struct-member access at call sites.
3. Non-trivial former methods were converted to free procedures and rewired (including crop growth, dry matter, N/C yield content, maturity/anthesis helpers, transplant and cutting flow, and VOC helpers).
4. The free-procedure API lives in `monica::cropmodule` and uses names without the old `cropModule` prefix.
5. Construction/serialization/deserialization use plain-struct flow via `makeCropModule(...)` plus procedural `cropmodule::serialize` / `cropmodule::deserialize`.
6. File renamed from `crop-module_simple.*` to `crop-module.*`.
7. Build and `sim-min-out_section_crop.csv` hash comparison against `_orig` remained unchanged after each conversion batch.

### `snow-component` (status: proceduralized + namespaced)

1. `SnowComponent` is a plain struct; former reference members were converted to pointers so it can stand alone.
2. Constructors/member methods were replaced by free procedures.
3. Free-procedure API lives under `monica::snowcomponent` and no longer uses the `snowComponent...` prefix.
4. `soilmoisture.cpp` call sites were rewired to `snowcomponent::...`.

### `frost-component` (status: proceduralized + namespaced)

1. `FrostComponent` is now a plain struct with pointer members (reference member replaced).
2. Constructors/member serdes were converted to free procedures and wiring updated.
3. All remaining class-style methods were converted to free procedures and call sites were rewired.
4. The free-procedure API lives in `monica::frostcomponent` with unprefixed names.
5. Build + `monica-run` + `sim-min-out_section_crop.csv` vs `_orig` remained identical after each conversion step.

### `monica-model` (status: complete)

1. `MonicaModel` is a plain struct (no member methods); `makeMonicaModel(...)` constructs it.
2. All behavioral methods (step, generalStep, cropStep, fertiliser/irrigation/tillage application,
   crop seeding/harvest/incorporation, event handling, CO2/groundwater helpers, etc.) are free
   procedures in `monica::monicamodel`, taking `MonicaModel*` as first argument.
3. External call sites (`run-monica.cpp`, `daily-monica-fbp-component-main.cpp`, `cultivation-method.cpp`) are rewired to the free procedures.
4. Build + `monica-run` + crop-output identity check remained unchanged after the current conversion batch.

### `soilcolumn` / `SoilLayer` (status: complete)

1. `SoilLayer` was converted to a plain struct (data members only), declared directly in `monica`
   (not nested in a namespace — leaner pattern, see goal #6 above).
2. Its two constructors were replaced by `makeSoilLayer(vs_LayerThickness, soilParams)`, declared
   directly in `monica` (the unused reader-constructor was dropped; the default constructor is now
   implicit since the struct is an aggregate).
3. Member `deserialize`/`serialize` became free `soillayer::deserialize`/`soillayer::serialize` in
   a `monica::soillayer` namespace holding only the free procedures.
4. `vs_SoilMoisture_pF()` became `soillayer::soilMoisturePF(const SoilLayer*)`; `get_SoilNmin()`
   became `soillayer::soilNmin(const SoilLayer*)` (both do real computation, unlike the getters below).
5. All trivial getters/setters (pure field passthroughs, including simple one-line forwards to
   `_sps`, e.g. `vs_SoilOrganicCarbon()` -> `_sps.vs_SoilOrganicCarbon()`) were inlined at call sites
   and removed. This touched `frost-component.cpp`, `monica-model.cpp`, `soilmoisture.cpp`,
   `soilorganic.cpp`, `soiltemperature.cpp`, `build-output.cpp`, `cultivation-method.cpp`, and
   `run-monica.cpp`. (`_sps` was later renamed to `sps` in `8c36201` and then removed entirely when
   its fields were flattened into `SoilLayer` — see "Follow-up" step 4 below.)
6. File renamed from `soilcolumn_simple.*` to `soilcolumn.*` (done in an earlier step, before the
   `SoilLayer` conversion).
7. `SoilColumn` was converted in three stages, each with its own build + regression check:
   - Stage 1: the member methods that already had a thin free-function wrapper
     (`soilColumnApplyMineralFertiliser`, `soilColumnApplyTillage`, `soilColumnDeserialize`, ...)
     had their real implementation moved into the free function, turning the member method into the
     thin forwarder instead (reversing the direction).
   - Stage 2: the constructors and remaining member methods without a free-function counterpart
     (`calculateNumberOfOrganicLayers`, `applyMineralFertiliserViaNDemand`, `vs_NumberOfLayers`,
     `vs_NumberOfOrganicLayers`, `vs_LayerThickness`, `get_DailyCropNUptake`,
     `getLayerNumberForDepth`, `sumSoilTemperature`) got new free functions, all member methods and
     both constructors were removed, and `makeSoilColumn(...)` now builds the plain struct directly.
     `SoilColumn` became a genuine aggregate (at this point still with a public
     `std::vector<SoilLayer>` base, no user-declared constructors; the base was replaced by an
     explicit `layers` member later — see "Follow-up" below).
   - Stage 3: all `soilColumnXxx` free functions moved into a `monica::soilcolumn` namespace with the
     prefix dropped (e.g. `soilColumnApplyIrrigation` -> `soilcolumn::applyIrrigation`). `SoilColumn`
     itself and `makeSoilColumn(...)` stay directly in `monica` (leaner pattern, see goal #6 above),
     so headers that only need the type (`frost-component.h`, `snow-component.h`, `soilmoisture.h`,
     `soilorganic.h`, `soiltransport.h`) forward-declare plain `struct SoilColumn;` and reference the
     unqualified name in field/parameter types — no nested-namespace forward declaration or alias
     needed.
8. `DelayedNMinApplicationParams` (nested inside `SoilColumn`) and `AOM_Properties` (also declared in
   `soilcolumn.h`) still had member `serialize`/`deserialize` at this point — since converted, see
   "Follow-up" step 2 below.

### Follow-up: the `SoilColumn` / `SoilLayer` / `SoilParameters` refactor (status: complete)

A four-step follow-up that finished off the soil data structures. Each step was its own commit with
its own build + regression check; all four verified byte-identical against
`sim-min-out_section_crop_3.6.60.csv` / `sim-min-out_section_daily_3.6.60.csv`.

**Step 1 — `SoilColumn` drops the `std::vector<SoilLayer>` base (`038dbe6`).**
`SoilColumn` no longer publicly inherits `std::vector<SoilLayer>`; it holds the layers as an
explicit `layers` member. Every call site relying on the inherited vector semantics (`operator[]`,
`.at()`, `.size()`, `.empty()`, `.back()`, range-for, `push_back`, `resize`) now goes through
`.layers` — this touched `crop-module.cpp`, `soilmoisture.cpp`, `soilorganic.cpp`,
`soiltemperature.cpp`, `soiltransport.cpp`, `frost-component.cpp`, `monica-model.cpp`,
`build-output.cpp`, `run-monica.cpp`, `workstep.cpp` and `automatic-sowing.cpp`.

**Step 2 — `AOM_Properties` / `DelayedNMinApplicationParams` serde (`eda85a7`).**
- `AOM_Properties::serialize`/`deserialize` -> `aomproperties::serialize`/`deserialize`.
- `SoilColumn::DelayedNMinApplicationParams::serialize`/`deserialize` -> free
  `soilcolumn::serializeDelayedNMinApplicationParams` / `deserializeDelayedNMinApplicationParams`
  (the struct itself stays nested inside `SoilColumn`).
- The generic `setComplexCapnpList` / `setFromComplexCapnpList` helpers call `.serialize()` /
  `.deserialize()` as *member* methods, so they no longer applied to `vo_AOM_Pool` /
  `_delayedNMinApplications`; those call sites now use explicit loops calling the free functions,
  mirroring how `soilcolumn::serialize`/`deserialize` already handled the list of layers.

**Step 3 — `Soil::SoilParameters` proceduralized (`b6cc4ea`).**
Brings `SoilParameters` in line with the plain-struct + free-procedures convention (goal #6). At
this step it was still composed inside `SoilLayer` as the `sps` member (flattening is step 4).
- Dropped `: public Tools::Json11Serializable`; `merge`/`to_json` are now free
  `Soil::soilparameters::merge` / `to_json`.
- Capnp `serialize`/`deserialize` -> free `soilparameters::serialize`/`deserialize`.
- Constructor -> free `Soil::makeSoilParameters(...)`, same default `setPwpFcSat` callback behavior.
- The four getter/setter pairs carrying fallback logic became free resolved-value procedures
  `soilparameters::soilRawDensity` / `soilBulkDensity` / `soilOrganicCarbon` / `soilOrganicMatter`
  `(const SoilParameters*)`. Their formerly-private backing fields are now plain public
  `_vs_SoilRawDensity` / `_vs_SoilBulkDensity` / `_vs_SoilOrganicCarbon` / `_vs_SoilOrganicMatter`,
  keeping the leading underscore both to match the existing `_delayedNMinApplications` convention
  for "internal but not truly encapsulated" members and to avoid colliding with the new
  resolved-value free procedures. `-1` means "unset"; read the underscore field only when the raw
  override is genuinely what's wanted, otherwise call the resolved getter.
- `vs_SoilSiltContent()` / `isValid()` -> free `soilparameters::soilSiltContent` / `isValid`.

**Step 4 — `SoilLayer` embeds the `SoilParameters` fields (`a765e40`).**
`SoilLayer` no longer composes a `Soil::SoilParameters sps` member. All of its fields are now
directly on `SoilLayer`, except `thickness` and `calculateAndSetPwpFcSat`, which are config /
parse-time-only concepts and stay behind in `SoilParameters`.
- `makeSoilLayer(...)` copies the *resolved* `SoilParameters` values field-by-field onto the new
  `SoilLayer` rather than storing the whole struct.
- `soillayer::serialize`/`deserialize` read/write those fields directly (still into the capnp-nested
  `Sps` sub-message), since there is no longer a `SoilParameters` sub-object to delegate to.
- Added `soillayer::soilRawDensity` / `soilBulkDensity` / `soilOrganicCarbon` / `soilOrganicMatter`
  / `soilSiltContent` `(const SoilLayer*)`, mirroring the step-3 `Soil::soilparameters::` procedures
  but operating on `SoilLayer`'s own fields. On `SoilLayer` the raw/override fields were then
  renamed *without* the leading underscore (`vs_SoilRawDensity` etc.), since they are no longer
  private and there is no same-named member accessor left to collide with. The equivalent rename
  in `Soil::SoilParameters` was simply missed — its fields are still `_vs_`-prefixed. This is an
  oversight, not a deliberate distinction; see "What to do next".
- ~110 `.sps.` / `->sps.` call sites were updated across `crop-module.cpp`, `soilorganic.cpp`
  (heaviest), `build-output.cpp`, `soilmoisture.cpp`, `soilcolumn.cpp` and the rest.
- `Soil::SoilParameters` itself is untouched by this step and is still used standalone for
  `site.json` horizon-spec parsing (`SoilPMs`) before a `SoilColumn` is built.

### `src/soil` (status: moved into the main tree, not yet transformed)

`c419285` copied the soil library out of the `mas_cpp_misc` submodule into the main tree as
`src/soil/{soil,conversion,constants}.{h,cpp}` and added them to the `monica_lib` target; the
`add_subdirectory(mas_cpp_misc/soil soil)` block in `CMakeLists.txt` is commented out. Include
paths are unchanged (`#include "soil/soil.h"` still resolves, now to `src/soil/`).

This was done specifically so that this code can be refactored along with the rest of the project
rather than across a submodule boundary — step 3 above (`SoilParameters`) is the first piece of it
to be converted. `conversion.cpp` and `constants.cpp` are still in their original form.

### `MineralFertilizerParameters` (status: accessors inlined, real methods untouched)

1. Converted from `class` (with an explicit `public:` section) to `struct`; the `private:` section
   was removed and its fields (`id`, `name`, `vo_Carbamid`, `vo_NH4`, `vo_NO3`) are now public.
2. The 10 trivial accessors (`getId`/`setId`, `getName`/`setName`, `getCarbamid`/`setCarbamid`,
   `getNH4`/`setNH4`, `getNO3`/`setNO3`) were removed. Only 3 had any live caller in the whole
   codebase (`fp.getNO3()`/`getNH4()`/`getCarbamid()` in `soilcolumn.cpp`'s `applyMineralFertiliser`),
   inlined to direct field access; the rest were dead code.
3. Constructors, `deserialize`, `serialize`, `merge`, `to_json` were intentionally left untouched
   (per goal of doing this as a first, low-risk step) — they already read/wrote the fields directly,
   never through the removed accessors.

## Regression history: the `ff0f0fc` duplicate-state divergence (RESOLVED)

Commit `ff0f0fc` ("removed copies of external params from crop module - is buggy and doesn't give
the same result -> to be fixed") removed the remaining copied-external-param fields from
`CropModule` and diverged from the 3.6.60 baseline (wrong yields, wrong harvest dates, starting in
the first simulated season of `installer/Hohenfinow2/sim-min.json`). Two independent root causes:

1. **Vernalisation factor.** `CropParameters::__enable_vernalisation_factor_fix__` was flattened
   from `kj::Maybe<bool>` to a plain `bool{false}`, which silently dropped the fallback to
   `CropModuleParameters::__enable_vernalisation_factor_fix__` (set to `true` by the top-level
   `"CropParameters"` block in `crop-min.json`, and never set by the crop's own
   `species`/`cultivar` JSON). Without it the `vc_VernalisationFactor` upper clamp in
   `cropmodule::fcVernalisationFactor` never ran, so the factor grew past `1.0` through the winter
   and wrecked phenology by year 1.

2. **Stale duplicated-state reads.** Call sites still read `pc_`/`residuePs` members that had been
   removed from `CropModule` — notably `soilcolumn::applyIrrigationViaTrigger` reading
   `cropModule->pc_HeatSumIrrigationStart/End`, plus the `residuePs` -> `residueParams` uses in
   `monica-model.cpp` and `sowing.cpp`. This surfaced as a small soil-moisture divergence that
   *appeared* to originate inside `soilmoisture::evapotranspiration(...)`, even though
   `soilmoisture.cpp` was textually unchanged — the actual cause was upstream.

**Both fixed in `f31fe97`** ("fixed introduced bugs and crop module has now (almost) no duplicate
state"): `__enable_vernalisation_factor_fix__` restored to `kj::Maybe<bool>` (with a comment
explaining why unset must not mean `false`), and the stale reads repointed at
`cropParams.cultivarParams.*` / `residueParams`.

Verified at HEAD: `sim-min-out_section_crop.csv` and `sim-min-out_section_daily.csv` are
byte-identical to `sim-min-out_section_crop_3.6.60.csv` / `sim-min-out_section_daily_3.6.60.csv`.

**Lesson for future conversion steps.** When a refactor step changes results, first look for a
field that used to be an *optional-with-fallback* (crop-specific override, else module-level
default) that got flattened to a plain value carrying only the "unset" default, and for call sites
still reading a duplicated copy that has since been removed. `git diff` filtered on `kj::Maybe<`
and `.orDefault(` is the highest-signal search — much faster than diffing whole functions
line-by-line. Note also that the *symptom* can appear in a file that is textually unchanged.

## What to do next (if starting fresh)

1. ~~Convert `DelayedNMinApplicationParams` and `AOM_Properties` member `serialize`/`deserialize`
   to free procedures.~~ **Done** in `eda85a7` — see "Follow-up" step 2 above.
2. `MineralFertilizerParameters` still has its constructors/`deserialize`/`serialize`/`merge`/
   `to_json` as member methods (intentionally left for a later pass, see above) — could be
   proceduralized following the same pattern as the other modules if desired.
3. `src/soil/conversion.cpp` and `src/soil/constants.cpp` are still in their original
   (pre-refactor) form now that they live in the main tree — convert if/when needed.
4. Low priority cleanup: rename `Soil::SoilParameters`' four override fields from `_vs_...` to
   `vs_...`, matching the rename already done on `SoilLayer` (they are no longer private, so the
   leading underscore no longer carries meaning). Pure rename, no behavior change — but check the
   `soilparameters::soilRawDensity` / `soilBulkDensity` / `soilOrganicCarbon` / `soilOrganicMatter`
   resolved-value procedures for accidental name collisions while doing it.
5. After each conversion step, run build and fix regressions immediately.

## Validation baseline

Use the project build task equivalent command:

`cmake --build build --parallel`

Notes:
- In this environment a transient `.ninja_lock`/PDB contention can appear; rerun (or serialize with `--parallel 1`) if needed.
- Finish each step with a successful build before moving on.

## Post-build behavior regression check (required)

After a successful build, run the **"Run monica-run"** task from `tasks.json` (currently in `.zed/tasks.json`).

Then compare:

`sim-min-out_section_crop.csv` vs `sim-min-out_section_crop_orig.csv`

The files must be **identical**. Treat any difference as a behavior regression from the refactor and investigate before continuing.
