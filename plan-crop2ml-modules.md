# Plan: Crop2ML modules in the Odin MONICA

Goal: run the AMEI Crop2ML modules (the 10 soil temperature models, MONICA's evapotranspiration
models) with the Odin MONICA, in three modes:

1. **standalone**: a module runs from a file of inputs, without MONICA;
2. **MONICA as driver**: MONICA supplies the inputs, the module's results are only reported;
3. **integrated**: the module replaces MONICA's own process and its results feed the rest of
   the model.

Out of scope: changing the internal MONICA processes. With no module selected, the output stays
byte-identical to the C++ reference, as everywhere else in this port.

Related: `odin/CONVENTIONS.md` (rules for Odin code in this tree), `plan-reflective-outputs.md`
(path-based outputs), the C++ AMEI branch (`monica_amei`, branch `amei`, and its
`plan-amei-integration.md`), and in PyCrop2ML `doc/odin-target.md` and `plan-odin-target.md`.

---

## 0. Starting point

### PyCrop2ML's Odin target (done)

`cyml -p <package> odin` generates one Odin package per Crop2ML package, shaped like the cpp2
output the C++ AMEI branch uses:

* `<Comp>_State`, `_Rate`, `_Auxiliary` and `_Exogenous` structs with `destroy_*` procs;
* one struct per ModelUnit (its parameters) with `make_*` (defaults), `destroy_*`, `*_init` and
  `*_calculate_model`;
* a Component struct holding the ModelUnits, with setters for the composition parameters and
  `*_init` / `*_calculate_model` procs.

All 12 AMEI packages pass `odin check -vet`. Run for 30 steps, the results equal those of the
generated Python code (bit-identical, or within 1e-12 for DSSAT_EPICST and ApsimCampbell); the
exception is BiomaSurfacePartonSoilSWATHourlyPartonC, which can't run on any target (package
bug B9 in `plan-odin-target.md`).

### How the C++ AMEI branch does it (for reference, not to copy)

* One hand-written adapter per module, `src/modules/<process>/<Model>_monica_interface.{h,cpp}`,
  deriving from `monica::Run` (`monica-parameters.h:44`, only `virtual void run()`). It owns the
  generated Component plus the State, State1, Rate, Auxiliary and Exogenous structs.
* The module is chosen at runtime by name (`SoilTempModel` / `EvapotranspirationModel` in
  sim.json), through an if/else chain in `monica-model.cpp` (from line 103).
* "MONICA as driver" is a compile-time switch (`AMEI_SENSITIVITY_ANALYSIS`, `CMakeLists.txt:38`).
  In that mode it instantiates all 9 soil temperature modules at once and takes extra inputs
  from `simPs.customData` (AWC, LAI, TAV, …) and spare climate columns (`x1`…`x6`).
* Standalone exists only on the Python/notebook side.
* Outputs are hard-wired ids in `build-output.cpp` (from about line 1609) that dereference the
  adapter without a null check.

What carries over: adapters as the only place that knows both MONICA and a module, and
selection by name. What shouldn't: the compile-time mode, implicit inputs through
customData/`x1…x6`, per-module output ids, and every adapter repeating the same input code.

---

## 1. Design

### 1.1 One interface per process

Per process, one input and one output struct that every module of that process uses. Soil
temperature:

```odin
Soil_Temperature_Input :: struct {
	// weather of the day
	tmin, tmax, tavg, globrad, precip, wind, rel_humid: f64,
	julian_day: int,
	// site (constant over a run)
	latitude, tav, tamp, albedo: f64,
	// per layer, top down: len == number of layers
	layer_thickness:          []f64, // [m]
	bulk_density:             []f64, // [kg m-3]
	sand, clay, silt, organic_carbon: []f64,
	field_capacity, permanent_wilting_point, saturation: []f64,
	soil_moisture:            []f64, // [m3 m-3]
	// cover and snow
	soil_coverage, lai, snow_depth, snow_water_equivalent: f64,
	// water fluxes some modules need
	et0, actual_evaporation, potential_evaporation: f64,
}

Soil_Temperature_Output :: struct {
	soil_surface_temperature: f64,
	layer_temperature:        [dynamic]f64, // per MONICA layer
}
```

The field list is the union of what the 9 C++ adapters read today; trim it while porting the
first two modules. Evapotranspiration gets its own pair.

### 1.2 One binding per module

```odin
Soil_Temperature_Module :: union {
	^Monica_Soil_Temp_Binding,
	^Dssat_St_Binding,
	// ...
}
```

A binding holds the generated Component and its five structs, and has three procs: `init(b,
input)`, `step(b, input, output)`, `destroy(b)`. That is all module-specific code; it maps
units (m → cm, kg m-3 → g cm-3, fractions → %) and layers in one place. Dispatch is a `switch`
over the union, the same pattern as `Workstep_Data` in `monica/run/workstep.odin`. A
`name → constructor` table replaces the if/else chain, so adding a module means writing a
binding and adding one table row.

Use the generated Component's `init`, not individual ModelUnit inits (the C++ adapters call
unit inits; the Odin Component's init also applies renamed inputs, see B3 in
`plan-odin-target.md`).

### 1.3 Modes = who fills the input, who uses the output

| Mode | Input from | Output goes to |
| --- | --- | --- |
| standalone | a CSV/JSON file (weather per day + soil profile + site) | a CSV file |
| MONICA as driver | `monica_model_general_step` (climate, soil column, crop, snow) | reported only |
| integrated | same | written into the soil column (`soil_temperature` per layer, surface temperature); MONICA's own `soil_temperature_step` is skipped |

A runtime setting in sim.json (e.g. `"SoilTempModel": {"name": "DSSAT_ST_standalone",
"mode": "driver"}`), not a compile-time flag. Unlike the C++ AMEI branch, driver mode doesn't
change the rest of MONICA: the internal soil temperature still runs and is what MONICA uses.
Driver mode can run several modules side by side (a list of names), which the C++ sensitivity
build did by instantiating all of them.

The extra driver-mode inputs that come from `customData` and `x1…x6` today become explicit
fields of the input file (standalone) or of a documented `"ModuleInputs"` section in sim.json.

### 1.4 Layers

MONICA's layer count and thicknesses come from `site_parameters` (`number_of_layers`,
`layer_thickness`). The binding sizes the module from them before `init` (the generated code
resizes its arrays in `init` from its size parameters) and maps them:

* 1:1 for DSSAT, Bioma, Stics, Simplace;
* Monica_SoilTemp: `n+2` temperature layers (ground and bottom layer appended), `n+3` for the
  secondary diagonal;
* ApsimCampbell: offset 2 (air and surface node);
* SQ: no layers; the same value for every layer.

Open: what exactly "a flexible amount of layers" means in the AMEI MONICA (only the count, or
also different thicknesses per layer). The bindings only need the per-layer thickness slice
either way, so decide before phase 3 but don't block on it.

### 1.5 Outputs

MonicaModel keeps one `Soil_Temperature_Output` per selected module (always allocated), plus a
pointer to the binding. With `plan-reflective-outputs.md`, any field is reachable by path, e.g.
`soilTemperatureModules.DSSAT_ST_standalone.output.soil_surface_temperature` or the raw
generated state `...DSSAT_ST_standalone.state.st`, so no per-module output ids. An unselected
module's path resolves to "not found" instead of dereferencing nil.

### 1.6 Where the generated code lives

`odin/amei/<package>/`, one directory per Crop2ML package, imported as
`import dssat_st "../../amei/DSSAT_ST_standalone"`. These directories are generated, never
edited. A pixi task regenerates them from the `monica_amei` submodules
(`cyml -p <package> odin`, then copy), and CI checks that regenerating changes nothing. No
per-component build files (the C++ branch's hand-copied component `CMakeLists.txt` files all
carry the same corrupted line).

---

## 2. Changes needed in PyCrop2ML's Odin target first

1. **`cr_math` instead of `core:math`.** `CONVENTIONS.md` requires every transcendental function
   to go through `support/cr_math`. The generator should take the math package as an option
   (e.g. `cyml -p <pkg> odin --odin-math=<import path>`, or a setting in the package): import it
   as `math` and call `math.exp`/`math.pow`/... Gaps: `cr_math` has no `sqrt`, `floor`, `ceil`,
   `round`, `is_nan`, `PI`; those can stay `core:math`, because they're exact or not
   transcendental. Two imports then: `cr_math` for the list in `CONVENTIONS.md`, `core:math` for
   the rest. Also: `math.ln(x)` becomes `cr_math.log(x)`, and `log(x, base)` becomes
   `cr_math.log(x) / cr_math.log(base)`.
2. **Allocator.** Today the generated code uses `context.allocator`. Decide whether `make_*` and
   `init` should take `allocator := context.allocator`, like the procs in this tree. Probably yes,
   for consistency.
3. **Package name.** The package name comes from the directory name (`dssat_st_standalone`).
   That's fine with an import alias; no change needed.

---

## 3. Phases

Each phase ends with a check, as in `plan-odin.md`.

**Phase 0: generator options.** §2.1 and §2.2 in PyCrop2ML. Check: the 12 packages still pass
`odin check -vet` and the PyCrop2ML numeric test, and nothing imports `core:math` for a function
`cr_math` has.

**Phase 1: standalone, two modules.** `Soil_Temperature_Input`/`_Output`, bindings for
Monica_SoilTemp and DSSAT_ST_standalone, and a `cmd/amei-soil-temp` program: input file in,
CSV out. Check: same numbers as the generated Python code run on the same file (the PyCrop2ML
`odin_reference.py` already does this comparison), and Monica_SoilTemp against MONICA's own
`soil_temperature_step`. They are the same algorithm, so it should be bit-identical for equal
inputs, with 0.25 AWC as today.

**Phase 2: MONICA as driver.** sim.json setting, the registry, filling the input in
`monica_model_general_step`, reporting through reflective outputs. Check: with no module
selected, `diff_outputs.sh` stays byte-identical. With modules selected, compare with the C++
AMEI sensitivity build on Hohenfinow2. Expect last-bit differences rather than identity:
the C++ generated code calls `std::exp` etc., not `cr::`.

**Phase 3: integrated.** Write the output into the soil column and skip the internal step. Check:
Monica_SoilTemp integrated equals MONICA's internal soil temperature (same algorithm). This is
the strongest end-to-end test available. Then the other modules run for a full Hohenfinow2
simulation without NaN or bounds-check panics.

**Phase 4: the remaining soil temperature modules, then evapotranspiration.** One binding each.
ET writes into `soil_moisture` like the C++ adapter does (`Monica_Evapotranspiration_...cpp`,
about lines 96–118). That adapter syncs state both ways, and its commit says it is "not yet
working correctly", so port it after the soil temperature modules.

**Phase 5: flexible layers** (see §1.4).

---

## 4. Known issues in the modules (fix in the Crop2ML packages, not in bindings)

* MONICA evapotranspiration, `radiation.pyx` lines 41–45 and `stomata_resistance.pyx` line 5:
  a result stays unassigned with the default input (B10/B11 in `plan-odin-target.md`). In Odin
  it silently becomes 0.0.
* BiomaSurfacePartonSoilSWATHourlyPartonC can't run (B9).
* ApsimCampbell's cpp2 code in `monica_amei` was hand-edited; the Odin code is regenerated
  from the Crop2ML sources, so compare behaviour, not code.
* Monica_SoilTemp's C++ adapter uses a constant soil moisture (`awc = 0.25`), as MONICA's own
  soil temperature does (`soil_temperature.odin:107`), while the other modules get the actual
  moisture per layer. Keep the constant for the phase 3 check (integrated Monica_SoilTemp ==
  internal soil temperature); afterwards decide whether the binding should use actual moisture.
* ApsimCampbell's adapter sets both Eo and Eos to the potential ET (with a `!!!` comment
  questioning it), and Simplace's sets `iRAIN = precip` "so that no snowcover will build up".
  These are adapter decisions; carry them into the bindings explicitly and document them, then
  revisit.
