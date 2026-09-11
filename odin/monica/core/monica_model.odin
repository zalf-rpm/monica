// Phase 6 checkpoint 1: src/core/monica-model.h's MonicaModel struct and the
// standalone (non-step()) half of src/core/monica-model.cpp -
// makeMonicaModel, CO2ForDate (both overloads), groundwaterDepthForDate,
// clearEvents, the daily-sum accumulators, dailyReset, the fertiliser/
// irrigation/tillage apply wrappers, and setOtherCropHeightAndLAIt.
//
// Dropped, per plan-odin.md's "Explicitly dropped" table: serialize/
// deserialize (Cap'n Proto), the Intercropping field and step()'s
// Intercropping-async branch (Cap'n Proto RPC). Also dropped: both
// monicamodel::seedCrop overloads - the mas::schema::model::monica::
// CropSpec::Reader overload's only caller is
// src/run/daily-monica-fbp-component-main.cpp (an FBP entry point, itself
// dropped), and the CropParameters* overload is already commented out
// (dead code) in the C++ source itself. Sowing/AutomaticSowing worksteps
// (checkpoint 3) inline the equivalent crop-construction logic directly,
// exactly like the real seedCrop's caller-less sibling would have.
//
// harvestCurrentCrop/incorporateCurrentCrop are added at the end of this
// file (their prerequisites - the cropmodule:: yield/N-content getters and
// HarvestData::Spec/OptCarbonManagementData - now exist: the getters landed
// in crop_module.odin, and Spec/OptCarbonManagementData are hoisted into
// this package below as Harvest_Spec/Harvest_Opt_Carbon_Management_Data,
// the same circular-dependency break used for Cutting_Value in
// crop_module.odin - the Harvest workstep lives in the `run` package, which
// imports `core`, so `core` cannot import it back).
// step()/generalStep()/cropStep() are checkpoint 6.
//
// Every monicamodel:: function here is prefixed monica_model_ to avoid
// colliding with the same-named soilcolumn:: function it wraps (e.g.
// soilcolumn::applyIrrigation vs monicamodel::applyIrrigation both flatten
// into this one `core` package) - the same collision-avoidance convention
// already used for soil_temperature_step/soil_moisture_step/crop_module_step.
package core

import clim "../../support/climate"
import d "../../support/date"
import p "../params"
import "../soil"
import libc "core:c/libc"
import "core:slice"

// C++: struct monica::MonicaModel
Monica_Model :: struct {
	sitePs:                         p.Site_Parameters,
	envPs:                          p.Environment_Parameters,
	cropPs:                         p.Crop_Module_Parameters,
	simPs:                          p.Simulation_Parameters,
	groundwaterInformation:         p.Measured_Groundwater_Table_Information,
	soilColumn:                     Soil_Column, // main soil data structure
	soilTemperature:                Soil_Temperature, // temperature code
	soilMoisture:                   Soil_Moisture, // moisture code
	soilOrganic:                    Soil_Organic, // organic code
	soilTransport:                  Soil_Transport, // transport code
	currentCropModule:              ^Crop_Module, // crop code for possibly planted crop; nil if none

	// store applied fertiliser during one production process
	sumFertiliser:                  f64, // mineral N
	sumOrgFertiliser:               f64, // organic N

	// stores the daily sum of applied fertiliser
	dailySumFertiliser:             f64, // mineral N
	dailySumOrgFertiliser:          f64, // organic N
	dailySumOrganicFertilizerDM:    f64,
	sumOrganicFertilizerDM:         f64,
	humusBalanceCarryOver:          f64,
	dailySumIrrigationWater:        f64,
	optCarbonExportedResidues:      f64,
	optCarbonReturnedResidues:      f64,
	currentStepDate:                d.Date,
	climateData:                    [dynamic]map[clim.ACD]f64,
	currentEvents:                  map[string]bool,
	previousDaysEvents:             map[string]bool,
	clearCropUponNextDay:           bool,
	p_daysWithCrop:                 int,
	p_accuNStress:                  f64,
	p_accuWaterStress:              f64,
	p_accuHeatStress:               f64,
	p_accuOxygenStress:             f64,
	vw_AtmosphericCO2Concentration: f64,
	vw_AtmosphericO3Concentration:  f64,
	vs_GroundwaterDepth:            f64,
	cultivationMethodCount:         int,
}

// C++: kj::Own<MonicaModel> monica::makeMonicaModel(const CentralParameterProvider&)
//
// Heap-allocated via `new` and never moved thereafter (risk register: every
// submodule below, and every CropModule created later by a Sowing/Transplant
// workstep, holds a pointer straight into these fields - &model.soilColumn
// etc - so the model's address, and every field's address inside it, must
// stay stable for the whole run).
make_monica_model :: proc(
	cpp: ^p.Central_Parameter_Provider,
	allocator := context.allocator,
) -> ^Monica_Model {
	model := new(Monica_Model, allocator)

	model.sitePs = cpp.siteParameters
	model.envPs = cpp.userEnvironmentParameters
	model.cropPs = cpp.userCropParameters
	model.simPs = cpp.simulationParameters
	model.groundwaterInformation = cpp.groundwaterInformation

	model.soilColumn = make_soil_column(
		model.simPs.p_LayerThickness,
		cpp.userSoilOrganicParameters.ps_MaxMineralisationDepth,
		model.sitePs.vs_SoilParameters[:],
		allocator,
	)
	model.soilTemperature = make_soil_temperature(
		&model.soilColumn,
		cpp.userSoilTemperatureParameters,
		model.envPs.p_timeStep,
	)
	model.soilMoisture = make_soil_moisture(
		&model.soilColumn,
		&model.sitePs,
		cpp.userSoilMoistureParameters,
		&model.envPs,
		&model.cropPs,
		model.simPs.p_LayerThickness,
		allocator,
	)
	model.soilOrganic = make_soil_organic(&model.soilColumn, cpp.userSoilOrganicParameters)
	model.soilTransport = make_soil_transport(
		cpp.userSoilTransportParameters,
		&model.soilColumn,
		&model.sitePs,
		&model.envPs,
		&model.cropPs,
	)

	// C++ in-class initialisers: std::set<std::string> currentEvents/
	// previousDaysEvents default-construct empty, never nullptr. Odin's zero
	// map value is nil, which panics on write (m[k] = v) - initialise both to
	// real, empty maps so monica_model_fire_event_cb (below) and clearEvents/
	// dailyReset's map reassignment can write to them from day one.
	model.currentEvents = make(map[string]bool, 0, allocator)
	model.previousDaysEvents = make(map[string]bool, 0, allocator)

	// Every CropModule created by a Sowing/Transplant/AutomaticSowing
	// workstep's apply() needs real fireEvent/addOrganicMatter/
	// getSnowDepthAndCalcTempUnderSnow callbacks wired to *this* model - see
	// monica_model_fire_event_cb's doc comment below for why this is a
	// package-level global rather than a captured closure.
	g_current_model = model

	return model
}

// This is the real resolution of the "Odin proc type has no capture" gap
// phase 5 checkpoint 2 deliberately deferred (crop_module.odin's own
// comment on Crop_Module.fireEvent/addOrganicMatter/
// getSnowDepthAndCalcTempUnderSnow): the C++ lambdas
// (sowing.cpp/automatic-sowing.cpp/transplant.cpp) capture `model` by value,
// but Odin's `proc(_: string)` etc. field types cannot close over anything.
// monica-run is a single-simulation-per-process CLI tool - there is only
// ever one live MonicaModel at a time in this port's scope (matching the
// real C++'s actual usage, not just a convenient shortcut) - so a
// package-level "the current model" pointer, set once by make_monica_model
// and never reassigned (the model is heap-allocated once and never moved,
// same invariant the risk register already requires), is behaviourally
// identical to the C++ capture for every caller in this port.
g_current_model: ^Monica_Model

// C++: [model](string event) { model->currentEvents.insert(std::move(event)); }
monica_model_fire_event_cb :: proc(event: string) {
	g_current_model.currentEvents[event] = true
}

// C++: [model](const std::map<size_t,double>& layer2amount, double nconc) {
//        soilorganic::addOrganicMatter(model->soilOrganic.get(),
//          model->currentCropModule->residueParams, layer2amount, nconc); }
monica_model_add_organic_matter_cb :: proc(layer2amount: map[int]f64, nconc: f64) {
	soil_organic_add_organic_matter(
		&g_current_model.soilOrganic,
		&g_current_model.currentCropModule.residue_params.base,
		layer2amount,
		nconc,
	)
}

// C++: [model](double avgAirTemp) {
//        return soilmoisture::getSnowDepthAndCalcTemperatureUnderSnow(
//          model->soilMoisture.get(), avgAirTemp); }
monica_model_get_snow_depth_cb :: proc(avgAirTemp: f64) -> (f64, f64) {
	return get_snow_depth_and_calc_temperature_under_snow(
		&g_current_model.soilMoisture,
		avgAirTemp,
	)
}

// C++: void monica::monicamodel::addDailySumFertiliser(MonicaModel*, double)
monica_model_add_daily_sum_fertiliser :: proc(model: ^Monica_Model, amount: f64) {
	model.dailySumFertiliser += amount
	model.sumFertiliser += amount
}

// C++: void monica::monicamodel::addDailySumOrganicFertilizerDM(MonicaModel*, double)
monica_model_add_daily_sum_organic_fertilizer_dm :: proc(model: ^Monica_Model, amountDM: f64) {
	model.dailySumOrganicFertilizerDM += amountDM
	model.sumOrganicFertilizerDM += amountDM
}

// C++: void monica::monicamodel::addDailySumIrrigationWater(MonicaModel*, double)
monica_model_add_daily_sum_irrigation_water :: proc(model: ^Monica_Model, amount: f64) {
	model.dailySumIrrigationWater += amount
}

// C++: void monica::monicamodel::resetFertiliserCounter(MonicaModel*)
monica_model_reset_fertiliser_counter :: proc(model: ^Monica_Model) {
	model.sumFertiliser = 0
	model.sumOrgFertiliser = 0
	model.sumOrganicFertilizerDM = 0
}

// C++: void monica::monicamodel::addDailySumOrgFertiliser(MonicaModel*,
//        double, const OrganicMatterParameters&)
monica_model_add_daily_sum_org_fertiliser :: proc(
	model: ^Monica_Model,
	amountFM: f64,
	params: ^p.Organic_Matter_Parameters,
) {
	AOM_fast_factor :=
		soil.PO_AOM_TO_C * params.vo_PartAOM_to_AOM_Fast / params.vo_CN_Ratio_AOM_Fast
	AOM_slow_factor :=
		soil.PO_AOM_TO_C * params.vo_PartAOM_to_AOM_Slow / params.vo_CN_Ratio_AOM_Slow
	SOM_Factor :=
		(1 - (params.vo_PartAOM_to_AOM_Fast + params.vo_PartAOM_to_AOM_Slow)) *
		soil.PO_AOM_TO_C /
		model.soilColumn.layers[0].soil_cn_ratio // TODO ask CN for correctness

	conversion :=
		AOM_fast_factor +
		AOM_slow_factor +
		SOM_Factor +
		params.vo_AOM_NH4Content +
		params.vo_AOM_NO3Content

	model.dailySumOrgFertiliser += amountFM * params.vo_AOM_DryMatterContent * conversion
	model.sumOrgFertiliser += amountFM * params.vo_AOM_DryMatterContent * conversion
}

// C++: void monica::monicamodel::applyMineralFertiliser(MonicaModel*,
//        MineralFertilizerParameters, double)
monica_model_apply_mineral_fertiliser :: proc(
	model: ^Monica_Model,
	partition: p.Mineral_Fertilizer_Parameters,
	amount: f64,
) {
	if !model.simPs.p_UseNMinMineralFertilisingMethod {
		apply_mineral_fertiliser(&model.soilColumn, partition, amount)
		monica_model_add_daily_sum_fertiliser(model, amount)
	}
}

// C++: void monica::monicamodel::applyOrganicFertiliser(MonicaModel*, const
//        OrganicMatterParameters&, double, bool, int)
monica_model_apply_organic_fertiliser :: proc(
	model: ^Monica_Model,
	params: ^p.Organic_Matter_Parameters,
	amountFM: f64,
	incorporation: bool,
	incorporateIntoLayerIndex: int = 0,
) {
	model.soilOrganic.incorporation = incorporation
	soil_organic_add_organic_matter_amount(
		&model.soilOrganic,
		params,
		amountFM,
		params.vo_NConcentration,
		incorporateIntoLayerIndex,
	)
	monica_model_add_daily_sum_org_fertiliser(model, amountFM, params)
	monica_model_add_daily_sum_organic_fertilizer_dm(
		model,
		amountFM * params.vo_AOM_DryMatterContent,
	)
}

// C++: double monica::monicamodel::applyMineralFertiliserViaNMinMethod(
//        MonicaModel*, MineralFertilizerParameters, NMinCropParameters)
monica_model_apply_mineral_fertiliser_via_n_min_method :: proc(
	model: ^Monica_Model,
	partition: p.Mineral_Fertilizer_Parameters,
	cps: p.NMin_Crop_Parameters,
) -> f64 {
	ups := &model.simPs.p_NMinUserParams
	return apply_mineral_fertiliser_via_n_min_method(
		&model.soilColumn,
		partition,
		cps.samplingDepth,
		cps.nTarget,
		cps.nTarget30,
		ups.max,
		ups.min,
		ups.delayInDays,
	)
}

// C++: void monica::monicamodel::dailyReset(MonicaModel*)
monica_model_daily_reset :: proc(model: ^Monica_Model, allocator := context.allocator) {
	model.dailySumIrrigationWater = 0.0
	model.dailySumFertiliser = 0.0
	model.dailySumOrgFertiliser = 0.0
	model.dailySumOrganicFertilizerDM = 0.0
	model.optCarbonExportedResidues = 0.0
	model.optCarbonReturnedResidues = 0.0
	monica_model_clear_events(model, allocator)

	if model.clearCropUponNextDay {
		soil_transport_remove_crop(&model.soilTransport)
		remove_crop(&model.soilColumn)
		model.soilMoisture.crop_module = nil
		model.soilOrganic.crop_module = nil
		model.currentCropModule = nil

		model.clearCropUponNextDay = false
	}
}

// C++: void monica::monicamodel::applyIrrigation(MonicaModel*, double,
//        double, double)
//
// sulfateConcentration is accepted-but-unused in the C++ too (parameter name
// commented out there); dropped from the signature rather than kept as an
// unused parameter, since Odin has no anonymous-parameter equivalent to
// silence it.
monica_model_apply_irrigation :: proc(
	model: ^Monica_Model,
	amount: f64,
	nitrateConcentration: f64 = 0,
) {
	// if the production process has still some defined manual irrigation dates
	if !model.simPs.p_UseAutomaticIrrigation {
		apply_irrigation(&model.soilColumn, amount, nitrateConcentration)
		model.soilOrganic.irrigation_amount += amount
		monica_model_add_daily_sum_irrigation_water(model, amount)
	}
}

// C++: void monica::monicamodel::applyTillage(MonicaModel*, double)
monica_model_apply_tillage :: proc(model: ^Monica_Model, depth: f64) {
	apply_tillage(&model.soilColumn, depth)
}

// C++: double monica::monicamodel::CO2ForDate(double year, double julianDay,
//        bool isLeapYear, RCP)
co2_for_date :: proc(year, julianDay: f64, isLeapYear: bool, rcp: p.RCP = .RCP85) -> f64 {
	decimalDate := year + julianDay / (isLeapYear ? 366.0 : 365.0)

	co2 := 0.0
	switch rcp {
	case .RCP19:
		co2 =
			309.61 +
			110.21 / (1 + libc.exp(-(0.0819 * (decimalDate - 1995.41)))) +
			(2.5 * libc.sin((decimalDate - 0.5) / 0.1592))
	case .RCP26:
		co2 =
			306.23 +
			158.52 / (1 + libc.exp(-(0.0601 * (decimalDate - 2005.77)))) +
			(2.5 * libc.sin((decimalDate - 0.5) / 0.1592))
	case .RCP34:
		co2 =
			302.24 +
			185.07 / (1 + libc.exp(-(0.0512 * (decimalDate - 2010.26)))) +
			(2.5 * libc.sin((decimalDate - 0.5) / 0.1592))
	case .RCP45:
		co2 =
			292.83 +
			348.08 / (1 + libc.exp(-(0.0349 * (decimalDate - 2036.46)))) +
			(2.5 * libc.sin((decimalDate - 0.5) / 0.1592))
	case .RCP60:
		co2 =
			287.56 +
			474.21 / (1 + libc.exp(-(0.0301 * (decimalDate - 2052.37)))) +
			(2.5 * libc.sin((decimalDate - 0.5) / 0.1592))
	case .RCP70:
		co2 =
			277.25 +
			1338.47 / (1 + libc.exp(-(0.0237 * (decimalDate - 2110.06)))) +
			(2.5 * libc.sin((decimalDate - 0.5) / 0.1592))
	case .RCP85:
		fallthrough
	case:
		co2 =
			294.27 +
			2361.06 / (1 + libc.exp(-(0.0287 * (decimalDate - 2120.53)))) +
			(2.5 * libc.sin((decimalDate - 0.5) / 0.1592))
	}

	return co2
}

// C++: double monica::monicamodel::CO2ForDate(const Date&, RCP)
co2_for_date_from_date :: proc(dt: d.Date, rcp: p.RCP = .RCP85) -> f64 {
	return co2_for_date(f64(d.year(dt)), f64(d.julian_day(dt)), d.is_leap_year(dt), rcp)
}

// C++: double monica::monicamodel::groundwaterDepthForDate(double, double,
//        int, double, bool)
groundwater_depth_for_date :: proc(
	maxGroundwaterDepth: f64,
	minGroundwaterDepth: f64,
	minGroundwaterDepthMonth: int,
	julianDay: f64,
	isLeapYear: bool,
) -> f64 {
	days := 365.0
	if isLeapYear {
		days = 366.0
	}

	meanGroundwaterDepth := (maxGroundwaterDepth + minGroundwaterDepth) / 2.0
	groundwaterAmplitude := (maxGroundwaterDepth - minGroundwaterDepth) / 2.0

	sinus := libc.sin(
		((julianDay / days * 360.0) - 90.0 - ((f64(minGroundwaterDepthMonth) * 30.0) - 15.0)) *
		3.14159265358979 /
		180.0,
	)

	groundwaterDepth := meanGroundwaterDepth + (sinus * groundwaterAmplitude)

	if groundwaterDepth < 0.0 {
		groundwaterDepth = 20.0
	}

	return groundwaterDepth
}

// C++: void monica::monicamodel::clearEvents(MonicaModel*)
//
// Odin maps are reference-like (assignment aliases the same backing store),
// so - unlike C++'s std::set copy-assignment, a genuine deep copy -
// previousDaysEvents is handed the old currentEvents map as-is and
// currentEvents is repointed at a brand-new, empty map, rather than clearing
// currentEvents in place after the alias (which would also empty
// previousDaysEvents, since they'd share the same backing store).
monica_model_clear_events :: proc(model: ^Monica_Model, allocator := context.allocator) {
	model.previousDaysEvents = model.currentEvents
	model.currentEvents = make(map[string]bool, 0, allocator)
}

// C++: void monica::monicamodel::setOtherCropHeightAndLAIt(MonicaModel*,
//        double, double)
monica_model_set_other_crop_height_and_lait :: proc(model: ^Monica_Model, cropHeight, lait: f64) {
	if model.currentCropModule != nil {
		set_other_crop_height_and_lait(model.currentCropModule, cropHeight, lait)
	}
}

// ---------------------------------------------------------------------------
// harvestCurrentCrop / incorporateCurrentCrop, and the HarvestData::Spec /
// OptCarbonManagementData payload types they need, hoisted from the Harvest
// workstep (src/worksteps/harvest.h) for the same reason Cutting_Value was
// hoisted into crop_module.odin: `core` cannot import the `run` package that
// will own the Harvest workstep itself.
// ---------------------------------------------------------------------------

// C++: enum HarvestData::CropUsage { greenManure = 0, biomassProduction }
Harvest_Crop_Usage :: enum {
	Green_Manure,
	Biomass_Production,
}

// C++: struct HarvestData::Spec::Value
Harvest_Spec_Value :: struct {
	exportPercentage: f64, // C++ in-class default: 100.0
	incorporate:      bool, // C++ in-class default: true
}

// C++: struct HarvestData::Spec
Harvest_Spec :: struct {
	organ2specVal: map[int]Harvest_Spec_Value,
}

// C++: struct HarvestData::OptCarbonManagementData
Harvest_Opt_Carbon_Management_Data :: struct {
	optCarbonConservation:     bool,
	cropImpactOnHumusBalance:  f64,
	maxResidueRecoverFraction: f64, // C++ in-class default: 1
	cropUsage:                 Harvest_Crop_Usage, // C++ in-class default: biomassProduction
	residueHeq:                f64,
	organicFertilizerHeq:      f64,
}

// C++ in-class defaults for OptCarbonManagementData
make_harvest_opt_carbon_management_data :: proc() -> Harvest_Opt_Carbon_Management_Data {
	return Harvest_Opt_Carbon_Management_Data {
		maxResidueRecoverFraction = 1,
		cropUsage = .Biomass_Production,
	}
}

// C++: void monica::monicamodel::harvestCurrentCrop(MonicaModel*, bool,
//        const HarvestData::Spec&, HarvestData::OptCarbonManagementData, int)
//
// Iterates spec.organ2specVal in ascending key order to match C++
// std::map's sorted iteration - the accumulators below (cropYield,
// primaryCropYield, sumOrganResidueBiomassAsOverlay,
// sumOrganResidueBiomassToIncorporate) are order-sensitive floating-point
// sums, the same "sort keys first" precaution phase 5 checkpoint 6's oracle
// regression and this phase's applyCutting both already needed.
monica_model_harvest_current_crop :: proc(
	model: ^Monica_Model,
	exported: bool,
	spec: Harvest_Spec,
	optCarbMgmtData: Harvest_Opt_Carbon_Management_Data = {
		maxResidueRecoverFraction = 1,
		cropUsage = .Biomass_Production,
	},
	incorporateIntoLayerIndex: int = 0,
	allocator := context.allocator,
) {
	if model.currentCropModule != nil {
		cm := model.currentCropModule

		// prepare to add root and crop residues to soilorganic (AOMs)
		// dead root biomass has already been added daily, so just living root
		// biomass is left
		rootBiomass := cm.organ_green_biomass[0]
		add_and_distribute_root_biomass_in_soil(cm, rootBiomass, allocator)

		if exported && len(spec.organ2specVal) == 0 {
			if optCarbMgmtData.optCarbonConservation {
				residueBiomass := get_residue_biomass(cm, false, -1)
				// kg ha-1, secondary yield is ignored with this approach
				cropContribToHumus := optCarbMgmtData.cropImpactOnHumusBalance
				appliedOrganicFertilizerDryMatter := model.sumOrganicFertilizerDM // kg ha-1
				intermediateHumusBalance :=
					model.humusBalanceCarryOver +
					cropContribToHumus +
					appliedOrganicFertilizerDryMatter /
						1000.0 *
						optCarbMgmtData.organicFertilizerHeq -
					model.sitePs.vs_SoilSpecificHumusBalanceCorrection
				potentialHumusFromResidues := residueBiomass / 1000.0 * optCarbMgmtData.residueHeq

				fractionToBeLeftOnField := 0.0
				if potentialHumusFromResidues > 0 {
					fractionToBeLeftOnField =
						-intermediateHumusBalance / potentialHumusFromResidues
					if fractionToBeLeftOnField > 1 {
						fractionToBeLeftOnField = 1.0
					} else if fractionToBeLeftOnField < 0 {
						fractionToBeLeftOnField = 0.0
					}
				}

				if optCarbMgmtData.cropUsage == .Green_Manure {
					// if the crop is used as green manure, all the residues are
					// incorporated regardless the humus balance
					fractionToBeLeftOnField = 1.0
				}

				// calculate theoretical residue removal
				model.optCarbonReturnedResidues = residueBiomass * fractionToBeLeftOnField
				model.optCarbonExportedResidues = residueBiomass - model.optCarbonReturnedResidues

				// adjust it if technically unfeasible
				maxExportedResidues := residueBiomass * optCarbMgmtData.maxResidueRecoverFraction
				if model.optCarbonExportedResidues > maxExportedResidues {
					model.optCarbonExportedResidues = maxExportedResidues
					model.optCarbonReturnedResidues =
						residueBiomass - model.optCarbonExportedResidues
				}

				soil_organic_add_organic_matter_amount(
					&model.soilOrganic,
					&cm.residue_params.base,
					model.optCarbonReturnedResidues,
					get_residues_n_concentration(cm, -1),
					incorporateIntoLayerIndex,
					allocator,
				)

				model.humusBalanceCarryOver =
					intermediateHumusBalance +
					model.optCarbonReturnedResidues / 1000.0 * optCarbMgmtData.residueHeq
			} else { 	// old default behavior
				residueBiomass := get_residue_biomass(cm, model.simPs.p_UseSecondaryYields, -1)
				residueNConcentration := get_residues_n_concentration(cm, -1)
				soil_organic_add_organic_matter_amount(
					&model.soilOrganic,
					&cm.residue_params.base,
					residueBiomass,
					residueNConcentration,
					incorporateIntoLayerIndex,
					allocator,
				)
			}
		} else if len(spec.organ2specVal) != 0 { 	// harvest with a more detailed specification
			cropYield := 0.0
			primaryCropYield := 0.0
			sumOrganResidueBiomassAsOverlay := 0.0
			sumOrganResidueBiomassToIncorporate := 0.0
			organIdsForPrimaryYield := organ_ids_for_primary_yield(cm, allocator)

			keys := make([dynamic]int, 0, len(spec.organ2specVal), allocator)
			for k in spec.organ2specVal {
				append(&keys, k)
			}
			slice.sort(keys[:])

			for organId in keys {
				specVal := spec.organ2specVal[organId]
				// ignore root, is probably an error, when the user specified the
				// root organ (0) as something to harvest
				if organId == 0 {
					continue
				}
				organBiomass := cm.organ_biomass[organId]
				organYield := organBiomass * specVal.exportPercentage / 100.0
				cropYield += organYield
				if organIdsForPrimaryYield[organId + 1] {
					primaryCropYield += organYield
				}
				if specVal.incorporate {
					sumOrganResidueBiomassToIncorporate += organBiomass - organYield
				} else {
					sumOrganResidueBiomassAsOverlay += organBiomass - organYield
				}
			}
			totalResidueBiomass := get_residue_biomass(cm, false, cropYield)
			totalResidueBiomassToIncorporate :=
				totalResidueBiomass - sumOrganResidueBiomassAsOverlay
			residuesNConcentration := get_residues_n_concentration(cm, primaryCropYield)
			soil_organic_add_organic_matter_amount(
				&model.soilOrganic,
				&cm.residue_params.base,
				totalResidueBiomassToIncorporate,
				residuesNConcentration,
				incorporateIntoLayerIndex,
				allocator,
			)
		} else {
			// prepare to add the total plant to soilorganic (AOMs)
			abovegroundBiomass := cm.aboveground_biomass
			abovegroundBiomassNConcentration := cm.n_concentration_aboveground_biomass
			soil_organic_add_organic_matter_amount(
				&model.soilOrganic,
				&cm.residue_params.base,
				abovegroundBiomass,
				abovegroundBiomassNConcentration,
				incorporateIntoLayerIndex,
				allocator,
			)
		}
	}

	model.clearCropUponNextDay = true
}

// C++: void monica::monicamodel::incorporateCurrentCrop(MonicaModel*)
monica_model_incorporate_current_crop :: proc(
	model: ^Monica_Model,
	allocator := context.allocator,
) {
	if model.currentCropModule != nil {
		cm := model.currentCropModule

		// prepare to add root and crop residues to soilorganic (AOMs)
		total_biomass := cm.total_biomass
		totalNContent :=
			get_aboveground_biomass_n_content(cm) +
			cm.n_concentration_root * cm.organ_biomass[0]
		totalNConcentration := totalNContent / total_biomass

		soil_organic_add_organic_matter_amount(
			&model.soilOrganic,
			&cm.residue_params.base,
			total_biomass,
			totalNConcentration,
			0,
			allocator,
		)
	}

	model.clearCropUponNextDay = true
}

// ---------------------------------------------------------------------------
// Phase 6 checkpoint 6: step() / generalStep() / cropStep() - the daily
// orchestration entry points, closing out src/core/monica-model.cpp.
//
// step() drops the Intercropping-async branch (Cap'n Proto RPC, per
// plan-odin.md's "Explicitly dropped" table) - only the crop-step dispatch
// survives.
// ---------------------------------------------------------------------------

// C++: void monica::monicamodel::step(MonicaModel*)
monica_model_step :: proc(model: ^Monica_Model, allocator := context.allocator) {
	if model.currentCropModule != nil && !model.clearCropUponNextDay {
		monica_model_crop_step(model, allocator)
	}

	monica_model_general_step(model, allocator)
}

// C++: void monica::monicamodel::generalStep(MonicaModel*)
//
// soil_temperature_step takes soil_coverage/snow_depth/temp_under_snow as
// explicit parameters instead of reading them through a `monica` back-
// pointer the way the C++ SoilTemperature does (phase 4's design decision,
// documented in soil_temperature.odin) - this is the first production call
// site that actually supplies live values instead of a bare-soil 0.0/nil.
monica_model_general_step :: proc(model: ^Monica_Model, allocator := context.allocator) {
	date := model.currentStepDate
	julday := d.julian_day(date)
	leapYear := d.is_leap_year(date)

	dailyClimate := model.climateData[len(model.climateData) - 1]
	tmin := dailyClimate[.tmin]
	tavg := dailyClimate[.tavg]
	tmax := dailyClimate[.tmax]
	precip := dailyClimate[.precip]
	wind := dailyClimate[.wind]
	globrad := dailyClimate[.globrad]

	// test if data for relhumid are available; if not, value is set to -1.0
	relhumid := -1.0
	if v, ok := dailyClimate[.relhumid]; ok {
		relhumid = v
	}

	// test if simulated gw or measured values should be used
	gw_available, gw_depth := p.get_groundwater_information(
		&model.groundwaterInformation,
		date,
		allocator,
	)
	if gw_available {
		model.vs_GroundwaterDepth = max(0.0, gw_depth)
	} else {
		model.vs_GroundwaterDepth = groundwater_depth_for_date(
			model.envPs.p_MaxGroundwaterDepth,
			model.envPs.p_MinGroundwaterDepth,
			model.envPs.p_MinGroundwaterDepthMonth,
			f64(julday),
			leapYear,
		)
	}

	// first try to get CO2 concentration from climate data
	if co2v, ok := dailyClimate[.co2]; ok {
		model.vw_AtmosphericCO2Concentration = co2v
	} else if co2s, ok2 := model.envPs.p_AtmosphericCO2s[d.year(date)]; ok2 {
		// try to get yearly values from UserEnvironmentParameters
		model.vw_AtmosphericCO2Concentration = co2s
		// potentially use MONICA algorithm to calculate CO2 concentration
	} else if int(model.envPs.p_AtmosphericCO2) <= 0 {
		model.vw_AtmosphericCO2Concentration = co2_for_date_from_date(date, model.envPs.rcp)
		// if everything fails value in UserEnvironmentParameters for the whole simulation
	} else {
		model.vw_AtmosphericCO2Concentration = model.envPs.p_AtmosphericCO2
	}

	delete_aom_pool(&model.soilColumn)

	possibleDelayedFertilizerAmount := apply_possible_delayed_fertilizer(&model.soilColumn)
	monica_model_add_daily_sum_fertiliser(model, possibleDelayedFertilizerAmount)
	possibleTopDressingAmount := apply_possible_top_dressing(&model.soilColumn)
	monica_model_add_daily_sum_fertiliser(model, possibleTopDressingAmount)

	if model.currentCropModule != nil &&
	   model.simPs.p_UseNMinMineralFertilisingMethod &&
	   model.currentCropModule.crop_params.cultivarParams.winterCrop &&
	   int(julday) == model.simPs.p_JulianDayAutomaticFertilising {
		clear_top_dressing_params(&model.soilColumn)
		sps := model.currentCropModule.crop_params.speciesParams
		fertilizerAmount := monica_model_apply_mineral_fertiliser_via_n_min_method(
			model,
			model.simPs.p_NMinFertiliserPartition,
			p.NMin_Crop_Parameters {
				samplingDepth = sps.pc_SamplingDepth,
				nTarget = sps.pc_TargetNSamplingDepth,
				nTarget30 = sps.pc_TargetN30,
			},
		)
		monica_model_add_daily_sum_fertiliser(model, fertilizerAmount)
	}

	soil_coverage := model.currentCropModule != nil ? model.currentCropModule.soil_coverage : 0.0
	soil_temperature_step(
		&model.soilTemperature,
		tmin,
		tmax,
		globrad,
		soil_coverage,
		model.soilMoisture.snow_component.snow_depth,
		model.soilMoisture.frost_component.temperature_under_snow,
	)

	// first try to get ReferenceEvapotranspiration from climate data
	et0 := -1.0
	if v, ok := dailyClimate[.et0]; ok {
		et0 = v
	}

	soil_moisture_step(
		&model.soilMoisture,
		model.vs_GroundwaterDepth,
		precip,
		tmax,
		tmin,
		(relhumid / 100.0),
		tavg,
		wind,
		model.envPs.p_WindSpeedHeight,
		globrad,
		int(julday),
		et0,
		model.simPs.dualKcMethod,
	)

	soil_organic_step(&model.soilOrganic, tavg, precip, wind)
	soil_transport_step(&model.soilTransport)
}

// C++: void monica::monicamodel::cropStep(MonicaModel*)
//
// The commented-out VOC-emissions block at the end of the C++ function
// (never compiled there either) is not ported.
monica_model_crop_step :: proc(model: ^Monica_Model, allocator := context.allocator) {
	date := model.currentStepDate
	dailyClimate := model.climateData[len(model.climateData) - 1]

	// do nothing if there is no crop
	if model.currentCropModule == nil {
		return
	}

	model.p_daysWithCrop += 1

	// C++ genuine dead store: computed (`unsigned int julday =
	// date.julianDay();`) but never read anywhere in the rest of cropStep -
	// kept for fidelity, matching the same "_ = x, not removed" precedent
	// phase 5 checkpoints 5-6 already established for other C++ dead stores.
	julday := d.julian_day(date)
	_ = julday

	tavg := dailyClimate[.tavg]
	tmax := dailyClimate[.tmax]
	tmin := dailyClimate[.tmin]
	globrad := dailyClimate[.globrad]

	// first try to get CO2 concentration from climate data
	if o3v, ok := dailyClimate[.o3]; ok {
		model.vw_AtmosphericO3Concentration = o3v
	} else if o3s, ok2 := model.envPs.p_AtmosphericO3s[d.year(date)]; ok2 {
		// try to get yearly values from UserEnvironmentParameters
		model.vw_AtmosphericO3Concentration = o3s
		// if everything fails value in UserEnvironmentParameters for the whole simulation
	} else {
		model.vw_AtmosphericO3Concentration = model.envPs.p_AtmosphericO3
	}

	// test if data for sunhours are available; if not, value is set to -1.0
	sunhours := -1.0
	if v, ok := dailyClimate[.sunhours]; ok {
		sunhours = v
	}

	// test if data for relhumid are available; if not, value is set to -1.0
	relhumid := -1.0
	if v, ok := dailyClimate[.relhumid]; ok {
		relhumid = v
	}

	wind := -1.0
	if v, ok := dailyClimate[.wind]; ok {
		wind = v
	}

	precip := dailyClimate[.precip]

	// check if reference evapotranspiration was provided via climate files
	et0 := -1.0
	if v, ok := dailyClimate[.et0]; ok {
		et0 = v
	}

	vw_WindSpeedHeight := model.envPs.p_WindSpeedHeight

	crop_module_step(
		model.currentCropModule,
		tavg,
		tmax,
		tmin,
		globrad,
		sunhours,
		date,
		(relhumid / 100.0),
		wind,
		vw_WindSpeedHeight,
		model.vw_AtmosphericCO2Concentration,
		model.vw_AtmosphericO3Concentration,
		precip,
		et0,
		allocator,
	)

	if model.simPs.p_UseAutomaticIrrigation &&
	   (!d.is_valid(model.simPs.p_AutoIrrigationParams.startDate) ||
			   d.le(model.simPs.p_AutoIrrigationParams.startDate, date)) &&
	   (!d.is_valid(model.simPs.p_AutoIrrigationParams.endDate) ||
			   d.le(date, model.simPs.p_AutoIrrigationParams.endDate)) {
		irrigationTriggered, irrigationAmount := apply_irrigation_via_trigger(
			&model.soilColumn,
			&model.simPs.p_AutoIrrigationParams,
		)
		if irrigationTriggered {
			model.soilOrganic.irrigation_amount += irrigationAmount
			monica_model_add_daily_sum_irrigation_water(model, irrigationAmount)
		}
	}

	model.p_accuNStress += model.currentCropModule.crop_n_redux
	model.p_accuWaterStress += model.currentCropModule.transpiration_deficit
	model.p_accuHeatStress += model.currentCropModule.crop_heat_redux
	model.p_accuOxygenStress += model.currentCropModule.oxygen_deficit
}
