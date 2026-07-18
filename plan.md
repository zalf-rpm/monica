# Plan

## Refactoring goals (important)

1. Convert `_simple` modules to **plain struct + explicit free procedures**.
2. Keep converted procedures **declared in the corresponding `_simple.h` interface**, even if currently only used internally.
3. Prefer direct named procedures over hidden anonymous-namespace orchestration.
4. Optimize for readability/simple structure/modular testability over encapsulation in this phase.
5. Continue stepwise (small safe conversions), preserving behavior and keeping build green.

## Current state summary

### `soilorganic_simple` (status: major conversion pass done)

1. Constructors and member `serialize`/`deserialize` are removed; `makeSoilOrganic(...)` + free `soilOrganicSerialize/Deserialize` are used.
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
5. Remaining public getters in `soilorganic_simple.h` intentionally kept where they still provide non-trivial conversions or are still used as interface.

### `soilmoisture_simple` (status: started, constructors/serdes converted)

1. Member constructors and member `serialize`/`deserialize` are removed from `soilmoisture_simple.h/.cpp`.
2. New procedural initializer added:
   - `soilMoistureInitializeFromParams(SoilMoisture* sm)`
3. `makeSoilMoisture(...)` now constructs the plain struct and initializes/deserializes procedurally.
4. `soilMoistureDeserialize(...)` and `soilMoistureSerialize(...)` now contain full logic directly (no forwarding to removed members).
5. Build is green after this conversion.

## What to do next (if starting fresh)

1. Continue in `src/core/soilmoisture_simple.h/.cpp` with the same pattern as soilorganic:
   - convert one method at a time to free procedures,
   - rewire `soilMoistureStep` and call sites,
   - remove obsolete member declarations/definitions.
2. Prioritize high-level methods first (`step` and its internal helper chain), then simple wrappers/getters/setters.
3. After each conversion step, run build and fix regressions immediately.

## Validation baseline

Use the project build task equivalent command:

`cmake --build _cmake_debug_ninja --parallel`

Notes:
- In this environment a transient `.ninja_lock`/PDB contention can appear; rerun (or serialize with `--parallel 1`) if needed.
- Finish each step with a successful build before moving on.
