# Plan

## Refactoring goals (important)

1. Convert `_simple` modules to **plain struct + explicit free procedures**.
2. Keep converted procedures **declared in the corresponding module's `.h` interface**, even if currently only used internally.
3. Prefer direct named procedures over hidden anonymous-namespace orchestration.
4. Optimize for readability/simple structure/modular testability over encapsulation in this phase.
5. Continue stepwise (small safe conversions), preserving behavior and keeping build green.
6. Once a module's struct + free procedures are stable, move the free procedures into their own
   lower-case namespace (e.g. `monica::cropmodule`) so they don't need a type-name prefix, and
   provide `monica::X = x::X` alias so external code referencing the plain type name is unaffected.

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

### `soilorganic` (status: major conversion pass done, namespace move still open)

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
6. Unlike `soilmoisture`/`soiltemperature`/`soiltransport`/`cropmodule`/`soillayer`, this module's
   free procedures still live flat in `monica` (prefixed `soilOrganic...`) rather than in a
   `monica::soilorganic` namespace — that move is still open (see "What to do next").

### `soilmoisture` (status: complete)

1. Member constructors and member `serialize`/`deserialize` are removed from `soilmoisture.h/.cpp`.
2. Procedural initializer added: `initializeFromParams(SoilMoisture* sm)`.
3. `makeSoilMoisture(...)` constructs the plain struct and initializes/deserializes procedurally.
4. `deserialize(...)` / `serialize(...)` contain full logic directly (no forwarding to removed members).
5. All former `SoilMoisture` member getters/setters were removed; direct field access or free
   procedures now cover the former API surface.
6. The free-procedure API lives in `monica::soilmoisture`, with a `monica::SoilMoisture` alias.

### `soiltransport` (status: complete)

1. The flat `soilTransport...` free procedures were moved under `monica::soiltransport` and renamed
   without the prefix.
2. Remaining trivial getters were removed and their call sites were inlined to direct field access.
3. File renamed from `soiltransport_simple.*` to `soiltransport.*`.

### `soiltemperature` (status: complete)

1. The module was moved under `monica::soiltemperature`, with a `monica::SoilTemperature` alias.
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

### `soilcolumn` / `SoilLayer` (status: `SoilLayer` complete, `SoilColumn` not started)

1. `SoilLayer` was converted to a plain struct (data members only) inside a new
   `monica::soillayer` namespace, with `using SoilLayer = soillayer::SoilLayer;` in `monica` so
   every other file referencing the type name needed no change.
2. Its two constructors were replaced by `soillayer::makeSoilLayer(vs_LayerThickness, soilParams)`
   (the unused reader-constructor was dropped; the default constructor is now implicit since the
   struct is an aggregate).
3. Member `deserialize`/`serialize` became free `soillayer::deserialize`/`soillayer::serialize`.
   `SoilColumn::deserialize`/`serialize`, which relied on `setFromComplexCapnpList`/
   `setComplexCapnpList` calling `.deserialize()`/`.serialize()` members, were switched to manual
   loops calling the new free functions.
4. `vs_SoilMoisture_pF()` became `soillayer::soilMoisturePF(const SoilLayer*)`; `get_SoilNmin()`
   became `soillayer::soilNmin(const SoilLayer*)` (both do real computation, unlike the getters below).
5. All trivial getters/setters (pure field passthroughs, including simple one-line forwards to
   `_sps`, e.g. `vs_SoilOrganicCarbon()` -> `_sps.vs_SoilOrganicCarbon()`) were inlined at call sites
   and removed. This touched `frost-component.cpp`, `monica-model.cpp`, `soilmoisture.cpp`,
   `soilorganic.cpp`, `soiltemperature.cpp`, `build-output.cpp`, `cultivation-method.cpp`, and
   `run-monica.cpp`.
6. File renamed from `soilcolumn_simple.*` to `soilcolumn.*` (done in an earlier step, before the
   `SoilLayer` conversion).
7. `SoilColumn` itself (also declared in `soilcolumn.h`) is **not** converted: it still has
   constructors and member methods (`deserialize`, `serialize`, `applyMineralFertiliser`,
   `applyTillage`, `applyIrrigationViaTrigger`, `calculateNumberOfOrganicLayers`, etc.). A layer of
   thin free-function wrappers already exists in `monica` (`soilColumnApplyMineralFertiliser`,
   `soilColumnApplyTillage`, `soilColumnDeserialize`, ...) that just forward to the members — this
   predates the current namespaced-free-procedure pattern and satisfies goal #2 only superficially.
8. `AOM_Properties` (also declared in `soilcolumn.h`) still has member `serialize`/`deserialize`;
   untouched so far, low priority given its small size.

## What to do next (if starting fresh)

1. Convert `SoilColumn` in `src/core/soilcolumn.h/.cpp`, following the pattern used for `SoilLayer`/`CropModule`:
   - Turn it into a plain struct (it currently also inherits from `std::vector<SoilLayer>`, which
     should stay unless it complicates the conversion).
   - Convert the constructors to a `makeSoilColumn(...)`-style factory (already exists as a
     `kj::Own`-returning function alongside the member constructors it still calls internally).
   - Convert `deserialize`/`serialize` and the `apply*`/`calculateNumberOfOrganicLayers`/
     `getLayerNumberForDepth`/`sumSoilTemperature` methods to free procedures.
   - Move the free procedures into a `monica::soilcolumn` namespace with unprefixed names, add a
     `monica::SoilColumn` alias, and remove the now-redundant `soilColumnApply...`/
     `soilColumnDeserialize`/... wrapper layer (replace their few call sites with direct calls into
     the new namespace).
2. Move `soilorganic`'s free procedures from flat `monica::soilOrganic...` into a
   `monica::soilorganic` namespace with unprefixed names (mirrors what was already done for
   `soilmoisture`, `soiltemperature`, `soiltransport`, `cropmodule`, `soillayer`).
3. Optionally convert the small `AOM_Properties` struct's member `serialize`/`deserialize` to free
   procedures for consistency, once `SoilColumn` is done (it's declared in the same header).
4. After each conversion step, run build and fix regressions immediately.

## Validation baseline

Use the project build task equivalent command:

`cmake --build _cmake_debug_ninja --parallel`

Notes:
- In this environment a transient `.ninja_lock`/PDB contention can appear; rerun (or serialize with `--parallel 1`) if needed.
- Finish each step with a successful build before moving on.

## Post-build behavior regression check (required)

After a successful build, run the **"Run monica-run"** task from `tasks.json` (currently in `.zed/tasks.json`).

Then compare:

`sim-min-out_section_crop.csv` vs `sim-min-out_section_crop_orig.csv`

The files must be **identical**. Treat any difference as a behavior regression from the refactor and investigate before continuing.
