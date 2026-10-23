# Plan: cleaning up the Crop2ML module integration in the C++ AMEI MONICA

Scope: branch `amei` of this repository, i.e. how the generated Crop2ML modules (the
`SoilTemperatureModels` and `EvapoTranspirationModels` submodules, cpp2 target) are wired into
MONICA through the adapters in `src/modules/`.

The long-term direction is the Odin MONICA (`monica_odin/plan-crop2ml-modules.md`). So this
plan is split: **part A** is worth doing in C++ anyway (bugs, cheap fixes, things that also make
the Odin port easier). **Part B** is the restructuring that only pays off if the C++ AMEI branch
stays in use for a while.

Line numbers as of the survey on 2026-10-22.

---

## Current state

* **Adapters:** one per module, `src/modules/<process>/<Model>_monica_interface.{h,cpp}`, class
  `MonicaInterface : monica::Run` (`src/core/monica-parameters.h:44`). Each owns the generated
  Component and its State, State1, Rate, Auxiliary and Exogenous structs. `init(cpp)` sets
  parameters through the generated setters; `run()` fills inputs from MONICA and writes results
  back.
* **Selection:** `SoilTempModel` / `EvapotranspirationModel` in sim.json (default `"internal"`),
  dispatched by an if/else chain in `MonicaModel::initComponents` (`src/core/monica-model.cpp`,
  from line 103). One `kj::Own<...>` per module in `monica-model.h` (about lines 213–226).
* **Modes:** integrated by default. "MONICA as driver" (sensitivity analysis) is the compile-time
  `AMEI_SENSITIVITY_ANALYSIS` (`CMakeLists.txt:38`, commented out). It instantiates all 9 soil
  temperature modules (`monica-model.cpp` from line 68), and each adapter has about 4
  `#ifdef`/`#ifndef` blocks. There is no C++ standalone mode.
* **Outputs:** hard-wired ids in `src/io/build-output.cpp` (from line 1609) reading
  `monica._instance_<Model>->...`.
* **Build:** `CMakeLists.txt` adds each component's `src/cpp2/<Model>` directory (from line 41).
  Those directories contain hand-copied `CMakeLists.txt` files.

---

## Part A: do now

**A1 Crash when an output of an unselected module is requested.** `build-output.cpp:1611` and
following dereference `_instance_<Model>` (a `kj::Own`) without checking it. If sim.json asks
for `AMEI_Monica_SurfTemp` while another model is selected, MONICA crashes. Fix: return an
empty/missing value when the instance is null. A small helper used by all those lambdas.

**A2 Monica_SoilTemp uses a constant soil moisture in integrated mode.**
`src/modules/soil_temperature/Monica_SoilTemp_monica_interface.cpp:34` sets `awc = 0.25`, and
line 66 derives a constant moisture profile from it once, while DSSAT, Apsim and Bioma refresh
soil water every `run()`. This matches MONICA's own soil temperature module, which also uses a
constant moisture parameter (`soil_temperature.odin:107` in the Odin port), so it's consistent.
But it makes Monica_SoilTemp the only integrated module that doesn't react to the water balance.
Decide which you want; if actual moisture, update `soilMoistureConst` from `soilColumn()` in
`run()`. Not a bug either way, just a decision worth writing down.

**A3 Corrupted line in all component CMakeLists.** 17 copies contain
`elseif(MSVC_VERSION GREATER_Echannel_mainQUAL 1910)` (about line 48); PyCrop2ML's
`example/Monica_SoilTemp/src/cpp/.../CMakeLists.txt:48` has it too, which is probably where they
were copied from. It's in a branch only MSVC takes. Fix: replace them with one CMake function in
the top-level `CMakeLists.txt` (`add_amei_component(<dir> <name>)` that globs the `.cpp` files),
and delete the copies from the generated directories.

**A4 Generated code edited by hand.** `ApsimCampbell/src/cpp2/.../SoilTemperature.cpp` was
reworked by hand (SoilTemperatureModels commit 9126a8f, "tightened generated code a bit"; commit
3ccce6e says "just compiles, not runtime tested"). Regenerating loses that. Either regenerate
and check whether the generated code now works as is, or move the edits back into the Crop2ML
source. Commit d72da22 (moving `noSnowSoilSurfaceTemperature` to auxiliary in
WithSnowSoilSurfaceTemperature) is right: auxiliary is that unit's view of the variable. Why it
was needed with the pre-merge PyCrop2ML (`5fdf390`): the composition's init returned every state
but only declared those a unit init produces, so the `state` version failed with
"noSnowSoilSurfaceTemperature is not defined". Upstream's code (merged 2026-10-22) declares all
states there, so that error is gone either way.

**A4b Unit order changed with the upstream merge. Check before regenerating.** Pre-merge
PyCrop2ML ran the units in topological order of the internal links. Upstream commit 29d0e35
(2026-06-26) changed `topology.py` `algorithm()` (now lines 260–262) to the order of the
`<Model>` elements in the composition XML. `Monica_SoilTemp`'s composition lists `SoilTemperature`
first, so regenerated code runs the profile *before* the surface temperature (with yesterday's
surface temperature), unlike the committed cpp2 code and MONICA's own step. It compiles and runs,
just with different numbers. Fix now: list `NoSnowSoilSurfaceTemperature`,
`WithSnowSoilSurfaceTemperature`, `SoilTemperature` in that order in
`composition.SoilTemperature.xml` (correct with either PyCrop2ML version). Check the other
packages' `<Model>` order too. Report upstream (B12 in PyCrop2ML's `plan-odin-target.md`).

**A5 Model bugs (Crop2ML sources, also affect Odin).**
`EvapoTranspirationModels/Monica/Monica_Evapotranspiration/crop2ml/algo/pyx/radiation.pyx`
lines 41–45 (`sunshine_hours_global_radiation` unassigned when `sunshine_hours <= 0`, default 0)
and `stomata_resistance.pyx` line 5 (`stomata_resistance` unassigned when
`carboxylation_pathway <= 0`, default 0). In C++ these read an uninitialised `double`. Add the
`else` branches and regenerate.

**A6 Use the Component's init.** Adapters call single ModelUnit inits
(`soilTempComp._SoilTemperature.Init(...)`, `Monica_SoilTemp_monica_interface.cpp:105`; Simplace
calls two). That's a workaround for PyCrop2ML bug B3 (the composition's init doesn't pass
renamed inputs). Once B3 is fixed in PyCrop2ML, regenerate and call the Component's `Init`, so
that adapters don't depend on which units have an init.

---

## Part B: restructure (only if the C++ branch stays in use)

**B1 Runtime mode instead of `AMEI_SENSITIVITY_ANALYSIS`.** A sim.json setting per module
(`"mode": "driver" | "integrated"`). Driver mode then only means "don't write the results
back"; MONICA's own processes keep running as in a normal simulation. Today the sensitivity
build drops SoilMoisture, SoilOrganic and SoilTransport and takes the soil from
`initSoilProfileSpec`. If that is needed for the sensitivity analysis, make it a separate
documented setting rather than a side effect of the mode. Removes all `#ifdef` blocks from the
adapters and lets one binary do both.

**B2 One input/output struct per process.** `SoilTemperatureInput` (weather, per-layer soil
properties and moisture, cover, snow, ET terms) filled once by MonicaModel, and
`SoilTemperatureOutput` (surface temperature + per-layer temperature) written back once. The
adapters only map these to and from their module. That removes the repeated input code: the
`PWP + awc*(FC-PWP)` formula is in 5 adapters, and DSSAT_ST and DSSAT_EPICST `init()` are
almost identical.

**B3 Explicit driver-mode inputs.** Replace `simPs.customData` (AWC, LAI, TAV, TAMP, XLAT,
SALB, …) and the reuse of `Climate::x1…x6` (Eo/Es/Eos, ETpot, heat flux, snow) with named
fields of the input struct, read from named climate columns or a documented sim.json section.
Today a climate file must have, for example, the heat flux for SQ in `x5`, which nothing
documents.

**B4 Registry instead of if/else.** `std::map<std::string, std::function<std::unique_ptr<Run>(
MonicaModel*, const CentralParameterProvider&)>>`, filled in one place. MonicaModel keeps
`std::unique_ptr<Run>` per process, not one `kj::Own` per module. Adding a module then touches
the registry and the adapter, instead of CMake (2 places), `monica-model.h` (2),
`monica-model.cpp` (3) and `build-output.cpp`.

**B5 Outputs through the shared output struct.** Generic output ids read
`SoilTemperatureOutput` (always present), e.g. `AMEI_SurfTemp`/`AMEI_SoilTemp`, whichever
module is selected. Module-specific raw values only where really needed. Replaces the
per-module ids, and fixes A1 by construction.

**B6 Serialization.** Adapter state is not part of `MonicaModel::serialize`/`deserialize`
(`monica-model.cpp`, about lines 160–275, handles only the internal `_soilTemperature`), so
save/restore of a simulation with a module selected loses the module state. Only needed if
save/restore is used with modules.

---

## Order

A1, A3, A4b, A5 are small and independent; do them first (A4b before regenerating anything). A2 is a decision, not a fix. A4 and A6 depend on
PyCrop2ML fixes (B3, B4 there). Part B only if the Odin MONICA (`plan-crop2ml-modules.md`) is
further away than a few months. Otherwise skip it: the Odin plan already has this structure.
