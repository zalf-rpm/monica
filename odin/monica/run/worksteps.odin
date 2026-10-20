// Phase 6 checkpoint 2: the concrete workstep merge/apply/condition/reinit
// functions and make_*_workstep factories, normally spread across
// src/worksteps/*.cpp - see workstep.odin's header comment for why they all
// live in this `run` package instead.
//
// make*Workstep's non-JSON overloads (e.g. makeIrrigationWorkstep(Date,
// double, ...)) are not ported: grepped src/ and confirmed only the
// JSON-taking overload is ever called (from src/run/workstep.cpp's
// makeWorkstep) - the others have no caller anywhere in this codebase.
package run

import clim "../../support/climate"
import d "../../support/date"
import jx "../../support/jsonx"
import tl "../../support/tools"
import core "../core"
import mio "../io"
import p "../params"
import "core:strings"

// ---------------------------------------------------------------------------
// Sowing (src/worksteps/sowing.{h,cpp})
// ---------------------------------------------------------------------------

// C++: Errors workstep::merge(SowingData*, json11::Json)
//
// s.crop_params is default-constructed via make_crop_parameters() before the
// merge below, not left at Odin's zero value: C++'s CropParameters has
// in-class member defaults (e.g. SpeciesParameters::KC25/KO25/AEKC/AEKO/AEVC,
// the Farquhar-model constants), and species/cultivar JSON files routinely
// omit fields that are meant to just take those defaults. Without this,
// every such field silently reads back as 0 instead of its real default -
// this exact gap froze crop growth solid from the second phenological stage
// onward (KC25=KO25=0 makes vc_CO2CompensationPoint's Mkc*Oi/Mko a 0/0 NaN),
// caught by the phase 7 checkpoint 4 runMonica oracle. The same
// "make_X() before merge" fix already applies to Env.params in
// run_monica.odin's env_merge.
sowing_merge :: proc(s: ^Sowing_Data, j: jx.Value, allocator := context.allocator) -> tl.Errors {
	res: tl.Errors
	s.crop_params = p.make_crop_parameters()

	jx.set_iso_date_value(&s.sowing_date, j, "seedDate")
	jx.set_iso_date_value(&s.harvest_date, j, "harvestDate")

	jc := jx.get(j, "crop")
	if jx.is_object(jc) {
		if jx.is_bool(jx.get(jc, "is-perennial-crop")) {
			s.is_perennial_crop = jx.bool_value_of(jx.get(jc, "is-perennial-crop"))
		}

		if jx.has_object_shape(jc, "cropParams") {
			jcps := jx.get(jc, "cropParams")
			if jx.has_object_shape(jcps, "species") && jx.has_object_shape(jcps, "cultivar") {
				_ = p.crop_parameters_merge(&s.crop_params, jcps)
			} else {
				tl.append_error(
					&res,
					strings.concatenate(
						{
							"Couldn't find 'species' or 'cultivar' key in JSON object 'cropParams':\n",
							jx.dump(jc, allocator),
						},
						allocator,
					),
				)
			}

			if val, ok := s.is_perennial_crop.?; ok {
				s.crop_params.cultivar_params.perennial = val
			} else {
				s.is_perennial_crop = s.crop_params.cultivar_params.perennial
			}

			s.is_valid = true
		} else {
			tl.append_error(
				&res,
				strings.concatenate(
					{"Couldn't find 'cropParams' key in JSON object:\n", jx.dump(jc, allocator)},
					allocator,
				),
			)
			s.is_valid = false
		}

		if val, ok := s.is_perennial_crop.?; ok && val {
			if jx.has_object_shape(jc, "perennialCropParams") {
				jcps := jx.get(jc, "perennialCropParams")
				if jx.has_object_shape(jcps, "species") && jx.has_object_shape(jcps, "cultivar") {
					s.separate_perennial_crop_params = new(p.Crop_Parameters, allocator)
					s.separate_perennial_crop_params^ = p.make_crop_parameters()
					_ = p.crop_parameters_merge(s.separate_perennial_crop_params, jcps)
				}
			}
		}

		if jx.has_object_shape(jc, "residueParams") {
			_ = p.crop_residue_parameters_merge(&s.residue_params, jx.get(jc, "residueParams"))
		} else {
			tl.append_error(
				&res,
				strings.concatenate(
					{
						"Couldn't find 'residueParams' key in JSON object:\n",
						jx.dump(jc, allocator),
					},
					allocator,
				),
			)
			s.is_valid = false
		}
	}

	jx.set_int_value(&s.plant_density, j, "PlantDensity")
	if s.plant_density > 0 {
		s.crop_params.species_params.plant_density = s.plant_density
	}
	// FAO-56 Dual Kc: optional initial Kcb at sowing (default 0.15 = bare soil)
	jx.set_double_value(&s.initial_kcb, j, "initialKcb")

	return res
}

// C++: bool workstep::apply(SowingData*, Workstep*, MonicaModel*)
sowing_apply :: proc(
	s: ^Sowing_Data,
	ws: ^Workstep,
	model: ^core.Monica_Model,
	allocator := context.allocator,
) -> bool {
	workstep_apply_common(ws, model)

	model.days_with_crop = 0
	model.accu_n_stress = 0.0
	model.accu_water_stress = 0.0
	model.accu_heat_stress = 0.0
	model.accu_oxygen_stress = 0.0

	if s.is_valid {
		model.cultivation_method_count += 1

		model.current_crop_module = nil
		cm := new(core.Crop_Module, allocator)
		cm^ = core.make_crop_module(
			&model.soil_column,
			&s.crop_params,
			&s.residue_params,
			&model.site_params,
			&model.crop_mod_params,
			&model.sim_params,
			core.monica_model_fire_event_cb,
			core.monica_model_add_organic_matter_cb,
			core.monica_model_get_snow_depth_cb,
			nil,
			allocator,
		)
		model.current_crop_module = cm

		if s.separate_perennial_crop_params != nil {
			model.current_crop_module.perennial_crop_params = new(p.Crop_Parameters, allocator)
			model.current_crop_module.perennial_crop_params^ = core.clone_crop_parameters(
				s.separate_perennial_crop_params^,
				allocator,
			)
		}

		model.soil_transport.crop_module = model.current_crop_module
		model.soil_column.crop_module = model.current_crop_module
		model.soil_moisture.crop_module = model.current_crop_module
		model.soil_organic.crop_module = model.current_crop_module

		if model.sim_params.use_n_min_mineral_fertilising_method &&
		   !model.current_crop_module.crop_params.cultivar_params.winter_crop {
			core.clear_top_dressing_params(&model.soil_column)
			fert_amount := core.monica_model_apply_mineral_fertiliser_via_n_min_method(
				model,
				model.sim_params.n_min_fertiliser_partition,
				p.NMin_Crop_Parameters {
					samplingDepth = s.crop_params.species_params.sampling_depth,
					nTarget = s.crop_params.species_params.target_n_sampling_depth,
					nTarget30 = s.crop_params.species_params.target_n30,
				},
			)
			core.monica_model_add_daily_sum_fertiliser(model, fert_amount)
		}
	}

	// FAO-56 Dual Kc: push initial Kcb into the freshly created crop module
	if model.sim_params.dual_kc_method && model.current_crop_module != nil {
		model.current_crop_module.kcb_ini = s.initial_kcb
	}
	model.current_events["Sowing"] = true

	return true
}

// C++: Workstep monica::makeSowingWorkstep(json11::Json)
make_sowing_workstep :: proc(j: jx.Value, allocator := context.allocator) -> ^Workstep {
	ws := new_workstep(allocator)
	ws.data = Sowing_Data {
		plant_density = -1,
		initial_kcb   = 0.15,
	}
	res := workstep_merge_common(ws, j)
	tl.append_errors(&res, sowing_merge(&ws.data.(Sowing_Data), j, allocator))
	ws.errors = res
	return ws
}

// ---------------------------------------------------------------------------
// Transplant (src/worksteps/transplant.{h,cpp})
// ---------------------------------------------------------------------------

// C++: Errors workstep::merge(TransplantData*, json11::Json)
transplant_merge :: proc(
	t: ^Transplant_Data,
	j: jx.Value,
	allocator := context.allocator,
) -> tl.Errors {
	// Mirrors Sowing's own merge (this is the only place that touches the
	// common Workstep fields, via mergeCommon, done once by the make*Workstep
	// factory - not repeated here).
	res := sowing_merge(&t.sowing, j, allocator)

	init_stage_v := jx.get(j, "initialStage")
	if !jx.is_null(init_stage_v) {
		t.initial_stage = jx.int_value_of(init_stage_v)
	}
	jx.set_double_value(&t.initial_gdd, j, "initialTemperatureSum")
	jx.set_double_value(&t.init_root_mass, j, "initialRootBiomass")
	jx.set_double_value(&t.init_leaf_mass, j, "initialLeafBiomass")
	jx.set_double_value(&t.init_shoot_mass, j, "initialShootBiomass")
	jx.set_double_value(&t.init_lai, j, "initialLAI")
	jx.set_int_value(&t.post_transplant_delay, j, "postTransplantDelay")
	// FAO-56 Dual Kc: optional initial Kcb at transplanting - re-parses the
	// same key sowing_merge (above) already parsed into this same promoted
	// field; harmless (idempotent), kept to mirror the C++'s two parse calls
	// (see Transplant_Data's doc comment for why there's only one field here)
	jx.set_double_value(&t.initial_kcb, j, "initialKcb")

	return res // propagates ALL sub-errors (crop parse errors included)
}

// C++: bool workstep::apply(TransplantData*, Workstep*, MonicaModel*)
transplant_apply :: proc(
	t: ^Transplant_Data,
	ws: ^Workstep,
	model: ^core.Monica_Model,
	allocator := context.allocator,
) -> bool {
	sowing_apply(&t.sowing, ws, model, allocator)

	cropModule := model.current_crop_module
	if cropModule == nil {
		return false
	}

	core.force_transplant_state(
		cropModule,
		t.initial_gdd,
		t.init_lai,
		t.initial_stage,
		t.init_root_mass,
		t.init_leaf_mass,
		t.init_shoot_mass,
		t.post_transplant_delay,
	)

	if model.sim_params.dual_kc_method {
		cropModule.kcb_ini = t.initial_kcb
	}
	model.current_events["Transplant"] = true

	return true
}

// C++: Workstep monica::makeTransplantWorkstep(json11::Json)
make_transplant_workstep :: proc(j: jx.Value, allocator := context.allocator) -> ^Workstep {
	ws := new_workstep(allocator)
	ws.data = Transplant_Data {
		sowing = {plant_density = -1, initial_kcb = 0.15},
		initial_stage = 2,
	}
	res := workstep_merge_common(ws, j)
	tl.append_errors(&res, transplant_merge(&ws.data.(Transplant_Data), j, allocator))
	ws.errors = res
	return ws
}

// ---------------------------------------------------------------------------
// AutomaticSowing (src/worksteps/automatic-sowing.{h,cpp})
// ---------------------------------------------------------------------------

// C++: bool isSoilTemperatureOk(const vector<double>&, int, double) -
//        anonymous-namespace helper in automatic-sowing.cpp
@(private)
is_soil_temperature_ok :: proc(
	soilTemps: [dynamic]f64,
	windowDays: int,
	targetAvgSoilTemp: f64,
) -> bool {
	if len(soilTemps) == 0 {
		return false
	}
	sum := 0.0
	size := len(soilTemps)
	for i := size - 1; i >= 0 && i >= size - windowDays; i -= 1 {
		sum += soilTemps[i]
	}
	avg := sum / f64(size > windowDays ? windowDays : size)
	return avg >= targetAvgSoilTemp
}

// C++: Errors workstep::merge(AutomaticSowingData*, json11::Json)
automatic_sowing_merge :: proc(
	as: ^Automatic_Sowing_Data,
	j: jx.Value,
	allocator := context.allocator,
) -> tl.Errors {
	res := sowing_merge(&as.sowing, j, allocator)

	jx.set_iso_date_value(&as.earliest_date, j, "earliest-date")
	jx.set_iso_date_value(&as.latest_date, j, "latest-date")
	jx.set_double_value(&as.min_temp_threshold, j, "min-temp")
	jx.set_int_value(&as.days_in_temp_window, j, "days-in-temp-window")
	jx.set_double_value(&as.min_percent_asw, j, "min-%-asw")
	jx.set_double_value(&as.max_percent_asw, j, "max-%-asw")
	jx.set_double_value(&as.max_3day_precip_sum, j, "max-3d-precip")
	jx.set_double_value(&as.max_current_day_precip_sum, j, "max-curr-day-precip")
	jx.set_double_value(&as.temp_sum_above_base_temp, j, "temp-sum-above-base-temp")
	jx.set_double_value(&as.base_temp, j, "base-temp")

	avgSoilTemp := jx.get(j, "avg-soil-temp")
	if jx.is_object(avgSoilTemp) {
		jx.set_double_value(&as.soil_depth_for_averaging, avgSoilTemp, "depth")
		jx.set_int_value(&as.days_in_soil_temp_window, avgSoilTemp, "days")
		jx.set_double_value(&as.sowing_if_above_avg_soil_temp, avgSoilTemp, "Tavg")
		as.check_for_soil_temperature =
			as.soil_depth_for_averaging > 0 &&
			as.days_in_soil_temp_window > 0 &&
			as.sowing_if_above_avg_soil_temp > 0
	}

	return res
}

// C++: bool workstep::apply(AutomaticSowingData*, Workstep*, MonicaModel*)
automatic_sowing_apply :: proc(
	as: ^Automatic_Sowing_Data,
	ws: ^Workstep,
	model: ^core.Monica_Model,
	allocator := context.allocator,
) -> bool {
	currentDate := model.current_step_date
	as.sowing_date = currentDate

	sowing_apply(&as.sowing, ws, model, allocator)
	model.current_events["AutomaticSowing"] = true
	as.crop_seeded = true
	as.in_sowing_range = false

	return true
}

// Package-level "the AutomaticSowingData currently registered for a daily
// callback" - the same capture-workaround shape as
// core.monica_model_fire_event_cb, but NOT verified against a real caller:
// run-monica.cpp (phase 7) is the only real caller of
// registerDailyFunction/this returned function, and no fixture in this repo
// uses AutomaticSowing at all (crop-min.json only has Sowing/
// NDemandFertilization/AutomaticHarvest/AutomaticIrrigation/
// OrganicFertilization). Unlike MonicaModel (genuinely one per process),
// multiple AutomaticSowing worksteps active at once would each need their
// own daily history, which a single global cannot support - kept only for
// interface completeness until phase 7 reveals the real caller-side shape.
@(private)
g_current_automatic_sowing: ^Automatic_Sowing_Data

@(private)
automatic_sowing_avg_soil_temp_fn :: proc(model: ^core.Monica_Model) -> f64 {
	as := g_current_automatic_sowing
	avgSoilTemp := 0.0
	i := 0
	size := core.get_layer_number_for_depth(&model.soil_column, as.soil_depth_for_averaging) + 1
	for ; i < size; i += 1 {
		avgSoilTemp += model.soil_temperature.soil_column.layers[i].soil_temperature
	}
	return avgSoilTemp / f64(i)
}

// C++: std::function<double(MonicaModel*)> workstep::registerDailyFunction(
//        AutomaticSowingData*, std::function<vector<double>&()>)
automatic_sowing_register_daily_function :: proc(
	as: ^Automatic_Sowing_Data,
	getDailyValues: proc() -> [dynamic]f64,
) -> (
	proc(_: ^core.Monica_Model) -> f64,
	bool,
) {
	if !as.check_for_soil_temperature {
		return nil, false
	}
	as.get_avg_soil_temps = getDailyValues
	g_current_automatic_sowing = as
	return automatic_sowing_avg_soil_temp_fn, true
}

// C++: bool workstep::condition(AutomaticSowingData*, MonicaModel*)
automatic_sowing_condition :: proc(as: ^Automatic_Sowing_Data, model: ^core.Monica_Model) -> bool {
	if as.crop_seeded {
		return false
	}

	currentDate := model.current_step_date

	if !as.in_sowing_range && d.lt(currentDate, as.abs_earliest_date) {
		return false
	} else {
		as.in_sowing_range = true
	}

	if as.in_sowing_range && d.ge(currentDate, as.abs_latest_date) {
		return true
	}

	// check soil temperature if requested
	if as.check_for_soil_temperature {
		if !is_soil_temperature_ok(
			as.get_avg_soil_temps(),
			as.days_in_soil_temp_window,
			as.sowing_if_above_avg_soil_temp,
		) {
			return false
		}
	}

	cd := model.climate_data
	currentCd := cd[len(cd) - 1]

	avg :: proc(cd: [dynamic]map[clim.ACD]f64, acd: clim.ACD, daysInTempWindow: int) -> f64 {
		n := min(len(cd), daysInTempWindow)
		sum := 0.0
		for i := len(cd) - 1; i >= len(cd) - n; i -= 1 {
			if v, ok := cd[i][acd]; ok {
				sum += v
			}
		}
		return sum / f64(n)
	}

	// check temperature
	Tok := false
	if as.crop_params.cultivar_params.winter_crop {
		avgTavg := avg(cd, .tavg, as.days_in_temp_window)
		Tok = avgTavg <= as.min_temp_threshold
	} else {
		avgTmin := avg(cd, .tmin, as.days_in_temp_window)
		avgTminOk := avgTmin >= as.min_temp_threshold
		TminOk := currentCd[.tmin] >= as.min_temp_threshold
		Tok = avgTminOk && TminOk
	}

	if !Tok {
		return false
	}

	// check soil moisture
	if !is_soil_moisture_ok(model, as.min_percent_asw, as.max_percent_asw) {
		return false
	}

	// check precipitation
	if !is_precipitation_ok(cd, as.max_3day_precip_sum, as.max_current_day_precip_sum) {
		return false
	}

	// check temperature sum
	baseTemp := as.base_temp
	tempSum := 0.0
	for m in cd {
		if v, ok := m[.tavg]; ok {
			tempSum += max(0.0, v - baseTemp)
		}
	}
	if tempSum < as.temp_sum_above_base_temp {
		return false
	}

	return true
}

// C++: bool workstep::reinit(AutomaticSowingData*, Workstep*, Date, bool, bool)
automatic_sowing_reinit :: proc(
	as: ^Automatic_Sowing_Data,
	ws: ^Workstep,
	date: d.Date,
	addYear: bool = false,
	forceInitYear: bool = false,
) -> bool {
	_ = workstep_reinit_common(ws, date, addYear)

	as.crop_seeded = false
	as.in_sowing_range = false
	ws.date = d.Date{}
	as.sowing_date = d.Date{}

	// init first the latest date, if the latest date stays in current year, so
	// has to stay the earliest date (thus force current year) if there is a
	// forced current (init) year, this will force both dates to this year
	addedYear1: bool
	as.abs_latest_date, addedYear1 = make_init_abs_date(as.latest_date, date, addYear, forceInitYear)
	addedYear2: bool
	as.abs_earliest_date, addedYear2 = make_init_abs_date(
		as.earliest_date,
		date,
		addYear,
		forceInitYear || !addedYear1,
	)
	_ = addedYear2

	return addedYear1 // || addedYear2
}

// C++: Workstep monica::makeAutomaticSowingWorkstep(json11::Json)
make_automatic_sowing_workstep :: proc(j: jx.Value, allocator := context.allocator) -> ^Workstep {
	ws := new_workstep(allocator)
	ws.data = Automatic_Sowing_Data {
		sowing = {plant_density = -1, initial_kcb = 0.15},
		max_percent_asw = 100,
		soil_depth_for_averaging = 0.30,
	}
	res := workstep_merge_common(ws, j)
	tl.append_errors(&res, automatic_sowing_merge(&ws.data.(Automatic_Sowing_Data), j, allocator))
	ws.errors = res
	return ws
}

// ---------------------------------------------------------------------------
// Harvest (src/worksteps/harvest.{h,cpp})
// ---------------------------------------------------------------------------

// C++: Errors workstep::merge(HarvestData*, json11::Json)
harvest_merge :: proc(h: ^Harvest_Data, j: jx.Value, allocator := context.allocator) -> tl.Errors {
	res: tl.Errors

	jx.set_int_value(&h.incorporate_into_layer_no, j, "incorporateIntoLayerNo")
	h.incorporate_into_layer_no = max(1, h.incorporate_into_layer_no)
	jx.set_bool_value(&h.exported, j, "exported")
	jx.set_bool_value(&h.opt_carb_mgmt_data.optCarbonConservation, j, "opt-carbon-conservation")
	jx.set_double_value(
		&h.opt_carb_mgmt_data.cropImpactOnHumusBalance,
		j,
		"crop-impact-on-humus-balance",
	)
	cu := jx.string_value(j, "crop-usage")
	if cu == "green-manure" {
		h.opt_carb_mgmt_data.cropUsage = .Green_Manure
	} else {
		h.opt_carb_mgmt_data.cropUsage = .Biomass_Production
	}
	jx.set_double_value(&h.opt_carb_mgmt_data.residueHeq, j, "residue-heq")
	jx.set_double_value(&h.opt_carb_mgmt_data.organicFertilizerHeq, j, "organic-fertilizer-heq")
	jx.set_double_value(
		&h.opt_carb_mgmt_data.maxResidueRecoverFraction,
		j,
		"max-residue-recover-fraction",
	)

	for organName in ([]string{"leaf", "shoot", "fruit", "struct", "sugar"}) {
		for k, v in jx.object_items(j) {
			if tl.to_lower(k, allocator) == organName && jx.is_object(v) {
				sv := core.Harvest_Spec_Value {
					exportPercentage = 100.0,
					incorporate      = true,
				}
				jx.set_double_value(&sv.exportPercentage, v, "export")
				if h.spec.organ2specVal == nil {
					h.spec.organ2specVal = make(map[int]core.Harvest_Spec_Value, 0, allocator)
				}
				h.spec.organ2specVal[organ_id_from_name(k, &res)] = sv
			}
		}
	}

	return res
}

// C++: bool workstep::apply(HarvestData*, Workstep*, MonicaModel*)
harvest_apply :: proc(
	h: ^Harvest_Data,
	ws: ^Workstep,
	model: ^core.Monica_Model,
	allocator := context.allocator,
) -> bool {
	workstep_apply_common(ws, model)

	if model.current_crop_module != nil {
		core.monica_model_harvest_current_crop(
			model,
			h.exported,
			h.spec,
			h.opt_carb_mgmt_data,
			h.incorporate_into_layer_no - 1,
			allocator,
		)
		model.current_events["Harvest"] = true
	}

	return true
}

// C++: Workstep monica::makeHarvestWorkstep(json11::Json)
make_harvest_workstep :: proc(j: jx.Value, allocator := context.allocator) -> ^Workstep {
	ws := new_workstep(allocator)
	ws.data = Harvest_Data {
		exported = true,
		incorporate_into_layer_no = 1,
		opt_carb_mgmt_data = {maxResidueRecoverFraction = 1, cropUsage = .Biomass_Production},
	}
	res := workstep_merge_common(ws, j)
	tl.append_errors(&res, harvest_merge(&ws.data.(Harvest_Data), j, allocator))
	ws.errors = res
	return ws
}

// ---------------------------------------------------------------------------
// AutomaticHarvest (src/worksteps/automatic-harvest.{h,cpp})
// ---------------------------------------------------------------------------

// C++: Errors workstep::merge(AutomaticHarvestData*, json11::Json)
automatic_harvest_merge :: proc(
	ah: ^Automatic_Harvest_Data,
	j: jx.Value,
	allocator := context.allocator,
) -> tl.Errors {
	res := harvest_merge(&ah.harvest, j, allocator)

	jx.set_iso_date_value(&ah.latest_date, j, "latest-date")
	jx.set_double_value(&ah.min_percent_asw, j, "min-%-asw")
	jx.set_double_value(&ah.max_percent_asw, j, "max-%-asw")
	jx.set_double_value(&ah.max_3day_precip_sum, j, "max-3d-precip-sum")
	jx.set_double_value(&ah.max_current_day_precip_sum, j, "max-curr-day-precip")
	jx.set_string_value(&ah.harvest_time, j, "harvest-time")

	return res
}

// C++: bool workstep::apply(AutomaticHarvestData*, Workstep*, MonicaModel*)
automatic_harvest_apply :: proc(
	ah: ^Automatic_Harvest_Data,
	ws: ^Workstep,
	model: ^core.Monica_Model,
	allocator := context.allocator,
) -> bool {
	harvest_apply(&ah.harvest, ws, model, allocator)

	model.current_events["AutomaticHarvest"] = true
	ah.crop_harvested = true

	return true
}

// C++: bool workstep::condition(AutomaticHarvestData*, MonicaModel*)
automatic_harvest_condition :: proc(
	ah: ^Automatic_Harvest_Data,
	model: ^core.Monica_Model,
) -> bool {
	conditionMet := false

	cg := model.current_crop_module
	// got a crop and not yet harvested
	if cg != nil && !ah.crop_harvested {
		conditionMet =
			d.ge(model.current_step_date, ah.abs_latest_date) ||
			(ah.harvest_time == "maturity" &&
					core.maturity_reached(cg) &&
					is_soil_moisture_ok(model, ah.min_percent_asw, ah.max_percent_asw) &&
					is_precipitation_ok(
						model.climate_data, // harvest after or at latest date// has maturity been reached// check soil moisture
						ah.max_3day_precip_sum,
						ah.max_current_day_precip_sum,
					)) // check precipitation
	}

	return conditionMet
}

// C++: bool workstep::reinit(AutomaticHarvestData*, Workstep*, Date, bool, bool)
automatic_harvest_reinit :: proc(
	ah: ^Automatic_Harvest_Data,
	ws: ^Workstep,
	date: d.Date,
	addYear: bool = false,
	forceInitYear: bool = false,
) -> bool {
	_ = workstep_reinit_common(ws, date, addYear)

	ah.crop_harvested = false
	ws.date = d.Date{}

	addedYear: bool
	ah.abs_latest_date, addedYear = make_init_abs_date(ah.latest_date, date, addYear, forceInitYear)

	return addedYear
}

// C++: Workstep monica::makeAutomaticHarvestWorkstep(json11::Json)
make_automatic_harvest_workstep :: proc(j: jx.Value, allocator := context.allocator) -> ^Workstep {
	ws := new_workstep(allocator)
	ws.data = Automatic_Harvest_Data {
		harvest_time               = "maturity",
		max_percent_asw            = 999,
		max_3day_precip_sum        = 9999,
		max_current_day_precip_sum = 9999,
	}
	{
		ahd := &ws.data.(Automatic_Harvest_Data)
		ahd.harvest.exported = true
		ahd.harvest.incorporate_into_layer_no = 1
		ahd.harvest.opt_carb_mgmt_data = {
			maxResidueRecoverFraction = 1,
			cropUsage                 = .Biomass_Production,
		}
	}
	res := workstep_merge_common(ws, j)
	tl.append_errors(
		&res,
		automatic_harvest_merge(&ws.data.(Automatic_Harvest_Data), j, allocator),
	)
	ws.errors = res
	return ws
}

// ---------------------------------------------------------------------------
// Cutting (src/worksteps/cutting.{h,cpp})
// ---------------------------------------------------------------------------

// C++: Errors workstep::merge(CuttingData*, json11::Json)
cutting_merge :: proc(c: ^Cutting_Data, j: jx.Value, allocator := context.allocator) -> tl.Errors {
	errors: tl.Errors

	export_v := jx.get(j, "export")
	export_ := jx.is_bool(export_v) ? jx.bool_value_of(export_v) : true

	for k, v in jx.object_items(jx.get(j, "organs")) {
		oid := organ_id_from_name(k, &errors)
		if oid == -1 {
			continue
		}
		val := core.Cutting_Value{}
		arr := jx.array_items(v)
		if len(arr) > 0 {
			val.value = jx.number_value(arr[0])
		}
		if len(arr) > 1 {
			val.unit = .Percentage
			p2 := jx.string_value_of(arr[1])
			if p2 == "kg ha-1" {
				val.unit = .Biomass
			} else if p2 == "m2 m-2" && oid == 1 {
				val.unit = .LAI
			} else if p2 == "%" {
				val.value = val.value / 100.0
			} else {
				val.value = val.value / 100.0
				tl.append_error(
					&errors,
					strings.concatenate(
						{"Unknown unit: ", p2, " in Cutting workstep: ", jx.dump(j, allocator)},
						allocator,
					),
				)
			}
		}
		if len(arr) > 2 {
			col := jx.string_value_of(arr[2])
			if col == "cut" {
				val.cut_or_left = .Cut
			} else if col == "left" {
				val.cut_or_left = .Left
			} else {
				val.cut_or_left = .None
			}
		}

		if c.organ_id_2_cutting_spec == nil {
			c.organ_id_2_cutting_spec = make(map[int]core.Cutting_Value, 0, allocator)
		}
		c.organ_id_2_cutting_spec[oid] = val
		if c.organ_id_2_export_fraction == nil {
			c.organ_id_2_export_fraction = make(map[int]f64, 0, allocator)
		}
		c.organ_id_2_export_fraction[oid] = export_ ? 1 : 0
	}

	for k, v in jx.object_items(jx.get(j, "export")) {
		oid := organ_id_from_name(k, &errors)
		if oid == -1 {
			continue
		}
		if c.organ_id_2_export_fraction == nil {
			c.organ_id_2_export_fraction = make(map[int]f64, 0, allocator)
		}
		c.organ_id_2_export_fraction[oid] = f64(jx.int_value_d(v, 0)) / 100.0
	}

	jx.set_double_value(
		&c.cut_max_assimilation_rate_fraction,
		j,
		"cut-max-assimilation-rate",
		.PERCENT,
	)

	return errors
}

// C++: bool workstep::apply(CuttingData*, Workstep*, MonicaModel*)
cutting_apply :: proc(c: ^Cutting_Data, ws: ^Workstep, model: ^core.Monica_Model) -> bool {
	workstep_apply_common(ws, model)

	assert(model.current_crop_module != nil)
	core.apply_cutting(
		model.current_crop_module,
		c.organ_id_2_cutting_spec,
		c.organ_id_2_export_fraction,
		c.cut_max_assimilation_rate_fraction,
	)
	model.current_events["Cutting"] = true

	return true
}

// C++: Workstep monica::makeCuttingWorkstep(json11::Json)
make_cutting_workstep :: proc(j: jx.Value, allocator := context.allocator) -> ^Workstep {
	ws := new_workstep(allocator)
	ws.data = Cutting_Data {
		cut_max_assimilation_rate_fraction = 1.0,
	}
	res := workstep_merge_common(ws, j)
	tl.append_errors(&res, cutting_merge(&ws.data.(Cutting_Data), j, allocator))
	ws.errors = res
	return ws
}

// ---------------------------------------------------------------------------
// MineralFertilization (src/worksteps/mineral-fertilization.{h,cpp})
// ---------------------------------------------------------------------------

// C++: Errors workstep::merge(MineralFertilizationData*, json11::Json)
mineral_fertilization_merge :: proc(mf: ^Mineral_Fertilization_Data, j: jx.Value) -> tl.Errors {
	res: tl.Errors
	if jx.has_object_shape(j, "partition") {
		_ = p.mineral_fertilizer_parameters_merge(&mf.partition, jx.get(j, "partition"))
	}
	jx.set_double_value(&mf.amount, j, "amount")
	return res
}

// C++: bool workstep::apply(MineralFertilizationData*, Workstep*, MonicaModel*)
mineral_fertilization_apply :: proc(
	mf: ^Mineral_Fertilization_Data,
	ws: ^Workstep,
	model: ^core.Monica_Model,
) -> bool {
	workstep_apply_common(ws, model)
	core.monica_model_apply_mineral_fertiliser(model, mf.partition, mf.amount)
	model.current_events["MineralFertilization"] = true
	return true
}

// C++: Workstep monica::makeMineralFertilizationWorkstep(json11::Json)
make_mineral_fertilization_workstep :: proc(
	j: jx.Value,
	allocator := context.allocator,
) -> ^Workstep {
	ws := new_workstep(allocator)
	ws.data = Mineral_Fertilization_Data{}
	res := workstep_merge_common(ws, j)
	tl.append_errors(&res, mineral_fertilization_merge(&ws.data.(Mineral_Fertilization_Data), j))
	ws.errors = res
	return ws
}

// ---------------------------------------------------------------------------
// NDemandFertilization (src/worksteps/n-demand-fertilization.{h,cpp})
// ---------------------------------------------------------------------------

// C++: Errors workstep::merge(NDemandFertilizationData*, Workstep*, json11::Json)
//
// note: merge needs ws (it copies the just-parsed common date into
// initial_date).
n_demand_fertilization_merge :: proc(
	nd: ^N_Demand_Fertilization_Data,
	ws: ^Workstep,
	j: jx.Value,
) -> tl.Errors {
	res: tl.Errors
	nd.initial_date = ws.date
	jx.set_double_value(&nd.n_demand, j, "N-demand")
	if jx.has_object_shape(j, "partition") {
		_ = p.mineral_fertilizer_parameters_merge(&nd.partition, jx.get(j, "partition"))
	}
	jx.set_double_value(&nd.depth, j, "depth")
	jx.set_int_value(&nd.stage, j, "stage")

	return res
}

// C++: bool workstep::apply(NDemandFertilizationData*, Workstep*, MonicaModel*)
n_demand_fertilization_apply :: proc(
	nd: ^N_Demand_Fertilization_Data,
	ws: ^Workstep,
	model: ^core.Monica_Model,
) -> bool {
	workstep_apply_common(ws, model)

	rd := model.current_crop_module.rooting_depth_m
	appliedAmount := core.apply_mineral_fertiliser_via_n_demand(
		&model.soil_column,
		nd.partition,
		rd < nd.depth ? rd : nd.depth,
		nd.n_demand,
	)
	// daily AND sum (SumNFert), as in the original C++ - the converted C++ only
	// added to the daily value (fixed there too)
	core.monica_model_add_daily_sum_fertiliser(model, appliedAmount)
	nd.applied_fertilizer = true
	// record date of application until next reinit
	ws.date = model.current_step_date
	model.current_events["NDemandFertilization"] = true

	return true
}

// C++: bool workstep::condition(NDemandFertilizationData*, Workstep*, MonicaModel*)
n_demand_fertilization_condition :: proc(
	nd: ^N_Demand_Fertilization_Data,
	ws: ^Workstep,
	model: ^core.Monica_Model,
) -> bool {
	conditionMet := false

	cg := model.current_crop_module
	if cg != nil && !nd.applied_fertilizer {
		currStage := cg.developmental_stage + 1
		conditionMet = d.is_valid(ws.date) || currStage == nd.stage // reached the requested stage
	}

	return conditionMet
}

// C++: bool workstep::reinit(NDemandFertilizationData*, Workstep*, Date, bool, bool)
n_demand_fertilization_reinit :: proc(
	nd: ^N_Demand_Fertilization_Data,
	ws: ^Workstep,
	date: d.Date,
	addYear: bool = false,
	forceInitYear: bool = false,
) -> bool {
	ws.date = nd.initial_date
	_ = workstep_reinit_common(ws, date, addYear, forceInitYear)
	nd.applied_fertilizer = false

	// NOTE: original NDemandFertilization::reinit computes addedYear but
	// always returns false unconditionally - preserved exactly.
	return false
}

// C++: Workstep monica::makeNDemandFertilizationWorkstep(json11::Json)
make_n_demand_fertilization_workstep :: proc(
	j: jx.Value,
	allocator := context.allocator,
) -> ^Workstep {
	ws := new_workstep(allocator)
	ws.data = N_Demand_Fertilization_Data {
		stage = 1,
	}
	res := workstep_merge_common(ws, j)
	tl.append_errors(
		&res,
		n_demand_fertilization_merge(&ws.data.(N_Demand_Fertilization_Data), ws, j),
	)
	ws.errors = res
	return ws
}

// ---------------------------------------------------------------------------
// OrganicFertilization (src/worksteps/organic-fertilization.{h,cpp})
// ---------------------------------------------------------------------------

// C++: Errors workstep::merge(OrganicFertilizationData*, json11::Json)
organic_fertilization_merge :: proc(of: ^Organic_Fertilization_Data, j: jx.Value) -> tl.Errors {
	res: tl.Errors
	_ = p.organic_matter_parameters_merge(&of.params, jx.get(j, "parameters"))
	jx.set_double_value(&of.amount, j, "amount")
	jx.set_int_value(&of.incorporate_into_layer_no, j, "incorporateIntoLayerNo")
	of.incorporate_into_layer_no = max(1, of.incorporate_into_layer_no)
	jx.set_bool_value(&of.incorporation, j, "incorporation")
	return res
}

// C++: bool workstep::apply(OrganicFertilizationData*, Workstep*, MonicaModel*)
organic_fertilization_apply :: proc(
	of: ^Organic_Fertilization_Data,
	ws: ^Workstep,
	model: ^core.Monica_Model,
) -> bool {
	workstep_apply_common(ws, model)
	core.monica_model_apply_organic_fertiliser(
		model,
		&of.params,
		of.amount,
		of.incorporation,
		of.incorporate_into_layer_no - 1,
	)
	model.current_events["OrganicFertilization"] = true
	return true
}

// C++: Workstep monica::makeOrganicFertilizationWorkstep(json11::Json)
make_organic_fertilization_workstep :: proc(
	j: jx.Value,
	allocator := context.allocator,
) -> ^Workstep {
	ws := new_workstep(allocator)
	ws.data = Organic_Fertilization_Data {
		incorporate_into_layer_no = 1,
	}
	res := workstep_merge_common(ws, j)
	tl.append_errors(&res, organic_fertilization_merge(&ws.data.(Organic_Fertilization_Data), j))
	ws.errors = res
	return ws
}

// ---------------------------------------------------------------------------
// Tillage (src/worksteps/tillage.{h,cpp})
// ---------------------------------------------------------------------------

// C++: Errors workstep::merge(TillageData*, json11::Json)
tillage_merge :: proc(t: ^Tillage_Data, j: jx.Value) -> tl.Errors {
	res: tl.Errors
	jx.set_double_value(&t.depth, j, "depth")
	return res
}

// C++: bool workstep::apply(TillageData*, Workstep*, MonicaModel*)
tillage_apply :: proc(t: ^Tillage_Data, ws: ^Workstep, model: ^core.Monica_Model) -> bool {
	workstep_apply_common(ws, model)
	core.monica_model_apply_tillage(model, t.depth)
	model.current_events["Tillage"] = true
	return true
}

// C++: Workstep monica::makeTillageWorkstep(json11::Json)
make_tillage_workstep :: proc(j: jx.Value, allocator := context.allocator) -> ^Workstep {
	ws := new_workstep(allocator)
	ws.data = Tillage_Data {
		depth = 0.3,
	}
	res := workstep_merge_common(ws, j)
	tl.append_errors(&res, tillage_merge(&ws.data.(Tillage_Data), j))
	ws.errors = res
	return ws
}

// ---------------------------------------------------------------------------
// Irrigation (src/worksteps/irrigation.{h,cpp})
// ---------------------------------------------------------------------------

// C++: Errors workstep::merge(IrrigationData*, json11::Json)
irrigation_merge :: proc(i: ^Irrigation_Data, j: jx.Value) -> tl.Errors {
	res: tl.Errors
	jx.set_double_value(&i.amount, j, "amount")
	if jx.is_object(jx.get(j, "parameters")) {
		_ = p.irrigation_parameters_merge(&i.params, jx.get(j, "parameters"))
	}
	return res
}

// C++: bool workstep::apply(IrrigationData*, Workstep*, MonicaModel*)
irrigation_apply :: proc(i: ^Irrigation_Data, ws: ^Workstep, model: ^core.Monica_Model) -> bool {
	workstep_apply_common(ws, model)

	core.monica_model_apply_irrigation(model, i.amount, i.params.nitrate_concentration)
	// FAO-56 Dual Kc: push event-level fw and isDrip into SoilMoisture for
	// today's ET calculation. LIMITATION: Auto-irrigation uses sim.json params
	// or defaults (fw=1.0, isDrip=false).
	if model.sim_params.dual_kc_method {
		model.soil_moisture.irrig_fw_event = i.params.fw
		model.soil_moisture.irrig_is_drip_event = i.params.is_drip_irrigation
	}
	model.current_events["Irrigation"] = true

	return true
}

// C++: Workstep monica::makeIrrigationWorkstep(json11::Json)
make_irrigation_workstep :: proc(j: jx.Value, allocator := context.allocator) -> ^Workstep {
	ws := new_workstep(allocator)
	ws.data = Irrigation_Data {
		params = p.Irrigation_Parameters{fw = 1.0},
	}
	res := workstep_merge_common(ws, j)
	tl.append_errors(&res, irrigation_merge(&ws.data.(Irrigation_Data), j))
	ws.errors = res
	return ws
}

// ---------------------------------------------------------------------------
// AutomaticIrrigation (src/worksteps/automatic-irrigation.{h,cpp})
// ---------------------------------------------------------------------------

// C++: Errors workstep::merge(AutomaticIrrigationData*, json11::Json)
automatic_irrigation_merge :: proc(ai: ^Automatic_Irrigation_Data, j: jx.Value) -> tl.Errors {
	res: tl.Errors

	jx.set_int_value(&ai.start_stage, j, "startStage")
	// NOTE(c++-quirk): 1-based stages (user side) -> 0-based stages (model
	// side). The C++ does `startStage = max(0, startStage--)` - the
	// post-decrement's side effect (decrementing startStage by one) is
	// immediately overwritten by the assignment right after it, so the
	// decrement is a complete no-op. Net effect is exactly `startStage =
	// max(0, startStage)`; reproduced without the pointless decrement.
	if ai.start_stage > -1 {
		ai.start_stage = max(0, ai.start_stage)
	}

	jx.set_int_value(&ai.end_stage, j, "endStage")
	// same clobbered-decrement non-effect as startStage above
	if ai.end_stage > -1 {
		ai.end_stage = min(7, ai.end_stage)
	}

	jx.set_bool_value(&ai.irrigate_crop, j, "irrigateCrop")
	if ai.start_stage > -1 || ai.end_stage > -1 {
		ai.irrigate_crop = true
	}

	if jx.is_object(jx.get(j, "parameters")) {
		_ = p.automatic_irrigation_parameters_merge(&ai.params, jx.get(j, "parameters"))
	}

	return res
}

// C++: bool workstep::apply(AutomaticIrrigationData*, MonicaModel*)
automatic_irrigation_apply :: proc(
	ai: ^Automatic_Irrigation_Data,
	model: ^core.Monica_Model,
) -> bool {
	if ai.done {
		return true
	}

	irrigationTriggered, irrigationAmount := core.apply_irrigation_via_trigger(
		&model.soil_column,
		&ai.params,
	)
	if irrigationTriggered {
		model.current_events["AutomaticIrrigation"] = true
		model.soil_organic.irrigation_amount += irrigationAmount
		core.monica_model_add_daily_sum_irrigation_water(model, irrigationAmount)
	}

	return false
}

// C++: bool workstep::condition(AutomaticIrrigationData*, MonicaModel*)
automatic_irrigation_condition :: proc(
	ai: ^Automatic_Irrigation_Data,
	model: ^core.Monica_Model,
) -> bool {
	if ai.done {
		return false
	}

	// meet the correct date range
	dateConditionMet := true
	date := model.current_step_date
	if d.is_valid(ai.abs_start_date) && d.is_valid(ai.abs_end_date) {
		dateConditionMet = d.ge(date, ai.abs_start_date) && d.le(date, ai.abs_end_date)
		if d.gt(date, ai.abs_end_date) {
			ai.done = true
		}
	} else if d.is_valid(ai.abs_start_date) {
		dateConditionMet = d.ge(date, ai.abs_start_date)
	} else if d.is_valid(ai.abs_end_date) {
		dateConditionMet = d.le(date, ai.abs_end_date)
		if d.gt(date, ai.abs_end_date) {
			ai.done = true
		}
	}
	if !dateConditionMet {
		return ai.done
	}

	// meet the correct crop stage
	cropConditionMet := dateConditionMet
	cg := model.current_crop_module
	if cg != nil && ai.irrigate_crop {
		ai.crop_planted = true
		stage := cg.developmental_stage
		if ai.start_stage > -1 && ai.end_stage > -1 {
			cropConditionMet = stage >= ai.start_stage && stage <= ai.end_stage
			if stage > ai.end_stage {
				ai.done = true
			}
		} else if ai.start_stage > -1 {
			cropConditionMet = stage >= ai.start_stage
		} else if ai.end_stage > -1 {
			cropConditionMet = stage <= ai.end_stage
			if stage > ai.end_stage {
				ai.done = true
			}
		}
	} else if ai.crop_planted {
		ai.done = true
		ai.crop_planted = false
		cropConditionMet = false
	} else {
		cropConditionMet = false
	}

	return ai.done || cropConditionMet
}

// C++: bool workstep::reinit(AutomaticIrrigationData*, Workstep*, Date, bool, bool)
automatic_irrigation_reinit :: proc(
	ai: ^Automatic_Irrigation_Data,
	ws: ^Workstep,
	date: d.Date,
	addYear: bool = false,
	forceInitYear: bool = false,
) -> bool {
	_ = workstep_reinit_common(ws, date, addYear)
	ws.date = d.Date{}

	startAddedYear: bool
	ai.abs_start_date, startAddedYear = make_init_abs_date(
		ai.params.start_date,
		date,
		addYear,
		forceInitYear,
	)
	stopAddedYear: bool
	ai.abs_end_date, stopAddedYear = make_init_abs_date(
		ai.params.end_date,
		date,
		addYear,
		forceInitYear,
	)
	_ = stopAddedYear
	ai.done = false

	return startAddedYear
}

// C++: Workstep monica::makeAutomaticIrrigationWorkstep(json11::Json)
make_automatic_irrigation_workstep :: proc(
	j: jx.Value,
	allocator := context.allocator,
) -> ^Workstep {
	ws := new_workstep(allocator)
	ws.data = Automatic_Irrigation_Data {
		start_stage = -1,
		end_stage   = -1,
		params      = p.make_automatic_irrigation_parameters(),
	}
	res := workstep_merge_common(ws, j)
	tl.append_errors(&res, automatic_irrigation_merge(&ws.data.(Automatic_Irrigation_Data), j))
	ws.errors = res
	return ws
}

// ---------------------------------------------------------------------------
// SetValue (src/worksteps/set-value.{h,cpp})
// ---------------------------------------------------------------------------

// C++: Json workstep::(anonymous getValue lambda)(const MonicaModel*)
//
// The dispatcher for Set_Value_Data.get_value (workstep.odin) - see that
// struct's own doc comment for why this is data + a dispatcher instead of a
// closure.
set_value_get_value :: proc(s: ^Set_Value_Data, model: ^core.Monica_Model) -> jx.Value {
	switch s.get_value.kind {
	case .CONSTANT:
		return s.value
	case .OID_LOOKUP:
		if v, ok := mio.oid_get_value(model, s.get_value.source_oid); ok {
			return v
		}
	case .CALC_EXPR:
		return mio.calc_expr_eval(s.get_value.calc, model)
	case .NONE:
	}
	return jx.Value{}
}

// C++: Errors workstep::merge(SetValueData*, json11::Json)
set_value_merge :: proc(
	s: ^Set_Value_Data,
	j: jx.Value,
	allocator := context.allocator,
) -> tl.Errors {
	res: tl.Errors

	oids := mio.parse_output_ids([]jx.Value{jx.get(j, "var")}, allocator = allocator)
	if len(oids) > 0 {
		s.oid = oids[0]
	} else {
		return res
	}

	s.value = jx.get(j, "value")
	if jx.is_array(s.value) {
		jva := jx.array_items(s.value)
		if len(jva) > 0 {
			if len(jva) == 4 && jx.is_string(jva[0]) && jx.string_value_of(jva[0]) == "=" {
				// C++: buildPrimitiveCalcExpression(J11Array(jva.begin()+1, jva.end()))
				// - the ["=", a, op, b] arithmetic form.
				//
				// NOTE(c++-quirk, NOT reproduced): the C++ assigns
				// `s->getValue = [f](auto mm){ return f(*mm); }` here
				// unconditionally, wrapping `f` even when
				// buildPrimitiveCalcExpression returned an EMPTY std::function
				// (two literal operands, an operand that names nothing, a
				// malformed array). The outer lambda is non-empty, so apply()'s
				// `if (!s->getValue) return true;` guard passes and the call
				// then throws std::bad_function_call. That is a crash, not a
				// value this port can match, so an unbuildable expression is
				// left as .NONE - the same "no write happens" outcome the guard
				// was plainly meant to produce.
				e := mio.build_primitive_calc_expression(jva[1:], allocator)
				if e.set {
					s.get_value = Set_Value_Get_Value {
						kind = .CALC_EXPR,
						calc = e,
					}
				}
			} else if jx.is_string(jva[0]) {
				oids2 := mio.parse_output_ids([]jx.Value{s.value}, allocator = allocator)
				if len(oids2) > 0 && mio.oid_has_getter(oids2[0]) {
					s.get_value = Set_Value_Get_Value {
						kind       = .OID_LOOKUP,
						source_oid = oids2[0],
					}
				}
			} else {
				// NOT in the C++: a literal per-layer value list,
				// "value": [0.05, 0.04, 0.03].
				//
				// The C++ treats *every* non-"=" array as an output-id spec,
				// so an array of numbers falls through parseOutputIds finding
				// nothing, get_value stays empty, and apply() silently writes
				// nothing at all. set_complex_values has handled an array
				// value since it was ported - it just had no way to receive
				// one, since only an oid returning a layer range could produce
				// it. Additive and unambiguous: an oid spec array always leads
				// with a string ("Mois"), a value array never does.
				s.get_value = Set_Value_Get_Value {
					kind = .CONSTANT,
				}
			}
		}
	} else {
		s.get_value = Set_Value_Get_Value {
			kind = .CONSTANT,
		}
	}

	return res
}

// C++: bool workstep::apply(SetValueData*, Workstep*, MonicaModel*)
set_value_apply :: proc(s: ^Set_Value_Data, ws: ^Workstep, model: ^core.Monica_Model) -> bool {
	workstep_apply_common(ws, model)

	if s.get_value.kind == .NONE {
		return true
	}

	// C++: `setfs.find(s->oid.id)`, widened by the path tier - oid_set_value
	// writes through a compiled path when the oid has one and falls back to a
	// registered setf otherwise. Before this, only the two ids with a setf
	// (Stage, Mois) could be set at all; now any field reachable by a path is
	// settable, which is what makes SetValue useful for the state pokes
	// experiment designs actually need.
	//
	// The value is fetched only when there is somewhere to put it, matching
	// the C++'s ordering (`getValue` runs inside the `if`, not before it) -
	// it matters because reading an oid can allocate.
	if mio.oid_has_setter(s.oid) {
		v := set_value_get_value(s, model)
		mio.oid_set_value(model, s.oid, v)
	}

	model.current_events["SetValue"] = true

	return true
}

// C++: Workstep monica::makeSetValueWorkstep(json11::Json)
//
// The (Date, OId, Json) non-JSON overload is not ported - grepped src/ and
// confirmed it has no caller anywhere in this codebase, the same "only the
// JSON-taking overload is ever called" finding already made for every other
// make*Workstep (see this file's own header comment).
make_set_value_workstep :: proc(j: jx.Value, allocator := context.allocator) -> ^Workstep {
	ws := new_workstep(allocator)
	ws.data = Set_Value_Data{}
	res := workstep_merge_common(ws, j)
	tl.append_errors(&res, set_value_merge(&ws.data.(Set_Value_Data), j, allocator))
	ws.errors = res
	return ws
}
