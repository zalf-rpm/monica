// Tranche 3b of the src/core/monica-parameters.{h,cpp} port: SpeciesParameters,
// CultivarParameters, CropParameters.
package params

import jx "../../support/jsonx"
import tl "../../support/tools"
import "core:strings"

// ---------------------------------------------------------------------------
// SpeciesParameters
// ---------------------------------------------------------------------------

// C++: struct monica::SpeciesParameters
Species_Parameters :: struct {
	species_id:                                  string,
	carboxylation_pathway:                       int, // old TEMPTYP
	default_radiation_use_efficiency:            f64,
	part_biological_n_fixation:                  f64,
	initial_kc_factor:                           f64, // old Kcini
	luxury_n_coeff:                              f64,
	max_crop_diameter:                           f64,
	stage_at_max_height:                         f64,
	stage_at_max_diameter:                       f64,
	minimum_n_concentration:                     f64,
	minimum_temperature_for_assimilation:        f64, // old MINTMP
	optimum_temperature_for_assimilation:        f64,
	maximum_temperature_for_assimilation:        f64,
	n_concentration_aboveground_biomass:         f64, // initial value of old GEHOB
	n_concentration_b0:                          f64,
	n_concentration_pn:                          f64,
	n_concentration_root:                        f64, // initial value to WUGEH
	development_acceleration_by_nitrogen_stress: int,
	field_condition_modifier:                    f64,
	assimilate_reallocation:                     f64,
	base_temperature:                            [dynamic]f64, // old BAS
	organ_maintenance_respiration:               [dynamic]f64, // old MAIRT
	organ_growth_respiration:                    [dynamic]f64, // old MAIRT
	stage_max_root_n_concentration:              [dynamic]f64, // old WGMAX
	initial_organ_biomass:                       [dynamic]f64,
	critical_oxygen_content:                     [dynamic]f64, // old LUKRIT
	stage_mobil_from_storage_coeff:              [dynamic]f64,
	aboveground_organ:                           [dynamic]bool, // old KOMP
	storage_organ:                               [dynamic]bool,
	sampling_depth:                              f64,
	target_n_sampling_depth:                     f64,
	target_n30:                                  f64,
	max_n_uptake_param:                          f64,
	root_distribution_param:                     f64,
	plant_density:                               int, // [plants m-2]
	root_growth_lag:                             f64,
	minimum_temperature_root_growth:             f64,
	initial_rooting_depth:                       f64,
	root_penetration_rate:                       f64,
	root_form_factor:                            f64,
	specific_root_length:                        f64,
	stage_after_cut:                             int, // stage number is zero-based
	limiting_temperature_heat_stress:            f64,
	cutting_delay_days:                          int,
	drought_impact_on_fertility_factor:          f64,
	ef_mono:                                     f64, // [ug gDW-1 h-1] Monoterpenes emitted right after synthesis
	ef_monos:                                    f64, // [ug gDW-1 h-1] Monoterpenes stored then emitted
	ef_iso:                                      f64, // Isoprene emission factor
	vcmax25:                                     f64, // max RubP saturated rate of carboxylation at 25oC (umol m-2 s-1)
	aekc:                                        f64, // activation energy for Michaelis-Menten constant for CO2 (J mol-1)
	aeko:                                        f64, // activation energy for Michaelis-Menten constant for O2 (J mol-1)
	aevc:                                        f64, // activation energy for photosynthesis (J mol-1)
	kc25:                                        f64, // Michaelis-Menten constant for CO2 at 25oC (umol mol-1 ubar-1)
	ko25:                                        f64, // Michaelis-Menten constant for O2 at 25oC (mmol mol-1 mbar-1)
	transition_stage_leaf_exp:                   int, // [1-7]
	dormancy_start_doy:                          int, // start dormancy of perennial crops at that DOY (0 = unset)
	dormancy_end_doy:                            int, // end dormancy, start accumulating temperature sums (0 = unset)
}

// C++ in-class initialisers
make_species_parameters :: proc() -> Species_Parameters {
	return Species_Parameters {
		field_condition_modifier = 1.0,
		ef_mono = 0.5,
		ef_monos = 0.5,
		aekc = 65800.0,
		aeko = 1400.0,
		aevc = 68800.0,
		kc25 = 460.0,
		ko25 = 330.0,
		transition_stage_leaf_exp = -1,
	}
}

// C++: Errors speciesparameters::merge(SpeciesParameters*, Json)
species_parameters_merge :: proc(sp: ^Species_Parameters, j: jx.Value) -> tl.Errors {
	res := default_merge(sp, j, species_parameters_merge)

	jx.set_string_value(&sp.species_id, j, "SpeciesName")
	jx.set_int_value(&sp.carboxylation_pathway, j, "CarboxylationPathway")
	jx.set_double_value(&sp.default_radiation_use_efficiency, j, "DefaultRadiationUseEfficiency")
	jx.set_double_value(&sp.part_biological_n_fixation, j, "PartBiologicalNFixation")
	jx.set_double_value(&sp.initial_kc_factor, j, "InitialKcFactor")
	jx.set_double_value(&sp.luxury_n_coeff, j, "LuxuryNCoeff")
	jx.set_double_value(&sp.max_crop_diameter, j, "MaxCropDiameter")
	jx.set_double_value(&sp.stage_at_max_height, j, "StageAtMaxHeight")
	jx.set_double_value(&sp.stage_at_max_diameter, j, "StageAtMaxDiameter")
	jx.set_double_value(&sp.minimum_n_concentration, j, "MinimumNConcentration")
	jx.set_double_value(
		&sp.minimum_temperature_for_assimilation,
		j,
		"MinimumTemperatureForAssimilation",
	)
	jx.set_double_value(
		&sp.optimum_temperature_for_assimilation,
		j,
		"OptimumTemperatureForAssimilation",
	)
	jx.set_double_value(
		&sp.maximum_temperature_for_assimilation,
		j,
		"MaximumTemperatureForAssimilation",
	)
	jx.set_double_value(
		&sp.n_concentration_aboveground_biomass,
		j,
		"NConcentrationAbovegroundBiomass",
	)
	jx.set_double_value(&sp.n_concentration_b0, j, "NConcentrationB0")
	jx.set_double_value(&sp.n_concentration_pn, j, "NConcentrationPN")
	jx.set_double_value(&sp.n_concentration_root, j, "NConcentrationRoot")
	jx.set_int_value(
		&sp.development_acceleration_by_nitrogen_stress,
		j,
		"DevelopmentAccelerationByNitrogenStress",
	)
	jx.set_double_value(&sp.field_condition_modifier, j, "FieldConditionModifier")
	jx.set_double_value(&sp.assimilate_reallocation, j, "AssimilateReallocation")
	jx.set_double_vector(&sp.base_temperature, j, "BaseTemperature")
	jx.set_double_vector(&sp.organ_maintenance_respiration, j, "OrganMaintenanceRespiration")
	jx.set_double_vector(&sp.organ_growth_respiration, j, "OrganGrowthRespiration")
	jx.set_double_vector(&sp.stage_max_root_n_concentration, j, "StageMaxRootNConcentration")
	jx.set_double_vector(&sp.initial_organ_biomass, j, "InitialOrganBiomass")
	jx.set_double_vector(&sp.critical_oxygen_content, j, "CriticalOxygenContent")

	jx.set_double_vector(&sp.stage_mobil_from_storage_coeff, j, "StageMobilFromStorageCoeff")
	if len(sp.stage_mobil_from_storage_coeff) == 0 {
		resize(&sp.stage_mobil_from_storage_coeff, len(sp.critical_oxygen_content))
	}

	jx.set_bool_vector(&sp.aboveground_organ, j, "AbovegroundOrgan")
	jx.set_bool_vector(&sp.storage_organ, j, "StorageOrgan")
	jx.set_double_value(&sp.sampling_depth, j, "SamplingDepth")
	jx.set_double_value(&sp.target_n_sampling_depth, j, "TargetNSamplingDepth")
	jx.set_double_value(&sp.target_n30, j, "TargetN30")
	jx.set_double_value(&sp.max_n_uptake_param, j, "MaxNUptakeParam")
	jx.set_double_value(&sp.root_distribution_param, j, "RootDistributionParam")
	jx.set_int_value(&sp.plant_density, j, "PlantDensity")
	jx.set_double_value(&sp.root_growth_lag, j, "RootGrowthLag")
	jx.set_double_value(&sp.minimum_temperature_root_growth, j, "MinimumTemperatureRootGrowth")
	jx.set_double_value(&sp.initial_rooting_depth, j, "InitialRootingDepth")
	jx.set_double_value(&sp.root_penetration_rate, j, "RootPenetrationRate")
	jx.set_double_value(&sp.root_form_factor, j, "RootFormFactor")
	jx.set_double_value(&sp.specific_root_length, j, "SpecificRootLength")
	jx.set_int_value(&sp.stage_after_cut, j, "StageAfterCut")
	if sp.stage_after_cut > 0 {
		sp.stage_after_cut -= 1
	}
	jx.set_double_value(&sp.limiting_temperature_heat_stress, j, "LimitingTemperatureHeatStress")
	jx.set_int_value(&sp.cutting_delay_days, j, "CuttingDelayDays")
	jx.set_double_value(
		&sp.drought_impact_on_fertility_factor,
		j,
		"DroughtImpactOnFertilityFactor",
	)

	jx.set_double_value(&sp.ef_mono, j, "EF_MONO")
	jx.set_double_value(&sp.ef_monos, j, "EF_MONOS")
	jx.set_double_value(&sp.ef_iso, j, "EF_ISO")
	jx.set_double_value(&sp.vcmax25, j, "VCMAX25")
	jx.set_double_value(&sp.aekc, j, "AEKC")
	jx.set_double_value(&sp.aevc, j, "AEVC")
	jx.set_double_value(&sp.aeko, j, "AEKO")
	jx.set_double_value(&sp.kc25, j, "KC25")
	jx.set_double_value(&sp.ko25, j, "KO25")

	jx.set_int_value(&sp.transition_stage_leaf_exp, j, "TransitionStageLeafExp")
	jx.set_int_value(&sp.dormancy_start_doy, j, "DormancyStartDoy")
	jx.set_int_value(&sp.dormancy_end_doy, j, "DormancyEndDoy")

	return res
}

// C++: json11::Json speciesparameters::to_json(const SpeciesParameters*)
species_parameters_to_json :: proc(sp: ^Species_Parameters, a: Allocator) -> jx.Value {
	return jx.obj(
		a,
		{"type", jx.sl("SpeciesParameters")},
		{"SpeciesName", jx.s(sp.species_id, a)},
		{"CarboxylationPathway", jx.i(sp.carboxylation_pathway)},
		{"DefaultRadiationUseEfficiency", jx.f(sp.default_radiation_use_efficiency)},
		{"PartBiologicalNFixation", jx.f(sp.part_biological_n_fixation)},
		{"InitialKcFactor", jx.f(sp.initial_kc_factor)},
		{"LuxuryNCoeff", jx.f(sp.luxury_n_coeff)},
		{"MaxCropDiameter", jx.f(sp.max_crop_diameter)},
		{"StageAtMaxHeight", jx.f(sp.stage_at_max_height)},
		{"StageAtMaxDiameter", jx.f(sp.stage_at_max_diameter)},
		{"MinimumNConcentration", jx.f(sp.minimum_n_concentration)},
		{"MinimumTemperatureForAssimilation", jx.f(sp.minimum_temperature_for_assimilation)},
		{"OptimumTemperatureForAssimilation", jx.f(sp.optimum_temperature_for_assimilation)},
		{"MaximumTemperatureForAssimilation", jx.f(sp.maximum_temperature_for_assimilation)},
		{"NConcentrationAbovegroundBiomass", jx.f(sp.n_concentration_aboveground_biomass)},
		{"NConcentrationB0", jx.f(sp.n_concentration_b0)},
		{"NConcentrationPN", jx.f(sp.n_concentration_pn)},
		{"NConcentrationRoot", jx.f(sp.n_concentration_root)},
		{
			"DevelopmentAccelerationByNitrogenStress",
			jx.i(sp.development_acceleration_by_nitrogen_stress),
		},
		{"FieldConditionModifier", jx.f(sp.field_condition_modifier)},
		{"AssimilateReallocation", jx.f(sp.assimilate_reallocation)},
		{"BaseTemperature", prim_arr_f64(sp.base_temperature[:], a)},
		{"OrganMaintenanceRespiration", prim_arr_f64(sp.organ_maintenance_respiration[:], a)},
		{"OrganGrowthRespiration", prim_arr_f64(sp.organ_growth_respiration[:], a)},
		{"StageMaxRootNConcentration", prim_arr_f64(sp.stage_max_root_n_concentration[:], a)},
		{"InitialOrganBiomass", prim_arr_f64(sp.initial_organ_biomass[:], a)},
		{"CriticalOxygenContent", prim_arr_f64(sp.critical_oxygen_content[:], a)},
		{"StageMobilFromStorageCoeff", prim_arr_f64(sp.stage_mobil_from_storage_coeff[:], a)},
		{"AbovegroundOrgan", prim_arr_bool(sp.aboveground_organ[:], a)},
		{"StorageOrgan", prim_arr_bool(sp.storage_organ[:], a)},
		{"SamplingDepth", jx.f(sp.sampling_depth)},
		{"TargetNSamplingDepth", jx.f(sp.target_n_sampling_depth)},
		{"TargetN30", jx.f(sp.target_n30)},
		{"MaxNUptakeParam", jx.f(sp.max_n_uptake_param)},
		{"RootDistributionParam", jx.f(sp.root_distribution_param)},
		{"PlantDensity", jx.vu_int(sp.plant_density, "plants m-2", a)},
		{"RootGrowthLag", jx.f(sp.root_growth_lag)},
		{"MinimumTemperatureRootGrowth", jx.f(sp.minimum_temperature_root_growth)},
		{"InitialRootingDepth", jx.f(sp.initial_rooting_depth)},
		{"RootPenetrationRate", jx.f(sp.root_penetration_rate)},
		{"RootFormFactor", jx.f(sp.root_form_factor)},
		{"SpecificRootLength", jx.f(sp.specific_root_length)},
		{"StageAfterCut", jx.i(sp.stage_after_cut)},
		{"LimitingTemperatureHeatStress", jx.f(sp.limiting_temperature_heat_stress)},
		{"CuttingDelayDays", jx.i(sp.cutting_delay_days)},
		{"DroughtImpactOnFertilityFactor", jx.f(sp.drought_impact_on_fertility_factor)},
		{"EF_MONO", jx.vu(sp.ef_mono, "ug gDW-1 h-1", a)},
		{"EF_MONOS", jx.vu(sp.ef_monos, "ug gDW-1 h-1", a)},
		{"EF_ISO", jx.vu(sp.ef_iso, "ug gDW-1 h-1", a)},
		{"VCMAX25", jx.vu(sp.vcmax25, "umol m-2 s-1", a)},
		{"AEKC", jx.vu(sp.aekc, "J mol-1", a)},
		{"AEKO", jx.vu(sp.aeko, "J mol-1", a)},
		{"AEVC", jx.vu(sp.aevc, "J mol-1", a)},
		{"KC25", jx.vu(sp.kc25, "umol mol-1 ubar-1", a)},
		{"KO25", jx.vu(sp.ko25, "mmol mol-1 mbar-1", a)},
		{"TransitionStageLeafExp", jx.vu_int(sp.transition_stage_leaf_exp, "1-7", a)},
		{"DormancyStartDoy", jx.i(sp.dormancy_start_doy)},
		{"DormancyEndDoy", jx.i(sp.dormancy_end_doy)},
	)
}

// C++: size_t speciesparameters::numberOfDevelopmentalStages(const SpeciesParameters*)
species_parameters_number_of_developmental_stages :: proc(sp: ^Species_Parameters) -> int {
	return len(sp.base_temperature)
}

// C++: size_t speciesparameters::numberOfOrgans(const SpeciesParameters*) - old NRKOM
species_parameters_number_of_organs :: proc(sp: ^Species_Parameters) -> int {
	return len(sp.organ_growth_respiration)
}

// ---------------------------------------------------------------------------
// CultivarParameters
// ---------------------------------------------------------------------------

// C++: struct monica::CultivarParameters
Cultivar_Parameters :: struct {
	cultivar_id:                       string,
	description:                       string,
	perennial:                         bool,
	max_assimilation_rate:             f64, // old MAXAMAX
	light_extinction_coefficient:      f64,
	max_crop_height:                   f64,
	residue_n_ratio:                   f64,
	lt50_cultivar:                     f64,
	crop_height_p1:                    f64,
	crop_height_p2:                    f64,
	crop_specific_max_rooting_depth:   f64, // old WUMAXPF [m]
	assimilate_partitioning_coeff:     [dynamic][dynamic]f64, // old PRO
	organ_senescence_rate:             [dynamic][dynamic]f64, // old DEAD
	base_daylength:                    [dynamic]f64, // old DLBAS
	optimum_temperature:               [dynamic]f64,
	daylength_requirement:             [dynamic]f64, // old DEC
	drought_stress_threshold:          [dynamic]f64, // old DRYswell
	specific_leaf_area:                [dynamic]f64, // old LAIFKT [ha kg-1]
	stage_kc_factor:                   [dynamic]f64, // old Kc
	stage_temperature_sum:             [dynamic]f64, // old TSUM
	vernalisation_requirement:         [dynamic]f64, // old VSCHWELL
	heat_sum_irrigation_start:         f64,
	heat_sum_irrigation_end:           f64,
	critical_temperature_heat_stress:  f64,
	begin_sensitive_phase_heat_stress: f64,
	end_sensitive_phase_heat_stress:   f64,
	frost_hardening:                   f64,
	frost_dehardening:                 f64,
	low_temperature_exposure:          f64,
	respiratory_stress:                f64,
	latest_harvest_doy:                int,
	organ_ids_for_primary_yield:       [dynamic]Yield_Component,
	organ_ids_for_secondary_yield:     [dynamic]Yield_Component,
	organ_ids_for_cutting:             [dynamic]Yield_Component,
	early_ref_leaf_exp:                f64, // 12 = wheat (first guess)
	ref_leaf_exp:                      f64, // 20 = wheat, 22 = maize (first guess)
	min_temp_dev_we:                   f64,
	opt_temp_dev_we:                   f64,
	max_temp_dev_we:                   f64,
	winter_crop:                       bool,
}

// C++ in-class initialisers
make_cultivar_parameters :: proc() -> Cultivar_Parameters {
	return Cultivar_Parameters {
		light_extinction_coefficient = 0.8,
		latest_harvest_doy = -1,
		early_ref_leaf_exp = 12.0,
		ref_leaf_exp = 20.0,
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
		cp.organ_ids_for_primary_yield = merge_yield_components(
			jx.get(j, "OrganIdsForPrimaryYield"),
		)
	} else {
		tl.append_errorf(
			&res,
			"Couldn't read 'OrganIdsForPrimaryYield' key from JSON object:\n%s",
			jx.dump(j),
		)
	}

	if jx.is_array(jx.get(j, "OrganIdsForSecondaryYield")) {
		cp.organ_ids_for_secondary_yield = merge_yield_components(
			jx.get(j, "OrganIdsForSecondaryYield"),
		)
	} else {
		tl.append_errorf(
			&res,
			"Couldn't read 'OrganIdsForSecondaryYield' key from JSON object:\n%s",
			jx.dump(j),
		)
	}

	if jx.is_array(jx.get(j, "OrganIdsForCutting")) {
		cp.organ_ids_for_cutting = merge_yield_components(jx.get(j, "OrganIdsForCutting"))
	} else {
		tl.append_warningf(
			&res,
			"Couldn't read 'OrganIdsForCutting' key from JSON object:\n%s",
			jx.dump(j),
		)
	}

	jx.set_string_value(&cp.cultivar_id, j, "CultivarName")
	jx.set_string_value(&cp.description, j, "Description")
	jx.set_bool_value(&cp.perennial, j, "Perennial")
	jx.set_double_value(&cp.max_assimilation_rate, j, "MaxAssimilationRate")
	jx.set_double_value(&cp.light_extinction_coefficient, j, "LightExtinctionCoefficient")
	jx.set_double_value(&cp.max_crop_height, j, "MaxCropHeight")
	jx.set_double_value(&cp.residue_n_ratio, j, "ResidueNRatio")
	jx.set_double_value(&cp.lt50_cultivar, j, "LT50cultivar")
	jx.set_double_value(&cp.crop_height_p1, j, "CropHeightP1")
	jx.set_double_value(&cp.crop_height_p2, j, "CropHeightP2")
	jx.set_double_value(&cp.crop_specific_max_rooting_depth, j, "CropSpecificMaxRootingDepth")
	jx.set_double_vector(&cp.base_daylength, j, "BaseDaylength")
	jx.set_double_vector(&cp.optimum_temperature, j, "OptimumTemperature")
	jx.set_double_vector(&cp.daylength_requirement, j, "DaylengthRequirement")
	jx.set_double_vector(&cp.drought_stress_threshold, j, "DroughtStressThreshold")
	jx.set_double_vector(&cp.specific_leaf_area, j, "SpecificLeafArea")
	jx.set_double_vector(&cp.stage_kc_factor, j, "StageKcFactor")
	jx.set_double_vector(&cp.stage_temperature_sum, j, "StageTemperatureSum")
	jx.set_double_vector(&cp.vernalisation_requirement, j, "VernalisationRequirement")
	jx.set_double_value(&cp.heat_sum_irrigation_start, j, "HeatSumIrrigationStart")
	jx.set_double_value(&cp.heat_sum_irrigation_end, j, "HeatSumIrrigationEnd")
	jx.set_double_value(&cp.critical_temperature_heat_stress, j, "CriticalTemperatureHeatStress")
	jx.set_double_value(&cp.begin_sensitive_phase_heat_stress, j, "BeginSensitivePhaseHeatStress")
	jx.set_double_value(&cp.end_sensitive_phase_heat_stress, j, "EndSensitivePhaseHeatStress")
	jx.set_double_value(&cp.frost_hardening, j, "FrostHardening")
	jx.set_double_value(&cp.frost_dehardening, j, "FrostDehardening")
	jx.set_double_value(&cp.low_temperature_exposure, j, "LowTemperatureExposure")
	jx.set_double_value(&cp.respiratory_stress, j, "RespiratoryStress")
	jx.set_int_value(&cp.latest_harvest_doy, j, "LatestHarvestDoy")
	jx.set_bool_value(&cp.winter_crop, j, "WinterCrop")

	if jx.is_array(jx.get(j, "AssimilatePartitioningCoeff")) {
		apcs := jx.array_items(jx.get(j, "AssimilatePartitioningCoeff"))
		resize(&cp.assimilate_partitioning_coeff, len(apcs))
		for js, idx in apcs {
			cp.assimilate_partitioning_coeff[idx] = jx.double_vector(js)
		}
	}
	if jx.is_array(jx.get(j, "OrganSenescenceRate")) {
		osrs := jx.array_items(jx.get(j, "OrganSenescenceRate"))
		resize(&cp.organ_senescence_rate, len(osrs))
		for js, idx in osrs {
			cp.organ_senescence_rate[idx] = jx.double_vector(js)
		}
	}

	jx.set_double_value(&cp.early_ref_leaf_exp, j, "EarlyRefLeafExp")
	jx.set_double_value(&cp.ref_leaf_exp, j, "RefLeafExp")

	jx.set_double_value(&cp.min_temp_dev_we, j, "MinTempDev_WE")
	jx.set_double_value(&cp.opt_temp_dev_we, j, "OptTempDev_WE")
	jx.set_double_value(&cp.max_temp_dev_we, j, "MaxTempDev_WE")

	return res
}

// C++: json11::Json cultivarparameters::to_json(const CultivarParameters*)
cultivar_parameters_to_json :: proc(cp: ^Cultivar_Parameters, a: Allocator) -> jx.Value {
	apcs := make(jx.Array, 0, len(cp.assimilate_partitioning_coeff), a)
	for row in cp.assimilate_partitioning_coeff {
		append(&apcs, prim_arr_f64(row[:], a))
	}

	osrs := make(jx.Array, 0, len(cp.organ_senescence_rate), a)
	for row in cp.organ_senescence_rate {
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
		{"CultivarName", jx.s(cp.cultivar_id, a)},
		{"Description", jx.s(cp.description, a)},
		{"Perennial", jx.b(cp.perennial)},
		{"MaxAssimilationRate", jx.f(cp.max_assimilation_rate)},
		{"LightExtinctionCoefficient", jx.f(cp.light_extinction_coefficient)},
		{"MaxCropHeight", jx.vu(cp.max_crop_height, "m", a)},
		{"ResidueNRatio", jx.f(cp.residue_n_ratio)},
		{"LT50cultivar", jx.f(cp.lt50_cultivar)},
		{"CropHeightP1", jx.f(cp.crop_height_p1)},
		{"CropHeightP2", jx.f(cp.crop_height_p2)},
		{"CropSpecificMaxRootingDepth", jx.f(cp.crop_specific_max_rooting_depth)},
		{"AssimilatePartitioningCoeff", jx.Value(apcs)},
		{"OrganSenescenceRate", jx.Value(osrs)},
		{"BaseDaylength", jx.arr(a, prim_arr_f64(cp.base_daylength[:], a), jx.sl("h"))},
		{
			"OptimumTemperature",
			jx.arr(a, prim_arr_f64(cp.optimum_temperature[:], a), jx.sl("°C")),
		},
		{
			"DaylengthRequirement",
			jx.arr(a, prim_arr_f64(cp.daylength_requirement[:], a), jx.sl("h")),
		},
		{"DroughtStressThreshold", prim_arr_f64(cp.drought_stress_threshold[:], a)},
		{
			"SpecificLeafArea",
			jx.arr(a, prim_arr_f64(cp.specific_leaf_area[:], a), jx.sl("ha kg-1")),
		},
		{"StageKcFactor", jx.arr(a, prim_arr_f64(cp.stage_kc_factor[:], a), jx.sl("1;0"))},
		{
			"StageTemperatureSum",
			jx.arr(a, prim_arr_f64(cp.stage_temperature_sum[:], a), jx.sl("°C d")),
		},
		{"VernalisationRequirement", prim_arr_f64(cp.vernalisation_requirement[:], a)},
		{"HeatSumIrrigationStart", jx.f(cp.heat_sum_irrigation_start)},
		{"HeatSumIrrigationEnd", jx.f(cp.heat_sum_irrigation_end)},
		{"CriticalTemperatureHeatStress", jx.vu(cp.critical_temperature_heat_stress, "°C", a)},
		{"BeginSensitivePhaseHeatStress", jx.vu(cp.begin_sensitive_phase_heat_stress, "°C d", a)},
		{"EndSensitivePhaseHeatStress", jx.vu(cp.end_sensitive_phase_heat_stress, "°C d", a)},
		{"FrostHardening", jx.f(cp.frost_hardening)},
		{"FrostDehardening", jx.f(cp.frost_dehardening)},
		{"LowTemperatureExposure", jx.f(cp.low_temperature_exposure)},
		{"RespiratoryStress", jx.f(cp.respiratory_stress)},
		{"LatestHarvestDoy", jx.i(cp.latest_harvest_doy)},
		{
			"OrganIdsForPrimaryYield",
			yield_components_to_json(cp.organ_ids_for_primary_yield[:], a),
		},
		{
			"OrganIdsForSecondaryYield",
			yield_components_to_json(cp.organ_ids_for_secondary_yield[:], a),
		},
		{"OrganIdsForCutting", yield_components_to_json(cp.organ_ids_for_cutting[:], a)},
		{"EarlyRefLeafExp", jx.f(cp.early_ref_leaf_exp)},
		{"RefLeafExp", jx.f(cp.ref_leaf_exp)},
		{"MinTempDev_WE", jx.f(cp.min_temp_dev_we)},
		{"OptTempDev_WE", jx.f(cp.opt_temp_dev_we)},
		{"MaxTempDev_WE", jx.f(cp.max_temp_dev_we)},
		{"WinterCrop", jx.b(cp.winter_crop)},
	)
}

// C++: inline size_t cultivarparameters::numberOfDevelopmentalStages(const CultivarParameters*)
cultivar_parameters_number_of_developmental_stages :: proc(cp: ^Cultivar_Parameters) -> int {
	return len(cp.base_daylength)
}

// ---------------------------------------------------------------------------
// CropParameters
// ---------------------------------------------------------------------------

// C++: struct monica::CropParameters
Crop_Parameters :: struct {
	speciesParams:                       Species_Parameters,
	cultivarParams:                      Cultivar_Parameters,
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
crop_parameters_merge_sj_cj :: proc(
	cp: ^Crop_Parameters,
	sj: jx.Value,
	cj: jx.Value,
) -> tl.Errors {
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
		{cp.speciesParams.species_id, "/", cp.cultivarParams.cultivar_id},
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
