# MONICA performance review (C++)

Status: analysis only, nothing changed yet (2026-11-03). Findings come from reading
`src/` (plus the parts of `mas_cpp_misc` they call); the rankings are estimates, not
profiling results. The bugs (section A) and the cheapest claims in section B were
checked against the code by hand.

## Ground rules

- **All math goes through `cr::`** (`mas_cpp_misc/cr_math/cr_math.h`): correctly
  rounded CORE-MATH functions, called out of line - the compiler can't inline or
  constant-fold them, so `cr::pow(10.0, -3)` costs a full `pow` every time. The soil
  modules alone make roughly 200-350 of these calls per day, many with arguments that
  never change during a run.
- **Bit-identical replacements** (both sides correctly rounded):
  `pow(x, 2)` -> `x*x`, `pow(x, 0.5)` -> `sqrt(x)` (x >= +0), `pow(10, -3)` -> `1e-3`,
  `pow(c, b)` with `b` in {0, 1} -> ternary. **Not** identical: `pow(x, 3)`,
  `pow(x, 4)`, `pow(x, 1.5)` as multiplications.
- Caching a value that only depends on parameters is bit-identical as long as the same
  expression is evaluated. Running sums are only identical if the summation order stays
  the same.
- Check every change with `pixi run test` and `pixi run compare` against results from
  before the change.

## Baseline measurement

Release build (`pixi run build`, Ninja + MSVC, Windows, 8 cores), `monica-run` on
`installer/Hohenfinow2/sim.json` with daily CSV output (2.9 MB), median of 7 runs:

| Simulated period | Time |
|---|---|
| 31 days | 165 ms |
| 3 years | 484 ms |
| 7 years (whole climate file) | 1003 ms |

- About 120-150 ms fixed cost per run (process start, parameter and climate parsing).
- About 0.3 ms per simulated day with `sim.json`'s outputs.
- Very noisy on that machine: five identical runs went from 630 to 932 ms. Differences
  below about +-30% can't be measured this way - **profile before optimizing**
  (`perf record -g` on Linux, or Visual Studio's Performance Profiler / CPU Usage on
  Windows; the Ninja builds have `/Zi` symbols).

**Most of that time is output, not the model.** The same 7 years with fewer outputs
(`sim.json` with its `"events"` replaced, median of 5 runs):

| Outputs | Time |
|---|---|
| none | 171 ms |
| daily `Date` | 188 ms |
| daily `Date`, `AbBiom`, `Mois` | 222 ms |
| `sim.json`'s ~178 entries, many daily and per layer | 668-1003 ms |

So the model step itself costs roughly 50 ms for 7 years (~20 us per day); with many
daily outputs, collecting and writing them is about 75% of the run time. For runs like
this, **B13-B17 matter far more than the model-internal items**, and the per-run setup
(B18-B20) is the next largest share. The model items (B1-B12) only become relevant
for runs with few outputs, e.g. yearly or crop-level outputs in grid simulations - and
even there the fixed setup cost per run is larger than the 7-year model step.

## A. Bugs found along the way (fixing them changes results)

1. **NH3 volatilisation is never subtracted from NH4, `vo_DaysAfterApplication` never
   advances.** `src/core/soilorganic.cpp:673` (`auto layer0 = soilColumn.at(0);`) and
   `:1162` (`auto lay0 = ...`) copy the top layer instead of referencing it:
   - `layer0.vs_SoilNH4 -= vo_NH3_Volatilising` (`:700`) only changes the copy - the
     volatilised N is reported, but not removed.
   - `AOM_Pool` (`:1170`) refers into the copy, so `props.vo_DaysAfterApplication++`
     (`:1242`) is lost.
   - Already a copy before the 2020 refactoring (`auto layer0 = soilColumn->at(0);`):
     all calibrations so far were made with this behaviour.
2. **The root density taper is always 1.** `src/core/crop-module.cpp:3657`:
   `(i_Layer - vc_RootingDepth) / (vc_RootingZone - vc_RootingDepth)` divides `size_t`
   by `size_t` - integer division, always 0.
3. **The automatic sowing temperature sum runs from simulation start.**
   `src/run/cultivation-method.cpp:429-433` sums `tavg` above base temperature over the
   whole climate history, so after the first year the condition is practically always
   met. Probably not intended; also a performance problem (B2).

Reported by a review agent, not checked by hand:

4. The static `ref` cache in `src/run/create-env-from-json-config.cpp:108` is keyed
   without its root JSON - in long-running servers a different crop/site JSON with the
   same keys can get stale values.
5. Data races in multi-threaded servers: `Tools::activateDebug` is a process global set
   per run (`run-monica.cpp:605`); the `initialized` flag in `readCapillaryRiseRates`
   (`mas_cpp_misc/soil/soil.cpp:261-267`) is a plain bool read outside the lock.
6. `_fireEvent("cereal-stem-elongation")` (`crop-module.cpp:906`) is called without the
   null check used elsewhere.
7. Hourly FvCB path: `hourlyGlobrads.back()` at h = 0 on an empty vector is UB if
   radiation at h = 0 is above 0 (polar day).

## B. Speed-ups with bit-identical results

### Likely large and cheap

1. **Debug output is formatted when debug is off.** `debug()` returns a dummy stream
   (`mas_cpp_misc/tools/debug.cpp:27`), but every argument is still computed and
   formatted, and `endl` flushes.
   - Worst case `src/core/soilorganic.cpp:318`: `params.toString()` builds and dumps a
     ~20-field JSON object on most crop days (via `fc_MoveDeadRootBiomassToSoil`,
     `crop-module.cpp:1799`), plus a per-layer `debug()` at `:359`.
   - `run-monica.cpp:824`: `currentDate.toString()` every day; about 110 more
     `debug()` calls in `src/` (`crop-module.cpp:1049, 1540, 2581-2592, 3174-3214` on
     the daily path).
   - Fix: a macro that skips argument evaluation, e.g.
     `#define DBG if (!Tools::activateDebug) {} else Tools::debug()`.
2. **Automatic sowing is quadratic in the simulated period.**
   `cultivation-method.cpp:429` walks the whole climate history (one map lookup per
   past day) on every day in the sowing window. Fix: running sum in the workstep - or
   gone with the fix of A3.
3. **Climate data is copied as a `std::map` several times a day and never trimmed.**
   - Copies: `monica-model.cpp:644, 692, 773` (`auto climateData =
     currentStepClimateData();`), `cultivation-method.cpp:397` (`auto currentCd =
     cd.back();`); built daily by `allDataForStep`
     (`mas_cpp_misc/climate/climate-common.cpp:423`).
   - `monica-model.h:181` keeps every day's map: ~36k maps, tens of MB, per 100 years.
   - Fix: `const auto&` plus `find()` (keeping `operator[]`'s 0 default for missing
     keys); `reserve`. Longer term a `std::array<double, ACD_count>` + presence mask per
     day and a bounded ring buffer (only possible after B2/A3: the sowing check reads
     the whole history; also `noOfPreviousDaysSerializedClimateData`).
4. **pF is recomputed about 5x per layer per day.** `SoilLayer::vs_SoilMoisture_pF`
   (`soilcolumn.cpp:151-169`, ~6 `cr` calls) is called from `soilorganic.cpp:645, 856,
   1164, 1274, 1281`; moisture and SOC don't change in between. Fix: compute once per
   layer at the start of `SoilOrganic::step`; cache van Genuchten `n` per layer (sand
   and clay only), only `alpha` (SOC) needs a daily `exp`.

### Allocations per day

5. **About 40 scratch vectors per day in SoilOrganic**: 28 in `fo_MIT`
   (`soilorganic.cpp:746-841`), 7 in `fo_Urea` (`:574-582`), 2 in nitrification
   (`:1258`), 1 in denitrification (`:1345`).
6. **Elsewhere**: one vector per transport sub-step (`soiltransport.cpp:224`), root
   density factors (`crop-module.cpp:3648`), N uptake (`crop-module.cpp:4095-4098`),
   a `std::map<size_t, double>` for dead roots (`crop-module.cpp:1788`).
   Fix for 5 and 6: members, sized once, `std::fill` where a zero start is needed.

### Values recomputed daily that only depend on parameters

7. **Frost**: `frost-component.cpp:80-86` - mean field capacity and bulk density (two
   layer loops), `calcSii` and both heat conductivities (5x `pow`, 2x `exp`), none of
   which change during a run. Compute in the constructor.
8. **Reference ET0 is computed in both the soil and the crop module**, each with about
   13 trig calls depending only on (day of year, latitude), plus `pow(..., 5.26)`
   (altitude only) and `log(67.8 * windHeight - 5.42)`:
   `soilmoisture.cpp:1220-1292`, `crop-module.cpp:1118` (`fc_Radiation`), `:3720`,
   `:3754`; `sin(8°)`, `sin(-6°)` at `crop-module.cpp:1137, 1150` are constants.
   Fix: once per day and shared, or a table per day of year.
9. **pH constants per layer per day**: `soilorganic.cpp:634` (`exp(-Ea/(8.314*310))`,
   a run constant), `:640`, `:675` (`pow(10, -pH)`), `:1433`, `:1814`.
10. **More of the same**:
    - root density `exp` per layer (`crop-module.cpp:3651`) - a table per cultivar,
    - deprivation factor, 2 `log` per evaporation layer (`soilmoisture.cpp:1455-1457`),
    - soil temperature re-factorizes a constant matrix (`soiltemperature.cpp:300-305`;
      `:285-289` is always 0),
    - transport sub-steps recompute gradient and `AD*exp(g*2*5)/g` per layer
      (`soiltransport.cpp:226-230, 312`) - hoist per day, keep the expression order,
    - `referenceTempResponseExpansion`, 2 nested `exp` (`crop-module.cpp:1648, 1654`),
    - stage-sum loops and Kcb `max_element` (`crop-module.cpp:935-989, 1601, 1615`),
    - capillary rise: `std::function` call plus a string map lookup per layer
      (`soilmoisture.cpp:562-563`, only with shallow groundwater),
    - REW from up to ~35 string comparisons per crop day (`soilmoisture.cpp:837-856`),
    - constant field capacity/saturation/wilting point copied daily
      (`soilmoisture.cpp:271-280`).
11. **`pow` calls with exact replacements**: `soiltemperature.cpp:358` (-> `sqrt`),
    `soilorganic.cpp:1192, 1197, 1200, 1205, 1215, 1644` (ternaries, squares, `1.0`),
    `mas_cpp_misc/soil/soil.cpp:975`; `crop-module.cpp:2783`/`2799` compute the same
    `pow(2, x)` twice (5 `pow(2, x)` per day in respiration; a vendored CORE-MATH
    `exp2` would also be identical).
12. **Duplicate light interception `exp`s**: `crop-module.cpp:2268-2290, 2594-2596`.

### Output collection (per output per day)

13. **`OId` copies**: an object with a vtable and 4 strings, copied 3-4x per output per
    day (`run-monica.cpp:374`, `build-output.h:51`, `build-output.cpp:200, 218`), plus
    a `std::function` per layered output. Fix: `const OId&`, template instead of
    `std::function`.
14. **Map lookup per output per day**: `buildOutputTable().ofs.find(oid.id)`
    (`run-monica.cpp:375, 388`). Resolve once in `setupStorage`.
15. **Unused** `string os = spec.origSpec.dump();` per spec per day
    (`run-monica.cpp:458`) - delete.
16. **Results storage**: every value is a heap-allocated json11 node; windows keep all
    daily values until aggregation (`run-monica.cpp:405-420, 490-502`,
    `build-output.cpp:69-93`, `for (auto j : js)` at `:88`); obj-outputs build a map and
    output names per day (`run-monica.cpp:386-391, 433-453`); results are copied, not
    moved, at the end (`run-monica.cpp:994, 1001`, `output.cpp:195-198`).
17. **CSV writer** flushes every row (`endl`, `csv-format.cpp:149, 215`) and copies
    `OId`/Json per cell (`:106-109, 169-172`).
17a. **Rounding calls `pow` per value**: `Tools::round` -> `round_to_digits`
    (`mas_cpp_misc/tools/algorithms.h:398`) computes `cr::pow(10.0, |digits|)` for every
    rounded output value, per layer, per day (`build-output.cpp:209, 227` and the
    `round(..., 2)` outputs). A table of exact powers of ten (0-22) is identical, since
    `cr::pow` is correctly rounded. (Found while reviewing the Odin port, which has the
    same cost.)

### Per run (matters for millions of grid runs)

18. **Unused** `auto str = msg.json.dump();` of the whole env, often including climate
    data, per request (`serve-monica-zmq.cpp:573`) - delete.
19. **Climate data round trip**: CSV -> DataAccessor -> JSON (one node per value) ->
    parsed back (`create-env-from-json-config.cpp:449-454`, `run-monica.cpp:107`); the
    servers re-read the CSV files per request (`serve-monica-zmq.cpp:526`,
    `run-monica-capnp.cpp:108-110`). Hand the DataAccessor over directly; cache by file
    in server processes.
20. **`auto sp = supportedPatterns();`** copies a 14-entry map of `std::function`s at
    every visited JSON node (`create-env-from-json-config.cpp:40`; also by-value copies
    at `:57, 81, 93`). `include-from-file` (`:281`) re-reads parameter files every run.

### Small

- `clearEvents` copies a `std::set<std::string>` (`monica-model.cpp:965`).
- `dailyValues[dailyFuncId].push_back` - map lookup per day, unbounded growth
  (`run-monica.cpp:668`).
- `allDynamicWorkstepsFinished` builds a `type()` string per workstep per day
  (`cultivation-method.cpp:1567`).
- Bounds-checked `.at()` in per-layer loops throughout `soilorganic.cpp`.

## C. Hourly FvCB photosynthesis (only with `__enable_hourly_FvCB_photosynthesis__`, off by default)

- About 380 transcendental calls per day depend only on the day but are recomputed
  each hour (`crop-module.cpp:2334-2357`, `algorithms.cpp:283-313`); `push_back`
  without reserve (use `std::array<double, 24>`).
- Hour-invariant FC/WP/SWC sums over the root zone inside the hour loop
  (`crop-module.cpp:2378-2382`).
- Constant `pow(10, -3)` etc., `pow(x, 2)`, repeated `Kc`/`Ko`/`Rd`, crop-constant
  `Vcmax_bernacchi_f(25.0, ...)` in `photosynthesis-FvCB.cpp` (`36, 45, 216, 234-238,
  274-277, 546-547, 564, 575-576, 619`).
- Running sums instead of `accumulate` (`crop-module.cpp:2448-2455`) would **change
  results** (summation order).

## Suggested order

1. Profile a run with many daily outputs, to see which part of output collection
   (B13-B16) and writing (B17) costs most, and a grid-style run (few outputs, server
   mode) for the per-run part.
2. Output path: B13-B17a - by the measurements above the biggest share for typical
   configurations.
3. Per-run setup for grid/server runs: B18-B20.
4. Model-internal, cheap and bit-identical: B1, the `const&` part of B3, B2 (only with
   automatic sowing).
5. Precomputing constants: B4-B12 - small in absolute terms (the whole model step is
   ~20 us per day).
6. The bugs in A, each as its own commit with a deliberate baseline update.
