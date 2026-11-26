// Evapotranspiration modules from plugin libraries (../plugin,
// plan-module-plugins.md).
//
// MONICA's side: find_module falls back to the loaded plugins' modules of this
// process, as a Module_Entry whose procedures call the plugin's; make_module
// makes either kind.
//
// The plugin's side: plugin_module_info turns a Module_Entry into the
// Module_Info a plugin library exports. The step allocator MONICA passes a
// built-in module (lasting for the first step, a scratch arena after) is
// recreated there, per instance.
package evapotranspiration

import pl "../plugin"
import "base:runtime"
import "core:mem"
import "core:strings"

PROCESS :: "Evapotranspiration"

@(init, private = "file")
register :: proc "contextless" () {
	context = runtime.default_context()
	pl.register_process(PROCESS, pl.layout_hash(Input), pl.layout_hash(Output), proc(name: string) -> bool {
		for e in modules {
			if e.name == name {
				return true
			}
		}
		return false
	})
}

// Makes a module of either kind (a plugin entry has no `make`).
make_module :: proc(e: Module_Entry, allocator := context.allocator) -> Module {
	if e.plugin != nil {
		return pl.make_instance(e.plugin, allocator)
	}
	return e.make(allocator)
}

// The names of the built-in modules and those of the loaded plugins.
module_names :: proc(allocator := context.allocator) -> []string {
	ns := make([dynamic]string, 0, len(modules), allocator)
	for e in modules {
		append(&ns, e.name)
	}
	for n in pl.names(PROCESS, context.temp_allocator) {
		append(&ns, n)
	}
	return ns[:]
}

@(private)
find_plugin_module :: proc(name: string) -> (Module_Entry, bool) {
	m, ok := pl.find(PROCESS, name)
	if !ok {
		return {}, false
	}
	return Module_Entry {
			name = m.name,
			init = proc(m: Module, in_: ^Input, a: Allocator) {pl.init_instance(m.(^pl.Instance), in_)},
			step = proc(m: Module, in_: ^Input, out: ^Output, a: Allocator) {
				pl.step_instance(m.(^pl.Instance), in_, out)
			},
			destroy = proc(m: Module, a: Allocator) {pl.destroy_instance(m.(^pl.Instance), a)},
			integrated = m.integrated,
			inputs = m.inputs,
			outputs = m.outputs,
			needs_layers = m.needs_layers,
			plugin = m.info,
		},
		true
}

// --- the plugin's side ---

@(private = "file")
Plugin_Instance :: struct {
	entry:   ^Module_Entry,
	module:  Module,
	scratch: mem.Dynamic_Arena,
	stepped: bool,
}

// The Module_Info a plugin library exports for `entry` (which must outlive
// it), under `name` (default: the entry's).
plugin_module_info :: proc(entry: ^Module_Entry, name := "") -> pl.Module_Info {
	context.allocator = runtime.heap_allocator()
	return pl.Module_Info {
		abi_version = pl.ABI_VERSION,
		process = PROCESS,
		name = strings.clone_to_cstring(name != "" ? name : entry.name),
		input_hash = pl.layout_hash(Input),
		output_hash = pl.layout_hash(Output),
		integrated = b8(entry.integrated),
		needs_layers = b8(entry.needs_layers),
		inputs = cstrings(entry.inputs),
		n_inputs = i32(len(entry.inputs)),
		outputs = cstrings(entry.outputs),
		n_outputs = i32(len(entry.outputs)),
		make = plugin_make,
		init = plugin_init,
		step = plugin_step,
		destroy = plugin_destroy,
		user = entry,
	}
}

// nil for nil (all)
@(private = "file")
cstrings :: proc(ss: []string) -> [^]cstring {
	if ss == nil {
		return nil
	}
	cs := make([]cstring, len(ss))
	for s, i in ss {
		cs[i] = strings.clone_to_cstring(s)
	}
	return raw_data(cs)
}

@(private = "file")
plugin_make :: proc "c" (info: ^pl.Module_Info) -> rawptr {
	context = runtime.default_context()
	pi := new(Plugin_Instance)
	pi.entry = (^Module_Entry)(info.user)
	pi.module = pi.entry.make(context.allocator)
	mem.dynamic_arena_init(&pi.scratch, context.allocator, context.allocator)
	return pi
}

@(private = "file")
plugin_init :: proc "c" (instance: rawptr, input: rawptr) {
	context = runtime.default_context()
	pi := (^Plugin_Instance)(instance)
	pi.entry.init(pi.module, (^Input)(input), context.allocator)
}

@(private = "file")
plugin_step :: proc "c" (instance: rawptr, input, output: rawptr) {
	context = runtime.default_context()
	pi := (^Plugin_Instance)(instance)
	if !pi.stepped { 	// a lasting allocator for the first step, as MONICA does
		pi.entry.step(pi.module, (^Input)(input), (^Output)(output), context.allocator)
		pi.stepped = true
		return
	}
	pi.entry.step(pi.module, (^Input)(input), (^Output)(output), mem.dynamic_arena_allocator(&pi.scratch))
	mem.dynamic_arena_reset(&pi.scratch)
}

@(private = "file")
plugin_destroy :: proc "c" (instance: rawptr) {
	context = runtime.default_context()
	pi := (^Plugin_Instance)(instance)
	pi.entry.destroy(pi.module, context.allocator)
	mem.dynamic_arena_destroy(&pi.scratch)
	free(pi)
}
