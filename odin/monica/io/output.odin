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

// C++: struct monica::OId
OId :: struct {
	id:          int, // C++ default: -1
	name:        string,
	displayName: string,
	unit:        string,
	jsonInput:   string,
	layerAggOp:  OId_Op, // C++ default: NONE
	timeAggOp:   OId_Op, // C++ default: AVG
	organ:       OId_Organ, // C++ default: UNDEFINED_ORGAN
	fromLayer:   int, // C++ default: -1
	toLayer:     int, // C++ default: -1
}

// C++ in-class defaults: id{-1}, layerAggOp{NONE}, timeAggOp{AVG},
// organ{_UNDEFINED_ORGAN_}, fromLayer{-1}, toLayer{-1}
make_default_oid :: proc() -> OId {
	return OId {
		id = -1,
		layerAggOp = .NONE,
		timeAggOp = .AVG,
		organ = .UNDEFINED_ORGAN,
		fromLayer = -1,
		toLayer = -1,
	}
}

// C++: inline bool oid::isRange(const OId*)
oid_is_range :: proc(oid: ^OId) -> bool {
	return oid.fromLayer >= 0 && oid.toLayer >= 0
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
	includeTimeAgg: bool = false,
	allocator := context.allocator,
) -> string {
	b := strings.builder_make(allocator)
	strings.write_string(&b, "[")
	strings.write_string(&b, oid.name)
	if oid_is_organ(oid) {
		strings.write_string(&b, ", ")
		strings.write_string(&b, oid_organ_to_string(oid.organ))
	} else if oid_is_range(oid) {
		fmt.sbprintf(&b, ", [%d, %d", oid.fromLayer + 1, oid.toLayer + 1)
		if oid.layerAggOp != .NONE {
			strings.write_string(&b, ", ")
			strings.write_string(&b, oid_op_to_string(oid.layerAggOp))
		}
		strings.write_string(&b, "]")
	} else if oid.fromLayer >= 0 {
		fmt.sbprintf(&b, ", %d", oid.fromLayer + 1)
	}
	if includeTimeAgg {
		strings.write_string(&b, ", ")
		strings.write_string(&b, oid_op_to_string(oid.timeAggOp))
	}
	strings.write_string(&b, "]")
	return strings.to_string(b)
}

// C++: string oid::outputName(const OId*)
oid_output_name :: proc(oid: ^OId, allocator := context.allocator) -> string {
	outName := oid.name
	if oid_is_organ(oid) {
		outName = strings.concatenate({outName, "/", oid_organ_to_string(oid.organ)}, allocator)
	}
	if oid.displayName != "" {
		outName = oid.displayName
	}
	return outName
}

// C++: struct Output::Data
//
// resultsObj (the obj-outputs? path) is not ported: sim-min.json never sets
// "obj-outputs?" (defaults false), so env_return_obj_outputs is always
// false for this port's fixture - "port on demand".
Output_Data :: struct {
	origSpec:  string,
	outputIds: [dynamic]OId,
	results:   [dynamic][dynamic]jx.Value,
}

// C++: struct monica::Output
//
// customId/errors/warnings are not ported: sim-min.json never sets a
// customId, and nothing in the CSV write path reads Output::errors/
// warnings - "port on demand".
Output :: struct {
	data: [dynamic]Output_Data,
}
