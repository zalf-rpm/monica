// Phase 5 checkpoint 2: src/core/crop-module.h's CropModule struct and
// src/core/crop-module.cpp's makeCropModule constructor (the first overload -
// the second, `mas::schema::model::monica::CropModuleState::Reader`-based, is
// Cap'n Proto deserialize and dropped, per plan-odin.md's "Explicitly
// dropped" table). Replaces crop_module_stub.odin's 11-field placeholder
// wholesale; every field here keeps its exact C++ name, so the phase-4
// modules that already hold a `cropModule: ^Crop_Module` pointer
// (soilmoisture, soiltransport, soilorganic) compile unchanged against this
// drop-in replacement.
//
// Phase 5 checkpoint 3 adds the phenology + canopy-geometry cropmodule::
// functions: fcRadiation, fcDaylengthFactor, fcVernalisationFactor,
// fcOxygenDeficiency, fcCropDevelopmentalStage (+ its
// fcUpdateCropParametersForPerennial dependency), fcKcFactor, fcCropSize,
// fcCropGreenArea, fcSoilCoverage, setStage, the anthesis/maturity query
// functions, and the small pure getters. Not yet ported: fcCropPhotosynthesis
// onward (checkpoint 4+), step() itself and every fireEvent-driven bit of
// orchestration inline in it (checkpoint 7), forceTransplantState (fired by a
// Transplant workstep, checkpoint 7), setPerennialCropParameters (fired by
// Sowing, checkpoint 7), organIdsForPrimaryYield (yield, checkpoint 5),
// getEffectiveRootingDepth (root/water, checkpoint 6).
package core

import d "../../support/date"
import tl "../../support/tools"
import p "../params"
import libc "core:c/libc"
import "core:fmt"
import "core:slice"

// Phase 5 checkpoint 4 adds fcCropPhotosynthesis (the largest function in
// crop-module.cpp, ~1100 lines) plus fcGrossPrimaryProduction,
// fcNetPrimaryProduction and calculateVOCEmissions - the functions that wire
// the phase-5-checkpoint-1 satellite modules (photosynthesis-FvCB,
// O3-impact, voc-guenther, voc-jjv) into the crop module proper.

// C++: monica::OId::ORGAN (src/io/output.h) - a plain (unscoped) C++ enum
// nested in struct OId, so OId::LEAF etc. are usable directly as organ
// indices into vc_OrganBiomass and friends. io/output.h itself is phase 7
// (not yet ported); crop-module.cpp only ever needs the bare index values,
// not the rest of OId, so they're defined locally here instead of waiting.
Organ_Root :: 0
Organ_Leaf :: 1
Organ_Shoot :: 2
Organ_Fruit :: 3
Organ_Struct :: 4
Organ_Sugar :: 5

// C++: struct monica::CropModule (src/core/crop-module.h)
Crop_Module :: struct {
	noOfOrgans:                             int, // C++ size_t
	noOfDevStages:                          int, // C++ size_t

	// --- BEGIN TRANSPLANT MODIFICATION ---
	vc_TransplantShockDuration:             int,
	vc_DaysSinceTransplant:                 int,
	vc_TransplantEfficiency:                f64,
	// --- END TRANSPLANT MODIFICATION ---
	vc_TranspirationDeficit:                f64, //! old TRREL
	vc_PotentialTranspirationDeficit:       f64,
	vc_ActualTranspirationDeficit:          f64,
	vc_TranspirationReduced:                f64,
	rootNRedux:                             f64, //! old REDWU
	vc_TimeUnderAnoxia:                     int,

	// C++: Intercropping *intercropping - Intercropping itself is dropped
	// (Cap'n Proto RPC, see plan-odin.md's dropped table); every real use of
	// this field is behind `if (isIntercropping)`, always false in every
	// fixture in this repo, so it stays a typed-but-inert pointer.
	intercropping:                          rawptr,
	soilColumn:                             ^Soil_Column,
	siteParams:                             ^p.Site_Parameters,
	simParams:                              ^p.Simulation_Parameters,
	cropModParams:                          ^p.Crop_Module_Parameters,
	cropParams:                             p.Crop_Parameters,
	residueParams:                          p.Crop_Residue_Parameters,
	// C++: kj::Own<CropParameters> perennialCropParams - nil unless a
	// Sowing workstep's separatePerennialCropParams is set (phase 6).
	perennialCropParams:                    ^p.Crop_Parameters,

	//! old N
	vc_AbovegroundBiomass:                  f64, //! old OBMAS
	vc_AbovegroundBiomassOld:               f64, //! old OBALT
	vc_ActualTranspiration:                 f64,
	vc_Assimilates:                         f64,
	vc_AssimilationRate:                    f64, //! old AMAX
	vc_AstronomicDayLenght:                 f64, //! old DL
	vc_BelowgroundBiomass:                  f64,
	vc_BelowgroundBiomassOld:               f64,
	vc_ClearDayRadiation:                   f64, //! old DRC
	pc_CO2Method:                           int,
	vc_CriticalNConcentration:              f64, //! old GEHMIN
	vc_CropDiameter:                        f64,
	vc_CropFrostRedux:                      f64,
	vc_CropHeatRedux:                       f64,
	vc_CropHeight:                          f64,
	vc_CropNDemand:                         f64, //! old DTGESN
	vc_CropNRedux:                          f64, //! old REDUK
	vc_CropWaterUptake:                     [dynamic]f64, //! old TP
	vc_CurrentTemperatureSum:               [dynamic]f64, //! old SUM
	vc_CurrentTotalTemperatureSum:          f64, //! old FP
	vc_CurrentTotalTemperatureSumRoot:      f64,
	vc_DaylengthFactor:                     f64, //! old DAYL
	vc_DaysAfterBeginFlowering:             int,
	vc_Declination:                         f64, //! old EFF0
	vc_DevelopmentalStage:                  int, // C++ size_t, //! old INTWICK
	noOfCropSteps:                          int,
	vc_DroughtImpactOnFertility:            f64,
	vc_EffectiveDayLength:                  f64, //! old DLE
	vc_ErrorStatus:                         bool,
	vc_ErrorMessage:                        string,
	vc_EvaporatedFromIntercept:             f64,
	vc_ExtraterrestrialRadiation:           f64,
	vc_FinalDevelopmentalStage:             int, // C++ size_t
	vc_FixedN:                              f64,
	vc_GlobalRadiation:                     f64,
	vc_GreenAreaIndex:                      f64,
	vc_GrossAssimilates:                    f64,
	vc_GrossPhotosynthesis:                 f64, //! old GPHOT
	vc_GrossPhotosynthesis_mol:             f64,
	vc_GrossPhotosynthesisReference_mol:    f64,
	vc_GrossPrimaryProduction:              f64,
	vc_GrowthCycleEnded:                    bool,
	vc_GrowthRespirationAS:                 f64,
	vc_InterceptionStorage:                 f64,
	vc_KcFactor:                            f64, //! old FKc
	vc_LeafAreaIndex:                       f64, //! old LAI
	vc_sunlitLeafAreaIndex:                 [dynamic]f64,
	vc_shadedLeafAreaIndex:                 [dynamic]f64,
	vc_LT50:                                f64,
	vc_LT50M:                               f64,
	vc_MaintenanceRespirationAS:            f64,
	vc_MaxNUptake:                          f64, //! old MAXUP
	vc_MaxRootingDepth:                     f64, //! old WURM
	vc_NetMaintenanceRespiration:           f64, //! old MAINT
	vc_NetPhotosynthesis:                   f64, //! old GTW
	vc_NetPrecipitation:                    f64,
	vc_NetPrimaryProduction:                f64,
	vc_NConcentrationAbovegroundBiomass:    f64, //! old GEHOB
	vc_NConcentrationAbovegroundBiomassOld: f64, //! old GEHALT
	vc_NContentDeficit:                     f64,
	vc_NConcentrationRoot:                  f64, //! old WUGEH
	vc_NConcentrationRootOld:               f64, //! old
	vc_NUptakeFromLayer:                    [dynamic]f64, //! old PE
	vc_OrganBiomass:                        [dynamic]f64, //! old WORG
	vc_OrganDeadBiomass:                    [dynamic]f64, //! old WDORG
	vc_OrganGreenBiomass:                   [dynamic]f64,
	vc_OrganGrowthIncrement:                [dynamic]f64, //! old GORG
	vc_OrganSenescenceIncrement:            [dynamic]f64, //! old DGORG
	vc_OvercastDayRadiation:                f64, //! old DRO
	vc_OxygenDeficit:                       f64, //! old LURED
	vc_PhotoperiodicDaylength:              f64, //! old DLP
	vc_PhotActRadiationMean:                f64, //! old RDN
	vc_PotentialTranspiration:              f64,
	vc_ReferenceEvapotranspiration:         f64,
	vc_RelativeTotalDevelopment:            f64,
	vc_RemainingEvapotranspiration:         f64,
	vc_ReserveAssimilatePool:               f64, //! old ASPOO
	vc_RootBiomass:                         f64, //! old WUMAS
	vc_RootBiomassOld:                      f64, //! old WUMALT
	vc_RootDensity:                         [dynamic]f64, //! old WUDICH
	vc_RootDiameter:                        [dynamic]f64, //! old WRAD
	vc_RootEffectivity:                     [dynamic]f64, //! old WUEFF
	vc_RootingDepth:                        int, // C++ size_t, //! old WURZ
	vc_RootingDepth_m:                      f64,
	vc_RootingZone:                         int, // C++ size_t
	vc_SoilCoverage:                        f64,
	vs_SoilMineralNContent:                 [dynamic]f64, //! old C1
	vc_SoilSpecificMaxRootingDepth:         f64, //! old WURZMAX [m]
	vs_SoilSpecificMaxRootingDepth:         f64,
	// FAO-56 Dual Kc: GDD-based trapezoidal Kcb curve state.
	vc_KcbFactor:                           f64, // Current daily Kcb (output of GDD-based 4-phase interpolation)
	vc_Kcb_ini:                             f64, // Initial/germination phase Kcb (flat, Phase 1)
	vc_Kcb_mid:                             f64, // Mid-season plateau Kcb (Phase 3)
	vc_Kcb_end:                             f64, // End of late-season Kcb target (Phase 4)
	vc_StomataResistance:                   f64, //! old RSTOM
	vc_StorageOrgan:                        int,
	vc_TargetNConcentration:                f64, //! old GEHMAX
	vc_TimeStep:                            f64, //! old dt
	TimeUnderAnoxiaThresholdDefault:        int,
	vc_TotalBiomass:                        f64,
	vc_TotalBiomassNContent:                f64, //! old PESUM
	vc_TotalCropHeatImpact:                 f64,
	vc_TotalNInput:                         f64,
	vc_TotalNUptake:                        f64, //! old SUMPE
	vc_TotalRespired:                       f64,
	vc_Respiration:                         f64,
	vc_SumTotalNUptake:                     f64, //! summation of all calculated NUptake; needed for sensitivity analysis
	vc_TotalRootLength:                     f64, //! old WULAEN
	vc_TotalTemperatureSum:                 f64,
	vc_TemperatureSumToFlowering:           f64,
	vc_Transpiration:                       [dynamic]f64, //! old TP
	vc_TranspirationRedux:                  [dynamic]f64, //! old TRRED
	vc_VernalisationDays:                   f64,
	vc_VernalisationFactor:                 f64, //! old FV
	dyingOut:                               bool,
	vc_AccumulatedETa:                      f64,
	vc_AccumulatedTranspiration:            f64,
	vc_sumExportedCutBiomass:               f64,
	vc_exportedCutBiomass:                  f64,
	vc_sumResidueCutBiomass:                f64,
	vc_residueCutBiomass:                   f64,
	vc_CuttingDelayDays:                    int,
	vc_AnthesisDay:                         int,
	vc_MaturityDay:                         int,
	vc_MaturityReached:                     bool,

	// VOC members
	stepSize24:                             int,
	stepSize240:                            int,
	rad24:                                  [dynamic]f64,
	rad240:                                 [dynamic]f64,
	tfol24:                                 [dynamic]f64,
	tfol240:                                [dynamic]f64,
	index24:                                int,
	index240:                               int,
	full24:                                 bool,
	full240:                                bool,
	guentherEmissions:                      Voc_Emissions,
	jjvEmissions:                           Voc_Emissions,
	vocSpecies:                             Voc_Species_Data,
	cropPhotosynthesisResults:              Voc_Cp_Data,

	// C++: std::function<void(std::string)> fireEvent;
	fireEvent:                              proc(_: string),
	// C++: std::function<void(std::map<size_t, double>, double)> addOrganicMatter;
	addOrganicMatter:                       proc(_: map[int]f64, _: f64),
	// C++: std::function<std::pair<double, double>(double)> getSnowDepthAndCalcTempUnderSnow;
	getSnowDepthAndCalcTempUnderSnow:       proc(_: f64) -> (f64, f64),
	vc_O3_shortTermDamage:                  f64,
	vc_O3_longTermDamage:                   f64,
	vc_O3_senescence:                       f64,
	vc_O3_sumUptake:                        f64,
	vc_O3_WStomatalClosure:                 f64,
	assimilatePartCoeffsReduced:            bool,
	vc_KTkc:                                f64, // old KTkc
	vc_KTko:                                f64, // old KTkc
	stemElongationEventFired:               bool,

	// intercropping
	intercroppingOtherCropHeight:           f64,
	intercroppingOtherLAIt:                 f64,
	fractionOfInterceptedRadiation1:        f64,
	fractionOfInterceptedRadiation2:        f64,
	perennialCropDormancyPeriodEndDate:     d.Date,
}

// C++ in-class initialisers
make_crop_module_defaults :: proc() -> Crop_Module {
	return Crop_Module {
		vc_DaysSinceTransplant = -1,
		vc_TransplantEfficiency = 1.0,
		vc_TranspirationDeficit = 1.0,
		pc_CO2Method = 3,
		vc_CropFrostRedux = 1.0,
		vc_CropHeatRedux = 1.0,
		vc_CropNRedux = 1.0,
		vc_DroughtImpactOnFertility = 1.0,
		vc_KcFactor = 0.6,
		vc_LT50 = -3.0,
		vc_LT50M = -3.0,
		vc_KcbFactor = 0.15,
		vc_Kcb_ini = 0.15,
		vc_StorageOrgan = 4,
		vc_TimeStep = 1.0,
		TimeUnderAnoxiaThresholdDefault = 4,
		vc_CuttingDelayDays = 0,
		vc_AnthesisDay = -1,
		vc_MaturityDay = -1,
		stepSize24 = 24,
		stepSize240 = 240,
		vc_O3_shortTermDamage = 1.0,
		vc_O3_longTermDamage = 1.0,
		vc_O3_senescence = 1.0,
		vc_O3_WStomatalClosure = 1.0,
		intercroppingOtherCropHeight = -1,
		intercroppingOtherLAIt = -1,
	}
}

@(private)
clone_f64_array :: proc(a: [dynamic]f64, allocator := context.allocator) -> [dynamic]f64 {
	out := make([dynamic]f64, len(a), allocator)
	copy(out[:], a[:])
	return out
}

@(private)
clone_bool_array :: proc(a: [dynamic]bool, allocator := context.allocator) -> [dynamic]bool {
	out := make([dynamic]bool, len(a), allocator)
	copy(out[:], a[:])
	return out
}

@(private)
clone_f64_2d_array :: proc(
	a: [dynamic][dynamic]f64,
	allocator := context.allocator,
) -> [dynamic][dynamic]f64 {
	out := make([dynamic][dynamic]f64, len(a), allocator)
	for row, i in a {
		out[i] = clone_f64_array(row, allocator)
	}
	return out
}

@(private)
clone_yield_component_array :: proc(
	a: [dynamic]p.Yield_Component,
	allocator := context.allocator,
) -> [dynamic]p.Yield_Component {
	out := make([dynamic]p.Yield_Component, len(a), allocator)
	copy(out[:], a[:])
	return out
}

@(private)
clone_species_parameters :: proc(
	sp: p.Species_Parameters,
	allocator := context.allocator,
) -> p.Species_Parameters {
	out := sp
	out.pc_BaseTemperature = clone_f64_array(sp.pc_BaseTemperature, allocator)
	out.pc_OrganMaintenanceRespiration = clone_f64_array(
		sp.pc_OrganMaintenanceRespiration,
		allocator,
	)
	out.pc_OrganGrowthRespiration = clone_f64_array(sp.pc_OrganGrowthRespiration, allocator)
	out.pc_StageMaxRootNConcentration = clone_f64_array(
		sp.pc_StageMaxRootNConcentration,
		allocator,
	)
	out.pc_InitialOrganBiomass = clone_f64_array(sp.pc_InitialOrganBiomass, allocator)
	out.pc_CriticalOxygenContent = clone_f64_array(sp.pc_CriticalOxygenContent, allocator)
	out.pc_StageMobilFromStorageCoeff = clone_f64_array(
		sp.pc_StageMobilFromStorageCoeff,
		allocator,
	)
	out.pc_AbovegroundOrgan = clone_bool_array(sp.pc_AbovegroundOrgan, allocator)
	out.pc_StorageOrgan = clone_bool_array(sp.pc_StorageOrgan, allocator)
	return out
}

@(private)
clone_cultivar_parameters :: proc(
	cp: p.Cultivar_Parameters,
	allocator := context.allocator,
) -> p.Cultivar_Parameters {
	out := cp
	out.pc_AssimilatePartitioningCoeff = clone_f64_2d_array(
		cp.pc_AssimilatePartitioningCoeff,
		allocator,
	)
	out.pc_OrganSenescenceRate = clone_f64_2d_array(cp.pc_OrganSenescenceRate, allocator)
	out.pc_BaseDaylength = clone_f64_array(cp.pc_BaseDaylength, allocator)
	out.pc_OptimumTemperature = clone_f64_array(cp.pc_OptimumTemperature, allocator)
	out.pc_DaylengthRequirement = clone_f64_array(cp.pc_DaylengthRequirement, allocator)
	out.pc_DroughtStressThreshold = clone_f64_array(cp.pc_DroughtStressThreshold, allocator)
	out.pc_SpecificLeafArea = clone_f64_array(cp.pc_SpecificLeafArea, allocator)
	out.pc_StageKcFactor = clone_f64_array(cp.pc_StageKcFactor, allocator)
	out.pc_StageTemperatureSum = clone_f64_array(cp.pc_StageTemperatureSum, allocator)
	out.pc_VernalisationRequirement = clone_f64_array(cp.pc_VernalisationRequirement, allocator)
	out.pc_OrganIdsForPrimaryYield = clone_yield_component_array(
		cp.pc_OrganIdsForPrimaryYield,
		allocator,
	)
	out.pc_OrganIdsForSecondaryYield = clone_yield_component_array(
		cp.pc_OrganIdsForSecondaryYield,
		allocator,
	)
	out.pc_OrganIdsForCutting = clone_yield_component_array(cp.pc_OrganIdsForCutting, allocator)
	return out
}

// C++'s CropModule constructor does `cm->cropParams = *cropParams;`, which
// deep-copies via CropParameters' implicit copy constructor (every
// std::vector member is deep-copied). Odin's plain struct assignment only
// copies [dynamic]T headers, aliasing the backing storage - the same class of
// bug as phase 4's clone_aom_pool (soilorganic.odin). This one is not
// hypothetical: `cropParams` typically points at a Sowing workstep's own,
// long-lived CropParameters, reused every time that workstep fires again
// across a multi-year crop rotation (see src/worksteps/sowing.cpp). Without
// this clone, a later checkpoint mutating cm.cropParams (perennial-crop
// handling, cutting) would corrupt the workstep's source object for the next
// season instead of only this crop's own copy.
clone_crop_parameters :: proc(
	cp: p.Crop_Parameters,
	allocator := context.allocator,
) -> p.Crop_Parameters {
	out := cp
	out.speciesParams = clone_species_parameters(cp.speciesParams, allocator)
	out.cultivarParams = clone_cultivar_parameters(cp.cultivarParams, allocator)
	return out
}

// C++: kj::Own<CropModule> monica::makeCropModule(SoilColumn*, const
// CropParameters*, const CropResidueParameters*, const SiteParameters*,
// const CropModuleParameters*, const SimulationParameters*,
// std::function<void(std::string)>, std::function<void(std::map<size_t,
// double>, double)>, std::function<std::pair<double, double>(double)>,
// Intercropping*) - the first overload only; the deserialize overload is
// dropped (Cap'n Proto).
make_crop_module :: proc(
	soilColumn: ^Soil_Column,
	cropParams: ^p.Crop_Parameters,
	residueParams: ^p.Crop_Residue_Parameters,
	siteParams: ^p.Site_Parameters,
	cropModuleParams: ^p.Crop_Module_Parameters,
	simParams: ^p.Simulation_Parameters,
	fireEvent: proc(_: string),
	addOrganicMatter: proc(_: map[int]f64, _: f64),
	getSnowDepthAndCalcTempUnderSnow: proc(_: f64) -> (f64, f64),
	intercropping: rawptr,
	allocator := context.allocator,
) -> Crop_Module {
	cm := make_crop_module_defaults()
	cm.intercropping = intercropping
	cm.soilColumn = soilColumn
	cm.cropModParams = cropModuleParams
	cm.cropParams = clone_crop_parameters(cropParams^, allocator)
	cm.siteParams = siteParams
	cm.simParams = simParams
	cm.residueParams = residueParams^
	cm.fireEvent = fireEvent
	cm.addOrganicMatter = addOrganicMatter
	cm.getSnowDepthAndCalcTempUnderSnow = getSnowDepthAndCalcTempUnderSnow

	cm.noOfOrgans = p.species_parameters_number_of_organs(&cm.cropParams.speciesParams)
	cm.noOfDevStages = p.species_parameters_number_of_developmental_stages(
		&cm.cropParams.speciesParams,
	)
	cm.vc_CurrentTemperatureSum = make([dynamic]f64, cm.noOfDevStages, allocator)
	cm.vc_sunlitLeafAreaIndex = make([dynamic]f64, 24, allocator)
	cm.vc_shadedLeafAreaIndex = make([dynamic]f64, 24, allocator)
	cm.vc_NUptakeFromLayer = make([dynamic]f64, len(cm.soilColumn.layers), allocator)
	cm.vc_OrganBiomass = make([dynamic]f64, cm.noOfOrgans, allocator)
	cm.vc_OrganDeadBiomass = make([dynamic]f64, cm.noOfOrgans, allocator)
	cm.vc_OrganGreenBiomass = make([dynamic]f64, cm.noOfOrgans, allocator)
	cm.vc_OrganGrowthIncrement = make([dynamic]f64, cm.noOfOrgans, allocator)
	cm.vc_OrganSenescenceIncrement = make([dynamic]f64, cm.noOfOrgans, allocator)
	cm.vc_RootDensity = make([dynamic]f64, len(cm.soilColumn.layers), allocator)
	cm.vc_RootDiameter = make([dynamic]f64, len(cm.soilColumn.layers), allocator)
	cm.vc_RootEffectivity = make([dynamic]f64, len(cm.soilColumn.layers), allocator)
	cm.vs_SoilMineralNContent = make([dynamic]f64, len(cm.soilColumn.layers), allocator)
	cm.vc_Transpiration = make([dynamic]f64, len(cm.soilColumn.layers), allocator)
	cm.vc_TranspirationRedux = make([dynamic]f64, len(cm.soilColumn.layers), allocator)
	for &v in cm.vc_TranspirationRedux {
		v = 1.0
	}
	cm.rad24 = make([dynamic]f64, cm.stepSize24, allocator)
	cm.rad240 = make([dynamic]f64, cm.stepSize240, allocator)
	cm.tfol24 = make([dynamic]f64, cm.stepSize24, allocator)
	cm.tfol240 = make([dynamic]f64, cm.stepSize240, allocator)

	// Determining the total temperature sum of all developmental stages after
	// emergence (that's why i_Stage starts with 1) until before senescence
	stageTempSum := cm.cropParams.cultivarParams.pc_StageTemperatureSum
	for i_Stage := 1; i_Stage < cm.noOfDevStages - 1; i_Stage += 1 {
		cm.vc_TotalTemperatureSum += stageTempSum[i_Stage]
		if i_Stage < cm.noOfDevStages - 3 {
			cm.vc_TemperatureSumToFlowering += stageTempSum[i_Stage]
		}
	}

	cm.vc_FinalDevelopmentalStage = cm.noOfDevStages - 1

	// Determining the initial crop organ's biomass
	ago := cm.cropParams.speciesParams.pc_AbovegroundOrgan
	for i_Organ := 0; i_Organ < cm.noOfOrgans; i_Organ += 1 {
		cm.vc_OrganBiomass[i_Organ] = cm.cropParams.speciesParams.pc_InitialOrganBiomass[i_Organ] // [kg ha-1]

		if ago[i_Organ] {
			cm.vc_AbovegroundBiomass +=
				cm.cropParams.speciesParams.pc_InitialOrganBiomass[i_Organ] // [kg ha-1]
		}

		cm.vc_TotalBiomass += cm.cropParams.speciesParams.pc_InitialOrganBiomass[i_Organ] // [kg ha-1]

		// Define storage organ
		if cm.cropParams.speciesParams.pc_StorageOrgan[i_Organ] {
			cm.vc_StorageOrgan = i_Organ
		}
	}

	cm.vc_OrganGreenBiomass = clone_f64_array(cm.vc_OrganBiomass, allocator)

	cm.vc_RootBiomass = cm.cropParams.speciesParams.pc_InitialOrganBiomass[0] // [kg ha-1]

	// Initialisisng the leaf area index
	sla := cm.cropParams.cultivarParams.pc_SpecificLeafArea
	cm.vc_LeafAreaIndex = cm.vc_OrganBiomass[Organ_Leaf] * sla[cm.vc_DevelopmentalStage] // [ha ha-1]

	if cm.vc_LeafAreaIndex <= 0.0 {
		cm.vc_LeafAreaIndex = 0.001
	}

	// Initialising the root
	cm.vc_RootBiomass = cm.vc_OrganBiomass[Organ_Root]

	// @todo Christian: Umrechnung korrekt wenn Biomasse in [kg m-2]?
	PI :: 3.14159265358979323
	cm.vc_TotalRootLength = (cm.vc_RootBiomass * 100000.0 * 100.0 / 7.0) / (0.015 * 0.015 * PI)

	NConcentrationAbovegroundBiomass :=
		cm.cropParams.speciesParams.pc_NConcentrationAbovegroundBiomass
	NConcentrationRoot := cm.cropParams.speciesParams.pc_NConcentrationRoot
	cm.vc_TotalBiomassNContent =
		(cm.vc_AbovegroundBiomass * NConcentrationAbovegroundBiomass) +
		(cm.vc_RootBiomass * NConcentrationRoot)
	cm.vc_NConcentrationAbovegroundBiomass = NConcentrationAbovegroundBiomass
	cm.vc_NConcentrationRoot = NConcentrationRoot

	// Initialising the initial maximum rooting depth
	cropSpecificMaxRootingDepth := cm.cropParams.cultivarParams.pc_CropSpecificMaxRootingDepth
	if cm.cropModParams.pc_AdjustRootDepthForSoilProps {
		R_P_max := cropSpecificMaxRootingDepth
		f_S := cm.soilColumn.layers[0].vs_SoilSandContent // [kg kg-1]
		R_S := (f_S - 0.5) * -0.6

		rho_B := soil_bulk_density(&cm.soilColumn.layers[0]) // [kg m-3]
		R_D := (rho_B / 1000.0 - 1) * -0.3

		cm.vc_MaxRootingDepth =
			R_P_max *
			((R_P_max + (R_P_max * R_S)) / R_P_max) *
			((R_P_max + (R_P_max * R_D)) / R_P_max)
	} else {
		cm.vc_MaxRootingDepth = cropSpecificMaxRootingDepth // [m]
	}

	if cm.siteParams.vs_ImpenetrableLayerDepth > 0 {
		cm.vc_MaxRootingDepth = min(cm.vc_MaxRootingDepth, cm.siteParams.vs_ImpenetrableLayerDepth)
	}

	// FAO-56 Dual Kc: Initialize GDD-based trapezoidal Kcb curve from
	// pc_StageKcFactor. vc_Kcb_ini defaults to 0.15 (FAO-56 Table 17 bare
	// soil); setInitialKcb() may override it.
	stageKcFactor := cm.cropParams.cultivarParams.pc_StageKcFactor
	if len(stageKcFactor) > 0 {
		max_Kc := stageKcFactor[0]
		for v in stageKcFactor {
			if v > max_Kc {
				max_Kc = v
			}
		}
		if max_Kc >= 1.0 {
			cm.vc_Kcb_mid = max(0.15, max_Kc - 0.05) // high-coverage crop (FAO-56 par 7)
		} else {
			cm.vc_Kcb_mid = max(0.15, max_Kc - 0.10) // low-coverage crop (FAO-56 par 7)
		}
		cm.vc_Kcb_end = max(0.0, stageKcFactor[len(stageKcFactor) - 1] - 0.05)
	}
	cm.vc_KcbFactor = cm.vc_Kcb_ini // start at initial value

	return cm
}

// ---------------------------------------------------------------------------
// Phase 5 checkpoint 3: phenology + canopy geometry
// ---------------------------------------------------------------------------

// C++: std::pair<const vector<double>&, const vector<double>&>
// monica::cropmodule::sunlitAndShadedLAI(const CropModule*)
sunlit_and_shaded_lai :: proc(cm: ^Crop_Module) -> ([dynamic]f64, [dynamic]f64) {
	return cm.vc_sunlitLeafAreaIndex, cm.vc_shadedLeafAreaIndex
}

// C++: void monica::cropmodule::setOtherCropHeightAndLAIt(CropModule*, double
// cropHeight, double lait)
set_other_crop_height_and_lait :: proc(cm: ^Crop_Module, cropHeight, lait: f64) {
	cm.intercroppingOtherCropHeight = cropHeight
	cm.intercroppingOtherLAIt = lait
}

// C++: double monica::cropmodule::getFractionOfInterceptedRadiation1(const CropModule*)
get_fraction_of_intercepted_radiation1 :: proc(cm: ^Crop_Module) -> f64 {
	return cm.fractionOfInterceptedRadiation1
}

// C++: double monica::cropmodule::getFractionOfInterceptedRadiation2(const CropModule*)
get_fraction_of_intercepted_radiation2 :: proc(cm: ^Crop_Module) -> f64 {
	return cm.fractionOfInterceptedRadiation2
}

// C++: double monica::cropmodule::getCurrentTotalTemperatureSum(const CropModule*)
get_current_total_temperature_sum :: proc(cm: ^Crop_Module) -> f64 {
	return cm.vc_CurrentTotalTemperatureSum
}

// C++: double monica::cropmodule::getCurrentStageTemperatureSum(const CropModule*)
get_current_stage_temperature_sum :: proc(cm: ^Crop_Module) -> f64 {
	if cm.vc_DevelopmentalStage < len(cm.vc_CurrentTemperatureSum) {
		return cm.vc_CurrentTemperatureSum[cm.vc_DevelopmentalStage]
	}
	return 0.0
}

// C++: double monica::cropmodule::getTotalTemperatureSum(const CropModule*)
get_total_temperature_sum :: proc(cm: ^Crop_Module) -> f64 {
	return cm.vc_TotalTemperatureSum
}

// C++: double monica::cropmodule::sumStageTemperatureSums(const CropModule*,
// int startAtStage, int endAtInclStage)
sum_stage_temperature_sums :: proc(cm: ^Crop_Module, startAtStage, endAtInclStage: int) -> f64 {
	ts := 0.0
	endAtInclStage2 :=
		endAtInclStage < 0 ? f64(cm.noOfDevStages + endAtInclStage + 1) : f64(endAtInclStage)
	for s := startAtStage; f64(s) < endAtInclStage2; s += 1 {
		ts += cm.cropParams.cultivarParams.pc_StageTemperatureSum[s]
	}
	return ts
}

// C++: void monica::cropmodule::fcRadiation(CropModule*, double julianDay,
// double globalRadiation, double sunshineHours)
//
// Taken from the original HERMES model - a separate, independent
// implementation of day-length/declination/radiation from soilmoisture.odin's
// own copy (soilmoisture computes its own for a different consumer); not
// deduplicated, matching the C++.
fc_radiation :: proc(cm: ^Crop_Module, julianDay, globalRadiation, sunshineHours: f64) {
	vs_Latitude := cm.siteParams.vs_Latitude

	PI :: 3.14159265358979323

	// Calculation of declination - old DEC
	cm.vc_Declination = -23.4 * libc.cos(2.0 * PI * ((julianDay + 10.0) / 365.0))

	vc_DeclinationSinus :=
		libc.sin(cm.vc_Declination * PI / 180.0) * libc.sin(vs_Latitude * PI / 180.0) // old SINLD
	vc_DeclinationCosinus :=
		libc.cos(cm.vc_Declination * PI / 180.0) * libc.cos(vs_Latitude * PI / 180.0) // old COSLD

	// Calculation of the atmospheric day lenght - old DL
	arg_AstroDayLength := vc_DeclinationSinus / vc_DeclinationCosinus
	arg_AstroDayLength = tl.bound(-1.0, arg_AstroDayLength, 1.0)
	cm.vc_AstronomicDayLenght = 12.0 * (PI + 2.0 * libc.asin(arg_AstroDayLength)) / PI

	// Calculation of the effective day length - old DLE
	EDLHelper := (-libc.sin(f64(8.0 * PI / 180.0)) + vc_DeclinationSinus) / vc_DeclinationCosinus

	if EDLHelper < -1.0 || EDLHelper > 1.0 {
		cm.vc_EffectiveDayLength = 0.01
	} else {
		cm.vc_EffectiveDayLength = 12.0 * (PI + 2.0 * libc.asin(EDLHelper)) / PI
	}

	// old DLP
	arg_PhotoDayLength :=
		(-libc.sin(f64(-6.0 * PI / 180.0)) + vc_DeclinationSinus) / vc_DeclinationCosinus
	arg_PhotoDayLength = tl.bound(-1.0, arg_PhotoDayLength, 1.0)
	cm.vc_PhotoperiodicDaylength = 12.0 * (PI + 2.0 * libc.asin(arg_PhotoDayLength)) / PI

	// Calculation of the mean photosynthetically active radiation [J m-2] - old RDN
	arg_PhotAct := min(
		1.0,
		(vc_DeclinationSinus / vc_DeclinationCosinus) *
		(vc_DeclinationSinus / vc_DeclinationCosinus),
	)
	cm.vc_PhotActRadiationMean =
		3600.0 *
		(vc_DeclinationSinus * cm.vc_AstronomicDayLenght +
				24.0 / PI * vc_DeclinationCosinus * libc.sqrt(1.0 - arg_PhotAct))

	// Calculation of radiation on a clear day [J m-2] - old DRC
	if cm.vc_PhotActRadiationMean > 0 && cm.vc_AstronomicDayLenght > 0 {
		cm.vc_ClearDayRadiation =
			0.5 *
			1300.0 *
			cm.vc_PhotActRadiationMean *
			libc.exp(-0.14 / (cm.vc_PhotActRadiationMean / (cm.vc_AstronomicDayLenght * 3600.0)))
	} else {
		cm.vc_ClearDayRadiation = 0
	}

	// Calculation of radiation on an overcast day [J m-2] - old DRO
	cm.vc_OvercastDayRadiation = 0.2 * cm.vc_ClearDayRadiation

	// Calculation of extraterrestrial radiation - old EXT
	pc_SolarConstant := 0.082
	SC :=
		24.0 *
		60.0 /
		PI *
		pc_SolarConstant *
		(1.0 + 0.033 * libc.cos(2.0 * PI * julianDay / 365.0))

	arg_SolarAngle :=
		-libc.tan(vs_Latitude * PI / 180.0) * libc.tan(cm.vc_Declination * PI / 180.0)
	arg_SolarAngle = tl.bound(-1.0, arg_SolarAngle, 1.0)
	vc_SunsetSolarAngle := libc.acos(arg_SolarAngle)
	cm.vc_ExtraterrestrialRadiation =
		SC *
		(vc_SunsetSolarAngle * vc_DeclinationSinus +
				vc_DeclinationCosinus * libc.sin(vc_SunsetSolarAngle)) // [MJ m-2]

	if globalRadiation > 0.0 {
		cm.vc_GlobalRadiation = globalRadiation
	} else if cm.vc_AstronomicDayLenght > 0 {
		cm.vc_GlobalRadiation =
			cm.vc_ExtraterrestrialRadiation *
			(0.19 + 0.55 * sunshineHours / cm.vc_AstronomicDayLenght)
	} else {
		cm.vc_GlobalRadiation = 0
	}
}

// C++: double monica::cropmodule::fcDaylengthFactor(CropModule*, double
// daylengthRequirement, double effectiveDayLength, double
// photoperiodicDayLength, double baseDaylength)
fc_daylength_factor :: proc(
	cm: ^Crop_Module,
	daylengthRequirement, effectiveDayLength, photoperiodicDayLength, baseDaylength: f64,
) -> f64 {
	if daylengthRequirement > 0.0 {
		// Long-day plants: development acceleration by day length (day length
		// requirement is positive).
		cm.vc_DaylengthFactor =
			(photoperiodicDayLength - baseDaylength) / (daylengthRequirement - baseDaylength)
	} else if daylengthRequirement < 0.0 {
		// Short-day plants: development acceleration by night length (day
		// length requirement is negative and represents critical day length).
		vc_CriticalDayLenght := -daylengthRequirement
		vc_MaximumDayLength := -baseDaylength
		if effectiveDayLength <= vc_CriticalDayLenght {
			cm.vc_DaylengthFactor = 1.0
		} else {
			cm.vc_DaylengthFactor =
				(effectiveDayLength - vc_MaximumDayLength) /
				(vc_CriticalDayLenght - vc_MaximumDayLength)
		}
	} else {
		cm.vc_DaylengthFactor = 1.0
	}

	cm.vc_DaylengthFactor = max(0.0, min(cm.vc_DaylengthFactor, 1.0))

	return cm.vc_DaylengthFactor
}

// C++: pair<double,double> monica::cropmodule::fcVernalisationFactor(CropModule*,
// double meanAirTemperature, double vernalisationRequirement, double vernalisationDays)
fc_vernalisation_factor :: proc(
	cm: ^Crop_Module,
	meanAirTemperature, vernalisationRequirement, vernalisationDaysIn: f64,
) -> (
	f64,
	f64,
) {
	vernalisationDays := vernalisationDaysIn

	// see if for this crop the fix is requested else use the default one at
	// the crop module level
	enable_vernalisation_factor_fix, ok := cm.cropParams.__enable_vernalisation_factor_fix__.?
	if !ok {
		enable_vernalisation_factor_fix = cm.cropModParams.__enable_vernalisation_factor_fix__
	}
	vc_EffectiveVernalisation: f64

	if vernalisationRequirement == 0.0 {
		cm.vc_VernalisationFactor = 1.0
	} else {
		switch {
		case meanAirTemperature > -4.0 && meanAirTemperature <= 0.0:
			vc_EffectiveVernalisation = (meanAirTemperature + 4.0) / 4.0
		case meanAirTemperature > 0.0 && meanAirTemperature <= 3.0:
			vc_EffectiveVernalisation = 1.0
		case meanAirTemperature > 3.0 && meanAirTemperature <= 7.0:
			vc_EffectiveVernalisation = 1.0 - (0.2 * (meanAirTemperature - 3.0) / 4.0)
		case meanAirTemperature > 7.0 && meanAirTemperature <= 9.0:
			vc_EffectiveVernalisation = 0.8 - (0.4 * (meanAirTemperature - 7.0) / 2.0)
		case meanAirTemperature > 9.0 && meanAirTemperature <= 18.0:
			vc_EffectiveVernalisation = 0.4 - (0.4 * (meanAirTemperature - 9.0) / 9.0)
		case meanAirTemperature <= -4.0 || meanAirTemperature > 18.0:
			vc_EffectiveVernalisation = 0.0
		case:
			vc_EffectiveVernalisation = 1.0
		}

		// old VERNTAGE
		vernalisationDays += vc_EffectiveVernalisation * cm.vc_TimeStep

		// old VERSCHWELL
		vc_VernalisationThreshold := min(vernalisationRequirement, 9.0) - 1.0

		if vc_VernalisationThreshold >= 1 {
			cm.vc_VernalisationFactor =
				(vernalisationDays - vc_VernalisationThreshold) /
				(vernalisationRequirement - vc_VernalisationThreshold)

			if enable_vernalisation_factor_fix {
				cm.vc_VernalisationFactor = min(max(0.0, cm.vc_VernalisationFactor), 1.0)
			}
			if cm.vc_VernalisationFactor < 0 {
				cm.vc_VernalisationFactor = 0.0 // MP: Vernalisation kann nie negativ sein
			}
		} else {
			cm.vc_VernalisationFactor = 1.0
			// MP: Vernalisation hat keinen Effekt, wenn kein (bzw. 0 als)
			// Threshold definiert ist.
		}
	}

	return cm.vc_VernalisationFactor, vernalisationDays
}

// C++: double monica::cropmodule::fcOxygenDeficiency(CropModule*, double criticalOxygenContent)
fc_oxygen_deficiency :: proc(cm: ^Crop_Module, criticalOxygenContent: f64) -> f64 {
	timeUnderAnoxiaThreshold := cm.cropModParams.pc_TimeUnderAnoxiaThreshold
	soilColumn := cm.soilColumn
	timeUnderAnoxiaThresholdAtStage := cm.TimeUnderAnoxiaThresholdDefault
	if cm.vc_DevelopmentalStage < len(timeUnderAnoxiaThreshold) {
		timeUnderAnoxiaThresholdAtStage = timeUnderAnoxiaThreshold[cm.vc_DevelopmentalStage]
	}

	// Reduktion bei Luftmangel Stauwasser berücksichtigen!!!!
	sumSaturation := 0.0
	sumSoilMoisture := 0.0
	sumLayers := 0
	// MP: changed to consider at least first 30 cm and then rooting depth
	nols := min(max(3, cm.vc_RootingDepth), len(soilColumn.layers))
	for i := 0; i < nols; i += 1 {
		sumSaturation += soilColumn.layers[i].vs_Saturation
		sumSoilMoisture += soilColumn.layers[i].vs_SoilMoisture_m3
		sumLayers += 1
	}
	avgAirFilledPoreVolume := (sumSaturation - sumSoilMoisture) / f64(sumLayers)
	if avgAirFilledPoreVolume < criticalOxygenContent {
		// MP: conditions changed for stage-dependent waterlogging
		cm.vc_TimeUnderAnoxia += int(cm.vc_TimeStep)
		if cm.vc_TimeUnderAnoxia >= timeUnderAnoxiaThresholdAtStage {
			cm.vc_OxygenDeficit = max(0.0, avgAirFilledPoreVolume) / criticalOxygenContent
		}
	} else {
		cm.vc_TimeUnderAnoxia = 0
		cm.vc_OxygenDeficit = 1.0
	}
	return cm.vc_OxygenDeficit
}

// C++: double WangEngelTemperatureResponse(double, double, double, double,
// double) - a file-scope free function in crop-module.cpp, used only by
// fcCropDevelopmentalStage below.
@(private)
wang_engel_temperature_response :: proc(t, tmin, topt, tmax, betacoeff: f64) -> f64 {
	// MP: what is this beta coefficient?
	// prevent nan values with t < tmin
	if t < tmin || t > tmax {
		return 0.0
	}

	alfa := libc.log(f64(2.0)) / libc.log((tmax - tmin) / (topt - tmin))
	numerator :=
		2 * libc.pow(t - tmin, alfa) * libc.pow(topt - tmin, alfa) - libc.pow(t - tmin, 2 * alfa)
	denominator := libc.pow(topt - tmin, 2 * alfa)

	// MP: beta coefficient should be 2*alfa
	return libc.pow(numerator / denominator, betacoeff)
}

// C++: void monica::cropmodule::fcCropDevelopmentalStage(CropModule*, double
// meanAirTemperature, double soilMoisture_m3, double fieldCapacity, double
// permanentWiltingPoint, Tools::Date currentDate)
fc_crop_developmental_stage :: proc(
	cm: ^Crop_Module,
	meanAirTemperature, soilMoisture_m3, fieldCapacity, permanentWiltingPoint: f64,
	currentDate: d.Date,
	allocator := context.allocator,
) {
	cropPs := cm.cropModParams
	cultivarPs := &cm.cropParams.cultivarParams
	speciesPs := &cm.cropParams.speciesParams
	soilColumn := cm.soilColumn
	pc_AssimilatePartitioningCoeff := cm.cropParams.cultivarParams.pc_AssimilatePartitioningCoeff
	pc_BaseTemperature := cm.cropParams.speciesParams.pc_BaseTemperature
	pc_DevelopmentAccelerationByNitrogenStress :=
		cm.cropParams.speciesParams.pc_DevelopmentAccelerationByNitrogenStress
	pc_DroughtStressThreshold := cm.cropParams.cultivarParams.pc_DroughtStressThreshold
	pc_EmergenceFloodingControlOn := cm.simParams.pc_EmergenceFloodingControlOn
	pc_EmergenceMoistureControlOn := cm.simParams.pc_EmergenceMoistureControlOn
	pc_OptimumTemperature := cm.cropParams.cultivarParams.pc_OptimumTemperature
	pc_Perennial := cm.cropParams.cultivarParams.pc_Perennial
	pc_StageTemperatureSum := cm.cropParams.cultivarParams.pc_StageTemperatureSum

	if cm.vc_DevelopmentalStage == 0 {
		if pc_Perennial { 	// pc_Perennial == true
			if meanAirTemperature > pc_BaseTemperature[cm.vc_DevelopmentalStage] {
				tempIncr :=
					(min(meanAirTemperature, pc_OptimumTemperature[cm.vc_DevelopmentalStage]) -
						pc_BaseTemperature[cm.vc_DevelopmentalStage]) *
					cm.vc_VernalisationFactor *
					cm.vc_DaylengthFactor *
					cm.vc_TimeStep
				cm.vc_CurrentTemperatureSum[cm.vc_DevelopmentalStage] += tempIncr
				cm.vc_CurrentTotalTemperatureSum += tempIncr
			}

			// @todo: shouldn't vc_CurrentTemperatureSum be reduces by the
			// excess temperature like further below?
			if cm.vc_CurrentTemperatureSum[cm.vc_DevelopmentalStage] >=
			   pc_StageTemperatureSum[cm.vc_DevelopmentalStage] {
				if cm.vc_DevelopmentalStage < cm.noOfDevStages - 1 {
					cm.vc_DevelopmentalStage += 1
				}
			}
		} else { 	// pc_Perennial == false
			vc_SoilTemperature := soilColumn.layers[0].vs_SoilTemperature // MP: Bodentemperatur der ersten 10cm
			if vc_SoilTemperature > pc_BaseTemperature[cm.vc_DevelopmentalStage] {
				emergenceCondition := true
				// Germination only if soil water content in top layer exceeds
				// 20% of capillary water, but is not beyond field capacity
				if pc_EmergenceMoistureControlOn {
					vc_CapillaryWater := fieldCapacity - permanentWiltingPoint
					emergenceCondition =
						emergenceCondition &&
						soilMoisture_m3 > ((0.2 * vc_CapillaryWater) + permanentWiltingPoint) &&
						soilMoisture_m3 <= fieldCapacity
				}
				// Germination only if no water is stored on the soil surface.
				if pc_EmergenceFloodingControlOn {
					emergenceCondition =
						emergenceCondition && soilColumn.vs_SurfaceWaterStorage < 0.001
				}

				if emergenceCondition {
					cm.vc_CurrentTemperatureSum[cm.vc_DevelopmentalStage] +=
						(vc_SoilTemperature - pc_BaseTemperature[cm.vc_DevelopmentalStage]) *
						cm.vc_TimeStep

					if cm.vc_CurrentTemperatureSum[cm.vc_DevelopmentalStage] >=
					   pc_StageTemperatureSum[cm.vc_DevelopmentalStage] {
						vc_StageExcessTemperatureSum :=
							cm.vc_CurrentTemperatureSum[cm.vc_DevelopmentalStage] -
							pc_StageTemperatureSum[cm.vc_DevelopmentalStage]
						if cm.vc_DevelopmentalStage < cm.noOfDevStages - 1 {
							cm.vc_DevelopmentalStage += 1
							cm.vc_CurrentTemperatureSum[cm.vc_DevelopmentalStage] +=
								vc_StageExcessTemperatureSum
						}
					}
				}
			}
		}
	} else if cm.vc_DevelopmentalStage > 0 {
		// MP: wenn die Frucht aufgegangen ist, können N- und Wasser-Stress zum
		// Tragen kommen (nur während der Kornfüllungsphase --> schnelleres Abreifen)
		apc := pc_AssimilatePartitioningCoeff[cm.vc_DevelopmentalStage][cm.vc_StorageOrgan]

		// Development acceleration by N deficit in crop tissue
		vc_DevelopmentAccelerationByNitrogenStress := 1.0 // old NPROG
		if pc_DevelopmentAccelerationByNitrogenStress == 1 && apc > 0.9 {
			vc_DevelopmentAccelerationByNitrogenStress =
				1.0 + ((1.0 - cm.vc_CropNRedux) * (1.0 - cm.vc_CropNRedux))
		}

		// Development acceleration by water deficit
		vc_DevelopmentAccelerationByWaterStress := 1.0 // old WPROG
		if cm.vc_TranspirationDeficit < pc_DroughtStressThreshold[cm.vc_DevelopmentalStage] &&
		   apc > 0.9 {
			// Das heißt, das betrifft nur die Kornfüllungsphase (acp>0.9).
			if cm.vc_OxygenDeficit >= 1.0 {
				vc_DevelopmentAccelerationByWaterStress =
					1.0 + ((1.0 - cm.vc_TranspirationDeficit) * (1.0 - cm.vc_TranspirationDeficit))
			}
		}

		// old DEVPROG
		vc_DevelopmentAccelerationByStress := max(
			vc_DevelopmentAccelerationByNitrogenStress,
			vc_DevelopmentAccelerationByWaterStress,
		)

		if cropPs.__enable_Phenology_WangEngelTemperatureResponse__ {
			devTresponse := max(
				0.0,
				wang_engel_temperature_response(
					meanAirTemperature,
					cultivarPs.pc_MinTempDev_WE,
					cultivarPs.pc_OptTempDev_WE,
					cultivarPs.pc_MaxTempDev_WE,
					1.0,
				), // MP: warum steht hier 1?
			)
			tempIncr :=
				devTresponse *
				meanAirTemperature *
				cm.vc_VernalisationFactor *
				cm.vc_DaylengthFactor *
				vc_DevelopmentAccelerationByStress *
				cm.vc_TimeStep
			cm.vc_CurrentTemperatureSum[cm.vc_DevelopmentalStage] += tempIncr
			cm.vc_CurrentTotalTemperatureSum += tempIncr
		} else {
			if meanAirTemperature > pc_BaseTemperature[cm.vc_DevelopmentalStage] {
				tempIncr :=
					(min(meanAirTemperature, pc_OptimumTemperature[cm.vc_DevelopmentalStage]) -
						pc_BaseTemperature[cm.vc_DevelopmentalStage]) *
					cm.vc_VernalisationFactor *
					cm.vc_DaylengthFactor *
					vc_DevelopmentAccelerationByStress *
					cm.vc_TimeStep
				cm.vc_CurrentTemperatureSum[cm.vc_DevelopmentalStage] += tempIncr // MP: effektive Temperatur wird aufsummiert
				cm.vc_CurrentTotalTemperatureSum += tempIncr
			}
		}

		doResetPerennialCrop :=
			pc_Perennial &&
			speciesPs.dormancyStartDoy > 0 &&
			int(d.day_of_year(currentDate)) >= speciesPs.dormancyStartDoy
		if cm.vc_CurrentTemperatureSum[cm.vc_DevelopmentalStage] >=
		   pc_StageTemperatureSum[cm.vc_DevelopmentalStage] {
			if cm.vc_DevelopmentalStage < cm.noOfDevStages - 1 {
				stageExcessTemperatureSum :=
					cm.vc_CurrentTemperatureSum[cm.vc_DevelopmentalStage] -
					pc_StageTemperatureSum[cm.vc_DevelopmentalStage]
				cm.vc_DevelopmentalStage += 1
				cm.vc_CurrentTemperatureSum[cm.vc_DevelopmentalStage] += stageExcessTemperatureSum
			} else if cm.vc_DevelopmentalStage == cm.noOfDevStages - 1 { 	// MP: Frucht ist reif
				if pc_Perennial && cm.vc_GrowthCycleEnded {
					doResetPerennialCrop = true
				}
			}
		}
		if doResetPerennialCrop {
			cm.vc_DevelopmentalStage = 0
			fc_update_crop_parameters_for_perennial(cm, allocator)
			for stage := 0; stage < cm.noOfDevStages; stage += 1 {
				cm.vc_CurrentTemperatureSum[stage] = 0.0
			}
			cm.vc_CurrentTotalTemperatureSum = 0.0
			cm.vc_GrowthCycleEnded = false
			if speciesPs.dormancyEndDoy == 0 {
				cm.perennialCropDormancyPeriodEndDate = currentDate
			} else {
				yearDelta := 0
				if int(d.day_of_year(currentDate)) > speciesPs.dormancyEndDoy {
					yearDelta = 1
				}
				cm.perennialCropDormancyPeriodEndDate = d.add(
					d.make_date(
						1,
						1,
						u16(d.year(currentDate) + yearDelta),
						false,
						false,
						d.DEFAULT_USE_LEAP_YEARS,
					),
					u64(speciesPs.dormancyEndDoy - 1),
				)
			}
		}
	} else {
		cm.vc_ErrorStatus = true
		cm.vc_ErrorMessage = "irregular developmental stage"
	}
}

// C++: double monica::cropmodule::fcKcFactor(const CropModule*, double
// d_StageTemperatureSum, double d_CurrentTemperatureSum, double
// d_StageKcFactor, double d_EarlierStageKcFactor)
fc_kc_factor :: proc(
	cm: ^Crop_Module,
	d_StageTemperatureSum, d_CurrentTemperatureSum, d_StageKcFactor, d_EarlierStageKcFactor: f64,
) -> f64 {
	pc_InitialKcFactor := cm.cropParams.speciesParams.pc_InitialKcFactor
	vc_RelativeDevelopment := 0.0
	if d_StageTemperatureSum > 0.0 {
		vc_RelativeDevelopment = min(d_CurrentTemperatureSum / d_StageTemperatureSum, 1.0) // old relint
	}

	if cm.vc_DevelopmentalStage == 0 {
		return pc_InitialKcFactor + (d_StageKcFactor - pc_InitialKcFactor) * vc_RelativeDevelopment
	} else {
		// Interpolating the Kc Factors
		return(
			d_EarlierStageKcFactor +
			((d_StageKcFactor - d_EarlierStageKcFactor) * vc_RelativeDevelopment) \
		)
	}
}

// C++: void monica::cropmodule::fcCropSize(CropModule*, double maxCropHeight)
fc_crop_size :: proc(cm: ^Crop_Module, maxCropHeight: f64) {
	pc_StageAtMaxHeight := cm.cropParams.speciesParams.pc_StageAtMaxHeight
	pc_StageTemperatureSum := cm.cropParams.cultivarParams.pc_StageTemperatureSum
	pc_CropHeightP1 := cm.cropParams.cultivarParams.pc_CropHeightP1
	pc_CropHeightP2 := cm.cropParams.cultivarParams.pc_CropHeightP2
	pc_StageAtMaxDiameter := cm.cropParams.speciesParams.pc_StageAtMaxDiameter
	pc_MaxCropDiameter := cm.cropParams.speciesParams.pc_MaxCropDiameter

	vc_TotalTemperatureSumForHeight := 0.0
	for stage := 1; f64(stage) < pc_StageAtMaxHeight + 1; stage += 1 {
		vc_TotalTemperatureSumForHeight += pc_StageTemperatureSum[stage]
	}
	vc_RelativeTotalDevelopmentForHeight := min(
		cm.vc_CurrentTotalTemperatureSum / vc_TotalTemperatureSumForHeight,
		1.0,
	)
	if vc_RelativeTotalDevelopmentForHeight > 0.0 {
		cm.vc_CropHeight =
			maxCropHeight /
			(1.0 +
					libc.exp(
						-pc_CropHeightP1 *
						(vc_RelativeTotalDevelopmentForHeight - pc_CropHeightP2),
					))
	} else {
		cm.vc_CropHeight = 0.0
	}

	vc_TotalTemperatureSumForDiameter := 0.0
	for stage := 1; f64(stage) < pc_StageAtMaxDiameter + 1; stage += 1 {
		vc_TotalTemperatureSumForDiameter += pc_StageTemperatureSum[stage]
	}
	vc_RelativeTotalDevelopmentForDiameter := min(
		cm.vc_CurrentTotalTemperatureSum / vc_TotalTemperatureSumForDiameter,
		1.0,
	)
	if vc_RelativeTotalDevelopmentForDiameter > 0.0 {
		cm.vc_CropDiameter = pc_MaxCropDiameter * vc_RelativeTotalDevelopmentForDiameter
	} else {
		cm.vc_CropDiameter = 0.0
	}
}

// C++: void monica::cropmodule::fcCropGreenArea(CropModule*, double
// vw_MeanAirTemperature, double d_LeafBiomassIncrement, double
// d_LeafBiomassDecrement, double d_SpecificLeafAreaStart, double
// d_SpecificLeafAreaEnd, double d_SpecificLeafAreaEarly, double
// d_StageTemperatureSum, double d_CurrentTemperatureSum)
fc_crop_green_area :: proc(
	cm: ^Crop_Module,
	vw_MeanAirTemperature, d_LeafBiomassIncrement, d_LeafBiomassDecrement: f64,
	d_SpecificLeafAreaStart, d_SpecificLeafAreaEnd, d_SpecificLeafAreaEarly: f64,
	d_StageTemperatureSum, d_CurrentTemperatureSum: f64,
) {
	cropPs := cm.cropModParams
	speciesPs := &cm.cropParams.speciesParams
	cultivarPs := &cm.cropParams.cultivarParams
	pc_PlantDensity := cm.cropParams.speciesParams.pc_PlantDensity

	TempResponseExpansion := 1.0
	if cropPs.__enable_T_response_leaf_expansion__ {
		// Stage switch T response leaf exp (wheat = 2, maize = -1 (deactivated))
		if cm.vc_DevelopmentalStage + 1 <= speciesPs.pc_TransitionStageLeafExp {
			// Early stages leaf expansion T response
			referenceTempResponseExpansion :=
				223.9 * libc.exp(-5.03 * libc.exp(-0.0653 * cultivarPs.pc_EarlyRefLeafExp))
			TempResponseExpansion = min(
				223.9 *
				libc.exp(-5.03 * libc.exp(-0.0653 * vw_MeanAirTemperature)) /
				referenceTempResponseExpansion,
				1.3,
			)
		} else {
			// leaf expansion T response
			referenceTempResponseExpansion :=
				37.7 * libc.exp(-7.23 * libc.exp(-0.1462 * cultivarPs.pc_RefLeafExp))
			TempResponseExpansion = min(
				37.7 *
				libc.exp(-7.23 * libc.exp(-0.1462 * vw_MeanAirTemperature)) /
				referenceTempResponseExpansion,
				1.3,
			)
		}
	}

	cm.vc_LeafAreaIndex +=
		(d_LeafBiomassIncrement *
			TempResponseExpansion *
			(d_SpecificLeafAreaStart +
					(d_CurrentTemperatureSum /
							d_StageTemperatureSum *
							(d_SpecificLeafAreaEnd - d_SpecificLeafAreaStart))) *
			cm.vc_TimeStep) -
		(d_LeafBiomassDecrement * d_SpecificLeafAreaEarly * cm.vc_TimeStep) // [ha ha-1]

	if cm.vc_LeafAreaIndex <= 0.0 {
		cm.vc_LeafAreaIndex = 0.001
	}
	PI :: 3.14159265358979323
	cm.vc_GreenAreaIndex =
		cm.vc_LeafAreaIndex + (cm.vc_CropHeight * PI * cm.vc_CropDiameter * f64(pc_PlantDensity)) // [m2 m-2]
}

// C++: double monica::cropmodule::fcSoilCoverage(const CropModule*)
fc_soil_coverage :: proc(cm: ^Crop_Module) -> f64 {
	return 1.0 - libc.exp(-0.5 * cm.vc_LeafAreaIndex)
}

// C++: void monica::cropmodule::fcUpdateCropParametersForPerennial(CropModule*)
fc_update_crop_parameters_for_perennial :: proc(cm: ^Crop_Module, allocator := context.allocator) {
	if cm.perennialCropParams == nil {
		return
	}
	cm.cropParams = clone_crop_parameters(cm.perennialCropParams^, allocator)
	cm.noOfOrgans = p.species_parameters_number_of_organs(&cm.cropParams.speciesParams)
	cm.noOfDevStages = p.species_parameters_number_of_developmental_stages(
		&cm.cropParams.speciesParams,
	)
}

// C++: bool monica::cropmodule::isAnthesisDay(const CropModule*, size_t
// old_dev_stage, size_t new_dev_stage)
//
// Method is called after calculation of the developmental stage.
is_anthesis_day :: proc(cm: ^Crop_Module, old_dev_stage, new_dev_stage: int) -> bool {
	a, b := anthesis_between_stages(cm)
	return a == old_dev_stage && b == new_dev_stage
}

// C++: kj::Tuple<int,int> monica::cropmodule::anthesisBetweenStages(const CropModule*)
anthesis_between_stages :: proc(cm: ^Crop_Module) -> (int, int) {
	if cm.noOfDevStages == 6 {
		return 3, 4
	} else if cm.noOfDevStages == 7 {
		return 4, 5
	}
	return -1, -1
}

// C++: bool monica::cropmodule::isMaturityDay(const CropModule*, size_t
// old_dev_stage, size_t new_dev_stage)
//
// Method is called after calculation of the developmental stage.
is_maturity_day :: proc(cm: ^Crop_Module, old_dev_stage, new_dev_stage: int) -> bool {
	// corn crops
	if cm.noOfDevStages == 6 {
		return old_dev_stage == 4 && new_dev_stage == 5
		// maize, sorghum and other crops with 7 developmental stages
	} else if cm.noOfDevStages == 7 {
		return old_dev_stage == 5 && new_dev_stage == 6
	}

	return false
}

// C++: int monica::cropmodule::getAnthesisDay(const CropModule*)
get_anthesis_day :: proc(cm: ^Crop_Module) -> int {
	return cm.vc_AnthesisDay
}

// C++: int monica::cropmodule::getMaturityDay(const CropModule*)
get_maturity_day :: proc(cm: ^Crop_Module) -> int {
	return cm.vc_MaturityDay
}

// C++: bool monica::cropmodule::maturityReached(const CropModule*)
maturity_reached :: proc(cm: ^Crop_Module) -> bool {
	return cm.vc_MaturityReached
}

// C++: void monica::cropmodule::setStage(CropModule*, size_t newStage)
set_stage :: proc(cm: ^Crop_Module, newStage: int) {
	cm.vc_CurrentTotalTemperatureSum = 0.0
	for stage := 0; stage < cm.noOfDevStages; stage += 1 {
		if stage < newStage {
			cm.vc_CurrentTotalTemperatureSum += cm.vc_CurrentTemperatureSum[stage]
		} else {
			cm.vc_CurrentTemperatureSum[stage] = 0.0
		}
	}

	cm.vc_DevelopmentalStage = newStage
}

// ---------------------------------------------------------------------------
// Phase 5 checkpoint 4: photosynthesis + assimilation
// ---------------------------------------------------------------------------

// C++: void monica::cropmodule::fcCropPhotosynthesis(CropModule*, double
// vw_MeanAirTemperature, double vw_MaxAirTemperature, double
// vw_MinAirTemperature, double vw_AtmosphericCO2Concentration, double
// vw_AtmosphericO3Concentration, Tools::Date currentDate)
//
// The largest function in the port (~1100 lines in C++). Two departures from
// a literal transliteration, both because the eliminated code is provably
// dead or undefined, not because it's inconvenient to port:
//   - the Intercropping branch of the C++ (a second, alternate call to its
//     `code` lambda with a different fraction-of-intercepted-radiation
//     function, gated on `intercroppingOtherCropHeight > zeroHeightEps`) is
//     unreachable in this port: Intercropping itself is a dropped feature
//     (Cap'n Proto RPC, see plan-odin.md's "Explicitly dropped" table), and
//     `intercroppingOtherCropHeight` starts at -1 and nothing in this port
//     ever sets it positive (setOtherCropHeightAndLAIt is only ever called by
//     intercropping wiring). With only one surviving call to `code`, its body
//     is inlined directly rather than reproduced as a `std::function`-taking
//     closure (Odin's `proc` type has no capture).
//   - the hourly sunrise-detection check reads C++'s `hourlyGlobrads.back()`
//     on a still-empty, freshly-constructed `std::vector` at hour 0 -
//     undefined behaviour (likely a null-pointer dereference), not a
//     reproducible quirk. Reimplemented as an explicit "previous hour"
//     tracker seeded at 0.0, which is what the logic clearly intends: "is
//     this the first hour with positive radiation".
fc_crop_photosynthesis :: proc(
	cm: ^Crop_Module,
	vw_MeanAirTemperature, vw_MaxAirTemperature, vw_MinAirTemperature: f64,
	vw_AtmosphericCO2Concentration, vw_AtmosphericO3Concentration: f64,
	currentDate: d.Date,
	allocator := context.allocator,
) {
	cropPs := cm.cropModParams
	soilColumn := cm.soilColumn
	speciesPs := &cm.cropParams.speciesParams
	cultivarPs := &cm.cropParams.cultivarParams
	pc_AssimilatePartitioningCoeff := cm.cropParams.cultivarParams.pc_AssimilatePartitioningCoeff
	pc_CarboxylationPathway := cm.cropParams.speciesParams.pc_CarboxylationPathway
	pc_DefaultRadiationUseEfficiency :=
		cm.cropParams.speciesParams.pc_DefaultRadiationUseEfficiency
	pc_DroughtStressThresholdArr := cm.cropParams.cultivarParams.pc_DroughtStressThreshold
	pc_FieldConditionModifier := cm.cropParams.speciesParams.pc_FieldConditionModifier
	pc_GrowthRespirationParameter_2 := cm.cropModParams.pc_GrowthRespirationParameter2
	pc_MaxAssimilationRate := cm.cropParams.cultivarParams.pc_MaxAssimilationRate
	pc_MinimumTemperatureForAssimilation :=
		cm.cropParams.speciesParams.pc_MinimumTemperatureForAssimilation
	pc_MaximumTemperatureForAssimilation :=
		cm.cropParams.speciesParams.pc_MaximumTemperatureForAssimilation
	pc_OrganGrowthRespiration := cm.cropParams.speciesParams.pc_OrganGrowthRespiration
	pc_OrganMaintenanceRespiration := cm.cropParams.speciesParams.pc_OrganMaintenanceRespiration
	pc_OptimumTemperatureForAssimilation :=
		cm.cropParams.speciesParams.pc_OptimumTemperatureForAssimilation
	pc_SpecificLeafArea := cm.cropParams.cultivarParams.pc_SpecificLeafArea
	pc_WaterDeficitResponseOn := cm.simParams.pc_WaterDeficitResponseOn
	vs_Latitude := cm.siteParams.vs_Latitude

	vc_AssimilationRateReference := 0.0

	pc_ReferenceLeafAreaIndex := cropPs.pc_ReferenceLeafAreaIndex
	pc_ReferenceMaxAssimilationRate := cropPs.pc_ReferenceMaxAssimilationRate
	pc_MaintenanceRespirationParameter_1 := cropPs.pc_MaintenanceRespirationParameter1
	pc_MaintenanceRespirationParameter_2 := cropPs.pc_MaintenanceRespirationParameter2

	pc_GrowthRespirationParameter_1 := cropPs.pc_GrowthRespirationParameter1
	pc_CanopyReflectionCoeff := cropPs.pc_CanopyReflectionCoefficient // old REFLC

	vc_RadiationUseEfficiency := pc_DefaultRadiationUseEfficiency
	vc_RadiationUseEfficiencyReference := pc_DefaultRadiationUseEfficiency

	D_IN_K :: VOC_D_IN_K
	RGAS :: VOC_RGAS
	TK25 :: VOC_TK25

	if pc_CarboxylationPathway == 1 {
		// Calculation of CO2 impact on crop growth
		if cm.pc_CO2Method == 3 {
			// Long 1991 / Mitchell et al. 1995
			tempK := vw_MeanAirTemperature + D_IN_K
			term1 := (tempK - TK25) / (TK25 * tempK * RGAS)
			term2 := libc.sqrt(tempK / TK25)
			cm.vc_KTkc = libc.exp(speciesPs.AEKC * term1) * term2
			cm.vc_KTko = libc.exp(speciesPs.AEKO * term1) * term2
			Mkc := speciesPs.KC25 * cm.vc_KTkc // [umol mol-1]
			cm.cropPhotosynthesisResults.kc = Mkc
			Mko := speciesPs.KO25 * cm.vc_KTko // [mmol mol-1]
			cm.cropPhotosynthesisResults.ko = Mko * 1000.0 // mmol -> umol

			// OLD exponential response
			KTvmax: f64
			if cropPs.__enable_Photosynthesis_WangEngelTemperatureResponse__ {
				KTvmax = max(
					0.00001,
					wang_engel_temperature_response(
						vw_MeanAirTemperature,
						pc_MinimumTemperatureForAssimilation,
						pc_OptimumTemperatureForAssimilation,
						pc_MaximumTemperatureForAssimilation,
						1.0,
					),
				)
			} else {
				KTvmax = libc.exp(speciesPs.AEVC * term1) * term2
			}

			// Berechnung des Transformationsfaktors fuer pflanzenspez. AMAX bei
			// 25 grad - old fakamax
			vc_AmaxFactor := pc_MaxAssimilationRate / 34.668
			vc_AmaxFactorReference := pc_ReferenceMaxAssimilationRate / 34.668
			// old vcmax
			vc_Vcmax := 98.0 * vc_AmaxFactor * KTvmax
			cm.cropPhotosynthesisResults.vcMax = vc_Vcmax
			vc_VcmaxReference := 98.0 * vc_AmaxFactorReference * KTvmax

			Oi :=
				210.0 *
				(0.047 -
						0.0013087 * vw_MeanAirTemperature +
						0.000025603 * (vw_MeanAirTemperature * vw_MeanAirTemperature) -
						0.00000021441 *
							(vw_MeanAirTemperature *
									vw_MeanAirTemperature *
									vw_MeanAirTemperature)) /
				0.026934 // [mmol mol-1]
			cm.cropPhotosynthesisResults.oi = Oi * 1000.0 // mmol -> umol

			Ci :=
				vw_AtmosphericCO2Concentration *
				0.7 *
				(1.674 -
						0.061294 * vw_MeanAirTemperature +
						0.0011688 * (vw_MeanAirTemperature * vw_MeanAirTemperature) -
						0.0000088741 *
							(vw_MeanAirTemperature *
									vw_MeanAirTemperature *
									vw_MeanAirTemperature)) /
				0.73547 // [umol mol-1]
			cm.cropPhotosynthesisResults.ci = Ci

			// similar to LDNDC::jarvis.cpp:217 - old COcomp
			vc_CO2CompensationPoint := 0.5 * 0.21 * vc_Vcmax * Mkc * Oi / (vc_Vcmax * Mko) // [umol mol-1]
			vc_CO2CompensationPointReference :=
				0.5 * 0.21 * vc_VcmaxReference * Mkc * Oi / (vc_VcmaxReference * Mko) // [umol mol-1]
			cm.cropPhotosynthesisResults.comp = vc_CO2CompensationPoint

			// Mitchell et al. 1995 - old EFF
			vc_RadiationUseEfficiency = max(
				0.0,
				min(
					0.77 /
					2.1 *
					(Ci - vc_CO2CompensationPoint) /
					(4.5 * Ci + 10.5 * vc_CO2CompensationPoint) *
					8.3769,
					0.5,
				),
			)
			vc_RadiationUseEfficiencyReference = max(
				0.0,
				min(
					0.77 /
					2.1 *
					(Ci - vc_CO2CompensationPointReference) /
					(4.5 * Ci + 10.5 * vc_CO2CompensationPointReference) *
					8.3769,
					0.5,
				),
			)

			cm.vc_AssimilationRate =
				(Ci - vc_CO2CompensationPoint) * vc_Vcmax / (Ci + Mkc * (1.0 + Oi / Mko)) * 1.656
			vc_AssimilationRateReference =
				(Ci - vc_CO2CompensationPointReference) *
				vc_VcmaxReference /
				(Ci + Mkc * (1.0 + Oi / Mko)) *
				1.656

			if vw_MeanAirTemperature < pc_MinimumTemperatureForAssimilation {
				cm.vc_AssimilationRate = 0.0 // MP: warum gibt es fuer C3-Pflanzen keine maximale Temperatur
				vc_AssimilationRateReference = 0.0
			}
		} else if cm.pc_CO2Method == 2 {
			// Hoffmann 1995
			t_response := wang_engel_temperature_response(
				vw_MeanAirTemperature,
				pc_MinimumTemperatureForAssimilation,
				pc_OptimumTemperatureForAssimilation,
				pc_MaximumTemperatureForAssimilation,
				1.0,
			)

			cm.vc_AssimilationRate = pc_MaxAssimilationRate * t_response
			vc_AssimilationRateReference = pc_ReferenceMaxAssimilationRate * t_response

			// old KCo1
			vc_HoffmannK1 := 220.0 + 0.158 * (cm.vc_GlobalRadiation * 86400.0 / 1000000.0)
			// old coco
			vc_HoffmannC0 := 80.0 - 0.036 * (cm.vc_GlobalRadiation * 86400.0 / 1000000.0)
			// old KCO2
			vc_HoffmannKCO2 :=
				((vw_AtmosphericCO2Concentration - vc_HoffmannC0) /
					(vc_HoffmannK1 + vw_AtmosphericCO2Concentration - vc_HoffmannC0)) /
				((350.0 - vc_HoffmannC0) / (vc_HoffmannK1 + 350.0 - vc_HoffmannC0))

			cm.vc_AssimilationRate = cm.vc_AssimilationRate * vc_HoffmannKCO2
			vc_AssimilationRateReference = vc_AssimilationRateReference * vc_HoffmannKCO2
		}
	} else { 	// pc_CarboxylationPathway == 2
		t_response := wang_engel_temperature_response(
			vw_MeanAirTemperature,
			pc_MinimumTemperatureForAssimilation,
			pc_OptimumTemperatureForAssimilation,
			pc_MaximumTemperatureForAssimilation,
			1.0,
		)

		cm.vc_AssimilationRate = pc_MaxAssimilationRate * t_response
		vc_AssimilationRateReference = pc_ReferenceMaxAssimilationRate * t_response
	}

	if cm.vc_CuttingDelayDays > 0 {
		cm.vc_AssimilationRate = 0.1
	}

	cm.vc_AssimilationRate = max(0.1, cm.vc_AssimilationRate)
	vc_AssimilationRateReference = max(0.1, vc_AssimilationRateReference)

	// ---------------------------------------------------------------------
	// Calculation of light interception in the crop (Penning De Vries & van
	// Laar 1982)
	// ---------------------------------------------------------------------

	PI :: 3.14159265358979323

	// old EFFE
	vc_NetRadiationUseEfficiency := (1.0 - pc_CanopyReflectionCoeff) * vc_RadiationUseEfficiency
	vc_NetRadiationUseEfficiencyReference :=
		(1.0 - pc_CanopyReflectionCoeff) * vc_RadiationUseEfficiencyReference

	SSLAE := libc.sin((90.0 + cm.vc_Declination - vs_Latitude) * PI / 180.0) // = HERMES

	X := libc.log(
		1.0 +
		0.45 *
			cm.vc_ClearDayRadiation /
			(cm.vc_EffectiveDayLength * 3600.0) *
			vc_NetRadiationUseEfficiency /
			(SSLAE * cm.vc_AssimilationRate),
	) // = HERMES
	XReference := libc.log(
		1.0 +
		0.45 *
			cm.vc_ClearDayRadiation /
			(cm.vc_EffectiveDayLength * 3600.0) *
			vc_NetRadiationUseEfficiencyReference /
			(SSLAE * vc_AssimilationRateReference),
	)

	PHCH1 := SSLAE * cm.vc_AssimilationRate * cm.vc_EffectiveDayLength * X / (1.0 + X) // = HERMES
	PHCH1Reference :=
		SSLAE *
		vc_AssimilationRateReference *
		cm.vc_EffectiveDayLength *
		XReference /
		(1.0 + XReference)

	Y := libc.log(
		1.0 +
		0.55 *
			cm.vc_ClearDayRadiation /
			(cm.vc_EffectiveDayLength * 3600.0) *
			vc_NetRadiationUseEfficiency /
			((5.0 - SSLAE) * cm.vc_AssimilationRate),
	) // = HERMES
	YReference := libc.log(
		1.0 +
		0.55 *
			cm.vc_ClearDayRadiation /
			(cm.vc_EffectiveDayLength * 3600.0) *
			vc_NetRadiationUseEfficiency /
			((5.0 - SSLAE) * vc_AssimilationRateReference),
	)

	PHCH2 := (5.0 - SSLAE) * cm.vc_AssimilationRate * cm.vc_EffectiveDayLength * Y / (1.0 + Y) // = HERMES
	PHCH2Reference :=
		(5.0 - SSLAE) *
		vc_AssimilationRateReference *
		cm.vc_EffectiveDayLength *
		YReference /
		(1.0 + YReference)

	PHCH := 0.95 * (PHCH1 + PHCH2) + 20.5 // = HERMES
	PHCHReference := 0.95 * (PHCH1Reference + PHCH2Reference) + 20.5

	// vc_OxygenDeficit separates drought stress (ETa/Etp) from saturation
	// stress - old VSWELL
	vc_DroughtStressThreshold :=
		cm.vc_OxygenDeficit < 1.0 ? 0.0 : pc_DroughtStressThresholdArr[cm.vc_DevelopmentalStage]

	// Calculation of time fraction for overcast sky situations by comparing
	// clear day radiation and measured PAR in [J m-2]. HERMES uses PAR as 50%
	// of global radiation - old FOV
	vc_OvercastSkyTimeFraction := 0.0
	if cm.vc_ClearDayRadiation != 0 {
		vc_OvercastSkyTimeFraction =
			(cm.vc_ClearDayRadiation - (1000000.0 * cm.vc_GlobalRadiation * 0.50)) /
			(0.8 * cm.vc_ClearDayRadiation)
	}
	vc_OvercastSkyTimeFraction = max(0.0, min(vc_OvercastSkyTimeFraction, 1.0))

	// C++'s `code` lambda inlined at its one surviving call site (see the
	// function comment above) - only ever invoked with F_t1/vc_LeafAreaIndex.
	LAI := cm.vc_LeafAreaIndex
	fractionOfInterceptedRadiation :=
		1.0 - libc.exp(-cultivarPs.pc_LightExtinctionCoefficient * LAI)

	PHC3 := PHCH * fractionOfInterceptedRadiation
	PHC3Reference :=
		PHCHReference *
		(1.0 - libc.exp(-cultivarPs.pc_LightExtinctionCoefficient * pc_ReferenceLeafAreaIndex))

	PHC4 := cm.vc_AstronomicDayLenght * LAI * cm.vc_AssimilationRate
	PHC4Reference :=
		cm.vc_AstronomicDayLenght * pc_ReferenceLeafAreaIndex * vc_AssimilationRateReference

	PHCL :=
		PHC3 < PHC4 ? PHC3 * (1.0 - libc.exp(-PHC4 / PHC3)) : PHC4 * (1.0 - libc.exp(-PHC3 / PHC4))

	PHCLReference :=
		PHC3Reference < PHC4Reference ? PHC3Reference * (1.0 - libc.exp(-PHC4Reference / PHC3Reference)) : PHC4Reference * (1.0 - libc.exp(-PHC3Reference / PHC4Reference))

	Z :=
		cm.vc_OvercastDayRadiation /
		(cm.vc_EffectiveDayLength * 3600.0) *
		vc_NetRadiationUseEfficiency /
		(5.0 * cm.vc_AssimilationRate)

	PHOH1 := 5.0 * cm.vc_AssimilationRate * cm.vc_EffectiveDayLength * Z / (1.0 + Z)
	PHOH := 0.9935 * PHOH1 + 1.1
	PHO3 := PHOH * fractionOfInterceptedRadiation
	PHO3Reference :=
		PHOH *
		(1.0 - libc.exp(-cultivarPs.pc_LightExtinctionCoefficient * pc_ReferenceLeafAreaIndex))

	PHOL :=
		PHO3 < PHC4 ? PHO3 * (1.0 - libc.exp(-PHC4 / PHO3)) : PHC4 * (1.0 - libc.exp(-PHO3 / PHC4))

	PHOLReference :=
		PHO3Reference < PHC4Reference ? PHO3Reference * (1.0 - libc.exp(-PHC4Reference / PHO3Reference)) : PHC4Reference * (1.0 - libc.exp(-PHO3Reference / PHC4Reference))

	vc_ClearDayCO2Assimilation := LAI < 5.0 ? PHCL : PHCH // [J m-2]
	vc_OvercastDayCO2Assimilation := LAI < 5.0 ? PHOL : PHOH // [J m-2]

	vc_ClearDayCO2AssimilationReference := PHCLReference
	vc_OvercastDayCO2AssimilationReference := PHOLReference

	// old DTGA
	vc_GrossCO2Assimilation :=
		vc_OvercastSkyTimeFraction * vc_OvercastDayCO2Assimilation +
		(1.0 - vc_OvercastSkyTimeFraction) * vc_ClearDayCO2Assimilation

	// used for ET0 calculation
	vc_GrossCO2AssimilationReference :=
		vc_OvercastSkyTimeFraction * vc_OvercastDayCO2AssimilationReference +
		(1.0 - vc_OvercastSkyTimeFraction) * vc_ClearDayCO2AssimilationReference

	// Gross CO2 assimilation is used for reference evapotranspiration
	// calculation. For this purpose it must not be affected by drought
	// stress, as the grass reference is defined as being always well
	// supplied with water. Water stress is acting at a later stage.
	// NOTE(c++-quirk): the C++ multiplies vc_GrossCO2Assimilation by itself
	// here (a no-op, per the source comment "* vc_TranspirationDeficit" being
	// commented out) - reproduced as the no-op it is, not simplified away.
	if cm.vc_TranspirationDeficit < vc_DroughtStressThreshold {
		vc_GrossCO2Assimilation = vc_GrossCO2Assimilation
	}

	vs_JulianDay := int(d.julian_day(currentDate))
	dailyGP := 0.0
	if cropPs.__enable_hourly_FvCB_photosynthesis__ && pc_CarboxylationPathway == 1 {
		hourlyGlobrads := make([dynamic]f64, 0, 24, context.temp_allocator)
		hourlyExtrarad := make([dynamic]f64, 0, 24, context.temp_allocator)
		sunriseH := 0
		// see the function comment above re: this vs. C++'s hourlyGlobrads.back()
		prevHgr := 0.0

		for h := 0; h < 24; h += 1 {
			hgr := tl.hourly_rad(cm.vc_GlobalRadiation, vs_Latitude, vs_JulianDay, h)
			if hgr > 0 && prevHgr == 0.0 {
				sunriseH = h
			}
			append(&hourlyGlobrads, hgr)
			prevHgr = hgr

			append(
				&hourlyExtrarad,
				tl.hourly_rad(cm.vc_ExtraterrestrialRadiation, vs_Latitude, vs_JulianDay, h),
			)
		}

		cm.guentherEmissions = Voc_Emissions{}
		cm.jjvEmissions = Voc_Emissions{}

		for h := 0; h < 24; h += 1 {
			// hourly photosynthesis
			FvCB_in: Fvcb_Canopy_Hourly_In

			hourlyTemp := tl.hourly_t(vw_MinAirTemperature, vw_MaxAirTemperature, h, sunriseH)
			FvCB_in.leaf_temp = hourlyTemp
			FvCB_in.global_rad = hourlyGlobrads[h]
			FvCB_in.extra_terr_rad = hourlyExtrarad[h]
			FvCB_in.LAI = LAI
			FvCB_in.solar_el = tl.solar_elevation(h, vs_Latitude, vs_JulianDay)
			FvCB_in.VPD = tl.hourly_vapor_pressure_deficit(
				hourlyTemp,
				vw_MinAirTemperature,
				vw_MeanAirTemperature,
				vw_MaxAirTemperature,
			)
			FvCB_in.Ca = vw_AtmosphericCO2Concentration

			hps := make_fvcb_canopy_hourly_params()
			hps.Vcmax_25 = speciesPs.VCMAX25 * cm.vc_O3_shortTermDamage * cm.vc_O3_senescence

			FvCB_res := fvcb_canopy_hourly_c3(FvCB_in, hps)

			cm.vc_sunlitLeafAreaIndex[h] = FvCB_res.sunlit.LAI
			cm.vc_shadedLeafAreaIndex[h] = FvCB_res.shaded.LAI

			// [umol CO2 m-2 (h-1)] -> [kg CO2 ha-1 (d-1)]
			dailyGP += FvCB_res.canopy_gross_photos * 44.0 / 100.0 / 1000.0

			// hourly O3 uptake and damage
			O3_par := make_o3_impact_params()
			O3_par.gamma3 = 0.05 // TODO: calibrate and add to crop params
			O3_par.gamma1 = 0.025 // TODO: calibrate and add to crop params

			root_depth := cm.vc_RootingDepth
			if root_depth >= 1 { 	// the crop has emerged
				FC := 0.0
				WP := 0.0
				SWC := 0.0
				for i := 0; i < root_depth; i += 1 {
					FC += soilColumn.layers[i].vs_FieldCapacity
					WP += soilColumn.layers[i].vs_PermanentWiltingPoint
					SWC += soilColumn.layers[i].vs_SoilMoisture_m3
				}

				// weighted average gs and conversion from unit ground area
				// to unit leaf area
				lai_sun_weight := FvCB_res.sunlit.LAI / (FvCB_res.sunlit.LAI + FvCB_res.shaded.LAI)
				lai_sh_weight := 1 - lai_sun_weight
				avg_leaf_gs := lai_sh_weight * FvCB_res.shaded.gs / FvCB_res.shaded.LAI
				if FvCB_res.sunlit.LAI > 0 {
					avg_leaf_gs += lai_sun_weight * FvCB_res.sunlit.gs / FvCB_res.sunlit.LAI
				}

				O3_in: O3_Impact_In
				O3_in.FC = FC / f64(root_depth + 1) // field capacity, m3 m-3, avg in the rooted zone
				O3_in.WP = WP / f64(root_depth + 1) // wilting point, m3 m-3
				O3_in.SWC = SWC / f64(root_depth + 1) // soil water content, m3 m-3
				O3_in.ET0 = cm.vc_ReferenceEvapotranspiration
				O3_in.O3a = vw_AtmosphericO3Concentration
				O3_in.gs = avg_leaf_gs
				O3_in.h = h
				O3_in.reldev = cm.vc_RelativeTotalDevelopment
				O3_in.GDD_flo = cm.vc_TemperatureSumToFlowering
				O3_in.GDD_mat = cm.vc_TotalTemperatureSum
				O3_in.fO3s_d_prev = cm.vc_O3_shortTermDamage
				O3_in.sum_O3_up = cm.vc_O3_sumUptake

				O3_res := o3_impact_hourly(O3_in, O3_par, pc_WaterDeficitResponseOn)

				cm.vc_O3_shortTermDamage = O3_res.fO3s_d
				cm.vc_O3_longTermDamage = O3_res.fO3l
				cm.vc_O3_senescence = O3_res.fLS
				cm.vc_O3_sumUptake += O3_res.hourly_O3_up
				cm.vc_O3_WStomatalClosure = O3_res.WS_st_clos
			}

			// calculate VOC emissions
			globradWm2 := FvCB_in.global_rad * 1000000.0 / 3600 // MJ m-2 h-1 -> W m-2
			if cm.index240 < cm.stepSize240 - 1 {
				cm.index240 += 1
			} else {
				cm.index240 = 0
				cm.full240 = true
			}
			cm.rad240[cm.index240] = globradWm2
			cm.tfol240[cm.index240] = FvCB_in.leaf_temp

			if cm.index24 < cm.stepSize24 - 1 {
				cm.index24 += 1
			} else {
				cm.index24 = 0
				cm.full24 = true
			}
			cm.rad24[cm.index24] = globradWm2
			cm.tfol24[cm.index24] = FvCB_in.leaf_temp

			mcd: Voc_Micro_Climate_Data
			mcd.rad = globradWm2
			rad24_sum := 0.0
			for v in cm.rad24 {
				rad24_sum += v
			}
			mcd.rad24 = rad24_sum / (cm.full24 ? f64(len(cm.rad24)) : f64(cm.index24 + 1))
			rad240_sum := 0.0
			for v in cm.rad240 {
				rad240_sum += v
			}
			mcd.rad240 = rad240_sum / (cm.full240 ? f64(len(cm.rad240)) : f64(cm.index240 + 1))
			mcd.tFol = FvCB_in.leaf_temp
			tfol24_sum := 0.0
			for v in cm.tfol24 {
				tfol24_sum += v
			}
			mcd.tFol24 = tfol24_sum / (cm.full24 ? f64(len(cm.tfol24)) : f64(cm.index24 + 1))
			tfol240_sum := 0.0
			for v in cm.tfol240 {
				tfol240_sum += v
			}
			mcd.tFol240 = tfol240_sum / (cm.full240 ? f64(len(cm.tfol240)) : f64(cm.index240 + 1))
			mcd.co2concentration = vw_AtmosphericCO2Concentration

			species: Voc_Species_Data
			species.lai = LAI
			species.mFol = cm.vc_OrganGreenBiomass[Organ_Leaf] / (100.0 * 100.0) // kg/ha -> kg/m2
			species.sla =
				species.mFol > 0 ? species.lai / species.mFol : pc_SpecificLeafArea[cm.vc_DevelopmentalStage] * 100.0 * 100.0 // ha/kg -> m2/kg

			species.EF_MONO = speciesPs.EF_MONO
			species.EF_MONOS = speciesPs.EF_MONOS
			species.EF_ISO = speciesPs.EF_ISO
			species.VCMAX25 = speciesPs.VCMAX25
			species.AEKC = speciesPs.AEKC
			species.AEKO = speciesPs.AEKO
			species.AEVC = speciesPs.AEVC
			species.KC25 = speciesPs.KC25

			ges := voc_guenther_emissions(species, &mcd, 1.0 / 24.0, allocator)
			voc_emissions_add(&cm.guentherEmissions, &ges, allocator)

			sun_LAI := FvCB_res.sunlit.LAI
			sh_LAI := FvCB_res.shaded.LAI
			leaf_fractions := []Fvcb_Leaf_Fraction{FvCB_res.sunlit, FvCB_res.shaded}
			for lf in leaf_fractions {
				species.lai = lf.LAI
				species.mFol =
					cm.vc_OrganGreenBiomass[Organ_Leaf] /
					(100.0 * 100.0) *
					lf.LAI /
					(sun_LAI + sh_LAI) // kg/ha -> kg/m2
				species.sla =
					species.mFol > 0 ? species.lai / species.mFol : pc_SpecificLeafArea[cm.vc_DevelopmentalStage] * 100.0 * 100.0 // ha/kg -> m2/kg

				mcd.rad = lf.rad // W m-2 global incident

				cm.cropPhotosynthesisResults.kc = lf.kc
				cm.cropPhotosynthesisResults.ko = lf.ko * 1000
				cm.cropPhotosynthesisResults.oi = lf.oi * 1000
				cm.cropPhotosynthesisResults.ci = lf.ci
				cm.cropPhotosynthesisResults.vcMax =
					fvcb_Vcmax_bernacchi_f(mcd.tFol, speciesPs.VCMAX25) *
					cm.vc_CropNRedux *
					cm.vc_TranspirationDeficit
				cm.cropPhotosynthesisResults.jMax =
					fvcb_Jmax_bernacchi_f(mcd.tFol, 120) *
					cm.vc_CropNRedux *
					cm.vc_TranspirationDeficit
				cm.cropPhotosynthesisResults.jj = lf.jj
				cm.cropPhotosynthesisResults.jj1000 = lf.jj1000
				cm.cropPhotosynthesisResults.jv = lf.jv

				jjves := voc_jjv_emissions(
					species,
					&mcd,
					cm.cropPhotosynthesisResults,
					1.0 / 24.0,
					false,
					allocator,
				)
				voc_emissions_add(&cm.jjvEmissions, &jjves, allocator)
			}
		}
	}

	vc_GrossCO2Assimilation =
		cropPs.__enable_hourly_FvCB_photosynthesis__ && pc_CarboxylationPathway == 1 ? dailyGP : vc_GrossCO2Assimilation

	cm.fractionOfInterceptedRadiation1 = fractionOfInterceptedRadiation

	// [TRANSPLANT SHOCK] Photosynthesis Limitation.
	if cm.vc_TransplantEfficiency < 1.0 {
		vc_GrossCO2Assimilation *= cm.vc_TransplantEfficiency
	}

	// Calculation of photosynthesis rate from [kg CO2 ha-1 d-1] to [kg CH2O ha-1 d-1]
	cm.vc_GrossPhotosynthesis = vc_GrossCO2Assimilation * 30.0 / 44.0

	// Calculation of photosynthesis rate from [kg CO2 ha-1 d-1] to [mol m-2 s-1]
	cm.vc_GrossPhotosynthesis_mol =
		vc_GrossCO2Assimilation * 22414.0 / (10.0 * 3600.0 * 24.0 * 44.0)
	cm.vc_GrossPhotosynthesisReference_mol =
		vc_GrossCO2AssimilationReference * 22414.0 / (10.0 * 3600.0 * 24.0 * 44.0)

	// Converting photosynthesis rate from [kg CO2 ha leaf-1 d-1] to [kg CH2O ha-1 d-1]
	cm.vc_Assimilates = vc_GrossCO2Assimilation * 30.0 / 44.0

	// reduction value for assimilate amount to simulate field conditions
	cm.vc_Assimilates *= pc_FieldConditionModifier
	// reduction value for assimilate amount to simulate frost damage
	cm.vc_Assimilates *= cm.vc_CropFrostRedux
	// MP: added reduction value for assimilate amount to simulate waterlogging
	cm.vc_Assimilates *= cm.vc_OxygenDeficit

	if cm.vc_TranspirationDeficit < vc_DroughtStressThreshold { 	// MP: Access point for drought optimisation
		cm.vc_Assimilates = cm.vc_Assimilates * cm.vc_TranspirationDeficit
	}

	cm.vc_GrossAssimilates = cm.vc_Assimilates

	// ---------------------------------------------------------------------
	// AGROSIM night and day maintenance and growth respiration
	// ---------------------------------------------------------------------

	vc_PhotoTemperature :=
		vw_MaxAirTemperature - ((vw_MaxAirTemperature - vw_MinAirTemperature) / 4.0)
	vc_NightTemperature :=
		vw_MinAirTemperature + ((vw_MaxAirTemperature - vw_MinAirTemperature) / 4.0)

	vc_MaintenanceRespirationSum := 0.0
	for i_Organ := 0; i_Organ < cm.noOfOrgans; i_Organ += 1 {
		vc_MaintenanceRespirationSum +=
			cm.vc_OrganGreenBiomass[i_Organ] * pc_OrganMaintenanceRespiration[i_Organ] // [kg CH2O ha-1]
	}

	vc_NormalisedDayLength := 2.0 - (cm.vc_PhotoperiodicDaylength / 12.0)

	vc_PhotoMaintenanceRespiration :=
		vc_MaintenanceRespirationSum *
		libc.pow(
			2.0,
			(pc_MaintenanceRespirationParameter_1 *
				(vc_PhotoTemperature - pc_MaintenanceRespirationParameter_2)),
		) *
		(2.0 - vc_NormalisedDayLength) // @todo: [g m-2] --> [kg ha-1]

	vc_DarkMaintenanceRespiration :=
		vc_MaintenanceRespirationSum *
		libc.pow(
			2.0,
			(pc_MaintenanceRespirationParameter_1 *
				(vc_NightTemperature - pc_MaintenanceRespirationParameter_2)),
		) *
		vc_NormalisedDayLength // @todo: [g m-2] --> [kg ha-1]

	cm.vc_MaintenanceRespirationAS =
		vc_PhotoMaintenanceRespiration + vc_DarkMaintenanceRespiration // [kg CH2O ha-1]

	cm.vc_Assimilates -= vc_PhotoMaintenanceRespiration + vc_DarkMaintenanceRespiration // [kg CH2O ha-1]

	vc_GrowthRespirationSum := 0.0
	if cm.vc_Assimilates > 0 {
		for i_Organ := 0; i_Organ < cm.noOfOrgans; i_Organ += 1 {
			vc_GrowthRespirationSum +=
				pc_AssimilatePartitioningCoeff[cm.vc_DevelopmentalStage][i_Organ] *
				cm.vc_Assimilates *
				pc_OrganGrowthRespiration[i_Organ]
		}
	}

	vc_PhotoGrowthRespiration := 0.0
	if cm.vc_Assimilates > 0.0 {
		vc_PhotoGrowthRespiration =
			vc_GrowthRespirationSum *
			libc.pow(
				2.0,
				(pc_GrowthRespirationParameter_1 *
					(vc_PhotoTemperature - pc_GrowthRespirationParameter_2)),
			) *
			(2.0 - vc_NormalisedDayLength) // [kg CH2O ha-1]

		if cm.vc_Assimilates > vc_PhotoGrowthRespiration {
			cm.vc_Assimilates -= vc_PhotoGrowthRespiration
		} else {
			vc_PhotoGrowthRespiration = cm.vc_Assimilates // in this case the plant will be restricted in growth!
			cm.vc_Assimilates = 0.0
		}
	}

	// NOTE(c++-quirk): uses vc_PhotoTemperature here too, not
	// vc_NightTemperature - asymmetric with the maintenance-respiration split
	// just above (which correctly uses Photo/Night respectively). Reproduced
	// exactly, not "fixed".
	vc_DarkGrowthRespiration := 0.0
	if cm.vc_Assimilates > 0.0 {
		vc_DarkGrowthRespiration =
			vc_GrowthRespirationSum *
			libc.pow(
				2.0,
				(pc_GrowthRespirationParameter_1 *
					(vc_PhotoTemperature - pc_GrowthRespirationParameter_2)),
			) *
			vc_NormalisedDayLength // [kg CH2O ha-1]

		if cm.vc_Assimilates > vc_DarkGrowthRespiration {
			cm.vc_Assimilates -= vc_DarkGrowthRespiration
		} else {
			vc_DarkGrowthRespiration = cm.vc_Assimilates // in this case the plant will be restricted in growth!
			cm.vc_Assimilates = 0.0
		}
	}
	cm.vc_GrowthRespirationAS = vc_PhotoGrowthRespiration + vc_DarkGrowthRespiration // [kg CH2O ha-1]
	cm.vc_TotalRespired = cm.vc_GrossAssimilates - cm.vc_Assimilates // [kg CH2O ha-1]

	// ---------------------------------------------------------------------
	// HERMES calculation of maintenance respiration in dependence of
	// temperature (to reactivate, vc_NetPhotosynthesis needs to be used
	// instead of vc_Assimilates in the subsequent methods)
	// ---------------------------------------------------------------------

	// old TEFF
	vc_MaintenanceTemperatureDependency := libc.pow(2.0, (0.1 * vw_MeanAirTemperature - 2.5))

	// old MAINTS
	vc_MaintenanceRespiration := 0.0
	for i_Organ := 0; i_Organ < cm.noOfOrgans; i_Organ += 1 {
		vc_MaintenanceRespiration +=
			cm.vc_OrganGreenBiomass[i_Organ] * pc_OrganMaintenanceRespiration[i_Organ]
	}

	if cm.vc_GrossPhotosynthesis <
	   (vc_MaintenanceRespiration * vc_MaintenanceTemperatureDependency) {
		cm.vc_NetMaintenanceRespiration = cm.vc_GrossPhotosynthesis
	} else {
		cm.vc_NetMaintenanceRespiration =
			vc_MaintenanceRespiration * vc_MaintenanceTemperatureDependency
	}

	if vw_MeanAirTemperature < pc_MinimumTemperatureForAssimilation {
		cm.vc_GrossPhotosynthesis = cm.vc_NetMaintenanceRespiration
	}
}

// C++: double monica::cropmodule::fcGrossPrimaryProduction(const CropModule*)
fc_gross_primary_production :: proc(cm: ^Crop_Module) -> f64 {
	// Converting photosynthesis rate from [kg CH2O ha-1 d-1] back to [kg C ha-1 d-1]
	return cm.vc_GrossAssimilates / 30.0 * 12.0
}

// C++: double monica::cropmodule::fcNetPrimaryProduction(CropModule*, double vc_TotalRespired)
fc_net_primary_production :: proc(cm: ^Crop_Module, vc_TotalRespired: f64) -> f64 {
	// Convert [kg CH2O ha-1 d-1] to [kg C ha-1 d-1]
	cm.vc_Respiration = vc_TotalRespired / 30.0 * 12.0
	return cm.vc_GrossPrimaryProduction - cm.vc_Respiration
}

// C++: void monica::cropmodule::calculateVOCEmissions(CropModule*, const Voc::MicroClimateData&)
calculate_voc_emissions :: proc(
	cm: ^Crop_Module,
	mcd: ^Voc_Micro_Climate_Data,
	allocator := context.allocator,
) {
	pc_SpecificLeafArea := cm.cropParams.cultivarParams.pc_SpecificLeafArea
	speciesPs := &cm.cropParams.speciesParams

	species: Voc_Species_Data
	species.lai = cm.vc_LeafAreaIndex
	species.mFol = cm.vc_OrganBiomass[Organ_Leaf] / (100.0 * 100.0) // kg/ha -> kg/m2
	species.sla = pc_SpecificLeafArea[cm.vc_DevelopmentalStage] * 100.0 * 100.0 // ha/kg -> m2/kg

	species.EF_MONO = speciesPs.EF_MONO
	species.EF_MONOS = speciesPs.EF_MONOS
	species.EF_ISO = speciesPs.EF_ISO
	species.VCMAX25 = speciesPs.VCMAX25
	species.AEKC = speciesPs.AEKC
	species.AEKO = speciesPs.AEKO
	species.AEVC = speciesPs.AEVC
	species.KC25 = speciesPs.KC25

	cm.guentherEmissions = voc_guenther_emissions(species, mcd, 1.0, allocator)
	cm.jjvEmissions = voc_jjv_emissions(
		species,
		mcd,
		cm.cropPhotosynthesisResults,
		1.0,
		false,
		allocator,
	)
}

// ---------------------------------------------------------------------------
// Phase 5 checkpoint 5: biomass/dry matter + stress
//
// Ported: fcHeatStressImpact, fcFrostKill, fcDroughtImpactOnFertility,
// fcCropNitrogen (a prerequisite pulled forward from the "water + nitrogen"
// bucket - it computes vc_CropNRedux/rootNRedux and, despite its name, most
// of the crop's root-growth-rate machinery, which fcCropDryMatter's root
// distribution genuinely depends on to be worth testing), fcCropDryMatter,
// fcMoveDeadRootBiomassToSoil, addAndDistributeRootBiomassInSoil,
// calcRootDensityFactorAndSum. fcCropNUptake (actual N uptake amounts from
// soil layers, a separate and larger concern) stays in checkpoint 6 as
// planned. Deliberately not ported here: the ~15 yield/N-content getters
// (getFruitBiomassNContent, getPrimaryCropYield, ...) and
// numberOfAbovegroundOrgans/organIdsForPrimaryYield - none of them are
// called anywhere inside crop-module.cpp itself (checked directly), they're
// pure output-API surface for build-output.cpp, so they can wait for
// whichever checkpoint actually needs them (phase 7's output table).
// ---------------------------------------------------------------------------

// C++: void monica::cropmodule::fcHeatStressImpact(CropModule*, double
// vw_MaxAirTemperature, double vw_MinAirTemperature)
fc_heat_stress_impact :: proc(cm: ^Crop_Module, vw_MaxAirTemperature, vw_MinAirTemperature: f64) {
	pc_BeginSensitivePhaseHeatStress :=
		cm.cropParams.cultivarParams.pc_BeginSensitivePhaseHeatStress
	pc_CriticalTemperatureHeatStress :=
		cm.cropParams.cultivarParams.pc_CriticalTemperatureHeatStress
	pc_EndSensitivePhaseHeatStress := cm.cropParams.cultivarParams.pc_EndSensitivePhaseHeatStress
	pc_LimitingTemperatureHeatStress :=
		cm.cropParams.speciesParams.pc_LimitingTemperatureHeatStress

	// AGROSIM night and day temperatures
	vc_PhotoTemperature :=
		vw_MaxAirTemperature - ((vw_MaxAirTemperature - vw_MinAirTemperature) / 4.0)
	vc_FractionOpenFlowers := 0.0
	vc_YesterdaysFractionOpenFlowers := 0.0

	if pc_BeginSensitivePhaseHeatStress == 0.0 && pc_EndSensitivePhaseHeatStress == 0.0 {
		cm.vc_TotalCropHeatImpact = 1.0
	}

	if cm.vc_CurrentTotalTemperatureSum >= pc_BeginSensitivePhaseHeatStress &&
	   cm.vc_CurrentTotalTemperatureSum < pc_EndSensitivePhaseHeatStress {
		// Crop heat redux: Challinor et al. (2005), Agric. and Forest
		// Meteorology 135, 180-189.
		vc_CropHeatImpact :=
			1.0 -
			((vc_PhotoTemperature - pc_CriticalTemperatureHeatStress) /
					(pc_LimitingTemperatureHeatStress - pc_CriticalTemperatureHeatStress))

		if vc_CropHeatImpact > 1.0 {
			vc_CropHeatImpact = 1.0
		}
		if vc_CropHeatImpact < 0.0 {
			vc_CropHeatImpact = 0.0
		}

		// Fraction open flowers: Moriondo et al. (2011), Climatic Change 104
		// (3-4), 679-701.
		vc_FractionOpenFlowers =
			1.0 /
			(1.0 + ((1.0 / 0.015) - 1.0) * libc.exp(-1.4 * f64(cm.vc_DaysAfterBeginFlowering)))
		if cm.vc_DaysAfterBeginFlowering > 0 {
			vc_YesterdaysFractionOpenFlowers =
				1.0 /
				(1.0 +
						((1.0 / 0.015) - 1.0) *
							libc.exp(-1.4 * f64(cm.vc_DaysAfterBeginFlowering - 1)))
		} else {
			vc_YesterdaysFractionOpenFlowers = 0.0
		}
		vc_DailyFloweringRate := vc_FractionOpenFlowers - vc_YesterdaysFractionOpenFlowers

		// Total effect: Challinor et al. (2005)
		cm.vc_TotalCropHeatImpact += vc_CropHeatImpact * vc_DailyFloweringRate

		cm.vc_DaysAfterBeginFlowering += 1
	}

	if cm.vc_CurrentTotalTemperatureSum >= pc_EndSensitivePhaseHeatStress ||
	   vc_FractionOpenFlowers > 0.999999 {
		if cm.vc_TotalCropHeatImpact < cm.vc_CropHeatRedux {
			cm.vc_CropHeatRedux = cm.vc_TotalCropHeatImpact
		}
	}
}

// C++: void monica::cropmodule::fcFrostKill(CropModule*, double maxAirTemp, double minAirTemp)
//
// Fowler, Byrns & Greer (2014): Overwinter Low-Temperature Responses of
// Cereals. Crop Sci. 54:2395-2405.
fc_frost_kill :: proc(cm: ^Crop_Module, maxAirTemp, minAirTemp: f64) {
	soilColumn := cm.soilColumn
	LT50cultivar := cm.cropParams.cultivarParams.pc_LT50cultivar

	LT50old := cm.vc_LT50
	cm.vc_LT50M = min(cm.vc_LT50, cm.vc_LT50M)

	nightTemperature := minAirTemp + ((maxAirTemp - minAirTemp) / 4.0)
	crownTemperature := nightTemperature * 0.8
	snowDepth, tempUnderSnow := cm.getSnowDepthAndCalcTempUnderSnow(crownTemperature)
	if cm.vc_DevelopmentalStage <= 1 {
		crownTemperature =
			(3.0 * soilColumn.vt_SoilSurfaceTemperature +
				2.0 * soilColumn.layers[0].vs_SoilTemperature) /
			5.0
	} else if snowDepth > 0.0 {
		crownTemperature = tempUnderSnow
	}

	frostHardening := 0.0
	thresholdInductionTemperature := 3.72135 - 0.401124 * LT50cultivar
	frostHardeningParam := cm.cropParams.cultivarParams.pc_FrostHardening
	if cm.vc_VernalisationFactor < 1.0 && crownTemperature < thresholdInductionTemperature {
		frostHardening =
			frostHardeningParam *
			(thresholdInductionTemperature - crownTemperature) *
			(LT50old - LT50cultivar)
	}

	frostDehardening := 0.0
	frostDehardeningParam := cm.cropParams.cultivarParams.pc_FrostDehardening
	stageTempSum := cm.cropParams.cultivarParams.pc_StageTemperatureSum
	vc_DoubleRidgeCounter := cm.vc_CurrentTemperatureSum[1] / stageTempSum[1]
	vc_VRTFactor := 1.0 / (1.0 + libc.exp(80.0 * (vc_DoubleRidgeCounter - 0.9)))
	if (vc_DoubleRidgeCounter < 1.0 && crownTemperature >= thresholdInductionTemperature) ||
	   vc_DoubleRidgeCounter >= 1.0 {
		frostDehardening = frostDehardeningParam / (1.0 + libc.exp(4.35 - 0.28 * crownTemperature))
	} else if vc_DoubleRidgeCounter < 1.0 &&
	   -4.0 <= crownTemperature &&
	   crownTemperature < thresholdInductionTemperature {
		frostDehardening =
			(1.0 - vc_VRTFactor) *
			frostDehardeningParam /
			(1.0 + libc.exp(4.35 - 0.28 * crownTemperature))
	}

	snowDepthFactor := 1.0
	if soilColumn.vm_SnowDepth <= 125.0 {
		snowDepthFactor = soilColumn.vm_SnowDepth / 125.0
	}
	respirationFactor := (libc.exp(0.84 + 0.051 * crownTemperature) - 2.0) / 1.85
	respiratoryStressParam := cm.cropParams.cultivarParams.pc_RespiratoryStress
	respiratoryStress := respiratoryStressParam * respirationFactor * snowDepthFactor

	cm.vc_LT50 = LT50old - frostHardening + frostDehardening + respiratoryStress

	if cm.vc_LT50 > -3.0 {
		cm.vc_LT50 = -3.0
	}
	if crownTemperature < cm.vc_LT50M {
		cm.vc_CropFrostRedux *= 0.5
	}
}

// C++: void monica::cropmodule::fcDroughtImpactOnFertility(CropModule*)
fc_drought_impact_on_fertility :: proc(cm: ^Crop_Module) {
	assimPartCoeff := cm.cropParams.cultivarParams.pc_AssimilatePartitioningCoeff
	droughtImpactOnFertilityFactor := cm.cropParams.speciesParams.pc_DroughtImpactOnFertilityFactor
	droughtStressThreshold := cm.cropParams.cultivarParams.pc_DroughtStressThreshold
	devStage := cm.vc_DevelopmentalStage

	if cm.vc_TranspirationDeficit < 0.0 {
		cm.vc_TranspirationDeficit = 0.0
	}

	// Fertility of the crop is reduced in cases of severe drought during bloom
	if cm.vc_TranspirationDeficit <
		   (droughtImpactOnFertilityFactor * droughtStressThreshold[devStage]) &&
	   assimPartCoeff[devStage][cm.vc_StorageOrgan] > 0.0 {
		vc_TranspirationDeficitHelper :=
			cm.vc_TranspirationDeficit /
			(droughtImpactOnFertilityFactor * droughtStressThreshold[devStage])

		if cm.vc_OxygenDeficit < 1.0 {
			cm.vc_DroughtImpactOnFertility = 1.0
		} else {
			cm.vc_DroughtImpactOnFertility =
				1.0 -
				((1.0 - vc_TranspirationDeficitHelper) * (1.0 - vc_TranspirationDeficitHelper))
		}
	} else {
		cm.vc_DroughtImpactOnFertility = 1.0
	}
}

// C++: void monica::cropmodule::fcCropNitrogen(CropModule*)
fc_crop_nitrogen :: proc(cm: ^Crop_Module) {
	pc_NConcentrationB0 := cm.cropParams.speciesParams.pc_NConcentrationB0
	pc_NConcentrationPN := cm.cropParams.speciesParams.pc_NConcentrationPN
	pc_LuxuryNCoeff := cm.cropParams.speciesParams.pc_LuxuryNCoeff
	pc_MinimumNConcentration := cm.cropParams.speciesParams.pc_MinimumNConcentration
	pc_NitrogenResponseOn := cm.simParams.pc_NitrogenResponseOn

	cm.vc_CriticalNConcentration =
		pc_NConcentrationPN *
		(1.0 +
				(pc_NConcentrationB0 *
						libc.exp(
							-0.26 * (cm.vc_AbovegroundBiomass + cm.vc_BelowgroundBiomass) / 1000.0,
						))) /
		100.0 // [kg ha-1 -> t ha-1]
	cm.vc_TargetNConcentration = cm.vc_CriticalNConcentration * pc_LuxuryNCoeff
	cm.vc_NConcentrationAbovegroundBiomassOld = cm.vc_NConcentrationAbovegroundBiomass
	cm.vc_NConcentrationRootOld = cm.vc_NConcentrationRoot

	if cm.vc_NConcentrationRoot < 0.01 {
		if cm.vc_NConcentrationRoot <= 0.005 {
			cm.rootNRedux = 0.0
		} else {
			// old WUX
			rootNReduxHelper := (cm.vc_NConcentrationRoot - 0.005) / 0.005
			cm.rootNRedux = 1.0 - libc.sqrt(1.0 - rootNReduxHelper * rootNReduxHelper)
		}
	} else {
		cm.rootNRedux = 1.0
	}

	if cm.vc_NConcentrationAbovegroundBiomass < cm.vc_CriticalNConcentration {
		if cm.vc_NConcentrationAbovegroundBiomass <= pc_MinimumNConcentration {
			cm.vc_CropNRedux = 0.0
		} else {
			cropNReduxHelper :=
				(cm.vc_NConcentrationAbovegroundBiomass - pc_MinimumNConcentration) /
				(cm.vc_CriticalNConcentration - pc_MinimumNConcentration)

			// New Monica approach
			cm.vc_CropNRedux = 1.0 - libc.exp(pc_MinimumNConcentration - (5.0 * cropNReduxHelper))
		}
	} else {
		cm.vc_CropNRedux = 1.0
	}

	if !pc_NitrogenResponseOn {
		cm.vc_CropNRedux = 1.0
	}
}

// C++: pair<vector<double>,double> monica::cropmodule::calcRootDensityFactorAndSum(const CropModule*)
calc_root_density_factor_and_sum :: proc(
	cm: ^Crop_Module,
	allocator := context.allocator,
) -> (
	[dynamic]f64,
	f64,
) {
	nols := len(cm.soilColumn.layers)
	layerThickness := cm.soilColumn.layers[0].vs_LayerThickness

	// Calculating a root density distribution factor []
	vc_RootDensityFactor := make([dynamic]f64, nols, allocator)
	for i_Layer := 0; i_Layer < nols; i_Layer += 1 {
		if i_Layer < cm.vc_RootingDepth {
			vc_RootDensityFactor[i_Layer] = libc.exp(
				-cm.cropParams.speciesParams.pc_RootFormFactor * (f64(i_Layer) * layerThickness),
			)
		} else if i_Layer < cm.vc_RootingZone {
			// NOTE(c++-quirk): (i_Layer - vc_RootingDepth) / (vc_RootingZone -
			// vc_RootingDepth) is genuine integer division in C++ (all size_t
			// operands) - the C++ source has a commented-out double-cast "fix"
			// deliberately left unapplied ("changes the outputs enough to talk
			// about it first"). Reproduced exactly: the division below is int/int.
			int_div := (i_Layer - cm.vc_RootingDepth) / (cm.vc_RootingZone - cm.vc_RootingDepth)
			vc_RootDensityFactor[i_Layer] =
				libc.exp(
					-cm.cropParams.speciesParams.pc_RootFormFactor *
					(f64(i_Layer) * layerThickness),
				) *
				(1.0 - f64(int_div))
		} else {
			vc_RootDensityFactor[i_Layer] = 0.0
		}
	}

	// Summing up all factors to scale to a relative factor between [0;1]
	vc_RootDensityFactorSum := 0.0
	for i_Layer := 0; i_Layer < cm.vc_RootingZone; i_Layer += 1 {
		vc_RootDensityFactorSum += vc_RootDensityFactor[i_Layer]
	}

	return vc_RootDensityFactor, vc_RootDensityFactorSum
}

// C++: void monica::cropmodule::fcMoveDeadRootBiomassToSoil(CropModule*,
// double deadRootBiomass, double rootDensityFactorSum, const
// vector<double>& rootDensityFactor)
fc_move_dead_root_biomass_to_soil :: proc(
	cm: ^Crop_Module,
	deadRootBiomass, rootDensityFactorSum: f64,
	rootDensityFactor: [dynamic]f64,
	allocator := context.allocator,
) {
	nools := cm.soilColumn.vs_NumberOfOrganicLayers

	layer2deadRootBiomassAtLayer := make(map[int]f64, allocator)
	for i := 0; i < cm.vc_RootingZone; i += 1 {
		deadRootBiomassAtLayer := rootDensityFactor[i] / rootDensityFactorSum * deadRootBiomass
		// just add organic matter if > 0.0001
		if int(deadRootBiomassAtLayer * 10000) > 0 {
			idx := i < nools ? i : nools - 1
			layer2deadRootBiomassAtLayer[idx] += deadRootBiomassAtLayer
		}
	}

	if len(layer2deadRootBiomassAtLayer) > 0 {
		cm.addOrganicMatter(layer2deadRootBiomassAtLayer, cm.vc_NConcentrationRoot)
	}
}

// C++: void monica::cropmodule::addAndDistributeRootBiomassInSoil(CropModule*, double rootBiomass)
add_and_distribute_root_biomass_in_soil :: proc(
	cm: ^Crop_Module,
	rootBiomass: f64,
	allocator := context.allocator,
) {
	rootDensityFactor, rootDensityFactorSum := calc_root_density_factor_and_sum(cm, allocator)
	fc_move_dead_root_biomass_to_soil(
		cm,
		rootBiomass,
		rootDensityFactorSum,
		rootDensityFactor,
		allocator,
	)
}

// C++: void monica::cropmodule::fcCropDryMatter(CropModule*, double vw_MeanAirTemperature)
//
// Dry matter allocation within the crop: the result from crop photosynthesis
// is allocated to the different crop organs under consideration of stress
// factors (water, nitrogen, temperature), plus root growth (rooting depth,
// root density/diameter per layer) and handing daily dead root biomass off
// to soil organic matter via fcMoveDeadRootBiomassToSoil/addOrganicMatter.
fc_crop_dry_matter :: proc(
	cm: ^Crop_Module,
	vw_MeanAirTemperature: f64,
	allocator := context.allocator,
) {
	cropPs := cm.cropModParams
	soilColumn := cm.soilColumn
	speciesPs := &cm.cropParams.speciesParams
	pc_AbovegroundOrgan := cm.cropParams.speciesParams.pc_AbovegroundOrgan
	pc_AssimilatePartitioningCoeffArr :=
		cm.cropParams.cultivarParams.pc_AssimilatePartitioningCoeff
	pc_AssimilateReallocation := cm.cropParams.speciesParams.pc_AssimilateReallocation
	pc_CropSpecificMaxRootingDepth := cm.cropParams.cultivarParams.pc_CropSpecificMaxRootingDepth
	pc_DroughtStressThreshold := cm.cropParams.cultivarParams.pc_DroughtStressThreshold
	pc_InitialRootingDepth := cm.cropParams.speciesParams.pc_InitialRootingDepth
	pc_MaxNUptakeParam := cm.cropParams.speciesParams.pc_MaxNUptakeParam
	pc_MinimumTemperatureRootGrowth := cm.cropParams.speciesParams.pc_MinimumTemperatureRootGrowth
	pc_OrganSenescenceRate := cm.cropParams.cultivarParams.pc_OrganSenescenceRate
	pc_Perennial := cm.cropParams.cultivarParams.pc_Perennial
	pc_ResidueNRatio := cm.cropParams.cultivarParams.pc_ResidueNRatio
	pc_RootGrowthLag := cm.cropParams.speciesParams.pc_RootGrowthLag
	pc_RootPenetrationRate := cm.cropParams.speciesParams.pc_RootPenetrationRate
	pc_SpecificRootLength := cm.cropParams.speciesParams.pc_SpecificRootLength
	pc_StageMaxRootNConcentration := cm.cropParams.speciesParams.pc_StageMaxRootNConcentration
	pc_StageTemperatureSum := cm.cropParams.cultivarParams.pc_StageTemperatureSum
	pc_StorageOrgan := cm.cropParams.speciesParams.pc_StorageOrgan
	vs_ImpenetrableLayerDepth := cm.siteParams.vs_ImpenetrableLayerDepth
	vs_MaxEffectiveRootingDepth := cm.siteParams.vs_MaxEffectiveRootingDepth

	nols := len(soilColumn.layers)
	layerThickness := soilColumn.layers[0].vs_LayerThickness

	vc_MaxRootNConcentration := 0.0 // old WGM
	vc_RootNIncrement := 0.0 // old WUMM
	vc_AssimilatePartitioningCoeffOld := 0.0
	vc_AssimilatePartitioningCoeff := 0.0

	pc_MaxCropNDemand := cropPs.pc_MaxCropNDemand

	// Assuming that growth respiration takes 30% of total assimilation - from
	// AGROSIM algorithms (the HERMES alternative is commented out in the C++)
	cm.vc_NetPhotosynthesis = cm.vc_Assimilates
	// TMP_Regulatory_factor: computed but never read again afterward in the
	// C++ either - a genuine dead store, kept for fidelity.
	TMP_Regulatory_factor := speciesPs.pc_StageMobilFromStorageCoeff[cm.vc_DevelopmentalStage]
	if cm.vc_DevelopmentalStage == 1 {
		TMP_Regulatory_factor =
			speciesPs.pc_StageMobilFromStorageCoeff[cm.vc_DevelopmentalStage] * cm.vc_KTkc
	}
	_ = TMP_Regulatory_factor

	mobilization_from_storage :=
		cm.vc_OrganBiomass[cm.vc_StorageOrgan] *
		speciesPs.pc_StageMobilFromStorageCoeff[cm.vc_DevelopmentalStage] *
		cm.vc_KTkc

	cm.vc_ReserveAssimilatePool = 0.0

	cm.vc_AbovegroundBiomassOld = cm.vc_AbovegroundBiomass
	cm.vc_AbovegroundBiomass = 0.0
	cm.vc_BelowgroundBiomassOld = cm.vc_BelowgroundBiomass
	cm.vc_BelowgroundBiomass = 0.0
	cm.vc_TotalBiomass = 0.0

	// Dry matter production - old NRKOM
	assimilate_partition_leaf := 0.05
	dailyDeadRootBiomassIncrement := 0.0
	for i_Organ := 0; i_Organ < cm.noOfOrgans; i_Organ += 1 {
		// Prevent out-of-bounds array access on developmental stage indices
		// during early phases. If vc_DevelopmentalStage is 0, fall back to
		// index 0 as the previous stage index.
		prevStage := cm.vc_DevelopmentalStage > 0 ? cm.vc_DevelopmentalStage - 1 : 0
		vc_AssimilatePartitioningCoeffOld = pc_AssimilatePartitioningCoeffArr[prevStage][i_Organ]
		vc_AssimilatePartitioningCoeff =
			pc_AssimilatePartitioningCoeffArr[cm.vc_DevelopmentalStage][i_Organ]

		// Identify storage organ and reduce assimilate flux in case of heat stress
		if pc_StorageOrgan[i_Organ] {
			vc_AssimilatePartitioningCoeffOld =
				vc_AssimilatePartitioningCoeffOld *
				cm.vc_CropHeatRedux *
				cm.vc_DroughtImpactOnFertility
			vc_AssimilatePartitioningCoeff =
				vc_AssimilatePartitioningCoeff *
				cm.vc_CropHeatRedux *
				cm.vc_DroughtImpactOnFertility
		}

		if (cm.vc_CurrentTemperatureSum[cm.vc_DevelopmentalStage] /
			   pc_StageTemperatureSum[cm.vc_DevelopmentalStage]) >
		   1 {
			// Pflanze ist ausgewachsen
			cm.vc_OrganGrowthIncrement[i_Organ] = 0.0
			cm.vc_OrganSenescenceIncrement[i_Organ] = 0.0
			if pc_Perennial {
				cm.vc_GrowthCycleEnded = true
			}
		} else {
			// test if there is a positive balance of produced assimilates; if
			// vc_NetPhotosynthesis is negative, the crop needs more for
			// maintenance than for building new biomass
			if cm.vc_NetPhotosynthesis < 0.0 {
				// reduce biomass from leaf and shoot because of negative assimilate
				if i_Organ == Organ_Leaf {
					incr := assimilate_partition_leaf * cm.vc_NetPhotosynthesis
					if libc.fabs(incr) <= cm.vc_OrganBiomass[i_Organ] {
						cm.vc_OrganGrowthIncrement[i_Organ] = incr
					} else {
						// temporary hack because complex algorithm produces
						// questionable results - reduce only what is available
						cm.vc_OrganGrowthIncrement[i_Organ] = (-1) * cm.vc_OrganBiomass[i_Organ]
					}
				} else if i_Organ == Organ_Shoot {
					incr := assimilate_partition_leaf * cm.vc_NetPhotosynthesis // should be negative
					if libc.fabs(incr) <= cm.vc_OrganBiomass[i_Organ] {
						cm.vc_OrganGrowthIncrement[i_Organ] = incr
					} else {
						cm.vc_OrganGrowthIncrement[i_Organ] = (-1) * cm.vc_OrganBiomass[i_Organ]
					}
				} else {
					// root or storage organ - do nothing in case of negative photosynthesis
					cm.vc_OrganGrowthIncrement[i_Organ] = 0
				}
			} else { 	// vc_NetPhotosynthesis >= 0.0
				cm.vc_OrganGrowthIncrement[i_Organ] =
					cm.vc_NetPhotosynthesis *
					(vc_AssimilatePartitioningCoeffOld +
							((vc_AssimilatePartitioningCoeff - vc_AssimilatePartitioningCoeffOld) *
									(cm.vc_CurrentTemperatureSum[cm.vc_DevelopmentalStage] /
											pc_StageTemperatureSum[cm.vc_DevelopmentalStage]))) *
					cm.vc_CropNRedux // [kg CH2O ha-1]

				_mobilization_from_storage := true
				if _mobilization_from_storage {
					if i_Organ != cm.vc_StorageOrgan {
						cm.vc_OrganGrowthIncrement[i_Organ] +=
							mobilization_from_storage *
							(vc_AssimilatePartitioningCoeffOld +
									((vc_AssimilatePartitioningCoeff -
												vc_AssimilatePartitioningCoeffOld) *
											(cm.vc_CurrentTemperatureSum[cm.vc_DevelopmentalStage] /
													pc_StageTemperatureSum[cm.vc_DevelopmentalStage]))) *
							cm.vc_CropNRedux
					} else {
						cm.vc_OrganGrowthIncrement[i_Organ] -=
							mobilization_from_storage * cm.vc_CropNRedux
						cm.vc_OrganGrowthIncrement[i_Organ] +=
							mobilization_from_storage *
							(vc_AssimilatePartitioningCoeffOld +
									((vc_AssimilatePartitioningCoeff -
												vc_AssimilatePartitioningCoeffOld) *
											(cm.vc_CurrentTemperatureSum[cm.vc_DevelopmentalStage] /
													pc_StageTemperatureSum[cm.vc_DevelopmentalStage]))) *
							cm.vc_CropNRedux
					}
				}
			}
			cm.vc_OrganSenescenceIncrement[i_Organ] =
				cm.vc_OrganGreenBiomass[i_Organ] *
				(pc_OrganSenescenceRate[prevStage][i_Organ] +
						((pc_OrganSenescenceRate[cm.vc_DevelopmentalStage][i_Organ] -
									pc_OrganSenescenceRate[prevStage][i_Organ]) *
								(cm.vc_CurrentTemperatureSum[cm.vc_DevelopmentalStage] /
										pc_StageTemperatureSum[cm.vc_DevelopmentalStage]))) // [kg CH2O ha-1]
		}

		cm.vc_OrganBiomass[i_Organ] += cm.vc_OrganGrowthIncrement[i_Organ] * cm.vc_TimeStep // [kg CH2O ha-1]
		if i_Organ == cm.vc_StorageOrgan {
			cm.vc_OrganDeadBiomass[i_Organ] +=
				cm.vc_OrganSenescenceIncrement[i_Organ] * cm.vc_TimeStep
		} else {
			// root, shoot, leaf
			reallocationRate :=
				pc_AssimilateReallocation *
				cm.vc_OrganSenescenceIncrement[i_Organ] *
				cm.vc_TimeStep
			cm.vc_OrganBiomass[cm.vc_StorageOrgan] += reallocationRate
			dailyDeadBiomassIncrement := cm.vc_OrganSenescenceIncrement[i_Organ] - reallocationRate

			if i_Organ == Organ_Root {
				cm.vc_OrganBiomass[Organ_Root] -= cm.vc_OrganSenescenceIncrement[Organ_Root]
				cm.vc_TotalBiomassNContent -= dailyDeadBiomassIncrement * cm.vc_NConcentrationRoot
				dailyDeadRootBiomassIncrement = dailyDeadBiomassIncrement
			} else {
				// shoot or leaf
				cm.vc_OrganBiomass[i_Organ] -= reallocationRate
				cm.vc_OrganDeadBiomass[i_Organ] += dailyDeadBiomassIncrement // [kg CH2O ha-1]
			}
		}

		cm.vc_OrganGreenBiomass[i_Organ] =
			cm.vc_OrganBiomass[i_Organ] - cm.vc_OrganDeadBiomass[i_Organ]
		if cm.vc_OrganGreenBiomass[i_Organ] < 0.0 {
			cm.vc_OrganDeadBiomass[i_Organ] = cm.vc_OrganBiomass[i_Organ]
			cm.vc_OrganGreenBiomass[i_Organ] = 0.0
		}

		if pc_AbovegroundOrgan[i_Organ] {
			cm.vc_AbovegroundBiomass += cm.vc_OrganBiomass[i_Organ]
		} else if i_Organ != Organ_Root {
			cm.vc_BelowgroundBiomass += cm.vc_OrganBiomass[i_Organ]
		}

		cm.vc_TotalBiomass += cm.vc_OrganBiomass[i_Organ]
	}

	// old @todo: N redux noch ausgeschaltet
	cm.vc_ReserveAssimilatePool = 0.0
	cm.vc_RootBiomassOld = cm.vc_RootBiomass
	cm.vc_RootBiomass = cm.vc_OrganBiomass[Organ_Root]

	if cm.vc_DevelopmentalStage > 0 {
		vc_MaxRootNConcentration =
			pc_StageMaxRootNConcentration[cm.vc_DevelopmentalStage - 1] -
			(pc_StageMaxRootNConcentration[cm.vc_DevelopmentalStage - 1] -
					pc_StageMaxRootNConcentration[cm.vc_DevelopmentalStage]) *
				cm.vc_CurrentTemperatureSum[cm.vc_DevelopmentalStage] /
				pc_StageTemperatureSum[cm.vc_DevelopmentalStage] // [kg kg-1]
	} else {
		vc_MaxRootNConcentration = pc_StageMaxRootNConcentration[cm.vc_DevelopmentalStage]
	}

	cm.vc_CropNDemand =
		((cm.vc_TargetNConcentration * cm.vc_AbovegroundBiomass) +
			(cm.vc_RootBiomass * vc_MaxRootNConcentration) +
			(cm.vc_TargetNConcentration * cm.vc_BelowgroundBiomass / pc_ResidueNRatio) -
			cm.vc_TotalBiomassNContent) *
		cm.vc_TimeStep // [kg ha-1]

	// vc_NConcentrationOptimum: computed but unused elsewhere in the C++ too -
	// a genuine dead store, kept for fidelity.
	vc_NConcentrationOptimum :=
		((cm.vc_TargetNConcentration -
					(cm.vc_TargetNConcentration - cm.vc_CriticalNConcentration) * 0.15) *
				cm.vc_AbovegroundBiomass +
			(cm.vc_TargetNConcentration -
					(cm.vc_TargetNConcentration - cm.vc_CriticalNConcentration) * 0.15) *
				cm.vc_BelowgroundBiomass /
				pc_ResidueNRatio +
			(cm.vc_RootBiomass * vc_MaxRootNConcentration) -
			cm.vc_TotalBiomassNContent) *
		cm.vc_TimeStep // [kg ha-1]
	_ = vc_NConcentrationOptimum

	if cm.vc_CropNDemand > (pc_MaxCropNDemand * cm.vc_TimeStep) {
		// Not more than 6kg N per day to be taken up.
		cm.vc_CropNDemand = pc_MaxCropNDemand * cm.vc_TimeStep
	}

	if cm.vc_CropNDemand < 0 {
		cm.vc_CropNDemand = 0.0
	}

	// vc_RootNIncrement: computed but unused elsewhere in the C++ too - a
	// genuine dead store, kept for fidelity.
	if cm.vc_RootBiomass < cm.vc_RootBiomassOld {
		vc_RootNIncrement = (cm.vc_RootBiomassOld - cm.vc_RootBiomass) * cm.vc_NConcentrationRoot
	} else {
		vc_RootNIncrement = 0
	}
	_ = vc_RootNIncrement

	layerIndexBelowRootingDepth := min(cm.vc_RootingDepth, nols - 1)
	vc_AvailableWaterPercentage := 0.0
	if cropPs.__enable_PASW_root_penetration__ {
		// In case of drought stress the root will grow deeper
		vc_AvailableWater :=
			soilColumn.layers[layerIndexBelowRootingDepth].vs_FieldCapacity -
			soilColumn.layers[layerIndexBelowRootingDepth].vs_PermanentWiltingPoint
		vc_AvailableWaterPercentage =
			(soilColumn.layers[layerIndexBelowRootingDepth].vs_SoilMoisture_m3 -
				soilColumn.layers[layerIndexBelowRootingDepth].vs_PermanentWiltingPoint) /
			vc_AvailableWater
		if vc_AvailableWaterPercentage < 0.0 {
			vc_AvailableWaterPercentage = 0.0
		}
	} else {
		if cm.vc_TranspirationDeficit <
			   (0.95 * pc_DroughtStressThreshold[cm.vc_DevelopmentalStage]) &&
		   pc_CropSpecificMaxRootingDepth >= 0.8 &&
		   cm.vc_RootingDepth_m > 0.95 * cm.vc_MaxRootingDepth &&
		   cm.vc_DevelopmentalStage < cm.noOfDevStages - 1 { 	// only if crop-specific max rooting depth is deeper than 80cm
			cm.vc_MaxRootingDepth += 0.005
		}
	}

	if cm.vc_MaxRootingDepth > (f64(nols - 1) * layerThickness) {
		cm.vc_MaxRootingDepth = f64(nols - 1) * layerThickness
	}

	// restrict root growth to everything above the impenetrable layer
	if vs_ImpenetrableLayerDepth > 0 {
		cm.vc_MaxRootingDepth = min(cm.vc_MaxRootingDepth, vs_ImpenetrableLayerDepth)
	}

	// Pedersen et al. (2010): Modelling diverse root density dynamics and
	// deep nitrogen uptake - a simple approach. Plant & Soil 326, 493-510.

	// Determining temperature sum for root growth
	pc_MaximumTemperatureRootGrowth := pc_MinimumTemperatureRootGrowth + 20.0
	vc_DailyTemperatureRoot := 0.0
	if vw_MeanAirTemperature >= pc_MaximumTemperatureRootGrowth {
		vc_DailyTemperatureRoot = pc_MaximumTemperatureRootGrowth - pc_MinimumTemperatureRootGrowth
	} else {
		vc_DailyTemperatureRoot = vw_MeanAirTemperature - pc_MinimumTemperatureRootGrowth
	}
	if vc_DailyTemperatureRoot < 0.0 {
		vc_DailyTemperatureRoot = 0.0
	}
	cm.vc_CurrentTotalTemperatureSumRoot += vc_DailyTemperatureRoot

	// Determining root penetration rate according to soil clay content [m degC-1 d-1]
	vc_RootPenetrationRate := 0.0
	if soilColumn.layers[layerIndexBelowRootingDepth].vs_SoilClayContent <= 0.02 {
		vc_RootPenetrationRate = 0.5 * pc_RootPenetrationRate
	} else if soilColumn.layers[layerIndexBelowRootingDepth].vs_SoilClayContent <= 0.08 {
		vc_RootPenetrationRate =
			((1.0 / 3.0) +
				(0.5 / 0.06 * soilColumn.layers[layerIndexBelowRootingDepth].vs_SoilClayContent)) *
			pc_RootPenetrationRate
	} else {
		vc_RootPenetrationRate = pc_RootPenetrationRate
	}
	if cropPs.__enable_PASW_root_penetration__ {
		if vc_AvailableWaterPercentage <= 0.25 {
			vc_RootPenetrationRate =
				min(1.0, 4 * vc_AvailableWaterPercentage) * vc_RootPenetrationRate
		} else {
			vc_RootPenetrationRate = pc_RootPenetrationRate
		}
	}

	// Calculating rooting depth [m]
	if cm.vc_CurrentTotalTemperatureSumRoot <= pc_RootGrowthLag {
		cm.vc_RootingDepth_m = pc_InitialRootingDepth
	} else {
		cm.vc_RootingDepth_m += vc_DailyTemperatureRoot * vc_RootPenetrationRate
	}

	if cm.vc_RootingDepth_m <= pc_InitialRootingDepth {
		cm.vc_RootingDepth_m = pc_InitialRootingDepth
	}
	if cm.vc_RootingDepth_m > cm.vc_MaxRootingDepth {
		cm.vc_RootingDepth_m = cm.vc_MaxRootingDepth
	}
	if cm.vc_RootingDepth_m > vs_MaxEffectiveRootingDepth {
		cm.vc_RootingDepth_m = vs_MaxEffectiveRootingDepth
	}

	cm.vc_RootingDepth = min(int(libc.round(cm.vc_RootingDepth_m / layerThickness)), nols) // layer no
	cm.vc_RootingZone = min(int(libc.round(1.3 * cm.vc_RootingDepth_m / layerThickness)), nols) // layer no

	cm.vc_TotalRootLength = cm.vc_RootBiomass * pc_SpecificRootLength // [m m-2]

	// Calculating a root density distribution factor []
	vc_RootDensityFactor, vc_RootDensityFactorSum := calc_root_density_factor_and_sum(
		cm,
		allocator,
	)

	// calculate the distribution of dead root biomass (for later addition
	// into AOM pools in soil-organic)
	if !cropPs.__disable_daily_root_biomass_to_soil__ {
		fc_move_dead_root_biomass_to_soil(
			cm,
			dailyDeadRootBiomassIncrement,
			vc_RootDensityFactorSum,
			vc_RootDensityFactor,
			allocator,
		)
	}

	// Calculating root density per layer from total root length and a
	// relative root density distribution factor
	for i_Layer := 0; i_Layer < cm.vc_RootingZone; i_Layer += 1 {
		cm.vc_RootDensity[i_Layer] =
			(vc_RootDensityFactor[i_Layer] / vc_RootDensityFactorSum) * cm.vc_TotalRootLength // [m m-3]
	}

	for i_Layer := 0; i_Layer < cm.vc_RootingZone; i_Layer += 1 {
		// Root diameter [m]
		if pc_AbovegroundOrgan[3] {
			cm.vc_RootDiameter[i_Layer] = 0.0002 - (f64(i_Layer + 1) * 0.00001)
		} else {
			cm.vc_RootDiameter[i_Layer] = 0.0001
		}
	}

	// Limiting the maximum N-uptake to 26-13*10^-14 mol/cm root/sec
	cm.vc_MaxNUptake =
		pc_MaxNUptakeParam - (cm.vc_CurrentTotalTemperatureSum / cm.vc_TotalTemperatureSum) // [kg m root-1]

	if (cm.vc_CropNDemand / 10000.0) >
	   (cm.vc_TotalRootLength * cm.vc_MaxNUptake * cm.vc_TimeStep) {
		cm.vc_CropNDemand = cm.vc_TotalRootLength * cm.vc_MaxNUptake * cm.vc_TimeStep // [kg m-2]
	} else {
		cm.vc_CropNDemand = cm.vc_CropNDemand / 10000.0 // [kg ha-1 -> kg m-2]
	}
}

// ---------------------------------------------------------------------------
// Phase 5 checkpoint 6: water + nitrogen uptake
//
// Ported: fcReferenceEvapotranspiration, fcCropWaterUptake, fcCropNUptake,
// getEffectiveRootingDepth (deferred here from checkpoint 3 - it reads
// vc_RootEffectivity, populated by fcCropWaterUptake below, so this is where
// it's actually meaningful to test). Together with checkpoint 5's
// fcCropNitrogen (root growth + N-redux factor) and fcCropDryMatter, this
// closes out every cropmodule:: function step() calls in its
// vc_DevelopmentalStage>0 block except the fireEvent-driven bookkeeping
// (checkpoint 7).
// ---------------------------------------------------------------------------

// C++: double monica::cropmodule::fcReferenceEvapotranspiration(CropModule*,
// double vw_MaxAirTemperature, double vw_MinAirTemperature, double
// vw_RelativeHumidity, double vw_MeanAirTemperature, double vw_WindSpeed,
// double vw_WindSpeedHeight, double vw_AtmosphericCO2Concentration)
//
// FAO-56 Penman-Monteith reference evapotranspiration (Allen, Pereira, Raes
// & Smith 1998, FAO Irrigation and Drainage Paper 56).
fc_reference_evapotranspiration :: proc(
	cm: ^Crop_Module,
	vw_MaxAirTemperature, vw_MinAirTemperature, vw_RelativeHumidity, vw_MeanAirTemperature: f64,
	vw_WindSpeed, vw_WindSpeedHeight, vw_AtmosphericCO2Concentration: f64,
) -> f64 {
	cropPs := cm.cropModParams
	pc_CarboxylationPathway := cm.cropParams.speciesParams.pc_CarboxylationPathway
	vs_HeightNN := cm.siteParams.vs_HeightNN

	pc_SaturationBeta := cropPs.pc_SaturationBeta // Yu et al. 2001; beta = 3.5
	pc_StomataConductanceAlpha := cropPs.pc_StomataConductanceAlpha // Yu et al. 2001; alpha = 0.06
	pc_ReferenceAlbedo := cropPs.pc_ReferenceAlbedo // FAO green grass reference albedo, Allen et al. 1998

	// Calculation of atmospheric pressure
	vc_AtmosphericPressure := 101.3 * libc.pow((293.0 - (0.0065 * vs_HeightNN)) / 293.0, 5.26)

	// Calculation of psychrometer constant
	vc_PsycrometerConstant := 0.000665 * vc_AtmosphericPressure

	// Calc. of saturated water vapour pressure at daily max/min temperature
	vc_SaturatedVapourPressureMax :=
		0.6108 * libc.exp((17.27 * vw_MaxAirTemperature) / (237.3 + vw_MaxAirTemperature))
	vc_SaturatedVapourPressureMin :=
		0.6108 * libc.exp((17.27 * vw_MinAirTemperature) / (237.3 + vw_MinAirTemperature))

	vc_SaturatedVapourPressure :=
		(vc_SaturatedVapourPressureMax + vc_SaturatedVapourPressureMin) / 2.0

	vc_VapourPressure: f64
	if vw_RelativeHumidity <= 0.0 {
		// Assuming Tdew = Tmin as suggested in FAO56 Allen et al. 1998
		vc_VapourPressure = vc_SaturatedVapourPressureMin
	} else {
		vc_VapourPressure = vw_RelativeHumidity * vc_SaturatedVapourPressure
	}

	vc_SaturationDeficit := vc_SaturatedVapourPressure - vc_VapourPressure

	// Slope of saturation water vapour pressure-to-temperature relation
	vc_SaturatedVapourPressureSlope :=
		(4098.0 *
			(0.6108 *
					libc.exp((17.27 * vw_MeanAirTemperature) / (vw_MeanAirTemperature + 237.3)))) /
		((vw_MeanAirTemperature + 237.3) * (vw_MeanAirTemperature + 237.3))

	// 0.5 minimum allowed windspeed for Penman-Monteith-Method FAO
	vc_WindSpeed_2m := max(0.5, vw_WindSpeed * (4.87 / libc.log(67.8 * vw_WindSpeedHeight - 5.42)))

	vc_AerodynamicResistance := 208.0 / vc_WindSpeed_2m

	if cm.vc_GrossPhotosynthesisReference_mol <= 0.0 {
		cm.vc_StomataResistance = 999999.9
	} else {
		if pc_CarboxylationPathway == 1 {
			cm.vc_StomataResistance =
				(vw_AtmosphericCO2Concentration *
					(1.0 + vc_SaturationDeficit / pc_SaturationBeta)) /
				(pc_StomataConductanceAlpha * cm.vc_GrossPhotosynthesisReference_mol)
		} else {
			cm.vc_StomataResistance =
				(vw_AtmosphericCO2Concentration *
					(1.0 + vc_SaturationDeficit / pc_SaturationBeta)) /
				(pc_StomataConductanceAlpha * cm.vc_GrossPhotosynthesisReference_mol)
		}
	}

	vc_SurfaceResistance := cm.vc_StomataResistance / 1.44

	vc_ClearSkyShortwaveRadiation :=
		(0.75 + 0.00002 * vs_HeightNN) * cm.vc_ExtraterrestrialRadiation

	vc_RelativeShortwaveRadiation :=
		vc_ClearSkyShortwaveRadiation > 0 ? cm.vc_GlobalRadiation / vc_ClearSkyShortwaveRadiation : 0.0

	vc_NetShortwaveRadiation := (1.0 - pc_ReferenceAlbedo) * cm.vc_GlobalRadiation

	pc_BolzmanConstant :: 0.0000000049 // 4.903 * 10^-9 MJ m-2 K-4 d-1
	vw_NetRadiation :=
		vc_NetShortwaveRadiation -
		(pc_BolzmanConstant *
				(libc.pow(vw_MinAirTemperature + 273.16, 4.0) +
						libc.pow(vw_MaxAirTemperature + 273.16, 4.0)) /
				2.0 *
				(1.35 * vc_RelativeShortwaveRadiation - 0.35) *
				(0.34 - 0.14 * libc.sqrt(vc_VapourPressure)))

	// Penman-Monteith-Method FAO
	vc_ReferenceEvapotranspiration :=
		((0.408 * vc_SaturatedVapourPressureSlope * vw_NetRadiation) +
			(vc_PsycrometerConstant *
					(900.0 / (vw_MeanAirTemperature + 273.0)) *
					vc_WindSpeed_2m *
					vc_SaturationDeficit)) /
		(vc_SaturatedVapourPressureSlope +
				vc_PsycrometerConstant * (1.0 + (vc_SurfaceResistance / vc_AerodynamicResistance)))

	if vc_ReferenceEvapotranspiration < 0.0 {
		vc_ReferenceEvapotranspiration = 0.0
	}

	return vc_ReferenceEvapotranspiration
}

// C++: void monica::cropmodule::fcCropWaterUptake(CropModule*, size_t
// vc_GroundwaterTable, double vw_GrossPrecipitation, double, double)
//
// Water uptake by the crop: potential transpiration from potential
// evapotranspiration by soil cover fraction, reduced by water availability
// in the soil according to actual water contents, root distribution and root
// effectivity. The trailing two C++ parameters are unused in the C++ body
// too (their names are commented out there) - kept for signature fidelity
// with step()'s call site.
fc_crop_water_uptake :: proc(
	cm: ^Crop_Module,
	vc_GroundwaterTable: int,
	vw_GrossPrecipitation: f64,
	_vc_CurrentTotalTemperatureSum: f64,
	_vc_TotalTemperatureSum: f64,
) {
	soilColumn := cm.soilColumn
	pc_WaterDeficitResponseOn := cm.simParams.pc_WaterDeficitResponseOn
	vs_MaxEffectiveRootingDepth := cm.siteParams.vs_MaxEffectiveRootingDepth

	nols := len(soilColumn.layers)
	layerThickness := soilColumn.layers[0].vs_LayerThickness
	cm.vc_PotentialTranspirationDeficit = 0.0 // [mm]
	cm.vc_PotentialTranspiration = 0.0 // old TRAMAX [mm]
	vc_PotentialEvapotranspiration := 0.0 // [mm]
	cm.vc_TranspirationReduced = 0.0 // old TDRED [mm]
	cm.vc_ActualTranspiration = 0.0 // [mm]
	vc_RemainingTotalRootEffectivity := 0.0 // old WEFFREST [m]
	vc_CropWaterUptakeFromGroundwater := 0.0 // old GAUF [mm]
	vc_TotalRootEffectivity := 0.0 // old WEFF [m]
	cm.vc_ActualTranspirationDeficit = 0.0 // old TREST [mm]
	vc_Interception := 0.0
	cm.vc_RemainingEvapotranspiration = 0.0

	for i_Layer := 0; i_Layer < nols; i_Layer += 1 {
		cm.vc_Transpiration[i_Layer] = 0.0 // old TP [mm]
		cm.vc_TranspirationRedux[i_Layer] = 0.0 // old TRRED []
		cm.vc_RootEffectivity[i_Layer] = 0.0 // old WUEFF [?]
	}

	// --- Interception ---
	vc_InterceptionStorageOld := cm.vc_InterceptionStorage

	// Interception in [mm d-1]
	vc_Interception = (2.5 * cm.vc_CropHeight * cm.vc_SoilCoverage) - cm.vc_InterceptionStorage
	if vc_Interception < 0 {
		vc_Interception = 0.0
	}
	// If no precipitation occurs, interception = 0
	if vw_GrossPrecipitation <= 0 {
		vc_Interception = 0.0
	}

	// Calculating net precipitation and adding to surface water
	if vw_GrossPrecipitation <= vc_Interception {
		vc_Interception = vw_GrossPrecipitation
		cm.vc_NetPrecipitation = 0.0
	} else {
		cm.vc_NetPrecipitation = vw_GrossPrecipitation - vc_Interception
	}

	// add intercepted precipitation to the virtual interception water storage
	cm.vc_InterceptionStorage = vc_InterceptionStorageOld + vc_Interception

	// --- Transpiration ---
	vc_PotentialEvapotranspiration = cm.vc_ReferenceEvapotranspiration * cm.vc_KcFactor // [mm]

	// from HERMES
	if vc_PotentialEvapotranspiration > 6.5 {
		vc_PotentialEvapotranspiration = 6.5
	}

	cm.vc_RemainingEvapotranspiration = vc_PotentialEvapotranspiration // [mm]

	// If crop holds intercepted water, first evaporation from crop surface
	if cm.vc_InterceptionStorage > 0.0 {
		if cm.vc_RemainingEvapotranspiration >= cm.vc_InterceptionStorage {
			cm.vc_RemainingEvapotranspiration -= cm.vc_InterceptionStorage
			cm.vc_EvaporatedFromIntercept = cm.vc_InterceptionStorage
			cm.vc_InterceptionStorage = 0.0
		} else {
			cm.vc_InterceptionStorage -= cm.vc_RemainingEvapotranspiration
			cm.vc_EvaporatedFromIntercept = cm.vc_RemainingEvapotranspiration
			cm.vc_RemainingEvapotranspiration = 0.0
		}
	} else {
		cm.vc_EvaporatedFromIntercept = 0.0
	}

	// if the plant has matured, no transpiration occurs!
	if cm.vc_DevelopmentalStage < cm.vc_FinalDevelopmentalStage {
		cm.vc_PotentialTranspiration = cm.vc_RemainingEvapotranspiration * cm.vc_SoilCoverage // [mm]

		for i_Layer := 0; i_Layer < cm.vc_RootingZone; i_Layer += 1 {
			vc_AvailableWater :=
				soilColumn.layers[i_Layer].vs_FieldCapacity -
				soilColumn.layers[i_Layer].vs_PermanentWiltingPoint
			vc_AvailableWaterPercentage :=
				(soilColumn.layers[i_Layer].vs_SoilMoisture_m3 -
					soilColumn.layers[i_Layer].vs_PermanentWiltingPoint) /
				vc_AvailableWater
			if vc_AvailableWaterPercentage < 0.0 {
				vc_AvailableWaterPercentage = 0.0
			}
			// MP: access point for drought optimisation (this could be
			// extended for waterlogging); an alternative approach for
			// compensatory effects is a soil water-dependent root
			// penetration rate.
			switch {
			case vc_AvailableWaterPercentage < 0.15:
				cm.vc_TranspirationRedux[i_Layer] = vc_AvailableWaterPercentage * 3.0 // []
				cm.vc_RootEffectivity[i_Layer] = 0.15 + 0.45 * vc_AvailableWaterPercentage / 0.15 // [] MP: essentially *3
			case vc_AvailableWaterPercentage < 0.3:
				cm.vc_TranspirationRedux[i_Layer] =
					0.45 + (0.25 * (vc_AvailableWaterPercentage - 0.15) / 0.15)
				cm.vc_RootEffectivity[i_Layer] =
					0.6 + (0.2 * (vc_AvailableWaterPercentage - 0.15) / 0.15)
			case vc_AvailableWaterPercentage < 0.5:
				// MP: ab hier hat das fast keinen Effekt mehr
				cm.vc_TranspirationRedux[i_Layer] =
					0.7 + (0.275 * (vc_AvailableWaterPercentage - 0.3) / 0.2)
				cm.vc_RootEffectivity[i_Layer] =
					0.8 + (0.2 * (vc_AvailableWaterPercentage - 0.3) / 0.2)
			case vc_AvailableWaterPercentage < 0.75:
				// MP: ab hier ist nur mehr die Transpiration betroffen
				cm.vc_TranspirationRedux[i_Layer] =
					0.975 + (0.025 * (vc_AvailableWaterPercentage - 0.5) / 0.25)
				cm.vc_RootEffectivity[i_Layer] = 1.0
			case:
				cm.vc_TranspirationRedux[i_Layer] = 1.0
				cm.vc_RootEffectivity[i_Layer] = 1.0
			}
			if cm.vc_TranspirationRedux[i_Layer] < 0 {
				cm.vc_TranspirationRedux[i_Layer] = 0.0
			}
			if cm.vc_RootEffectivity[i_Layer] < 0 {
				cm.vc_RootEffectivity[i_Layer] = 0.0
			}
			if i_Layer == vc_GroundwaterTable { 	// old GRW
				cm.vc_RootEffectivity[i_Layer] = 0.5
			}
			if i_Layer > vc_GroundwaterTable { 	// old GRW
				cm.vc_RootEffectivity[i_Layer] = 0.0
			}
			if f64(i_Layer + 1) * layerThickness >= vs_MaxEffectiveRootingDepth {
				cm.vc_RootEffectivity[i_Layer] = 0.0
			}

			vc_TotalRootEffectivity += cm.vc_RootEffectivity[i_Layer] * cm.vc_RootDensity[i_Layer] // [m m-3]
			vc_RemainingTotalRootEffectivity = vc_TotalRootEffectivity
		}

		// [TRANSPLANT SHOCK] Water Uptake Limitation.
		if cm.vc_TransplantEfficiency < 1.0 {
			vc_TotalRootEffectivity *= cm.vc_TransplantEfficiency
			vc_RemainingTotalRootEffectivity = vc_TotalRootEffectivity
		}

		for i_Layer := 0; i_Layer < nols; i_Layer += 1 {
			if i_Layer > min(cm.vc_RootingZone, vc_GroundwaterTable + 1) {
				cm.vc_Transpiration[i_Layer] = 0.0 // [mm]
			} else {
				if vc_TotalRootEffectivity != 0.0 {
					cm.vc_Transpiration[i_Layer] =
						cm.vc_PotentialTranspiration *
						((cm.vc_RootEffectivity[i_Layer] * cm.vc_RootDensity[i_Layer]) /
								vc_TotalRootEffectivity) *
						cm.vc_OxygenDeficit
				} else {
					// MP: why is this not changing anything? (probably only
					// matters for too-dry conditions)
					cm.vc_Transpiration[i_Layer] = 0
				}
			}
		}

		for i_Layer := 0; i_Layer < min(cm.vc_RootingZone, vc_GroundwaterTable + 1); i_Layer += 1 {
			vc_RemainingTotalRootEffectivity -=
				cm.vc_RootEffectivity[i_Layer] * cm.vc_RootDensity[i_Layer] // [m m-3]

			if vc_RemainingTotalRootEffectivity <= 0.0 {
				vc_RemainingTotalRootEffectivity = 0.00001
			}
			if ((cm.vc_Transpiration[i_Layer] / 1000.0) / layerThickness) >
			   (soilColumn.layers[i_Layer].vs_SoilMoisture_m3 -
					   soilColumn.layers[i_Layer].vs_PermanentWiltingPoint) {
				cm.vc_PotentialTranspirationDeficit =
					(((cm.vc_Transpiration[i_Layer] / 1000.0) / layerThickness) -
						(soilColumn.layers[i_Layer].vs_SoilMoisture_m3 -
								soilColumn.layers[i_Layer].vs_PermanentWiltingPoint)) *
					layerThickness *
					1000.0 // [mm]
				if cm.vc_PotentialTranspirationDeficit < 0.0 {
					cm.vc_PotentialTranspirationDeficit = 0.0
				}
				if cm.vc_PotentialTranspirationDeficit > cm.vc_Transpiration[i_Layer] {
					cm.vc_PotentialTranspirationDeficit = cm.vc_Transpiration[i_Layer] // [mm]
				}
			} else {
				cm.vc_PotentialTranspirationDeficit = 0.0
			}
			cm.vc_TranspirationReduced =
				cm.vc_Transpiration[i_Layer] * (1.0 - cm.vc_TranspirationRedux[i_Layer])

			// MP: this is a key line for water stress response
			cm.vc_ActualTranspirationDeficit = max(
				cm.vc_TranspirationReduced,
				cm.vc_PotentialTranspirationDeficit,
			) // [mm]
			if cm.vc_ActualTranspirationDeficit > 0.0 {
				if i_Layer < min(cm.vc_RootingZone, vc_GroundwaterTable + 1) {
					for i_Layer2 := i_Layer + 1;
					    i_Layer2 < min(cm.vc_RootingZone, vc_GroundwaterTable + 1);
					    i_Layer2 += 1 {
						cm.vc_Transpiration[i_Layer2] +=
							cm.vc_ActualTranspirationDeficit *
							(cm.vc_RootEffectivity[i_Layer2] *
									cm.vc_RootDensity[i_Layer2] /
									vc_RemainingTotalRootEffectivity)
					}
				}
			}
			cm.vc_Transpiration[i_Layer] =
				cm.vc_Transpiration[i_Layer] - cm.vc_ActualTranspirationDeficit
			if cm.vc_Transpiration[i_Layer] < 0.0 {
				cm.vc_Transpiration[i_Layer] = 0.0
			}
			cm.vc_ActualTranspiration += cm.vc_Transpiration[i_Layer]
			if i_Layer == vc_GroundwaterTable {
				vc_CropWaterUptakeFromGroundwater =
					(cm.vc_Transpiration[i_Layer] / 1000.0) / layerThickness // [m3 m-3]
			}
		}
		if cm.vc_PotentialTranspiration > 0 {
			cm.vc_TranspirationDeficit = cm.vc_ActualTranspiration / cm.vc_PotentialTranspiration
		} else {
			cm.vc_TranspirationDeficit = 1.0
		}

		vm_GroundwaterDistance := vc_GroundwaterTable - cm.vc_RootingDepth
		if vm_GroundwaterDistance <= 1 {
			cm.vc_TranspirationDeficit = 1.0
		}
		if !pc_WaterDeficitResponseOn {
			cm.vc_TranspirationDeficit = 1.0
		}
	}
	// vc_CropWaterUptakeFromGroundwater: computed but unused elsewhere in the
	// C++ too - a genuine dead store, kept for fidelity.
	_ = vc_CropWaterUptakeFromGroundwater
}

// C++: void monica::cropmodule::fcCropNUptake(CropModule*, size_t
// vc_GroundwaterTable, double, double)
//
// The trailing two C++ parameters are unused in the C++ body too (their
// names are commented out there) - kept for signature fidelity.
fc_crop_n_uptake :: proc(
	cm: ^Crop_Module,
	vc_GroundwaterTable: int,
	_vc_CurrentTotalTemperatureSum: f64,
	_vc_TotalTemperatureSum: f64,
) {
	cropPs := cm.cropModParams
	soilColumn := cm.soilColumn
	pc_PartBiologicalNFixation := cm.cropParams.speciesParams.pc_PartBiologicalNFixation
	pc_ResidueNRatio := cm.cropParams.cultivarParams.pc_ResidueNRatio
	pc_StageMaxRootNConcentration := cm.cropParams.speciesParams.pc_StageMaxRootNConcentration
	pc_Tortuosity := cm.cropModParams.pc_Tortuosity

	nols := len(soilColumn.layers)
	layerThickness := soilColumn.layers[0].vs_LayerThickness

	vc_ConvectiveNUptake := 0.0 // old TRNSUM
	vc_DiffusiveNUptake := 0.0 // old SUMDIFF
	vc_ConvectiveNUptakeFromLayer := make([dynamic]f64, nols, context.temp_allocator) // old MASS

	vc_DiffusionCoeff := make([dynamic]f64, nols, context.temp_allocator) // old D
	vc_DiffusiveNUptakeFromLayer := make([dynamic]f64, nols, context.temp_allocator) // old DIFF
	vc_ConvectiveNUptake_1 := 0.0 // old MASSUM
	vc_DiffusiveNUptake_1 := 0.0 // old DIFFSUM
	pc_MinimumAvailableN := cropPs.pc_MinimumAvailableN // kg m-3
	pc_MinimumNConcentrationRoot := cropPs.pc_MinimumNConcentrationRoot // kg kg-1
	pc_MaxCropNDemand := cropPs.pc_MaxCropNDemand

	cm.vc_TotalNUptake = 0.0
	cm.vc_TotalNInput = 0.0
	cm.vc_FixedN = 0.0
	for &v in cm.vc_NUptakeFromLayer {
		v = 0.0
	}

	PI :: 3.14159265358979323

	// if the plant has matured, no N uptake occurs!
	if cm.vc_DevelopmentalStage < cm.vc_FinalDevelopmentalStage {
		for i_Layer := 0; i_Layer < min(cm.vc_RootingZone, vc_GroundwaterTable); i_Layer += 1 {
			cm.vs_SoilMineralNContent[i_Layer] = soilColumn.layers[i_Layer].vs_SoilNO3 // [kg m-3]

			// Convective N uptake per layer
			// ([mm -> m]) * ([kg m-3] / old WG [m3 m-3]) -> [kg m-2]
			vc_ConvectiveNUptakeFromLayer[i_Layer] =
				(cm.vc_Transpiration[i_Layer] / 1000.0) *
				(cm.vs_SoilMineralNContent[i_Layer] /
						soilColumn.layers[i_Layer].vs_SoilMoisture_m3) *
				cm.vc_TimeStep

			vc_ConvectiveNUptake += vc_ConvectiveNUptakeFromLayer[i_Layer] // [kg m-2]

			vc_DiffusionCoeff[i_Layer] =
				0.000214 *
				(pc_Tortuosity * libc.exp(soilColumn.layers[i_Layer].vs_SoilMoisture_m3 * 10)) /
				soilColumn.layers[i_Layer].vs_SoilMoisture_m3 // [m2 d-1]

			// ([m2 d-1] * [m3 m-3] * [m] * ([kg m-3])=[m3 m-3])=[m m-3] -> [kg m-2]
			vc_DiffusiveNUptakeFromLayer[i_Layer] =
				(vc_DiffusionCoeff[i_Layer] *
					soilColumn.layers[i_Layer].vs_SoilMoisture_m3 *
					2.0 *
					PI *
					cm.vc_RootDiameter[i_Layer] *
					(cm.vs_SoilMineralNContent[i_Layer] /
								1000.0 /
								soilColumn.layers[i_Layer].vs_SoilMoisture_m3 -
							0.000014) *
					libc.sqrt(PI * cm.vc_RootDensity[i_Layer])) *
				cm.vc_RootDensity[i_Layer] *
				1000.0 *
				cm.vc_TimeStep

			if vc_DiffusiveNUptakeFromLayer[i_Layer] < 0.0 {
				vc_DiffusiveNUptakeFromLayer[i_Layer] = 0
			}

			vc_DiffusiveNUptake += vc_DiffusiveNUptakeFromLayer[i_Layer] // [kg m-2]
		}

		for i_Layer := 0; i_Layer < min(cm.vc_RootingZone, vc_GroundwaterTable); i_Layer += 1 {
			if cm.vc_CropNDemand > 0.0 {
				if vc_ConvectiveNUptake >= cm.vc_CropNDemand {
					// convective N uptake is sufficient
					cm.vc_NUptakeFromLayer[i_Layer] =
						cm.vc_CropNDemand *
						vc_ConvectiveNUptakeFromLayer[i_Layer] /
						vc_ConvectiveNUptake
				} else {
					// N demand is not covered
					if (cm.vc_CropNDemand - vc_ConvectiveNUptake) < vc_DiffusiveNUptake {
						cm.vc_NUptakeFromLayer[i_Layer] =
							vc_ConvectiveNUptakeFromLayer[i_Layer] +
							((cm.vc_CropNDemand - vc_ConvectiveNUptake) *
									vc_DiffusiveNUptakeFromLayer[i_Layer] /
									vc_DiffusiveNUptake)
					} else {
						cm.vc_NUptakeFromLayer[i_Layer] =
							vc_ConvectiveNUptakeFromLayer[i_Layer] +
							vc_DiffusiveNUptakeFromLayer[i_Layer]
					}
				}

				vc_ConvectiveNUptake_1 += vc_ConvectiveNUptakeFromLayer[i_Layer]
				vc_DiffusiveNUptake_1 += vc_DiffusiveNUptakeFromLayer[i_Layer]

				if cm.vc_NUptakeFromLayer[i_Layer] >
				   ((cm.vs_SoilMineralNContent[i_Layer] * layerThickness) - pc_MinimumAvailableN) {
					cm.vc_NUptakeFromLayer[i_Layer] =
						(cm.vs_SoilMineralNContent[i_Layer] * layerThickness) -
						pc_MinimumAvailableN
				}

				if cm.vc_NUptakeFromLayer[i_Layer] > (pc_MaxCropNDemand / 10000.0 * 0.75) {
					cm.vc_NUptakeFromLayer[i_Layer] = pc_MaxCropNDemand / 10000.0 * 0.75
				}

				if cm.vc_NUptakeFromLayer[i_Layer] < 0.0 {
					cm.vc_NUptakeFromLayer[i_Layer] = 0.0
				}
			} else {
				cm.vc_NUptakeFromLayer[i_Layer] = 0.0
			}

			cm.vc_TotalNUptake += cm.vc_NUptakeFromLayer[i_Layer] * 10000.0 // [kg m-2] -> [kg ha-1]
		}

		// Biological N Fixation - part of the deficit which can be covered
		// by biological N fixation.
		cm.vc_FixedN = pc_PartBiologicalNFixation * cm.vc_CropNDemand * 10000.0 // [kg N ha-1]

		if ((cm.vc_CropNDemand * 10000.0) - cm.vc_TotalNUptake) < cm.vc_FixedN {
			cm.vc_TotalNInput = cm.vc_CropNDemand * 10000.0
			cm.vc_FixedN = (cm.vc_CropNDemand * 10000.0) - cm.vc_TotalNUptake
		} else {
			cm.vc_TotalNInput = cm.vc_TotalNUptake + cm.vc_FixedN
		}
	}
	// vc_ConvectiveNUptake_1/vc_DiffusiveNUptake_1: computed but unused
	// elsewhere in the C++ too - genuine dead stores, kept for fidelity.
	_ = vc_ConvectiveNUptake_1
	_ = vc_DiffusiveNUptake_1

	cm.vc_SumTotalNUptake += cm.vc_TotalNUptake
	cm.vc_TotalBiomassNContent += cm.vc_TotalNInput

	if cm.vc_RootBiomass > cm.vc_RootBiomassOld {
		// root has been growing
		cm.vc_NConcentrationRoot =
			((cm.vc_RootBiomassOld * cm.vc_NConcentrationRoot) +
				((cm.vc_RootBiomass - cm.vc_RootBiomassOld) /
						(cm.vc_AbovegroundBiomass -
								cm.vc_AbovegroundBiomassOld +
								cm.vc_BelowgroundBiomass -
								cm.vc_BelowgroundBiomassOld +
								cm.vc_RootBiomass -
								cm.vc_RootBiomassOld) *
						cm.vc_TotalNInput)) /
			cm.vc_RootBiomass

		cm.vc_NConcentrationRoot = tl.bound(
			pc_MinimumNConcentrationRoot,
			cm.vc_NConcentrationRoot,
			pc_StageMaxRootNConcentration[cm.vc_DevelopmentalStage],
		)
	}

	cm.vc_NConcentrationAbovegroundBiomass =
		(cm.vc_TotalBiomassNContent - (cm.vc_RootBiomass * cm.vc_NConcentrationRoot)) /
		(cm.vc_AbovegroundBiomass + (cm.vc_BelowgroundBiomass / pc_ResidueNRatio))

	if (cm.vc_NConcentrationAbovegroundBiomass * cm.vc_AbovegroundBiomass) <
	   (cm.vc_NConcentrationAbovegroundBiomassOld * cm.vc_AbovegroundBiomassOld) {
		tempNConcentrationAbovegroundBiomass :=
			cm.vc_NConcentrationAbovegroundBiomassOld *
			cm.vc_AbovegroundBiomassOld /
			cm.vc_AbovegroundBiomass

		tempNConcentrationRoot :=
			(cm.vc_TotalBiomassNContent -
				(cm.vc_NConcentrationAbovegroundBiomass * cm.vc_AbovegroundBiomass) -
				(cm.vc_NConcentrationAbovegroundBiomass *
						cm.vc_BelowgroundBiomass /
						pc_ResidueNRatio)) /
			cm.vc_RootBiomass

		if tempNConcentrationRoot >= pc_MinimumNConcentrationRoot {
			cm.vc_NConcentrationAbovegroundBiomass = tempNConcentrationAbovegroundBiomass
			cm.vc_NConcentrationRoot = tempNConcentrationRoot
		}
	}
}

// C++: double monica::cropmodule::getEffectiveRootingDepth(const CropModule*)
get_effective_rooting_depth :: proc(cm: ^Crop_Module) -> f64 {
	nols := len(cm.soilColumn.layers)

	for i_Layer := 0; i_Layer < nols; i_Layer += 1 {
		if cm.vc_RootEffectivity[i_Layer] == 0.0 {
			return f64(i_Layer + 1) / 10.0
		}
	}

	return f64(nols + 1) / 10.0
}

// ---------------------------------------------------------------------------
// Phase 5 checkpoint 7: step() orchestration
//
// Ported: step() itself (crop_module_step - "step" collides with the other
// core/*.odin files' own step functions, so it gets the same owning-struct
// prefix soil_temperature_step/soil_moisture_step/soil_organic_step already
// established), the FAO-56 dual-Kc block (never a separate cropmodule::
// function in C++ - inline in step() there too), forceTransplantState,
// setPerennialCropParameters. This is where every function ported in
// checkpoints 3-6 is finally wired together into one real, callable daily
// entry point, and where fireEvent stops being a no-op stub in the oracle.
//
// Deliberately not ported here: applyCutting. It takes a
// map<int, CuttingData::Value> - CuttingData is declared in
// src/worksteps/cutting.h, which this port hasn't reached yet (worksteps are
// phase 6, a separate later phase from this phase-5 checkpoint). Deferred to
// whichever phase-6 checkpoint ports the Cutting workstep itself.
// ---------------------------------------------------------------------------

// C++: void monica::cropmodule::setPerennialCropParameters(CropModule*, const CropParameters&)
//
// kj::heap<CropParameters>(cps) copy-constructs onto the heap - deep-copies
// via CropParameters' implicit copy constructor, same as makeCropModule's
// constructor and fcUpdateCropParametersForPerennial. Reuses
// clone_crop_parameters for the same reason those do.
set_perennial_crop_parameters :: proc(
	cm: ^Crop_Module,
	cps: p.Crop_Parameters,
	allocator := context.allocator,
) {
	ptr := new(p.Crop_Parameters, allocator)
	ptr^ = clone_crop_parameters(cps, allocator)
	cm.perennialCropParams = ptr
}

// C++: void monica::cropmodule::forceTransplantState(CropModule*, double
// temperatureSum, double lai, size_t stage, double rootMass, double
// leafMass, double shootMass, int postTransplantDelay)
//
// --- BEGIN TRANSPLANT MODIFICATION --- (matching the C++ source's own
// begin/end markers around this function)
force_transplant_state :: proc(
	cm: ^Crop_Module,
	temperatureSum, lai: f64,
	stage: int,
	rootMass, leafMass, shootMass: f64,
	postTransplantDelay: int,
) {
	soilColumn := cm.soilColumn
	pc_InitialRootingDepth := cm.cropParams.speciesParams.pc_InitialRootingDepth
	pc_NConcentrationAbovegroundBiomass :=
		cm.cropParams.speciesParams.pc_NConcentrationAbovegroundBiomass
	pc_NConcentrationRoot := cm.cropParams.speciesParams.pc_NConcentrationRoot
	pc_StageTemperatureSum := cm.cropParams.cultivarParams.pc_StageTemperatureSum

	// Initialize transplant shock duration parameters
	cm.vc_TransplantShockDuration = postTransplantDelay
	cm.vc_DaysSinceTransplant = 0

	// --- Step 1: Force developmental stage and LAI ---
	set_stage(cm, stage)
	cm.vc_LeafAreaIndex = lai

	// --- Step 2: Force cumulative and stage-specific GDD temperature sums ---
	cm.vc_CurrentTotalTemperatureSum = temperatureSum

	remainingGDD := temperatureSum
	for i := 0; i < len(cm.vc_CurrentTemperatureSum); i += 1 {
		if i < stage {
			threshold := pc_StageTemperatureSum[i]
			cm.vc_CurrentTemperatureSum[i] = threshold
			remainingGDD -= threshold
		} else if i == stage {
			cm.vc_CurrentTemperatureSum[i] = max(0.0, remainingGDD)
		} else {
			cm.vc_CurrentTemperatureSum[i] = 0.0
		}
	}

	// --- Step 3: Initialize organ biomass pools ---
	if len(cm.vc_OrganBiomass) > 2 {
		cm.vc_OrganBiomass[0] = rootMass
		cm.vc_OrganBiomass[1] = leafMass
		cm.vc_OrganBiomass[2] = shootMass
		cm.vc_OrganGreenBiomass[0] = rootMass
		cm.vc_OrganGreenBiomass[1] = leafMass
		cm.vc_OrganGreenBiomass[2] = shootMass
	}

	// --- Step 4: Update carbon balance and rooting depth state variables ---
	cm.vc_RootBiomass = rootMass
	cm.vc_AbovegroundBiomass = leafMass + shootMass
	cm.vc_TotalBiomass = rootMass + leafMass + shootMass

	// Settle rooting zone and layers based on standard species parameters
	nols := len(soilColumn.layers)
	layerThickness := soilColumn.layers[0].vs_LayerThickness
	cm.vc_RootingDepth_m = pc_InitialRootingDepth
	cm.vc_RootingDepth = min(int(libc.round(cm.vc_RootingDepth_m / layerThickness)), nols)
	cm.vc_RootingZone = min(int(libc.round(1.3 * cm.vc_RootingDepth_m / layerThickness)), nols)

	// Force total root length based on physical constants
	cm.vc_TotalRootLength =
		(cm.vc_RootBiomass * 100000.0 * 100.0 / 7.0) / (0.015 * 0.015 * 3.14159265358979323)

	// Force nitrogen pools to prevent severe immediate starvation stress in
	// new seedlings
	cm.vc_NConcentrationAbovegroundBiomass = pc_NConcentrationAbovegroundBiomass
	cm.vc_NConcentrationRoot = pc_NConcentrationRoot
	cm.vc_TotalBiomassNContent =
		(cm.vc_AbovegroundBiomass * pc_NConcentrationAbovegroundBiomass) +
		(cm.vc_RootBiomass * pc_NConcentrationRoot)
}

// --- END TRANSPLANT MODIFICATION ---

// C++: the icSendRcv lambda inside step(). The intercropping RPC exchange
// body is unreachable (Intercropping is a dropped feature, see
// plan-odin.md's "Explicitly dropped" table; cropModParams.isIntercropping
// is false in every fixture in this repo) and is not ported, matching
// checkpoint 4's fcCropPhotosynthesis precedent. Kept as a real, callable
// no-op so step()'s call sites and control flow stay literal.
@(private)
crop_module_ic_send_rcv :: proc(cm: ^Crop_Module, outmsg: string) {
	if cm.cropModParams.isIntercropping {
		// intercropping RPC exchange - dropped feature, not ported.
	}
}

// C++: void monica::cropmodule::step(CropModule*, double meanAirTemperature,
// double maxAirTemperature, double minAirTemperature, double globalRadiation,
// double sunshineHours, Tools::Date currentDate, double relativeHumidity,
// double windSpeed, double windSpeedHeight, double
// atmosphericCO2Concentration, double atmosphericO3Concentration, double
// grossPrecipitation, double referenceEvapotranspiration)
//
// The daily orchestration entry point: calls every cropmodule:: function
// ported in checkpoints 3-6 in step()'s real order, plus the FAO-56 dual-Kc
// block below.
crop_module_step :: proc(
	cm: ^Crop_Module,
	meanAirTemperature, maxAirTemperature, minAirTemperature: f64,
	globalRadiation, sunshineHours: f64,
	currentDate: d.Date,
	relativeHumidity, windSpeed, windSpeedHeight: f64,
	atmosphericCO2Concentration, atmosphericO3Concentration: f64,
	grossPrecipitation, referenceEvapotranspiration: f64,
	allocator := context.allocator,
) {
	pc_BaseDaylength := cm.cropParams.cultivarParams.pc_BaseDaylength
	pc_CriticalOxygenContent := cm.cropParams.speciesParams.pc_CriticalOxygenContent
	pc_DaylengthRequirement := cm.cropParams.cultivarParams.pc_DaylengthRequirement
	pc_MaxCropHeight := cm.cropParams.cultivarParams.pc_MaxCropHeight
	pc_Perennial := cm.cropParams.cultivarParams.pc_Perennial
	pc_SpecificLeafArea := cm.cropParams.cultivarParams.pc_SpecificLeafArea
	pc_StageKcFactor := cm.cropParams.cultivarParams.pc_StageKcFactor
	pc_StageTemperatureSum := cm.cropParams.cultivarParams.pc_StageTemperatureSum
	pc_VernalisationRequirement := cm.cropParams.cultivarParams.pc_VernalisationRequirement
	soilColumn := cm.soilColumn
	speciesPs := &cm.cropParams.speciesParams

	vs_JulianDay := int(d.julian_day(currentDate))

	if cm.vc_CuttingDelayDays > 0 {
		cm.vc_CuttingDelayDays -= 1
	}

	// [TRANSPLANT SHOCK] Daily stress recovery calculation. The daily
	// transplant efficiency factors in transplant shock, increasing linearly
	// from a baseline of 0.2 (80% initial stress) to 1.0 (no stress) over
	// the post-transplant delay duration.
	cm.vc_TransplantEfficiency = 1.0
	if cm.vc_DaysSinceTransplant >= 0 &&
	   cm.vc_DaysSinceTransplant < cm.vc_TransplantShockDuration {
		cm.vc_TransplantEfficiency =
			0.2 + 0.8 * (f64(cm.vc_DaysSinceTransplant) / f64(cm.vc_TransplantShockDuration))
		cm.vc_DaysSinceTransplant += 1
	} else if cm.vc_DaysSinceTransplant >= cm.vc_TransplantShockDuration {
		cm.vc_DaysSinceTransplant = -1 // Recovery period has successfully concluded
	}

	fc_radiation(cm, f64(vs_JulianDay), globalRadiation, sunshineHours)

	cm.vc_OxygenDeficit = fc_oxygen_deficiency(
		cm,
		pc_CriticalOxygenContent[cm.vc_DevelopmentalStage],
	)

	old_DevelopmentalStage := cm.vc_DevelopmentalStage

	// start accumulating temperature sums only after dormancy
	if !d.is_valid(cm.perennialCropDormancyPeriodEndDate) {
		if speciesPs.dormancyEndDoy == 0 {
			cm.perennialCropDormancyPeriodEndDate = currentDate
		} else {
			cm.perennialCropDormancyPeriodEndDate = d.add(
				d.make_date(
					1,
					1,
					u16(d.year(currentDate)),
					false,
					false,
					d.DEFAULT_USE_LEAP_YEARS,
				),
				u64(speciesPs.dormancyEndDoy - 1),
			)
		}
	}
	if !pc_Perennial || d.ge(currentDate, cm.perennialCropDormancyPeriodEndDate) {
		fc_crop_developmental_stage(
			cm,
			meanAirTemperature,
			soilColumn.layers[0].vs_SoilMoisture_m3,
			soilColumn.layers[0].vs_FieldCapacity,
			soilColumn.layers[0].vs_PermanentWiltingPoint,
			currentDate,
			allocator,
		)
	}

	if old_DevelopmentalStage == 0 && cm.vc_DevelopmentalStage == 1 {
		if cm.fireEvent != nil {
			cm.fireEvent("emergence")
		}
	} else if is_anthesis_day(cm, old_DevelopmentalStage, cm.vc_DevelopmentalStage) {
		cm.vc_AnthesisDay = vs_JulianDay
		if cm.fireEvent != nil {
			cm.fireEvent("anthesis")
		}
	} else if is_maturity_day(cm, old_DevelopmentalStage, cm.vc_DevelopmentalStage) {
		cm.vc_MaturityDay = vs_JulianDay
		cm.vc_MaturityReached = true
		if cm.fireEvent != nil {
			cm.fireEvent("maturity")
		}
	}

	// NOTE(c++-quirk): this fireEvent call is not guarded by a nil check,
	// unlike every other call site in this function - harmless in practice
	// since fireEvent is a required, always-set constructor parameter, but
	// reproduced exactly rather than "fixed" to match the others.
	if !cm.stemElongationEventFired &&
	   cm.vc_CurrentTotalTemperatureSum >=
		   pc_StageTemperatureSum[2] * 0.25 + pc_StageTemperatureSum[1] {
		cm.fireEvent("cereal-stem-elongation")
		cm.stemElongationEventFired = true
	}

	// fire stage event on stage change or right after sowing
	if old_DevelopmentalStage != cm.vc_DevelopmentalStage || cm.noOfCropSteps == 0 {
		if cm.fireEvent != nil {
			cm.fireEvent(fmt.tprintf("Stage-%d", cm.vc_DevelopmentalStage + 1))
		}
	}

	cm.vc_DaylengthFactor = fc_daylength_factor(
		cm,
		pc_DaylengthRequirement[cm.vc_DevelopmentalStage],
		cm.vc_EffectiveDayLength,
		cm.vc_PhotoperiodicDaylength,
		pc_BaseDaylength[cm.vc_DevelopmentalStage],
	)

	cm.vc_VernalisationFactor, cm.vc_VernalisationDays = fc_vernalisation_factor(
		cm,
		meanAirTemperature,
		pc_VernalisationRequirement[cm.vc_DevelopmentalStage],
		cm.vc_VernalisationDays,
	)

	if cm.vc_TotalTemperatureSum == 0.0 {
		cm.vc_RelativeTotalDevelopment = 0.0
	} else {
		cm.vc_RelativeTotalDevelopment =
			cm.vc_CurrentTotalTemperatureSum / cm.vc_TotalTemperatureSum
	}

	if cm.vc_DevelopmentalStage == 0 {
		cm.vc_KcFactor = cm.siteParams.bareSoilKcFactor // @todo Claas: muss hier etwas Genaueres hin, siehe FAO?
	} else {
		cm.vc_KcFactor = fc_kc_factor(
			cm,
			pc_StageTemperatureSum[cm.vc_DevelopmentalStage],
			cm.vc_CurrentTemperatureSum[cm.vc_DevelopmentalStage],
			pc_StageKcFactor[cm.vc_DevelopmentalStage],
			pc_StageKcFactor[cm.vc_DevelopmentalStage - 1],
		)
	}

	// FAO-56 Dual Kc: GDD-based 4-phase trapezoidal Kcb curve (replaces
	// static per-stage arrays). Phase 1 (flat initial) | Phase 2 (linear
	// ascent) | Phase 3 (mid-season) | Phase 4 (descent). Not a
	// cropmodule:: function in C++ - inline in step() itself there too.
	{
		nStages := len(pc_StageKcFactor)
		if nStages > 0 {
			// Compute total elapsed GDD as sum of all completed stages plus
			// current stage progress
			elapsed_GDD := 0.0
			for s := 0; s < nStages; s += 1 {
				elapsed_GDD += cm.vc_CurrentTemperatureSum[s]
			}

			// Identify mid-season start: first stage where Kc equals the maximum
			max_Kc := pc_StageKcFactor[0]
			for v in pc_StageKcFactor {
				if v > max_Kc {
					max_Kc = v
				}
			}
			mid_stage_start := nStages - 1
			for i := 1; i < nStages; i += 1 {
				if pc_StageKcFactor[i] >= max_Kc - 1e-6 {
					mid_stage_start = i
					break
				}
			}
			// Identify late-season start: first stage after plateau where Kc drops
			late_stage_start := nStages - 1
			for i := mid_stage_start + 1; i < nStages; i += 1 {
				if pc_StageKcFactor[i] < max_Kc - 1e-6 {
					late_stage_start = i
					break
				}
			}

			// GDD boundaries
			gdd_phase1_end := pc_StageTemperatureSum[0] // end of germination/initial phase
			gdd_to_mid := 0.0
			for i := 0; i < mid_stage_start; i += 1 {
				gdd_to_mid += pc_StageTemperatureSum[i]
			}
			gdd_late_start := 0.0
			for i := 0; i < late_stage_start; i += 1 {
				gdd_late_start += pc_StageTemperatureSum[i]
			}
			gdd_late_total := 0.0
			for i := late_stage_start; i < nStages; i += 1 {
				gdd_late_total += pc_StageTemperatureSum[i]
			}

			if elapsed_GDD <= gdd_phase1_end {
				// Phase 1: flat initial
				cm.vc_KcbFactor = cm.vc_Kcb_ini
			} else if cm.vc_DevelopmentalStage < mid_stage_start {
				// Phase 2: linear development ascent
				denom := gdd_to_mid - gdd_phase1_end
				frac := denom > 0.0 ? min(1.0, (elapsed_GDD - gdd_phase1_end) / denom) : 1.0
				cm.vc_KcbFactor = cm.vc_Kcb_ini + frac * (cm.vc_Kcb_mid - cm.vc_Kcb_ini)
			} else if cm.vc_DevelopmentalStage < late_stage_start {
				// Phase 3: mid-season plateau
				cm.vc_KcbFactor = cm.vc_Kcb_mid
			} else {
				// Phase 4: late-season linear descent
				gdd_since_late := elapsed_GDD - gdd_late_start
				frac := gdd_late_total > 0.0 ? min(1.0, gdd_since_late / gdd_late_total) : 1.0
				cm.vc_KcbFactor = cm.vc_Kcb_mid + frac * (cm.vc_Kcb_end - cm.vc_Kcb_mid)
			}
			cm.vc_KcbFactor = max(0.0, cm.vc_KcbFactor)
		}
	}

	if cm.vc_DevelopmentalStage > 0 {
		maxCropHeight :=
			cm.cropModParams.isIntercropping && cm.intercroppingOtherCropHeight > cm.vc_CropHeight ? pc_MaxCropHeight * cm.cropModParams.pc_intercropping_phRedux : pc_MaxCropHeight

		fc_crop_size(cm, maxCropHeight)

		crop_module_ic_send_rcv(cm, "devstage > 0: ")

		fc_crop_green_area(
			cm,
			meanAirTemperature,
			cm.vc_OrganGrowthIncrement[Organ_Leaf],
			cm.vc_OrganSenescenceIncrement[Organ_Leaf],
			pc_SpecificLeafArea[cm.vc_DevelopmentalStage - 1],
			pc_SpecificLeafArea[cm.vc_DevelopmentalStage],
			pc_SpecificLeafArea[1],
			pc_StageTemperatureSum[cm.vc_DevelopmentalStage],
			cm.vc_CurrentTemperatureSum[cm.vc_DevelopmentalStage],
		)

		cm.vc_SoilCoverage = fc_soil_coverage(cm)

		fc_crop_photosynthesis(
			cm,
			meanAirTemperature,
			maxAirTemperature,
			minAirTemperature,
			atmosphericCO2Concentration,
			atmosphericO3Concentration,
			currentDate,
			allocator,
		)

		fc_heat_stress_impact(cm, maxAirTemperature, minAirTemperature)

		if cm.simParams.pc_FrostKillOn {
			fc_frost_kill(cm, maxAirTemperature, minAirTemperature)
		}

		fc_drought_impact_on_fertility(cm)

		fc_crop_nitrogen(cm)

		fc_crop_dry_matter(cm, meanAirTemperature, allocator)

		// calculate reference evapotranspiration if not provided directly
		// via climate files
		if referenceEvapotranspiration < 0 {
			cm.vc_ReferenceEvapotranspiration = fc_reference_evapotranspiration(
				cm,
				maxAirTemperature,
				minAirTemperature,
				relativeHumidity,
				meanAirTemperature,
				windSpeed,
				windSpeedHeight,
				atmosphericCO2Concentration,
			)
		} else {
			// use reference evapotranspiration from climate file
			cm.vc_ReferenceEvapotranspiration = referenceEvapotranspiration
		}
		fc_crop_water_uptake(
			cm,
			soilColumn.vm_GroundwaterTableLayer,
			grossPrecipitation,
			cm.vc_CurrentTotalTemperatureSum,
			cm.vc_TotalTemperatureSum,
		)

		fc_crop_n_uptake(
			cm,
			soilColumn.vm_GroundwaterTableLayer,
			cm.vc_CurrentTotalTemperatureSum,
			cm.vc_TotalTemperatureSum,
		)

		cm.vc_GrossPrimaryProduction = fc_gross_primary_production(cm)

		cm.vc_NetPrimaryProduction = fc_net_primary_production(cm, cm.vc_TotalRespired)
	} else {
		crop_module_ic_send_rcv(cm, "devstage 0: ")
	}

	cm.noOfCropSteps += 1
}

// ---------------------------------------------------------------------------
// Phase 6 checkpoint 2 prerequisite: the yield/N-content getters and
// applyCutting, deferred from phase 5 (checkpoint 5's writeup: "whichever
// checkpoint actually needs them") - the Harvest workstep (harvestCurrentCrop,
// monica_model.odin) and the Cutting workstep both need these.
//
// getRawProteinConcentration is NOT ported: grepped and confirmed it has no
// callers anywhere in crop-module.cpp, harvestCurrentCrop, or any workstep in
// this port's scope - pure build-output.cpp API surface (phase 7), same
// "port on demand" call phase 5 checkpoint 5 already made for its siblings.
// ---------------------------------------------------------------------------

// C++: std::set<int> monica::cropmodule::organIdsForPrimaryYield(const CropModule*)
//
// std::set<int> -> map[int]bool, the same set idiom used elsewhere in this
// port (e.g. MonicaModel.currentEvents, phase 6 checkpoint 1).
organ_ids_for_primary_yield :: proc(
	cm: ^Crop_Module,
	allocator := context.allocator,
) -> map[int]bool {
	ids := make(map[int]bool, 0, allocator)
	for yc in cm.cropParams.cultivarParams.pc_OrganIdsForPrimaryYield {
		ids[yc.organId] = true
	}
	return ids
}

// C++: double calculateCropYield(const VYC&, const vector<double>&) - anonymous namespace
@(private)
calculate_crop_yield :: proc(ycs: [dynamic]p.Yield_Component, bmv: [dynamic]f64) -> f64 {
	yield := 0.0
	for yc in ycs {
		yield += bmv[yc.organId - 1] * yc.yieldPercentage
	}
	return yield
}

// C++: double monica::cropmodule::getPrimaryCropYield(const CropModule*)
get_primary_crop_yield :: proc(cm: ^Crop_Module) -> f64 {
	return calculate_crop_yield(
		cm.cropParams.cultivarParams.pc_OrganIdsForPrimaryYield,
		cm.vc_OrganBiomass,
	)
}

// C++: double monica::cropmodule::getSecondaryCropYield(const CropModule*)
get_secondary_crop_yield :: proc(cm: ^Crop_Module) -> f64 {
	return calculate_crop_yield(
		cm.cropParams.cultivarParams.pc_OrganIdsForSecondaryYield,
		cm.vc_OrganBiomass,
	)
}

// C++: double monica::cropmodule::getResidueBiomass(const CropModule*, bool, double)
get_residue_biomass :: proc(
	cm: ^Crop_Module,
	useSecondaryCropYields: bool = true,
	alternativeCropYield: f64 = -1,
) -> f64 {
	cropYield :=
		alternativeCropYield >= 0 ? alternativeCropYield : get_primary_crop_yield(cm) + (useSecondaryCropYields ? get_secondary_crop_yield(cm) : 0)

	return cm.vc_TotalBiomass - cm.vc_OrganBiomass[0] - cropYield
}

// C++: double monica::cropmodule::getResiduesNConcentration(const CropModule*, double)
get_residues_n_concentration :: proc(
	cm: ^Crop_Module,
	alternativePrimaryCropYield: f64 = -1,
) -> f64 {
	primaryCropYield :=
		alternativePrimaryCropYield >= 0 ? alternativePrimaryCropYield : get_primary_crop_yield(cm)
	rootBiomass := cm.vc_OrganBiomass[0]

	return(
		(cm.vc_TotalBiomassNContent - (rootBiomass * cm.vc_NConcentrationRoot)) /
		((primaryCropYield / cm.cropParams.cultivarParams.pc_ResidueNRatio) +
				(cm.vc_TotalBiomass - rootBiomass - primaryCropYield)) \
	)
}

// C++: double monica::cropmodule::getPrimaryYieldNConcentration(const CropModule*, double)
get_primary_yield_n_concentration :: proc(
	cm: ^Crop_Module,
	alternativePrimaryCropYield: f64 = -1,
) -> f64 {
	primaryCropYield :=
		alternativePrimaryCropYield >= 0 ? alternativePrimaryCropYield : get_primary_crop_yield(cm)
	rootBiomass := cm.vc_OrganBiomass[0]

	return(
		(cm.vc_TotalBiomassNContent - (rootBiomass * cm.vc_NConcentrationRoot)) /
		(primaryCropYield +
				(cm.cropParams.cultivarParams.pc_ResidueNRatio *
						(cm.vc_TotalBiomass - rootBiomass - primaryCropYield))) \
	)
}

// C++: double monica::cropmodule::getResiduesNContent(const CropModule*, bool, double, double)
get_residues_n_content :: proc(
	cm: ^Crop_Module,
	useSecondaryCropYields: bool = true,
	alternativePrimaryCropYield: f64 = -1,
	alternativeCropYield: f64 = -1,
) -> f64 {
	return(
		get_residue_biomass(cm, useSecondaryCropYields, alternativeCropYield) *
		get_residues_n_concentration(cm, alternativePrimaryCropYield) \
	)
}

// C++: double monica::cropmodule::getPrimaryYieldNContent(const CropModule*, double)
get_primary_yield_n_content :: proc(
	cm: ^Crop_Module,
	alternativePrimaryCropYield: f64 = -1,
) -> f64 {
	primaryCropYield :=
		alternativePrimaryCropYield >= 0 ? alternativePrimaryCropYield : get_primary_crop_yield(cm)
	return primaryCropYield * get_primary_yield_n_concentration(cm, alternativePrimaryCropYield)
}

// C++: double monica::cropmodule::getSecondaryYieldNContent(const CropModule*, double, double)
get_secondary_yield_n_content :: proc(
	cm: ^Crop_Module,
	alternativePrimaryCropYield: f64 = -1,
	alternativeSecondaryCropYield: f64 = -1,
) -> f64 {
	secondaryCropYield :=
		alternativeSecondaryCropYield >= 0 ? alternativeSecondaryCropYield : get_secondary_crop_yield(cm)
	return secondaryCropYield * get_residues_n_concentration(cm, alternativePrimaryCropYield)
}

// C++: double monica::cropmodule::getAbovegroundBiomassNContent(const CropModule*)
get_aboveground_biomass_n_content :: proc(cm: ^Crop_Module) -> f64 {
	return cm.vc_AbovegroundBiomass * cm.vc_NConcentrationAbovegroundBiomass
}

// ---------------------------------------------------------------------------
// applyCutting - deferred from phase 5 checkpoint 7 to whichever checkpoint
// ports the Cutting workstep (this one). Its payload types (CuttingData::
// Value's Unit/CL enums) are hoisted into `core` here, the same
// circular-dependency-breaking move phase 6 checkpoint 1 used for
// HarvestData::Spec - the Cutting workstep itself lives in the `run` package,
// which imports `core`, so `core` cannot import it back.
// ---------------------------------------------------------------------------

// C++: enum CuttingData::Unit { percentage, biomass, LAI }
Cutting_Unit :: enum {
	Percentage,
	Biomass,
	LAI,
}

// C++: enum CuttingData::CL { cut, left, none }
Cutting_Cl :: enum {
	Cut,
	Left,
	None,
}

// C++: struct CuttingData::Value
Cutting_Value :: struct {
	value:       f64,
	unit:        Cutting_Unit, // C++ in-class default: percentage
	cut_or_left: Cutting_Cl, // C++ in-class default: cut
}

// C++: void monica::cropmodule::applyCutting(CropModule*,
//        std::map<int, CuttingData::Value>&, std::map<int, double>&, double)
//
// `organs`/`exports` are genuine in-out maps in the C++ (organs gets
// populated from pc_OrganIdsForCutting when passed in empty) - Odin's
// map[K]V parameters already alias the caller's backing store like a C++
// reference, so no extra indirection is needed to reproduce that.
//
// Iterates `organs` in ascending key order to match C++ std::map's sorted
// iteration - Odin map iteration order is unspecified and floating-point
// addition (vc_AbovegroundBiomass -=, sumCutBiomass +=, sumResidueBiomass +=)
// is not associative, the same "sort keys first" fix phase 5 checkpoint 6's
// oracle regression needed.
apply_cutting :: proc(
	cm: ^Crop_Module,
	organs: map[int]Cutting_Value,
	exports: map[int]f64,
	cutMaxAssimilationFraction: f64,
	allocator := context.allocator,
) {
	oldAbovegroundBiomass := cm.vc_AbovegroundBiomass
	oldAgbNcontent := cm.vc_AbovegroundBiomass * cm.vc_NConcentrationAbovegroundBiomass
	sumCutBiomass := 0.0
	currentSLA := cm.vc_LeafAreaIndex / cm.vc_OrganGreenBiomass[Organ_Leaf]

	organs := organs
	if len(organs) == 0 {
		for yc in cm.cropParams.cultivarParams.pc_OrganIdsForCutting {
			organs[yc.organId - 1] = Cutting_Value {
				value = yc.yieldPercentage,
			}
		}
	}

	keys := make([dynamic]int, 0, len(organs), allocator)
	for k in organs {
		append(&keys, k)
	}
	slice.sort(keys[:])

	sumResidueBiomass := 0.0
	for organId in keys {
		organSpec := organs[organId]

		oldOrganBiomass := cm.vc_OrganBiomass[organId]
		oldOrganDeadBiomass := cm.vc_OrganDeadBiomass[organId]
		oldOrganGreenBiomass := oldOrganBiomass - oldOrganDeadBiomass
		newOrganBiomass := 0.0
		cutOrganBiomass := 0.0

		if organSpec.unit == .Biomass {
			if organSpec.cut_or_left == .Cut {
				cutOrganBiomass = min(organSpec.value, oldOrganBiomass)
				newOrganBiomass = oldOrganBiomass - cutOrganBiomass
			} else if organSpec.cut_or_left == .Left {
				newOrganBiomass = min(organSpec.value, oldOrganBiomass)
				cutOrganBiomass = oldOrganBiomass - newOrganBiomass
			}

			if oldOrganBiomass == 0 {
				cm.vc_OrganDeadBiomass[organId] = 0
			} else {
				cm.vc_OrganDeadBiomass[organId] =
					newOrganBiomass * min(oldOrganDeadBiomass / oldOrganBiomass, 1.0)
			}
		} else if organSpec.unit == .Percentage {
			if organSpec.cut_or_left == .Cut {
				cutOrganBiomass = organSpec.value * oldOrganBiomass
				newOrganBiomass = oldOrganBiomass - cutOrganBiomass
			} else if organSpec.cut_or_left == .Left {
				newOrganBiomass = organSpec.value * oldOrganBiomass
				cutOrganBiomass = oldOrganBiomass - newOrganBiomass
			}

			if oldOrganBiomass == 0 {
				cm.vc_OrganDeadBiomass[organId] = 0
			} else {
				cm.vc_OrganDeadBiomass[organId] =
					newOrganBiomass * min(oldOrganDeadBiomass / oldOrganBiomass, 1.0)
			}
		} else if organSpec.unit == .LAI {
			// only "left" is supported for LAI
			currentLAI := cm.vc_LeafAreaIndex
			if organSpec.value > currentLAI {
				newOrganBiomass = oldOrganGreenBiomass
				cutOrganBiomass = oldOrganDeadBiomass
				cm.vc_OrganDeadBiomass[organId] = 0 // all the dead biomass is assumed to be cut
			} else {
				newOrganBiomass = min(organSpec.value / currentSLA, oldOrganGreenBiomass)
				cutOrganBiomass = oldOrganBiomass - newOrganBiomass
				cm.vc_OrganDeadBiomass[organId] = 0 // all the dead biomass is assumed to be cut
			}
		}

		exportBiomass := cutOrganBiomass * exports[organId]

		cm.vc_AbovegroundBiomass -= cutOrganBiomass
		sumCutBiomass += cutOrganBiomass
		sumResidueBiomass += cutOrganBiomass - exportBiomass
		cm.vc_OrganBiomass[organId] = newOrganBiomass
		cm.vc_OrganGreenBiomass[organId] =
			cm.vc_OrganBiomass[organId] - cm.vc_OrganDeadBiomass[organId]
	}

	cm.vc_exportedCutBiomass = sumCutBiomass - sumResidueBiomass
	cm.vc_sumExportedCutBiomass += cm.vc_exportedCutBiomass
	cm.vc_residueCutBiomass = sumResidueBiomass
	cm.vc_sumResidueCutBiomass += cm.vc_residueCutBiomass

	if sumResidueBiomass > 0 {
		// prepare to add crop residues to soilorganic (AOMs)
		residueNConcentration := cm.vc_NConcentrationAbovegroundBiomass
		residueMap := make(map[int]f64, 1, allocator)
		residueMap[0] = sumResidueBiomass
		cm.addOrganicMatter(residueMap, residueNConcentration)
	}

	// update LAI
	if cm.vc_OrganGreenBiomass[Organ_Leaf] > 0 {
		cm.vc_LeafAreaIndex = cm.vc_OrganGreenBiomass[Organ_Leaf] * currentSLA
	}

	// reset stage and temperature sum after cutting
	set_stage(cm, cm.cropParams.speciesParams.pc_StageAfterCut)

	cm.vc_CuttingDelayDays = cm.cropParams.speciesParams.pc_CuttingDelayDays
	cm.cropParams.cultivarParams.pc_MaxAssimilationRate *= cutMaxAssimilationFraction

	if oldAbovegroundBiomass > 0.0 {
		cm.vc_TotalBiomassNContent -=
			(1 - cm.vc_AbovegroundBiomass / oldAbovegroundBiomass) * oldAgbNcontent
	}
}
