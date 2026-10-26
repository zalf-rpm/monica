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

import stm "../modules/soil_temperature"
import p "../params"
import "base:runtime"
import "core:fmt"
import "core:mem"

// NOTE(deviation): no #no_nil yet. The pinned compiler rejects #no_nil on a
// union with a single variant ("must have at least 2 variants"), so until the
// first module joins (phase 2) this is the Maybe-shaped union: its zero value
// is nil instead of MONICA's own implementation. make_monica_model always
// sets it, and reflective output paths unwrap it like any Maybe(T).
Soil_Temperature_Impl :: union {
	Soil_Temperature, // MONICA's own (soil_temperature.odin), "internal"
}

Soil_Temperature_Constructor :: #type proc(
	model: ^Monica_Model,
	cpp: ^p.Central_Parameter_Provider,
	allocator := context.allocator,
) -> Soil_Temperature_Impl

// sim.json name -> implementation. "" (no "Modules" section) means "internal".
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

soil_temperature_constructor :: proc(name: string) -> (Soil_Temperature_Constructor, bool) {
	name := name == "" ? p.MODULE_IMPL_INTERNAL : name
	for e in soil_temperature_impls {
		if e.name == name {
			return e.make, true
		}
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
	names := make([]string, len(soil_temperature_impls), allocator)
	for e, i in soil_temperature_impls {
		names[i] = e.name
	}
	return names
}

// The part of a soil temperature module's Input that stays constant over a
// run, from the model as MONICA's own soil temperature sees it when it is
// made: make_monica_model calls this (through make_soil_temperature_drivers)
// right after it (plus the soil moisture, which the step updates). tav/tamp are the climate
// data's (clim.data_accessor_calc_tamp_and_tav) and left to the caller, as
// are the daily values. Slices are allocated with `allocator`.
soil_temperature_input_from_model :: proc(
	model: ^Monica_Model,
	params: p.Soil_Temperature_Module_Parameters,
	in_: ^stm.Input,
	allocator := context.allocator,
) {
	in_.latitude = model.site_params.latitude
	in_.albedo = model.env_params.albedo
	in_.time_step = model.env_params.time_step
	in_.params = params
	n := len(model.soil_column.layers)
	in_.layer_thickness = make([]f64, n, allocator)
	in_.bulk_density = make([]f64, n, allocator)
	in_.saturation = make([]f64, n, allocator)
	in_.organic_matter = make([]f64, n, allocator)
	in_.field_capacity = make([]f64, n, allocator)
	in_.permanent_wilting_point = make([]f64, n, allocator)
	in_.soil_moisture = make([]f64, n, allocator)
	for &l, i in model.soil_column.layers {
		in_.layer_thickness[i] = l.layer_thickness_m
		in_.bulk_density[i] = soil_bulk_density(&l)
		in_.saturation[i] = l.saturation
		in_.organic_matter[i] = soil_organic_matter(&l)
		in_.field_capacity[i] = l.field_capacity
		in_.permanent_wilting_point[i] = l.permanent_wilting_point
		in_.soil_moisture[i] = l.soil_moisture_m3
	}
}

// ---------------------------------------------------------------------------
// MONICA as driver (plan-crop2ml-modules.md §1.4): modules run alongside
// MONICA's own soil temperature, fed with what MONICA's own gets, and only
// reported - nothing they compute flows back into the model.
// ---------------------------------------------------------------------------

Soil_Temperature_Driver :: struct {
	name:        string,
	module:      stm.Module,
	input:       stm.Input,
	output:      stm.Output,
	initialized: bool, // on the first step, which the modules' init reads
}

// Monica_Model.modules. Reachable by output path, e.g.
// modules.soil_temperature_drivers.DSSAT_ST_standalone.output.layer_temperature
// (§1.5); a module that isn't configured is missing, not a crash.
Modules :: struct {
	soil_temperature_drivers:  map[string]^Soil_Temperature_Driver,
	// the same drivers in the order of sim.json, for a deterministic step
	soil_temperature_stepping: [dynamic]^Soil_Temperature_Driver,
	allocator:                 runtime.Allocator, // the model's
	// The modules' temporaries during a day's step, reset after it. Not
	// the model's allocator: under a server's per-call arena every
	// temporary would stay until the end of the run. Steps must not keep
	// anything allocated here (the bindings' state arrays are sized in
	// init, with the model's allocator, and resizing keeps that).
	scratch:                   mem.Dynamic_Arena,
}

// Called by make_monica_model; does nothing without configured drivers.
// tav/tamp come later, from run_monica (soil_temperature_drivers_set_tav_tamp).
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
	ms.allocator = allocator
	ms.soil_temperature_drivers = make(map[string]^Soil_Temperature_Driver, len(names), allocator)
	ms.soil_temperature_stepping = make([dynamic]^Soil_Temperature_Driver, 0, len(names), allocator)
	mem.dynamic_arena_init(&ms.scratch, allocator, allocator)
	for name in names {
		entry, ok := stm.find_module(name)
		assert(ok, "unknown Modules.SoilTemperatureDrivers entry - check_modules_config wasn't called")
		d := new(Soil_Temperature_Driver, allocator)
		d.name = name
		d.module = entry.make(allocator)
		soil_temperature_input_from_model(model, cpp.soil_temperature_mod_params, &d.input, allocator)
		d.output = stm.make_output(len(d.input.layer_thickness), allocator)
		ms.soil_temperature_drivers[name] = d
		append(&ms.soil_temperature_stepping, d)
	}
}

// The climate's tav/tamp (clim.data_accessor_calc_tamp_and_tav), as the C++
// AMEI branch's run-monica.cpp hands them to MonicaModel::setTAMPandTAV.
soil_temperature_drivers_set_tav_tamp :: proc(model: ^Monica_Model, tav, tamp: f64) {
	for d in model.modules.soil_temperature_stepping {
		d.input.tav = tav
		d.input.tamp = tamp
	}
}

// One day of every driver, with exactly the values MONICA's own soil
// temperature got that day (monica_model_general_step). The soil moisture is
// the column's at that point - before today's soil moisture step, as in the
// C++ AMEI adapters.
soil_temperature_drivers_step :: proc(
	model: ^Monica_Model,
	julian_day: int,
	tmin, tavg, tmax, globrad: f64,
	soil_coverage, snow_depth, temperature_under_snow: f64,
) {
	ms := &model.modules
	scratch := mem.dynamic_arena_allocator(&ms.scratch)
	for d in ms.soil_temperature_stepping {
		in_ := &d.input
		in_.julian_day = julian_day
		in_.tmin, in_.tavg, in_.tmax = tmin, tavg, tmax
		in_.globrad = globrad
		in_.soil_coverage = soil_coverage
		in_.snow_depth = snow_depth
		in_.temperature_under_snow = temperature_under_snow
		for &l, i in model.soil_column.layers {
			in_.soil_moisture[i] = l.soil_moisture_m3
		}
		if !d.initialized {
			err := stm.module_init(d.module, in_, ms.allocator)
			assert(err == "", err) // only fails without layers, which run_monica refuses
			d.initialized = true
		}
		stm.module_step(d.module, in_, &d.output, scratch)
	}
	mem.dynamic_arena_reset(&ms.scratch)
}
