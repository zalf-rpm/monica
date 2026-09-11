// Phase 6 checkpoint 2: src/run/workstep.h/.cpp (the WorkstepType tag, the
// WorkstepData tagged union, the Workstep struct, the common/central-dispatch
// functions) plus every concrete workstep's own payload + merge/apply/
// condition/reinit, normally spread across src/worksteps/*.{h,cpp} - kept in
// one file here since Odin's `union` (the direct translation of
// std::variant) requires every member's full type to exist at the point the
// union itself is declared, unlike C++ where each payload struct/its
// functions can live in separate translation units compiled independently.
//
// Dropped, per plan-odin.md's "Explicitly dropped" table: SaveMonicaState
// (Cap'n Proto).
//
// SetValue was also dropped here in phase 6 (needed OId/buildOutputTable/
// the Spec expression evaluator, none of which existed yet) but is ported
// now that phase 7 built all three - see Set_Value_Data below and
// set_value_merge/set_value_apply/make_set_value_workstep in worksteps.odin.
// sim-min.json's crop-min.json rotation still doesn't use it, so it has no
// oracle coverage of its own; ported for completeness now that nothing
// blocks it, translated as literally as everything else in this file.
//
// to_json is not ported for any workstep in this checkpoint: its only real
// callers are cultivation-method::to_json/toString (CLI workflow dumps) and
// env round-tripping, neither exercised by this port's regression fixture
// (a single sim-min.json run) - "port on demand" once phase 7 or a workflow-
// dump feature actually needs it, the same call already made for
// getRawProteinConcentration and other build-output.cpp-only surface.
//
// Debug logging (Tools::debug()) is dropped throughout, matching every
// other file in this port.
package run

import core "../core"
import mio "../io"
import p "../params"
import d "../../support/date"
import clim "../../support/climate"
import jx "../../support/jsonx"
import tl "../../support/tools"

// ---------------------------------------------------------------------------
// WorkstepType / WorkstepData / Workstep
// ---------------------------------------------------------------------------

// C++: enum class monica::WorkstepType
Workstep_Type :: enum {
	Sowing,
	Automatic_Sowing,
	Transplant,
	Harvest,
	Automatic_Harvest,
	Cutting,
	Mineral_Fertilization,
	N_Demand_Fertilization,
	Organic_Fertilization,
	Tillage,
	Irrigation,
	Automatic_Irrigation,
	Set_Value,
}

// ---------------------------------------------------------------------------
// Concrete workstep payloads (src/worksteps/*.h)
// ---------------------------------------------------------------------------

// C++: struct SowingData
Sowing_Data :: struct {
	isValid:                     bool,
	sowingDate:                  d.Date,
	harvestDate:                 d.Date,
	isPerennialCrop:             Maybe(bool),
	cropParams:                  p.Crop_Parameters,
	separatePerennialCropParams: ^p.Crop_Parameters,
	residueParams:               p.Crop_Residue_Parameters,
	plantDensity:                int, // C++ default: -1
	initialKcb:                  f64, // C++ default: 0.15 - FAO-56 Dual Kc initial Kcb at planting
}

// C++: struct TransplantData : SowingData
//
// NOTE(c++-quirk): TransplantData declares its OWN initialKcb, which SHADOWS
// (not shares) SowingData's - two distinct storage locations in C++, both
// parsed from the same JSON "initialKcb" key during merge (kept in sync by
// redundant parsing, not by aliasing); only the derived one is actually read
// in apply() (C++ unqualified-name lookup prefers the derived member).
// Unlike C++, Odin's `using` field promotion does NOT allow an outer field
// to share a name with a promoted one (a hard redeclaration error, not a
// shadow) - so this struct has no second initialKcb at all. Since both C++
// storage locations always hold the same value (same JSON key, parsed
// twice), reusing the single promoted `sowing.initialKcb` (reachable as
// `t.initialKcb` via `using`) for both merge and apply is behaviourally
// identical to the C++, not an approximation - transplant_merge below
// simply doesn't repeat sowing_merge's already-done "initialKcb" parse.
Transplant_Data :: struct {
	using sowing:        Sowing_Data,
	initialStage:        int, // C++ default: 2 (size_t)
	initialGDD:          f64,
	initRootMass:        f64,
	initLeafMass:        f64,
	initShootMass:       f64,
	initLAI:             f64,
	postTransplantDelay: int,
}

// C++: struct AutomaticSowingData : SowingData
//
// getAvgSoilTemps (C++ std::function<vector<double>&()>) is the daily-values
// callback registerDailyFunction below wires up - its real, non-nil
// implementation is registered and driven by run-monica.cpp (phase 7, not
// yet ported); sim-min.json's crop-min.json never uses AutomaticSowing at
// all (confirmed - only Sowing/NDemandFertilization/AutomaticHarvest/
// AutomaticIrrigation/OrganicFertilization appear there), so this field
// stays nil until a phase-7 fixture actually exercises
// checkForSoilTemperature.
Automatic_Sowing_Data :: struct {
	using sowing:             Sowing_Data,
	absEarliestDate:          d.Date,
	earliestDate:             d.Date,
	latestDate:               d.Date,
	absLatestDate:            d.Date,
	minTempThreshold:         f64,
	daysInTempWindow:         int,
	minPercentASW:            f64,
	maxPercentASW:            f64, // C++ default: 100
	max3dayPrecipSum:         f64,
	maxCurrentDayPrecipSum:   f64,
	tempSumAboveBaseTemp:     f64,
	baseTemp:                 f64,
	checkForSoilTemperature:  bool,
	soilDepthForAveraging:    f64, // C++ default: 0.30
	daysInSoilTempWindow:     int,
	sowingIfAboveAvgSoilTemp: f64,
	getAvgSoilTemps:          proc() -> [dynamic]f64,
	inSowingRange:            bool,
	cropSeeded:                bool,
}

// C++: struct HarvestData
Harvest_Data :: struct {
	sowing:                 ^Sowing_Data, // non-owning, points into another workstep's variant payload
	exported:               bool, // C++ default: true
	spec:                   core.Harvest_Spec,
	optCarbMgmtData:        core.Harvest_Opt_Carbon_Management_Data,
	incorporateIntoLayerNo: int, // C++ default: 1
}

// C++: struct AutomaticHarvestData : HarvestData
Automatic_Harvest_Data :: struct {
	using harvest:          Harvest_Data,
	harvestTime:            string, // C++ default: "maturity"
	latestDate:             d.Date,
	absLatestDate:          d.Date,
	minPercentASW:          f64,
	maxPercentASW:          f64, // C++ default: 999
	max3dayPrecipSum:       f64, // C++ default: 9999
	maxCurrentDayPrecipSum: f64, // C++ default: 9999
	cropHarvested:          bool,
}

// C++: struct CuttingData - CuttingData::Value/Unit/CL are hoisted into
// `core` (Cutting_Value/Cutting_Unit/Cutting_Cl) since crop_module.odin's
// applyCutting needs them and cannot import this `run` package back.
Cutting_Data :: struct {
	organId2cuttingSpec:            map[int]core.Cutting_Value,
	organId2biomAfterCutting:       map[int]f64,
	organId2exportFraction:         map[int]f64,
	cutMaxAssimilationRateFraction: f64, // C++ default: 1.0
}

// C++: struct MineralFertilizationData
Mineral_Fertilization_Data :: struct {
	partition: p.Mineral_Fertilizer_Parameters,
	amount:    f64,
}

// C++: struct NDemandFertilizationData
N_Demand_Fertilization_Data :: struct {
	initialDate:       d.Date,
	partition:         p.Mineral_Fertilizer_Parameters,
	Ndemand:           f64,
	depth:             f64,
	stage:             int, // C++ default: 1
	appliedFertilizer: bool,
}

// C++: struct OrganicFertilizationData
Organic_Fertilization_Data :: struct {
	params:                 p.Organic_Matter_Parameters,
	amount:                 f64,
	incorporation:          bool,
	incorporateIntoLayerNo: int, // C++ default: 1
}

// C++: struct TillageData
Tillage_Data :: struct {
	depth: f64, // C++ default: 0.3
}

// C++: struct IrrigationData
Irrigation_Data :: struct {
	amount: f64,
	params: p.Irrigation_Parameters,
}

// C++: struct AutomaticIrrigationData
Automatic_Irrigation_Data :: struct {
	absStartDate: d.Date,
	absEndDate:   d.Date,
	irrigateCrop: bool,
	startStage:   int, // C++ default: -1
	endStage:     int, // C++ default: -1
	params:       p.Automatic_Irrigation_Parameters,
	done:         bool,
	cropPlanted:  bool,
}

// C++: std::function<json11::Json(const MonicaModel*)> SetValueData::getValue
//
// Materialised as data + a dispatcher (set_value_get_value, worksteps.odin)
// instead of a closure - Odin procs can't capture, the same shape used for
// run_monica.odin's Spec_Expr. NONE matches the C++ empty/falsy
// std::function (SetValueData::merge leaves it unset when its expression
// can't be resolved; SetValueData::apply's `if (!s->getValue) return true;`
// becomes a `kind == .NONE` check).
Set_Value_Get_Value_Kind :: enum {
	NONE,
	CONSTANT, // return SetValueData::value unchanged
	OID_LOOKUP, // read sourceOid - a registered lambda or a compiled path
	CALC_EXPR, // the ["=", a, op, b] arithmetic form
}

Set_Value_Get_Value :: struct {
	kind:      Set_Value_Get_Value_Kind,
	sourceOid: mio.OId, // used when kind == .OID_LOOKUP
	calc:      mio.Calc_Expr, // used when kind == .CALC_EXPR
}

// C++: struct SetValueData
Set_Value_Data :: struct {
	oid:      mio.OId,
	value:    jx.Value,
	getValue: Set_Value_Get_Value,
}

// C++: using WorkstepData = std::variant<...>
Workstep_Data :: union {
	Sowing_Data,
	Automatic_Sowing_Data,
	Transplant_Data,
	Harvest_Data,
	Automatic_Harvest_Data,
	Cutting_Data,
	Mineral_Fertilization_Data,
	N_Demand_Fertilization_Data,
	Organic_Fertilization_Data,
	Tillage_Data,
	Irrigation_Data,
	Automatic_Irrigation_Data,
	Set_Value_Data,
}

// C++: struct Workstep
Workstep :: struct {
	date:                         d.Date,
	absDate:                      d.Date,
	applyNoOfDaysAfterEvent:      int,
	afterEvent:                   string,
	daysAfterEventCount:          int,
	daysAfterEventCountActivated: bool,
	isActive:                     bool, // C++ default: true
	runAtStartOfDay:              bool, // C++ default: true
	errors:                       tl.Errors,

	data: Workstep_Data,
}

// Heap-allocates a Workstep with the C++ in-class defaults applied
// (isActive{true}, runAtStartOfDay{true}) - Odin's `new()` zero-initialises,
// which would silently give both `false`. Every make_*_workstep factory
// uses this instead of a raw `new(Workstep, allocator)`.
new_workstep :: proc(allocator := context.allocator) -> ^Workstep {
	ws := new(Workstep, allocator)
	ws.isActive = true
	ws.runAtStartOfDay = true
	return ws
}

// ---------------------------------------------------------------------------
// Shared helpers (workstep::, used by more than one concrete workstep)
// ---------------------------------------------------------------------------

// C++: std::pair<Date,bool> workstep::makeInitAbsDate(Date, Date, bool, bool)
make_init_abs_date :: proc(
	date: d.Date,
	initDate: d.Date,
	addYear: bool,
	forceInitYear: bool = false,
) -> (
	absDate: d.Date,
	addedYear: bool,
) {
	if d.is_absolute_date(date) {
		return date, false
	}

	absDate = d.to_absolute_date(date, u16(d.year(initDate)))
	if !forceInitYear && (addYear || d.lt(absDate, initDate)) {
		addedYear = true
		d.add_years(&absDate, 1)
	}

	return absDate, addedYear
}

// C++: int workstep::organIdFromName(const string&, Tools::Errors&)
organ_id_from_name :: proc(organName: string, err: ^tl.Errors) -> int {
	os := tl.to_lower(organName)
	switch os {
	case "root":
		return 0
	case "leaf":
		return 1
	case "shoot":
		return 2
	case "fruit":
		return 3
	case "struct":
		return 4
	case "sugar":
		return 5
	}
	tl.append_error(err, "organ id could not be resolved")
	return -1 // default error
}

// C++: string workstep::organNameFromId(int)
organ_name_from_id :: proc(organId: int) -> string {
	switch organId {
	case 0:
		return "Root"
	case 1:
		return "Leaf"
	case 2:
		return "Shoot"
	case 3:
		return "Fruit"
	case 4:
		return "Struct"
	case 5:
		return "Sugar"
	}
	return "unknown"
}

// C++: bool workstep::isSoilMoistureOk(MonicaModel*, double, double)
is_soil_moisture_ok :: proc(model: ^core.Monica_Model, minPercentASW, maxPercentASW: f64) -> bool {
	pwp := model.soil_column.layers[0].permanent_wilting_point
	sm := max(0.0, model.soil_column.layers[0].soil_moisture_m3 - pwp)
	asw := model.soil_column.layers[0].field_capacity - pwp
	currentPercentASW := sm / asw * 100.0
	return minPercentASW <= currentPercentASW && currentPercentASW <= maxPercentASW
}

// C++: bool workstep::isPrecipitationOk(const vector<map<ACD,double>>&,
//        double, double)
//
// Sums exactly the last 3 entries (not clamped to size), matching the C++'s
// unclamped `climateData.rbegin(), climateData.rbegin() + 3` - reproduced
// as-is; both sides only ever call this well past day 3 of a real run.
is_precipitation_ok :: proc(
	climateData: [dynamic]map[clim.ACD]f64,
	max3dayPrecipSum: f64,
	maxCurrentDayPrecipSum: f64,
) -> bool {
	n := len(climateData)
	psum3d := 0.0
	for i := n - 1; i >= n - 3; i -= 1 {
		if v, ok := climateData[i][.precip]; ok {
			psum3d += v
		}
	}
	currentp := climateData[n - 1][.precip]
	return psum3d <= max3dayPrecipSum && currentp <= maxCurrentDayPrecipSum
}

// C++: Errors workstep::mergeCommon(Workstep*, json11::Json)
workstep_merge_common :: proc(ws: ^Workstep, j: jx.Value) -> tl.Errors {
	res := p.default_merge(ws, j, workstep_merge_common)

	jx.set_iso_date_value(&ws.date, j, "date")
	// at is a shortcut for after=event and days=1
	at := jx.string_value(j, "at")
	if at != "" {
		ws.afterEvent = at
		ws.applyNoOfDaysAfterEvent = 1
	}
	jx.set_int_value(&ws.applyNoOfDaysAfterEvent, j, "days")
	jx.set_string_value(&ws.afterEvent, j, "after")
	jx.set_bool_value(&ws.runAtStartOfDay, j, "runAtStartOfDay")

	return res
}

// C++: bool workstep::applyCommon(Workstep*, MonicaModel*)
workstep_apply_common :: proc(ws: ^Workstep, model: ^core.Monica_Model) -> bool {
	model.current_events["Workstep"] = true
	return true
}

// C++: bool workstep::conditionCommon(Workstep*, MonicaModel*)
workstep_condition_common :: proc(ws: ^Workstep, model: ^core.Monica_Model) -> bool {
	if ws.afterEvent == "" || ws.applyNoOfDaysAfterEvent <= 0 {
		return false
	}

	_, curr_ok := model.current_events[ws.afterEvent]
	_, prev_ok := model.previous_days_events[ws.afterEvent]

	if ws.daysAfterEventCountActivated {
		ws.daysAfterEventCount += 1
	} else if curr_ok || prev_ok {
		ws.daysAfterEventCountActivated = true
	}

	return ws.daysAfterEventCount == ws.applyNoOfDaysAfterEvent
}

// C++: bool workstep::reinitCommon(Workstep*, Date, bool, bool)
workstep_reinit_common :: proc(
	ws: ^Workstep,
	date: d.Date,
	addYear: bool = false,
	forceInitYear: bool = false,
) -> bool {
	addedYear := false

	if d.is_valid(ws.date) {
		ws.absDate, addedYear = make_init_abs_date(ws.date, date, addYear, forceInitYear)
	} else {
		ws.absDate = d.Date{}
	}

	ws.isActive = true
	ws.daysAfterEventCount = 0
	ws.daysAfterEventCountActivated = false

	return addedYear
}

// ---------------------------------------------------------------------------
// Central dispatch (workstep::, switches on type(ws))
// ---------------------------------------------------------------------------

// C++: WorkstepType workstep::type(const Workstep*)
//
// A type switch, not std::variant::index() - more robust than mirroring the
// union's declaration order, and Odin has no direct index() equivalent
// anyway.
workstep_type :: proc(ws: ^Workstep) -> Workstep_Type {
	switch _ in ws.data {
	case Sowing_Data:
		return .Sowing
	case Automatic_Sowing_Data:
		return .Automatic_Sowing
	case Transplant_Data:
		return .Transplant
	case Harvest_Data:
		return .Harvest
	case Automatic_Harvest_Data:
		return .Automatic_Harvest
	case Cutting_Data:
		return .Cutting
	case Mineral_Fertilization_Data:
		return .Mineral_Fertilization
	case N_Demand_Fertilization_Data:
		return .N_Demand_Fertilization
	case Organic_Fertilization_Data:
		return .Organic_Fertilization
	case Tillage_Data:
		return .Tillage
	case Irrigation_Data:
		return .Irrigation
	case Automatic_Irrigation_Data:
		return .Automatic_Irrigation
	case Set_Value_Data:
		return .Set_Value
	}
	unreachable()
}

// C++: bool workstep::isDynamicWorkstep(const Workstep*)
is_dynamic_workstep :: proc(ws: ^Workstep) -> bool {
	return !d.is_valid(ws.date)
}

// C++: Date workstep::absDate(const Workstep*)
workstep_abs_date :: proc(ws: ^Workstep) -> d.Date {
	return d.is_absolute_date(ws.date) ? ws.date : ws.absDate
}

// C++: Date workstep::earliestDate(const Workstep*)
workstep_earliest_date :: proc(ws: ^Workstep) -> d.Date {
	if workstep_type(ws) == .Automatic_Sowing {
		return ws.data.(Automatic_Sowing_Data).earliestDate
	}
	return ws.date
}

// C++: Date workstep::absEarliestDate(const Workstep*)
workstep_abs_earliest_date :: proc(ws: ^Workstep) -> d.Date {
	if workstep_type(ws) == .Automatic_Sowing {
		return ws.data.(Automatic_Sowing_Data).absEarliestDate
	}
	return workstep_abs_date(ws)
}

// C++: Date workstep::latestDate(const Workstep*)
workstep_latest_date :: proc(ws: ^Workstep) -> d.Date {
	#partial switch workstep_type(ws) {
	case .Automatic_Sowing:
		return ws.data.(Automatic_Sowing_Data).latestDate
	case .Automatic_Harvest:
		return ws.data.(Automatic_Harvest_Data).latestDate
	case:
		return ws.date
	}
}

// C++: Date workstep::absLatestDate(const Workstep*)
workstep_abs_latest_date :: proc(ws: ^Workstep) -> d.Date {
	#partial switch workstep_type(ws) {
	case .Automatic_Sowing:
		return ws.data.(Automatic_Sowing_Data).absLatestDate
	case .Automatic_Harvest:
		return ws.data.(Automatic_Harvest_Data).absLatestDate
	case:
		return workstep_abs_date(ws)
	}
}

// C++: Errors workstep::merge(Workstep*, json11::Json)
//
// mergeCommon already applies the DEFAULT/"=" unwrap - not repeated here.
workstep_merge :: proc(ws: ^Workstep, j: jx.Value) -> tl.Errors {
	res := workstep_merge_common(ws, j)

	switch _ in ws.data {
	case Sowing_Data:
		tl.append_errors(&res, sowing_merge(&ws.data.(Sowing_Data), j))
	case Automatic_Sowing_Data:
		tl.append_errors(&res, automatic_sowing_merge(&ws.data.(Automatic_Sowing_Data), j))
	case Transplant_Data:
		tl.append_errors(&res, transplant_merge(&ws.data.(Transplant_Data), j))
	case Harvest_Data:
		tl.append_errors(&res, harvest_merge(&ws.data.(Harvest_Data), j))
	case Automatic_Harvest_Data:
		tl.append_errors(&res, automatic_harvest_merge(&ws.data.(Automatic_Harvest_Data), j))
	case Cutting_Data:
		tl.append_errors(&res, cutting_merge(&ws.data.(Cutting_Data), j))
	case Mineral_Fertilization_Data:
		tl.append_errors(&res, mineral_fertilization_merge(&ws.data.(Mineral_Fertilization_Data), j))
	case N_Demand_Fertilization_Data:
		tl.append_errors(
			&res,
			n_demand_fertilization_merge(&ws.data.(N_Demand_Fertilization_Data), ws, j),
		)
	case Organic_Fertilization_Data:
		tl.append_errors(&res, organic_fertilization_merge(&ws.data.(Organic_Fertilization_Data), j))
	case Tillage_Data:
		tl.append_errors(&res, tillage_merge(&ws.data.(Tillage_Data), j))
	case Irrigation_Data:
		tl.append_errors(&res, irrigation_merge(&ws.data.(Irrigation_Data), j))
	case Automatic_Irrigation_Data:
		tl.append_errors(&res, automatic_irrigation_merge(&ws.data.(Automatic_Irrigation_Data), j))
	case Set_Value_Data:
		tl.append_errors(&res, set_value_merge(&ws.data.(Set_Value_Data), j))
	}

	return res
}

// C++: bool workstep::isActive(const Workstep*)
workstep_is_active :: proc(ws: ^Workstep) -> bool {
	#partial switch workstep_type(ws) {
	case .Automatic_Sowing:
		return !ws.data.(Automatic_Sowing_Data).cropSeeded
	case .Automatic_Harvest:
		return !ws.data.(Automatic_Harvest_Data).cropHarvested
	case .N_Demand_Fertilization:
		return !ws.data.(N_Demand_Fertilization_Data).appliedFertilizer
	case:
		return ws.isActive
	}
}

// C++: bool workstep::apply(Workstep*, MonicaModel*)
workstep_apply :: proc(ws: ^Workstep, model: ^core.Monica_Model) -> bool {
	switch _ in ws.data {
	case Sowing_Data:
		return sowing_apply(&ws.data.(Sowing_Data), ws, model)
	case Automatic_Sowing_Data:
		return automatic_sowing_apply(&ws.data.(Automatic_Sowing_Data), ws, model)
	case Transplant_Data:
		return transplant_apply(&ws.data.(Transplant_Data), ws, model)
	case Harvest_Data:
		return harvest_apply(&ws.data.(Harvest_Data), ws, model)
	case Automatic_Harvest_Data:
		return automatic_harvest_apply(&ws.data.(Automatic_Harvest_Data), ws, model)
	case Cutting_Data:
		return cutting_apply(&ws.data.(Cutting_Data), ws, model)
	case Mineral_Fertilization_Data:
		return mineral_fertilization_apply(&ws.data.(Mineral_Fertilization_Data), ws, model)
	case N_Demand_Fertilization_Data:
		return n_demand_fertilization_apply(&ws.data.(N_Demand_Fertilization_Data), ws, model)
	case Organic_Fertilization_Data:
		return organic_fertilization_apply(&ws.data.(Organic_Fertilization_Data), ws, model)
	case Tillage_Data:
		return tillage_apply(&ws.data.(Tillage_Data), ws, model)
	case Irrigation_Data:
		return irrigation_apply(&ws.data.(Irrigation_Data), ws, model)
	case Automatic_Irrigation_Data:
		return automatic_irrigation_apply(&ws.data.(Automatic_Irrigation_Data), model)
	case Set_Value_Data:
		return set_value_apply(&ws.data.(Set_Value_Data), ws, model)
	}
	return false // unreachable, all WorkstepType values handled above
}

// C++: bool workstep::applyWithPossibleCondition(Workstep*, MonicaModel*)
workstep_apply_with_possible_condition :: proc(ws: ^Workstep, model: ^core.Monica_Model) -> bool {
	workstepFinished := false
	if workstep_is_active(ws) {
		if is_dynamic_workstep(ws) {
			workstepFinished = workstep_condition(ws, model) ? workstep_apply(ws, model) : false
		} else {
			workstepFinished = workstep_apply(ws, model)
		}
		ws.isActive = !workstepFinished
	}
	return workstepFinished
}

// C++: bool workstep::condition(Workstep*, MonicaModel*)
workstep_condition :: proc(ws: ^Workstep, model: ^core.Monica_Model) -> bool {
	#partial switch workstep_type(ws) {
	case .Automatic_Sowing:
		return automatic_sowing_condition(&ws.data.(Automatic_Sowing_Data), model)
	case .Automatic_Harvest:
		return automatic_harvest_condition(&ws.data.(Automatic_Harvest_Data), model)
	case .N_Demand_Fertilization:
		return n_demand_fertilization_condition(&ws.data.(N_Demand_Fertilization_Data), ws, model)
	case .Automatic_Irrigation:
		return automatic_irrigation_condition(&ws.data.(Automatic_Irrigation_Data), model)
	case:
		return workstep_condition_common(ws, model)
	}
}

// C++: bool workstep::reinit(Workstep*, Date, bool, bool)
workstep_reinit :: proc(
	ws: ^Workstep,
	date: d.Date,
	addYear: bool = false,
	forceInitYear: bool = false,
) -> bool {
	#partial switch workstep_type(ws) {
	case .Automatic_Sowing:
		return automatic_sowing_reinit(&ws.data.(Automatic_Sowing_Data), ws, date, addYear, forceInitYear)
	case .Automatic_Harvest:
		return automatic_harvest_reinit(&ws.data.(Automatic_Harvest_Data), ws, date, addYear, forceInitYear)
	case .N_Demand_Fertilization:
		return n_demand_fertilization_reinit(
			&ws.data.(N_Demand_Fertilization_Data),
			ws,
			date,
			addYear,
			forceInitYear,
		)
	case .Automatic_Irrigation:
		return automatic_irrigation_reinit(
			&ws.data.(Automatic_Irrigation_Data),
			ws,
			date,
			addYear,
			forceInitYear,
		)
	case:
		return workstep_reinit_common(ws, date, addYear, forceInitYear)
	}
}

// C++: std::function<double(MonicaModel*)> workstep::registerDailyFunction(
//        Workstep*, std::function<vector<double>&()>)
//
// Returns (proc, ok) instead of a possibly-empty std::function: Odin procs
// have no "empty" sentinel value the way std::function does.
workstep_register_daily_function :: proc(
	ws: ^Workstep,
	getDailyValues: proc() -> [dynamic]f64,
) -> (
	proc(_: ^core.Monica_Model) -> f64,
	bool,
) {
	if workstep_type(ws) == .Automatic_Sowing {
		return automatic_sowing_register_daily_function(&ws.data.(Automatic_Sowing_Data), getDailyValues)
	}
	return nil, false
}

// C++: WSPtr monica::makeWorkstep(json11::Json)
//
// Returns nil on an unrecognised/dropped type, matching the C++ empty
// WSPtr{} return - the caller (cultivation_method.odin, checkpoint 5) skips
// nil worksteps the same way cultivation-method.cpp's `if (!ws) continue;`
// does.
make_workstep :: proc(j: jx.Value, allocator := context.allocator) -> ^Workstep {
	type := jx.string_value_of(jx.get(j, "type"))

	switch type {
	case "Sowing", "Seed":
		return make_sowing_workstep(j, allocator)
	case "Transplant":
		return make_transplant_workstep(j, allocator)
	case "AutomaticSowing":
		return make_automatic_sowing_workstep(j, allocator)
	case "Harvest":
		return make_harvest_workstep(j, allocator)
	case "AutomaticHarvest":
		return make_automatic_harvest_workstep(j, allocator)
	case "Cutting":
		return make_cutting_workstep(j, allocator)
	case "MineralFertilization", "MineralFertiliserApplication": // deprecated name
		return make_mineral_fertilization_workstep(j, allocator)
	case "NDemandFertilization":
		return make_n_demand_fertilization_workstep(j, allocator)
	case "OrganicFertilization", "OrganicFertiliserApplication": // deprecated name
		return make_organic_fertilization_workstep(j, allocator)
	case "Tillage", "TillageApplication": // deprecated name
		return make_tillage_workstep(j, allocator)
	case "Irrigation", "IrrigationApplication": // deprecated name
		return make_irrigation_workstep(j, allocator)
	case "AutomaticIrrigation":
		return make_automatic_irrigation_workstep(j, allocator)
	case "SetValue":
		return make_set_value_workstep(j, allocator)
	}

	return nil
}
