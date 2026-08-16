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

import libc "core:c/libc"
import p "../params"
import d "../../support/date"
import tl "../../support/tools"

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
	noOfOrgans:    int, // C++ size_t
	noOfDevStages: int, // C++ size_t

	// --- BEGIN TRANSPLANT MODIFICATION ---
	vc_TransplantShockDuration: int,
	vc_DaysSinceTransplant:     int,
	vc_TransplantEfficiency:    f64,
	// --- END TRANSPLANT MODIFICATION ---

	vc_TranspirationDeficit:         f64, //! old TRREL
	vc_PotentialTranspirationDeficit: f64,
	vc_ActualTranspirationDeficit:   f64,
	vc_TranspirationReduced:         f64,
	rootNRedux:                      f64, //! old REDWU
	vc_TimeUnderAnoxia:              int,

	// C++: Intercropping *intercropping - Intercropping itself is dropped
	// (Cap'n Proto RPC, see plan-odin.md's dropped table); every real use of
	// this field is behind `if (isIntercropping)`, always false in every
	// fixture in this repo, so it stays a typed-but-inert pointer.
	intercropping: rawptr,

	soilColumn:    ^Soil_Column,
	siteParams:    ^p.Site_Parameters,
	simParams:     ^p.Simulation_Parameters,
	cropModParams: ^p.Crop_Module_Parameters,
	cropParams:    p.Crop_Parameters,
	residueParams: p.Crop_Residue_Parameters,
	// C++: kj::Own<CropParameters> perennialCropParams - nil unless a
	// Sowing workstep's separatePerennialCropParams is set (phase 6).
	perennialCropParams: ^p.Crop_Parameters,

	//! old N
	vc_AbovegroundBiomass:    f64, //! old OBMAS
	vc_AbovegroundBiomassOld: f64, //! old OBALT
	vc_ActualTranspiration:   f64,
	vc_Assimilates:           f64,
	vc_AssimilationRate:      f64, //! old AMAX
	vc_AstronomicDayLenght:   f64, //! old DL
	vc_BelowgroundBiomass:    f64,
	vc_BelowgroundBiomassOld: f64,
	vc_ClearDayRadiation:     f64, //! old DRC
	pc_CO2Method:             int,
	vc_CriticalNConcentration: f64, //! old GEHMIN
	vc_CropDiameter:          f64,
	vc_CropFrostRedux:        f64,
	vc_CropHeatRedux:         f64,
	vc_CropHeight:            f64,
	vc_CropNDemand:           f64, //! old DTGESN
	vc_CropNRedux:            f64, //! old REDUK
	vc_CropWaterUptake:       [dynamic]f64, //! old TP
	vc_CurrentTemperatureSum: [dynamic]f64, //! old SUM
	vc_CurrentTotalTemperatureSum:     f64, //! old FP
	vc_CurrentTotalTemperatureSumRoot: f64,
	vc_DaylengthFactor:       f64, //! old DAYL
	vc_DaysAfterBeginFlowering: int,
	vc_Declination:           f64, //! old EFF0
	vc_DevelopmentalStage:    int, // C++ size_t, //! old INTWICK
	noOfCropSteps:            int,
	vc_DroughtImpactOnFertility: f64,
	vc_EffectiveDayLength:    f64, //! old DLE
	vc_ErrorStatus:           bool,
	vc_ErrorMessage:          string,
	vc_EvaporatedFromIntercept: f64,
	vc_ExtraterrestrialRadiation: f64,
	vc_FinalDevelopmentalStage: int, // C++ size_t
	vc_FixedN:                f64,
	vc_GlobalRadiation:       f64,
	vc_GreenAreaIndex:        f64,
	vc_GrossAssimilates:      f64,
	vc_GrossPhotosynthesis:   f64, //! old GPHOT
	vc_GrossPhotosynthesis_mol: f64,
	vc_GrossPhotosynthesisReference_mol: f64,
	vc_GrossPrimaryProduction: f64,
	vc_GrowthCycleEnded:      bool,
	vc_GrowthRespirationAS:   f64,
	vc_InterceptionStorage:   f64,
	vc_KcFactor:              f64, //! old FKc
	vc_LeafAreaIndex:         f64, //! old LAI
	vc_sunlitLeafAreaIndex:   [dynamic]f64,
	vc_shadedLeafAreaIndex:   [dynamic]f64,
	vc_LT50:                  f64,
	vc_LT50M:                 f64,
	vc_MaintenanceRespirationAS: f64,
	vc_MaxNUptake:            f64, //! old MAXUP
	vc_MaxRootingDepth:       f64, //! old WURM
	vc_NetMaintenanceRespiration: f64, //! old MAINT
	vc_NetPhotosynthesis:     f64, //! old GTW
	vc_NetPrecipitation:      f64,
	vc_NetPrimaryProduction:  f64,
	vc_NConcentrationAbovegroundBiomass:    f64, //! old GEHOB
	vc_NConcentrationAbovegroundBiomassOld: f64, //! old GEHALT
	vc_NContentDeficit:       f64,
	vc_NConcentrationRoot:    f64, //! old WUGEH
	vc_NConcentrationRootOld: f64, //! old
	vc_NUptakeFromLayer:      [dynamic]f64, //! old PE
	vc_OrganBiomass:          [dynamic]f64, //! old WORG
	vc_OrganDeadBiomass:      [dynamic]f64, //! old WDORG
	vc_OrganGreenBiomass:     [dynamic]f64,
	vc_OrganGrowthIncrement:  [dynamic]f64, //! old GORG
	vc_OrganSenescenceIncrement: [dynamic]f64, //! old DGORG
	vc_OvercastDayRadiation:  f64, //! old DRO
	vc_OxygenDeficit:         f64, //! old LURED
	vc_PhotoperiodicDaylength: f64, //! old DLP
	vc_PhotActRadiationMean:  f64, //! old RDN
	vc_PotentialTranspiration: f64,
	vc_ReferenceEvapotranspiration: f64,
	vc_RelativeTotalDevelopment: f64,
	vc_RemainingEvapotranspiration: f64,
	vc_ReserveAssimilatePool: f64, //! old ASPOO
	vc_RootBiomass:           f64, //! old WUMAS
	vc_RootBiomassOld:        f64, //! old WUMALT
	vc_RootDensity:           [dynamic]f64, //! old WUDICH
	vc_RootDiameter:          [dynamic]f64, //! old WRAD
	vc_RootEffectivity:       [dynamic]f64, //! old WUEFF
	vc_RootingDepth:          int, // C++ size_t, //! old WURZ
	vc_RootingDepth_m:        f64,
	vc_RootingZone:           int, // C++ size_t
	vc_SoilCoverage:          f64,
	vs_SoilMineralNContent:   [dynamic]f64, //! old C1
	vc_SoilSpecificMaxRootingDepth: f64, //! old WURZMAX [m]
	vs_SoilSpecificMaxRootingDepth: f64,
	// FAO-56 Dual Kc: GDD-based trapezoidal Kcb curve state.
	vc_KcbFactor:             f64, // Current daily Kcb (output of GDD-based 4-phase interpolation)
	vc_Kcb_ini:               f64, // Initial/germination phase Kcb (flat, Phase 1)
	vc_Kcb_mid:               f64, // Mid-season plateau Kcb (Phase 3)
	vc_Kcb_end:               f64, // End of late-season Kcb target (Phase 4)
	vc_StomataResistance:     f64, //! old RSTOM
	vc_StorageOrgan:          int,
	vc_TargetNConcentration:  f64, //! old GEHMAX
	vc_TimeStep:              f64, //! old dt
	TimeUnderAnoxiaThresholdDefault: int,
	vc_TotalBiomass:          f64,
	vc_TotalBiomassNContent:  f64, //! old PESUM
	vc_TotalCropHeatImpact:   f64,
	vc_TotalNInput:           f64,
	vc_TotalNUptake:          f64, //! old SUMPE
	vc_TotalRespired:         f64,
	vc_Respiration:           f64,
	vc_SumTotalNUptake:       f64, //! summation of all calculated NUptake; needed for sensitivity analysis
	vc_TotalRootLength:       f64, //! old WULAEN
	vc_TotalTemperatureSum:   f64,
	vc_TemperatureSumToFlowering: f64,
	vc_Transpiration:         [dynamic]f64, //! old TP
	vc_TranspirationRedux:    [dynamic]f64, //! old TRRED
	vc_VernalisationDays:     f64,
	vc_VernalisationFactor:   f64, //! old FV
	dyingOut:                 bool,
	vc_AccumulatedETa:        f64,
	vc_AccumulatedTranspiration: f64,
	vc_sumExportedCutBiomass: f64,
	vc_exportedCutBiomass:    f64,
	vc_sumResidueCutBiomass:  f64,
	vc_residueCutBiomass:     f64,
	vc_CuttingDelayDays:      int,
	vc_AnthesisDay:           int,
	vc_MaturityDay:           int,
	vc_MaturityReached:       bool,

	// VOC members
	stepSize24:  int,
	stepSize240: int,
	rad24:       [dynamic]f64,
	rad240:      [dynamic]f64,
	tfol24:      [dynamic]f64,
	tfol240:     [dynamic]f64,
	index24:     int,
	index240:    int,
	full24:      bool,
	full240:     bool,

	guentherEmissions:         Voc_Emissions,
	jjvEmissions:              Voc_Emissions,
	vocSpecies:                Voc_Species_Data,
	cropPhotosynthesisResults: Voc_Cp_Data,

	// C++: std::function<void(std::string)> fireEvent;
	fireEvent: proc(_: string),
	// C++: std::function<void(std::map<size_t, double>, double)> addOrganicMatter;
	addOrganicMatter: proc(_: map[int]f64, _: f64),
	// C++: std::function<std::pair<double, double>(double)> getSnowDepthAndCalcTempUnderSnow;
	getSnowDepthAndCalcTempUnderSnow: proc(_: f64) -> (f64, f64),

	vc_O3_shortTermDamage:   f64,
	vc_O3_longTermDamage:    f64,
	vc_O3_senescence:        f64,
	vc_O3_sumUptake:         f64,
	vc_O3_WStomatalClosure:  f64,

	assimilatePartCoeffsReduced: bool,
	vc_KTkc: f64, // old KTkc
	vc_KTko: f64, // old KTkc

	stemElongationEventFired: bool,

	// intercropping
	intercroppingOtherCropHeight: f64,
	intercroppingOtherLAIt:       f64,

	fractionOfInterceptedRadiation1: f64,
	fractionOfInterceptedRadiation2: f64,

	perennialCropDormancyPeriodEndDate: d.Date,
}

// C++ in-class initialisers
make_crop_module_defaults :: proc() -> Crop_Module {
	return Crop_Module {
		vc_DaysSinceTransplant   = -1,
		vc_TransplantEfficiency  = 1.0,
		vc_TranspirationDeficit  = 1.0,
		pc_CO2Method             = 3,
		vc_CropFrostRedux        = 1.0,
		vc_CropHeatRedux         = 1.0,
		vc_CropNRedux            = 1.0,
		vc_DroughtImpactOnFertility = 1.0,
		vc_KcFactor              = 0.6,
		vc_LT50                  = -3.0,
		vc_LT50M                 = -3.0,
		vc_KcbFactor             = 0.15,
		vc_Kcb_ini               = 0.15,
		vc_StorageOrgan          = 4,
		vc_TimeStep              = 1.0,
		TimeUnderAnoxiaThresholdDefault = 4,
		vc_CuttingDelayDays      = 0,
		vc_AnthesisDay           = -1,
		vc_MaturityDay           = -1,
		stepSize24               = 24,
		stepSize240              = 240,
		vc_O3_shortTermDamage    = 1.0,
		vc_O3_longTermDamage     = 1.0,
		vc_O3_senescence         = 1.0,
		vc_O3_WStomatalClosure   = 1.0,
		intercroppingOtherCropHeight = -1,
		intercroppingOtherLAIt   = -1,
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
clone_f64_2d_array :: proc(a: [dynamic][dynamic]f64, allocator := context.allocator) -> [dynamic][dynamic]f64 {
	out := make([dynamic][dynamic]f64, len(a), allocator)
	for row, i in a {
		out[i] = clone_f64_array(row, allocator)
	}
	return out
}

@(private)
clone_yield_component_array :: proc(a: [dynamic]p.Yield_Component, allocator := context.allocator) -> [dynamic]p.Yield_Component {
	out := make([dynamic]p.Yield_Component, len(a), allocator)
	copy(out[:], a[:])
	return out
}

@(private)
clone_species_parameters :: proc(sp: p.Species_Parameters, allocator := context.allocator) -> p.Species_Parameters {
	out := sp
	out.pc_BaseTemperature = clone_f64_array(sp.pc_BaseTemperature, allocator)
	out.pc_OrganMaintenanceRespiration = clone_f64_array(sp.pc_OrganMaintenanceRespiration, allocator)
	out.pc_OrganGrowthRespiration = clone_f64_array(sp.pc_OrganGrowthRespiration, allocator)
	out.pc_StageMaxRootNConcentration = clone_f64_array(sp.pc_StageMaxRootNConcentration, allocator)
	out.pc_InitialOrganBiomass = clone_f64_array(sp.pc_InitialOrganBiomass, allocator)
	out.pc_CriticalOxygenContent = clone_f64_array(sp.pc_CriticalOxygenContent, allocator)
	out.pc_StageMobilFromStorageCoeff = clone_f64_array(sp.pc_StageMobilFromStorageCoeff, allocator)
	out.pc_AbovegroundOrgan = clone_bool_array(sp.pc_AbovegroundOrgan, allocator)
	out.pc_StorageOrgan = clone_bool_array(sp.pc_StorageOrgan, allocator)
	return out
}

@(private)
clone_cultivar_parameters :: proc(cp: p.Cultivar_Parameters, allocator := context.allocator) -> p.Cultivar_Parameters {
	out := cp
	out.pc_AssimilatePartitioningCoeff = clone_f64_2d_array(cp.pc_AssimilatePartitioningCoeff, allocator)
	out.pc_OrganSenescenceRate = clone_f64_2d_array(cp.pc_OrganSenescenceRate, allocator)
	out.pc_BaseDaylength = clone_f64_array(cp.pc_BaseDaylength, allocator)
	out.pc_OptimumTemperature = clone_f64_array(cp.pc_OptimumTemperature, allocator)
	out.pc_DaylengthRequirement = clone_f64_array(cp.pc_DaylengthRequirement, allocator)
	out.pc_DroughtStressThreshold = clone_f64_array(cp.pc_DroughtStressThreshold, allocator)
	out.pc_SpecificLeafArea = clone_f64_array(cp.pc_SpecificLeafArea, allocator)
	out.pc_StageKcFactor = clone_f64_array(cp.pc_StageKcFactor, allocator)
	out.pc_StageTemperatureSum = clone_f64_array(cp.pc_StageTemperatureSum, allocator)
	out.pc_VernalisationRequirement = clone_f64_array(cp.pc_VernalisationRequirement, allocator)
	out.pc_OrganIdsForPrimaryYield = clone_yield_component_array(cp.pc_OrganIdsForPrimaryYield, allocator)
	out.pc_OrganIdsForSecondaryYield = clone_yield_component_array(cp.pc_OrganIdsForSecondaryYield, allocator)
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
clone_crop_parameters :: proc(cp: p.Crop_Parameters, allocator := context.allocator) -> p.Crop_Parameters {
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
	cm.noOfDevStages = p.species_parameters_number_of_developmental_stages(&cm.cropParams.speciesParams)
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
			cm.vc_AbovegroundBiomass += cm.cropParams.speciesParams.pc_InitialOrganBiomass[i_Organ] // [kg ha-1]
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

	NConcentrationAbovegroundBiomass := cm.cropParams.speciesParams.pc_NConcentrationAbovegroundBiomass
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
			R_P_max * ((R_P_max + (R_P_max * R_S)) / R_P_max) * ((R_P_max + (R_P_max * R_D)) / R_P_max)
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

	vc_DeclinationSinus := libc.sin(cm.vc_Declination*PI/180.0) * libc.sin(vs_Latitude*PI/180.0) // old SINLD
	vc_DeclinationCosinus := libc.cos(cm.vc_Declination*PI/180.0) * libc.cos(vs_Latitude*PI/180.0) // old COSLD

	// Calculation of the atmospheric day lenght - old DL
	arg_AstroDayLength := vc_DeclinationSinus / vc_DeclinationCosinus
	arg_AstroDayLength = tl.bound(-1.0, arg_AstroDayLength, 1.0)
	cm.vc_AstronomicDayLenght = 12.0 * (PI + 2.0*libc.asin(arg_AstroDayLength)) / PI

	// Calculation of the effective day length - old DLE
	EDLHelper := (-libc.sin(f64(8.0*PI/180.0)) + vc_DeclinationSinus) / vc_DeclinationCosinus

	if EDLHelper < -1.0 || EDLHelper > 1.0 {
		cm.vc_EffectiveDayLength = 0.01
	} else {
		cm.vc_EffectiveDayLength = 12.0 * (PI + 2.0*libc.asin(EDLHelper)) / PI
	}

	// old DLP
	arg_PhotoDayLength := (-libc.sin(f64(-6.0*PI/180.0)) + vc_DeclinationSinus) / vc_DeclinationCosinus
	arg_PhotoDayLength = tl.bound(-1.0, arg_PhotoDayLength, 1.0)
	cm.vc_PhotoperiodicDaylength = 12.0 * (PI + 2.0*libc.asin(arg_PhotoDayLength)) / PI

	// Calculation of the mean photosynthetically active radiation [J m-2] - old RDN
	arg_PhotAct := min(
		1.0,
		(vc_DeclinationSinus / vc_DeclinationCosinus) * (vc_DeclinationSinus / vc_DeclinationCosinus),
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
	SC := 24.0 * 60.0 / PI * pc_SolarConstant * (1.0 + 0.033*libc.cos(2.0*PI*julianDay/365.0))

	arg_SolarAngle := -libc.tan(vs_Latitude*PI/180.0) * libc.tan(cm.vc_Declination*PI/180.0)
	arg_SolarAngle = tl.bound(-1.0, arg_SolarAngle, 1.0)
	vc_SunsetSolarAngle := libc.acos(arg_SolarAngle)
	cm.vc_ExtraterrestrialRadiation =
		SC *
		(vc_SunsetSolarAngle * vc_DeclinationSinus + vc_DeclinationCosinus * libc.sin(vc_SunsetSolarAngle)) // [MJ m-2]

	if globalRadiation > 0.0 {
		cm.vc_GlobalRadiation = globalRadiation
	} else if cm.vc_AstronomicDayLenght > 0 {
		cm.vc_GlobalRadiation =
			cm.vc_ExtraterrestrialRadiation * (0.19 + 0.55*sunshineHours/cm.vc_AstronomicDayLenght)
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
				(effectiveDayLength - vc_MaximumDayLength) / (vc_CriticalDayLenght - vc_MaximumDayLength)
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
		avgAirFilledPoreVolume = max(0.0, avgAirFilledPoreVolume) // to guarantee for positive values
		cm.vc_TimeUnderAnoxia = max(
			cm.vc_TimeUnderAnoxia + int(cm.vc_TimeStep),
			timeUnderAnoxiaThresholdAtStage,
		)
		maxOxygenDeficit := avgAirFilledPoreVolume / criticalOxygenContent
		cm.vc_OxygenDeficit =
			1.0 -
			f64(cm.vc_TimeUnderAnoxia) / f64(timeUnderAnoxiaThresholdAtStage) * (1.0 - maxOxygenDeficit)
		cm.vc_OxygenDeficit = max(0.0, cm.vc_OxygenDeficit)
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
	numerator := 2*libc.pow(t-tmin, alfa)*libc.pow(topt-tmin, alfa) - libc.pow(t-tmin, 2*alfa)
	denominator := libc.pow(topt-tmin, 2*alfa)

	// MP: beta coefficient should be 2*alfa
	return libc.pow(numerator/denominator, betacoeff)
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
		if pc_Perennial { // pc_Perennial == true
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
		} else { // pc_Perennial == false
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
					emergenceCondition = emergenceCondition && soilColumn.vs_SurfaceWaterStorage < 0.001
				}

				if emergenceCondition {
					cm.vc_CurrentTemperatureSum[cm.vc_DevelopmentalStage] +=
						(vc_SoilTemperature - pc_BaseTemperature[cm.vc_DevelopmentalStage]) * cm.vc_TimeStep

					if cm.vc_CurrentTemperatureSum[cm.vc_DevelopmentalStage] >=
					   pc_StageTemperatureSum[cm.vc_DevelopmentalStage] {
						vc_StageExcessTemperatureSum :=
							cm.vc_CurrentTemperatureSum[cm.vc_DevelopmentalStage] -
							pc_StageTemperatureSum[cm.vc_DevelopmentalStage]
						if cm.vc_DevelopmentalStage < cm.noOfDevStages - 1 {
							cm.vc_DevelopmentalStage += 1
							cm.vc_CurrentTemperatureSum[cm.vc_DevelopmentalStage] += vc_StageExcessTemperatureSum
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
			} else if cm.vc_DevelopmentalStage == cm.noOfDevStages - 1 { // MP: Frucht ist reif
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
					d.make_date(1, 1, u16(d.year(currentDate) + yearDelta), false, false, d.DEFAULT_USE_LEAP_YEARS),
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
		vc_RelativeDevelopment = min(d_CurrentTemperatureSum/d_StageTemperatureSum, 1.0) // old relint
	}

	if cm.vc_DevelopmentalStage == 0 {
		return pc_InitialKcFactor + (d_StageKcFactor-pc_InitialKcFactor)*vc_RelativeDevelopment
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
			(1.0 + libc.exp(-pc_CropHeightP1 * (vc_RelativeTotalDevelopmentForHeight - pc_CropHeightP2)))
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
				223.9 * libc.exp(-5.03*libc.exp(-0.0653*cultivarPs.pc_EarlyRefLeafExp))
			TempResponseExpansion = min(
				223.9 * libc.exp(-5.03*libc.exp(-0.0653*vw_MeanAirTemperature)) / referenceTempResponseExpansion,
				1.3,
			)
		} else {
			// leaf expansion T response
			referenceTempResponseExpansion :=
				37.7 * libc.exp(-7.23*libc.exp(-0.1462*cultivarPs.pc_RefLeafExp))
			TempResponseExpansion = min(
				37.7 * libc.exp(-7.23*libc.exp(-0.1462*vw_MeanAirTemperature)) / referenceTempResponseExpansion,
				1.3,
			)
		}
	}

	cm.vc_LeafAreaIndex +=
		(d_LeafBiomassIncrement *
				TempResponseExpansion *
				(d_SpecificLeafAreaStart +
						(d_CurrentTemperatureSum / d_StageTemperatureSum * (d_SpecificLeafAreaEnd - d_SpecificLeafAreaStart))) *
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
	cm.noOfDevStages = p.species_parameters_number_of_developmental_stages(&cm.cropParams.speciesParams)
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
