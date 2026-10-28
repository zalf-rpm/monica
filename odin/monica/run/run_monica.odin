// Phase 7 checkpoint 4: src/run/run-monica.h/.cpp.
//
// Ports only the non-Intercropping half of runMonicaIC (the C++ isIC=false
// path): CropRotation/Env/Spec/StoreData, setupStorage, and a genuinely
// single-model runMonica rather than isIC-branch-pruning of the dual-model
// C++ function. Dropped entirely, all "port on demand": the second
// MonicaModel and every isSyncIC/Intercropping branch; Cap'n Proto state
// load-at-start/save-at-end (loadSerializedMonicaStateAtStart,
// serializeMonicaStateAtEnd); the daily-function registration loop
// (workstep::registerDailyFunction/applyDailyFuncs) - the only workstep
// needing it, AutomaticSowing, is unused by crop-min.json; env_to_json/
// env_to_string/spec_to_json/crop_rotation_to_json (debug/RPC dumps);
// writeDebugInputs. climateCSV/pathsToClimateCSV/csvViaHeaderOptions/
// sharedId/berestRequestAddress are dropped from Env too - runMonicaIC's own
// body never reads them (pathsToClimateCSV only feeds the Cap'n Proto
// sturdy-ref climate-merging branch in monica-run-main.cpp), and
// create_env_json.odin's create_env_json_from_json_objects already resolves
// climateData to a complete DataAccessor before env_merge ever runs.
package run

import clim "../../support/climate"
import d "../../support/date"
import jx "../../support/jsonx"
import tl "../../support/tools"
import core "../core"
import mio "../io"
import p "../params"
import "core:strconv"
import "core:strings"

// ---------------------------------------------------------------------------
// CropRotation
// ---------------------------------------------------------------------------

// C++: struct monica::CropRotation
Crop_Rotation :: struct {
	start:         d.Date,
	end:           d.Date,
	crop_rotation: [dynamic]Cultivation_Method,
}

// C++: CropRotation monica::makeCropRotation(Date, Date, vector<CultivationMethod>)
make_crop_rotation :: proc(
	start, end: d.Date,
	crop_rotation: [dynamic]Cultivation_Method,
) -> Crop_Rotation {
	return Crop_Rotation{start = start, end = end, crop_rotation = crop_rotation}
}

// C++: Errors monica::extractAndStore(const Json&, vector<CultivationMethod>&) [private]
@(private)
extract_and_store :: proc(
	jv: jx.Value,
	vec: ^[dynamic]Cultivation_Method,
	allocator := context.allocator,
) -> tl.Errors {
	es: tl.Errors
	clear(vec)
	for cmj in jx.array_items(jv) {
		v := Cultivation_Method {
			repeat = true,
		}
		tl.append_errors(&es, cultivation_method_merge(&v, cmj, allocator))
		append(vec, v)
	}
	return es
}

// C++: Errors monica::extractAndStoreCropRotations(const Json&, vector<CropRotation>&) [private]
@(private)
extract_and_store_crop_rotations :: proc(
	jv: jx.Value,
	vec: ^[dynamic]Crop_Rotation,
	allocator := context.allocator,
) -> tl.Errors {
	es: tl.Errors
	clear(vec)
	for crj in jx.array_items(jv) {
		v: Crop_Rotation
		tl.append_errors(&es, crop_rotation_merge(&v, crj, allocator))
		append(vec, v)
	}
	return es
}

// C++: Errors monica::crop_rotation_merge(CropRotation*, Json)
crop_rotation_merge :: proc(
	cr: ^Crop_Rotation,
	j: jx.Value,
	allocator := context.allocator,
) -> tl.Errors {
	es: tl.Errors
	jx.set_iso_date_value(&cr.start, j, "start")
	jx.set_iso_date_value(&cr.end, j, "end")
	tl.append_errors(
		&es,
		extract_and_store(jx.get(j, "cropRotation"), &cr.crop_rotation, allocator),
	)
	return es
}

// ---------------------------------------------------------------------------
// Env
// ---------------------------------------------------------------------------

// C++: struct monica::Env - see the file header comment for what's dropped.
Env :: struct {
	climate_data:   clim.Data_Accessor,
	crop_rotation:  [dynamic]Cultivation_Method,
	crop_rotations: [dynamic]Crop_Rotation,
	events:         jx.Value,
	outputs:        jx.Value,
	custom_id:      jx.Value,
	params:         p.Central_Parameter_Provider,
	debug_mode:     bool,
}

// C++: Env monica::makeEnv(CentralParameterProvider&&)
make_env :: proc(cpp: p.Central_Parameter_Provider, allocator := context.allocator) -> Env {
	env: Env
	env.params = cpp
	env.climate_data = clim.make_data_accessor(allocator)
	return env
}

// C++: Errors monica::env_merge(Env*, Json)
//
// path_to_soil_dir is threaded through to central_parameter_provider_merge -
// see its own doc comment (site_sim_parameters.odin) for why this differs
// from the C++ signature.
env_merge :: proc(
	env: ^Env,
	j: jx.Value,
	path_to_soil_dir: string,
	allocator := context.allocator,
) -> tl.Errors {
	res: tl.Errors

	// C++'s `Env env;` default-constructs `CentralParameterProvider params`
	// with its own in-class member defaults (Site_Parameters.latitude=52.5
	// etc - see site_sim_parameters.odin); a zero-value Odin Central_Parameter_Provider{}
	// lacks those, so callers starting from a zero Env need this run explicitly.
	env.params = p.make_central_parameter_provider(allocator)

	tl.append_errors(
		&res,
		p.central_parameter_provider_merge(
			&env.params,
			jx.get(j, "params"),
			path_to_soil_dir,
			allocator,
		),
	)

	env.climate_data = clim.make_data_accessor(allocator)
	tl.append_errors(
		&res,
		clim.data_accessor_merge(&env.climate_data, jx.get(j, "climateData"), allocator),
	)

	env.events = jx.get(j, "events")
	env.outputs = jx.get(j, "outputs")

	tl.append_errors(
		&res,
		extract_and_store(jx.get(j, "cropRotation"), &env.crop_rotation, allocator),
	)
	tl.append_errors(
		&res,
		extract_and_store_crop_rotations(
			jx.get(j, "cropRotations"),
			&env.crop_rotations,
			allocator,
		),
	)

	jx.set_bool_value(&env.debug_mode, j, "debugMode")

	env.custom_id = jx.get(j, "customId")

	return res
}

// C++: bool monica::env_return_obj_outputs(const Env*)
env_return_obj_outputs :: proc(env: ^Env) -> bool {
	return jx.bool_value_of(jx.get(env.outputs, "obj-outputs?"))
}

// Not in the C++: the A/B bisection switch for the reflection-driven output
// tier (sim.json: output."use-legacy-output-fns?"). See
// plan-reflective-outputs.md §2.5 - when a CSV column moves, this is what
// pins it to the engine rather than to anything else in the same commit.
env_use_legacy_output_fns :: proc(env: ^Env) -> bool {
	return jx.bool_value_of(jx.get(env.outputs, "use-legacy-output-fns?"))
}

// ---------------------------------------------------------------------------
// Spec
// ---------------------------------------------------------------------------

Spec_Expr_Kind :: enum {
	NONE,
	DATE,
	EVENT,
}

// C++: std::function<bool(const MonicaModel&)>, materialised as data plus a
// single dispatcher (spec_expr_eval) instead of a closure. Odin procs can't
// capture; unlike the "one live model" global-pointer workaround used
// elsewhere in this port (monica_model.odin's g_current_model), here many
// differently-parameterised expressions coexist at once (each shortcut's
// date pattern, each workstep event name), so there is no single global to
// redirect calls through - the captured data has to live somewhere, and a
// plain struct is the direct Odin equivalent.
Spec_Expr :: struct {
	kind:       Spec_Expr_Kind,
	day:        Maybe(int),
	month:      Maybe(int),
	year:       Maybe(int),
	event_name: string,
}

spec_expr_is_set :: proc(e: Spec_Expr) -> bool {
	return e.kind != .NONE
}

// C++: struct monica::Spec
Spec :: struct {
	orig_spec: jx.Value,
	startf:    Spec_Expr,
	endf:      Spec_Expr,
	fromf:     Spec_Expr,
	tof:       Spec_Expr,
	atf:       Spec_Expr,
	whilef:    Spec_Expr,
}

// C++: Spec monica::makeSpec(Json)
make_spec :: proc(j: jx.Value) -> Spec {
	spec: Spec
	spec_merge(&spec, j)
	return spec
}

// C++: Errors monica::spec_merge(Spec*, Json)
spec_merge :: proc(spec: ^Spec, j: jx.Value) -> tl.Errors {
	spec.startf = spec_create_expression_func(jx.get(j, "start"))
	spec.endf = spec_create_expression_func(jx.get(j, "end"))
	spec.atf = spec_create_expression_func(jx.get(j, "at"))
	spec.fromf = spec_create_expression_func(jx.get(j, "from"))
	spec.tof = spec_create_expression_func(jx.get(j, "to"))
	spec.whilef = spec_create_expression_func(jx.get(j, "while"))
	return {}
}

// C++: template<typename T> Maybe<T> parseInt(const string&) [private]
@(private)
parse_date_component :: proc(s: string) -> Maybe(int) {
	if len(s) == 0 || (len(s) > 1 && s[0:2] == "xx") {
		return nil
	}
	v, ok := strconv.parse_int(s)
	if !ok {
		return nil
	}
	return v
}

// C++: std::function<bool(const MonicaModel&)> monica::spec_create_expression_func(Json)
//
// The j.is_array() branch (buildCompareExpression, the ["while"|"at", [oid,
// "op", value]] comparison-expression syntax) is not ported: grepped
// sim-min.json's "events" section (the "_events"/"__events" siblings are
// underscore-disabled and never read by create_env_json_from_json_objects)
// and confirmed every spec is a shortcut string, a workstep-event-name
// string, or a plain output-id array - never that syntax. "Port on demand".
spec_create_expression_func :: proc(j: jx.Value) -> Spec_Expr {
	if jx.is_array(j) {
		return Spec_Expr{kind = .NONE}
	} else if jx.is_string(j) {
		jts := jx.string_value_of(j)
		if jts != "" {
			s := strings.split(jts, "-", context.temp_allocator)
			// is date event
			if len(jts) == 10 &&
			   len(s) == 3 &&
			   len(s[0]) == 4 &&
			   len(s[1]) == 2 &&
			   len(s[2]) == 2 {
				year := parse_date_component(s[0])
				month := parse_date_component(s[1])
				day := parse_date_component(s[2])
				return Spec_Expr{kind = .DATE, day = day, month = month, year = year}
			} else { 	// treat all other strings as potential workstep event
				return Spec_Expr{kind = .EVENT, event_name = jts}
			}
		}
	}
	return Spec_Expr{kind = .NONE}
}

// C++: the lambda bodies inside spec_create_expression_func's date-string and
// workstep-event-name branches.
spec_expr_eval :: proc(e: Spec_Expr, model: ^core.Monica_Model) -> bool {
	switch e.kind {
	case .DATE:
		cd := model.current_step_date
		// apply min() for day, to allow matching of the last day for each month
		// by choosing 31st
		day := d.day(cd)
		if dv, ok := e.day.?; ok {
			day = min(u8(dv), d.days_in_month(cd))
		}
		month := d.month(cd)
		if mv, ok := e.month.?; ok {
			month = u8(mv)
		}
		year := d.year(cd)
		if yv, ok := e.year.?; ok {
			year = yv
		}
		date := d.make_date(day, month, u16(year), false, true)
		return d.eq(date, cd)
	case .EVENT:
		_, ok := model.current_events[e.event_name]
		return ok
	case .NONE:
	}
	return false
}

// ---------------------------------------------------------------------------
// StoreData
// ---------------------------------------------------------------------------

// C++: struct monica::StoreData
Store_Data :: struct {
	within_event_start_end_range: Maybe(bool),
	within_event_from_to_range:   Maybe(bool),
	spec:                         Spec,
	output_ids:                   [dynamic]mio.OId,
	intermediate_results:         [dynamic][dynamic]jx.Value,
	results:                      [dynamic][dynamic]jx.Value,
	results_obj:                  [dynamic]jx.Value,
}

// C++: void storeResults(const vector<OId>&, vector<J11Array>&, const MonicaModel&) [private]
@(private)
store_results :: proc(
	output_ids: []mio.OId,
	results: ^[dynamic][dynamic]jx.Value,
	model: ^core.Monica_Model,
) {
	if len(results^) < len(output_ids) {
		resize(results, len(output_ids))
	}
	for oid, i in output_ids {
		// Reflection-backed oids (an alias or a raw path in sim.json) resolve
		// their value by walking a plan compiled at setup; everything else is
		// still a registered lambda. oid_get_value picks
		// (plan-reflective-outputs.md §2.5), and reports false only when the
		// oid has neither, in which case the C++ appends nothing either.
		if v, ok := mio.oid_get_value(model, oid); ok {
			append(&results^[i], v)
		}
	}
}

// C++: void storeResultsObj(const vector<OId>&, vector<J11Object>&, const MonicaModel&) [private]
@(private)
store_results_obj :: proc(
	output_ids: []mio.OId,
	results: ^[dynamic]jx.Value,
	model: ^core.Monica_Model,
	allocator := context.allocator,
) {
	result := make(jx.Object, allocator)
	for &oid in output_ids {
		if v, ok := mio.oid_get_value(model, oid); ok {
			jx.obj_set(&result, mio.oid_output_name(&oid, context.temp_allocator), v, allocator)
		}
	}
	append(results, jx.Value(result))
}

// C++: void monica::store_data_aggregate_results(StoreData*)
store_data_aggregate_results :: proc(sd: ^Store_Data, allocator := context.allocator) {
	if len(sd.intermediate_results) == 0 {
		return
	}
	if len(sd.results) < len(sd.intermediate_results) {
		resize(&sd.results, len(sd.intermediate_results))
	}

	for oid, i in sd.output_ids {
		ivs := sd.intermediate_results[i]
		if len(ivs) > 0 {
			if jx.is_string(ivs[0]) {
				switch oid.time_agg_op {
				case .FIRST:
					append(&sd.results[i], ivs[0])
				case .LAST:
					append(&sd.results[i], ivs[len(ivs) - 1])
				case .AVG, .MEDIAN, .SUM, .MIN, .MAX, .NONE, .UNDEFINED_OP:
					append(&sd.results[i], ivs[0])
				}
			} else {
				append(&sd.results[i], mio.apply_oid_op_json(oid.time_agg_op, ivs[:], allocator))
			}
			clear(&sd.intermediate_results[i])
		}
	}
}

// C++: void monica::store_data_aggregate_results_obj(StoreData*)
//
// Same aggregation as store_data_aggregate_results, collected into one object
// per call instead of appended per column.
//
// NOTE(c++-quirk): the object is appended even when every intermediate list
// was empty. run_monica's final aggregation pass therefore adds one empty
// row ({}) to every while / from-to section whose last range was already
// aggregated, which the array path doesn't. Reproduced.
store_data_aggregate_results_obj :: proc(sd: ^Store_Data, allocator := context.allocator) {
	if len(sd.intermediate_results) == 0 {
		return
	}

	result := make(jx.Object, allocator)
	for &oid, i in sd.output_ids {
		ivs := sd.intermediate_results[i]
		if len(ivs) > 0 {
			name := mio.oid_output_name(&oid, context.temp_allocator)
			if jx.is_string(ivs[0]) {
				switch oid.time_agg_op {
				case .FIRST:
					jx.obj_set(&result, name, ivs[0], allocator)
				case .LAST:
					jx.obj_set(&result, name, ivs[len(ivs) - 1], allocator)
				case .AVG, .MEDIAN, .SUM, .MIN, .MAX, .NONE, .UNDEFINED_OP:
					jx.obj_set(&result, name, ivs[0], allocator)
				}
			} else {
				jx.obj_set(&result, name, mio.apply_oid_op_json(oid.time_agg_op, ivs[:], allocator), allocator)
			}
			clear(&sd.intermediate_results[i])
		}
	}
	append(&sd.results_obj, jx.Value(result))
}

@(private)
maybe_bool_is_nothing :: proc(m: Maybe(bool)) -> bool {
	_, ok := m.?
	return !ok
}

@(private)
maybe_bool_is_value :: proc(m: Maybe(bool)) -> bool {
	_, ok := m.?
	return ok
}

@(private)
maybe_bool_value :: proc(m: Maybe(bool)) -> bool {
	v, _ := m.?
	return v
}

// C++: void monica::store_data_store_results_if_spec_applies(StoreData*, const MonicaModel&, bool)
//
// Dropped: `string os = spec.origSpec.dump();` - a local that's computed and
// never read anywhere in the C++ body either (dump() is pure, so omitting it
// changes nothing observable).
store_data_store_results_if_spec_applies :: proc(
	sd: ^Store_Data,
	model: ^core.Monica_Model,
	store_obj_outputs := false,
) {
	spec := &sd.spec
	is_currently_end_event := false

	// check for possible start event (if one exists at all; enter only if false)
	if maybe_bool_is_nothing(sd.within_event_start_end_range) ||
	   !maybe_bool_value(sd.within_event_start_end_range) {
		if spec_expr_is_set(spec.startf) {
			sd.within_event_start_end_range = spec_expr_eval(spec.startf, model)
		}
	}

	// check for end event (doesn't need a start event, but if there was one at
	// all, it has to be true)
	//
	// NOTE(c++-quirk): `isNothing() || isValue()` is a tautology for any
	// Maybe(bool) - reproduced as-is rather than simplified to `true`.
	if maybe_bool_is_nothing(sd.within_event_start_end_range) ||
	   maybe_bool_is_value(sd.within_event_start_end_range) {
		if spec_expr_is_set(spec.endf) {
			is_currently_end_event = spec_expr_eval(spec.endf, model)
		}
	}

	// do something if we are in start/end range or nothing is set at all (means
	// do it always)
	if maybe_bool_is_nothing(sd.within_event_start_end_range) ||
	   maybe_bool_value(sd.within_event_start_end_range) {
		// check for at event
		if spec_expr_is_set(spec.atf) && spec_expr_eval(spec.atf, model) {
			if store_obj_outputs {
				store_results_obj(sd.output_ids[:], &sd.results_obj, model)
			} else {
				store_results(sd.output_ids[:], &sd.results, model)
			}
		} else if spec_expr_is_set(spec.fromf) && spec_expr_is_set(spec.tof) { 	// or from/to range event
			is_currently_to_event := false
			if maybe_bool_is_nothing(sd.within_event_from_to_range) ||
			   !maybe_bool_value(sd.within_event_from_to_range) {
				sd.within_event_from_to_range = spec_expr_eval(spec.fromf, model)
			} else if maybe_bool_is_value(sd.within_event_from_to_range) {
				is_currently_to_event = spec_expr_eval(spec.tof, model)
			}

			if maybe_bool_value(sd.within_event_from_to_range) {
				// if while is specified together with a from/to range, store only if
				// the while is true but aggregate only if the range is left - this
				// means the range specifies the extent of recording
				if spec_expr_is_set(spec.whilef) {
					if spec_expr_eval(spec.whilef, model) {
						store_results(sd.output_ids[:], &sd.intermediate_results, model)
					}
				} else {
					store_results(sd.output_ids[:], &sd.intermediate_results, model)
				}
				if is_currently_to_event {
					if store_obj_outputs {
						store_data_aggregate_results_obj(sd)
					} else {
						store_data_aggregate_results(sd)
					}
					sd.within_event_from_to_range = false
				}
			}
		} else if spec_expr_is_set(spec.whilef) { 	// or a single while aggregating expression
			if spec_expr_eval(spec.whilef, model) {
				store_results(sd.output_ids[:], &sd.intermediate_results, model)
			} else if len(sd.intermediate_results) > 0 && len(sd.intermediate_results[0]) > 0 {
				// if while event was not successful but we got intermediate results,
				// they should be aggregated
				if store_obj_outputs {
					store_data_aggregate_results_obj(sd)
				} else {
					store_data_aggregate_results(sd)
				}
			}
		}
	}

	if is_currently_end_event {
		sd.within_event_start_end_range = false
	}
}

// C++: vector<StoreData> monica::setupStorage(const Json&, const Date&, const Date&)
setup_storage :: proc(
	event2oids: jx.Value,
	start_date, end_date: d.Date,
	use_legacy_output_fns := false,
	allocator := context.allocator,
) -> [dynamic]Store_Data {
	shortcuts := make(map[string]jx.Value, 0, context.temp_allocator)
	shortcuts["daily"] = jx.obj(allocator, {"at", jx.sl("xxxx-xx-xx")})
	shortcuts["monthly"] = jx.obj(
		allocator,
		{"from", jx.sl("xxxx-xx-01")},
		{"to", jx.sl("xxxx-xx-31")},
	)
	shortcuts["yearly"] = jx.obj(
		allocator,
		{"from", jx.sl("xxxx-01-01")},
		{"to", jx.sl("xxxx-12-31")},
	)
	shortcuts["run"] = jx.obj(
		allocator,
		{"from", jx.s(d.to_iso_date_string(start_date, "", allocator), allocator)},
		{"to", jx.s(d.to_iso_date_string(end_date, "", allocator), allocator)},
	)
	shortcuts["crop"] = jx.obj(allocator, {"from", jx.sl("Sowing")}, {"to", jx.sl("Harvest")})

	store_data := make([dynamic]Store_Data, 0, allocator)

	e2os := jx.array_items(event2oids)
	for i := 0; i < len(e2os); i += 2 {
		if i + 1 >= len(e2os) {
			break
		}

		sd: Store_Data
		sd.spec.orig_spec = e2os[i]
		spec := sd.spec.orig_spec

		// find shortcut for string or store string as 'at' pattern
		if jx.is_string(spec) {
			ss := jx.string_value_of(spec)
			if sc, ok := shortcuts[ss]; ok {
				spec = sc
			} else {
				spec = jx.obj(allocator, {"at", jx.s(ss, allocator)})
			}
		} else if jx.is_array(spec) &&
		   len(jx.array_items(spec)) == 4 &&
		   jx.is_string(jx.at(spec, 0)) &&
		   (jx.string_value_of(jx.at(spec, 0)) == "while" ||
				   jx.string_value_of(jx.at(spec, 0)) == "at") {
			// an array means it's an expression pattern to be stored at 'at'
			sa := jx.array_items(spec)
			key := jx.string_value_of(sa[0])
			rest := make(jx.Array, 0, len(sa) - 1, allocator)
			for k in 1 ..< len(sa) {
				append(&rest, sa[k])
			}
			spec = jx.obj(allocator, {key, jx.Value(rest)})
		} else if jx.is_array(spec) {
			spec = jx.obj(allocator, {"at", spec})
		} else if !jx.is_object(spec) {
			// everything else (number, bool, null) we ignore; object is the
			// default we assume
			continue
		}

		// if "at" and "while" are missing, add by default an "every day" "at"
		if jx.is_null(jx.get(spec, "at")) &&
		   jx.is_null(jx.get(spec, "while")) &&
		   jx.is_null(jx.get(spec, "from")) &&
		   jx.is_null(jx.get(spec, "to")) {
			o := make(jx.Object, 0, allocator)
			for k, v in jx.object_items(spec) {
				o[strings.clone(k, allocator)] = v
			}
			o[strings.clone("at", allocator)] = jx.sl("xxxx-xx-xx")
			spec = jx.Value(o)
		}

		spec_merge(&sd.spec, spec)
		sd.output_ids = mio.parse_output_ids(
			jx.array_items(e2os[i + 1]),
			use_legacy_output_fns,
			allocator,
		)

		append(&store_data, sd)
	}

	return store_data
}

// ---------------------------------------------------------------------------
// runMonica
// ---------------------------------------------------------------------------

// C++: bool cultivationmethod::areOnlyAbsoluteWorksteps/reinit/... driven
// crop-rotation-cycling closures inside runMonicaIC (checkAndInitShadowOfNextCropRotation_,
// findNextCultivationMethod_) - hoisted to package-level procs here since
// Odin procs can't capture; every captured C++ local becomes an explicit
// parameter instead.

// C++: the checkAndInitShadowOfNextCropRotation_ lambda
@(private)
check_and_init_shadow_of_next_crop_rotation :: proc(
	env_crop_rotations: []Crop_Rotation,
	crit: ^int,
	crop_rotation: ^[dynamic]^Cultivation_Method,
	current_date: d.Date,
) -> bool {
	if crit^ < len(env_crop_rotations) {
		// if current cropRotation is finished, try to move to next
		cr := &env_crop_rotations[crit^]
		if d.is_valid(cr.end) && d.eq(current_date, d.add(cr.end, 1)) {
			crit^ += 1
			clear(crop_rotation)
		}

		// check again, because we might have moved to next cropRotation
		if crit^ < len(env_crop_rotations) {
			// if a new cropRotation starts, copy the pointers to the CMs to the
			// shadow CR
			cr2 := &env_crop_rotations[crit^]
			if d.is_valid(cr2.start) && d.eq(current_date, cr2.start) {
				for i in 0 ..< len(cr2.crop_rotation) {
					append(crop_rotation, &cr2.crop_rotation[i])
				}
				return true
			}
		}
	}
	return false
}

// C++: the findNextCultivationMethod_ lambda
@(private)
find_next_cultivation_method :: proc(
	current_date: d.Date,
	crop_rotation: ^[dynamic]^Cultivation_Method,
	cmit: ^int,
	advance_to_next_cm_in: bool,
	allocator: Allocator,
) -> (
	current_cm: ^Cultivation_Method,
	next_absolute_cm_application_date: d.Date,
) {
	advance_to_next_cm := advance_to_next_cm_in

	// it might be possible that the next cultivation method has to be skipped
	// (if cover/catch crop)
	not_found_next_cm := true
	for not_found_next_cm {
		if advance_to_next_cm {
			// delete fully cultivation methods with only absolute worksteps,
			// because they won't participate in a new run when wrapping the crop
			// rotation
			cm := crop_rotation^[cmit^]
			if cultivation_method_are_only_absolute_worksteps(cm) || !cm.repeat {
				ordered_remove(crop_rotation, cmit^)
			} else {
				cmit^ += 1
			}

			// start anew if we reached the end of the crop rotation
			if cmit^ >= len(crop_rotation^) {
				cmit^ = 0
			}
		}

		// check if there's at least a cultivation method left in cropRotation
		if cmit^ < len(crop_rotation^) {
			advance_to_next_cm = true
			current_cm = crop_rotation^[cmit^]

			// addedYear tells that the start of the cultivation method was before
			// currentDate and thus the whole CM had to be moved into the next year
			// - possible for relative dates
			added_year := cultivation_method_reinit(current_cm, current_date)
			if added_year {
				// current CM is a cover crop, check if the latest sowing date would
				// have been before current date, if so, skip current CM
				if current_cm.is_cover_crop {
					// if current CM's latest sowing date is actually after current
					// date, we have to reinit current CM again, but this time prevent
					// shifting it to the next year
					lsd := d.with_year(
						cultivation_method_abs_latest_sowing_date(current_cm),
						u16(d.year(current_date)),
					)
					not_found_next_cm = d.lt(lsd, current_date)
					if !not_found_next_cm {
						cultivation_method_reinit(current_cm, current_date, true)
					}
				} else {
					not_found_next_cm = current_cm.can_be_skipped // if current CM was marked skipable, skip it
				}
			} else { 	// not added year or CM also had absolute dates
				if current_cm.is_cover_crop {
					not_found_next_cm = d.lt(
						cultivation_method_abs_latest_sowing_date(current_cm),
						current_date,
					)
				} else if current_cm.can_be_skipped {
					not_found_next_cm = d.lt(
						cultivation_method_abs_start_date(current_cm),
						current_date,
					)
				} else {
					not_found_next_cm = false
				}
			}

			if not_found_next_cm {
				next_absolute_cm_application_date = d.Date{}
			} else {
				static_ws := cultivation_method_static_worksteps(current_cm, allocator)
				next_absolute_cm_application_date =
					len(static_ws) == 0 ? d.Date{} : cultivation_method_abs_start_date(current_cm, false)
			}
		} else {
			current_cm = nil
			next_absolute_cm_application_date = d.Date{}
			not_found_next_cm = false
		}
	}

	return
}

// C++: Output monica::runMonica(Env)
//
// This ports the non-Intercropping half of runMonicaIC directly (not as an
// isIC=false branch of a dual-model function) - see the file header comment
// for the full list of what's dropped.
run_monica :: proc(env: ^Env, allocator := context.allocator) -> mio.Output {
	out: mio.Output
	// C++: out.customId = env.customId; (runMonicaIC, run-monica.cpp) - was missing
	// here, which only showed up once the Cap'n Proto server started relying on it:
	// unlike run/serve_zmq.odin, run-monica-capnp.cpp does NOT re-assign customId
	// after the call.
	out.custom_id = env.custom_id

	// NOTE(deviation): no C++ counterpart - runMonicaIC has no such guard.
	//
	// Every soil module indexes the soil column unconditionally (the first to do
	// so is soilTemperature, whose layer lookup reaches for the layer below the
	// last one). With an empty profile that is an out-of-bounds access: undefined
	// behaviour in the C++, and in Odin a bounds-check panic that aborts the
	// process outright. For monica-run that is merely a bad error message, but a
	// server dies on the spot - so any client able to send an env without a soil
	// profile can take monica-zmq-server or monica-capnp-server down. Odin has no
	// exceptions, so the C++'s try/catch around runMonica cannot be ported;
	// refusing the run up front is the equivalent, and it turns the crash into an
	// ordinary entry in Output.errors that every caller already reports.
	//
	// Deliberately checked here rather than in each server: monica-run reaches the
	// same code path, and this is the one place all three entry points share.
	if len(env.params.site_params.soil_parameters) == 0 {
		out.errors = make([dynamic]string, allocator)
		append(
			&out.errors,
			"Error: no soil layers - MONICA cannot run without a soil profile (SiteParameters.SoilProfileParameters)!",
		)
		return out
	}

	// NOTE(deviation): no C++ counterpart - sim.json's "Modules" section
	// (plan-crop2ml-modules.md §1.4). An unknown implementation is refused here,
	// for the same reasons as the soil profile above, before make_monica_model
	// would have to.
	if errs := core.check_modules_config(
		&env.params.sim_params.modules,
		env.params.sim_params.dual_kc_method,
		allocator,
	); len(errs) > 0 {
		out.errors = errs
		return out
	}

	// prefer multiple crop rotations, but use a single rotation if there
	if len(env.crop_rotations) == 0 && len(env.crop_rotation) > 0 {
		append(
			&env.crop_rotations,
			make_crop_rotation(
				clim.data_accessor_start_date(&env.climate_data),
				clim.data_accessor_end_date(&env.climate_data),
				env.crop_rotation,
			),
		)
	}

	model := core.make_monica_model(&env.params, allocator)
	// NOTE(deviation): as the C++ AMEI branch (run-monica.cpp) for its modules
	if core.soil_temperature_modules_configured(model) {
		tamp, tav := clim.data_accessor_calc_tamp_and_tav(&env.climate_data, allocator)
		core.soil_temperature_modules_set_tav_tamp(model, tav, tamp)
	}
	model.sim_params.start_date = clim.data_accessor_start_date(&env.climate_data)
	model.sim_params.end_date = clim.data_accessor_end_date(&env.climate_data)

	current_date := clim.data_accessor_start_date(&env.climate_data)

	// cropRotation is a shadow of env.cropRotations[crit].cropRotation, holding
	// pointers to CMs, which might shrink if pure absolute CMs are finished -
	// see find_next_cultivation_method / check_and_init_shadow_of_next_crop_rotation
	crop_rotation := make([dynamic]^Cultivation_Method, 0, allocator)
	crit := 0
	cmit := 0

	current_cm, next_absolute_cm_application_date := find_next_cultivation_method(
		current_date,
		&crop_rotation,
		&cmit,
		false,
		allocator,
	)

	return_obj_outputs := env_return_obj_outputs(env)
	store := setup_storage(
		env.events,
		clim.data_accessor_start_date(&env.climate_data),
		clim.data_accessor_end_date(&env.climate_data),
		env_use_legacy_output_fns(env),
		allocator,
	)
	model.current_events["run-started"] = true

	nods := clim.data_accessor_no_of_steps_possible(&env.climate_data)
	for step_no in 0 ..< nods {
		if check_and_init_shadow_of_next_crop_rotation(
			env.crop_rotations[:],
			&crit,
			&crop_rotation,
			current_date,
		) {
			cmit = 0
			current_cm, next_absolute_cm_application_date = find_next_cultivation_method(
				current_date,
				&crop_rotation,
				&cmit,
				false,
				allocator,
			)
		}

		core.monica_model_daily_reset(model, allocator)

		model.current_step_date = current_date
		append(
			&model.climate_data,
			clim.data_accessor_all_data_for_step(
				&env.climate_data,
				step_no,
				env.params.site_params.latitude,
				allocator,
			),
		)

		// test if monica's crop has been dying in the previous step; if yes, it
		// will be incorporated into soil
		if model.current_crop_module != nil && model.current_crop_module.dying_out {
			core.monica_model_incorporate_current_crop(model, allocator)
		}

		// try to apply dynamic worksteps marked to run before everything else
		// that day
		if current_cm != nil {
			cultivation_method_apply(current_cm, model, true)
		}

		// apply worksteps and cycle through crop rotation
		if current_cm != nil && d.eq(next_absolute_cm_application_date, current_date) {
			cultivation_method_abs_apply(
				current_cm,
				next_absolute_cm_application_date,
				model,
				allocator,
			)
			next_absolute_cm_application_date = cultivation_method_next_abs_date(
				current_cm,
				next_absolute_cm_application_date,
			)
		}

		// monica main stepping method
		core.monica_model_step(model, allocator)

		// try to apply dynamic worksteps marked to run AFTER everything else
		// that day
		if current_cm != nil {
			cultivation_method_apply(current_cm, model, false)
		}

		// store results
		for &s in store {
			store_data_store_results_if_spec_applies(&s, model, return_obj_outputs)
		}

		// if the next application date is not valid, we're at the end of the
		// application list of this cultivation method and go to the next one in
		// the crop rotation
		if current_cm != nil &&
		   cultivation_method_all_dynamic_worksteps_finished(current_cm) &&
		   !d.is_valid(next_absolute_cm_application_date) {
			core.monica_model_reset_fertiliser_counter(model)
			current_cm, next_absolute_cm_application_date = find_next_cultivation_method(
				d.add(current_date, 1),
				&crop_rotation,
				&cmit,
				true,
				allocator,
			)
		}

		current_date = d.add(current_date, 1)
	}

	for &sd in store {
		// aggregate results of while events or unfinished other from/to ranges
		// (where the to event didn't happen yet)
		if return_obj_outputs {
			store_data_aggregate_results_obj(&sd)
		} else {
			store_data_aggregate_results(&sd)
		}
		append(
			&out.data,
			mio.Output_Data {
				orig_spec = jx.dump(sd.spec.orig_spec, allocator),
				output_ids = sd.output_ids,
				results = sd.results,
				results_obj = sd.results_obj,
			},
		)
	}

	return out
}
