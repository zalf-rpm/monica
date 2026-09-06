// Port of src/run/run-monica-capnp.{h,cpp} - the RunMonica capability, i.e. the
// Cap'n Proto equivalent of run/serve-monica-zmq.cpp's message loop.
//
// Its own package (not part of `run`) on purpose: everything here links the
// Cap'n Proto dynamic-API shim (support/capnp), and monica-run / monica-zmq-server
// must keep building without it.
//
// WHY THE DYNAMIC API AT ALL. The C++ implements the generated, statically typed
// server class
//
//     mas::schema::model::EnvInstance<StructuredText, StructuredText>::Server
//
// There is no generated Odin code, so this hosts the interface by NAME out of the
// raw model.capnp instead (see support/capnp/README.md, "Option C"). Cap'n Proto
// hands back the UNBOUND generic schema for `EnvInstance(RestInput, Output)`, in
// which both type parameters degrade to AnyPointer - so where the C++ reads
// `envR.getRest()` as a StructuredText directly, this has to reinterpret an
// AnyPointer as one (any_pointer_as_struct), and symmetrically build the result
// with any_pointer_from_struct. That round trip is wire-identical: a generic
// interface has one type id regardless of how a client brands it, which is why a
// normally-branded client (pycapnp, the C++ MONICA clients) talks to this
// unchanged. Verified end to end against pycapnp - see tests/capnp/.
//
// NOT PORTED YET (stage 1 scope, MVP direct-bootstrap):
//   - env.timeSeries / env.soilProfile: both are capabilities the C++ calls BACK
//     into (capnp-helper.cpp's dataAccessorFromTimeSeries/fromCapnpSoilProfile).
//     The shim only allows that from an async handler (a blocking call into a
//     capability belonging to the dispatching connection deadlocks by
//     construction), so it needs host_async + call_async_then. run_monica_env
//     already takes `da`/`soil_layers` so that stage drops straight in.
//   - save (persistence.capnp Persistent): needs a Restorer, which this MVP
//     replaces by serving RunMonica as the bootstrap capability directly.
//   - stop (service.capnp Stoppable): commented out in the C++ too.
package capnp

import "core:encoding/uuid"
import "core:fmt"
import "core:sync"
import capnp_dyn "../../support/capnp/odin/capnp_dynamic"
import clim "../../support/climate"
import jx "../../support/jsonx"
import tl "../../support/tools"
import mio "../io"
import "../run"

// Disk locations of the raw .capnp files the shim parses at runtime. There is no
// codegen step, so these are needed by the running binary, not just at build time.
Schema_Paths :: struct {
	root:   string, // zalfmas_capnp_schemas/ - import root for "/common/common.capnp" etc
	model:  string, // model/model.capnp   - declares EnvInstance
	common: string, // common/common.capnp - declares StructuredText
}

make_schema_paths :: proc(root: string, allocator := context.allocator) -> Schema_Paths {
	return Schema_Paths {
		root = root,
		model = fmt.aprintf("%s/model/model.capnp", root, allocator = allocator),
		common = fmt.aprintf("%s/common/common.capnp", root, allocator = allocator),
	}
}

// C++: class monica::RunMonica final : public MonicaEnvInstance::Server
//
// _client/_restorer are absent: both exist only for save() (see the file header).
// _da/_soilLayers are absent as members deliberately - the C++ keeps them on the
// object, which races if two run() calls ever overlap; here they are ordinary
// arguments to run_monica_env instead.
Run_Monica :: struct {
	_startedServerInDebugMode: bool,
	_id:                       string,
	_name:                     string,
	_description:              string,

	// Not in the C++: the schema paths (there, the generated code carries the
	// schema) and the per-call scratch arena / dispatch lock described on
	// handle_call.
	schema:                    Schema_Paths,
	pathToSoilDir:             string,
	lock:                      sync.Mutex,
	call_arena:                jx.Arena,
	call_arena_live:           bool,
}

// C++: RunMonica::RunMonica(bool startedServerInDebugMode, Restorer*)
make_run_monica :: proc(
	started_server_in_debug_mode: bool,
	schema: Schema_Paths,
	allocator := context.allocator,
) -> ^Run_Monica {
	rm := new(Run_Monica, allocator)
	rm._startedServerInDebugMode = started_server_in_debug_mode
	// C++: _id = kj::str(sole::uuid4().str());
	rm._id = uuid.to_string(uuid.generate_v4(), allocator)
	// C++: _name = kj::str("Monica capnp server");
	rm._name = "Monica capnp server"
	rm._description = ""
	rm.schema = schema
	// C++ resolves this inside the run lambda on every call; hoisted because it
	// cannot change between calls.
	rm.pathToSoilDir = tl.fix_system_separator(
		tl.replace_env_vars("${MONICA_PARAMETERS}/soil/", allocator),
		allocator,
	)
	return rm
}

// C++: kj::Promise<void> RunMonica::info(InfoContext context)
run_monica_info :: proc(rm: ^Run_Monica, allocator := context.allocator) -> []capnp_dyn.Field {
	fields := make([]capnp_dyn.Field, 3, allocator)
	fields[0] = {name = "id", value = rm._id}
	fields[1] = {name = "name", value = rm._name}
	fields[2] = {name = "description", value = rm._description}
	return fields
}

// C++: the `runMonica` lambda inside RunMonica::run
//
// Deliberately NOT shared with run/serve_zmq.odin's handle_env_message even
// though the two overlap heavily - they are separate functions in separate C++
// translation units (run-monica-capnp.cpp vs serve-monica-zmq.cpp) and differ in
// real ways: this one takes soil layers from a capability, has no `nodata`
// pass-through, and reports env_merge failures through `eda` rather than
// returning early. Porting each on its own is CONVENTIONS.md section 1.
run_monica_env :: proc(
	rm: ^Run_Monica,
	envJson: jx.Value,
	da: clim.Data_Accessor,
	soil_layers: jx.Value, // C++: J11Array soilLayers; a nil Value when absent
	allocator := context.allocator,
) -> mio.Output {
	env: run.Env

	// C++ installs env.params.siteParameters.calculateAndSetPwpFcSatFunctions here;
	// in this port those are selected inside central_parameter_provider_merge from
	// path_to_soil_dir, so passing the directory to env_merge is the equivalent.
	errors := run.env_merge(&env, envJson, rm.pathToSoilDir, allocator)

	// C++: if (!soilLayers.empty()) { erase "Soil profile is empty!"; merge
	// SoilProfileParameters } - stage 2 (the soil-profile capability) fills this
	// in; see the file header.
	_ = soil_layers

	out: mio.Output
	eda: tl.EResult(clim.Data_Accessor)
	eda.allocator = allocator
	// C++: eda.append(errors);
	tl.append_errors(&eda.errs, errors)

	da := da
	if clim.data_accessor_is_valid(&da) {
		eda.result = da
	} else if !clim.data_accessor_is_valid(&env.climateData) {
		// C++ reads env.climateCSV / env.pathsToClimateCSV, both dropped from this
		// port's Env - so read them straight off the incoming JSON, exactly as
		// run/serve_zmq.odin does and for the same reason (see its file header).
		csv_opts := clim.make_csv_via_header_options()
		_ = clim.csv_via_header_options_merge(&csv_opts, jx.get(envJson, "csvViaHeaderOptions"), allocator)

		climate_csv := jx.string_value_of(jx.get(envJson, "climateCSV"))
		if climate_csv != "" {
			eda = clim.read_climate_data_from_csv_string_via_headers(climate_csv, csv_opts, allocator)
		} else {
			paths := extract_paths_to_climate_csv(jx.get(envJson, "pathToClimateCSV"), allocator)
			if len(paths) > 0 {
				eda = clim.read_climate_data_from_csv_files_via_headers(paths[:], csv_opts, allocator)
			}
		}
	}

	if tl.success(eda.errs) {
		if clim.data_accessor_is_valid(&eda.result) {
			env.climateData = eda.result
		}
		env.debugMode = rm._startedServerInDebugMode && env.debugMode
		// C++ also installs env.params.userSoilMoistureParameters.getCapillaryRiseRate
		// here; this port reads the capillary-rise table directly instead.
		out = run.run_monica(&env, allocator)
	} else {
		out.customId = env.customId
	}
	out.errors = eda.errs.errors
	out.warnings = eda.errs.warnings

	return out
}

// C++: (inline, run-monica.cpp) the pathToClimateCSV -> pathsToClimateCSV
// extraction that used to live in Env::merge. Same helper as run/serve_zmq.odin's
// own private copy, which is not visible from this package.
@(private)
extract_paths_to_climate_csv :: proc(v: jx.Value, allocator := context.allocator) -> [dynamic]string {
	paths := make([dynamic]string, 0, allocator)
	if jx.is_string(v) {
		s := jx.string_value_of(v)
		if s != "" {
			append(&paths, s)
		}
	} else if jx.is_array(v) {
		for item in jx.array_items(v) {
			s := jx.string_value_of(item)
			if s != "" {
				append(&paths, s)
			}
		}
	}
	return paths
}

// C++: kj::Promise<void> RunMonica::run(RunContext context)
//
// The C++ gathers the time-series and soil-profile capabilities first and only
// then calls the lambda; with neither ported yet (see the file header) this is
// just the .then() body.
run_monica_run :: proc(
	rm: ^Run_Monica,
	params: []capnp_dyn.Field,
	allocator := context.allocator,
) -> (
	result: []capnp_dyn.Field,
	err: string,
	ok: bool,
) {
	env_value, has_env := capnp_dyn.field_get(params, "env")
	if !has_env {
		return nil, "run: missing 'env' parameter", false
	}
	env_fields, env_is_struct := env_value.([]capnp_dyn.Field)
	if !env_is_struct {
		return nil, "run: 'env' parameter is not a struct", false
	}

	// C++: auto rest = envR.getRest();  - here an AnyPointer, since RestInput is
	// an unbound generic parameter (see the file header).
	rest_value, has_rest := capnp_dyn.field_get(env_fields, "rest")
	if !has_rest {
		return nil, "run: missing 'env.rest'", false
	}
	rest_ap, rest_is_ap := rest_value.(capnp_dyn.Any_Pointer)
	if !rest_is_ap {
		return nil, "run: 'env.rest' is not an AnyPointer", false
	}
	rest, as_err, as_ok := capnp_dyn.any_pointer_as_struct(
		rest_ap,
		rm.schema.common,
		rm.schema.root,
		"StructuredText",
	)
	if !as_ok {
		return nil, fmt.tprintf("run: 'env.rest' is not a StructuredText: %s", as_err), false
	}

	out: mio.Output
	// C++: if (rest.getType() != StructuredText::Type::JSON)
	//        return makeOutput("Error: 'rest' field is not valid JSON!");
	if !structured_text_is_json(rest) {
		out = mio.make_output("Error: 'rest' field is not valid JSON!", allocator)
	} else {
		value := ""
		if v, got := capnp_dyn.field_get(rest, "value"); got {
			value, _ = v.(string)
		}
		// C++: const Json &envJson = Json::parse(rest.getValue().cStr(), err);
		pr := jx.parse_json_string(value, allocator)
		if !tl.success(pr.errs) {
			// json11 hands back a null Json plus an error string, which the C++ then
			// ignores and feeds to env_merge anyway; reporting it is strictly more
			// informative and cannot change an otherwise-successful run.
			out = mio.make_output(pr.errs.errors[0], allocator)
		} else {
			out = run_monica_env(rm, pr.result, clim.Data_Accessor{}, nil, allocator)
		}
	}

	// C++: res.setType(JSON); res.setValue(output::to_json(&out).dump());
	out_fields := make([]capnp_dyn.Field, 2, allocator)
	out_fields[0] = {
		name  = "value",
		value = jx.dump(mio.output_to_json(&out, allocator), allocator),
	}
	out_fields[1] = {name = "type", value = capnp_dyn.Enum_Value{name = "json"}}

	result_ap, from_err, from_ok := capnp_dyn.any_pointer_from_struct(
		rm.schema.common,
		rm.schema.root,
		"StructuredText",
		out_fields,
	)
	if !from_ok {
		return nil, fmt.tprintf("run: building the result StructuredText failed: %s", from_err), false
	}

	fields := make([]capnp_dyn.Field, 1, allocator)
	fields[0] = {name = "result", value = result_ap}
	return fields, "", true
}

// C++: rest.getType() == mas::schema::common::StructuredText::Type::JSON
@(private)
structured_text_is_json :: proc(st: []capnp_dyn.Field) -> bool {
	v, got := capnp_dyn.field_get(st, "type")
	if !got {
		return false
	}
	e, is_enum := v.(capnp_dyn.Enum_Value)
	return is_enum && e.name == "json"
}

// capnp_dyn.Handler for the hosted EnvInstance capability. Dispatches by method
// name the way DynamicCapability::Server::dispatchCall does for the generated C++
// class - including the methods EnvInstance inherits (info, from Identifiable).
//
// THREADING: runs on the shim's own background thread for whichever connection
// the call arrived over (see capnp_dynamic.odin), so two clients really would run
// concurrently - unlike the C++, where every call is serialized on the single KJ
// event loop thread. `lock` restores that, which the model needs anyway (the C++
// RunMonica even caches _da/_soilLayers on the object).
//
// MEMORY: everything a call allocates goes in a per-call arena that is destroyed
// at the START of the next call, not at the end of this one - the returned []Field
// is still read (and copied into the shim's own value tree) by the trampoline
// after this returns. Serialized by the same lock.
handle_call :: proc(
	user_data: rawptr,
	method_name: string,
	params: []capnp_dyn.Field,
) -> (
	result: []capnp_dyn.Field,
	err: string,
	ok: bool,
) {
	rm := (^Run_Monica)(user_data)
	sync.mutex_lock(&rm.lock)
	defer sync.mutex_unlock(&rm.lock)

	if rm.call_arena_live {
		jx.arena_destroy(&rm.call_arena)
		rm.call_arena_live = false
	}
	if !jx.arena_init(&rm.call_arena) {
		return nil, "failed to allocate the per-call arena", false
	}
	rm.call_arena_live = true
	allocator := jx.arena_allocator(&rm.call_arena)

	switch method_name {
	case "info":
		return run_monica_info(rm, allocator), "", true
	case "run":
		return run_monica_run(rm, params, allocator)
	}
	return nil, fmt.tprintf("unimplemented method '%s'", method_name), false
}
