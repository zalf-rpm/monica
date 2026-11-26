// Output paths into a module's own state, e.g.
// modules.soil_temperature_drivers.DSSAT_ST_standalone.module.Dssat_St.s.srftemp
// (plan-module-plugins.md). A module is opaque to MONICA's types
// (modules/<process>'s Module: a pointer and the binding's type), so
// reflectpath hands the rest of such a path to the hooks registered here: they
// find the module by the driver's name (the map key before it) and compile the
// rest against the binding's struct - in MONICA for a built-in module, in the
// plugin library for a plugin's (path_compile, path_read). Read-only.
package core

import etm "../modules/evapotranspiration"
import pl "../modules/plugin"
import stm "../modules/soil_temperature"
import rp "../../support/reflectpath"
import "base:runtime"
import "core:fmt"
import "core:strings"

@(init, private = "file")
register_module_paths :: proc "contextless" () {
	context = runtime.default_context()
	rp.register_foreign(stm.Module, proc(key, rest: string, allocator: runtime.Allocator) -> (rp.Foreign, string) {
		e, ok := stm.find_module(key)
		if !ok {
			return {}, module_path_unknown("soil_temperature", key, allocator)
		}
		return module_path(e.type, e.plugin, rest, allocator)
	})
	rp.register_foreign(etm.Module, proc(key, rest: string, allocator: runtime.Allocator) -> (rp.Foreign, string) {
		e, ok := etm.find_module(key)
		if !ok {
			return {}, module_path_unknown("evapotranspiration", key, allocator)
		}
		return module_path(e.type, e.plugin, rest, allocator)
	})
}

@(private = "file")
module_path_unknown :: proc(process, key: string, allocator: runtime.Allocator) -> string {
	if key == "" {
		return fmt.aprintf(
			"a module's own state is reachable through its driver: modules.%s_drivers.<name>.module...",
			process,
			allocator = allocator,
		)
	}
	return fmt.aprintf("unknown %s module %q", process, key, allocator = allocator)
}

// `value` in the read procedures is the module's Module {data, type}; data is
// the binding's struct, or a plugin's ^pl.Instance.
@(private = "file")
module_path :: proc(type: typeid, plugin: ^pl.Module_Info, rest: string, allocator: runtime.Allocator) -> (rp.Foreign, string) {
	if plugin == nil {
		plan, err := pl.compile_module_path(type, rest, allocator)
		if err != "" {
			return {}, err
		}
		return rp.Foreign {
				handle = plan,
				read = proc(handle: rawptr, value: rawptr, open_index: int) -> (f64, bool) {
					return pl.read_module_path((^rp.Path_Plan)(handle), (^rawptr)(value)^, open_index)
				},
				open = plan.open_step >= 0,
				unit = plan.unit,
				has_unit = plan.has_unit,
			},
			""
	}
	info: pl.Path_Info
	handle := plugin.path_compile(plugin, strings.clone_to_cstring(rest, context.temp_allocator), &info)
	if handle == nil {
		return {}, strings.clone(string(info.error), allocator)
	}
	ph := new(Plugin_Path, allocator)
	ph^ = {plugin, handle}
	return rp.Foreign {
			handle = ph,
			read = proc(handle: rawptr, value: rawptr, open_index: int) -> (f64, bool) {
				ph := (^Plugin_Path)(handle)
				inst := (^pl.Instance)((^rawptr)(value)^)
				if inst == nil {
					return 0, false
				}
				v: f64
				ok := ph.info.path_read(inst.handle, ph.path, i32(open_index), &v)
				return v, bool(ok)
			},
			open = bool(info.open),
			unit = strings.clone(string(info.unit), allocator),
			has_unit = bool(info.has_unit),
		},
		""
}

@(private = "file")
Plugin_Path :: struct {
	info: ^pl.Module_Info,
	path: rawptr,
}
