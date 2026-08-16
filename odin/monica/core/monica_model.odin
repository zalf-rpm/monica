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

import "core:slice"
import libc "core:c/libc"
import p "../params"
import d "../../support/date"
import clim "../../support/climate"
import "../soil"

// C++: struct monica::MonicaModel
Monica_Model :: struct {
	sitePs:                 p.Site_Parameters,
	envPs:                  p.Environment_Parameters,
	cropPs:                 p.Crop_Module_Parameters,
	simPs:                  p.Simulation_Parameters,
	groundwaterInformation: p.Measured_Groundwater_Table_Information,

	soilColumn:      Soil_Column,      // main soil data structure
	soilTemperature: Soil_Temperature, // temperature code
	soilMoisture:    Soil_Moisture,    // moisture code
	soilOrganic:     Soil_Organic,     // organic code
	soilTransport:   Soil_Transport,   // transport code
	currentCropModule: ^Crop_Module,   // crop code for possibly planted crop; nil if none

	// store applied fertiliser during one production process
	sumFertiliser:    f64, // mineral N
	sumOrgFertiliser: f64, // organic N

	// stores the daily sum of applied fertiliser
	dailySumFertiliser:    f64, // mineral N
	dailySumOrgFertiliser: f64, // organic N

	dailySumOrganicFertilizerDM: f64,
	sumOrganicFertilizerDM:      f64,

	humusBalanceCarryOver: f64,

	dailySumIrrigationWater: f64,

	optCarbonExportedResidues: f64,
	optCarbonReturnedResidues: f64,

	currentStepDate:    d.Date,
	climateData:        [dynamic]map[clim.ACD]f64,
	currentEvents:      map[string]bool,
	previousDaysEvents: map[string]bool,

	clearCropUponNextDay: bool,

	p_daysWithCrop:      int,
	p_accuNStress:       f64,
	p_accuWaterStress:   f64,
	p_accuHeatStress:    f64,
	p_accuOxygenStress:  f64,

	vw_AtmosphericCO2Concentration: f64,
	vw_AtmosphericO3Concentration:  f64,
	vs_GroundwaterDepth:            f64,

	cultivationMethodCount: int,
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

	return model
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
			model.soilColumn.layers[0].vs_Soil_CN_Ratio // TODO ask CN for correctness

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
	monica_model_add_daily_sum_organic_fertilizer_dm(model, amountFM * params.vo_AOM_DryMatterContent)
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
		model.soilMoisture.cropModule = nil
		model.soilOrganic.cropModule = nil
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
		model.soilOrganic.irrigationAmount += amount
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
	optCarbonConservation:      bool,
	cropImpactOnHumusBalance:   f64,
	maxResidueRecoverFraction:  f64, // C++ in-class default: 1
	cropUsage:                  Harvest_Crop_Usage, // C++ in-class default: biomassProduction
	residueHeq:                 f64,
	organicFertilizerHeq:       f64,
}

// C++ in-class defaults for OptCarbonManagementData
make_harvest_opt_carbon_management_data :: proc() -> Harvest_Opt_Carbon_Management_Data {
	return Harvest_Opt_Carbon_Management_Data {
		maxResidueRecoverFraction = 1,
		cropUsage                 = .Biomass_Production,
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
		rootBiomass := cm.vc_OrganGreenBiomass[0]
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
					appliedOrganicFertilizerDryMatter / 1000.0 * optCarbMgmtData.organicFertilizerHeq -
					model.sitePs.vs_SoilSpecificHumusBalanceCorrection
				potentialHumusFromResidues := residueBiomass / 1000.0 * optCarbMgmtData.residueHeq

				fractionToBeLeftOnField := 0.0
				if potentialHumusFromResidues > 0 {
					fractionToBeLeftOnField = -intermediateHumusBalance / potentialHumusFromResidues
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
					model.optCarbonReturnedResidues = residueBiomass - model.optCarbonExportedResidues
				}

				soil_organic_add_organic_matter_amount(
					&model.soilOrganic,
					&cm.residueParams.base,
					model.optCarbonReturnedResidues,
					get_residues_n_concentration(cm, -1),
					incorporateIntoLayerIndex,
					allocator,
				)

				model.humusBalanceCarryOver =
					intermediateHumusBalance +
					model.optCarbonReturnedResidues / 1000.0 * optCarbMgmtData.residueHeq
			} else { // old default behavior
				residueBiomass := get_residue_biomass(cm, model.simPs.p_UseSecondaryYields, -1)
				residueNConcentration := get_residues_n_concentration(cm, -1)
				soil_organic_add_organic_matter_amount(
					&model.soilOrganic,
					&cm.residueParams.base,
					residueBiomass,
					residueNConcentration,
					incorporateIntoLayerIndex,
					allocator,
				)
			}
		} else if len(spec.organ2specVal) != 0 { // harvest with a more detailed specification
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
				organBiomass := cm.vc_OrganBiomass[organId]
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
			totalResidueBiomassToIncorporate := totalResidueBiomass - sumOrganResidueBiomassAsOverlay
			residuesNConcentration := get_residues_n_concentration(cm, primaryCropYield)
			soil_organic_add_organic_matter_amount(
				&model.soilOrganic,
				&cm.residueParams.base,
				totalResidueBiomassToIncorporate,
				residuesNConcentration,
				incorporateIntoLayerIndex,
				allocator,
			)
		} else {
			// prepare to add the total plant to soilorganic (AOMs)
			abovegroundBiomass := cm.vc_AbovegroundBiomass
			abovegroundBiomassNConcentration := cm.vc_NConcentrationAbovegroundBiomass
			soil_organic_add_organic_matter_amount(
				&model.soilOrganic,
				&cm.residueParams.base,
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
monica_model_incorporate_current_crop :: proc(model: ^Monica_Model, allocator := context.allocator) {
	if model.currentCropModule != nil {
		cm := model.currentCropModule

		// prepare to add root and crop residues to soilorganic (AOMs)
		total_biomass := cm.vc_TotalBiomass
		totalNContent :=
			get_aboveground_biomass_n_content(cm) + cm.vc_NConcentrationRoot * cm.vc_OrganBiomass[0]
		totalNConcentration := totalNContent / total_biomass

		soil_organic_add_organic_matter_amount(
			&model.soilOrganic,
			&cm.residueParams.base,
			total_biomass,
			totalNConcentration,
			0,
			allocator,
		)
	}

	model.clearCropUponNextDay = true
}
