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
	all_worksteps:                [dynamic]^Workstep,
	all_abs_worksteps:            [dynamic]^Workstep,
	unfinished_dynamic_worksteps: [dynamic]^Workstep,
	custom_id:                    int,
	name:                         string,
	can_be_skipped:               bool,
	is_cover_crop:                bool,
	repeat:                       bool, // C++ default: true
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

	jx.set_int_value(&cm.custom_id, j, "customId")
	jx.set_string_value(&cm.name, j, "name")
	jx.set_bool_value(&cm.can_be_skipped, j, "can-be-skipped")
	jx.set_bool_value(&cm.is_cover_crop, j, "is-cover-crop")
	jx.set_bool_value(&cm.repeat, j, "repeat")

	// keep reference to sowing workstep for use with harvest workstep
	sowing_ws: ^Sowing_Data = nil

	for wsj in jx.array_items(jx.get(j, "worksteps")) {
		ws := make_workstep(wsj, allocator)
		if ws == nil {
			continue
		}
		tl.append_errors(&res, ws.errors)
		append(&cm.all_worksteps, ws)
		switch _ in ws.data {
		case Sowing_Data:
			sowing_ws = &ws.data.(Sowing_Data)
		case Automatic_Sowing_Data:
			asd := &ws.data.(Automatic_Sowing_Data)
			sowing_ws = &asd.sowing
		case Harvest_Data:
			if sowing_ws != nil {
				hd := &ws.data.(Harvest_Data)
				hd.sowing = sowing_ws
			}
		case Automatic_Harvest_Data:
			if sowing_ws != nil {
				ahd := &ws.data.(Automatic_Harvest_Data)
				ahd.sowing = sowing_ws
			}
		case Transplant_Data,
		     Cutting_Data,
		     Mineral_Fertilization_Data,
		     N_Demand_Fertilization_Data,
		     Organic_Fertilization_Data,
		     Tillage_Data,
		     Irrigation_Data,
		     Automatic_Irrigation_Data,
		     Set_Value_Data:
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
// Odin translation of the erase-remove idiom: walk unfinished_dynamic_worksteps
// once, removing (via ordered_remove, so relative order among survivors is
// preserved like std::vector::erase) any workstep whose run_at_start_of_day flag
// matches the requested filter AND that applyWithPossibleCondition reports
// finished.
cultivation_method_apply :: proc(
	cm: ^Cultivation_Method,
	model: ^core.Monica_Model,
	run_only_at_start_of_day_worksteps: bool,
) {
	i := 0
	for i < len(cm.unfinished_dynamic_worksteps) {
		wsp := cm.unfinished_dynamic_worksteps[i]
		if run_only_at_start_of_day_worksteps == wsp.run_at_start_of_day &&
		   workstep_apply_with_possible_condition(wsp, model) {
			ordered_remove(&cm.unfinished_dynamic_worksteps, i)
		} else {
			i += 1
		}
	}
}

// C++: Date cultivationmethod::nextDate(const CultivationMethod*, const Date&)
cultivation_method_next_date :: proc(cm: ^Cultivation_Method, date: d.Date) -> d.Date {
	for ws in cm.all_worksteps {
		dt := ws.date
		if d.is_valid(dt) && d.gt(dt, date) {
			return dt
		}
	}
	return d.Date{}
}

// C++: Date cultivationmethod::nextAbsDate(const CultivationMethod*, const Date&)
cultivation_method_next_abs_date :: proc(cm: ^Cultivation_Method, date: d.Date) -> d.Date {
	for ws in cm.all_abs_worksteps {
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
	for ws in cm.all_worksteps {
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
	for ws in cm.all_abs_worksteps {
		ad := workstep_abs_date(ws)
		if d.is_valid(ad) && d.eq(ad, date) {
			append(&apps, ws)
		}
	}
	return apps
}

// C++: bool cultivationmethod::areOnlyAbsoluteWorksteps(const CultivationMethod*)
cultivation_method_are_only_absolute_worksteps :: proc(cm: ^Cultivation_Method) -> bool {
	for ws in cm.all_worksteps {
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
	for ws in cm.all_worksteps {
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
	if len(cm.unfinished_dynamic_worksteps) == 0 {
		return true
	}
	for wsp in cm.unfinished_dynamic_worksteps {
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
	if len(cm.all_worksteps) == 0 {
		return d.Date{}
	}

	dyn_earliest_start := d.Date{}
	for ws in cultivation_method_worksteps_at(cm, d.Date{}, allocator) {
		ed := workstep_earliest_date(ws)
		if (d.is_valid(ed) && d.is_valid(dyn_earliest_start) && d.lt(ed, dyn_earliest_start)) ||
		   (d.is_valid(ed) && !d.is_valid(dyn_earliest_start)) {
			dyn_earliest_start = ed
		}
	}

	start_date := dyn_earliest_start
	for ws in cm.all_worksteps {
		dt := ws.date
		if d.is_valid(dt) && (d.lt(dt, start_date) || !d.is_valid(start_date)) {
			start_date = dt
		}
	}

	return start_date
}

// C++: Date cultivationmethod::absStartDate(const CultivationMethod*, bool)
cultivation_method_abs_start_date :: proc(
	cm: ^Cultivation_Method,
	include_dynamic_worksteps: bool = true,
	allocator := context.allocator,
) -> d.Date {
	if len(cm.all_abs_worksteps) == 0 {
		return d.Date{}
	}

	dyn_earliest_start := d.Date{}
	if include_dynamic_worksteps {
		for ws in cultivation_method_abs_worksteps_at(cm, d.Date{}, allocator) {
			ed := workstep_abs_earliest_date(ws)
			if (d.is_valid(ed) && d.is_valid(dyn_earliest_start) && d.lt(ed, dyn_earliest_start)) ||
			   (d.is_valid(ed) && !d.is_valid(dyn_earliest_start)) {
				dyn_earliest_start = ed
			}
		}
	}

	start_date := dyn_earliest_start
	for ws in cm.all_abs_worksteps {
		ad := workstep_abs_date(ws)
		if d.is_valid(ad) && (d.lt(ad, start_date) || !d.is_valid(start_date)) {
			start_date = ad
		}
	}

	return start_date
}

// C++: Date cultivationmethod::absLatestSowingDate(const CultivationMethod*)
cultivation_method_abs_latest_sowing_date :: proc(cm: ^Cultivation_Method) -> d.Date {
	dyn_latest_sowing_date := d.Date{}
	for ws in cm.all_abs_worksteps {
		t := workstep_type(ws)
		if t == .Sowing || t == .Automatic_Sowing {
			lsd := workstep_abs_latest_date(ws)
			if d.is_valid(lsd) && d.lt(dyn_latest_sowing_date, lsd) {
				dyn_latest_sowing_date = lsd
			}
		}
	}
	return dyn_latest_sowing_date
}

// C++: Date cultivationmethod::endDate(const CultivationMethod*)
cultivation_method_end_date :: proc(
	cm: ^Cultivation_Method,
	allocator := context.allocator,
) -> d.Date {
	if len(cm.all_worksteps) == 0 {
		return d.Date{}
	}

	dyn_latest_end := d.Date{}
	for ws in cultivation_method_worksteps_at(cm, d.Date{}, allocator) {
		ed := workstep_latest_date(ws)
		if (d.is_valid(ed) && d.is_valid(dyn_latest_end) && d.gt(ed, dyn_latest_end)) ||
		   (d.is_valid(ed) && !d.is_valid(dyn_latest_end)) {
			dyn_latest_end = ed
		}
	}

	end_date := dyn_latest_end
	for ws in cm.all_worksteps {
		dt := ws.date
		if d.is_valid(dt) && (d.gt(dt, end_date) || !d.is_valid(end_date)) {
			end_date = dt
		}
	}

	return end_date
}

// C++: Date cultivationmethod::absEndDate(const CultivationMethod*)
cultivation_method_abs_end_date :: proc(
	cm: ^Cultivation_Method,
	allocator := context.allocator,
) -> d.Date {
	if len(cm.all_abs_worksteps) == 0 {
		return d.Date{}
	}

	dyn_latest_end := d.Date{}
	for ws in cultivation_method_abs_worksteps_at(cm, d.Date{}, allocator) {
		ed := workstep_abs_latest_date(ws)
		if (d.is_valid(ed) && d.is_valid(dyn_latest_end) && d.gt(ed, dyn_latest_end)) ||
		   (d.is_valid(ed) && !d.is_valid(dyn_latest_end)) {
			dyn_latest_end = ed
		}
	}

	end_date := dyn_latest_end
	for ws in cm.all_abs_worksteps {
		ad := workstep_abs_date(ws)
		if d.is_valid(ad) && (d.gt(ad, end_date) || !d.is_valid(end_date)) {
			end_date = ad
		}
	}

	return end_date
}

// C++: bool cultivationmethod::reinit(CultivationMethod*, Tools::Date, bool)
cultivation_method_reinit :: proc(
	cm: ^Cultivation_Method,
	date: d.Date,
	force_init_year: bool = false,
) -> bool {
	clear(&cm.all_abs_worksteps)
	clear(&cm.unfinished_dynamic_worksteps)

	added_year := false
	for ws in cm.all_worksteps {
		added_year = workstep_reinit(ws, date, added_year, force_init_year) || added_year
		append(&cm.all_abs_worksteps, ws)
		if !d.is_valid(workstep_abs_date(ws)) {
			append(&cm.unfinished_dynamic_worksteps, ws)
		}
	}

	return added_year
}
