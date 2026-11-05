# MONICA Odin port: performance review

Status: analysis only, nothing changed yet (2026-11-03). Companion to the review of the
C++ MONICA (`PERFORMANCE.md` in the C++ repo, C++ `master`; finding numbers like "C++ B5"
refer to it). Paths below are relative to `odin/` unless they say otherwise. Findings
come from reading the code plus the measurements below; the leak and copy findings were
checked by hand (no `delete` in `monica/core/soil_organic.odin`, no
`free_all(context.temp_allocator)` outside `cmd/monica-module`).

## Measurements

Windows, 8 cores. `monica-run` on the C++ repo's `installer/Hohenfinow2/sim.json`
(7 years, ~178 output entries, many daily and per layer, 2.9 MB CSV, ~552k cells),
median of 5 runs, all three binaries alternating:

| Binary | Time |
|---|---|
| C++ Release (`pixi run build`, MSVC `/O2`) | 0.67-1.0 s |
| Odin `pixi run build` (no `-o`, i.e. `-o:minimal`) | 6.1 s |
| Odin `pixi run build-release` (`-o:speed`) | 5.6 s |

**All three CSVs are byte-identical** - also the `-o:speed` one (for this simulation).

Where the time goes (C++ Release vs Odin `-o:speed`, same 7 years, `"events"` of
`sim.json` replaced):

| Outputs | C++ | Odin |
|---|---|---|
| none | 171 ms | 148 ms |
| daily `Date` | 188 ms | 148 ms |
| daily `Date`, `AbBiom`, `Mois` | 222 ms | 219 ms |
| `sim.json`'s ~178 entries | 668 ms | 3475 ms |
| 31 days of `sim.json` (setup + a month) | 121 ms | 173 ms |

- **The model step is as fast as in C++**: ~50 ms for 7 years after ~120-150 ms of
  setup, i.e. ~20 us per day. The port's daily simulation is not the problem.
- **The whole gap is in the output path**: with many daily outputs Odin spends ~3.3 s
  on collecting and writing them, C++ ~0.5-0.8 s.
- **CSV formatting explains a good part of it**: a stand-alone benchmark of
  `fmt.wprintf(w, "%.6g", x)` into a 64 KiB `bufio` writer takes 1.78 us per value at
  `-o:speed` and 3.48 us at `-o:minimal`; `strconv.write_float(..., 'g', 6, 64)` takes
  0.20 / 0.82 us. With 552k cells that is ~1.0 s (`-o:speed`) / ~1.9 s (default) for
  `fmt` alone, against ~0.1 s with `strconv`. The rest (~2 s) is collection; a profiler
  would have to break that down (candidates: O6, O7).
- **Memory**: peak working set without outputs is 26 MB for Odin vs 13 MB for C++ -
  about 5 KB more per simulated day, which fits the leaks below (O2, O3); ~180 MB more
  for a 100-year run, and in the servers it adds up across requests. With `sim.json`'s
  outputs: 84 MB vs 61 MB. (Sampled while running, approximate.)

## A. C++ bugs: how the port handles them

The port reproduces the C++ quirks on purpose, to stay value-by-value identical, and
documents them with `NOTE(c++-quirk)`. Fixing any of them has to happen on both sides
together, with a deliberate baseline update.

- **C++ A1-A3: fixed (2026-11-04)**, together with the C++ `master` (`cbaae4cb`, see
  its `QUIRK-FIXES.md`) and the C++ copy in `src/`; `cpp_reference` points at that commit.
  A1: the top soil layer is referenced in `soil_organic_fo_urea` and
  `soil_organic_fo_volatilisation` (no more copy, no `clone_aom_pool`); A2: the root
  taper divides as `f64` (`calc_root_density_factor_and_sum`); A3: the automatic sowing
  temperature sum runs from the sowing window's start (`automatic_sowing_condition`).
  The fixed port is identical to the fixed C++ in both test suites.
- **C++ A4 (`ref` cache keyed without root): applies within a run**
  (`monica/run/create_env.odin:37-60`), not across requests (`reset_caches` per request,
  `serve_zmq.odin:413`, `run_monica_capnp.odin:451`).
- **C++ A5 (races): does not apply.** `activateDebug` isn't ported, the zmq server is
  single-threaded, the capnp server runs models under `rm.lock`
  (`run_monica_capnp.odin:445-452`) - no race, but one run at a time per process.
- **C++ A6 (unguarded `cereal-stem-elongation` event): reproduced**, `crop_module.odin:3558-3565`.
- **C++ A7 (`hourlyGlobrads.back()` UB): applies differently** - `crop_module.odin:1639-1649`
  tracks `prev_hgr` starting at 0.0, which is defined and matches C++ wherever C++ isn't UB.

Found only in the port:

- **`avg-soil-temp` automatic sowing would crash**: `as.get_avg_soil_temps()` is a nil
  proc call (`monica/run/worksteps.odin:431-433`; the wiring was dropped, see
  `workstep.odin:106-113`). Not checked by hand.

## O. Odin-specific findings

### O1. Shipped builds are barely optimized (high)

- `tools/odinw.py:591-596` adds `-o:speed` only with `--release`; without `-o` Odin uses
  `-o:minimal`. `../flake.nix:172` and `Dockerfile:66-67` (empty `ODIN_BUILD_FLAGS`)
  build the same way, so the `zalfrpm/monica-odin` image runs `-o:minimal`.
- No `-no-bounds-check` anywhere; every `sc.layers[i]` and `[dynamic]` index in the
  per-layer loops is bounds-checked.
- Why the docs call release builds "not what the comparisons validate"
  (`how-to-build.md`, `odinw.py:590-594`): `-o:speed` was simply never compared. There is
  no known divergence, and it shouldn't change results: no fast-math in Odin, no FMA on
  the generic x86-64 target (CONVENTIONS.md §1), LLVM doesn't reassociate float math
  without fast-math, `cr_math` calls are external. Small remaining risk: `min`/`max` on
  ±0/NaN. **For `sim.json` the `-o:speed` output is byte-identical to C++.**
- Measured gain is only ~10% overall (the time is in the output path, O4), but the
  formatting benchmark is 2x faster with `-o:speed`.
- Change: run the test suites with `pixi run build-release` + `compare`; if identical, make
  `-o:speed` the default for pixi, Nix and Docker. Then try `-no-bounds-check` (can't
  change results). Keep `-microarch` off (FMA).

### O2. `context.temp_allocator` is never reset (high, results unchanged)

- The default temp allocator is a growing arena; `free_all(context.temp_allocator)` only
  appears in `cmd/monica-module/main.odin:182` (and tests) - not in `monica-run`, the zmq
  server or the capnp server.
- Per-day temp allocations: `core/soil_organic.odin:1043-1044, 1128`,
  `core/crop_module.odin:3141-3144` (C++ B6, N uptake), `core/soil_transport.odin:183`
  (per sub-step), `io/build_output.odin:111, 125` (per aggregation),
  `run/run_monica.odin:380, 433` (obj-outputs), `crop_module.odin:3572` (`fmt.tprintf`).
- Roughly 1 KB per day (~35 MB per 100 years); in the servers unbounded across requests
  (one arena per thread for capnp).
- Change: `free_all(context.temp_allocator)` at the end of each day in
  `run/run_monica.odin` (~`:978`) - `monica/trace/trace.odin:276` already assumes this.
  Order matters: after `monica_model_clear_events` (`core/monica_model.odin:320`),
  because `monica_model_fire_event_cb` (`:179`) stores the `tprintf`'d event name as a
  map key without cloning it.

### O3. Heap allocations per day that are never freed (high, results unchanged)

`monica-run` and the model use the plain `context.allocator` (`cmd/monica-run/main.odin:131`
-> `run_monica.odin:949` -> `monica_model.odin:924`), and nothing frees these:

- `core/soil_organic.odin:192-198` (`fo_urea`, 7 arrays) and `:366-414` (`fo_mit`,
  26 arrays): 33 `make([dynamic]f64, nools)` per day, no `delete` in the file
  (C++ B5, worse here: leaked). (The daily `clone_aom_pool` copy went away with the A1 fix.)
- `core/crop_module.odin:2282` (root density factors, daily), `:2326` (dead roots as a
  `map[int]f64`), `soil_organic.odin:1410` (sorted `keys` array for that map).
- `core/monica_model.odin:451-454`: `clear_events` makes a new map daily and drops the
  old one.
- Hourly FvCB (off by default): 6 maps per hour in `voc_guenther.odin:67-68` /
  `voc_jjv.odin:177-178`, 144 per day; `crop_module.odin:1657-1658` drop the previous
  day's accumulator maps.
- In the servers these escape the per-request arena (`serve_zmq.odin:372-375`), because
  a zero-value `[dynamic]`/map takes `context.allocator` on first use. Same for
  `Store_Data.results` / `intermediate_results` (`run/run_monica.odin:354-365`, one
  48-byte `jx.Value` per output per day, e.g. ~16 MB for 30 daily columns x 30 years),
  `oid_get_value` without allocator (`run_monica.odin:363, 379` ->
  `io/build_output.odin:150-151, 543, 560`), `model.climate_data`
  (`run_monica.odin:912`) and the `ref_cache` maps (`run/create_env.odin:37-60`,
  `reset_caches` sets them to nil without freeing).
- Change: scratch arrays as members sized once in the constructors (zeroed where C++ relies
  on `vector(n)` zero-init); dead roots as a dense `[]f64` per layer (ascending order =
  `std::map` order, identical); swap + `clear()` for the event maps; pass the request
  allocator explicitly where results are built. Don't make the arena the ambient
  allocator: `monica/capnp/run_monica_capnp.odin:351-358` records that this corrupted the
  heap.

### O4. Output path: ~5x slower than C++ (high)

- **CSV formatting with `fmt`** (`io/csv_format.odin:139-140`): `fmt.wprintf(w, "%.6g",
  ...)` per cell, ~1.8 us (`-o:speed`) / 3.5 us (default) per value - see the
  measurements. The `precision != 6` branch right below already uses
  `strconv.write_float(..., 'g', precision, 64)` at ~0.2 us. Change: use a `strconv`
  path for 6 as well, **but it must reproduce `%.6g` byte for byte** (exponent format,
  trailing zeros, `-0`) - presumably why `fmt` is used there; validate with the test
  suites, or write a small `%g` formatter on top of `strconv`.
- **Rounding every output value calls `pow`**: `support/tools/algorithms.odin:39`
  (`cr_math.pow(10, digits)`) per value, per layer, per day via `round_or_not`
  (`io/output_paths.odin:489`). A table of exact powers of ten (0-22) is identical,
  since `cr` `pow` is correctly rounded. The C++ `Tools::round` has the same cost
  (`mas_cpp_misc/tools/algorithms.h:398`, now C++ B17a).
- **Per-day allocations in collection** (`io/build_output.odin:150-151`): every
  unaggregated layer output allocates an array per day growing from capacity 0 (give it
  `to-from+1`); every aggregated layer output allocates `vs` and never frees it (temp
  allocator); `Date` allocates twice per day (`:543`, `to_iso_date_string` + clone in
  `jx.s`); obj-outputs build a map and clone a key per output per day
  (`run_monica.odin:377-383`) - compute names once.
- **Values as `jx.Value`**: a 48-byte union per cell (vs. 8 bytes for an `f64` column); a
  bigger, bit-identical refactor.
- Lambda outputs do one int-keyed map lookup per output per day
  (`io/output_paths.odin:393`): store the proc pointer in `OId` when parsing.
- `for oid, i in output_ids` (`run_monica.odin:357`) and `oid := oid_in`
  (`build_output.odin:144`) copy a ~150-byte struct; use `for &oid`. Low.

### O5. Per-day climate map and unbounded history (medium, bit-identical)

- `support/climate/climate_common.odin:261-286` builds a new `map[ACD]f64` per day
  (1-2 heap allocations, ~10 hashed inserts), appended to a history that is never trimmed
  (`run_monica.odin:912`, `core/monica_model.odin:70`); ~15 hashed lookups per day
  (`monica_model.odin:713-805, 868-905`).
- Change: `[ACD]f64` plus `bit_set[ACD]` per day, keeping the "0 if missing" semantics.
  The `climate_data.#last.tavg` output aliases (`io/output_paths.odin:88-217`) use
  reflectpath's map-key step, so reflectpath would need an enum-array step. Bounding the
  history is possible now that A3 is fixed (the sowing check only reads the sowing window).

### O6. `cr_math` calls

`support/cr_math/cr_math.odin:25-37` binds `cr_*` directly as foreign C calls - same cost
as in C++, nothing can be constant-folded. All C++ items about constant-argument math
carry over 1:1 (below).

## C++ findings carried over to the port

Same fixes as in C++, same result identity; doing them on both sides together keeps the
code comparable line by line.

| C++ | Port | Where |
|---|---|---|
| B1 debug output | **does not apply** - `debug()` wasn't ported | only `crop_module.odin:3572` (`tprintf`, O2) |
| B2 sowing sum, quadratic | applies | `run/worksteps.odin:487-494`: keep `(n_done, partial_sum)` in `Automatic_Sowing_Data` (not reset in `reinit`) |
| B3 climate copies | copies **don't apply** (map assignment copies the header); history does | O5 |
| B4 pF ~5x per layer per day | applies | `core/soil_column.odin:160-189`, called at `soil_organic.odin:261, 426, 945, 1053, 1060`; `res.m = 1.0` (`soil/soil_pwp_fc_sat.odin:445-452`) makes `pow(x, 1/m)` at `soil_column.odin:182` an exact identity |
| B5/B6 scratch vectors | applies, leaked | O3 |
| B7 frost constants | applies | `core/frost_component.odin:49-59` |
| B8 ET0 twice, ~13 trig each | applies | `core/soil_moisture.odin:1163-1290` (`pow(...,5.26)` `:1228`, `log` `:1266`, `sin(8°)`/`sin(-6°)` `:1175, 1180`); `crop_module.odin:620-703` (`fc_radiation`, 17 `cr` calls), `:2768`, `:2777`, `:2809`; `pow(T, 4.0)` (`soil_moisture.odin:1282`, `crop_module.odin:2846`) must stay |
| B9 pH constants | applies | `soil_organic.odin:248, 254, 290, 916, 1222`; `fo_temp_on_nitrification` evaluated twice per layer (`:1052, 1059`) |
| B10 per-day constants | applies | deprivation factor `soil_moisture.odin:1396-1401`; soil temperature factorization `soil_temperature.odin:241-247` (`:234-235` always 0); transport `soil_transport.odin:248-269`; root density `exp` `crop_module.odin:2285, 2296` (rebuild in `fc_update_crop_parameters_for_perennial`, `:1180`); `referenceTempResponseExpansion` `:1135-1136, 1145-1146`; stage sums `:1082, 1103, 3621-3661`; REW strings `soil_moisture.odin:788-829` (cheap). Capillary rise: no `std::function`, but a string- plus int-map lookup per layer (`soil/soil_tables.odin:53-63`). FC/SAT/PWP copies **don't apply** |
| B11 exact `pow` replacements | applies | `soil_temperature.odin:290` (`sqrt`), `soil_organic.odin:785, 964, 969, 973, 978, 984`, `soil/soil_pwp_fc_sat.odin:452`; duplicate `pow(2, x)` `crop_module.odin:1931 = 1955` (5 per day: `1897, 1906, 1931, 1955, 1978`); `pow(x, 1.0)` `:862` is `x`; the `pow(c, temperature)`/`pow(c, wind)` factors at `soil_organic.odin:963-977` can move out of the pool loop (same multiplication order). `pow(T, 3.0)` (`soil_organic.odin:227`) must stay |
| B12 light interception | applies once | `crop_module.odin:1576 = 1599` (the intercropping copy wasn't ported) |
| B13 `OId` copies | mostly doesn't apply | no vtable, slices, no `std::function`; see O4 |
| B14 output function lookup | applies differently | path outputs use a plan compiled once (`support/reflectpath/reflectpath.odin:455-617`, cheap); lambda outputs see O4 |
| B15 unused `origSpec.dump()` | doesn't apply (removed, `run_monica.odin:472`) | |
| B16 results storage | applies differently | no heap node per scalar, but see O4; the end-of-run copy doesn't apply |
| B17 CSV `endl` flush | doesn't apply | 64 KiB `bufio`, one flush per section (`csv_format.odin:191`); but see O4 (`fmt`) |
| B18 unused env dump | doesn't apply | |
| B19 climate round trip, re-read per request | applies | `run/create_env_json.odin:174-190` -> `run_monica.odin:158`; `serve_zmq.odin:231-238`, `run_monica_capnp.odin:573-581` |
| B20 pattern map copy | doesn't apply (built once); `include-from-file` re-read per run applies | `run/create_env.odin:90, 282` |
| small: `clearEvents` | applies differently | O3 |
| small: `.at()` | as bounds checks | O1 |
| small: `dailyValues` | not ported | see the crash note in A |
| small: `allDynamicWorkstepsFinished` | doesn't apply (enum compare) | |
| C hourly FvCB | applies 1:1 | day-invariant sums in the hour loop `crop_module.odin:1696-1703`; hourly radiation `:1643-1655` (`[24]f64` instead of temp arrays); `vcmax`/`jmax` twice per hour (`:1820, 1824`); `photosynthesis_fvcb.odin` constant `pow(10, -n)` at `284, 302-303, 346-349, 558-559, 614`, `pow(x, 2)` at `143, 149, 302, 303, 306, 347, 478`; `pow(x, 3)` (`348, 475`) and the ring-buffer sums (`crop_module.odin:1759-1779`) must stay |

## Suggested order

1. **Output path (O4)**: `%.6g` via `strconv` (validated byte for byte), the `round`
   power table, pre-sized/temp arrays in collection. Then profile the remaining ~2 s of
   collection.
2. **Memory (O2, O3)**: per-day `free_all(context.temp_allocator)` (after
   `clear_events`), members instead of leaked per-day arrays, explicit allocators in the
   servers. Matters most for long runs and long-running servers.
3. **Build (O1)**: validate `-o:speed` with the test suites, make it the default for
   pixi, Nix and Docker; then try `-no-bounds-check`.
4. B2 running sum (with automatic sowing), O5 climate storage.
5. The model-internal C++ items (B4, B7-B12) together with the C++ side - small in
   absolute terms, the whole model step is ~20 us per day.
6. Any further quirk fix (A4-A7) only on both sides together, as A1-A3 were.
