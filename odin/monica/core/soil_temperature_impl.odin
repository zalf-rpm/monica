// Soil temperature as a swappable process (plan-crop2ml-modules.md §1.1, §1.4).
// No C++ counterpart: MonicaModel holds a SoilTemperature directly there.
//
// MonicaModel.soil_temperature holds one of the implementations below, chosen
// by sim.json's "Modules.SoilTemperature.impl" through the name table. MONICA's
// own implementation (soil_temperature.odin) is the default and runs exactly
// as before: monica_model_general_step's switch calls the same proc with the
// same arguments, so an unconfigured run stays byte-identical (R1).
//
// A new variant is one union entry, one table row, and its own code (R3).
//
// Why this lives in core rather than in monica/modules/soil_temperature/
// (§1.7): the union names Soil_Temperature, which needs Soil_Column, and
// Monica_Model holds the union - so a package outside core would have to
// import core while core imports it. The union, the table and the dispatch
// therefore stay in core; what doesn't depend on core (the Input/Output
// structs and the bindings of generated modules, phase 2) goes to
// monica/modules/soil_temperature/, which core imports.
package core

import clim "../../support/climate"
import d "../../support/date"
import stm "../modules/soil_temperature"
import p "../params"
import "base:runtime"
import "core:fmt"
import "core:mem"

// The zero value is MONICA's own implementation (#no_nil makes it the first
// variant's), which make_monica_model then initialises.
Soil_Temperature_Impl :: union #no_nil {
	Soil_Temperature, // MONICA's own (soil_temperature.odin), "internal"
	^Soil_Temperature_Module, // a module (monica/modules/soil_temperature), integrated
}

Soil_Temperature_Constructor :: #type proc(
	model: ^Monica_Model,
	cpp: ^p.Central_Parameter_Provider,
	allocator := context.allocator,
) -> Soil_Temperature_Impl

// sim.json name -> implementation. "" (no "Modules" section) means "internal".
// Every module of monica/modules/soil_temperature's table is an impl too,
// integrated, without a row here.
@(rodata)
soil_temperature_impls := [?]struct {
	name: string,
	make: Soil_Temperature_Constructor,
} {
	{p.MODULE_IMPL_INTERNAL, make_internal_soil_temperature},
}

@(private = "file")
make_internal_soil_temperature :: proc(
	model: ^Monica_Model,
	cpp: ^p.Central_Parameter_Provider,
	allocator := context.allocator,
) -> Soil_Temperature_Impl {
	return make_soil_temperature(
		&model.soil_column,
		cpp.soil_temperature_mod_params,
		model.env_params.time_step,
	)
}

// The module named by sim.json's Modules.SoilTemperature.impl (a constructor
// can't capture the name).
@(private = "file")
make_integrated_soil_temperature :: proc(
	model: ^Monica_Model,
	cpp: ^p.Central_Parameter_Provider,
	allocator := context.allocator,
) -> Soil_Temperature_Impl {
	return make_soil_temperature_module(model, cpp, model.sim_params.modules.soil_temperature.impl, allocator)
}

soil_temperature_constructor :: proc(name: string) -> (Soil_Temperature_Constructor, bool) {
	name := name == "" ? p.MODULE_IMPL_INTERNAL : name
	for e in soil_temperature_impls {
		if e.name == name {
			return e.make, true
		}
	}
	if _, ok := stm.find_module(name); ok {
		return make_integrated_soil_temperature, true
	}
	return nil, false
}

// Checks the names in sim.json's "Modules" section against the name tables;
// returns the problems, empty if the configuration can run. run_monica calls
// this before make_monica_model, which assumes a valid configuration.
check_modules_config :: proc(mc: ^p.Modules_Config, allocator := context.allocator) -> [dynamic]string {
	errs := make([dynamic]string, allocator)
	if _, ok := soil_temperature_constructor(mc.soil_temperature.impl); !ok {
		append(
			&errs,
			fmt.aprintf(
				"Error: Modules.SoilTemperature: unknown impl %q (known: %v)",
				mc.soil_temperature.impl,
				soil_temperature_impl_names(context.temp_allocator),
				allocator = allocator,
			),
		)
	}
	for name, i in mc.soil_temperature_drivers {
		if _, ok := stm.find_module(name); !ok {
			append(
				&errs,
				fmt.aprintf(
					"Error: Modules.SoilTemperatureDrivers: unknown module %q (known: %v)",
					name,
					soil_temperature_module_names(context.temp_allocator),
					allocator = allocator,
				),
			)
		}
		for other in mc.soil_temperature_drivers[:i] {
			if other == name {
				append(
					&errs,
					fmt.aprintf(
						"Error: Modules.SoilTemperatureDrivers: %q is listed twice",
						name,
						allocator = allocator,
					),
				)
				break
			}
		}
	}
	return errs
}

@(private = "file")
soil_temperature_module_names :: proc(allocator := context.allocator) -> []string {
	names := make([]string, len(stm.modules), allocator)
	for e, i in stm.modules {
		names[i] = e.name
	}
	return names
}

@(private = "file")
soil_temperature_impl_names :: proc(allocator := context.allocator) -> []string {
	names := make([dynamic]string, 0, len(soil_temperature_impls) + len(stm.modules), allocator)
	for e in soil_temperature_impls {
		append(&names, e.name)
	}
	for e in stm.modules {
		append(&names, e.name)
	}
	return names[:]
}

// The part of a soil temperature module's Input that stays constant over a
// run, from the model as MONICA's own soil temperature sees it when it is
// made: make_monica_model makes the modules there, and make_soil_organic
// changes the organic matter right after (plus the soil moisture, which the
// step updates). tav/tamp are the climate
// data's (clim.data_accessor_calc_tamp_and_tav) and left to the caller, as
// are the daily values. Slices are allocated with `allocator`.
soil_temperature_input_from_model :: proc(
	model: ^Monica_Model,
	cpp: ^p.Central_Parameter_Provider,
	in_: ^stm.Input,
	allocator := context.allocator,
) {
	in_.latitude = model.site_params.latitude
	in_.albedo = model.env_params.albedo
	in_.time_step = model.env_params.time_step
	in_.kc_factor_bare_soil = cpp.soil_moisture_mod_params.kc_factor
	in_.params = cpp.soil_temperature_mod_params
	n := len(model.soil_column.layers)
	in_.layer_thickness = make([]f64, n, allocator)
	in_.bulk_density = make([]f64, n, allocator)
	in_.saturation = make([]f64, n, allocator)
	in_.organic_matter = make([]f64, n, allocator)
	in_.organic_carbon = make([]f64, n, allocator)
	in_.sand = make([]f64, n, allocator)
	in_.clay = make([]f64, n, allocator)
	in_.silt = make([]f64, n, allocator)
	in_.stones = make([]f64, n, allocator)
	in_.field_capacity = make([]f64, n, allocator)
	in_.permanent_wilting_point = make([]f64, n, allocator)
	in_.soil_moisture = make([]f64, n, allocator)
	for &l, i in model.soil_column.layers {
		in_.layer_thickness[i] = l.layer_thickness_m
		in_.bulk_density[i] = soil_bulk_density(&l)
		in_.saturation[i] = l.saturation
		in_.organic_matter[i] = soil_organic_matter(&l)
		in_.organic_carbon[i] = soil_organic_carbon(&l)
		in_.sand[i] = l.soil_sand_content
		in_.clay[i] = l.soil_clay_content
		in_.silt[i] = soil_silt_content(&l)
		in_.stones[i] = l.soil_stone_content
		in_.field_capacity[i] = l.field_capacity
		in_.permanent_wilting_point[i] = l.permanent_wilting_point
		in_.soil_moisture[i] = l.soil_moisture_m3
	}
}

Soil_Temperature_Daily :: stm.Daily

// The day's values for the modules, as MONICA has them when its own soil
// temperature steps (monica_model_general_step): before the day's soil
// moisture step, so evaporation, ET0 and the snow are the previous day's, as
// in the C++ AMEI adapters.
soil_temperature_daily :: proc(
	model: ^Monica_Model,
	julian_day: int,
	climate: map[clim.ACD]f64,
	soil_coverage: f64,
) -> (
	day: stm.Daily,
) {
	day.julian_day = julian_day
	day.tmin = climate[.tmin]
	day.tavg = climate[.tavg]
	day.tmax = climate[.tmax]
	day.globrad = climate[.globrad]
	day.precip = climate[.precip]
	day.wind = climate[.wind]
	day.air_pressure = climate[.airpress] or_else 1010 // as the C++ AMEI adapter
	if dl, ok := climate[.daylength]; ok {
		day.day_length = dl
	} else {
		day.day_length = d.day_lengths(model.site_params.latitude, f64(julian_day)).astronomic_day_length
	}
	day.irrigation = model.daily_sum_irrigation_water
	sm := &model.soil_moisture
	day.et0 = sm.reference_evapotranspiration
	day.kc_factor = sm.kc_factor
	day.actual_evaporation = sm.actual_evaporation
	day.soil_coverage = soil_coverage
	day.snow_depth = sm.snow_component.snow_depth
	day.snow_water_equivalent = sm.snow_component.frozen_water_in_snow + sm.snow_component.liquid_water_in_snow
	day.temperature_under_snow = sm.frost_component.temperature_under_snow
	if cm := model.current_crop_module; cm != nil {
		day.above_ground_biomass = cm.aboveground_biomass
		day.crop_height = cm.crop_height
		day.lai = cm.leaf_area_index
	}
	return
}

// ---------------------------------------------------------------------------
// Modules in MONICA (plan-crop2ml-modules.md §1.4). Integrated: one replaces
// MONICA's own soil temperature (Soil_Temperature_Impl) and its results go
// into the soil column. As drivers: they run alongside MONICA's own (or the
// integrated one), on the same values, and are only reported.
// ---------------------------------------------------------------------------

Soil_Temperature_Module :: struct {
	name:        string,
	module:      stm.Module,
	input:       stm.Input,
	output:      stm.Output,
	initialized: bool, // on the first step, which the modules' init reads
}

// Monica_Model.modules. The drivers are reachable by output path, e.g.
// modules.soil_temperature_drivers.DSSAT_ST_standalone.output.layer_temperature
// (§1.5); a module that isn't configured is missing, not a crash.
Modules :: struct {
	soil_temperature_drivers:  map[string]^Soil_Temperature_Module,
	// the same drivers in the order of sim.json, for a deterministic step
	soil_temperature_stepping: [dynamic]^Soil_Temperature_Module,
	allocator:                 runtime.Allocator, // the model's; nil: no modules
	// The modules' temporaries during a step, reset after it. Not the
	// model's allocator: under a server's per-call arena every temporary
	// would stay until the end of the run. Steps must not keep anything
	// allocated here (the bindings' state arrays are sized in init, with the
	// model's allocator, and resizing keeps that).
	scratch:                   mem.Dynamic_Arena,
}

@(private = "file")
modules_init :: proc(ms: ^Modules, allocator: runtime.Allocator) {
	if ms.allocator.procedure == nil {
		ms.allocator = allocator
		mem.dynamic_arena_init(&ms.scratch, allocator, allocator)
	}
}

// For the integrated module and the drivers. tav/tamp come later, from
// run_monica (soil_temperature_modules_set_tav_tamp).
make_soil_temperature_module :: proc(
	model: ^Monica_Model,
	cpp: ^p.Central_Parameter_Provider,
	name: string,
	allocator := context.allocator,
) -> ^Soil_Temperature_Module {
	entry, ok := stm.find_module(name)
	assert(ok, "unknown soil temperature module - check_modules_config wasn't called")
	modules_init(&model.modules, allocator)
	m := new(Soil_Temperature_Module, allocator)
	m.name = name
	m.module = entry.make(allocator)
	soil_temperature_input_from_model(model, cpp, &m.input, allocator)
	m.output = stm.make_output(len(m.input.layer_thickness), allocator)
	return m
}

// Called by make_monica_model; does nothing without configured drivers.
make_soil_temperature_drivers :: proc(
	model: ^Monica_Model,
	cpp: ^p.Central_Parameter_Provider,
	allocator := context.allocator,
) {
	names := model.sim_params.modules.soil_temperature_drivers
	if len(names) == 0 {
		return
	}
	ms := &model.modules
	ms.soil_temperature_drivers = make(map[string]^Soil_Temperature_Module, len(names), allocator)
	ms.soil_temperature_stepping = make([dynamic]^Soil_Temperature_Module, 0, len(names), allocator)
	for name in names {
		d := make_soil_temperature_module(model, cpp, name, allocator)
		ms.soil_temperature_drivers[name] = d
		append(&ms.soil_temperature_stepping, d)
	}
}

// Whether any module runs - the integrated one or drivers.
soil_temperature_modules_configured :: proc(model: ^Monica_Model) -> bool {
	return model.modules.allocator.procedure != nil
}

// The climate's tav/tamp (clim.data_accessor_calc_tamp_and_tav), as the C++
// AMEI branch's run-monica.cpp hands them to MonicaModel::setTAMPandTAV.
soil_temperature_modules_set_tav_tamp :: proc(model: ^Monica_Model, tav, tamp: f64) {
	if m, ok := model.soil_temperature.(^Soil_Temperature_Module); ok {
		m.input.tav, m.input.tamp = tav, tamp
	}
	for d in model.modules.soil_temperature_stepping {
		d.input.tav, d.input.tamp = tav, tamp
	}
}

// One day of a module, on the day's values (soil_temperature_daily) and the
// soil column's moisture.
soil_temperature_module_step :: proc(model: ^Monica_Model, m: ^Soil_Temperature_Module, day: ^stm.Daily) {
	ms := &model.modules
	in_ := &m.input
	in_.daily = day^
	for &l, i in model.soil_column.layers {
		in_.soil_moisture[i] = l.soil_moisture_m3
	}
	if !m.initialized {
		err := stm.module_init(m.module, in_, ms.allocator)
		assert(err == "", err) // only fails without layers, which run_monica refuses
		m.initialized = true
		// the first step with the model's allocator: a module that sizes state
		// arrays there (ApsimCampbell) must not get them from the scratch
		stm.module_step(m.module, in_, &m.output, ms.allocator)
		return
	}
	stm.module_step(m.module, in_, &m.output, mem.dynamic_arena_allocator(&ms.scratch))
	mem.dynamic_arena_reset(&ms.scratch)
}

// Integrated: the module's results become the soil column's, which is where
// the rest of MONICA reads soil temperatures (§1.2), as MONICA's own step
// writes them.
soil_temperature_module_scatter :: proc(model: ^Monica_Model, m: ^Soil_Temperature_Module) {
	model.soil_column.soil_surface_temperature = m.output.soil_surface_temperature
	for &l, i in model.soil_column.layers {
		l.soil_temperature = m.output.layer_temperature[i]
	}
}
