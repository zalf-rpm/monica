// Odin side of the phase 5 checkpoint 2 (CropModule scaffolding)
// differential test.
//
// Must emit byte-identical output to
// odin/tests/cpp_ref/crop_module_ref_main.cpp - see that file's header
// comment for the four-scenario rationale and the list of fields
// deliberately not dumped.
// Run odin/tests/cpp_ref/run_crop_module.sh to build both and diff them.
package crop_module_ref

import "core:fmt"
import "core:os"
import "core:strings"
import core "../../monica/core"
import p "../../monica/params"
import tr "../../monica/trace"
import mrun "../../monica/run"
import jx "../../support/jsonx"
import tl "../../support/tools"

noop_fire_event :: proc(event: string) {}
noop_add_organic_matter :: proc(layer2amount: map[int]f64, nConcentration: f64) {}
noop_get_snow_depth :: proc(avgAirTemp: f64) -> (f64, f64) {return 0.0, 0.0}

dump_crop_module :: proc(t: ^tr.Tracer, cm: ^core.Crop_Module) {
	tr.dump(t, "cropModule.noOfOrgans", cm.no_of_organs)
	tr.dump(t, "cropModule.noOfDevStages", cm.no_of_dev_stages)

	tr.dump(t, "cropModule.vc_TransplantShockDuration", cm.transplant_shock_duration)
	tr.dump(t, "cropModule.vc_DaysSinceTransplant", cm.days_since_transplant)
	tr.dump(t, "cropModule.vc_TransplantEfficiency", cm.transplant_efficiency)

	tr.dump(t, "cropModule.vc_TranspirationDeficit", cm.transpiration_deficit)
	tr.dump(t, "cropModule.vc_PotentialTranspirationDeficit", cm.potential_transpiration_deficit)
	tr.dump(t, "cropModule.vc_ActualTranspirationDeficit", cm.actual_transpiration_deficit)
	tr.dump(t, "cropModule.vc_TranspirationReduced", cm.transpiration_reduced)
	tr.dump(t, "cropModule.rootNRedux", cm.root_n_redux)
	tr.dump(t, "cropModule.vc_TimeUnderAnoxia", cm.time_under_anoxia)

	tr.dump(t, "cropModule.intercropping", cm.intercropping)
	tr.dump(t, "cropModule.soilColumn", cm.soil_column)
	tr.dump(t, "cropModule.siteParams", cm.site_params)
	tr.dump(t, "cropModule.simParams", cm.sim_params)
	tr.dump(t, "cropModule.cropModParams", cm.mod_params)

	tr.dump(t, "cropModule.vc_AbovegroundBiomass", cm.aboveground_biomass)
	tr.dump(t, "cropModule.vc_AbovegroundBiomassOld", cm.aboveground_biomass_old)
	tr.dump(t, "cropModule.vc_ActualTranspiration", cm.actual_transpiration)
	tr.dump(t, "cropModule.vc_Assimilates", cm.assimilates)
	tr.dump(t, "cropModule.vc_AssimilationRate", cm.assimilation_rate)
	tr.dump(t, "cropModule.vc_AstronomicDayLenght", cm.astronomic_day_lenght)
	tr.dump(t, "cropModule.vc_BelowgroundBiomass", cm.belowground_biomass)
	tr.dump(t, "cropModule.vc_BelowgroundBiomassOld", cm.belowground_biomass_old)
	tr.dump(t, "cropModule.vc_ClearDayRadiation", cm.clear_day_radiation)
	tr.dump(t, "cropModule.pc_CO2Method", cm.pc_co2_method)
	tr.dump(t, "cropModule.vc_CriticalNConcentration", cm.critical_n_concentration)
	tr.dump(t, "cropModule.vc_CropDiameter", cm.crop_diameter)
	tr.dump(t, "cropModule.vc_CropFrostRedux", cm.crop_frost_redux)
	tr.dump(t, "cropModule.vc_CropHeatRedux", cm.crop_heat_redux)
	tr.dump(t, "cropModule.vc_CropHeight", cm.crop_height)
	tr.dump(t, "cropModule.vc_CropNDemand", cm.crop_n_demand)
	tr.dump(t, "cropModule.vc_CropNRedux", cm.crop_n_redux)
	tr.dump(t, "cropModule.vc_CropWaterUptake", cm.crop_water_uptake)
	tr.dump(t, "cropModule.vc_CurrentTemperatureSum", cm.current_temperature_sum)
	tr.dump(t, "cropModule.vc_CurrentTotalTemperatureSum", cm.current_total_temperature_sum)
	tr.dump(t, "cropModule.vc_CurrentTotalTemperatureSumRoot", cm.current_total_temperature_sum_root)
	tr.dump(t, "cropModule.vc_DaylengthFactor", cm.daylength_factor)
	tr.dump(t, "cropModule.vc_DaysAfterBeginFlowering", cm.days_after_begin_flowering)
	tr.dump(t, "cropModule.vc_Declination", cm.declination)
	tr.dump(t, "cropModule.vc_DevelopmentalStage", cm.developmental_stage)
	tr.dump(t, "cropModule.noOfCropSteps", cm.no_of_crop_steps)
	tr.dump(t, "cropModule.vc_DroughtImpactOnFertility", cm.drought_impact_on_fertility)
	tr.dump(t, "cropModule.vc_EffectiveDayLength", cm.effective_day_length)
	tr.dump(t, "cropModule.vc_ErrorStatus", cm.error_status)
	tr.dump(t, "cropModule.vc_ErrorMessage", cm.error_message)
	tr.dump(t, "cropModule.vc_EvaporatedFromIntercept", cm.evaporated_from_intercept)
	tr.dump(t, "cropModule.vc_ExtraterrestrialRadiation", cm.extraterrestrial_radiation)
	tr.dump(t, "cropModule.vc_FinalDevelopmentalStage", cm.final_developmental_stage)
	tr.dump(t, "cropModule.vc_FixedN", cm.fixed_n)
	tr.dump(t, "cropModule.vc_GlobalRadiation", cm.global_radiation)
	tr.dump(t, "cropModule.vc_GreenAreaIndex", cm.green_area_index)
	tr.dump(t, "cropModule.vc_GrossAssimilates", cm.gross_assimilates)
	tr.dump(t, "cropModule.vc_GrossPhotosynthesis", cm.gross_photosynthesis)
	tr.dump(t, "cropModule.vc_GrossPhotosynthesis_mol", cm.gross_photosynthesis_mol)
	tr.dump(t, "cropModule.vc_GrossPhotosynthesisReference_mol", cm.gross_photosynthesis_reference_mol)
	tr.dump(t, "cropModule.vc_GrossPrimaryProduction", cm.gross_primary_production)
	tr.dump(t, "cropModule.vc_GrowthCycleEnded", cm.growth_cycle_ended)
	tr.dump(t, "cropModule.vc_GrowthRespirationAS", cm.growth_respiration_as)
	tr.dump(t, "cropModule.vc_InterceptionStorage", cm.interception_storage)
	tr.dump(t, "cropModule.vc_KcFactor", cm.kc_factor)
	tr.dump(t, "cropModule.vc_LeafAreaIndex", cm.leaf_area_index)
	tr.dump(t, "cropModule.vc_sunlitLeafAreaIndex", cm.sunlit_leaf_area_index)
	tr.dump(t, "cropModule.vc_shadedLeafAreaIndex", cm.shaded_leaf_area_index)
	tr.dump(t, "cropModule.vc_LT50", cm.lt50)
	tr.dump(t, "cropModule.vc_LT50M", cm.lt50_m)
	tr.dump(t, "cropModule.vc_MaintenanceRespirationAS", cm.maintenance_respiration_as)
	tr.dump(t, "cropModule.vc_MaxNUptake", cm.max_n_uptake)
	tr.dump(t, "cropModule.vc_MaxRootingDepth", cm.max_rooting_depth)
	tr.dump(t, "cropModule.vc_NetMaintenanceRespiration", cm.net_maintenance_respiration)
	tr.dump(t, "cropModule.vc_NetPhotosynthesis", cm.net_photosynthesis)
	tr.dump(t, "cropModule.vc_NetPrecipitation", cm.net_precipitation)
	tr.dump(t, "cropModule.vc_NetPrimaryProduction", cm.net_primary_production)
	tr.dump(t, "cropModule.vc_NConcentrationAbovegroundBiomass", cm.n_concentration_aboveground_biomass)
	tr.dump(
		t,
		"cropModule.vc_NConcentrationAbovegroundBiomassOld",
		cm.n_concentration_aboveground_biomass_old,
	)
	tr.dump(t, "cropModule.vc_NContentDeficit", cm.n_content_deficit)
	tr.dump(t, "cropModule.vc_NConcentrationRoot", cm.n_concentration_root)
	tr.dump(t, "cropModule.vc_NConcentrationRootOld", cm.n_concentration_root_old)
	tr.dump(t, "cropModule.vc_NUptakeFromLayer", cm.n_uptake_from_layer)
	tr.dump(t, "cropModule.vc_OrganBiomass", cm.organ_biomass)
	tr.dump(t, "cropModule.vc_OrganDeadBiomass", cm.organ_dead_biomass)
	tr.dump(t, "cropModule.vc_OrganGreenBiomass", cm.organ_green_biomass)
	tr.dump(t, "cropModule.vc_OrganGrowthIncrement", cm.organ_growth_increment)
	tr.dump(t, "cropModule.vc_OrganSenescenceIncrement", cm.organ_senescence_increment)
	tr.dump(t, "cropModule.vc_OvercastDayRadiation", cm.overcast_day_radiation)
	tr.dump(t, "cropModule.vc_OxygenDeficit", cm.oxygen_deficit)
	tr.dump(t, "cropModule.vc_PhotoperiodicDaylength", cm.photoperiodic_daylength)
	tr.dump(t, "cropModule.vc_PhotActRadiationMean", cm.phot_act_radiation_mean)
	tr.dump(t, "cropModule.vc_PotentialTranspiration", cm.potential_transpiration)
	tr.dump(t, "cropModule.vc_ReferenceEvapotranspiration", cm.reference_evapotranspiration)
	tr.dump(t, "cropModule.vc_RelativeTotalDevelopment", cm.relative_total_development)
	tr.dump(t, "cropModule.vc_RemainingEvapotranspiration", cm.remaining_evapotranspiration)
	tr.dump(t, "cropModule.vc_ReserveAssimilatePool", cm.reserve_assimilate_pool)
	tr.dump(t, "cropModule.vc_RootBiomass", cm.root_biomass)
	tr.dump(t, "cropModule.vc_RootBiomassOld", cm.root_biomass_old)
	tr.dump(t, "cropModule.vc_RootDensity", cm.root_density)
	tr.dump(t, "cropModule.vc_RootDiameter", cm.root_diameter)
	tr.dump(t, "cropModule.vc_RootEffectivity", cm.root_effectivity)
	tr.dump(t, "cropModule.vc_RootingDepth", cm.rooting_depth)
	tr.dump(t, "cropModule.vc_RootingDepth_m", cm.rooting_depth_m)
	tr.dump(t, "cropModule.vc_RootingZone", cm.rooting_zone)
	tr.dump(t, "cropModule.vc_SoilCoverage", cm.soil_coverage)
	tr.dump(t, "cropModule.vs_SoilMineralNContent", cm.vs_soil_mineral_n_content)
	tr.dump(t, "cropModule.vc_SoilSpecificMaxRootingDepth", cm.soil_specific_max_rooting_depth)
	tr.dump(t, "cropModule.vs_SoilSpecificMaxRootingDepth", cm.vs_soil_specific_max_rooting_depth)
	tr.dump(t, "cropModule.vc_KcbFactor", cm.kcb_factor)
	tr.dump(t, "cropModule.vc_Kcb_ini", cm.kcb_ini)
	tr.dump(t, "cropModule.vc_Kcb_mid", cm.kcb_mid)
	tr.dump(t, "cropModule.vc_Kcb_end", cm.kcb_end)
	tr.dump(t, "cropModule.vc_StomataResistance", cm.stomata_resistance)
	tr.dump(t, "cropModule.vc_StorageOrgan", cm.storage_organ)
	tr.dump(t, "cropModule.vc_TargetNConcentration", cm.target_n_concentration)
	tr.dump(t, "cropModule.vc_TimeStep", cm.time_step)
	tr.dump(t, "cropModule.TimeUnderAnoxiaThresholdDefault", cm.time_under_anoxia_threshold_default)
	tr.dump(t, "cropModule.vc_TotalBiomass", cm.total_biomass)
	tr.dump(t, "cropModule.vc_TotalBiomassNContent", cm.total_biomass_n_content)
	tr.dump(t, "cropModule.vc_TotalCropHeatImpact", cm.total_crop_heat_impact)
	tr.dump(t, "cropModule.vc_TotalNInput", cm.total_n_input)
	tr.dump(t, "cropModule.vc_TotalNUptake", cm.total_n_uptake)
	tr.dump(t, "cropModule.vc_TotalRespired", cm.total_respired)
	tr.dump(t, "cropModule.vc_Respiration", cm.respiration)
	tr.dump(t, "cropModule.vc_SumTotalNUptake", cm.sum_total_n_uptake)
	tr.dump(t, "cropModule.vc_TotalRootLength", cm.total_root_length)
	tr.dump(t, "cropModule.vc_TotalTemperatureSum", cm.total_temperature_sum)
	tr.dump(t, "cropModule.vc_TemperatureSumToFlowering", cm.temperature_sum_to_flowering)
	tr.dump(t, "cropModule.vc_Transpiration", cm.transpiration)
	tr.dump(t, "cropModule.vc_TranspirationRedux", cm.transpiration_redux)
	tr.dump(t, "cropModule.vc_VernalisationDays", cm.vernalisation_days)
	tr.dump(t, "cropModule.vc_VernalisationFactor", cm.vernalisation_factor)
	tr.dump(t, "cropModule.dyingOut", cm.dying_out)
	tr.dump(t, "cropModule.vc_AccumulatedETa", cm.accumulated_et_a)
	tr.dump(t, "cropModule.vc_AccumulatedTranspiration", cm.accumulated_transpiration)
	tr.dump(t, "cropModule.vc_sumExportedCutBiomass", cm.sum_exported_cut_biomass)
	tr.dump(t, "cropModule.vc_exportedCutBiomass", cm.exported_cut_biomass)
	tr.dump(t, "cropModule.vc_sumResidueCutBiomass", cm.sum_residue_cut_biomass)
	tr.dump(t, "cropModule.vc_residueCutBiomass", cm.residue_cut_biomass)
	tr.dump(t, "cropModule.vc_CuttingDelayDays", cm.cutting_delay_days)
	tr.dump(t, "cropModule.vc_AnthesisDay", cm.anthesis_day)
	tr.dump(t, "cropModule.vc_MaturityDay", cm.maturity_day)
	tr.dump(t, "cropModule.vc_MaturityReached", cm.maturity_reached)

	tr.dump(t, "cropModule.stepSize24", cm.step_size24)
	tr.dump(t, "cropModule.stepSize240", cm.step_size240)
	tr.dump(t, "cropModule.rad24", cm.rad24)
	tr.dump(t, "cropModule.rad240", cm.rad240)
	tr.dump(t, "cropModule.tfol24", cm.tfol24)
	tr.dump(t, "cropModule.tfol240", cm.tfol240)
	tr.dump(t, "cropModule.index24", cm.index24)
	tr.dump(t, "cropModule.index240", cm.index240)
	tr.dump(t, "cropModule.full24", cm.full24)
	tr.dump(t, "cropModule.full240", cm.full240)

	tr.dump(t, "cropModule.vc_O3_shortTermDamage", cm.o3_short_term_damage)
	tr.dump(t, "cropModule.vc_O3_longTermDamage", cm.o3_long_term_damage)
	tr.dump(t, "cropModule.vc_O3_senescence", cm.o3_senescence)
	tr.dump(t, "cropModule.vc_O3_sumUptake", cm.o3_sum_uptake)
	tr.dump(t, "cropModule.vc_O3_WStomatalClosure", cm.o3_w_stomatal_closure)

	tr.dump(t, "cropModule.assimilatePartCoeffsReduced", cm.assimilate_part_coeffs_reduced)
	tr.dump(t, "cropModule.vc_KTkc", cm.k_tkc)
	tr.dump(t, "cropModule.vc_KTko", cm.k_tko)

	tr.dump(t, "cropModule.stemElongationEventFired", cm.stem_elongation_event_fired)

	tr.dump(t, "cropModule.intercroppingOtherCropHeight", cm.intercropping_other_crop_height)
	tr.dump(t, "cropModule.intercroppingOtherLAIt", cm.intercropping_other_lait)

	tr.dump(t, "cropModule.fractionOfInterceptedRadiation1", cm.fraction_of_intercepted_radiation1)
	tr.dump(t, "cropModule.fractionOfInterceptedRadiation2", cm.fraction_of_intercepted_radiation2)
}

main :: proc() {
	args := os.args
	if len(args) < 2 {
		fmt.eprintln("usage: crop_module_ref <pathToSimJson>")
		os.exit(2)
	}
	path_to_sim_json := args[1]

	arena: jx.Arena
	if !jx.arena_init(&arena) {
		fmt.eprintln("arena init failed")
		os.exit(1)
	}
	defer jx.arena_destroy(&arena)
	a := jx.arena_allocator(&arena)

	// --- build CentralParameterProvider, exactly like central_params_ref ---
	path_of_sim_json, _ := tl.split_path_to_file(path_to_sim_json, a)

	simr := jx.read_and_parse_json_file(path_to_sim_json, a)
	if tl.failure(simr.errs) {
		tl.print_possible_errors(simr.errs)
		os.exit(1)
	}

	simm := make(jx.Object, 0, a)
	for k, v in jx.object_items(simr.result) {
		simm[strings.clone(k, a)] = v
	}
	simm[strings.clone("sim.json", a)] = jx.Value(jx.String(strings.clone(path_to_sim_json, a)))

	fixup :: proc(m: ^jx.Object, key: string, base: string, a: jx.Allocator) {
		pth := jx.string_value_of(m[key] or_else jx.Value{})
		if !tl.is_absolute_path(pth) {
			m[strings.clone(key, a)] = jx.Value(jx.String(strings.concatenate({base, pth}, a)))
		}
	}
	fixup(&simm, "crop.json", path_of_sim_json, a)
	fixup(&simm, "site.json", path_of_sim_json, a)
	fixup(&simm, "climate.csv", path_of_sim_json, a)

	sim_v := jx.Value(simm)

	cropr := jx.read_and_parse_json_file(jx.string_value(sim_v, "crop.json"), a)
	tl.print_possible_errors(cropr.errs)
	siter := jx.read_and_parse_json_file(jx.string_value(sim_v, "site.json"), a)
	tl.print_possible_errors(siter.errs)

	env := mrun.create_env_json_from_json_objects(cropr.result, siter.result, sim_v, a)
	env_params := jx.get(env, "params")

	path_to_soil_dir := tl.fix_system_separator(
		tl.replace_env_vars("${MONICA_PARAMETERS}/soil/", a),
		a,
	)

	cpp := p.make_central_parameter_provider(a)
	_ = p.central_parameter_provider_merge(&cpp, env_params, path_to_soil_dir, a)

	// --- real, live SoilColumn - makeCropModule only reads soilColumn.layers,
	// no need for SoilTemperature/Moisture/Organic here ---
	sc := core.make_soil_column(
		cpp.simulationParameters.p_LayerThickness,
		cpp.userSoilOrganicParameters.ps_MaxMineralisationDepth,
		cpp.siteParameters.vs_SoilParameters[:],
		a,
	)

	// --- real CropParameters / CropResidueParameters, loaded the same way
	// params_ref/main.odin loads them ---
	monica_parameters_dir := tl.fix_system_separator(tl.replace_env_vars("${MONICA_PARAMETERS}", a), a)

	load :: proc(dir, name: string, a: jx.Allocator) -> jx.Value {
		path := strings.concatenate({dir, "/", name}, a)
		r := jx.read_and_parse_json_file(path, a)
		if tl.failure(r.errs) {
			tl.print_possible_errors(r.errs)
			return jx.Value{}
		}
		return r.result
	}

	wheat_crop_params := p.make_crop_parameters()
	_ = p.crop_parameters_merge_sj_cj(
		&wheat_crop_params,
		load(monica_parameters_dir, "crops/wheat.json", a),
		load(monica_parameters_dir, "crops/wheat/winter-wheat.json", a),
	)

	wheat_residue_params: p.Crop_Residue_Parameters
	_ = p.crop_residue_parameters_merge(
		&wheat_residue_params,
		load(monica_parameters_dir, "crop-residues/wheat.json", a),
	)

	t := tr.make_tracer(os.to_stream(os.stdout), a)
	defer tr.destroy_tracer(&t)

	// --- scenario 0: baseline ---
	{
		cm := core.make_crop_module(
			&sc,
			&wheat_crop_params,
			&wheat_residue_params,
			&cpp.siteParameters,
			&cpp.userCropParameters,
			&cpp.simulationParameters,
			noop_fire_event,
			noop_add_organic_matter,
			noop_get_snow_depth,
			nil,
			a,
		)
		tr.set_day(&t, 0)
		dump_crop_module(&t, &cm)
	}

	// --- scenario 1: pc_AdjustRootDepthForSoilProps forced false ---
	{
		crop_mod_params := cpp.userCropParameters
		crop_mod_params.pc_AdjustRootDepthForSoilProps = false
		cm := core.make_crop_module(
			&sc,
			&wheat_crop_params,
			&wheat_residue_params,
			&cpp.siteParameters,
			&crop_mod_params,
			&cpp.simulationParameters,
			noop_fire_event,
			noop_add_organic_matter,
			noop_get_snow_depth,
			nil,
			a,
		)
		tr.set_day(&t, 1)
		dump_crop_module(&t, &cm)
	}

	// --- scenario 2: vs_ImpenetrableLayerDepth clamp branch ---
	{
		site_params := cpp.siteParameters
		site_params.vs_ImpenetrableLayerDepth = 0.2
		cm := core.make_crop_module(
			&sc,
			&wheat_crop_params,
			&wheat_residue_params,
			&site_params,
			&cpp.userCropParameters,
			&cpp.simulationParameters,
			noop_fire_event,
			noop_add_organic_matter,
			noop_get_snow_depth,
			nil,
			a,
		)
		tr.set_day(&t, 2)
		dump_crop_module(&t, &cm)
	}

	// --- scenario 3: synthetic low-Kc cultivar (Kcb "low-coverage crop" branch) ---
	{
		low_kc_crop_params := wheat_crop_params
		low_kc_crop_params.cultivarParams.pc_StageKcFactor = make(
			[dynamic]f64,
			len(wheat_crop_params.cultivarParams.pc_StageKcFactor),
			a,
		)
		for v, i in wheat_crop_params.cultivarParams.pc_StageKcFactor {
			low_kc_crop_params.cultivarParams.pc_StageKcFactor[i] = v * 0.5
		}
		cm := core.make_crop_module(
			&sc,
			&low_kc_crop_params,
			&wheat_residue_params,
			&cpp.siteParameters,
			&cpp.userCropParameters,
			&cpp.simulationParameters,
			noop_fire_event,
			noop_add_organic_matter,
			noop_get_snow_depth,
			nil,
			a,
		)
		tr.set_day(&t, 3)
		dump_crop_module(&t, &cm)
	}
}
