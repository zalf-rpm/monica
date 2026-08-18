// Phase 7 checkpoint 2: src/io/build-output.h/.cpp - a 21-entry replacement
// for the ~300-entry buildOutputTable, exactly the ids sim-min.json's
// output.events section needs (plan-odin.md's own scoping call): CM-count,
// Date, Year, Crop, Stage, AbBiom, OrgBiom, Yield, LAI, Mois, Irrig, RunOff,
// Kc, Recharge, NLeach, SOC, Tavg, Precip, Globrad, N, ETa/ETc.
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

import "core:strings"
import core "../core"
import p "../params"
import jx "../../support/jsonx"
import tl "../../support/tools"
import d "../../support/date"

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

// C++: template<typename T> Json getComplexValues(OId, function<T(int)>, int)
//
// get_value takes `model` as an explicit parameter instead of capturing it -
// Odin `proc` values can't close over anything, the same capture-workaround
// shape used throughout this port since phase 5.
get_complex_values :: proc(
	model: ^core.Monica_Model,
	oid_in: OId,
	get_value: proc(_: ^core.Monica_Model, _: int) -> f64,
	roundToDigits: int = 0,
	allocator := context.allocator,
) -> jx.Value {
	oid := oid_in
	if oid_is_organ(&oid) {
		oid.toLayer = int(oid.organ)
		oid.fromLayer = int(oid.organ)
	}

	multipleValues := make([dynamic]jx.Value, 0, allocator)
	vs := make([dynamic]f64, 0, allocator)
	for i := oid.fromLayer; i <= oid.toLayer; i += 1 {
		v := 0.0
		if i >= 0 {
			v = get_value(model, i)
		}
		if oid.layerAggOp == .NONE {
			append(&multipleValues, jx.f(tl.round(v, roundToDigits)))
		} else {
			append(&vs, v)
		}
	}

	if oid.layerAggOp == .NONE {
		return jx.Value(jx.Array(multipleValues))
	}
	return jx.f(apply_oid_op(oid.layerAggOp, vs[:]))
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
		oid.toLayer = int(oid.organ)
		oid.fromLayer = int(oid.organ)
	}

	if jx.is_object(value) || jx.is_null(value) {
		return
	}

	values: []jx.Value
	if jx.is_array(value) {
		values = jx.array_items(value)
	} else {
		n := oid.toLayer - oid.fromLayer + 1
		fill := make([]jx.Value, max(n, 0), allocator)
		for i in 0 ..< len(fill) {
			fill[i] = value
		}
		values = fill
	}

	k := 0
	for i := oid.fromLayer; i <= oid.toLayer && k < len(values); i, k = i + 1, k + 1 {
		if i >= 0 {
			set_value(model, i, values[k])
		}
	}
}

// C++: vector<OId> monica::parseOutputIds(const Tools::J11Array&)
parse_output_ids :: proc(oidArray: []jx.Value, allocator := context.allocator) -> [dynamic]OId {
	outputIds := make([dynamic]OId, 0, allocator)

	get_aggregation_op :: proc(arr: []jx.Value, index: int, def: OId_Op = .UNDEFINED_OP) -> OId_Op {
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

	get_organ :: proc(arr: []jx.Value, index: int, def: OId_Organ = .UNDEFINED_ORGAN) -> OId_Organ {
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

	split2 :: proc(name: string, allocator: jx.Allocator) -> (n0: string, n1: string) {
		parts := strings.split(name, "|", allocator)
		n0 = len(parts) > 0 ? parts[0] : ""
		n1 = len(parts) > 1 ? parts[1] : ""
		return
	}

	name2metadata := build_output_table().name2metadata

	for idj in oidArray {
		if jx.is_string(idj) {
			name := jx.string_value_of(idj)
			n0, n1 := split2(name, allocator)
			if data, ok := name2metadata[n0]; ok {
				oid := make_default_oid()
				oid.id = data.id
				oid.name = data.name
				oid.displayName = n1
				oid.unit = data.unit
				oid.jsonInput = name
				append(&outputIds, oid)
			}
		} else if jx.is_array(idj) {
			arr := jx.array_items(idj)
			if len(arr) >= 1 {
				name := jx.string_value_of(arr[0])
				n0, n1 := split2(name, allocator)
				if data, ok := name2metadata[n0]; ok {
					oid := make_default_oid()
					oid.id = data.id
					oid.name = data.name
					oid.displayName = n1
					oid.unit = data.unit
					oid.jsonInput = jx.dump(idj, allocator)

					if len(arr) >= 2 {
						val1 := arr[1]
						if jx.is_number(val1) {
							oid.fromLayer = jx.int_value_of(val1) - 1
							oid.toLayer = oid.fromLayer
						} else if jx.is_string(val1) {
							op := get_aggregation_op(arr, 1)
							if op != .UNDEFINED_OP {
								oid.timeAggOp = op
							} else {
								oid.organ = get_organ(arr, 1, .UNDEFINED_ORGAN)
							}
						} else if jx.is_array(val1) {
							arr2 := jx.array_items(val1)
							if len(arr2) >= 1 {
								val1_0 := arr2[0]
								if jx.is_number(val1_0) {
									oid.fromLayer = jx.int_value_of(val1_0) - 1
								} else if jx.is_string(val1_0) {
									oid.organ = get_organ(arr2, 0, .UNDEFINED_ORGAN)
								}
							}
							if len(arr2) >= 2 {
								val1_1 := arr2[1]
								if jx.is_number(val1_1) {
									oid.toLayer = jx.int_value_of(val1_1) - 1
								} else if jx.is_string(val1_1) {
									oid.toLayer = oid.fromLayer
									oid.layerAggOp = get_aggregation_op(arr2, 1, .AVG)
								}
							}
							if len(arr2) >= 3 {
								oid.layerAggOp = get_aggregation_op(arr2, 2, .AVG)
							}
						}
					}
					if len(arr) >= 3 {
						oid.timeAggOp = get_aggregation_op(arr, 2, .AVG)
					}

					append(&outputIds, oid)
				}
			}
		}
	}

	return outputIds
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
	return jx.i(model.cultivationMethodCount)
}

@(private)
of_date :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	return jx.s(d.to_iso_date_string(model.currentStepDate))
}

@(private)
of_year :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	return jx.i(d.year(model.currentStepDate))
}

@(private)
of_crop :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	if model.currentCropModule != nil {
		return jx.s(p.crop_name(&model.currentCropModule.cropParams, context.allocator))
	}
	return jx.s("")
}

@(private)
of_stage :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	if model.currentCropModule != nil {
		return jx.i(model.currentCropModule.vc_DevelopmentalStage + 1)
	}
	return jx.i(0)
}

@(private)
of_stage_set :: proc(model: ^core.Monica_Model, oid: OId, value: jx.Value) {
	if jx.is_number(value) && model.currentCropModule != nil {
		core.set_stage(model.currentCropModule, max(0, jx.int_value_of(value) - 1))
	}
}

@(private)
of_ab_biom :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	if model.currentCropModule != nil {
		return jx.f(tl.round(model.currentCropModule.vc_AbovegroundBiomass, 1))
	}
	return jx.f(0.0)
}

@(private)
of_org_biom :: proc(model: ^core.Monica_Model, oid_in: OId) -> jx.Value {
	oid := oid_in
	if oid_is_organ(&oid) &&
	   model.currentCropModule != nil &&
	   p.species_parameters_number_of_organs(&model.currentCropModule.cropParams.speciesParams) >
			   int(oid.organ) {
		return jx.f(tl.round(model.currentCropModule.vc_OrganBiomass[int(oid.organ)], 1))
	}
	return jx.f(0.0)
}

@(private)
of_yield :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	if model.currentCropModule != nil {
		return jx.f(tl.round(core.get_primary_crop_yield(model.currentCropModule), 1))
	}
	return jx.f(0.0)
}

@(private)
of_lai :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	if model.currentCropModule != nil {
		return jx.f(tl.round(model.currentCropModule.vc_LeafAreaIndex, 4))
	}
	return jx.f(0.0)
}

@(private)
mois_get_value :: proc(model: ^core.Monica_Model, i: int) -> f64 {
	return model.soilColumn.layers[i].vs_SoilMoisture_m3
}

@(private)
of_mois :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	return get_complex_values(model, oid, mois_get_value, 3)
}

@(private)
mois_set_value :: proc(model: ^core.Monica_Model, i: int, value: jx.Value) {
	if jx.is_number(value) {
		model.soilColumn.layers[i].vs_SoilMoisture_m3 = jx.number_value(value)
	}
}

@(private)
of_mois_set :: proc(model: ^core.Monica_Model, oid: OId, value: jx.Value) {
	set_complex_values(model, oid, mois_set_value, value)
}

@(private)
of_irrig :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	return jx.f(tl.round(model.dailySumIrrigationWater, 3))
}

@(private)
of_runoff :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	return jx.f(tl.round(model.soilMoisture.vm_SurfaceRunOff, 1))
}

@(private)
of_kc :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	return jx.f(tl.round(model.soilMoisture.vc_KcFactor, 3))
}

@(private)
of_recharge :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	return jx.f(tl.round(model.soilMoisture.vm_FluxAtLowerBoundary, 3))
}

@(private)
of_nleach :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	return jx.f(tl.round(model.soilTransport.vq_LeachingAtBoundary, 3))
}

@(private)
soc_get_value :: proc(model: ^core.Monica_Model, i: int) -> f64 {
	return core.soil_organic_carbon(&model.soilColumn.layers[i])
}

@(private)
of_soc :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	return get_complex_values(model, oid, soc_get_value, 6)
}

@(private)
of_tavg :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	cd := model.climateData[len(model.climateData) - 1]
	if v, ok := cd[.tavg]; ok {
		return jx.f(tl.round(v, 4))
	}
	return jx.f(0.0)
}

@(private)
of_precip :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	cd := model.climateData[len(model.climateData) - 1]
	if v, ok := cd[.precip]; ok {
		return jx.f(tl.round(v, 4))
	}
	return jx.f(0.0)
}

@(private)
of_globrad :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	cd := model.climateData[len(model.climateData) - 1]
	if v, ok := cd[.globrad]; ok {
		return jx.f(tl.round(v, 4))
	}
	return jx.f(0.0)
}

@(private)
n_get_value :: proc(model: ^core.Monica_Model, i: int) -> f64 {
	return core.soil_nmin(&model.soilColumn.layers[i])
}

@(private)
of_n :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	return get_complex_values(model, oid, n_get_value, 3)
}

@(private)
of_eta_etc :: proc(model: ^core.Monica_Model, oid: OId) -> jx.Value {
	potET := model.soilMoisture.vm_ReferenceEvapotranspiration * model.soilMoisture.vc_KcFactor
	actET := model.soilMoisture.vm_ActualEvapotranspiration
	if potET > 0 {
		return jx.f(tl.round(actET / potET, 2))
	}
	return jx.f(1.0)
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

	g_output_table.ofs = make(map[int]proc(_: ^core.Monica_Model, _: OId) -> jx.Value, allocator)
	g_output_table.setfs = make(map[int]proc(_: ^core.Monica_Model, _: OId, _: jx.Value), allocator)
	g_output_table.name2metadata = make(map[string]OutputMetadata, allocator)

	build(0, "CM-count", "", "output the order number of the current cultivation method", of_cm_count)
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
	build(12, "Kc", "", "plant coefficient to calculate with ET0 the plants water use (ET0 * Kc)", of_kc)
	build(13, "Recharge", "mm", "Groundwater recharge", of_recharge)
	build(14, "NLeach", "kgN ha-1", "N leaching", of_nleach)
	build(15, "SOC", "kgC kg-1", "get soil organic carbon content", of_soc)
	build(16, "Tavg", "", "", of_tavg)
	build(17, "Precip", "mm", "Precipitation", of_precip)
	build(18, "Globrad", "", "", of_globrad)
	build(19, "N", "kgN m-3", "", of_n)
	build(20, "ETa/ETc", "", "Act_ET / Pot_ET", of_eta_etc)

	g_output_table_built = true
	return &g_output_table
}
