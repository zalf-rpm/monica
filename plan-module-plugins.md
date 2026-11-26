# Plan: modules as shared libraries (plugins)

Written 2026-11-26. Odin port only; the C++ AMEI branch isn't covered.

## Goal

A module of a swappable process (soil temperature, evapotranspiration; plan-crop2ml-modules.md)
can be either

- **built in**: compiled into MONICA, as all of them are now, or
- **a plugin**: a shared library (`.so`, `.dll`) loaded at startup, so that experimental modules -
  mostly those running as drivers beside MONICA - don't have to be compiled into MONICA.

A plugin module can do everything a built-in one can, as a driver *and* integrated: the
integration is per process, written against the process's `Input`/`Output`, not per module. Only a
new *process* needs changes in MONICA. Results are the same bits either way.

## What crosses the boundary

The process interface already is the boundary: a module only sees its process's `Input` (plain
numbers and slices, MONICA's units) and writes its `Output` (numbers, slices MONICA allocated).
That stays as it is. Across the library boundary:

- **Data**: `^Input` and `^Output` as raw pointers to MONICA's structs. The plugin is compiled
  against the same process package, so it has the same struct definitions. A **layout hash**
  (computed on both sides from Odin's type info: names, offsets, sizes and types of all fields,
  recursively) rejects a plugin built against another layout.
- **Calls**: `"c"` procedures on an opaque instance pointer: make, init, step, destroy. No Odin
  `context` crosses (each Odin shared library has its own runtime); every exported procedure
  starts with its own `runtime.default_context()`.
- **Memory**: never crosses. The plugin allocates and frees its own memory; it only reads `Input`
  and fills `Output`'s numbers and existing slices, never resizes them. The step allocator MONICA
  passes to built-in modules (a scratch arena, reset after each step; a lasting allocator for the
  first step, for modules that size arrays lazily) is recreated on the plugin side: a lasting
  allocator for init and the first step, a per-instance arena reset after every later step.
- **Description**: per module the process name, the module name, `integrated`, `needs_layers`,
  `inputs`, `outputs` (the `Module_Entry` fields), as C strings and arrays.

One library can contain several modules (e.g. all AMEI soil temperature modules).

## ABI (package `monica/modules/plugin`)

```odin
ABI_VERSION :: 1

Module_Info :: struct {
	abi_version:  u32,
	process:      cstring, // "SoilTemperature", "Evapotranspiration"
	name:         cstring, // as in sim.json
	input_hash:   u64,     // layout hashes of the process's Input and Output
	output_hash:  u64,
	integrated:   b8,
	needs_layers: b8,
	inputs:       [^]cstring, n_inputs: i32,   // nil: all
	outputs:      [^]cstring, n_outputs: i32,
	make:         proc "c" (info: ^Module_Info) -> rawptr,
	init:         proc "c" (instance: rawptr, input: rawptr),
	step:         proc "c" (instance: rawptr, input, output: rawptr),
	destroy:      proc "c" (instance: rawptr),
	user:         rawptr, // the plugin's own (its Module_Entry)
}

// exported by every plugin
monica_module_plugin :: proc "c" (n: ^i32) -> [^]Module_Info
```

The package also has the host's registry (load a library, check its modules, find a module by
process and name) and `layout_hash`. It imports no process package; the process packages import it.

## Host side

- Each process package gets a `^plugin.Instance` variant in its `Module` union and a `plugin` field
  in `Module_Entry`. `find_module(name)` looks in the built-in table, then among the loaded
  plugins of its process, and returns a `Module_Entry` whose procedures call the plugin's.
  `make_module(entry, allocator)` replaces `entry.make(allocator)` (a plugin entry's make needs
  the entry). The "known: ..." lists in the error messages include plugin modules.
- A plugin module with the name of a built-in one is refused (the built-in one wins, and the
  library is reported).
- Loading: sim.json `"Modules": {"Plugins": ["path/libx.so", ...]}` (relative to the working
  directory) and the environment variable `MONICA_MODULE_PLUGINS` (paths separated like PATH),
  for all executables. A library is loaded once per process, by its path; errors (not found, no
  `monica_module_plugin`, ABI version or layout hash mismatch, unknown process) are run errors
  like an unknown module name. `monica-module` reads the environment variable too.
- Output paths: `input`/`output` of a plugin module work as for a built-in one
  (`modules.evapotranspiration_drivers.<name>.output.x`). Paths into a module's own state
  (`module.Dssat_St.s.srftemp`) need its types; step 2.

## Plugin side

- Each process package gets `plugin_module_info(entry, name)` and the `"c"` procedures wrapping a
  `Module_Entry` (dead code in MONICA itself, linked only into plugins). (Step 2 changed the
  module representation, see there.)
- A plugin is a package in `odin/plugins/<name>/` with the exported `monica_module_plugin`,
  listing the `Module_Entry`s it contains. It imports the process package(s) and its binding(s).
- Built with `python tools/odinw.py build-plugin <name>` into `build/plugins/`
  (`-build-mode:shared`), with the same flags as MONICA: `cr_math` linked statically (built with
  `-fPIC`), no FMA contraction (generic x86-64), so results are the same bits.

## Steps

1. **Plugins for the existing processes** - done 2026-11-26:
   - the ABI package, host and plugin side for soil temperature and evapotranspiration, loading via
     sim.json and the environment variable, `build-plugin`;
   - two test plugins built from the existing bindings under other names
     (`Plugin_Monica_Reference_Evapotranspiration`, `Plugin_DSSAT_ST_standalone`);
   - checks (`tests/check_drivers.py`): as a driver the plugin gives exactly the built-in module's
     outputs; `Plugin_DSSAT_ST_standalone` integrated gives exactly the outputs of
     `DSSAT_ST_standalone` integrated; a plugin with a wrong layout hash is refused;
   - CI: build and load a plugin on Linux and Windows.

   As done: `monica/modules/plugin/plugin.odin` (ABI, registry, `layout_hash`),
   `monica/modules/<process>/plugins.odin` (both sides), `tools/odinw.py build-plugin`,
   `tests/plugins/test_modules` and `tests/plugins/test_broken` (another ABI version, an unknown
   process, another layout, a built-in's name - refused module by module; its good module
   loads), `tests/check_plugins.py` (CI, the pixi job on Linux and Windows). Checked on Linux:
   plugin == built-in as drivers every day (ET0, surface and 20 layer temperatures) and
   integrated byte-identical, through `Modules.Plugins` and `MONICA_MODULE_PLUGINS`;
   `monica-module` standalone identical; MONICA's outputs without plugins unchanged (66 suite
   simulations). Any load error fails the run, also one in a module the run doesn't use.
2. **Modules out of MONICA** - done 2026-11-26:
   - **Bindings as packages.** Each binding is its own package,
     `monica/modules/<process>/<name>/`, importing only its process's interface. The process
     package has no union of module types any more: `Module` is `{data: rawptr, type: typeid}`
     (the binding's struct), `Module_Entry` got `type`, and the built-in table is filled at
     startup (`register_builtin`). Bindings changed only mechanically (`m.(^T)` ->
     `(^T)(m.data)`, `make` returns `Module{b, T}`, names qualified).
   - **Built in or plugin** is a line in `monica/core/builtin_modules.odin` or in a plugin
     package. Built in: MONICA's own two (`Monica_SoilTemp`, `Monica_Evapotranspiration`), which
     the checks compare MONICA with. Everything else is in `plugins/amei`.
   - **Paths into a module's state** keep their form (`...drivers.<name>.module.Dssat_St.s.srftemp`):
     `reflectpath` got *foreign types* (`register_foreign`): at a registered type the rest of the
     path goes to a hook, with the last string map key (the driver's name). MONICA's hooks
     (`core/module_paths.odin`) compile it against the binding's struct - in MONICA for a built-in
     module, in the plugin for a plugin's (ABI 2: `path_compile`, `path_read`; both use
     `plugin.compile_module_path`). Read-only; unit and layer dimension come from the module's
     types. Only through a driver (the integrated module has no name in the path).
   - **Paths of libraries**: `${VAR}`s replaced, the platform's extension added when there is
     none (one sim.json for Linux and Windows). The same library under two paths is loaded once;
     two libraries with a module of the same name are refused.
   - **Nix and Docker** build `plugins/amei` and set `MONICA_MODULE_PLUGINS` to it by default, so
     the packaged MONICA has the AMEI modules as before. `sim-min-amei.json` names the pixi
     build's (`../../odin/build/plugins/amei`).
   - Checks: `check_modules.sh` runs every AMEI module from the plugin (Odin == generated Python
     as before, drivers/integrated as before, `module.Dssat_St.s.srftemp` through the plugin);
     `check_plugins.py` with MONICA's built-in two as twins (drivers, integrated, state paths with
     a layer dimension and the plugin's unit, standalone), the AMEI plugin (extension-less path,
     twice under two paths, unknown without it). MONICA's outputs unchanged (66 suite
     simulations); the Nix package built and ran an AMEI module without `Plugins`.

## Not planned

- Plugins for new processes: a new process needs its interface in MONICA (process package,
  gathering in core, configuration).
- Stable ABI across Odin versions or MONICA versions: plugins are built with MONICA's pinned Odin
  from the same source tree; the version and hash checks refuse anything else.
