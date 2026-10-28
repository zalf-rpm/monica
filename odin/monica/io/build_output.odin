// src/io/build-output.h/.cpp: the output table. Phase 7 checkpoint 2 started
// it with the 21 ids sim-min.json needs; it now holds every output of the C++
// table that is *computed* (a proc call, arithmetic, a Date method, a guard).
// The ones that are just a field read plus rounding live in
// output_paths.odin's alias table instead (gen_output_aliases.py), so
// together they cover the whole C++ table. Verified against C++ master over
// its test suites (Hohenfinow2 + 48 muencheberg_v140 simulations).
//
// Each entry's `id` is purely an internal map key in both the C++ and this
// port - never serialized, never displayed (csv-format.cpp writes name/
// displayName/unit, not id) - so this port's own 0..20 numbering doesn't
// need to match the C++ table's id assignment (which is just registration
// order across all ~300 entries there); only internal self-consistency
// (parseOutputIds resolves a name to an id, buildOutputTable resolves that
// id to a function) matters.
//
// `setfs`/`setComplexValues` (the SetValue workstep's write side) ARE ported
// - see set_complex_values below and run/worksteps.odin's set_value_* - but
// only for the two ids the real C++ table actually registers a setter for
// among these 21 (Stage, Mois); the other 19 have no C++ setf either.
//
// Dropped: the getCompareOp/applyCompareOp/buildExpression/
// buildCompareExpression machinery, AND its sibling getPrimitiveCalcOp/
// applyPrimitiveCalcOp/buildPrimitiveCalcExpression (the SetValue-only
// `["=", a, op, b]` arithmetic-expression array syntax) - see
// run/worksteps.odin's set_value_merge for why the latter stays deferred
// even though SetValue itself is now ported. Grepped sim-min.json's entire
// output.events/_events section: every spec is either a shortcut string
// ("daily"/"crop"/"yearly"/"run"), a workstep-event-name string
// ("OrganicFertilization"), or a plain output-id array - never the
// `["while"|"at", [oid, "op", value]]` comparison-expression array syntax
// buildCompareExpression exists for. "Port on demand" if a future fixture
// needs either.
//
// No mutex/lazy-static-init: the C++ guards buildOutputTable's one-time
// construction with a mutex because MonicaModel instances can run
// concurrently in the RPC/ZeroMQ server paths this port drops entirely -
// monica-run is single-threaded, so build_output_table is just called once
// up front by the CLI entry point (phase 7 checkpoint 5) instead.
package monica_io

import d "../../support/date"
import jx "../../support/jsonx"
import rp "../../support/reflectpath"
import tl "../../support/tools"
import core "../core"
import p "../params"
import "core:fmt"
import "core:strings"

// C++: struct monica::OutputMetadata
OutputMetadata :: struct {
	id:          int,
	name:        string,
	unit:        string,
	description: string,
}

// C++: double monica::applyOIdOP(OId::OP, const vector<double>&)
//
// NOTE(c++-quirk): the MEDIAN case calls Tools::median(vs) directly on vs in
// whatever order the caller built it - median's own doc comment says it
// "expects an ordered vector", but applyOIdOP never sorts vs first. Not
// exercised by any of this port's 21 ids (none use MEDIAN), reproduced
// as-is rather than "fixed" with an unrequested sort.
apply_oid_op :: proc(op: OId_Op, vs: []f64) -> f64 {
	v := 0.0
	if len(vs) > 0 {
		v = vs[len(vs) - 1]
		switch op {
		case .AVG:
			sum := 0.0
			for x in vs {
				sum += x
			}
			v = sum / f64(len(vs))
		case .MEDIAN:
			v = tl.median(vs)
		case .SUM:
			sum := 0.0
			for x in vs {
				sum += x
			}
			v = sum
		case .MIN:
			mn, _ := tl.min_max(vs)
			v = mn
		case .MAX:
			_, mx := tl.min_max(vs)
			v = mx
		case .FIRST:
			v = vs[0]
		case .LAST, .NONE, .UNDEFINED_OP:
		}
	}
	return v
}

// C++: Json monica::applyOIdOP(OId::OP, const vector<Json>&)
//
// Time aggregation of whatever an output produced each day: a scalar per
// day aggregates to a scalar; an array per day - a layer range without its
// own layer op, e.g. ["N", [1, 3]] in a yearly section - aggregates element
// by element (per layer) to an array.
//
// NOTE(c++-quirk): the element count comes from the first day's array; a
// later, longer one would index past it in the C++ (undefined behaviour) -
// its extra elements are ignored here instead.
apply_oid_op_json :: proc(op: OId_Op, js: []jx.Value, allocator := context.allocator) -> jx.Value {
	if len(js) > 0 && jx.is_array(js[0]) {
		dss := make([][dynamic]f64, len(jx.array_items(js[0])), context.temp_allocator)
		for j in js {
			for j2, i in jx.array_items(j) {
				if i < len(dss) {
					append(&dss[i], jx.number_value(j2))
				}
			}
		}
		r := make(jx.Array, 0, len(dss), allocator)
		for ds in dss {
			append(&r, jx.f(apply_oid_op(op, ds[:])))
		}
		return jx.Value(r)
	}
	ds := make([dynamic]f64, 0, len(js), context.temp_allocator)
	for j in js {
		append(&ds, jx.number_value(j))
	}
	return jx.f(apply_oid_op(op, ds[:]))
}

// C++: template<typename T> Json getComplexValues(OId, function<T(int)>, int)
//
// get_value takes `model` as an explicit parameter instead of capturing it -
// Odin `proc` values can't close over anything, the same capture-workaround
// shape used throughout this port since phase 5.
get_complex_values :: proc(
	model: ^core.Monica_Model,
	oid_in: OId,
	get_value: proc(_: ^core.Monica_Model, _: int) -> f64,
	round_to_digits: int = 0,
	allocator := context.allocator,
) -> jx.Value {
	oid := oid_in
	if oid_is_organ(&oid) {
		oid.to_layer = int(oid.organ)
		oid.from_layer = int(oid.organ)
	}

	multiple_values := make([dynamic]jx.Value, 0, allocator)
	vs := make([dynamic]f64, 0, allocator)
	for i := oid.from_layer; i <= oid.to_layer; i += 1 {
		v := 0.0
		if i >= 0 {
			// A reflection-backed oid reuses this loop instead of forking the
			// layer-range and aggregation semantics into a second copy - the
			// open (unindexed) dimension of its path IS `i`. get_value is nil
			// for those oids (output_paths.odin: resolve_oid_value).
			if oid.plan != nil {
				v, _ = rp.resolve_f64(oid.plan, model_root(model), i)
			} else {
				v = get_value(model, i)
			}
		}
		if oid.layer_agg_op == .NONE {
			append(&multiple_values, jx.f(round_or_not(v, round_to_digits)))
		} else {
			append(&vs, v)
		}
	}

	if oid.layer_agg_op == .NONE {
		return jx.Value(jx.Array(multiple_values))
	}
	return jx.f(apply_oid_op(oid.layer_agg_op, vs[:]))
}

// C++: void setComplexValues(OId, function<void(int, Json)>, Json) - the
// setfs-side counterpart to get_complex_values, used by the SetValue
// workstep. set_value takes `model` as an explicit parameter for the same
// no-capture reason get_value does above.
set_complex_values :: proc(
	model: ^core.Monica_Model,
	oid_in: OId,
	set_value: proc(_: ^core.Monica_Model, _: int, _: jx.Value),
	value: jx.Value,
	allocator := context.allocator,
) {
	oid := oid_in
	if oid_is_organ(&oid) {
		oid.to_layer = int(oid.organ)
		oid.from_layer = int(oid.organ)
	}

	if jx.is_object(value) || jx.is_null(value) {
		return
	}

	values: []jx.Value
	if jx.is_array(value) {
		values = jx.array_items(value)
	} else {
		n := oid.to_layer - oid.from_layer + 1
		fill := make([]jx.Value, max(n, 0), allocator)
		for i in 0 ..< len(fill) {
			fill[i] = value
		}
		values = fill
	}

	k := 0
	for i := oid.from_layer; i <= oid.to_layer && k < len(values); i, k = i + 1, k + 1 {
		if i >= 0 {
			// mirrors get_complex_values: a path-backed oid drives the same
			// range/organ loop, writing through its open dimension instead of
			// calling a registered setter. set_value is nil for those.
			if oid.plan != nil {
				set_plan_value_at(model, oid, i, values[k])
			} else {
				set_value(model, i, values[k])
			}
		}
	}
}

// Resolves an output name to an OId, in the tier order of
// plan-reflective-outputs.md §2.5:
//
//   1. the name has a path alias    -> reflection-backed (compiled here)
//   2. the name is a registered id  -> lambda-backed (the pre-reflection path)
//   3. the name IS a path           -> reflection-backed, no metadata
//   4. otherwise                    -> skipped WITH A WARNING
//
// Tier 1 deliberately outranks tier 2: it means the ids sim-min.json already
// exercises run through the new engine on the existing fixture, which is what
// turns the CSV self-diff into a test of the engine instead of a test of
// nothing. `use_legacy` swaps 1 and 2 back for A/B bisection.
//
// All of this happens at SETUP. Nothing here runs per day.
@(private)
resolve_oid_name :: proc(
	n0: string,
	name2metadata: map[string]OutputMetadata,
	use_legacy: bool,
	allocator: jx.Allocator,
) -> (
	oid: OId,
	ok: bool,
) {
	oid = make_default_oid()

	// An empty name is not a typo to report: it is what an output spec whose
	// first element is not a string (a SetValue "value" that is a plain
	// per-layer number array, say) reduces to, and parse_output_ids is called
	// on those speculatively. Nothing was named, so there is nothing to warn
	// about - just no oid.
	if n0 == "" {
		return oid, false
	}

	md, has_md := name2metadata[n0]

	if !use_legacy {
		if a, has_alias := legacy_aliases(allocator)[n0]; has_alias {
			err := oid_bind_path(&oid, a.path, allocator)
			if !rp.failed(err) {
				oid.round_to_digits = a.round
				oid.cast_to = a.cast_to
				// Keep the legacy id, name and unit: the CSV header prints
				// name/unit, and worksteps.odin still dispatches setfs by id.
				oid.id = has_md ? md.id : -1
				oid.name = has_md ? md.name : n0
				oid.unit = has_md ? md.unit : a.unit
				return oid, true
			}
			// An alias that does not compile is a bug in the table, not in
			// the user's sim.json - say so and fall through to the lambda.
			warn_bad_path(n0, err)
		}
	}

	if has_md {
		oid.id = md.id
		oid.name = md.name
		oid.unit = md.unit
		return oid, true
	}

	err := oid_bind_path(&oid, n0, allocator)
	if !rp.failed(err) {
		oid.name = n0
		return oid, true
	}

	// Tier 4. Before this, an unknown name was a silent skip - a typo cost
	// you a column and no message.
	warn_bad_path(n0, err)
	return oid, false
}

// C++: the `getAggregationOp` lambda inside parseOutputIds. Lifted to file
// scope so the object form (below) can share the exact same parsing rather
// than reimplementing it slightly differently.
@(private)
oid_aggregation_op :: proc(arr: []jx.Value, index: int, def: OId_Op = .UNDEFINED_OP) -> OId_Op {
	if len(arr) > index && jx.is_string(arr[index]) {
		ops := strings.to_upper(jx.string_value_of(arr[index]), context.temp_allocator)
		switch ops {
		case "SUM":
			return .SUM
		case "AVG":
			return .AVG
		case "MEDIAN":
			return .MEDIAN
		case "MIN":
			return .MIN
		case "MAX":
			return .MAX
		case "FIRST":
			return .FIRST
		case "LAST":
			return .LAST
		case "NONE":
			return .NONE
		}
	}
	return def
}

// C++: the `getOrgan` lambda inside parseOutputIds.
@(private)
oid_organ_of :: proc(arr: []jx.Value, index: int, def: OId_Organ = .UNDEFINED_ORGAN) -> OId_Organ {
	if len(arr) > index && jx.is_string(arr[index]) {
		ops := strings.to_upper(jx.string_value_of(arr[index]), context.temp_allocator)
		switch ops {
		case "ROOT":
			return .ROOT
		case "LEAF":
			return .LEAF
		case "SHOOT":
			return .SHOOT
		case "FRUIT":
			return .FRUIT
		case "STRUCT":
			return .STRUCT
		case "SUGAR":
			return .SUGAR
		}
	}
	return def
}

// C++: the body of parseOutputIds' `if(arr.size() >= 2)` block - slot 1 of
// the array form, which is a layer number, a layer range, an organ name or a
// time-aggregation op depending on its JSON type. Lifted verbatim so the
// object form's "layers" key means exactly what the array form's slot 1
// means (plan-reflective-outputs.md §2.6).
@(private)
oid_apply_slot1 :: proc(oid: ^OId, val1: jx.Value) {
	if jx.is_number(val1) {
		oid.from_layer = jx.int_value_of(val1) - 1
		oid.to_layer = oid.from_layer
	} else if jx.is_string(val1) {
		one := []jx.Value{val1}
		op := oid_aggregation_op(one, 0)
		if op != .UNDEFINED_OP {
			oid.time_agg_op = op
		} else {
			oid.organ = oid_organ_of(one, 0, .UNDEFINED_ORGAN)
		}
	} else if jx.is_array(val1) {
		arr2 := jx.array_items(val1)
		if len(arr2) >= 1 {
			val1_0 := arr2[0]
			if jx.is_number(val1_0) {
				oid.from_layer = jx.int_value_of(val1_0) - 1
			} else if jx.is_string(val1_0) {
				oid.organ = oid_organ_of(arr2, 0, .UNDEFINED_ORGAN)
			}
		}
		if len(arr2) >= 2 {
			val1_1 := arr2[1]
			if jx.is_number(val1_1) {
				oid.to_layer = jx.int_value_of(val1_1) - 1
			} else if jx.is_string(val1_1) {
				oid.to_layer = oid.from_layer
				oid.layer_agg_op = oid_aggregation_op(arr2, 1, .AVG)
			}
		}
		if len(arr2) >= 3 {
			oid.layer_agg_op = oid_aggregation_op(arr2, 2, .AVG)
		}
	}
}

@(private)
oid_split2 :: proc(name: string, allocator: jx.Allocator) -> (n0: string, n1: string) {
	parts := strings.split(name, "|", allocator)
	n0 = len(parts) > 0 ? parts[0] : ""
	n1 = len(parts) > 1 ? parts[1] : ""
	return
}

// Not in the C++: the object form of an output spec.
//
// A raw path carries no display name, and the positional slots of the array
// form ([name, layers, timeAgg]) are all taken - so metadata needs somewhere
// to live:
//
//   { "path": "soilMoisture.actual_evaporation", "name": "ActEvap",
//     "unit": "mm", "round": 3 }
//   { "path": "soilColumn.layers.vs_SoilNH4", "unit": "kgN m-3", "round": 6,
//     "layers": [1, 6, "AVG"], "agg": "SUM" }
//   { "path": "soil_moisture.actual_evapotranspiration", "name": "ETa_m",
//     "unit": "m", "calc": ["/", 1000] }
//
// "unit" defaults to the unit of the path's field (its `unit` tag, see
// reflectpath), except with a "calc" (output_calc.odin), which may change
// it: then the column has no unit unless "unit" gives one.
//
// Purely additive: parseOutputIds already ignores anything that is not a
// string or an array.
//
// A raw path is NOT rounded unless it says so - roundToDigits stays -1 and
// the CSV writer's %.6g decides. Only the legacy aliases carry digits, which
// is what keeps them byte-identical.
@(private)
oid_from_object :: proc(idj: jx.Value, allocator: jx.Allocator) -> (oid: OId, ok: bool) {
	pathj := jx.get(idj, "path")
	if !jx.is_string(pathj) {
		return oid, false
	}
	path := jx.string_value_of(pathj)

	oid = make_default_oid()
	if err := oid_bind_path(&oid, path, allocator); rp.failed(err) {
		warn_bad_path(path, err)
		return oid, false
	}

	if namej := jx.get(idj, "name"); jx.is_string(namej) {
		oid.name = jx.string_value_of(namej)
	} else {
		oid.name = oid.path
	}
	if roundj := jx.get(idj, "round"); jx.is_number(roundj) {
		oid.round_to_digits = jx.int_value_of(roundj)
	}
	if strings.to_upper(jx.string_value_of(jx.get(idj, "cast")), context.temp_allocator) == "INT" {
		oid.cast_to = .INT
	}
	if layersj := jx.get(idj, "layers"); !jx.is_null(layersj) {
		oid_apply_slot1(&oid, layersj)
	}
	if organj := jx.get(idj, "organ"); jx.is_string(organj) {
		oid.organ = oid_organ_of([]jx.Value{organj}, 0, .UNDEFINED_ORGAN)
	}
	if op := oid_aggregation_op([]jx.Value{jx.get(idj, "agg")}, 0); op != .UNDEFINED_OP {
		oid.time_agg_op = op
	}
	if calcj := jx.get(idj, "calc"); !jx.is_null(calcj) {
		if msg := oid_parse_calc(&oid, calcj, allocator); msg != "" {
			fmt.eprintfln("parse_output_ids: ignoring output %q - %s", path, msg)
			return oid, false
		}
		oid.unit = ""
	}
	if unitj := jx.get(idj, "unit"); jx.is_string(unitj) {
		oid.unit = jx.string_value_of(unitj)
	}
	oid.json_input = jx.dump(idj, allocator)
	return oid, true
}

// C++: vector<OId> monica::parseOutputIds(const Tools::J11Array&)
parse_output_ids :: proc(
	oid_array: []jx.Value,
	use_legacy_output_fns := false,
	allocator := context.allocator,
) -> [dynamic]OId {
	output_ids := make([dynamic]OId, 0, allocator)

	name2metadata := build_output_table().name2metadata

	for idj in oid_array {
		if jx.is_string(idj) {
			name := jx.string_value_of(idj)
			n0, n1 := oid_split2(name, allocator)
			oid, ok := resolve_oid_name(n0, name2metadata, use_legacy_output_fns, allocator)
			if !ok {
				continue
			}
			oid.display_name = n1
			oid.json_input = name
			append(&output_ids, oid)
		} else if jx.is_array(idj) {
			arr := jx.array_items(idj)
			if len(arr) < 1 {
				continue
			}
			name := jx.string_value_of(arr[0])
			n0, n1 := oid_split2(name, allocator)
			oid, ok := resolve_oid_name(n0, name2metadata, use_legacy_output_fns, allocator)
			if !ok {
				continue
			}
			oid.display_name = n1
			oid.json_input = jx.dump(idj, allocator)

			if len(arr) >= 2 {
				oid_apply_slot1(&oid, arr[1])
			}
			if len(arr) >= 3 {
				oid.time_agg_op = oid_aggregation_op(arr, 2, .AVG)
			}

			append(&output_ids, oid)
		} else if jx.is_object(idj) {
			if oid, ok := oid_from_object(idj, allocator); ok {
				append(&output_ids, oid)
			}
		}
	}

	return output_ids
}

// ---------------------------------------------------------------------------
// The 21 output-id functions themselves (C++: the lambdas passed to build()
// inside buildOutputTable). Named top-level procs, not inline closures - the
// layer-based ones (Mois/SOC/N) need get_complex_values's explicit-model-
// parameter shape, so naming all 21 the same way keeps the table below
// uniform.
// ---------------------------------------------------------------------------

@(private)
of_cm_count :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	return jx.i(model.cultivation_method_count)
}

@(private)
of_date :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	return jx.s(d.to_iso_date_string(model.current_step_date))
}

@(private)
of_year :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	return jx.i(d.year(model.current_step_date))
}

// C++: build({id++, "DOY", ...}) - int(monica.currentStepDate.dayOfYear())
@(private)
of_doy :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	return jx.i(int(d.day_of_year(model.current_step_date)))
}

@(private)
of_crop :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	if model.current_crop_module != nil {
		return jx.s(p.crop_name(&model.current_crop_module.crop_params, context.allocator))
	}
	return jx.s("")
}

@(private)
of_stage :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	if model.current_crop_module != nil {
		return jx.i(model.current_crop_module.developmental_stage + 1)
	}
	return jx.i(0)
}

@(private)
of_stage_set :: proc(model: ^core.Monica_Model, oid: OId, value: jx.Value) {
	if jx.is_number(value) && model.current_crop_module != nil {
		core.set_stage(model.current_crop_module, max(0, jx.int_value_of(value) - 1))
	}
}

@(private)
of_ab_biom :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	if model.current_crop_module != nil {
		return jx.f(tl.round(model.current_crop_module.aboveground_biomass, 1))
	}
	return jx.f(0.0)
}

@(private)
of_org_biom :: proc(model: ^core.Monica_Model, oid_in: OId) -> jx.Value {
	oid := oid_in
	if oid_is_organ(&oid) &&
	   model.current_crop_module != nil &&
	   p.species_parameters_number_of_organs(
		   &model.current_crop_module.crop_params.species_params,
	   ) >
		   int(oid.organ) {
		return jx.f(tl.round(model.current_crop_module.organ_biomass[int(oid.organ)], 1))
	}
	return jx.f(0.0)
}

@(private)
of_yield :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	if model.current_crop_module != nil {
		return jx.f(tl.round(core.get_primary_crop_yield(model.current_crop_module), 1))
	}
	return jx.f(0.0)
}

@(private)
of_lai :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	if model.current_crop_module != nil {
		return jx.f(tl.round(model.current_crop_module.leaf_area_index, 4))
	}
	return jx.f(0.0)
}

@(private)
mois_get_value :: proc(model: ^core.Monica_Model, i: int) -> f64 {
	return model.soil_column.layers[i].soil_moisture_m3
}

@(private)
of_mois :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	return get_complex_values(model, oid, mois_get_value, 3)
}

@(private)
mois_set_value :: proc(model: ^core.Monica_Model, i: int, value: jx.Value) {
	if jx.is_number(value) {
		model.soil_column.layers[i].soil_moisture_m3 = jx.number_value(value)
	}
}

@(private)
of_mois_set :: proc(model: ^core.Monica_Model, oid: OId, value: jx.Value) {
	set_complex_values(model, oid, mois_set_value, value)
}

@(private)
of_irrig :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	return jx.f(tl.round(model.daily_sum_irrigation_water, 3))
}

@(private)
of_runoff :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	return jx.f(tl.round(model.soil_moisture.surface_run_off, 1))
}

@(private)
of_kc :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	return jx.f(tl.round(model.soil_moisture.kc_factor, 3))
}

@(private)
of_recharge :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	return jx.f(tl.round(model.soil_column.flux_at_lower_boundary, 3))
}

@(private)
of_nleach :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	return jx.f(tl.round(model.soil_transport.leaching_at_boundary, 3))
}

@(private)
soc_get_value :: proc(model: ^core.Monica_Model, i: int) -> f64 {
	return core.soil_organic_carbon(&model.soil_column.layers[i])
}

@(private)
of_soc :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	return get_complex_values(model, oid, soc_get_value, 6)
}

@(private)
of_tavg :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	cd := model.climate_data[len(model.climate_data) - 1]
	if v, ok := cd[.tavg]; ok {
		return jx.f(tl.round(v, 4))
	}
	return jx.f(0.0)
}

@(private)
of_precip :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	cd := model.climate_data[len(model.climate_data) - 1]
	if v, ok := cd[.precip]; ok {
		return jx.f(tl.round(v, 4))
	}
	return jx.f(0.0)
}

@(private)
of_globrad :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	cd := model.climate_data[len(model.climate_data) - 1]
	if v, ok := cd[.globrad]; ok {
		return jx.f(tl.round(v, 4))
	}
	return jx.f(0.0)
}

@(private)
n_get_value :: proc(model: ^core.Monica_Model, i: int) -> f64 {
	return core.soil_nmin(&model.soil_column.layers[i])
}

@(private)
of_n :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	return get_complex_values(model, oid, n_get_value, 3)
}

@(private)
of_eta_etc :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	pot_et := model.soil_moisture.reference_evapotranspiration * model.soil_moisture.kc_factor
	act_et := model.soil_moisture.actual_evapotranspiration
	if pot_et > 0 {
		return jx.f(tl.round(act_et / pot_et, 2))
	}
	return jx.f(1.0)
}

// ---------------------------------------------------------------------------
// The rest of the C++ table's computed outputs (the ones that are more than a
// field read + rounding, so not in output_paths.odin's alias table). Each is
// a literal port of its build() entry in src/io/build-output.cpp.
// ---------------------------------------------------------------------------

// C++: the recurring
//   auto nools = int(soilcolumn::numberOfOrganicLayers(monica.soilColumn.get()));
//   oid.fromLayer = min(oid.fromLayer, nools - 1);
//   oid.toLayer = min(oid.toLayer, nools - 1);
// of the soil-organic outputs, whose arrays only have the organic layers.
@(private)
clamp_to_organic_layers :: proc(model: ^core.Monica_Model, oid_in: OId) -> OId {
	oid := oid_in
	nools := core.number_of_organic_layers(&model.soil_column)
	oid.from_layer = min(oid.from_layer, nools - 1)
	oid.to_layer = min(oid.to_layer, nools - 1)
	return oid
}

// C++: the `isOrgan(&oid) && currentCropModule && numberOfOrgans(...) > oid.organ`
// guard of the organ-specific outputs.
@(private)
organ_in_crop :: proc(model: ^core.Monica_Model, oid_in: OId) -> bool {
	oid := oid_in
	return oid_is_organ(&oid) &&
		model.current_crop_module != nil &&
		p.species_parameters_number_of_organs(&model.current_crop_module.crop_params.species_params) >
			int(oid.organ)
}

@(private)
of_count :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	return jx.i(1)
}

@(private)
of_days_since_start :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	return jx.i(d.diff(model.current_step_date, model.sim_params.start_date))
}

@(private)
of_month :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	return jx.i(int(d.month(model.current_step_date)))
}

@(private)
of_tmax_ge_40 :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	cd := model.climate_data[len(model.climate_data) - 1]
	if v, ok := cd[.tmax]; ok {
		return jx.i(v >= 40 ? 1 : 0)
	}
	return jx.i(0)
}

// --- crop module -----------------------------------------------------------

@(private)
of_ab_biom_n :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	if model.current_crop_module != nil {
		return jx.f(tl.round(core.get_aboveground_biomass_n_content(model.current_crop_module), 1))
	}
	return jx.f(0.0)
}

@(private)
of_eff_root_dep :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	if model.current_crop_module != nil {
		return jx.f(tl.round(core.get_effective_rooting_depth(model.current_crop_module), 2))
	}
	return jx.f(0.0)
}

@(private)
of_grain_n :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	if model.current_crop_module != nil {
		return jx.f(tl.round(core.get_fruit_biomass_n_content(model.current_crop_module), 5))
	}
	return jx.f(0.0)
}

@(private)
of_light_interception1 :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	if model.current_crop_module != nil {
		return jx.f(tl.round(core.get_fraction_of_intercepted_radiation1(model.current_crop_module), 5))
	}
	return jx.f(0.0)
}

@(private)
of_light_interception2 :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	if model.current_crop_module != nil {
		return jx.f(tl.round(core.get_fraction_of_intercepted_radiation2(model.current_crop_module), 5))
	}
	return jx.f(0.0)
}

@(private)
of_nresid :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	if model.current_crop_module != nil {
		return jx.f(tl.round(core.get_residues_n_content(model.current_crop_module), 1))
	}
	return jx.f(0.0)
}

// NOTE(c++-quirk): AbBiomNc/CritN are the *rounded* values (5 digits) the
// C++ compares and divides. Reproduced.
@(private)
of_nstress :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	nstress := 0.0
	cm := model.current_crop_module
	ab_biom_nc := cm != nil ? tl.round(cm.n_concentration_aboveground_biomass, 5) : 0.0
	crit_n := cm != nil ? tl.round(cm.critical_n_concentration, 5) : 0.0
	if cm != nil {
		nstress = ab_biom_nc < crit_n ? tl.round(ab_biom_nc / crit_n, 5) : 1
	}
	return jx.f(nstress)
}

@(private)
of_pot_nup :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	if model.current_crop_module != nil {
		return jx.f(tl.round(model.current_crop_module.crop_n_demand * 10000.0, 2))
	}
	return jx.f(0.0)
}

@(private)
of_protein :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	if model.current_crop_module != nil {
		return jx.f(tl.round(core.get_raw_protein_concentration(model.current_crop_module), 3))
	}
	return jx.f(0.0)
}

@(private)
of_ra :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	if model.current_crop_module != nil {
		return jx.f(tl.round(core.get_autotrophic_respiration(model.current_crop_module), 5))
	}
	return jx.f(0.0)
}

@(private)
of_secondary_yield :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	if model.current_crop_module != nil {
		return jx.f(tl.round(core.get_secondary_crop_yield(model.current_crop_module), 3))
	}
	return jx.f(0.0)
}

@(private)
of_yield_n :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	if model.current_crop_module != nil {
		return jx.f(tl.round(core.get_primary_yield_n_content(model.current_crop_module), 3))
	}
	return jx.f(0.0)
}

@(private)
of_yield_nc :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	if model.current_crop_module != nil {
		return jx.f(tl.round(core.get_primary_yield_n_concentration(model.current_crop_module), 3))
	}
	return jx.f(0.0)
}

@(private)
of_npp_organs :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	if organ_in_crop(model, oid) {
		return jx.f(tl.round(core.get_organ_specific_npp(model.current_crop_module, int(oid.organ)), 4))
	}
	return jx.f(0.0)
}

@(private)
of_ra_organs :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	if organ_in_crop(model, oid) {
		return jx.f(
			tl.round(core.get_organ_specific_total_respired(model.current_crop_module, int(oid.organ)), 4),
		)
	}
	return jx.f(0.0)
}

@(private)
of_org_green_biom :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	if organ_in_crop(model, oid) {
		return jx.f(tl.round(model.current_crop_module.organ_green_biomass[int(oid.organ)], 1))
	}
	return jx.f(0.0)
}

@(private)
act_nup_layer_get_value :: proc(model: ^core.Monica_Model, i: int) -> f64 {
	cm := model.current_crop_module
	return cm != nil ? cm.n_uptake_from_layer[i] * 10000.0 : 0.0
}

@(private)
of_act_nup_layer :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	return get_complex_values(model, oid, act_nup_layer_get_value, 4)
}

// --- soil organic (organic layers only) ------------------------------------

@(private)
actammoxrate_get_value :: proc(model: ^core.Monica_Model, i: int) -> f64 {
	return model.soil_organic.act_ammonia_oxidation_rate[i]
}

@(private)
of_actammoxrate :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	return get_complex_values(model, clamp_to_organic_layers(model, oid), actammoxrate_get_value, 6)
}

@(private)
actdenitrate_get_value :: proc(model: ^core.Monica_Model, i: int) -> f64 {
	return model.soil_organic.act_denitrification_rate[i]
}

@(private)
of_actdenitrate :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	return get_complex_values(model, clamp_to_organic_layers(model, oid), actdenitrate_get_value, 6)
}

@(private)
actnitrate_get_value :: proc(model: ^core.Monica_Model, i: int) -> f64 {
	return model.soil_organic.act_nitrification_rate[i]
}

@(private)
of_actnitrate :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	return get_complex_values(model, clamp_to_organic_layers(model, oid), actnitrate_get_value, 6)
}

@(private)
aomf_get_value :: proc(model: ^core.Monica_Model, i: int) -> f64 {
	return model.soil_organic.aom_fast_sum[i]
}

@(private)
of_aomf :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	return get_complex_values(model, clamp_to_organic_layers(model, oid), aomf_get_value, 4)
}

@(private)
aoms_get_value :: proc(model: ^core.Monica_Model, i: int) -> f64 {
	return model.soil_organic.aom_slow_sum[i]
}

@(private)
of_aoms :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	return get_complex_values(model, clamp_to_organic_layers(model, oid), aoms_get_value, 4)
}

@(private)
cbal_get_value :: proc(model: ^core.Monica_Model, i: int) -> f64 {
	return model.soil_organic.c_balance[i]
}

@(private)
of_cbal :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	return get_complex_values(model, clamp_to_organic_layers(model, oid), cbal_get_value, 4)
}

@(private)
co_get_value :: proc(model: ^core.Monica_Model, i: int) -> f64 {
	return core.soil_organic_get_soil_organic_c(&model.soil_organic, i)
}

@(private)
of_co :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	return get_complex_values(model, clamp_to_organic_layers(model, oid), co_get_value, 2)
}

@(private)
nmin_get_value :: proc(model: ^core.Monica_Model, i: int) -> f64 {
	return core.soil_organic_get_net_n_mineralisation_rate(&model.soil_organic, i)
}

@(private)
of_nmin :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	return get_complex_values(model, clamp_to_organic_layers(model, oid), nmin_get_value, 6)
}

@(private)
org_n_get_value :: proc(model: ^core.Monica_Model, i: int) -> f64 {
	return core.soil_organic_get_organic_n(&model.soil_organic, i)
}

@(private)
of_org_n :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	return get_complex_values(model, clamp_to_organic_layers(model, oid), org_n_get_value, 4)
}

@(private)
smb_co2_er_get_value :: proc(model: ^core.Monica_Model, i: int) -> f64 {
	return model.soil_organic.smb_co2_evolution_rate[i]
}

@(private)
of_smb_co2_er :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	return get_complex_values(model, clamp_to_organic_layers(model, oid), smb_co2_er_get_value, 1)
}

@(private)
smbf_get_value :: proc(model: ^core.Monica_Model, i: int) -> f64 {
	return model.soil_organic.soil_column.layers[i].smb_fast
}

@(private)
of_smbf :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	return get_complex_values(model, clamp_to_organic_layers(model, oid), smbf_get_value, 4)
}

@(private)
smbs_get_value :: proc(model: ^core.Monica_Model, i: int) -> f64 {
	return model.soil_organic.soil_column.layers[i].smb_slow
}

@(private)
of_smbs :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	return get_complex_values(model, clamp_to_organic_layers(model, oid), smbs_get_value, 4)
}

@(private)
somf_get_value :: proc(model: ^core.Monica_Model, i: int) -> f64 {
	return model.soil_organic.soil_column.layers[i].som_fast
}

@(private)
of_somf :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	return get_complex_values(model, clamp_to_organic_layers(model, oid), somf_get_value, 4)
}

@(private)
soms_get_value :: proc(model: ^core.Monica_Model, i: int) -> f64 {
	return model.soil_organic.soil_column.layers[i].som_slow
}

@(private)
of_soms :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	return get_complex_values(model, clamp_to_organic_layers(model, oid), soms_get_value, 4)
}

@(private)
of_denit :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	return jx.f(tl.round(core.soil_organic_get_denitrification(&model.soil_organic), 5))
}

@(private)
of_net_nmin :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	return jx.f(tl.round(core.soil_organic_get_net_n_mineralisation(&model.soil_organic), 5))
}

@(private)
of_nh3 :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	return jx.f(tl.round(core.soil_organic_get_nh3_volatilised(&model.soil_organic), 3))
}

@(private)
of_rh :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	return jx.f(tl.round(core.soil_organic_get_decomposer_respiration(&model.soil_organic), 5))
}

// --- soil column / moisture ------------------------------------------------

@(private)
awc_get_value :: proc(model: ^core.Monica_Model, i: int) -> f64 {
	fc := model.soil_column.layers[i].field_capacity
	pwp := model.soil_column.layers[i].permanent_wilting_point
	return fc - pwp
}

@(private)
of_awc :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	return get_complex_values(model, oid, awc_get_value, 4)
}

// C++ reads soilMoisture->soilColumn - the same SoilColumn as monica.soilColumn.
@(private)
pasw_get_value :: proc(model: ^core.Monica_Model, i: int) -> f64 {
	return model.soil_moisture.soil_column.layers[i].soil_moisture_m3 -
		model.soil_column.layers[i].permanent_wilting_point
}

@(private)
of_pasw :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	return get_complex_values(model, oid, pasw_get_value, 3)
}

@(private)
water_content_get_value :: proc(model: ^core.Monica_Model, i: int) -> f64 {
	smm3 := model.soil_moisture.soil_column.layers[i].soil_moisture_m3
	fc := model.soil_column.layers[i].field_capacity
	pwp := model.soil_column.layers[i].permanent_wilting_point
	return (smm3 - pwp) / (fc - pwp) //[%nFK]
}

@(private)
of_water_content :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	return get_complex_values(model, oid, water_content_get_value, 4)
}

@(private)
silt_get_value :: proc(model: ^core.Monica_Model, i: int) -> f64 {
	return core.soil_silt_content(&model.soil_column.layers[i])
}

@(private)
of_silt :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	return get_complex_values(model, oid, silt_get_value, 2)
}

@(private)
soc_x_y_get_value :: proc(model: ^core.Monica_Model, i: int) -> f64 {
	layer := &model.soil_column.layers[i]
	return core.soil_organic_carbon(layer) * core.soil_bulk_density(layer) * layer.layer_thickness_m * 1000
}

@(private)
of_soc_x_y :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	return get_complex_values(model, oid, soc_x_y_get_value, 4)
}

@(private)
of_pot_et :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	return jx.f(
		tl.round(model.soil_moisture.reference_evapotranspiration * model.soil_moisture.kc_factor, 1),
	)
}

// ---------------------------------------------------------------------------
// the primitive-calc expression: ["=", a, op, b] in a SetValue's "value"
// ---------------------------------------------------------------------------
//
// C++: getPrimitiveCalcOp / applyPrimitiveCalcOp / buildPrimitiveCalcExpression
// (build-output.{h,cpp}), i.e. buildExpression<double, Json> - the arithmetic
// half of the same template buildCompareExpression uses.
//
// Data + an eval proc rather than a std::function, for the usual reason: Odin
// proc values cannot capture. The C++ closes over `lf`/`rf`/`loid`/`roid`/`op`;
// Calc_Expr just stores them.

// C++: the operand slots of buildExpression - `leftj`/`rightj`, each either a
// literal number or an output id resolved per day.
Calc_Operand_Kind :: enum {
	NONE,
	CONSTANT,
	OID,
}

Calc_Operand :: struct {
	kind:  Calc_Operand_Kind,
	value: jx.Value, // CONSTANT
	oid:   OId, // OID
}

// C++: getPrimitiveCalcOp's four recognised operator strings, plus the
// fall-through it returns for anything else.
//
// NOTE(c++-quirk): an unrecognised operator is NOT an error there - the
// default `[](double, double) { return 0.0; }` is returned, and since
// buildExpression only tests `if (op)` (always true for a non-empty
// std::function) the expression builds fine and evaluates to 0.0 forever.
// ZERO reproduces that.
Calc_Op :: enum {
	ADD,
	SUB,
	MUL,
	DIV,
	ZERO,
}

// C++: the std::function buildPrimitiveCalcExpression returns. `set = false`
// is the empty std::function.
Calc_Expr :: struct {
	set:   bool,
	op:    Calc_Op,
	left:  Calc_Operand,
	right: Calc_Operand,
}

// C++: function<double(double,double)> monica::getPrimitiveCalcOp(string)
get_primitive_calc_op :: proc(ops: string) -> Calc_Op {
	switch ops {
	case "+":
		return .ADD
	case "-":
		return .SUB
	case "*":
		return .MUL
	case "/":
		return .DIV
	}
	return .ZERO
}

@(private)
calc_op_apply :: proc(op: Calc_Op, l, r: f64) -> f64 {
	switch op {
	case .ADD:
		return l + r
	case .SUB:
		return l - r
	case .MUL:
		return l * r
	case .DIV:
		return l / r
	case .ZERO:
	}
	return 0.0
}

// C++: Json monica::applyPrimitiveCalcOp(op, Json lj, Json rj)
//
// Four shapes, because either side can be the array a layer-range oid
// produces: scalar/scalar, array/scalar, scalar/array and array/array.
apply_primitive_calc_op :: proc(
	op: Calc_Op,
	lj, rj: jx.Value,
	allocator := context.allocator,
) -> jx.Value {
	if jx.is_number(lj) && jx.is_number(rj) {
		return jx.f(calc_op_apply(op, jx.number_value(lj), jx.number_value(rj)))
	}

	if jx.is_array(lj) && jx.is_number(rj) {
		rn := jx.number_value(rj)
		lja := jx.array_items(lj)
		res := make(jx.Array, 0, len(lja), allocator)
		for left in lja {
			append(
				&res,
				jx.f(jx.is_number(left) ? calc_op_apply(op, jx.number_value(left), rn) : 0.0),
			)
		}
		return jx.Value(res)
	}

	if jx.is_number(lj) && jx.is_array(rj) {
		ln := jx.number_value(lj)
		rja := jx.array_items(rj)
		res := make(jx.Array, 0, len(rja), allocator)
		for right in rja {
			append(
				&res,
				jx.f(jx.is_number(right) ? calc_op_apply(op, ln, jx.number_value(right)) : 0.0),
			)
		}
		return jx.Value(res)
	}

	if jx.is_array(lj) && jx.is_array(rj) {
		lja := jx.array_items(lj)
		rja := jx.array_items(rj)

		// NOTE(c++-quirk): the C++ array/array branch collects into a
		// `vector<bool>`, not a `vector<double>` - a copy-paste slip from its
		// applyCompareOp sibling directly above it. Every arithmetic result is
		// therefore narrowed to a boolean before toPrimJsonArray turns it into
		// JSON, so `[[1,2],"+",[10,20]]` yields [true, true] rather than
		// [11, 22], and any pair summing to exactly 0 yields false.
		// Reproduced, not fixed.
		//
		// NOTE(c++-quirk, NOT reproduced): the C++ transform() walks lja's full
		// length while reading rja through an unchecked iterator, so a shorter
		// right operand reads past the end of the vector - undefined behaviour,
		// not a value this port could match. Clamped to the shorter of the two.
		n := min(len(lja), len(rja))
		res := make(jx.Array, 0, n, allocator)
		for k in 0 ..< n {
			left, right := lja[k], rja[k]
			v :=
				jx.is_number(left) && jx.is_number(right) ? calc_op_apply(op, jx.number_value(left), jx.number_value(right)) : 0.0
			append(&res, jx.b(v != 0))
		}
		return jx.Value(res)
	}

	return jx.f(0.0)
}

// C++: buildExpression<double, Json>(a, getPrimitiveCalcOp, applyPrimitiveCalcOp)
//
// `a` is the ["=", a, op, b] array with the "=" already stripped, so exactly
// [left, op, right].
//
// The three accepted combinations are the C++'s, verbatim: oid/oid,
// oid/number, number/oid. Two literal numbers do NOT build - the C++ has no
// branch for it - and neither does an operand that names something with no
// getter.
build_primitive_calc_expression :: proc(
	a: []jx.Value,
	allocator := context.allocator,
) -> Calc_Expr {
	operand_ok :: proc(v: jx.Value) -> bool {
		return jx.is_number(v) || jx.is_string(v) || jx.is_array(v)
	}

	if len(a) != 3 || !operand_ok(a[0]) || !jx.is_string(a[1]) || !operand_ok(a[2]) {
		return Calc_Expr{}
	}

	op := get_primitive_calc_op(jx.string_value_of(a[1]))

	// A non-number operand is an output id spec - the same string/array forms
	// parse_output_ids takes everywhere else, so it reaches the path tier too:
	// ["=", "soilMoisture.actual_evaporation", "*", 2] works with no table
	// entry for either side.
	loid, roid: OId
	lf, rf: bool
	if !jx.is_number(a[0]) {
		if loids := parse_output_ids([]jx.Value{a[0]}, allocator = allocator); len(loids) > 0 {
			loid = loids[0]
			lf = oid_has_getter(loid)
		}
	}
	if !jx.is_number(a[2]) {
		if roids := parse_output_ids([]jx.Value{a[2]}, allocator = allocator); len(roids) > 0 {
			roid = roids[0]
			rf = oid_has_getter(roid)
		}
	}

	if lf && rf {
		return Calc_Expr {
			set = true,
			op = op,
			left = {kind = .OID, oid = loid},
			right = {kind = .OID, oid = roid},
		}
	}
	if lf && jx.is_number(a[2]) {
		return Calc_Expr {
			set = true,
			op = op,
			left = {kind = .OID, oid = loid},
			right = {kind = .CONSTANT, value = a[2]},
		}
	}
	if jx.is_number(a[0]) && rf {
		return Calc_Expr {
			set = true,
			op = op,
			left = {kind = .CONSTANT, value = a[0]},
			right = {kind = .OID, oid = roid},
		}
	}
	return Calc_Expr{}
}

@(private)
calc_operand_eval :: proc(
	o: Calc_Operand,
	model: ^core.Monica_Model,
	allocator: jx.Allocator,
) -> jx.Value {
	switch o.kind {
	case .CONSTANT:
		return o.value
	case .OID:
		if v, ok := oid_get_value(model, o.oid, allocator); ok {
			return v
		}
	case .NONE:
	}
	return jx.Value{}
}

// C++: the lambda buildExpression returns - `applyOp(op, lf(m, loid), rf(m, roid))`.
calc_expr_eval :: proc(
	e: Calc_Expr,
	model: ^core.Monica_Model,
	allocator := context.allocator,
) -> jx.Value {
	return apply_primitive_calc_op(
		e.op,
		calc_operand_eval(e.left, model, allocator),
		calc_operand_eval(e.right, model, allocator),
		allocator,
	)
}

// C++: struct BOTRes
BOT_Res :: struct {
	ofs:           map[int]proc(_: ^core.Monica_Model, _: OId) -> jx.Value,
	setfs:         map[int]proc(_: ^core.Monica_Model, _: OId, _: jx.Value),
	name2metadata: map[string]OutputMetadata,
}

@(private)
g_output_table: BOT_Res
@(private)
g_output_table_built: bool

// C++: BOTRes& monica::buildOutputTable()
build_output_table :: proc(allocator := context.allocator) -> ^BOT_Res {
	if g_output_table_built {
		return &g_output_table
	}

	build :: proc(
		id: int,
		name: string,
		unit: string,
		description: string,
		of: proc(_: ^core.Monica_Model, _: OId) -> jx.Value,
		setf: proc(_: ^core.Monica_Model, _: OId, _: jx.Value) = nil,
	) {
		g_output_table.ofs[id] = of
		if setf != nil {
			g_output_table.setfs[id] = setf
		}
		g_output_table.name2metadata[name] = OutputMetadata {
			id          = id,
			name        = name,
			unit        = unit,
			description = description,
		}
	}

	// Built once and read for the rest of the process (C++: a function-local
	// static BOTRes), so NOT from `allocator` - in a server that is one request's
	// arena, and reading the table back on the next request would be a use after
	// free. See tools.process_cache_allocator. Everything stored is a procedure
	// pointer or a string literal, so none of it depends on the caller's data.
	cache_allocator := tl.process_cache_allocator()
	g_output_table.ofs = make(
		map[int]proc(_: ^core.Monica_Model, _: OId) -> jx.Value,
		cache_allocator,
	)
	g_output_table.setfs = make(
		map[int]proc(_: ^core.Monica_Model, _: OId, _: jx.Value),
		cache_allocator,
	)
	g_output_table.name2metadata = make(map[string]OutputMetadata, cache_allocator)

	build(
		0,
		"CM-count",
		"",
		"output the order number of the current cultivation method",
		of_cm_count,
	)
	build(1, "Date", "", "output current date", of_date)
	build(2, "Year", "", "output current Year", of_year)
	build(3, "Crop", "", "crop name", of_crop)
	build(4, "Stage", "1-6/7", "DevelopmentalStage", of_stage, of_stage_set)
	build(5, "AbBiom", "kgDM ha-1", "AbovegroundBiomass", of_ab_biom)
	build(6, "OrgBiom", "kgDM ha-1", "get_OrganBiomass(i)", of_org_biom)
	build(7, "Yield", "kgDM ha-1", "get_PrimaryCropYield", of_yield)
	build(8, "LAI", "m2 m-2", "LeafAreaIndex", of_lai)
	build(9, "Mois", "m3 m-3", "Soil moisture content", of_mois, of_mois_set)
	build(10, "Irrig", "mm", "Irrigation", of_irrig)
	build(11, "RunOff", "mm", "Surface runoff of current day", of_runoff)
	build(
		12,
		"Kc",
		"",
		"plant coefficient to calculate with ET0 the plants water use (ET0 * Kc)",
		of_kc,
	)
	build(13, "Recharge", "mm", "Groundwater recharge", of_recharge)
	build(14, "NLeach", "kgN ha-1", "N leaching", of_nleach)
	build(15, "SOC", "kgC kg-1", "get soil organic carbon content", of_soc)
	build(16, "Tavg", "", "", of_tavg)
	build(17, "Precip", "mm", "Precipitation", of_precip)
	build(18, "Globrad", "", "", of_globrad)
	build(19, "N", "kgN m-3", "", of_n)
	build(20, "ETa/ETc", "", "Act_ET / Pot_ET", of_eta_etc)
	// Ported on demand: the Hohenfinow2 Python pipeline (test_env.json) asks for
	// "DOY|S2_10_DOY" etc.; without it those columns were silently dropped.
	build(21, "DOY", "", "output current day of year", of_doy)
	// The rest of the C++ table's computed outputs; names, units and
	// descriptions exactly as in src/io/build-output.cpp.
	build(22, "Count", "", "output 1 for counting things", of_count)
	build(23, "days-since-start", "", "output number of days since simulation start", of_days_since_start)
	build(24, "Month", "", "output current Month", of_month)
	build(25, "Tmax>=40", "0|1", "if Tmax >= 40°C then 1 else 0", of_tmax_ge_40)
	build(26, "AbBiomN", "kgN ha-1", "AbovegroundBiomassNContent", of_ab_biom_n)
	build(27, "EffRootDep", "m", "Effective RootingDepth", of_eff_root_dep)
	build(28, "GrainN", "kg ha-1", "get_FruitBiomassNContent", of_grain_n)
	build(29, "LightInterception1", "", "LightInterception of single crop or top layer of taller crop", of_light_interception1)
	build(30, "LightInterception2", "", "LightInterception of lower layer of taller crop", of_light_interception2)
	build(31, "Nresid", "kg N ha-1", "Nitrogen content in crop residues", of_nresid)
	build(32, "Nstress", "-", "NitrogenStressIndex", of_nstress)
	build(33, "PotNup", "kgN ha-1", "PotNUptake", of_pot_nup)
	build(34, "Protein", "kg kg-1", "RawProteinConcentration", of_protein)
	build(35, "Ra", "kgC ha-1", "autotrophic respiration", of_ra)
	build(36, "SecondaryYield", "kgDM ha-1", "get_SecondaryCropYield", of_secondary_yield)
	build(37, "YieldN", "kgN ha-1", "PrimaryYieldNContent", of_yield_n)
	build(38, "YieldNc", "kgN ha-1", "PrimaryYieldNConcentration", of_yield_nc)
	build(39, "NPP-Organs", "kgC ha-1", "organ specific NPP", of_npp_organs)
	build(40, "Ra-Organs", "kgC ha-1", "organ specific autotrophic respiration", of_ra_organs)
	build(41, "OrgGreenBiom", "kgDM ha-1", "get_OrganGreenBiomass(i)", of_org_green_biom)
	build(42, "ActNupLayer", "KgN ha-1", "ActNUptakefromLayer", of_act_nup_layer)
	build(43, "actammoxrate", "kgN/m3/d", "actual ammonia oxidation rate in layer", of_actammoxrate)
	build(44, "actdenitrate", "kgN/m3/d", "actual denitrification rate in layer", of_actdenitrate)
	build(45, "actnitrate", "kgN/m3/d", "actual nitrification rate in layer", of_actnitrate)
	build(46, "AOMf", "kgC m-3", "get_AOM_FastSum", of_aomf)
	build(47, "AOMs", "kgC m-3", "get_AOM_SlowSum", of_aoms)
	build(48, "CBal", "kgC m-3", "get_CBalance", of_cbal)
	build(49, "Co", "kgC m-3", "", of_co)
	build(50, "Nmin", "kgN ha-1", "NetNMineralisationRate", of_nmin)
	build(51, "OrgN", "kg N m-3", "get_Organic_N", of_org_n)
	build(52, "SMB-CO2-ER", "", "soilOrganic.get_SMB_CO2EvolutionRate", of_smb_co2_er)
	build(53, "SMBf", "kgC m-3", "get_SMB_Fast", of_smbf)
	build(54, "SMBs", "kgC m-3", "get_SMB_Slow", of_smbs)
	build(55, "SOMf", "kgC m-3", "get_SOM_Fast", of_somf)
	build(56, "SOMs", "kgC m-3", "get_SOM_Slow", of_soms)
	build(57, "Denit", "kgN ha-1", "Denit", of_denit)
	build(58, "NetNmin", "kgN ha-1", "NetNmin", of_net_nmin)
	build(59, "NH3", "kgN ha-1", "NH3_Volatilised", of_nh3)
	build(60, "Rh", "kgC ha-", "Rh", of_rh)
	build(61, "AWC", "m3 m-3", "available water capacity", of_awc)
	build(62, "PASW", "m3 m-3", "PASW", of_pasw)
	build(63, "WaterContent", "fraction nFC", "soil water content in % of available soil water", of_water_content)
	build(64, "Silt", "kg kg-1", "Soil silt content", of_silt)
	build(65, "SOC-X-Y", "gC m-2", "SOC-X-Y", of_soc_x_y)
	build(66, "Pot_ET", "mm", "potential evapotranspiration = ET0 * Kc = the plants water use", of_pot_et)

	g_output_table_built = true
	return &g_output_table
}

// Test-only teardown for the two lazily-built package globals
// (build_output_table's three maps and legacy_aliases' one).
//
// monica-run builds each exactly once and lets the process exit own them, so
// nothing in production calls this. `odin test` runs under a tracking
// allocator, though, and a one-time global init it cannot tell apart from a
// leak becomes a permanent WARN that trains everyone to ignore leak reports.
destroy_output_tables :: proc() {
	delete(g_output_table.ofs)
	delete(g_output_table.setfs)
	delete(g_output_table.name2metadata)
	g_output_table = BOT_Res{}
	g_output_table_built = false

	delete(g_legacy_aliases)
	g_legacy_aliases = nil
	g_legacy_aliases_built = false
}
