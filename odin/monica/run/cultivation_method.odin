// Phase 6 checkpoint 5: src/run/cultivation-method.h/.cpp.
//
// to_json/toString are not ported, matching workstep.odin's own to_json
// deferral (toString's only non-trivial content is workstep::to_json output
// anyway) - "port on demand" once a workflow-dump feature or phase 7 needs
// it; this port's regression fixture never calls either.
package run

import core "../core"
import d "../../support/date"
import jx "../../support/jsonx"
import tl "../../support/tools"

// C++: struct monica::CultivationMethod
Cultivation_Method :: struct {
	allWorksteps:               [dynamic]^Workstep,
	allAbsWorksteps:            [dynamic]^Workstep,
	unfinishedDynamicWorksteps: [dynamic]^Workstep,
	customId:                   int,
	name:                       string,
	canBeSkipped:               bool,
	isCoverCrop:                bool,
	repeat:                     bool, // C++ default: true
}

// C++: CultivationMethod monica::makeCultivationMethod(json11::Json)
//
// NOTE: like the original CultivationMethod(json11::Json) constructor, the
// merge() result (Errors) is not stored anywhere - discarded, not a
// mistake, matches the original exactly.
make_cultivation_method :: proc(j: jx.Value, allocator := context.allocator) -> Cultivation_Method {
	cm := Cultivation_Method{repeat = true}
	_ = cultivation_method_merge(&cm, j, allocator)
	return cm
}

// C++: Errors cultivationmethod::merge(CultivationMethod*, json11::Json)
cultivation_method_merge :: proc(
	cm: ^Cultivation_Method,
	j: jx.Value,
	allocator := context.allocator,
) -> tl.Errors {
	res: tl.Errors

	jx.set_int_value(&cm.customId, j, "customId")
	jx.set_string_value(&cm.name, j, "name")
	jx.set_bool_value(&cm.canBeSkipped, j, "can-be-skipped")
	jx.set_bool_value(&cm.isCoverCrop, j, "is-cover-crop")
	jx.set_bool_value(&cm.repeat, j, "repeat")

	// keep reference to sowing workstep for use with harvest workstep
	sowingWS: ^Sowing_Data = nil

	for wsj in jx.array_items(jx.get(j, "worksteps")) {
		ws := make_workstep(wsj, allocator)
		if ws == nil {
			continue
		}
		tl.append_errors(&res, ws.errors)
		append(&cm.allWorksteps, ws)
		switch _ in ws.data {
		case Sowing_Data:
			sowingWS = &ws.data.(Sowing_Data)
		case Automatic_Sowing_Data:
			asd := &ws.data.(Automatic_Sowing_Data)
			sowingWS = &asd.sowing
		case Harvest_Data:
			if sowingWS != nil {
				hd := &ws.data.(Harvest_Data)
				hd.sowing = sowingWS
			}
		case Automatic_Harvest_Data:
			if sowingWS != nil {
				ahd := &ws.data.(Automatic_Harvest_Data)
				ahd.sowing = sowingWS
			}
		case Transplant_Data,
		     Cutting_Data,
		     Mineral_Fertilization_Data,
		     N_Demand_Fertilization_Data,
		     Organic_Fertilization_Data,
		     Tillage_Data,
		     Irrigation_Data,
		     Automatic_Irrigation_Data:
		// no-op, matches C++'s `default: break;`
		}
	}

	return res
}

// C++: void cultivationmethod::apply(const CultivationMethod*, const Date&, MonicaModel*)
cultivation_method_apply_at_date :: proc(
	cm: ^Cultivation_Method,
	date: d.Date,
	model: ^core.Monica_Model,
	allocator := context.allocator,
) {
	for ws in cultivation_method_worksteps_at(cm, date, allocator) {
		workstep_apply(ws, model)
	}
}

// C++: void cultivationmethod::absApply(const CultivationMethod*, const Date&, MonicaModel*)
cultivation_method_abs_apply :: proc(
	cm: ^Cultivation_Method,
	date: d.Date,
	model: ^core.Monica_Model,
	allocator := context.allocator,
) {
	for ws in cultivation_method_abs_worksteps_at(cm, date, allocator) {
		workstep_apply(ws, model)
	}
}

// C++: void cultivationmethod::apply(CultivationMethod*, MonicaModel*, bool)
//
// Odin translation of the erase-remove idiom: walk unfinishedDynamicWorksteps
// once, removing (via ordered_remove, so relative order among survivors is
// preserved like std::vector::erase) any workstep whose runAtStartOfDay flag
// matches the requested filter AND that applyWithPossibleCondition reports
// finished.
cultivation_method_apply :: proc(
	cm: ^Cultivation_Method,
	model: ^core.Monica_Model,
	runOnlyAtStartOfDayWorksteps: bool,
) {
	i := 0
	for i < len(cm.unfinishedDynamicWorksteps) {
		wsp := cm.unfinishedDynamicWorksteps[i]
		if runOnlyAtStartOfDayWorksteps == wsp.runAtStartOfDay &&
		   workstep_apply_with_possible_condition(wsp, model) {
			ordered_remove(&cm.unfinishedDynamicWorksteps, i)
		} else {
			i += 1
		}
	}
}

// C++: Date cultivationmethod::nextDate(const CultivationMethod*, const Date&)
cultivation_method_next_date :: proc(cm: ^Cultivation_Method, date: d.Date) -> d.Date {
	for ws in cm.allWorksteps {
		dt := ws.date
		if d.is_valid(dt) && d.gt(dt, date) {
			return dt
		}
	}
	return d.Date{}
}

// C++: Date cultivationmethod::nextAbsDate(const CultivationMethod*, const Date&)
cultivation_method_next_abs_date :: proc(cm: ^Cultivation_Method, date: d.Date) -> d.Date {
	for ws in cm.allAbsWorksteps {
		ad := workstep_abs_date(ws)
		if d.is_valid(ad) && d.gt(ad, date) {
			return ad
		}
	}
	return d.Date{}
}

// C++: vector<WSPtr> cultivationmethod::workstepsAt(const CultivationMethod*, const Date&)
cultivation_method_worksteps_at :: proc(
	cm: ^Cultivation_Method,
	date: d.Date,
	allocator := context.allocator,
) -> [dynamic]^Workstep {
	apps := make([dynamic]^Workstep, 0, allocator)
	for ws in cm.allWorksteps {
		if d.is_valid(ws.date) && d.eq(ws.date, date) {
			append(&apps, ws)
		}
	}
	return apps
}

// C++: vector<WSPtr> cultivationmethod::absWorkstepsAt(const CultivationMethod*, const Date&)
cultivation_method_abs_worksteps_at :: proc(
	cm: ^Cultivation_Method,
	date: d.Date,
	allocator := context.allocator,
) -> [dynamic]^Workstep {
	apps := make([dynamic]^Workstep, 0, allocator)
	for ws in cm.allAbsWorksteps {
		ad := workstep_abs_date(ws)
		if d.is_valid(ad) && d.eq(ad, date) {
			append(&apps, ws)
		}
	}
	return apps
}

// C++: bool cultivationmethod::areOnlyAbsoluteWorksteps(const CultivationMethod*)
cultivation_method_are_only_absolute_worksteps :: proc(cm: ^Cultivation_Method) -> bool {
	for ws in cm.allWorksteps {
		if !(d.is_valid(ws.date) && d.is_absolute_date(ws.date)) {
			return false
		}
	}
	return true
}

// C++: vector<WSPtr> cultivationmethod::staticWorksteps(const CultivationMethod*)
cultivation_method_static_worksteps :: proc(
	cm: ^Cultivation_Method,
	allocator := context.allocator,
) -> [dynamic]^Workstep {
	wss := make([dynamic]^Workstep, 0, allocator)
	for ws in cm.allWorksteps {
		if d.is_valid(ws.date) {
			append(&wss, ws)
		}
	}
	return wss
}

// C++: vector<WSPtr> cultivationmethod::allDynamicWorksteps(const CultivationMethod*)
cultivation_method_all_dynamic_worksteps :: proc(
	cm: ^Cultivation_Method,
	allocator := context.allocator,
) -> [dynamic]^Workstep {
	return cultivation_method_worksteps_at(cm, d.Date{}, allocator)
}

// C++: bool cultivationmethod::allDynamicWorkstepsFinished(const CultivationMethod*)
cultivation_method_all_dynamic_worksteps_finished :: proc(cm: ^Cultivation_Method) -> bool {
	if len(cm.unfinishedDynamicWorksteps) == 0 {
		return true
	}
	for wsp in cm.unfinishedDynamicWorksteps {
		if workstep_type(wsp) != .N_Demand_Fertilization {
			return false
		}
	}
	return true
}

// C++: Date cultivationmethod::startDate(const CultivationMethod*)
cultivation_method_start_date :: proc(
	cm: ^Cultivation_Method,
	allocator := context.allocator,
) -> d.Date {
	if len(cm.allWorksteps) == 0 {
		return d.Date{}
	}

	dynEarliestStart := d.Date{}
	for ws in cultivation_method_worksteps_at(cm, d.Date{}, allocator) {
		ed := workstep_earliest_date(ws)
		if (d.is_valid(ed) && d.is_valid(dynEarliestStart) && d.lt(ed, dynEarliestStart)) ||
		   (d.is_valid(ed) && !d.is_valid(dynEarliestStart)) {
			dynEarliestStart = ed
		}
	}

	startDate := dynEarliestStart
	for ws in cm.allWorksteps {
		dt := ws.date
		if d.is_valid(dt) && (d.lt(dt, startDate) || !d.is_valid(startDate)) {
			startDate = dt
		}
	}

	return startDate
}

// C++: Date cultivationmethod::absStartDate(const CultivationMethod*, bool)
cultivation_method_abs_start_date :: proc(
	cm: ^Cultivation_Method,
	includeDynamicWorksteps: bool = true,
	allocator := context.allocator,
) -> d.Date {
	if len(cm.allAbsWorksteps) == 0 {
		return d.Date{}
	}

	dynEarliestStart := d.Date{}
	if includeDynamicWorksteps {
		for ws in cultivation_method_abs_worksteps_at(cm, d.Date{}, allocator) {
			ed := workstep_abs_earliest_date(ws)
			if (d.is_valid(ed) && d.is_valid(dynEarliestStart) && d.lt(ed, dynEarliestStart)) ||
			   (d.is_valid(ed) && !d.is_valid(dynEarliestStart)) {
				dynEarliestStart = ed
			}
		}
	}

	startDate := dynEarliestStart
	for ws in cm.allAbsWorksteps {
		ad := workstep_abs_date(ws)
		if d.is_valid(ad) && (d.lt(ad, startDate) || !d.is_valid(startDate)) {
			startDate = ad
		}
	}

	return startDate
}

// C++: Date cultivationmethod::absLatestSowingDate(const CultivationMethod*)
cultivation_method_abs_latest_sowing_date :: proc(cm: ^Cultivation_Method) -> d.Date {
	dynLatestSowingDate := d.Date{}
	for ws in cm.allAbsWorksteps {
		t := workstep_type(ws)
		if t == .Sowing || t == .Automatic_Sowing {
			lsd := workstep_abs_latest_date(ws)
			if d.is_valid(lsd) && d.lt(dynLatestSowingDate, lsd) {
				dynLatestSowingDate = lsd
			}
		}
	}
	return dynLatestSowingDate
}

// C++: Date cultivationmethod::endDate(const CultivationMethod*)
cultivation_method_end_date :: proc(
	cm: ^Cultivation_Method,
	allocator := context.allocator,
) -> d.Date {
	if len(cm.allWorksteps) == 0 {
		return d.Date{}
	}

	dynLatestEnd := d.Date{}
	for ws in cultivation_method_worksteps_at(cm, d.Date{}, allocator) {
		ed := workstep_latest_date(ws)
		if (d.is_valid(ed) && d.is_valid(dynLatestEnd) && d.gt(ed, dynLatestEnd)) ||
		   (d.is_valid(ed) && !d.is_valid(dynLatestEnd)) {
			dynLatestEnd = ed
		}
	}

	endDate := dynLatestEnd
	for ws in cm.allWorksteps {
		dt := ws.date
		if d.is_valid(dt) && (d.gt(dt, endDate) || !d.is_valid(endDate)) {
			endDate = dt
		}
	}

	return endDate
}

// C++: Date cultivationmethod::absEndDate(const CultivationMethod*)
cultivation_method_abs_end_date :: proc(
	cm: ^Cultivation_Method,
	allocator := context.allocator,
) -> d.Date {
	if len(cm.allAbsWorksteps) == 0 {
		return d.Date{}
	}

	dynLatestEnd := d.Date{}
	for ws in cultivation_method_abs_worksteps_at(cm, d.Date{}, allocator) {
		ed := workstep_abs_latest_date(ws)
		if (d.is_valid(ed) && d.is_valid(dynLatestEnd) && d.gt(ed, dynLatestEnd)) ||
		   (d.is_valid(ed) && !d.is_valid(dynLatestEnd)) {
			dynLatestEnd = ed
		}
	}

	endDate := dynLatestEnd
	for ws in cm.allAbsWorksteps {
		ad := workstep_abs_date(ws)
		if d.is_valid(ad) && (d.gt(ad, endDate) || !d.is_valid(endDate)) {
			endDate = ad
		}
	}

	return endDate
}

// C++: bool cultivationmethod::reinit(CultivationMethod*, Tools::Date, bool)
cultivation_method_reinit :: proc(
	cm: ^Cultivation_Method,
	date: d.Date,
	forceInitYear: bool = false,
) -> bool {
	clear(&cm.allAbsWorksteps)
	clear(&cm.unfinishedDynamicWorksteps)

	addedYear := false
	for ws in cm.allWorksteps {
		addedYear = workstep_reinit(ws, date, addedYear, forceInitYear) || addedYear
		append(&cm.allAbsWorksteps, ws)
		if !d.is_valid(workstep_abs_date(ws)) {
			append(&cm.unfinishedDynamicWorksteps, ws)
		}
	}

	return addedYear
}
