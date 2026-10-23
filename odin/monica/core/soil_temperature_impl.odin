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

import p "../params"
import "core:fmt"

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
	if len(mc.soil_temperature_drivers) > 0 {
		append(
			&errs,
			fmt.aprintf(
				"Error: Modules.SoilTemperatureDrivers: not supported yet (got %v)",
				mc.soil_temperature_drivers,
				allocator = allocator,
			),
		)
	}
	return errs
}

@(private = "file")
soil_temperature_impl_names :: proc(allocator := context.allocator) -> []string {
	names := make([]string, len(soil_temperature_impls), allocator)
	for e, i in soil_temperature_impls {
		names[i] = e.name
	}
	return names
}
