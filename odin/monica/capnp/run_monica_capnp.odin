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
// WHY THE HANDLER IS ASYNC. `env.timeSeries` and `env.soilProfile` are
// capabilities this server has to call BACK into (climate.capnp TimeSeries,
// soil.capnp Profile). A capability received as a hosted method's own parameter
// belongs to the very connection currently dispatching that method, so a
// blocking capnp_dyn_call on it would deadlock - nothing would be left to pump
// the event loop that resolves it - and the shim refuses it outright. The only
// way through is host_async: issue the sub-calls with call_async_then and
// fulfil this call's own result from their continuations. That is structurally
// what the C++ does too (kj::joinPromises over the gather promises, then a
// .then() that runs the model), just spelled out as an explicit state machine
// (Run_Call) instead of a promise chain.
//
// NOT PORTED:
//   - save (persistence.capnp Persistent): needs a Restorer, which this MVP
//     replaces by serving RunMonica as the bootstrap capability directly.
//   - stop (service.capnp Stoppable): commented out in the C++ too.
package capnp

import "base:runtime"
import "core:encoding/uuid"
import "core:fmt"
import "core:sync"
import capnp_dyn "../../support/capnp/odin/capnp_dynamic"
import clim "../../support/climate"
import jx "../../support/jsonx"
import tl "../../support/tools"
import mio "../io"
import p "../params"
import "../run"

// Disk locations of the raw .capnp files the shim parses at runtime. There is no
// codegen step, so these are needed by the running binary, not just at build time.
Schema_Paths :: struct {
	root:        string, // zalfmas_capnp_schemas/ - import root for "/common/common.capnp" etc
	model:       string, // model/model.capnp             - declares EnvInstance
	common:      string, // common/common.capnp           - declares StructuredText
	persistence: string, // persistence/persistence.capnp - declares Restorer
}

make_schema_paths :: proc(root: string, allocator := context.allocator) -> Schema_Paths {
	return Schema_Paths {
		root = root,
		model = fmt.aprintf("%s/model/model.capnp", root, allocator = allocator),
		common = fmt.aprintf("%s/common/common.capnp", root, allocator = allocator),
		persistence = fmt.aprintf("%s/persistence/persistence.capnp", root, allocator = allocator),
	}
}

// C++: class monica::RunMonica final : public MonicaEnvInstance::Server
//
// _client/_restorer are absent: both exist only for save() (see the file header).
// _da/_soilLayers are absent too - the C++ keeps the gathered climate and soil
// data on the OBJECT, which two overlapping run() calls would trample; here they
// live on the per-call Run_Call instead.
Run_Monica :: struct {
	_startedServerInDebugMode: bool,
	_id:                       string,
	_name:                     string,
	_description:              string,

	// Not in the C++: the schema paths (there, the generated code carries the
	// schema) and the dispatch lock described on run_call_finish.
	schema:                    Schema_Paths,
	pathToSoilDir:             string,
	lock:                      sync.Mutex,
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

// ---------------------------------------------------------------------------
// RunMonica::run - the gather-then-run state machine
// ---------------------------------------------------------------------------

// One in-flight run() call. Stands in for the C++'s promise chain plus the
// _da/_soilLayers members it accumulates into, but per call rather than per
// object (see Run_Monica's comment).
//
// Owns an arena holding everything the call allocates, destroyed in
// run_call_finish once the response has been handed to the shim.
@(private)
Run_Call :: struct {
	rm:           ^Run_Monica,
	token:        capnp_dyn.Pending_Result,
	arena:        jx.Arena,
	allocator:    runtime.Allocator,

	// Decided up front, before any sub-call is issued.
	envJson:      jx.Value,
	rest_error:   string, // non-empty: respond with make_output(rest_error), no run
	ts:           capnp_dyn.Capability, // nil when env.timeSeries was absent
	profile:      capnp_dyn.Capability, // nil when env.soilProfile was absent

	// Guards everything below - the continuations can resolve on different
	// threads, one per connection the capabilities came from.
	lock:         sync.Mutex,
	remaining:    int,

	// C++: RunMonica::_da / _soilLayers, filled in by the gather promises.
	range_fields: []capnp_dyn.Field,
	header:       []capnp_dyn.Value,
	dataT:        []capnp_dyn.Value,
	soil_layers:  jx.Value,
}

// Which sub-call a continuation is completing. The C++ distinguishes these by
// having a separate lambda per promise; a C callback only carries one pointer,
// so the tag rides along with it.
@(private)
Gather_Step :: enum {
	Range,
	Header,
	DataT,
	SoilProfile,
}

@(private)
Gather_Ctx :: struct {
	call: ^Run_Call,
	step: Gather_Step,
}

// C++: kj::Promise<void> RunMonica::run(RunContext context)
//
// Returns .Pending unconditionally, including when there is nothing to gather:
// fulfilling from here rather than returning the fields directly is what lets
// run_call_finish free the call's arena immediately (pending_result_fulfill
// copies into the shim's own value tree), instead of leaking it until some later
// call cleans up.
@(private)
run_monica_run :: proc(
	rm: ^Run_Monica,
	params: []capnp_dyn.Field,
	token: capnp_dyn.Pending_Result,
) -> (
	err: string,
	ok: bool,
) {
	call := new(Run_Call)
	call.rm = rm
	call.token = token
	if !jx.arena_init(&call.arena) {
		free(call)
		return "failed to allocate the per-call arena", false
	}
	call.allocator = jx.arena_allocator(&call.arena)

	env_value, has_env := capnp_dyn.field_get(params, "env")
	if !has_env {
		run_call_abort(call, "run: missing 'env' parameter")
		return "", true
	}
	env_fields, env_is_struct := env_value.([]capnp_dyn.Field)
	if !env_is_struct {
		run_call_abort(call, "run: 'env' parameter is not a struct")
		return "", true
	}

	// C++: auto rest = envR.getRest();  - here an AnyPointer, since RestInput is
	// an unbound generic parameter (see the file header).
	rest_value, has_rest := capnp_dyn.field_get(env_fields, "rest")
	if !has_rest {
		run_call_abort(call, "run: missing 'env.rest'")
		return "", true
	}
	rest_ap, rest_is_ap := rest_value.(capnp_dyn.Any_Pointer)
	if !rest_is_ap {
		run_call_abort(call, "run: 'env.rest' is not an AnyPointer")
		return "", true
	}
	rest, as_err, as_ok := capnp_dyn.any_pointer_as_struct(
		rest_ap,
		rm.schema.common,
		rm.schema.root,
		"StructuredText",
	)
	if !as_ok {
		run_call_abort(
			call,
			fmt.aprintf("run: 'env.rest' is not a StructuredText: %s", as_err, allocator = call.allocator),
		)
		return "", true
	}

	// C++: if (rest.getType() != StructuredText::Type::JSON)
	//        return makeOutput("Error: 'rest' field is not valid JSON!");
	if !structured_text_is_json(rest) {
		call.rest_error = "Error: 'rest' field is not valid JSON!"
	} else {
		value := ""
		if v, got := capnp_dyn.field_get(rest, "value"); got {
			value, _ = v.(string)
		}
		// C++: const Json &envJson = Json::parse(rest.getValue().cStr(), err);
		pr := jx.parse_json_string(value, call.allocator)
		if !tl.success(pr.errs) {
			// json11 hands back a null Json plus an error string, which the C++
			// then ignores and feeds to env_merge anyway; reporting it is strictly
			// more informative and cannot change an otherwise-successful run.
			call.rest_error = pr.errs.errors[0]
		} else {
			call.envJson = pr.result
		}
	}

	// C++: if (envR.hasTimeSeries()) ... if (envR.hasSoilProfile()) ...
	// An absent capability field reads back as a null handle rather than being
	// missing, so "has" is a nil check.
	if v, got := capnp_dyn.field_get(env_fields, "timeSeries"); got {
		if c, is_cap := v.(capnp_dyn.Capability); is_cap && c != nil {
			call.ts = c
		}
	}
	if v, got := capnp_dyn.field_get(env_fields, "soilProfile"); got {
		if c, is_cap := v.(capnp_dyn.Capability); is_cap && c != nil {
			call.profile = c
		}
	}

	// C++: kj::heapArrayBuilder<kj::Promise<void>>(2) - one gather promise per
	// capability, joined before the model runs. dataAccessorFromTimeSeries sends
	// range/header/dataT together rather than in sequence, so three of the four
	// steps belong to the time series.
	if call.ts != nil {
		call.remaining += 3
	}
	if call.profile != nil {
		call.remaining += 1
	}
	if call.remaining == 0 {
		run_call_finish(call)
		return "", true
	}

	started := 0
	if call.ts != nil {
		started += run_call_start(call, call.ts, "range", .Range)
		started += run_call_start(call, call.ts, "header", .Header)
		started += run_call_start(call, call.ts, "dataT", .DataT)
	}
	if call.profile != nil {
		started += run_call_start(call, call.profile, "data", .SoilProfile)
	}
	// Every sub-call that failed to even start was already counted down by
	// run_call_start, so if they all failed the last one has finished the call.
	_ = started
	return "", true
}

// Issues one gather sub-call. Returns 1 if it started, 0 if it failed outright -
// in which case it has already counted itself down (C++: the per-promise
// errback, which logs and leaves the corresponding data empty).
@(private)
run_call_start :: proc(
	call: ^Run_Call,
	cap: capnp_dyn.Capability,
	method: string,
	step: Gather_Step,
) -> int {
	ctx := new(Gather_Ctx)
	ctx.call = call
	ctx.step = step

	_, err, ok := capnp_dyn.call_async_then(cap, method, nil, run_call_continue, ctx)
	if !ok {
		// C++: KJ_LOG(INFO, "Error while trying to get data accessor from time
		// series: ", e) / "Error while trying to get soil layers: "
		fmt.eprintfln("monica-capnp-server: could not start '%s': %s", method, err)
		free(ctx)
		run_call_step_done(call)
		return 0
	}
	return 1
}

// capnp_dyn.Continuation for every gather sub-call. Runs on whichever thread
// resolved it (the dispatching connection's, in practice).
@(private)
run_call_continue :: proc(user_data: rawptr, pending: capnp_dyn.Pending_Call) {
	ctx := (^Gather_Ctx)(user_data)
	call := ctx.call
	step := ctx.step
	free(ctx)
	defer capnp_dyn.pending_call_release(pending)

	// call_take_result allocates through context.allocator, so the arena has to be
	// installed for exactly that call - and put back straight after. The ported
	// MONICA code below is written the way run/serve_zmq.odin calls it, with the
	// arena passed EXPLICITLY and context.allocator left as the ordinary heap;
	// making the arena ambient instead corrupts the heap, because code that
	// allocates through the context but frees through an explicitly passed
	// allocator (or the reverse) then crosses the two. Found the hard way - it
	// only crashed once free_params gave the heap something to trip over.
	previous := context.allocator
	context.allocator = call.allocator
	fields, err, ok := capnp_dyn.call_take_result(pending)
	context.allocator = previous
	if !ok {
		// C++: each gather promise has its own errback that logs and yields an
		// empty DataAccessor / J11Array, so a failure here degrades the run
		// rather than failing it.
		fmt.eprintfln("monica-capnp-server: gather step %v failed: %s", step, err)
		run_call_step_done(call)
		return
	}

	sync.mutex_lock(&call.lock)
	switch step {
	case .Range:
		call.range_fields = fields
	case .Header:
		if v, got := capnp_dyn.field_get(fields, "header"); got {
			call.header, _ = v.([]capnp_dyn.Value)
		}
	case .DataT:
		if v, got := capnp_dyn.field_get(fields, "data"); got {
			call.dataT, _ = v.([]capnp_dyn.Value)
		}
	case .SoilProfile:
		// C++: fromCapnpSoilProfile's .then() body.
		call.soil_layers = soil_layers_from_capnp_profile_data(fields, call.allocator)
	}
	sync.mutex_unlock(&call.lock)

	run_call_step_done(call)
}

// C++: the join point - kj::joinPromises(proms.finish()).then(...)
@(private)
run_call_step_done :: proc(call: ^Run_Call) {
	sync.mutex_lock(&call.lock)
	call.remaining -= 1
	last := call.remaining == 0
	sync.mutex_unlock(&call.lock)
	if last {
		run_call_finish(call)
	}
}

// Responds without running the model at all - the malformed-parameters cases,
// which the C++ does not have (its generated code would have failed to decode
// long before reaching RunMonica::run).
@(private)
run_call_abort :: proc(call: ^Run_Call, message: string) {
	call.rest_error = message
	run_call_finish(call)
}

// C++: the body of the final .then() - run the model, serialise, respond.
//
// THREADING: reached from whichever thread resolved the last gather sub-call,
// or directly from the handler when there was nothing to gather. rm.lock
// serialises the model run itself, which the C++ gets for free by living on a
// single KJ event loop thread.
@(private)
run_call_finish :: proc(call: ^Run_Call) {
	out: mio.Output
	if call.rest_error != "" {
		out = mio.make_output(call.rest_error, call.allocator)
	} else {
		// C++: fromCapnpData(Date(sd), Date(ed), header, data) inside
		// dataAccessorFromTimeSeries' innermost .then().
		da: clim.Data_Accessor
		if call.range_fields != nil && call.header != nil && call.dataT != nil {
			sd, has_sd := capnp_dyn.field_get(call.range_fields, "startDate")
			ed, has_ed := capnp_dyn.field_get(call.range_fields, "endDate")
			sd_fields, sd_ok := sd.([]capnp_dyn.Field)
			ed_fields, ed_ok := ed.([]capnp_dyn.Field)
			if has_sd && has_ed && sd_ok && ed_ok {
				da = from_capnp_data(
					date_from_capnp(sd_fields),
					date_from_capnp(ed_fields),
					call.header,
					call.dataT,
					call.allocator,
				)
			}
		}

		sync.mutex_lock(&call.rm.lock)
		// Each request is an independent run, and this call's arena (destroyed in
		// run_call_destroy) is what the previous one's cached values live in - so
		// drop them before they become dangling. Without this the server died on
		// its SECOND request; see run.reset_caches' own comment. Under rm.lock,
		// since the cache is process-global.
		run.reset_caches()
		out = run_monica_env(call.rm, call.envJson, da, call.soil_layers, call.allocator)
		sync.mutex_unlock(&call.rm.lock)
	}

	// C++: res.setType(JSON); res.setValue(output::to_json(&out).dump());
	out_fields := make([]capnp_dyn.Field, 2, call.allocator)
	out_fields[0] = {
		name  = "value",
		value = jx.dump(mio.output_to_json(&out, call.allocator), call.allocator),
	}
	out_fields[1] = {name = "type", value = capnp_dyn.Enum_Value{name = "json"}}

	result_ap, from_err, from_ok := capnp_dyn.any_pointer_from_struct(
		call.rm.schema.common,
		call.rm.schema.root,
		"StructuredText",
		out_fields,
	)
	if !from_ok {
		capnp_dyn.pending_result_fulfill(
			call.token,
			nil,
			fmt.tprintf("run: building the result StructuredText failed: %s", from_err),
		)
		run_call_destroy(call)
		return
	}

	fields := make([]capnp_dyn.Field, 1, call.allocator)
	fields[0] = {name = "result", value = result_ap}
	// Copies into the shim's own value tree, so the arena can go immediately after.
	capnp_dyn.pending_result_fulfill(call.token, fields)
	capnp_dyn.any_pointer_free(result_ap)
	run_call_destroy(call)
}

@(private)
run_call_destroy :: proc(call: ^Run_Call) {
	// The capabilities came in as this call's own parameters and stay valid past
	// the handler returning (the shim keeps the dispatching connection alive for
	// them), but they are independently owned - so this is where they go.
	if call.ts != nil {
		capnp_dyn.release_capability(call.ts)
	}
	if call.profile != nil {
		capnp_dyn.release_capability(call.profile)
	}
	jx.arena_destroy(&call.arena)
	free(call)
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

	// C++: if (!soilLayers.empty()) { erase "Soil profile is empty!"; append
	//        siteparameters::merge(..., {{"SoilProfileParameters", soilLayers}}) }
	if jx.is_array(soil_layers) && len(jx.array_items(soil_layers)) > 0 {
		for e, i in errors.errors {
			if e == "Soil profile is empty!" {
				ordered_remove(&errors.errors, i)
				break
			}
		}
		tl.append_errors(
			&errors,
			p.site_parameters_merge(
				&env.params.siteParameters,
				jx.obj(allocator, {"SoilProfileParameters", soil_layers}),
				rm.pathToSoilDir,
				allocator,
			),
		)
	}

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
	// C++: out.errors = eda.errors; - appended rather than assigned, because
	// run_monica can now put an error there itself (the empty-soil-profile guard;
	// see its comment in run_monica.odin) and assigning would drop it, leaving the
	// client with an empty result and no reason. The C++ can assign safely only
	// because runMonica there never sets Output.errors at all.
	if out.errors == nil {
		out.errors = make([dynamic]string, allocator)
	}
	if out.warnings == nil {
		out.warnings = make([dynamic]string, allocator)
	}
	append(&out.errors, ..eda.errs.errors[:])
	append(&out.warnings, ..eda.errs.warnings[:])

	return out
}

// The whole of RunMonica::run - reading `rest`, gathering the time series and
// soil profile, and running the model - but driven with BLOCKING sub-calls
// instead of the promise chain run_monica_run sets up.
//
// For monica-capnp-fbp-component's inline MONICA. The async machinery exists
// because a hosted method may not block on a capability belonging to the
// connection dispatching it; here nothing is being dispatched - the component is
// an ordinary client, on its own thread, and the capabilities belong to some
// channel's connection, whose event loop runs elsewhere. So the plain blocking
// path applies, and it is much easier to follow.
//
// `env` is a model.capnp Env struct as read off an FBP IP, i.e. the same shape
// run_monica_run finds under its "env" parameter.
run_monica_env_blocking :: proc(
	rm: ^Run_Monica,
	env: []capnp_dyn.Field,
	allocator := context.allocator,
) -> mio.Output {
	// C++: auto rest = envR.getRest();
	rest_value, has_rest := capnp_dyn.field_get(env, "rest")
	if !has_rest {
		return mio.make_output("Error: env has no 'rest' field!", allocator)
	}
	rest_ap, rest_is_ap := rest_value.(capnp_dyn.Any_Pointer)
	if !rest_is_ap {
		return mio.make_output("Error: 'rest' field is not an AnyPointer!", allocator)
	}
	rest, as_err, as_ok := capnp_dyn.any_pointer_as_struct(
		rest_ap,
		rm.schema.common,
		rm.schema.root,
		"StructuredText",
	)
	if !as_ok {
		return mio.make_output(
			fmt.aprintf("Error: 'rest' is not a StructuredText: %s", as_err, allocator = allocator),
			allocator,
		)
	}
	if !structured_text_is_json(rest) {
		return mio.make_output("Error: 'rest' field is not valid JSON!", allocator)
	}
	value := ""
	if v, got := capnp_dyn.field_get(rest, "value"); got {
		value, _ = v.(string)
	}
	pr := jx.parse_json_string(value, allocator)
	if !tl.success(pr.errs) {
		return mio.make_output(pr.errs.errors[0], allocator)
	}

	// C++: if (envR.hasTimeSeries()) proms.add(dataAccessorFromTimeSeries(...))
	da: clim.Data_Accessor
	if ts, has_ts := capability_field(env, "timeSeries"); has_ts {
		da = data_accessor_from_time_series(ts, allocator)
	}

	// C++: if (envR.hasSoilProfile()) proms.add(fromCapnpSoilProfile(...))
	soil_layers: jx.Value
	if profile, has_profile := capability_field(env, "soilProfile"); has_profile {
		if data, _, ok := capnp_dyn.call(profile, "data", nil); ok {
			soil_layers = soil_layers_from_capnp_profile_data(data, allocator)
		} else {
			// C++: KJ_LOG(ERROR, "Error while trying to get soil profile data.") and
			// carry on with an empty J11Array.
			fmt.eprintfln("monica: could not read the soil profile capability")
		}
	}

	return run_monica_env(rm, pr.result, da, soil_layers, allocator)
}

// C++: dataAccessorFromTimeSeries - range/header/dataT, then fromCapnpData.
//
// The C++ sends all three requests before waiting on any of them; these are
// sequential, so three round trips instead of one. Worth revisiting if a remote
// climate service ever makes that matter - the shim's call_async would allow it.
@(private)
data_accessor_from_time_series :: proc(
	ts: capnp_dyn.Capability,
	allocator := context.allocator,
) -> clim.Data_Accessor {
	// Each failure below is logged and yields an empty DataAccessor, matching the
	// C++'s per-promise errbacks.
	range_fields, range_err, range_ok := capnp_dyn.call(ts, "range", nil)
	if !range_ok {
		fmt.eprintfln("monica: error while trying to get range data: %s", range_err)
		return {}
	}
	header_res, header_err, header_ok := capnp_dyn.call(ts, "header", nil)
	if !header_ok {
		fmt.eprintfln("monica: error while trying to get header data: %s", header_err)
		return {}
	}
	data_res, data_err, data_ok := capnp_dyn.call(ts, "dataT", nil)
	if !data_ok {
		fmt.eprintfln("monica: error while trying to get transposed time series data: %s", data_err)
		return {}
	}

	sd, has_sd := capnp_dyn.field_get(range_fields, "startDate")
	ed, has_ed := capnp_dyn.field_get(range_fields, "endDate")
	sd_fields, sd_ok := sd.([]capnp_dyn.Field)
	ed_fields, ed_ok := ed.([]capnp_dyn.Field)
	if !has_sd || !has_ed || !sd_ok || !ed_ok {
		return {}
	}

	header: []capnp_dyn.Value
	if v, got := capnp_dyn.field_get(header_res, "header"); got {
		header, _ = v.([]capnp_dyn.Value)
	}
	dataT: []capnp_dyn.Value
	if v, got := capnp_dyn.field_get(data_res, "data"); got {
		dataT, _ = v.([]capnp_dyn.Value)
	}

	return from_capnp_data(
		date_from_capnp(sd_fields),
		date_from_capnp(ed_fields),
		header,
		dataT,
		allocator,
	)
}

// C++: envR.hasTimeSeries() / hasSoilProfile() - an absent capability field
// reads back as a null handle rather than being missing.
@(private)
capability_field :: proc(
	fields: []capnp_dyn.Field,
	name: string,
) -> (
	cap: capnp_dyn.Capability,
	ok: bool,
) {
	v, has := capnp_dyn.field_get(fields, name)
	if !has {
		return nil, false
	}
	c, is_cap := v.(capnp_dyn.Capability)
	if !is_cap || c == nil {
		return nil, false
	}
	return c, true
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

// capnp_dyn.Async_Handler for the hosted EnvInstance capability. Dispatches by
// method name the way DynamicCapability::Server::dispatchCall does for the
// generated C++ class - including the methods EnvInstance inherits (info, from
// Identifiable).
//
// THREADING: runs on the shim's own background thread for whichever connection
// the call arrived over (see capnp_dynamic.odin), and must return promptly - all
// the real work happens in the continuations run_monica_run sets up.
handle_call :: proc(
	user_data: rawptr,
	method_name: string,
	params: []capnp_dyn.Field,
	token: capnp_dyn.Pending_Result,
) -> (
	result: []capnp_dyn.Field,
	err: string,
	ok: bool,
	outcome: capnp_dyn.Call_Outcome,
) {
	rm := (^Run_Monica)(user_data)
	// The shim's trampoline converts `params` into Odin values with the default
	// heap allocator and never frees them; released here, once everything worth
	// keeping has been copied into the call's own arena. Capability handles are
	// deliberately left alone - run() keeps them past this return.
	defer free_params(params)

	switch method_name {
	case "info":
		// Synchronous, and small enough that the temp allocator covers it - the
		// trampoline copies the fields into the shim's tree before returning.
		return run_monica_info(rm, context.temp_allocator), "", true, .Done
	case "run":
		err, ok := run_monica_run(rm, params, token)
		if !ok {
			return nil, err, false, .Done
		}
		return nil, "", true, .Pending
	}
	return nil, fmt.tprintf("unimplemented method '%s'", method_name), false, .Done
}

// Frees the Odin-side memory of a params tree produced by the shim's
// c_async_trampoline (fields_from_raw allocates strings, slices and AnyPointer
// handles through context.allocator and nothing frees them, so a long-running
// server would otherwise leak a full copy of every env it is sent).
//
// Capability handles are NOT touched: they are plain handles the caller decides
// the lifetime of, and run() holds on to them past the handler returning.
@(private)
free_params :: proc(fields: []capnp_dyn.Field) {
	for f in fields {
		delete(f.name)
		free_param_value(f.value)
	}
	delete(fields)
}

@(private)
free_param_value :: proc(v: capnp_dyn.Value) {
	#partial switch x in v {
	case string:
		delete(x)
	case []u8:
		delete(x)
	case capnp_dyn.Enum_Value:
		delete(x.name)
	case []capnp_dyn.Value:
		for item in x {
			free_param_value(item)
		}
		delete(x)
	case []capnp_dyn.Field:
		free_params(x)
	case capnp_dyn.Any_Pointer:
		capnp_dyn.any_pointer_free(x)
	}
}
