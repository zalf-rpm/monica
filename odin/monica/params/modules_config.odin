// sim.json's "Modules" section: which implementation runs a process
// (plan-crop2ml-modules.md §1.4). No C++ counterpart - the C++ AMEI branch
// selected its modules through an if/else chain on customData instead.
//
//   "Modules": {
//     "SoilTemperature": {"impl": "internal", "mode": "integrated"},
//     "SoilTemperatureDrivers": []
//   }
//
// Absent means MONICA's own implementations, exactly as without this section.
// Only the shape is checked here; whether a name exists is the business of the
// process's name table in core (soil_temperature_impl.odin), which params
// can't import.
package params

import jx "../../support/jsonx"
import tl "../../support/tools"

// What a sim.json selects for one process. Empty strings are the defaults:
// impl "" is the same as "internal", mode "" the same as "integrated".
Module_Config :: struct {
	impl: string,
	mode: string,
}

Modules_Config :: struct {
	soil_temperature:         Module_Config,
	// modules run alongside MONICA's own implementation, only reported (§1.4)
	soil_temperature_drivers: [dynamic]string,
}

// The names every process's table accepts for MONICA's own implementation and
// for the only mode that exists so far.
MODULE_IMPL_INTERNAL :: "internal"
MODULE_MODE_INTEGRATED :: "integrated"

// True if nothing but MONICA's own implementations is configured, the
// situation in which "Modules" must not change a single output byte (R1).
modules_config_is_default :: proc(mc: ^Modules_Config) -> bool {
	impl := mc.soil_temperature.impl
	return (impl == "" || impl == MODULE_IMPL_INTERNAL) && len(mc.soil_temperature_drivers) == 0
}

modules_config_merge :: proc(mc: ^Modules_Config, j: jx.Value) -> tl.Errors {
	res: tl.Errors
	if jx.is_null(j) {
		return res
	}
	if !jx.is_object(j) {
		tl.append_error(&res, "Modules: expected an object")
		return res
	}
	tl.append_errors(&res, module_config_merge(&mc.soil_temperature, jx.get(j, "SoilTemperature"), "SoilTemperature"))
	drivers := jx.get(j, "SoilTemperatureDrivers")
	if !jx.is_null(drivers) && !jx.is_array(drivers) {
		tl.append_error(&res, "Modules.SoilTemperatureDrivers: expected an array of module names")
	} else {
		jx.set_string_vector(&mc.soil_temperature_drivers, j, "SoilTemperatureDrivers")
	}
	return res
}

@(private = "file")
module_config_merge :: proc(mc: ^Module_Config, j: jx.Value, process: string) -> tl.Errors {
	res: tl.Errors
	if jx.is_null(j) {
		return res
	}
	if !jx.is_object(j) {
		tl.append_errorf(&res, "Modules.%s: expected an object like {{\"impl\": \"internal\"}}", process)
		return res
	}
	jx.set_string_value(&mc.impl, j, "impl")
	jx.set_string_value(&mc.mode, j, "mode")
	if mc.mode != "" && mc.mode != MODULE_MODE_INTEGRATED {
		tl.append_errorf(
			&res,
			"Modules.%s: unknown mode %q (only %q is supported)",
			process,
			mc.mode,
			MODULE_MODE_INTEGRATED,
		)
	}
	return res
}

// Written only when something is configured, so that an unconfigured env's
// JSON stays what it was before "Modules" existed.
modules_config_to_json :: proc(mc: ^Modules_Config, a: Allocator) -> (jx.Value, bool) {
	if mc.soil_temperature == {} && len(mc.soil_temperature_drivers) == 0 {
		return nil, false
	}
	drivers := make([dynamic]jx.Value, 0, len(mc.soil_temperature_drivers), a)
	for name in mc.soil_temperature_drivers {
		append(&drivers, jx.s(name, a))
	}
	return jx.obj(
			a,
			{
				"SoilTemperature",
				jx.obj(a, {"impl", jx.s(mc.soil_temperature.impl, a)}, {"mode", jx.s(mc.soil_temperature.mode, a)}),
			},
			{"SoilTemperatureDrivers", jx.arr(a, ..drivers[:])},
		),
		true
}
