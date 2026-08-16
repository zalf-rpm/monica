// Phase 5 checkpoint 2: src/core/crop-module.h's CropModule struct and
// src/core/crop-module.cpp's makeCropModule constructor (the first overload -
// the second, `mas::schema::model::monica::CropModuleState::Reader`-based, is
// Cap'n Proto deserialize and dropped, per plan-odin.md's "Explicitly
// dropped" table).
//
// This checkpoint is scaffolding only: the struct and its constructor, not
// yet any of the ~40 cropmodule:: step functions (fcRadiation,
// fcCropPhotosynthesis, step, ...) - those are checkpoints 3-7. Replaces
// crop_module_stub.odin's 11-field placeholder wholesale; every field here
// keeps its exact C++ name, so the phase-4 modules that already hold a
// `cropModule: ^Crop_Module` pointer (soilmoisture, soiltransport,
// soilorganic) compile unchanged against this drop-in replacement.
package core

import p "../params"
import d "../../support/date"

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
