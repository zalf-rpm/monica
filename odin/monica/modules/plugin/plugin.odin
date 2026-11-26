// Modules as shared libraries (plugins), see ../../../../plan-module-plugins.md.
//
// A plugin is a shared library exporting `monica_module_plugin`, which lists
// its modules as Module_Info: process, name, the process interface's layout
// hashes and "c" procedures on an opaque instance. MONICA passes its process
// Input/Output structs by pointer; the plugin is compiled against the same
// process package, and the layout hashes refuse one compiled against another
// layout. Nothing else crosses: no Odin context (each library has its own
// runtime), no memory (the plugin allocates and frees its own, only reads the
// Input and fills the Output's numbers and existing slices).
//
// This package is the ABI and MONICA's registry of loaded plugin modules. It
// imports no process package; the process packages (soil_temperature,
// evapotranspiration) import it, register their interface's layout
// (register_process) and wrap plugin modules as their Module_Entry
// (find_module), and on the plugin side wrap a Module_Entry as Module_Info
// (plugin_module_info).
package plugin

import "base:runtime"
import "core:dynlib"
import "core:fmt"
import "core:os"
import "core:strings"
import "core:sync"

// Changed whenever Module_Info or the meaning of its procedures changes.
ABI_VERSION :: 1

// The name every plugin exports: monica_module_plugin(&n) -> n Module_Infos.
ENTRY_SYMBOL :: "monica_module_plugin"
Entry_Proc :: #type proc "c" (n: ^i32) -> [^]Module_Info

// Paths of plugin libraries, separated like PATH, loaded by load_from_env.
ENV_VAR :: "MONICA_MODULE_PLUGINS"

// One module of a plugin.
Module_Info :: struct {
	abi_version:  u32,
	process:      cstring, // "SoilTemperature", "Evapotranspiration"
	name:         cstring, // as in sim.json
	input_hash:   u64, // layout_hash of the process's Input and Output
	output_hash:  u64,
	integrated:   b8, // can replace MONICA's own (evapotranspiration)
	needs_layers: b8,
	inputs:       [^]cstring, // the Daily fields it reads; nil: all
	n_inputs:     i32,
	outputs:      [^]cstring, // the Output fields it computes; nil: all
	n_outputs:    i32,
	make:         proc "c" (info: ^Module_Info) -> rawptr,
	init:         proc "c" (instance: rawptr, input: rawptr),
	step:         proc "c" (instance: rawptr, input, output: rawptr),
	destroy:      proc "c" (instance: rawptr),
	user:         rawptr, // the plugin's own (its Module_Entry)
}

// A plugin module made by MONICA: the Module union variant of a plugin module.
Instance :: struct {
	info:   ^Module_Info,
	handle: rawptr, // what info.make returned
}

// A loaded plugin module, with its strings as Odin strings.
Loaded_Module :: struct {
	info:         ^Module_Info,
	process:      string,
	name:         string,
	library:      string, // the path it was loaded from
	integrated:   bool,
	needs_layers: bool,
	inputs:       []string, // nil: all
	outputs:      []string,
}

@(private = "file")
Process :: struct {
	name:                    string,
	input_hash, output_hash: u64,
	builtin:                 proc(name: string) -> bool, // a built-in module of that name exists
}

@(private = "file")
State :: struct {
	mutex:      sync.Mutex,
	processes:  [dynamic]Process,
	libraries:  [dynamic]string, // paths loaded (or tried) already
	modules:    [dynamic]Loaded_Module,
	env_loaded: bool,
	env_errors: [dynamic]string,
}

@(private = "file")
state: State

// Called by each process package at startup (@(init)), with its interface's
// layout and a lookup of its built-in modules.
register_process :: proc(name: string, input_hash, output_hash: u64, builtin: proc(name: string) -> bool) {
	sync.guard(&state.mutex)
	context.allocator = runtime.heap_allocator()
	append(&state.processes, Process{name, input_hash, output_hash, builtin})
}

// Loads the plugin library at `path` and registers its modules. A path loaded
// before is not loaded again (its errors were reported then). Errors: the
// library can't be loaded, has no monica_module_plugin, a module has another
// ABI version, an unknown process, another layout, or the name of a built-in
// or an already loaded module (that module is skipped, the others load).
load :: proc(path: string, allocator := context.allocator) -> (errors: [dynamic]string) {
	errors = make([dynamic]string, allocator)
	sync.guard(&state.mutex)
	for l in state.libraries {
		if l == path {
			return
		}
	}
	{
		context.allocator = runtime.heap_allocator()
		append(&state.libraries, strings.clone(path))
	}

	lib, ok := dynlib.load_library(path, false, context.temp_allocator)
	if !ok {
		append(&errors, fmt.aprintf("Error: module plugin %q: can't load it: %s", path, dynlib.last_error(), allocator = allocator))
		return
	}
	sym, found := dynlib.symbol_address(lib, ENTRY_SYMBOL, context.temp_allocator)
	if !found {
		append(&errors, fmt.aprintf("Error: module plugin %q: no %s", path, ENTRY_SYMBOL, allocator = allocator))
		return
	}
	n: i32
	infos := (cast(Entry_Proc)sym)(&n)
	for i in 0 ..< int(n) {
		info := &infos[i]
		if info.abi_version != ABI_VERSION {
			append(
				&errors,
				fmt.aprintf(
					"Error: module plugin %q: ABI version %d, MONICA's is %d - rebuild the plugin",
					path,
					info.abi_version,
					ABI_VERSION,
					allocator = allocator,
				),
			)
			continue
		}
		process, name := string(info.process), string(info.name)
		pr: ^Process
		for &p in state.processes {
			if p.name == process {
				pr = &p
			}
		}
		if pr == nil {
			append(
				&errors,
				fmt.aprintf("Error: module plugin %q: %q is for the unknown process %q", path, name, process, allocator = allocator),
			)
			continue
		}
		if info.input_hash != pr.input_hash || info.output_hash != pr.output_hash {
			append(
				&errors,
				fmt.aprintf(
					"Error: module plugin %q: %q was built against another %s interface (Input/Output) - rebuild the plugin",
					path,
					name,
					process,
					allocator = allocator,
				),
			)
			continue
		}
		if pr.builtin(name) {
			append(
				&errors,
				fmt.aprintf("Error: module plugin %q: %q is the name of a built-in module", path, name, allocator = allocator),
			)
			continue
		}
		dup := false
		for m in state.modules {
			if m.process == process && m.name == name {
				append(
					&errors,
					fmt.aprintf(
						"Error: module plugin %q: %q was loaded already from %q",
						path,
						name,
						m.library,
						allocator = allocator,
					),
				)
				dup = true
			}
		}
		if dup {
			continue
		}
		context.allocator = runtime.heap_allocator()
		strs :: proc(cs: [^]cstring, n: i32) -> []string {
			if cs == nil {
				return nil
			}
			ss := make([]string, n)
			for i in 0 ..< int(n) {
				ss[i] = strings.clone(string(cs[i]))
			}
			return ss
		}
		append(
			&state.modules,
			Loaded_Module {
				info = info,
				process = strings.clone(process),
				name = strings.clone(name),
				library = strings.clone(path),
				integrated = bool(info.integrated),
				needs_layers = bool(info.needs_layers),
				inputs = strs(info.inputs, info.n_inputs),
				outputs = strs(info.outputs, info.n_outputs),
			},
		)
	}
	return
}

// Loads the libraries in MONICA_MODULE_PLUGINS, once per program run; the
// errors (of that one time) every time.
load_from_env :: proc(allocator := context.allocator) -> (errors: [dynamic]string) {
	{
		sync.guard(&state.mutex)
		if state.env_loaded {
			errors = make([dynamic]string, len(state.env_errors), allocator)
			for e, i in state.env_errors {
				errors[i] = strings.clone(e, allocator)
			}
			return
		}
		state.env_loaded = true
	}
	errors = make([dynamic]string, allocator)
	paths := os.get_env(ENV_VAR, context.temp_allocator)
	for q in strings.split(paths, ODIN_OS == .Windows ? ";" : ":", context.temp_allocator) {
		if p := strings.trim_space(q); p != "" {
			for e in load(p, allocator) {
				append(&errors, e)
			}
		}
	}
	sync.guard(&state.mutex)
	context.allocator = runtime.heap_allocator()
	for e in errors {
		append(&state.env_errors, strings.clone(e))
	}
	return
}

// The loaded module `name` of `process`.
find :: proc(process, name: string) -> (Loaded_Module, bool) {
	sync.guard(&state.mutex)
	for m in state.modules {
		if m.process == process && m.name == name {
			return m, true
		}
	}
	return {}, false
}

// The names of the loaded modules of `process`.
names :: proc(process: string, allocator := context.allocator) -> []string {
	sync.guard(&state.mutex)
	ns := make([dynamic]string, 0, len(state.modules), allocator)
	for m in state.modules {
		if m.process == process {
			append(&ns, m.name)
		}
	}
	return ns[:]
}

// MONICA's side of a plugin module: make, init, step, destroy.
make_instance :: proc(info: ^Module_Info, allocator := context.allocator) -> ^Instance {
	i := new(Instance, allocator)
	i.info = info
	i.handle = info.make(info)
	return i
}

init_instance :: proc(i: ^Instance, input: rawptr) {
	i.info.init(i.handle, input)
}

step_instance :: proc(i: ^Instance, input, output: rawptr) {
	i.info.step(i.handle, input, output)
}

destroy_instance :: proc(i: ^Instance, allocator := context.allocator) {
	i.info.destroy(i.handle)
	free(i, allocator)
}

// A hash of a type's memory layout: for structs the names, offsets and types
// of the fields, recursively; named types by name. MONICA and a plugin each
// compute it for the process's Input and Output from their own type info; the
// same value means the plugin reads and writes them as MONICA lays them out.
layout_hash :: proc(T: typeid) -> u64 {
	h: u64 = 14695981039346656037 // FNV-1a
	hash_type(&h, type_info_of(T))
	return h
}

@(private = "file")
hash_type :: proc(h: ^u64, ti: ^runtime.Type_Info) {
	mix :: proc(h: ^u64, bytes: []byte) {
		for b in bytes {
			h^ = (h^ ~ u64(b)) * 1099511628211
		}
	}
	mix_int :: proc(h: ^u64, x: int) {
		x := x
		mix(h, ([^]byte)(&x)[:size_of(x)])
	}
	mix_str :: proc(h: ^u64, s: string) {
		mix_int(h, len(s))
		mix(h, transmute([]byte)s)
	}
	if ti == nil {
		mix_str(h, "nil")
		return
	}
	mix_int(h, ti.size)
	mix_int(h, ti.align)
	#partial switch v in ti.variant {
	case runtime.Type_Info_Named:
		mix_str(h, "named")
		mix_str(h, v.name)
		hash_type(h, v.base)
	case runtime.Type_Info_Struct:
		mix_str(h, "struct")
		mix_int(h, int(v.field_count))
		for i in 0 ..< int(v.field_count) {
			mix_str(h, v.names[i])
			mix_int(h, int(v.offsets[i]))
			hash_type(h, v.types[i])
		}
	case runtime.Type_Info_Slice:
		mix_str(h, "slice")
		hash_type(h, v.elem)
	case runtime.Type_Info_Dynamic_Array:
		mix_str(h, "dynamic")
		hash_type(h, v.elem)
	case runtime.Type_Info_Array:
		mix_str(h, "array")
		mix_int(h, v.count)
		hash_type(h, v.elem)
	case runtime.Type_Info_Pointer:
		mix_str(h, "pointer")
		hash_type(h, v.elem)
	case runtime.Type_Info_Multi_Pointer:
		mix_str(h, "multi-pointer")
		hash_type(h, v.elem)
	case runtime.Type_Info_Enum:
		mix_str(h, "enum")
		hash_type(h, v.base)
		for n in v.names {
			mix_str(h, n)
		}
	case:
		// numbers, booleans, strings: the type, plus size/align above
		mix_str(h, fmt.tprint(ti.id))
	}
}
