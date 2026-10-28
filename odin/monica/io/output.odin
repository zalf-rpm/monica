// Phase 7 checkpoint 1: src/io/output.h/.cpp - the OId/Output data model.
//
// oid::merge/to_json, Output::merge/to_json, and every makeOId(...) factory
// overload are not ported: their only callers are RPC/env-round-trip paths
// (Cap'n Proto bridging, env_to_json debug dumps) this port drops - the CSV
// write path this phase targets builds OId values directly via
// parseOutputIds (build_output.odin) and never serializes them back to
// JSON. "Port on demand" if a later feature needs them.
package monica_io

import "core:fmt"
import "core:strings"
import jx "../../support/jsonx"
import rp "../../support/reflectpath"

// C++: enum OId::OP
OId_Op :: enum {
	AVG,
	MEDIAN,
	SUM,
	MIN,
	MAX,
	FIRST,
	LAST,
	NONE,
	UNDEFINED_OP,
}

// C++: enum OId::ORGAN
OId_Organ :: enum {
	ROOT,
	LEAF,
	SHOOT,
	FRUIT,
	STRUCT,
	SUGAR,
	UNDEFINED_ORGAN,
}

// How a reflection-backed oid's leaf is turned into the JSON the CSV writer
// prints. A handful of legacy entries truncate (RootDep is
// `int(vc_RootingDepth)`); without CAST_INT those columns would gain decimals.
OId_Cast :: enum {
	NONE,
	INT,
}

// C++: struct monica::OId
//
// path/plan/roundToDigits/castTo are this port's extension, not in the C++:
// they are what let an oid name a field by *path* rather than by a
// pre-registered lambda id (plan-reflective-outputs.md §2.3). An oid is
// reflection-backed iff `plan != nil`; everything else still goes through
// build_output_table's ofs/setfs keyed by `id`.
OId :: struct {
	id:              int, // C++ default: -1
	name:            string,
	display_name:    string,
	unit:            string,
	json_input:      string,
	layer_agg_op:    OId_Op, // C++ default: NONE
	time_agg_op:     OId_Op, // C++ default: AVG
	organ:           OId_Organ, // C++ default: UNDEFINED_ORGAN
	from_layer:      int, // C++ default: -1
	to_layer:        int, // C++ default: -1

	path:            string, // "" for lambda-backed oids
	// OId is copied by value all over this port (`oid := oid_in`), so the
	// plan MUST be a pointer into a setup-time allocation and never an owned
	// value - see the risk register in plan-reflective-outputs.md §8.
	plan:            ^rp.Path_Plan, // nil for lambda-backed oids
	round_to_digits: int, // -1 = do not round
	cast_to:         OId_Cast,
	// the object form's "calc" (output_calc.odin), applied per day before
	// roundToDigits/castTo; empty for a plain read
	calc:            []Calc_Step,
}

// C++ in-class defaults: id{-1}, layerAggOp{NONE}, timeAggOp{AVG},
// organ{_UNDEFINED_ORGAN_}, fromLayer{-1}, toLayer{-1}
make_default_oid :: proc() -> OId {
	return OId {
		id = -1,
		layer_agg_op = .NONE,
		time_agg_op = .AVG,
		organ = .UNDEFINED_ORGAN,
		from_layer = -1,
		to_layer = -1,
		round_to_digits = -1,
		cast_to = .NONE,
	}
}

// C++: inline bool oid::isRange(const OId*)
oid_is_range :: proc(oid: ^OId) -> bool {
	return oid.from_layer >= 0 && oid.to_layer >= 0
}

// C++: inline bool oid::isOrgan(const OId*)
oid_is_organ :: proc(oid: ^OId) -> bool {
	return oid.organ != .UNDEFINED_ORGAN
}

// C++: string oid::toString(const OId*, OId::OP)
oid_op_to_string :: proc(op: OId_Op) -> string {
	switch op {
	case .AVG:
		return "AVG"
	case .MEDIAN:
		return "MEDIAN"
	case .SUM:
		return "SUM"
	case .MIN:
		return "MIN"
	case .MAX:
		return "MAX"
	case .FIRST:
		return "FIRST"
	case .LAST:
		return "LAST"
	case .NONE:
		return "NONE"
	case .UNDEFINED_OP:
	}
	return "undef"
}

// C++: string oid::toString(const OId*, OId::ORGAN)
oid_organ_to_string :: proc(organ: OId_Organ) -> string {
	switch organ {
	case .ROOT:
		return "Root"
	case .LEAF:
		return "Leaf"
	case .SHOOT:
		return "Shoot"
	case .FRUIT:
		return "Fruit"
	case .STRUCT:
		return "Struct"
	case .SUGAR:
		return "Sugar"
	case .UNDEFINED_ORGAN:
	}
	return "undef"
}

// C++: string oid::toString(const OId*, bool)
oid_to_string :: proc(
	oid: ^OId,
	include_time_agg: bool = false,
	allocator := context.allocator,
) -> string {
	b := strings.builder_make(allocator)
	strings.write_string(&b, "[")
	strings.write_string(&b, oid.name)
	if oid_is_organ(oid) {
		strings.write_string(&b, ", ")
		strings.write_string(&b, oid_organ_to_string(oid.organ))
	} else if oid_is_range(oid) {
		fmt.sbprintf(&b, ", [%d, %d", oid.from_layer + 1, oid.to_layer + 1)
		if oid.layer_agg_op != .NONE {
			strings.write_string(&b, ", ")
			strings.write_string(&b, oid_op_to_string(oid.layer_agg_op))
		}
		strings.write_string(&b, "]")
	} else if oid.from_layer >= 0 {
		fmt.sbprintf(&b, ", %d", oid.from_layer + 1)
	}
	if include_time_agg {
		strings.write_string(&b, ", ")
		strings.write_string(&b, oid_op_to_string(oid.time_agg_op))
	}
	strings.write_string(&b, "]")
	return strings.to_string(b)
}

// C++: string oid::outputName(const OId*)
oid_output_name :: proc(oid: ^OId, allocator := context.allocator) -> string {
	out_name := oid.name
	if oid_is_organ(oid) {
		out_name = strings.concatenate({out_name, "/", oid_organ_to_string(oid.organ)}, allocator)
	}
	if oid.display_name != "" {
		out_name = oid.display_name
	}
	return out_name
}

// C++: struct Output::Data
//
// Exactly one of results / resultsObj is filled, depending on the env's
// "obj-outputs?" (see run.env_return_obj_outputs): results is one column per
// output id, resultsObj one JSON object per row, keyed by oid_output_name.
Output_Data :: struct {
	orig_spec:   string,
	output_ids:  [dynamic]OId,
	results:     [dynamic][dynamic]jx.Value,
	results_obj: [dynamic]jx.Value,
}

// C++: struct monica::Output
//
// customId/errors/warnings were dropped by phase 7 ("nothing in the CSV
// write path reads them" - "port on demand"); the ZMQ server phase is that
// demand, since serve-monica-zmq.cpp round-trips customId and reports
// env-merge/climate-read errors back over the wire via output::to_json.
Output :: struct {
	data:      [dynamic]Output_Data,
	custom_id: jx.Value,
	errors:    [dynamic]string,
	warnings:  [dynamic]string,
}

// C++: Output monica::makeOutput(std::string error)
//
// The makeOutput(json11::Json) overload alongside it in src/io/output.cpp is
// not ported - nothing calls it here (it needs output::merge, itself unported).
make_output :: proc(error: string, allocator := context.allocator) -> Output {
	output: Output
	output.errors = make([dynamic]string, allocator)
	append(&output.errors, error)
	return output
}

// C++: json11::Json oid::to_json(const OId*)
oid_to_json :: proc(oid: ^OId, allocator := context.allocator) -> jx.Value {
	return jx.obj(
		allocator,
		{"type", jx.sl("OId")},
		{"id", jx.i(oid.id)},
		{"name", jx.s(oid.name, allocator)},
		{"displayName", jx.s(oid.display_name, allocator)},
		{"unit", jx.s(oid.unit, allocator)},
		{"jsonInput", jx.s(oid.json_input, allocator)},
		{"path", jx.s(oid.path, allocator)},
		{"layerAggOp", jx.i(int(oid.layer_agg_op))},
		{"timeAggOp", jx.i(int(oid.time_agg_op))},
		{"organ", jx.i(int(oid.organ))},
		{"fromLayer", jx.i(oid.from_layer)},
		{"toLayer", jx.i(oid.to_layer)},
	)
}

// C++: json11::Json output::to_json(const Output*)
output_to_json :: proc(out: ^Output, allocator := context.allocator) -> jx.Value {
	ds := make(jx.Array, 0, len(out.data), allocator)
	for &dd in out.data {
		rs := make(jx.Array, 0, max(len(dd.results), len(dd.results_obj)), allocator)
		if len(dd.results) > 0 {
			for r in dd.results {
				row := make(jx.Array, 0, len(r), allocator)
				append(&row, ..r[:])
				append(&rs, jx.Value(row))
			}
		} else {
			append(&rs, ..dd.results_obj[:])
		}
		output_ids := make(jx.Array, 0, len(dd.output_ids), allocator)
		for &o in dd.output_ids {
			append(&output_ids, oid_to_json(&o, allocator))
		}
		append(
			&ds,
			jx.obj(
				allocator,
				{"origSpec", jx.s(dd.orig_spec, allocator)},
				{"outputIds", jx.Value(output_ids)},
				{"results", jx.Value(rs)},
			),
		)
	}

	errs := make(jx.Array, 0, len(out.errors), allocator)
	for e in out.errors {
		append(&errs, jx.s(e, allocator))
	}
	warns := make(jx.Array, 0, len(out.warnings), allocator)
	for w in out.warnings {
		append(&warns, jx.s(w, allocator))
	}

	return jx.obj(
		allocator,
		{"type", jx.sl("Output")},
		{"customId", out.custom_id},
		{"data", jx.Value(ds)},
		{"errors", jx.Value(errs)},
		{"warnings", jx.Value(warns)},
	)
}
