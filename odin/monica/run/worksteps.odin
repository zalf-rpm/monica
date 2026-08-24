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
// s.cropParams is default-constructed via make_crop_parameters() before the
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
	s.cropParams = p.make_crop_parameters()

	jx.set_iso_date_value(&s.sowingDate, j, "seedDate")
	jx.set_iso_date_value(&s.harvestDate, j, "harvestDate")

	jc := jx.get(j, "crop")
	if jx.is_object(jc) {
		if jx.is_bool(jx.get(jc, "is-perennial-crop")) {
			s.isPerennialCrop = jx.bool_value_of(jx.get(jc, "is-perennial-crop"))
		}

		if jx.has_object_shape(jc, "cropParams") {
			jcps := jx.get(jc, "cropParams")
			if jx.has_object_shape(jcps, "species") && jx.has_object_shape(jcps, "cultivar") {
				_ = p.crop_parameters_merge(&s.cropParams, jcps)
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

			if val, ok := s.isPerennialCrop.?; ok {
				s.cropParams.cultivarParams.pc_Perennial = val
			} else {
				s.isPerennialCrop = s.cropParams.cultivarParams.pc_Perennial
			}

			s.isValid = true
		} else {
			tl.append_error(
				&res,
				strings.concatenate(
					{"Couldn't find 'cropParams' key in JSON object:\n", jx.dump(jc, allocator)},
					allocator,
				),
			)
			s.isValid = false
		}

		if val, ok := s.isPerennialCrop.?; ok && val {
			if jx.has_object_shape(jc, "perennialCropParams") {
				jcps := jx.get(jc, "perennialCropParams")
				if jx.has_object_shape(jcps, "species") && jx.has_object_shape(jcps, "cultivar") {
					s.separatePerennialCropParams = new(p.Crop_Parameters, allocator)
					s.separatePerennialCropParams^ = p.make_crop_parameters()
					_ = p.crop_parameters_merge(s.separatePerennialCropParams, jcps)
				}
			}
		}

		if jx.has_object_shape(jc, "residueParams") {
			_ = p.crop_residue_parameters_merge(&s.residueParams, jx.get(jc, "residueParams"))
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
			s.isValid = false
		}
	}

	jx.set_int_value(&s.plantDensity, j, "PlantDensity")
	if s.plantDensity > 0 {
		s.cropParams.speciesParams.pc_PlantDensity = s.plantDensity
	}
	// FAO-56 Dual Kc: optional initial Kcb at sowing (default 0.15 = bare soil)
	jx.set_double_value(&s.initialKcb, j, "initialKcb")

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

	model.p_daysWithCrop = 0
	model.p_accuNStress = 0.0
	model.p_accuWaterStress = 0.0
	model.p_accuHeatStress = 0.0
	model.p_accuOxygenStress = 0.0

	if s.isValid {
		model.cultivationMethodCount += 1

		model.currentCropModule = nil
		cm := new(core.Crop_Module, allocator)
		cm^ = core.make_crop_module(
			&model.soilColumn,
			&s.cropParams,
			&s.residueParams,
			&model.sitePs,
			&model.cropPs,
			&model.simPs,
			core.monica_model_fire_event_cb,
			core.monica_model_add_organic_matter_cb,
			core.monica_model_get_snow_depth_cb,
			nil,
			allocator,
		)
		model.currentCropModule = cm

		if s.separatePerennialCropParams != nil {
			model.currentCropModule.perennialCropParams = new(p.Crop_Parameters, allocator)
			model.currentCropModule.perennialCropParams^ = core.clone_crop_parameters(
				s.separatePerennialCropParams^,
				allocator,
			)
		}

		core.soil_transport_put_crop(&model.soilTransport, model.currentCropModule)
		core.put_crop(&model.soilColumn, model.currentCropModule)
		model.soilMoisture.crop_module = model.currentCropModule
		model.soilOrganic.crop_module = model.currentCropModule

		if model.simPs.p_UseNMinMineralFertilisingMethod &&
		   !model.currentCropModule.cropParams.cultivarParams.winterCrop {
			core.clear_top_dressing_params(&model.soilColumn)
			fert_amount := core.monica_model_apply_mineral_fertiliser_via_n_min_method(
				model,
				model.simPs.p_NMinFertiliserPartition,
				p.NMin_Crop_Parameters {
					samplingDepth = s.cropParams.speciesParams.pc_SamplingDepth,
					nTarget = s.cropParams.speciesParams.pc_TargetNSamplingDepth,
					nTarget30 = s.cropParams.speciesParams.pc_TargetN30,
				},
			)
			core.monica_model_add_daily_sum_fertiliser(model, fert_amount)
		}
	}

	// FAO-56 Dual Kc: push initial Kcb into the freshly created crop module
	if model.simPs.dualKcMethod && model.currentCropModule != nil {
		model.currentCropModule.vc_Kcb_ini = s.initialKcb
	}
	model.currentEvents["Sowing"] = true

	return true
}

// C++: Workstep monica::makeSowingWorkstep(json11::Json)
make_sowing_workstep :: proc(j: jx.Value, allocator := context.allocator) -> ^Workstep {
	ws := new_workstep(allocator)
	ws.data = Sowing_Data {
		plantDensity = -1,
		initialKcb   = 0.15,
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
		t.initialStage = jx.int_value_of(init_stage_v)
	}
	jx.set_double_value(&t.initialGDD, j, "initialTemperatureSum")
	jx.set_double_value(&t.initRootMass, j, "initialRootBiomass")
	jx.set_double_value(&t.initLeafMass, j, "initialLeafBiomass")
	jx.set_double_value(&t.initShootMass, j, "initialShootBiomass")
	jx.set_double_value(&t.initLAI, j, "initialLAI")
	jx.set_int_value(&t.postTransplantDelay, j, "postTransplantDelay")
	// FAO-56 Dual Kc: optional initial Kcb at transplanting - re-parses the
	// same key sowing_merge (above) already parsed into this same promoted
	// field; harmless (idempotent), kept to mirror the C++'s two parse calls
	// (see Transplant_Data's doc comment for why there's only one field here)
	jx.set_double_value(&t.initialKcb, j, "initialKcb")

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

	cropModule := model.currentCropModule
	if cropModule == nil {
		return false
	}

	core.force_transplant_state(
		cropModule,
		t.initialGDD,
		t.initLAI,
		t.initialStage,
		t.initRootMass,
		t.initLeafMass,
		t.initShootMass,
		t.postTransplantDelay,
	)

	if model.simPs.dualKcMethod {
		cropModule.vc_Kcb_ini = t.initialKcb
	}
	model.currentEvents["Transplant"] = true

	return true
}

// C++: Workstep monica::makeTransplantWorkstep(json11::Json)
make_transplant_workstep :: proc(j: jx.Value, allocator := context.allocator) -> ^Workstep {
	ws := new_workstep(allocator)
	ws.data = Transplant_Data {
		sowing = {plantDensity = -1, initialKcb = 0.15},
		initialStage = 2,
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

	jx.set_iso_date_value(&as.earliestDate, j, "earliest-date")
	jx.set_iso_date_value(&as.latestDate, j, "latest-date")
	jx.set_double_value(&as.minTempThreshold, j, "min-temp")
	jx.set_int_value(&as.daysInTempWindow, j, "days-in-temp-window")
	jx.set_double_value(&as.minPercentASW, j, "min-%-asw")
	jx.set_double_value(&as.maxPercentASW, j, "max-%-asw")
	jx.set_double_value(&as.max3dayPrecipSum, j, "max-3d-precip")
	jx.set_double_value(&as.maxCurrentDayPrecipSum, j, "max-curr-day-precip")
	jx.set_double_value(&as.tempSumAboveBaseTemp, j, "temp-sum-above-base-temp")
	jx.set_double_value(&as.baseTemp, j, "base-temp")

	avgSoilTemp := jx.get(j, "avg-soil-temp")
	if jx.is_object(avgSoilTemp) {
		jx.set_double_value(&as.soilDepthForAveraging, avgSoilTemp, "depth")
		jx.set_int_value(&as.daysInSoilTempWindow, avgSoilTemp, "days")
		jx.set_double_value(&as.sowingIfAboveAvgSoilTemp, avgSoilTemp, "Tavg")
		as.checkForSoilTemperature =
			as.soilDepthForAveraging > 0 &&
			as.daysInSoilTempWindow > 0 &&
			as.sowingIfAboveAvgSoilTemp > 0
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
	currentDate := model.currentStepDate
	as.sowingDate = currentDate

	sowing_apply(&as.sowing, ws, model, allocator)
	model.currentEvents["AutomaticSowing"] = true
	as.cropSeeded = true
	as.inSowingRange = false

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
	size := core.get_layer_number_for_depth(&model.soilColumn, as.soilDepthForAveraging) + 1
	for ; i < size; i += 1 {
		avgSoilTemp += model.soilTemperature.soilColumn.layers[i].vs_SoilTemperature
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
	if !as.checkForSoilTemperature {
		return nil, false
	}
	as.getAvgSoilTemps = getDailyValues
	g_current_automatic_sowing = as
	return automatic_sowing_avg_soil_temp_fn, true
}

// C++: bool workstep::condition(AutomaticSowingData*, MonicaModel*)
automatic_sowing_condition :: proc(as: ^Automatic_Sowing_Data, model: ^core.Monica_Model) -> bool {
	if as.cropSeeded {
		return false
	}

	currentDate := model.currentStepDate

	if !as.inSowingRange && d.lt(currentDate, as.absEarliestDate) {
		return false
	} else {
		as.inSowingRange = true
	}

	if as.inSowingRange && d.ge(currentDate, as.absLatestDate) {
		return true
	}

	// check soil temperature if requested
	if as.checkForSoilTemperature {
		if !is_soil_temperature_ok(
			as.getAvgSoilTemps(),
			as.daysInSoilTempWindow,
			as.sowingIfAboveAvgSoilTemp,
		) {
			return false
		}
	}

	cd := model.climateData
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
	if as.cropParams.cultivarParams.winterCrop {
		avgTavg := avg(cd, .tavg, as.daysInTempWindow)
		Tok = avgTavg <= as.minTempThreshold
	} else {
		avgTmin := avg(cd, .tmin, as.daysInTempWindow)
		avgTminOk := avgTmin >= as.minTempThreshold
		TminOk := currentCd[.tmin] >= as.minTempThreshold
		Tok = avgTminOk && TminOk
	}

	if !Tok {
		return false
	}

	// check soil moisture
	if !is_soil_moisture_ok(model, as.minPercentASW, as.maxPercentASW) {
		return false
	}

	// check precipitation
	if !is_precipitation_ok(cd, as.max3dayPrecipSum, as.maxCurrentDayPrecipSum) {
		return false
	}

	// check temperature sum
	baseTemp := as.baseTemp
	tempSum := 0.0
	for m in cd {
		if v, ok := m[.tavg]; ok {
			tempSum += max(0.0, v - baseTemp)
		}
	}
	if tempSum < as.tempSumAboveBaseTemp {
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

	as.cropSeeded = false
	as.inSowingRange = false
	ws.date = d.Date{}
	as.sowingDate = d.Date{}

	// init first the latest date, if the latest date stays in current year, so
	// has to stay the earliest date (thus force current year) if there is a
	// forced current (init) year, this will force both dates to this year
	addedYear1: bool
	as.absLatestDate, addedYear1 = make_init_abs_date(as.latestDate, date, addYear, forceInitYear)
	addedYear2: bool
	as.absEarliestDate, addedYear2 = make_init_abs_date(
		as.earliestDate,
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
		sowing = {plantDensity = -1, initialKcb = 0.15},
		maxPercentASW = 100,
		soilDepthForAveraging = 0.30,
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

	jx.set_int_value(&h.incorporateIntoLayerNo, j, "incorporateIntoLayerNo")
	h.incorporateIntoLayerNo = max(1, h.incorporateIntoLayerNo)
	jx.set_bool_value(&h.exported, j, "exported")
	jx.set_bool_value(&h.optCarbMgmtData.optCarbonConservation, j, "opt-carbon-conservation")
	jx.set_double_value(
		&h.optCarbMgmtData.cropImpactOnHumusBalance,
		j,
		"crop-impact-on-humus-balance",
	)
	cu := jx.string_value(j, "crop-usage")
	if cu == "green-manure" {
		h.optCarbMgmtData.cropUsage = .Green_Manure
	} else {
		h.optCarbMgmtData.cropUsage = .Biomass_Production
	}
	jx.set_double_value(&h.optCarbMgmtData.residueHeq, j, "residue-heq")
	jx.set_double_value(&h.optCarbMgmtData.organicFertilizerHeq, j, "organic-fertilizer-heq")
	jx.set_double_value(
		&h.optCarbMgmtData.maxResidueRecoverFraction,
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

	if model.currentCropModule != nil {
		core.monica_model_harvest_current_crop(
			model,
			h.exported,
			h.spec,
			h.optCarbMgmtData,
			h.incorporateIntoLayerNo - 1,
			allocator,
		)
		model.currentEvents["Harvest"] = true
	}

	return true
}

// C++: Workstep monica::makeHarvestWorkstep(json11::Json)
make_harvest_workstep :: proc(j: jx.Value, allocator := context.allocator) -> ^Workstep {
	ws := new_workstep(allocator)
	ws.data = Harvest_Data {
		exported = true,
		incorporateIntoLayerNo = 1,
		optCarbMgmtData = {maxResidueRecoverFraction = 1, cropUsage = .Biomass_Production},
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

	jx.set_iso_date_value(&ah.latestDate, j, "latest-date")
	jx.set_double_value(&ah.minPercentASW, j, "min-%-asw")
	jx.set_double_value(&ah.maxPercentASW, j, "max-%-asw")
	jx.set_double_value(&ah.max3dayPrecipSum, j, "max-3d-precip-sum")
	jx.set_double_value(&ah.maxCurrentDayPrecipSum, j, "max-curr-day-precip")
	jx.set_string_value(&ah.harvestTime, j, "harvest-time")

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

	model.currentEvents["AutomaticHarvest"] = true
	ah.cropHarvested = true

	return true
}

// C++: bool workstep::condition(AutomaticHarvestData*, MonicaModel*)
automatic_harvest_condition :: proc(
	ah: ^Automatic_Harvest_Data,
	model: ^core.Monica_Model,
) -> bool {
	conditionMet := false

	cg := model.currentCropModule
	// got a crop and not yet harvested
	if cg != nil && !ah.cropHarvested {
		conditionMet =
			d.ge(model.currentStepDate, ah.absLatestDate) ||
			(ah.harvestTime == "maturity" &&
					core.maturity_reached(cg) &&
					is_soil_moisture_ok(model, ah.minPercentASW, ah.maxPercentASW) &&// harvest after or at latest date
					is_precipitation_ok(
						model.climateData, // has maturity been reached// check soil moisture
						ah.max3dayPrecipSum,
						ah.maxCurrentDayPrecipSum,
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

	ah.cropHarvested = false
	ws.date = d.Date{}

	addedYear: bool
	ah.absLatestDate, addedYear = make_init_abs_date(ah.latestDate, date, addYear, forceInitYear)

	return addedYear
}

// C++: Workstep monica::makeAutomaticHarvestWorkstep(json11::Json)
make_automatic_harvest_workstep :: proc(j: jx.Value, allocator := context.allocator) -> ^Workstep {
	ws := new_workstep(allocator)
	ws.data = Automatic_Harvest_Data {
		harvestTime            = "maturity",
		maxPercentASW          = 999,
		max3dayPrecipSum       = 9999,
		maxCurrentDayPrecipSum = 9999,
	}
	{
		ahd := &ws.data.(Automatic_Harvest_Data)
		ahd.harvest.exported = true
		ahd.harvest.incorporateIntoLayerNo = 1
		ahd.harvest.optCarbMgmtData = {
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

		if c.organId2cuttingSpec == nil {
			c.organId2cuttingSpec = make(map[int]core.Cutting_Value, 0, allocator)
		}
		c.organId2cuttingSpec[oid] = val
		if c.organId2exportFraction == nil {
			c.organId2exportFraction = make(map[int]f64, 0, allocator)
		}
		c.organId2exportFraction[oid] = export_ ? 1 : 0
	}

	for k, v in jx.object_items(jx.get(j, "export")) {
		oid := organ_id_from_name(k, &errors)
		if oid == -1 {
			continue
		}
		if c.organId2exportFraction == nil {
			c.organId2exportFraction = make(map[int]f64, 0, allocator)
		}
		c.organId2exportFraction[oid] = f64(jx.int_value_d(v, 0)) / 100.0
	}

	jx.set_double_value(
		&c.cutMaxAssimilationRateFraction,
		j,
		"cut-max-assimilation-rate",
		.PERCENT,
	)

	return errors
}

// C++: bool workstep::apply(CuttingData*, Workstep*, MonicaModel*)
cutting_apply :: proc(c: ^Cutting_Data, ws: ^Workstep, model: ^core.Monica_Model) -> bool {
	workstep_apply_common(ws, model)

	assert(model.currentCropModule != nil)
	core.apply_cutting(
		model.currentCropModule,
		c.organId2cuttingSpec,
		c.organId2exportFraction,
		c.cutMaxAssimilationRateFraction,
	)
	model.currentEvents["Cutting"] = true

	return true
}

// C++: Workstep monica::makeCuttingWorkstep(json11::Json)
make_cutting_workstep :: proc(j: jx.Value, allocator := context.allocator) -> ^Workstep {
	ws := new_workstep(allocator)
	ws.data = Cutting_Data {
		cutMaxAssimilationRateFraction = 1.0,
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
	model.currentEvents["MineralFertilization"] = true
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
// initialDate).
n_demand_fertilization_merge :: proc(
	nd: ^N_Demand_Fertilization_Data,
	ws: ^Workstep,
	j: jx.Value,
) -> tl.Errors {
	res: tl.Errors
	nd.initialDate = ws.date
	jx.set_double_value(&nd.Ndemand, j, "N-demand")
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

	rd := model.currentCropModule.vc_RootingDepth_m
	appliedAmount := core.apply_mineral_fertiliser_via_n_demand(
		&model.soilColumn,
		nd.partition,
		rd < nd.depth ? rd : nd.depth,
		nd.Ndemand,
	)
	model.dailySumFertiliser += appliedAmount
	nd.appliedFertilizer = true
	// record date of application until next reinit
	ws.date = model.currentStepDate
	model.currentEvents["NDemandFertilization"] = true

	return true
}

// C++: bool workstep::condition(NDemandFertilizationData*, Workstep*, MonicaModel*)
n_demand_fertilization_condition :: proc(
	nd: ^N_Demand_Fertilization_Data,
	ws: ^Workstep,
	model: ^core.Monica_Model,
) -> bool {
	conditionMet := false

	cg := model.currentCropModule
	if cg != nil && !nd.appliedFertilizer {
		currStage := cg.vc_DevelopmentalStage + 1
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
	ws.date = nd.initialDate
	_ = workstep_reinit_common(ws, date, addYear, forceInitYear)
	nd.appliedFertilizer = false

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
	jx.set_int_value(&of.incorporateIntoLayerNo, j, "incorporateIntoLayerNo")
	of.incorporateIntoLayerNo = max(1, of.incorporateIntoLayerNo)
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
		of.incorporateIntoLayerNo - 1,
	)
	model.currentEvents["OrganicFertilization"] = true
	return true
}

// C++: Workstep monica::makeOrganicFertilizationWorkstep(json11::Json)
make_organic_fertilization_workstep :: proc(
	j: jx.Value,
	allocator := context.allocator,
) -> ^Workstep {
	ws := new_workstep(allocator)
	ws.data = Organic_Fertilization_Data {
		incorporateIntoLayerNo = 1,
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
	model.currentEvents["Tillage"] = true
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

	core.monica_model_apply_irrigation(model, i.amount, i.params.nitrateConcentration)
	// FAO-56 Dual Kc: push event-level fw and isDrip into SoilMoisture for
	// today's ET calculation. LIMITATION: Auto-irrigation uses sim.json params
	// or defaults (fw=1.0, isDrip=false).
	if model.simPs.dualKcMethod {
		model.soilMoisture.irrig_fw_event = i.params.fw
		model.soilMoisture.irrig_is_drip_event = i.params.isDripIrrigation
	}
	model.currentEvents["Irrigation"] = true

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

	jx.set_int_value(&ai.startStage, j, "startStage")
	// NOTE(c++-quirk): 1-based stages (user side) -> 0-based stages (model
	// side). The C++ does `startStage = max(0, startStage--)` - the
	// post-decrement's side effect (decrementing startStage by one) is
	// immediately overwritten by the assignment right after it, so the
	// decrement is a complete no-op. Net effect is exactly `startStage =
	// max(0, startStage)`; reproduced without the pointless decrement.
	if ai.startStage > -1 {
		ai.startStage = max(0, ai.startStage)
	}

	jx.set_int_value(&ai.endStage, j, "endStage")
	// same clobbered-decrement non-effect as startStage above
	if ai.endStage > -1 {
		ai.endStage = min(7, ai.endStage)
	}

	jx.set_bool_value(&ai.irrigateCrop, j, "irrigateCrop")
	if ai.startStage > -1 || ai.endStage > -1 {
		ai.irrigateCrop = true
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
		&model.soilColumn,
		&ai.params,
	)
	if irrigationTriggered {
		model.currentEvents["AutomaticIrrigation"] = true
		model.soilOrganic.irrigation_amount += irrigationAmount
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
	date := model.currentStepDate
	if d.is_valid(ai.absStartDate) && d.is_valid(ai.absEndDate) {
		dateConditionMet = d.ge(date, ai.absStartDate) && d.le(date, ai.absEndDate)
		if d.gt(date, ai.absEndDate) {
			ai.done = true
		}
	} else if d.is_valid(ai.absStartDate) {
		dateConditionMet = d.ge(date, ai.absStartDate)
	} else if d.is_valid(ai.absEndDate) {
		dateConditionMet = d.le(date, ai.absEndDate)
		if d.gt(date, ai.absEndDate) {
			ai.done = true
		}
	}
	if !dateConditionMet {
		return ai.done
	}

	// meet the correct crop stage
	cropConditionMet := dateConditionMet
	cg := model.currentCropModule
	if cg != nil && ai.irrigateCrop {
		ai.cropPlanted = true
		stage := cg.vc_DevelopmentalStage
		if ai.startStage > -1 && ai.endStage > -1 {
			cropConditionMet = stage >= ai.startStage && stage <= ai.endStage
			if stage > ai.endStage {
				ai.done = true
			}
		} else if ai.startStage > -1 {
			cropConditionMet = stage >= ai.startStage
		} else if ai.endStage > -1 {
			cropConditionMet = stage <= ai.endStage
			if stage > ai.endStage {
				ai.done = true
			}
		}
	} else if ai.cropPlanted {
		ai.done = true
		ai.cropPlanted = false
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
	ai.absStartDate, startAddedYear = make_init_abs_date(
		ai.params.startDate,
		date,
		addYear,
		forceInitYear,
	)
	stopAddedYear: bool
	ai.absEndDate, stopAddedYear = make_init_abs_date(
		ai.params.endDate,
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
		startStage = -1,
		endStage   = -1,
		params     = p.make_automatic_irrigation_parameters(),
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
// The dispatcher for Set_Value_Data.getValue (workstep.odin) - see that
// struct's own doc comment for why this is data + a dispatcher instead of a
// closure.
set_value_get_value :: proc(s: ^Set_Value_Data, model: ^core.Monica_Model) -> jx.Value {
	switch s.getValue.kind {
	case .CONSTANT:
		return s.value
	case .OID_LOOKUP:
		if v, ok := mio.oid_get_value(model, s.getValue.sourceOid); ok {
			return v
		}
	case .CALC_EXPR:
		return mio.calc_expr_eval(s.getValue.calc, model)
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
					s.getValue = Set_Value_Get_Value {
						kind = .CALC_EXPR,
						calc = e,
					}
				}
			} else if jx.is_string(jva[0]) {
				oids2 := mio.parse_output_ids([]jx.Value{s.value}, allocator = allocator)
				if len(oids2) > 0 && mio.oid_has_getter(oids2[0]) {
					s.getValue = Set_Value_Get_Value {
						kind      = .OID_LOOKUP,
						sourceOid = oids2[0],
					}
				}
			} else {
				// NOT in the C++: a literal per-layer value list,
				// "value": [0.05, 0.04, 0.03].
				//
				// The C++ treats *every* non-"=" array as an output-id spec,
				// so an array of numbers falls through parseOutputIds finding
				// nothing, getValue stays empty, and apply() silently writes
				// nothing at all. set_complex_values has handled an array
				// value since it was ported - it just had no way to receive
				// one, since only an oid returning a layer range could produce
				// it. Additive and unambiguous: an oid spec array always leads
				// with a string ("Mois"), a value array never does.
				s.getValue = Set_Value_Get_Value {
					kind = .CONSTANT,
				}
			}
		}
	} else {
		s.getValue = Set_Value_Get_Value {
			kind = .CONSTANT,
		}
	}

	return res
}

// C++: bool workstep::apply(SetValueData*, Workstep*, MonicaModel*)
set_value_apply :: proc(s: ^Set_Value_Data, ws: ^Workstep, model: ^core.Monica_Model) -> bool {
	workstep_apply_common(ws, model)

	if s.getValue.kind == .NONE {
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

	model.currentEvents["SetValue"] = true

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
