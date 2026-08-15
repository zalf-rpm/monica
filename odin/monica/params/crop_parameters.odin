// Tranche 3b of the src/core/monica-parameters.{h,cpp} port: SpeciesParameters,
// CultivarParameters, CropParameters.
package params

import "core:strings"
import jx "../../support/jsonx"
import tl "../../support/tools"

// ---------------------------------------------------------------------------
// SpeciesParameters
// ---------------------------------------------------------------------------

// C++: struct monica::SpeciesParameters
Species_Parameters :: struct {
	pc_SpeciesId:                        string,
	pc_CarboxylationPathway:             int, // old TEMPTYP
	pc_DefaultRadiationUseEfficiency:    f64,
	pc_PartBiologicalNFixation:          f64,
	pc_InitialKcFactor:                  f64, // old Kcini
	pc_LuxuryNCoeff:                     f64,
	pc_MaxCropDiameter:                  f64,
	pc_StageAtMaxHeight:                 f64,
	pc_StageAtMaxDiameter:               f64,
	pc_MinimumNConcentration:            f64,
	pc_MinimumTemperatureForAssimilation: f64, // old MINTMP
	pc_OptimumTemperatureForAssimilation: f64,
	pc_MaximumTemperatureForAssimilation: f64,
	pc_NConcentrationAbovegroundBiomass: f64, // initial value of old GEHOB
	pc_NConcentrationB0:                 f64,
	pc_NConcentrationPN:                 f64,
	pc_NConcentrationRoot:               f64, // initial value to WUGEH
	pc_DevelopmentAccelerationByNitrogenStress: int,
	pc_FieldConditionModifier:           f64,
	pc_AssimilateReallocation:           f64,

	pc_BaseTemperature:             [dynamic]f64, // old BAS
	pc_OrganMaintenanceRespiration: [dynamic]f64, // old MAIRT
	pc_OrganGrowthRespiration:      [dynamic]f64, // old MAIRT
	pc_StageMaxRootNConcentration:  [dynamic]f64, // old WGMAX
	pc_InitialOrganBiomass:         [dynamic]f64,
	pc_CriticalOxygenContent:       [dynamic]f64, // old LUKRIT
	pc_StageMobilFromStorageCoeff:  [dynamic]f64,

	pc_AbovegroundOrgan: [dynamic]bool, // old KOMP
	pc_StorageOrgan:     [dynamic]bool,

	pc_SamplingDepth:                    f64,
	pc_TargetNSamplingDepth:             f64,
	pc_TargetN30:                        f64,
	pc_MaxNUptakeParam:                  f64,
	pc_RootDistributionParam:            f64,
	pc_PlantDensity:                     int, // [plants m-2]
	pc_RootGrowthLag:                    f64,
	pc_MinimumTemperatureRootGrowth:     f64,
	pc_InitialRootingDepth:              f64,
	pc_RootPenetrationRate:              f64,
	pc_RootFormFactor:                   f64,
	pc_SpecificRootLength:               f64,
	pc_StageAfterCut:                    int, // stage number is zero-based
	pc_LimitingTemperatureHeatStress:    f64,
	pc_CuttingDelayDays:                 int,
	pc_DroughtImpactOnFertilityFactor:   f64,

	EF_MONO:  f64, // [ug gDW-1 h-1] Monoterpenes emitted right after synthesis
	EF_MONOS: f64, // [ug gDW-1 h-1] Monoterpenes stored then emitted
	EF_ISO:   f64, // Isoprene emission factor
	VCMAX25:  f64, // max RubP saturated rate of carboxylation at 25oC (umol m-2 s-1)
	AEKC:     f64, // activation energy for Michaelis-Menten constant for CO2 (J mol-1)
	AEKO:     f64, // activation energy for Michaelis-Menten constant for O2 (J mol-1)
	AEVC:     f64, // activation energy for photosynthesis (J mol-1)
	KC25:     f64, // Michaelis-Menten constant for CO2 at 25oC (umol mol-1 ubar-1)
	KO25:     f64, // Michaelis-Menten constant for O2 at 25oC (mmol mol-1 mbar-1)

	pc_TransitionStageLeafExp: int, // [1-7]
	dormancyStartDoy:          int, // start dormancy of perennial crops at that DOY (0 = unset)
	dormancyEndDoy:            int, // end dormancy, start accumulating temperature sums (0 = unset)
}

// C++ in-class initialisers
make_species_parameters :: proc() -> Species_Parameters {
	return Species_Parameters {
		pc_FieldConditionModifier = 1.0,
		EF_MONO                   = 0.5,
		EF_MONOS                  = 0.5,
		AEKC                      = 65800.0,
		AEKO                      = 1400.0,
		AEVC                      = 68800.0,
		KC25                      = 460.0,
		KO25                      = 330.0,
		pc_TransitionStageLeafExp = -1,
	}
}

// C++: Errors speciesparameters::merge(SpeciesParameters*, Json)
species_parameters_merge :: proc(sp: ^Species_Parameters, j: jx.Value) -> tl.Errors {
	res := default_merge(sp, j, species_parameters_merge)

	jx.set_string_value(&sp.pc_SpeciesId, j, "SpeciesName")
	jx.set_int_value(&sp.pc_CarboxylationPathway, j, "CarboxylationPathway")
	jx.set_double_value(&sp.pc_DefaultRadiationUseEfficiency, j, "DefaultRadiationUseEfficiency")
	jx.set_double_value(&sp.pc_PartBiologicalNFixation, j, "PartBiologicalNFixation")
	jx.set_double_value(&sp.pc_InitialKcFactor, j, "InitialKcFactor")
	jx.set_double_value(&sp.pc_LuxuryNCoeff, j, "LuxuryNCoeff")
	jx.set_double_value(&sp.pc_MaxCropDiameter, j, "MaxCropDiameter")
	jx.set_double_value(&sp.pc_StageAtMaxHeight, j, "StageAtMaxHeight")
	jx.set_double_value(&sp.pc_StageAtMaxDiameter, j, "StageAtMaxDiameter")
	jx.set_double_value(&sp.pc_MinimumNConcentration, j, "MinimumNConcentration")
	jx.set_double_value(
		&sp.pc_MinimumTemperatureForAssimilation,
		j,
		"MinimumTemperatureForAssimilation",
	)
	jx.set_double_value(
		&sp.pc_OptimumTemperatureForAssimilation,
		j,
		"OptimumTemperatureForAssimilation",
	)
	jx.set_double_value(
		&sp.pc_MaximumTemperatureForAssimilation,
		j,
		"MaximumTemperatureForAssimilation",
	)
	jx.set_double_value(
		&sp.pc_NConcentrationAbovegroundBiomass,
		j,
		"NConcentrationAbovegroundBiomass",
	)
	jx.set_double_value(&sp.pc_NConcentrationB0, j, "NConcentrationB0")
	jx.set_double_value(&sp.pc_NConcentrationPN, j, "NConcentrationPN")
	jx.set_double_value(&sp.pc_NConcentrationRoot, j, "NConcentrationRoot")
	jx.set_int_value(
		&sp.pc_DevelopmentAccelerationByNitrogenStress,
		j,
		"DevelopmentAccelerationByNitrogenStress",
	)
	jx.set_double_value(&sp.pc_FieldConditionModifier, j, "FieldConditionModifier")
	jx.set_double_value(&sp.pc_AssimilateReallocation, j, "AssimilateReallocation")
	jx.set_double_vector(&sp.pc_BaseTemperature, j, "BaseTemperature")
	jx.set_double_vector(&sp.pc_OrganMaintenanceRespiration, j, "OrganMaintenanceRespiration")
	jx.set_double_vector(&sp.pc_OrganGrowthRespiration, j, "OrganGrowthRespiration")
	jx.set_double_vector(&sp.pc_StageMaxRootNConcentration, j, "StageMaxRootNConcentration")
	jx.set_double_vector(&sp.pc_InitialOrganBiomass, j, "InitialOrganBiomass")
	jx.set_double_vector(&sp.pc_CriticalOxygenContent, j, "CriticalOxygenContent")

	jx.set_double_vector(&sp.pc_StageMobilFromStorageCoeff, j, "StageMobilFromStorageCoeff")
	if len(sp.pc_StageMobilFromStorageCoeff) == 0 {
		resize(&sp.pc_StageMobilFromStorageCoeff, len(sp.pc_CriticalOxygenContent))
	}

	jx.set_bool_vector(&sp.pc_AbovegroundOrgan, j, "AbovegroundOrgan")
	jx.set_bool_vector(&sp.pc_StorageOrgan, j, "StorageOrgan")
	jx.set_double_value(&sp.pc_SamplingDepth, j, "SamplingDepth")
	jx.set_double_value(&sp.pc_TargetNSamplingDepth, j, "TargetNSamplingDepth")
	jx.set_double_value(&sp.pc_TargetN30, j, "TargetN30")
	jx.set_double_value(&sp.pc_MaxNUptakeParam, j, "MaxNUptakeParam")
	jx.set_double_value(&sp.pc_RootDistributionParam, j, "RootDistributionParam")
	jx.set_int_value(&sp.pc_PlantDensity, j, "PlantDensity")
	jx.set_double_value(&sp.pc_RootGrowthLag, j, "RootGrowthLag")
	jx.set_double_value(&sp.pc_MinimumTemperatureRootGrowth, j, "MinimumTemperatureRootGrowth")
	jx.set_double_value(&sp.pc_InitialRootingDepth, j, "InitialRootingDepth")
	jx.set_double_value(&sp.pc_RootPenetrationRate, j, "RootPenetrationRate")
	jx.set_double_value(&sp.pc_RootFormFactor, j, "RootFormFactor")
	jx.set_double_value(&sp.pc_SpecificRootLength, j, "SpecificRootLength")
	jx.set_int_value(&sp.pc_StageAfterCut, j, "StageAfterCut")
	if sp.pc_StageAfterCut > 0 {
		sp.pc_StageAfterCut -= 1
	}
	jx.set_double_value(&sp.pc_LimitingTemperatureHeatStress, j, "LimitingTemperatureHeatStress")
	jx.set_int_value(&sp.pc_CuttingDelayDays, j, "CuttingDelayDays")
	jx.set_double_value(
		&sp.pc_DroughtImpactOnFertilityFactor,
		j,
		"DroughtImpactOnFertilityFactor",
	)

	jx.set_double_value(&sp.EF_MONO, j, "EF_MONO")
	jx.set_double_value(&sp.EF_MONOS, j, "EF_MONOS")
	jx.set_double_value(&sp.EF_ISO, j, "EF_ISO")
	jx.set_double_value(&sp.VCMAX25, j, "VCMAX25")
	jx.set_double_value(&sp.AEKC, j, "AEKC")
	jx.set_double_value(&sp.AEVC, j, "AEVC")
	jx.set_double_value(&sp.AEKO, j, "AEKO")
	jx.set_double_value(&sp.KC25, j, "KC25")
	jx.set_double_value(&sp.KO25, j, "KO25")

	jx.set_int_value(&sp.pc_TransitionStageLeafExp, j, "TransitionStageLeafExp")
	jx.set_int_value(&sp.dormancyStartDoy, j, "DormancyStartDoy")
	jx.set_int_value(&sp.dormancyEndDoy, j, "DormancyEndDoy")

	return res
}

// C++: json11::Json speciesparameters::to_json(const SpeciesParameters*)
species_parameters_to_json :: proc(sp: ^Species_Parameters, a: Allocator) -> jx.Value {
	return jx.obj(
		a,
		{"type", jx.sl("SpeciesParameters")},
		{"SpeciesName", jx.s(sp.pc_SpeciesId, a)},
		{"CarboxylationPathway", jx.i(sp.pc_CarboxylationPathway)},
		{"DefaultRadiationUseEfficiency", jx.f(sp.pc_DefaultRadiationUseEfficiency)},
		{"PartBiologicalNFixation", jx.f(sp.pc_PartBiologicalNFixation)},
		{"InitialKcFactor", jx.f(sp.pc_InitialKcFactor)},
		{"LuxuryNCoeff", jx.f(sp.pc_LuxuryNCoeff)},
		{"MaxCropDiameter", jx.f(sp.pc_MaxCropDiameter)},
		{"StageAtMaxHeight", jx.f(sp.pc_StageAtMaxHeight)},
		{"StageAtMaxDiameter", jx.f(sp.pc_StageAtMaxDiameter)},
		{"MinimumNConcentration", jx.f(sp.pc_MinimumNConcentration)},
		{
			"MinimumTemperatureForAssimilation",
			jx.f(sp.pc_MinimumTemperatureForAssimilation),
		},
		{
			"OptimumTemperatureForAssimilation",
			jx.f(sp.pc_OptimumTemperatureForAssimilation),
		},
		{
			"MaximumTemperatureForAssimilation",
			jx.f(sp.pc_MaximumTemperatureForAssimilation),
		},
		{"NConcentrationAbovegroundBiomass", jx.f(sp.pc_NConcentrationAbovegroundBiomass)},
		{"NConcentrationB0", jx.f(sp.pc_NConcentrationB0)},
		{"NConcentrationPN", jx.f(sp.pc_NConcentrationPN)},
		{"NConcentrationRoot", jx.f(sp.pc_NConcentrationRoot)},
		{
			"DevelopmentAccelerationByNitrogenStress",
			jx.i(sp.pc_DevelopmentAccelerationByNitrogenStress),
		},
		{"FieldConditionModifier", jx.f(sp.pc_FieldConditionModifier)},
		{"AssimilateReallocation", jx.f(sp.pc_AssimilateReallocation)},
		{"BaseTemperature", prim_arr_f64(sp.pc_BaseTemperature[:], a)},
		{"OrganMaintenanceRespiration", prim_arr_f64(sp.pc_OrganMaintenanceRespiration[:], a)},
		{"OrganGrowthRespiration", prim_arr_f64(sp.pc_OrganGrowthRespiration[:], a)},
		{"StageMaxRootNConcentration", prim_arr_f64(sp.pc_StageMaxRootNConcentration[:], a)},
		{"InitialOrganBiomass", prim_arr_f64(sp.pc_InitialOrganBiomass[:], a)},
		{"CriticalOxygenContent", prim_arr_f64(sp.pc_CriticalOxygenContent[:], a)},
		{"StageMobilFromStorageCoeff", prim_arr_f64(sp.pc_StageMobilFromStorageCoeff[:], a)},
		{"AbovegroundOrgan", prim_arr_bool(sp.pc_AbovegroundOrgan[:], a)},
		{"StorageOrgan", prim_arr_bool(sp.pc_StorageOrgan[:], a)},
		{"SamplingDepth", jx.f(sp.pc_SamplingDepth)},
		{"TargetNSamplingDepth", jx.f(sp.pc_TargetNSamplingDepth)},
		{"TargetN30", jx.f(sp.pc_TargetN30)},
		{"MaxNUptakeParam", jx.f(sp.pc_MaxNUptakeParam)},
		{"RootDistributionParam", jx.f(sp.pc_RootDistributionParam)},
		{"PlantDensity", jx.vu_int(sp.pc_PlantDensity, "plants m-2", a)},
		{"RootGrowthLag", jx.f(sp.pc_RootGrowthLag)},
		{"MinimumTemperatureRootGrowth", jx.f(sp.pc_MinimumTemperatureRootGrowth)},
		{"InitialRootingDepth", jx.f(sp.pc_InitialRootingDepth)},
		{"RootPenetrationRate", jx.f(sp.pc_RootPenetrationRate)},
		{"RootFormFactor", jx.f(sp.pc_RootFormFactor)},
		{"SpecificRootLength", jx.f(sp.pc_SpecificRootLength)},
		{"StageAfterCut", jx.i(sp.pc_StageAfterCut)},
		{"LimitingTemperatureHeatStress", jx.f(sp.pc_LimitingTemperatureHeatStress)},
		{"CuttingDelayDays", jx.i(sp.pc_CuttingDelayDays)},
		{"DroughtImpactOnFertilityFactor", jx.f(sp.pc_DroughtImpactOnFertilityFactor)},
		{"EF_MONO", jx.vu(sp.EF_MONO, "ug gDW-1 h-1", a)},
		{"EF_MONOS", jx.vu(sp.EF_MONOS, "ug gDW-1 h-1", a)},
		{"EF_ISO", jx.vu(sp.EF_ISO, "ug gDW-1 h-1", a)},
		{"VCMAX25", jx.vu(sp.VCMAX25, "umol m-2 s-1", a)},
		{"AEKC", jx.vu(sp.AEKC, "J mol-1", a)},
		{"AEKO", jx.vu(sp.AEKO, "J mol-1", a)},
		{"AEVC", jx.vu(sp.AEVC, "J mol-1", a)},
		{"KC25", jx.vu(sp.KC25, "umol mol-1 ubar-1", a)},
		{"KO25", jx.vu(sp.KO25, "mmol mol-1 mbar-1", a)},
		{"TransitionStageLeafExp", jx.vu_int(sp.pc_TransitionStageLeafExp, "1-7", a)},
		{"DormancyStartDoy", jx.i(sp.dormancyStartDoy)},
		{"DormancyEndDoy", jx.i(sp.dormancyEndDoy)},
	)
}

// C++: size_t speciesparameters::numberOfDevelopmentalStages(const SpeciesParameters*)
species_parameters_number_of_developmental_stages :: proc(sp: ^Species_Parameters) -> int {
	return len(sp.pc_BaseTemperature)
}

// C++: size_t speciesparameters::numberOfOrgans(const SpeciesParameters*) - old NRKOM
species_parameters_number_of_organs :: proc(sp: ^Species_Parameters) -> int {
	return len(sp.pc_OrganGrowthRespiration)
}

// ---------------------------------------------------------------------------
// CultivarParameters
// ---------------------------------------------------------------------------

// C++: struct monica::CultivarParameters
Cultivar_Parameters :: struct {
	pc_CultivarId:                  string,
	pc_Description:                 string,
	pc_Perennial:                   bool,
	pc_MaxAssimilationRate:         f64, // old MAXAMAX
	pc_LightExtinctionCoefficient:  f64,
	pc_MaxCropHeight:               f64,
	pc_ResidueNRatio:               f64,
	pc_LT50cultivar:                f64,

	pc_CropHeightP1:                f64,
	pc_CropHeightP2:                f64,
	pc_CropSpecificMaxRootingDepth: f64, // old WUMAXPF [m]

	pc_AssimilatePartitioningCoeff: [dynamic][dynamic]f64, // old PRO
	pc_OrganSenescenceRate:         [dynamic][dynamic]f64, // old DEAD

	pc_BaseDaylength:             [dynamic]f64, // old DLBAS
	pc_OptimumTemperature:        [dynamic]f64,
	pc_DaylengthRequirement:      [dynamic]f64, // old DEC
	pc_DroughtStressThreshold:    [dynamic]f64, // old DRYswell
	pc_SpecificLeafArea:          [dynamic]f64, // old LAIFKT [ha kg-1]
	pc_StageKcFactor:             [dynamic]f64, // old Kc
	pc_StageTemperatureSum:       [dynamic]f64, // old TSUM
	pc_VernalisationRequirement:  [dynamic]f64, // old VSCHWELL

	pc_HeatSumIrrigationStart: f64,
	pc_HeatSumIrrigationEnd:   f64,

	pc_CriticalTemperatureHeatStress:  f64,
	pc_BeginSensitivePhaseHeatStress:  f64,
	pc_EndSensitivePhaseHeatStress:    f64,

	pc_FrostHardening:        f64,
	pc_FrostDehardening:      f64,
	pc_LowTemperatureExposure: f64,
	pc_RespiratoryStress:     f64,
	pc_LatestHarvestDoy:      int,

	pc_OrganIdsForPrimaryYield:   [dynamic]Yield_Component,
	pc_OrganIdsForSecondaryYield: [dynamic]Yield_Component,
	pc_OrganIdsForCutting:        [dynamic]Yield_Component,

	pc_EarlyRefLeafExp: f64, // 12 = wheat (first guess)
	pc_RefLeafExp:      f64, // 20 = wheat, 22 = maize (first guess)

	pc_MinTempDev_WE: f64,
	pc_OptTempDev_WE: f64,
	pc_MaxTempDev_WE: f64,

	winterCrop: bool,
}

// C++ in-class initialisers
make_cultivar_parameters :: proc() -> Cultivar_Parameters {
	return Cultivar_Parameters {
		pc_LightExtinctionCoefficient = 0.8,
		pc_LatestHarvestDoy           = -1,
		pc_EarlyRefLeafExp            = 12.0,
		pc_RefLeafExp                 = 20.0,
	}
}

// C++: Errors cultivarparameters::merge(CultivarParameters*, Json)
cultivar_parameters_merge :: proc(cp: ^Cultivar_Parameters, j: jx.Value) -> tl.Errors {
	res := default_merge(cp, j, cultivar_parameters_merge)

	merge_yield_components :: proc(arr: jx.Value) -> [dynamic]Yield_Component {
		ycs := make([dynamic]Yield_Component, 0, context.allocator)
		for jyc in jx.array_items(arr) {
			yc := make_yield_component()
			_ = yield_component_merge(&yc, jyc)
			append(&ycs, yc)
		}
		return ycs
	}

	if jx.is_array(jx.get(j, "OrganIdsForPrimaryYield")) {
		cp.pc_OrganIdsForPrimaryYield = merge_yield_components(jx.get(j, "OrganIdsForPrimaryYield"))
	} else {
		tl.append_errorf(
			&res,
			"Couldn't read 'OrganIdsForPrimaryYield' key from JSON object:\n%s",
			jx.dump(j),
		)
	}

	if jx.is_array(jx.get(j, "OrganIdsForSecondaryYield")) {
		cp.pc_OrganIdsForSecondaryYield =
			merge_yield_components(jx.get(j, "OrganIdsForSecondaryYield"))
	} else {
		tl.append_errorf(
			&res,
			"Couldn't read 'OrganIdsForSecondaryYield' key from JSON object:\n%s",
			jx.dump(j),
		)
	}

	if jx.is_array(jx.get(j, "OrganIdsForCutting")) {
		cp.pc_OrganIdsForCutting = merge_yield_components(jx.get(j, "OrganIdsForCutting"))
	} else {
		tl.append_warningf(
			&res,
			"Couldn't read 'OrganIdsForCutting' key from JSON object:\n%s",
			jx.dump(j),
		)
	}

	jx.set_string_value(&cp.pc_CultivarId, j, "CultivarName")
	jx.set_string_value(&cp.pc_Description, j, "Description")
	jx.set_bool_value(&cp.pc_Perennial, j, "Perennial")
	jx.set_double_value(&cp.pc_MaxAssimilationRate, j, "MaxAssimilationRate")
	jx.set_double_value(&cp.pc_LightExtinctionCoefficient, j, "LightExtinctionCoefficient")
	jx.set_double_value(&cp.pc_MaxCropHeight, j, "MaxCropHeight")
	jx.set_double_value(&cp.pc_ResidueNRatio, j, "ResidueNRatio")
	jx.set_double_value(&cp.pc_LT50cultivar, j, "LT50cultivar")
	jx.set_double_value(&cp.pc_CropHeightP1, j, "CropHeightP1")
	jx.set_double_value(&cp.pc_CropHeightP2, j, "CropHeightP2")
	jx.set_double_value(&cp.pc_CropSpecificMaxRootingDepth, j, "CropSpecificMaxRootingDepth")
	jx.set_double_vector(&cp.pc_BaseDaylength, j, "BaseDaylength")
	jx.set_double_vector(&cp.pc_OptimumTemperature, j, "OptimumTemperature")
	jx.set_double_vector(&cp.pc_DaylengthRequirement, j, "DaylengthRequirement")
	jx.set_double_vector(&cp.pc_DroughtStressThreshold, j, "DroughtStressThreshold")
	jx.set_double_vector(&cp.pc_SpecificLeafArea, j, "SpecificLeafArea")
	jx.set_double_vector(&cp.pc_StageKcFactor, j, "StageKcFactor")
	jx.set_double_vector(&cp.pc_StageTemperatureSum, j, "StageTemperatureSum")
	jx.set_double_vector(&cp.pc_VernalisationRequirement, j, "VernalisationRequirement")
	jx.set_double_value(&cp.pc_HeatSumIrrigationStart, j, "HeatSumIrrigationStart")
	jx.set_double_value(&cp.pc_HeatSumIrrigationEnd, j, "HeatSumIrrigationEnd")
	jx.set_double_value(&cp.pc_CriticalTemperatureHeatStress, j, "CriticalTemperatureHeatStress")
	jx.set_double_value(&cp.pc_BeginSensitivePhaseHeatStress, j, "BeginSensitivePhaseHeatStress")
	jx.set_double_value(&cp.pc_EndSensitivePhaseHeatStress, j, "EndSensitivePhaseHeatStress")
	jx.set_double_value(&cp.pc_FrostHardening, j, "FrostHardening")
	jx.set_double_value(&cp.pc_FrostDehardening, j, "FrostDehardening")
	jx.set_double_value(&cp.pc_LowTemperatureExposure, j, "LowTemperatureExposure")
	jx.set_double_value(&cp.pc_RespiratoryStress, j, "RespiratoryStress")
	jx.set_int_value(&cp.pc_LatestHarvestDoy, j, "LatestHarvestDoy")
	jx.set_bool_value(&cp.winterCrop, j, "WinterCrop")

	if jx.is_array(jx.get(j, "AssimilatePartitioningCoeff")) {
		apcs := jx.array_items(jx.get(j, "AssimilatePartitioningCoeff"))
		resize(&cp.pc_AssimilatePartitioningCoeff, len(apcs))
		for js, idx in apcs {
			cp.pc_AssimilatePartitioningCoeff[idx] = jx.double_vector(js)
		}
	}
	if jx.is_array(jx.get(j, "OrganSenescenceRate")) {
		osrs := jx.array_items(jx.get(j, "OrganSenescenceRate"))
		resize(&cp.pc_OrganSenescenceRate, len(osrs))
		for js, idx in osrs {
			cp.pc_OrganSenescenceRate[idx] = jx.double_vector(js)
		}
	}

	jx.set_double_value(&cp.pc_EarlyRefLeafExp, j, "EarlyRefLeafExp")
	jx.set_double_value(&cp.pc_RefLeafExp, j, "RefLeafExp")

	jx.set_double_value(&cp.pc_MinTempDev_WE, j, "MinTempDev_WE")
	jx.set_double_value(&cp.pc_OptTempDev_WE, j, "OptTempDev_WE")
	jx.set_double_value(&cp.pc_MaxTempDev_WE, j, "MaxTempDev_WE")

	return res
}

// C++: json11::Json cultivarparameters::to_json(const CultivarParameters*)
cultivar_parameters_to_json :: proc(cp: ^Cultivar_Parameters, a: Allocator) -> jx.Value {
	apcs := make(jx.Array, 0, len(cp.pc_AssimilatePartitioningCoeff), a)
	for row in cp.pc_AssimilatePartitioningCoeff {
		append(&apcs, prim_arr_f64(row[:], a))
	}

	osrs := make(jx.Array, 0, len(cp.pc_OrganSenescenceRate), a)
	for row in cp.pc_OrganSenescenceRate {
		append(&osrs, prim_arr_f64(row[:], a))
	}

	yield_components_to_json :: proc(ycs: []Yield_Component, a: Allocator) -> jx.Value {
		out := make(jx.Array, 0, len(ycs), a)
		for yc in ycs {
			yc := yc
			append(&out, yield_component_to_json(&yc, a))
		}
		return jx.Value(out)
	}

	return jx.obj(
		a,
		{"type", jx.sl("CultivarParameters")},
		{"CultivarName", jx.s(cp.pc_CultivarId, a)},
		{"Description", jx.s(cp.pc_Description, a)},
		{"Perennial", jx.b(cp.pc_Perennial)},
		{"MaxAssimilationRate", jx.f(cp.pc_MaxAssimilationRate)},
		{"LightExtinctionCoefficient", jx.f(cp.pc_LightExtinctionCoefficient)},
		{"MaxCropHeight", jx.vu(cp.pc_MaxCropHeight, "m", a)},
		{"ResidueNRatio", jx.f(cp.pc_ResidueNRatio)},
		{"LT50cultivar", jx.f(cp.pc_LT50cultivar)},
		{"CropHeightP1", jx.f(cp.pc_CropHeightP1)},
		{"CropHeightP2", jx.f(cp.pc_CropHeightP2)},
		{"CropSpecificMaxRootingDepth", jx.f(cp.pc_CropSpecificMaxRootingDepth)},
		{"AssimilatePartitioningCoeff", jx.Value(apcs)},
		{"OrganSenescenceRate", jx.Value(osrs)},
		{"BaseDaylength", jx.arr(a, prim_arr_f64(cp.pc_BaseDaylength[:], a), jx.sl("h"))},
		{"OptimumTemperature", jx.arr(a, prim_arr_f64(cp.pc_OptimumTemperature[:], a), jx.sl("°C"))},
		{
			"DaylengthRequirement",
			jx.arr(a, prim_arr_f64(cp.pc_DaylengthRequirement[:], a), jx.sl("h")),
		},
		{"DroughtStressThreshold", prim_arr_f64(cp.pc_DroughtStressThreshold[:], a)},
		{
			"SpecificLeafArea",
			jx.arr(a, prim_arr_f64(cp.pc_SpecificLeafArea[:], a), jx.sl("ha kg-1")),
		},
		{"StageKcFactor", jx.arr(a, prim_arr_f64(cp.pc_StageKcFactor[:], a), jx.sl("1;0"))},
		{
			"StageTemperatureSum",
			jx.arr(a, prim_arr_f64(cp.pc_StageTemperatureSum[:], a), jx.sl("°C d")),
		},
		{"VernalisationRequirement", prim_arr_f64(cp.pc_VernalisationRequirement[:], a)},
		{"HeatSumIrrigationStart", jx.f(cp.pc_HeatSumIrrigationStart)},
		{"HeatSumIrrigationEnd", jx.f(cp.pc_HeatSumIrrigationEnd)},
		{"CriticalTemperatureHeatStress", jx.vu(cp.pc_CriticalTemperatureHeatStress, "°C", a)},
		{"BeginSensitivePhaseHeatStress", jx.vu(cp.pc_BeginSensitivePhaseHeatStress, "°C d", a)},
		{"EndSensitivePhaseHeatStress", jx.vu(cp.pc_EndSensitivePhaseHeatStress, "°C d", a)},
		{"FrostHardening", jx.f(cp.pc_FrostHardening)},
		{"FrostDehardening", jx.f(cp.pc_FrostDehardening)},
		{"LowTemperatureExposure", jx.f(cp.pc_LowTemperatureExposure)},
		{"RespiratoryStress", jx.f(cp.pc_RespiratoryStress)},
		{"LatestHarvestDoy", jx.i(cp.pc_LatestHarvestDoy)},
		{
			"OrganIdsForPrimaryYield",
			yield_components_to_json(cp.pc_OrganIdsForPrimaryYield[:], a),
		},
		{
			"OrganIdsForSecondaryYield",
			yield_components_to_json(cp.pc_OrganIdsForSecondaryYield[:], a),
		},
		{"OrganIdsForCutting", yield_components_to_json(cp.pc_OrganIdsForCutting[:], a)},
		{"EarlyRefLeafExp", jx.f(cp.pc_EarlyRefLeafExp)},
		{"RefLeafExp", jx.f(cp.pc_RefLeafExp)},
		{"MinTempDev_WE", jx.f(cp.pc_MinTempDev_WE)},
		{"OptTempDev_WE", jx.f(cp.pc_OptTempDev_WE)},
		{"MaxTempDev_WE", jx.f(cp.pc_MaxTempDev_WE)},
		{"WinterCrop", jx.b(cp.winterCrop)},
	)
}

// C++: inline size_t cultivarparameters::numberOfDevelopmentalStages(const CultivarParameters*)
cultivar_parameters_number_of_developmental_stages :: proc(cp: ^Cultivar_Parameters) -> int {
	return len(cp.pc_BaseDaylength)
}

// ---------------------------------------------------------------------------
// CropParameters
// ---------------------------------------------------------------------------

// C++: struct monica::CropParameters
Crop_Parameters :: struct {
	speciesParams:  Species_Parameters,
	cultivarParams: Cultivar_Parameters,
	// Maybe, because unset should fall back to CropModuleParameters'
	// __enable_vernalisation_factor_fix__ default, not to false.
	__enable_vernalisation_factor_fix__: Maybe(bool),
}

make_crop_parameters :: proc() -> Crop_Parameters {
	return Crop_Parameters {
		speciesParams = make_species_parameters(),
		cultivarParams = make_cultivar_parameters(),
	}
}

// C++: Errors cropparameters::merge(CropParameters*, Json j) - single-document overload
crop_parameters_merge :: proc(cp: ^Crop_Parameters, j: jx.Value) -> tl.Errors {
	evff := jx.get(j, "__enable_vernalisation_factor_fix__")
	if !jx.is_null(evff) && jx.is_bool(evff) {
		cp.__enable_vernalisation_factor_fix__ = jx.bool_value_of(evff)
	}
	return crop_parameters_merge_sj_cj(cp, jx.get(j, "species"), jx.get(j, "cultivar"))
}

// C++: Errors cropparameters::merge(CropParameters*, Json sj, Json cj) - split-document overload
crop_parameters_merge_sj_cj :: proc(cp: ^Crop_Parameters, sj: jx.Value, cj: jx.Value) -> tl.Errors {
	res: tl.Errors
	tl.append_errors(&res, species_parameters_merge(&cp.speciesParams, sj))
	tl.append_errors(&res, cultivar_parameters_merge(&cp.cultivarParams, cj))
	return res
}

// C++: json11::Json cropparameters::to_json(const CropParameters*)
crop_parameters_to_json :: proc(cp: ^Crop_Parameters, a: Allocator) -> jx.Value {
	return jx.obj(
		a,
		{"type", jx.sl("CropParameters")},
		{"species", species_parameters_to_json(&cp.speciesParams, a)},
		{"cultivar", cultivar_parameters_to_json(&cp.cultivarParams, a)},
	)
}

// C++: inline string cropparameters::cropName(const CropParameters*) - old FRUCHT$(AKF)
crop_name :: proc(cp: ^Crop_Parameters, a: Allocator) -> string {
	return strings.concatenate(
		{cp.speciesParams.pc_SpeciesId, "/", cp.cultivarParams.pc_CultivarId},
		a,
	)
}

// ---------------------------------------------------------------------------
// local helpers
// ---------------------------------------------------------------------------

// C++: template<class Collection> J11Array Tools::toPrimJsonArray(const Collection&)
@(private)
prim_arr_f64 :: proc(vals: []f64, a: Allocator) -> jx.Value {
	out := make(jx.Array, 0, len(vals), a)
	for v in vals {
		append(&out, jx.f(v))
	}
	return jx.Value(out)
}

@(private)
prim_arr_bool :: proc(vals: []bool, a: Allocator) -> jx.Value {
	out := make(jx.Array, 0, len(vals), a)
	for v in vals {
		append(&out, jx.b(v))
	}
	return jx.Value(out)
}
