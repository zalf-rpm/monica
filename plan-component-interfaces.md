# Plan: declared component interfaces for the Odin MONICA

The step-by-step implementation of the proposals in `odin/architecture.md`
(sections 5, 6, 8, 9, 12). That document explains *why*; this one says *what to
do, in which order, and how each step is checked*. Branch: `component-interfaces`.

Phases A-H restructure without changing a single result. Phase I is a
deliberate model change and gets its own branch.

---

## Rules for every step

- **Bit-identical.** Same floating-point operations in the same order. A step
  that can't stay identical is stopped and discussed, not "fixed" by accepting
  differences.
- **One step, one commit** (sometimes a few, if a step splits naturally),
  after the gate below passes. Push when asked.
- **Outputs don't move.** Output paths (`crop.<field>`, the alias table in
  `io/output_paths.odin`) keep working. `tr.dump` labels keep their C++
  spelling.
- **Small and reversible.** Introduce the new thing next to the old, switch the
  readers one by one, then remove the old thing.
- **Docs follow.** At the end of each phase, update the affected sections of
  `odin/architecture.md` (and `NAME_MAP.md` / `CONVENTIONS.md` if names or
  rules change), and tick the status table below.

### The gate (run from `odin/`)

| # | Check | Command |
|---|---|---|
| 1 | builds, unit tests | `pixi run build && pixi run test` |
| 2 | same bytes as the phase-0 baseline (both fixtures) | `bash tests/diff_outputs.sh` |
| 3 | same bytes as the C++ master (needs `../monica` built, with the uncommitted 6-digit `Mois`) | `bash tests/diff_vs_cpp.sh` |
| 4 | Crop2ML modules unchanged (only for steps touching `soil_temperature_impl`, `evapotranspiration_impl`, `monica/modules`) | `bash tests/check_modules.sh` (Linux / CI) |
| 5 | C++ test suites, value by value | CI on push (`.github/workflows/odin.yml`) |
| 6 | scenarios the fixtures don't cover, byte-identical to a reference binary: automatic irrigation (workstep and model-level), FAO-56 dual Kc, output sections keyed to every crop event, N-min fertilising, hourly FvCB with O3 and VOC, a Transplant workstep, perennials with cuttings | `tests/extra_scenarios.py` (`--save --reference <binary>` once, from the starting commit) |
| 7 | the Odin test drivers (`tests/*_ref`) and `cmd/monica-module` still compile | `tests/compile_drivers.sh` |
| 8 | the Crop2ML modules as drivers and integrated, before/after | `tests/modules_before_after.sh <reference bin dir>` (~30 min per run) |
| 9 | the components' declared inputs; `components.md` up to date (also in CI) | `pixi run check-components`, `tools/component_check.py doc -o components.md --check` |

Per step: 1-3, 6, 7 and 9 in one go with `pixi run bash tests/refactor_gate.sh`
(`BASE=` the baseline directory, `CPP_RUN=` the C++ monica-run). 4 and 8
where they apply. 5 at the latest at the end of each phase. (During phases
A-H these helpers lived uncommitted under `build/tmp_cm`; they were moved to
`odin/tests` afterwards.)

---

## Status

| Phase | Step | What | Status |
|---|---|---|---|
| 0 | 0.1-0.3 | branch, baseline, reference scans | done (80e13d1) |
| A | A1-A5 | crop → soil exchange struct, callbacks as return values | done except A4b (moved to E1): 7ca53e2 .. 22d1a1e |
| B | B1-B3 | previous-day carry-over struct | done: d88401c .. f9e568a |
| C | C1-C3 | crop fields grouped into component sub-structs | done: 0866110, afdcffd, C3 in the docs commit |
| D | D1-D6 | leaf components with input/state structs and tags | done: 48fe4e4 .. 4f16b1c |
| E | E1-E3 | cut the big procedures (roots, O3/VOC, crop ET0) | done (incl. A4b): 6d8c48d .. 2d84e45 |
| F | F1-F6 | medium components, management, parameters | done: e20cc9d .. 272a9d3 (F6 "reference" part not done, see below) |
| G | G1 | share radiation/day length with soil ET0 | done: a7a116f |
| H | H1-H3 | tag and graph tool, generated docs | done: 13f06e3, d1ed0f1 |
| I | I1-I3 | soil mineral N as rate/state (model change, own branch) | I1, I2 done on branch `soil-n-rates`, behind a switch (off by default); I3: divergence and SOC measured, decision with the MONICA team open |

---

## Phase 0: preparation

- **0.1** Branch `component-interfaces` from `simplify_copilot` (done).
- **0.2** Baseline: `bash tests/diff_outputs.sh --save` on the branch's
  starting commit, and confirm that `diff_vs_cpp.sh` passes. Every later step
  is compared against this baseline.
  - Done: saved to `build/out-component-base` (8 CSVs, 3 fixtures). Gate 2 is
    therefore `BASE=build/out-component-base pixi run bash tests/diff_outputs.sh`.
  - C++ (gate 3): `../monica` master is at `cbaae4cb` (A1-A3 fixed),
    `build/monica-run.exe` doesn't exist and the 6-digit `Mois` change is no
    longer in the checkout. `_cmake_release/monica-run.exe` (3 Nov) used via
    `CPP_RUN=`: all sections identical except `Mois_1..3`; the Odin values
    rounded to 3 digits match (5 rounding ties within ±0.0005).
- **0.3** Reference scans, kept under `build/` (not committed), so each step
  can be checked against what the code did before:
  - `field_deps.py fields monica/core/crop_module.odin Crop_Module cm`
  - `field_deps.py lag ... crop_module_step`: the full previous-day list for
    phase B.
  - All `crop_module` reads in the soil files: the reader list for phase A.

## Phase A: crop → soil exchange (architecture.md §9)

Goal: the soil components no longer hold a `^Crop_Module`. This removes the
dependency cycle crop ↔ soil.

- **A1 Define and fill `Crop_To_Soil`.** It is a new struct in `core`, owned
  by `Monica_Model`, reset each day (bare soil = zero values) and filled at the
  end of `crop_module_step`.
  - Per-layer arrays are allocated once and overwritten daily.
  - Nobody reads it yet, so the gate is trivially identical.
  - Done (7ca53e2), with two changes to the plan:
    - Filled at the **start of the general step**, not at the end of the
      crop step: worksteps run before the step, and on a harvest day the crop
      step is skipped while the soil components still read the harvested
      crop's last values (soil transport its N uptake, soil organic its NPP,
      soil moisture its rooting depth; soil moisture treats it as absent
      otherwise). `has_crop` and `being_cleared` carry that.
    - The per-layer arrays are views of the crop's arrays (no copy, no
      allocation), valid for the day.
    - The automatic irrigation trigger, checked in the crop step and by a
      workstep, gets its own small `Irrigation_Crop_Window`.
- **A2 Switch the readers, one file per commit.** Easiest first:
  1. `soil_transport`: `n_uptake_from_layer[]`.
  2. `soil_organic`: NPP.
  3. `soil_column`: automatic irrigation, temperature sum, heat sums.
  4. `soil_temperature_impl`: LAI, height, biomass.
  5. `soil_moisture`: 11 fields, 17 references.

  For each reader, check that its bare-soil default really was what the zero
  value gives.
  - Done: 37f08d8, 189289b, 72bcbfb, 1c0f37c, 0bac111. The readers get the
    struct as a by-value (read-only) parameter; inside soil moisture it is
    passed on to evapotranspiration, dual Kc and capillary rise.
- **A3 Remove the `crop_module` fields** from `Soil_Moisture`,
  `Soil_Organic`, `Soil_Transport` and `Soil_Column`.
  - `evapotranspiration_impl` keeps its crop access until E3, because it calls
    `fc_reference_evapotranspiration` on a crop copy.
  - The model's stress sums keep reading the crop directly: the model owns it.
  - Done (89d9b21); `put_crop`/`remove_crop` went too. The ET modules path
    takes the whole crop from `model.current_crop_module`.
- **A4 Return events and organic matter instead of using callbacks.**
  `crop_module_step` returns `Crop_Day_Results` (events, organic-matter
  additions), and the model applies them.
  - First check, per call, that adding the organic matter *after* the step is
    identical. The crop doesn't read soil organic's pools, but verify it.
    `apply_cutting` is outside the step.
  - **A4a events: done (38618be).** `crop_module_step` returns `Crop_Events`
    (no allocation); the model adds the names to its current events.
  - **A4b organic matter: not bit-identical, moved to E1.** The dead roots go
    to the soil *inside* the step (`fc_crop_dry_matter` →
    `fc_move_dead_root_biomass_to_soil`), and `soil_organic_add_organic_matter`
    also adds the residue's NH4 and NO3 to the layers - which the crop's N
    uptake reads later the same day. 68 of the 71 crop residue files in
    monica-parameters have non-zero `AOM_NH4Content`/`AOM_NO3Content`, so
    applying the dead roots after the step changes the N uptake. Once the root
    system is its own component (E1), the wiring in `crop_module_step` can
    hand the dead roots to the soil between the root system and N uptake -
    same timing, but visible. Until then the `add_organic_matter` callback and
    `g_current_model` stay (only for this).
- **A5 Pass the snow depth as a parameter.** The value for frost kill (from
  the snow component) becomes an explicit input of the crop step, then remove
  the crop's uses of `g_current_model`.
  - Done (22d1a1e): `Snow_For_Crop` (snow depth, frost depth), and a pure
    `temperature_under_snow` the frost kill calls itself.

Done when: no soil file mentions `Crop_Module`, apart from
`evapotranspiration_impl`, and the crop calls nothing through a global.
- Reached, except the organic-matter callback (A4b, see E1).

## Phase B: previous-day values (architecture.md §8)

- **B1 Define `Crop_Carry_Over`** from the 0.3 lag scan: all values read
  before that day's writer runs. Snapshot it at the **start** of
  `crop_module_step` (management between steps must stay visible). Keep it to
  scalars: copy single elements of organ arrays, not the arrays.
- **B2 Switch the readers one by one** (development, canopy procedures,
  photosynthesis, oxygen deficiency, crop N) from `cm.x` to `prev.x`. This
  includes the clamp: `fc_drought_impact_on_fertility` no longer writes
  `transpiration_deficit`; the step uses `max(prev.water_stress, 0)`.
- **B3 Document the daylength/vernalisation ordering quirk** at its line, as a
  deliberate, C++-compatible choice.

Done when: `field_deps.py lag` reports only fields that are in the carry-over.

What was done:
- `Crop_Carry_Over` (`core/crop_carry_over.odin`, d88401c) holds 15 values:
  stage, water/N/frost stress, the daylength and vernalisation factors,
  rooting depth, the leaf growth/senescence increments, ET0, crop height,
  above/belowground biomass and the two N concentrations. More than the
  architecture.md sketch: the scan also found oxygen deficiency reading
  yesterday's stage, and crop N status yesterday's biomass and N
  concentrations.
- The readers get them as parameters (8f26372, d228f39, 22aeca8):
  development, oxygen deficiency, the intercropping height, the green area,
  photosynthesis, drought impact, crop N status, dry matter (and the dead-root
  transfer it calls).
- **The clamp keeps a write.** At the final developmental stage water uptake
  doesn't write `transpiration_deficit`, so the clamped value survives into
  the next day and the model's stress sum. Dropping the write would change
  that, so the clamp moved from `fc_drought_impact_on_fertility` into the step
  as a visible `cm.transpiration_deficit = water_stress`.
- Remaining in `field_deps.py lag`: each part's own counters and state
  (cutting delay, transplant days, dormancy date, stem elongation flag, step
  count) and photosynthesis reading growth's whole `organ_green_biomass` -
  an array, deliberately not copied (see the struct's comment).
- Checked additionally on two perennial scenarios (clover-grass ley, alfalfa,
  1991-1996 with cuttings: perennial reset and cutting), added to
  `extra_sims.py`.

## Phase C: group the crop's fields (architecture.md §6 step 1)

- **C1 Teach `reflectpath` to look through `using` fields**, so
  `crop.leaf_area_index` resolves when the field lives in an embedded
  sub-struct. Add a unit test next to the existing reflection tests. This is
  the precondition: without it, C2 breaks output paths.
- **C2 Split `Crop_Module`** into the sub-structs of §5, embedded with
  `using`: `Phenology`, `Canopy_State`, `Photosynthesis`, `Stress`,
  `Crop_N_Status`, `Growth`, `Root_System`, `Water_Uptake`, `N_Uptake`, and so
  on. Move fields only; no code change outside the struct definitions.
- **C3 Field ownership table.** In `architecture.md`, list which sub-struct
  owns each field, and which procedures still write fields of another
  sub-struct. This is the to-do list for D-F.

What was done:
- C1 (0866110): `reflectpath` resolves fields of `using`-embedded structs by
  their own name (offsets added at compile time, still one step); the trace
  walker lists them flat. New unit test.
- C2 (afdcffd): 16 sub-structs (`Crop_Radiation`, `Crop_Phenology`,
  `Crop_Canopy`, `Crop_Photosynthesis`, `Crop_O3`, `Crop_Voc`, `Crop_Stress`,
  `Crop_N_Status`, `Crop_Growth`, `Crop_Root_System`, `Crop_Et0`,
  `Crop_Water_Uptake`, `Crop_N_Uptake`, `Crop_Carbon`, `Crop_Management`,
  `Crop_Intercropping`), named `Crop_*` rather than `*_State` so the names stay
  free for the components' own state structs in D-F. Compound literals (the
  defaults) work through `using` unchanged.
- C3: architecture.md, "Field ownership after step 1" in section 6,
  generated from `field_deps.py writers` (`build/tmp_cm/c3_ownership.py`).

## Phase D: leaf components with declared inputs (architecture.md §12)

Pattern for each component:
- an `X_In` struct with tagged fields (`unit`, `src`, `lag`, `cat`);
- a `x_step(params, ^X_State, X_In[, other components' state by value])`;
- one wiring block in `crop_module_step`.

Order (easy first), each one commit:

- **D1 Radiation and day length.** A pure function; it returns a `Radiation`
  value instead of writing fields.
- **D2 Daylength and vernalisation factors.**
- **D3 Kc, Kcb, soil coverage, crop size and green area.** This is today's
  "Canopy" grouping. It is the slot a future canopy model plugs into (see the
  note at the end).
- **D4 Stress factors**: heat, frost (snow as input), oxygen deficiency,
  drought impact on fertility.
- **D5 Crop N status.**
- **D6 Review**: are the tag names and the `In`/`State` convention right? Fix
  them in `CONVENTIONS.md` before the bigger components.

What was done:
- D1 (48fe4e4) `crop_radiation(Radiation_In) -> Crop_Radiation`, pure.
- D2 (2ee4ef9) `crop_daylength_factor`, `crop_vernalisation_factor`, pure;
  the vernalisation-fix lookup became an input.
- D3 (c34f08d) `core/crop_canopy.odin`: `canopy_kc_factor`,
  `canopy_kcb_factor` (the dual-Kc block from the step), `canopy_growth`
  (size, green area, coverage). The intercropping exchange between size and
  green area is an empty stub in Odin, so the three could become one call.
- D4 (5fafabe) `core/crop_stress.odin`: oxygen, heat, frost, drought impact.
  Made visible: frost kill reads the snow depth twice (soil moisture's snow
  component and the soil column's `snow_depth_mm`); drought impact reads
  growth's storage organ.
- D5 (23a1add) `core/crop_n_status.odin`: `n_status_step`.
- D6: `CONVENTIONS.md` §9b (names, inputs as values, tags and their
  vocabulary, verbatim moves); `tests/component_inputs_test.odin` checks the
  tags; the architecture.md §12 sketch corrected (`in` is a keyword).
- Output units: the radiation fields got unit tags, which only shows in the
  units row of outputs that name them by path (none of the legacy ids do).

## Phase E: cut the big procedures (architecture.md §4, §6 step 4)

- **E1 Root system out of `fc_crop_dry_matter`**: rooting depth and zone, root
  density, diameter and length, and the dead roots going to the soil.
  - Then A4b: the root system returns the dead roots, and the wiring hands
    them to soil organic before N uptake runs (same timing as today); the
    `add_organic_matter` callback and `g_current_model` go. `apply_cutting`
    (management, outside the step) returns its residues to the caller.
- **E2 O3 impact and VOC emissions out of `fc_crop_photosynthesis`.** VOC
  becomes an output-only component (driver candidate).
- **E3 Crop reference ET as a component.** `evapotranspiration_impl` uses it
  instead of copying the whole crop (`cmc := cm^`). This removes the last soil
  → crop access from phase A.

What was done:
- E1a (6d8c48d) `core/crop_root_system.odin`: `root_system_step`. The C++
  fcCropDryMatter is now three calls in the step: growth
  (`fc_crop_dry_matter`, returns the day's dead root biomass), the root
  system, and `fc_crop_n_demand_by_root_length` (the N demand capped by the
  root length the root system just computed).
- E1b = A4b (4fc5386): the step hands the dead roots to soil organic right
  after the root system, as the callback did; `crop_module_step` takes the
  soil organic as a parameter. Harvest and cutting return their roots and
  residues to the caller. The `add_organic_matter` callback and
  `g_current_model` are gone - the crop module no longer reaches into the
  model.
- E2 (5d9505f) `core/crop_o3.odin`, `core/crop_voc.odin`. O3 stays an hourly
  step inside photosynthesis' FvCB loop (its damage lowers the next hour's
  vcmax - a real hourly coupling); VOC runs after the loop on the 24 stored
  hours (only reported). Checked with a new extra scenario that enables the
  hourly FvCB path with O3 - no fixture does. Pre-existing, not changed: the
  JJV VOC emissions are NaN in that scenario (with the base binary too).
- E3 (2d84e45) `core/crop_et0.odin`: `crop_et0` is a pure function returning
  ET0 and its terms (incl. the stomata resistance); the ET modules call it
  instead of copying the crop. `evapotranspiration_impl` still reads the crop
  for the modules' inputs (transpiration, carboxylation pathway, ...) - via
  the model's current crop, read-only.

## Phase F: medium components, management, parameters

- **F1 Phenology**: stage, temperature sums, vernalisation, dormancy, events.
- **F2 Growth and partitioning.**
- **F3 Water uptake**, **F4 N uptake**, **F5 Photosynthesis.** Each gets
  `In`/`State`, and reads of other components become by-value parameters.
- **F6 Management as commands applied by each owner.**
  `force_transplant_state`, `apply_cutting` and `set_stage` become
  `x_apply(...)` per component.
  - Immutable parameters: cutting stores a modifier instead of changing
    `max_assimilation_rate`, and the perennial reset swaps a parameter
    reference.
  - Only if the multiplication order can be kept. Otherwise, document the
    current behaviour and leave it.

Done when: no step procedure takes `^Crop_Module`. `crop_module_step` is
wiring only.

What was done:
- F1 (e20cc9d) `core/crop_phenology.odin`: `phenology_start_dormancy`,
  `phenology_development`, `phenology_reset_perennial`, `phenology_events`,
  plus the factor functions. The perennial reset is split where it replaces
  the crop parameters: development returns that a new cycle starts, the step
  swaps the parameters and calls `phenology_reset_perennial` with the new
  ones.
- F2 (db70653) `core/crop_growth.odin`: `growth_step` (returns the dead root
  biomass, the updated N content - N uptake's - and the end of a perennial's
  growth cycle - phenology's) and `growth_limit_n_demand`.
- F3 (3b5b4a0) `core/crop_water_uptake.odin`, F4 (a6ad909)
  `core/crop_n_uptake.odin`, F5 (e65d2a9) `core/crop_photosynthesis.odin`
  (gets the O3 and VOC states: it steps both), 48736f2 `core/crop_carbon.odin`
  and `core/crop_radiation.odin`.
- F6 (8c11707, 7dcebc3, 80184bc): `force_transplant_state` and
  `apply_cutting` (`core/crop_management.odin`) dispatch to commands each
  owner applies (`phenology_set_stage`, `phenology_transplant`,
  `canopy_transplant`, `growth_cut`, `canopy_after_cut`, `management_cut`,
  `n_uptake_after_cut`, ...). Immutable parameters: the max assimilation rate
  a cutting reduces is photosynthesis state (the same multiplications in the
  same order) - the crop parameters are no longer written during a run.
  Visible difference: an output path or SetValue on
  `crop_params.cultivar_params.max_assimilation_rate` sees/sets the unchanged
  parameter.
- **Not done: the perennial reset as a parameter reference swap.** With the
  cutting no longer writing parameters, the clone at the reset copies
  unchanged data; replacing `crop_params` (a value) by a pointer to the
  sowing's or the perennial parameters would only change ownership and
  memory, with no visible effect, and touches every `cm.crop_params` use and
  the output paths. Left as it is.
- 272a9d3: the last inline updates (`management_daily_step`,
  `phenology_relative_development`). `crop_module_step` now fills inputs,
  calls the components and does the documented hand-overs: the carry-over,
  the water-stress clamp, the ET0 choice (climate file or `crop_et0`), the
  dead roots to soil organic, the perennial parameter swap.
- Extra scenarios added: a Transplant workstep; cuttings that reduce the max
  assimilation rate (90 %).

## Phase G: remove duplicates (architecture.md §6 step 6)

- **G1 One radiation and day-length function** (from D1), used by the crop and
  by `soil_moisture`'s ET0. First check that the formulas are really identical
  operation by operation.
  - Done (a7a116f). Not all are identical - the effective day length differs
    (the crop clips to 0.01 outside [-1, 1], soil moisture clamped the
    argument) - but soil moisture's ET0 uses only the extraterrestrial
    radiation; the rest were dead stores there (in the C++ too). The
    extraterrestrial radiation is bit-identical: the former code is kept in
    `tests/radiation_shared_test.odin` as the reference, checked for every day
    of the year at latitudes -66..66.

## Phase H: tooling (architecture.md §12)

H1 can start as soon as D1-D2 exist, and is useful as a check from then on.

- **H1 Tag checker** (extend `tools/field_deps.py` or a sibling). It reports:
  - `In` fields a component never reads;
  - same-day `src` edges that run *after* their reader;
  - `lag:"1d"` inputs not coming from the carry-over.
- **H2 Generate the component graph** (mermaid) from the tags, replacing the
  hand-drawn §5 diagram.
- **H3 Generate documentation from the tags**: output units and descriptions,
  and the MCP server's `monica://docs/outputs` (`odin/mcp`). Crop2ML
  descriptions are optional.

What was done:
- H1 (13f06e3) `tools/component_check.py check` (`pixi run check-components`):
  unused input fields, previous-day inputs that are neither the carry-over nor
  state not yet written that day, same-day inputs filled before their source
  runs, fields filled from another component than their `src`. Its first
  findings were fixed: `storage_organ` is a species constant, not growth state
  (moved to the crop's direct fields); the critical oxygen content at
  yesterday's stage and growth's in/out N content are previous-day values.
- H2/H3: `component_check.py graph` (components only, `--all` with weather,
  soil, site and parameters) and `doc` -> `odin/components.md`
  (`pixi run gen-components-doc`; `doc --check` tells whether it is up to
  date): the graph, each component's state with units, its inputs with unit,
  source and previous-day flag. The MCP server serves it as
  `monica://docs/components`.
- Not done: output descriptions and Crop2ML descriptions from tags - the
  fields have no description tags; units and sources are what the tags
  carry. `monica://docs/outputs` still describes the MCP server's own daily
  columns.

## Phase I: soil mineral N as rate/state (model change, separate branch)

Not bit-compatible: it is validated, not diffed.

- **I1** Each N process delivers a demand or rate instead of changing the NH4,
  NO2 and NO3 pools in place: urea hydrolysis, mineralisation, volatilisation,
  nitrification, denitrification, crop uptake, leaching.
- **I2** The owner limits the demands when a pool doesn't suffice, by an
  explicit rule, applies them, and checks the mass balance.
- **I3** Validate against measurements and the current version; decide with
  the MONICA team before merging.

### What was done (branch `soil-n-rates`)

- **A switch, not a replacement.** `Soil_Organic_Module_Parameters.mineral_n_as_rates`
  (JSON `"MineralNAsRates"` in the soil organic parameters; for comparison runs
  `MONICA_MINERAL_N_AS_RATES=1` in monica-run's environment), off by default.
  Off is the C++'s in-place path, unchanged: the refactoring gate passes
  (fixtures, C++, extra scenarios byte-identical), so CI keeps its exact
  check, and one binary runs both variants for comparisons.
- **I1.** Each process procedure (`soil_organic_fo_urea`, `_mit`,
  `_volatilisation`, both nitrification and both denitrification models) takes
  an optional `^Mineral_N_Rates`: nil = in place as before, non-nil = record its
  demand from the unchanged start-of-step state. One implementation of each
  process serves both paths. Deposition and crop uptake come from soil
  transport (`soil_transport_deposition_rate`, the crop's `n_uptake_from_layer`).
  `soil_organic_step` and `soil_transport_step` are split into halves both paths
  use.
- **I2.** `core/soil_mineral_n.odin`: `mineral_n_apply` limits along
  carbamid -> NH4 -> NO2 -> NO3. Available = start content + the same day's
  inflows; committed claims first (immobilisation - its N is booked in the
  microbial biomass by the C deltas; crop uptake - the crop has already added
  it); the soil's own transformations and losses share the rest
  proportionally. Pools stay >= 0, the mass balance is asserted every day
  (< 1e-9 kg N m-3, rounding is ~1e-18). Leaching (transport) runs after, on
  the new state. Unit tests: `tests/soil_mineral_n_test.odin`.
- **Kept on purpose**, so the divergence is the rate/state change alone: the
  processes' own rules (immobilisation split, NH3 caps, denitrification at most
  `transport_rate_coeff` * NO3), including two C++ quirks found on the way -
  after the immobilisation-limited recalculation, `|n_balance|` is *added* to
  NH4 even when it is still negative; the ALFAM volatilisation cap compares
  kg m-3 with kg m-2. Both are questions for the MONICA team on their own.
- **I3, how much it diverges** (`../monica/tests/deviation.py` on the
  hohenfinow2 and muencheberg_v140 suites, off vs on; 55 simulations, all ran):
  - yields: mean change per harvest within ±0.1 % for every crop, single
    harvests -3.8 % ... +3.6 %; above-ground biomass and LAI similar
  - mineral N pools (hohenfinow2 daily means): NH4 +1.0 %, NO2 +1.0 %, NO3
    +0.1 %; NH3 volatilisation +4 % in total (+10 % mean monthly); N2O
    +0.3 %, denitrification -0.5 %, leaching -0.2 % in total
  - SOC: changes of 0.01-0.1 %
  The limits rarely bind; most of the change comes from all processes seeing
  the start-of-day state instead of what the previous process left.
- **I3, against measurements**: `odin/tests/v140_soc_vs_measured.py` compares
  the V140 suite's daily SOC (0-30 cm) with `soil_corg_measured.csv` (1006
  samples). Off and on are the same to the 4th digit (bias +0.117 % SOC, RMSE
  0.191, r 0.51) - SOC can't tell the variants apart. The suite has no N
  measurements (Nmin, N uptake, leaching), which is what would discriminate.
- **Open for the decision**: N measurements to validate against; whether
  committed claims first + proportional sharing is the rule wanted (a fixed
  priority would be the alternative); whether the same-day inflows should
  count as available (pure explicit Euler would not); the two quirks above.

---

## Later / optional

- `Soil_To_Crop` read view instead of the crop's `^Soil_Column`, once the
  crop's components have explicit inputs (end of F).
- The same analysis and split for `soil_moisture.odin` (water balance vs. ET
  vs. snow and frost) and `soil_organic.odin` (architecture.md §7).
- Debug-build mass-balance asserts per component (water, N, C).
- Unit types (`distinct f64`) at component boundaries.

## Note: the planned canopy model

MONICA has no canopy component today. D3 turns the existing canopy-related
procedures into one component with a declared `Canopy_In` and
`Canopy_State`. A new canopy model then implements the same interface:
- its inputs say which phenology, growth and weather values it needs;
- photosynthesis, water uptake and `Crop_To_Soil` read its output.

That makes it swappable against today's procedures (architecture.md §6
step 7). If the canopy work starts before D3, do D3 first (it is small) so the
new model has a target.
