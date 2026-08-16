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
	tr.dump(t, "cropModule.noOfOrgans", cm.noOfOrgans)
	tr.dump(t, "cropModule.noOfDevStages", cm.noOfDevStages)

	tr.dump(t, "cropModule.vc_TransplantShockDuration", cm.vc_TransplantShockDuration)
	tr.dump(t, "cropModule.vc_DaysSinceTransplant", cm.vc_DaysSinceTransplant)
	tr.dump(t, "cropModule.vc_TransplantEfficiency", cm.vc_TransplantEfficiency)

	tr.dump(t, "cropModule.vc_TranspirationDeficit", cm.vc_TranspirationDeficit)
	tr.dump(t, "cropModule.vc_PotentialTranspirationDeficit", cm.vc_PotentialTranspirationDeficit)
	tr.dump(t, "cropModule.vc_ActualTranspirationDeficit", cm.vc_ActualTranspirationDeficit)
	tr.dump(t, "cropModule.vc_TranspirationReduced", cm.vc_TranspirationReduced)
	tr.dump(t, "cropModule.rootNRedux", cm.rootNRedux)
	tr.dump(t, "cropModule.vc_TimeUnderAnoxia", cm.vc_TimeUnderAnoxia)

	tr.dump(t, "cropModule.intercropping", cm.intercropping)
	tr.dump(t, "cropModule.soilColumn", cm.soilColumn)
	tr.dump(t, "cropModule.siteParams", cm.siteParams)
	tr.dump(t, "cropModule.simParams", cm.simParams)
	tr.dump(t, "cropModule.cropModParams", cm.cropModParams)

	tr.dump(t, "cropModule.vc_AbovegroundBiomass", cm.vc_AbovegroundBiomass)
	tr.dump(t, "cropModule.vc_AbovegroundBiomassOld", cm.vc_AbovegroundBiomassOld)
	tr.dump(t, "cropModule.vc_ActualTranspiration", cm.vc_ActualTranspiration)
	tr.dump(t, "cropModule.vc_Assimilates", cm.vc_Assimilates)
	tr.dump(t, "cropModule.vc_AssimilationRate", cm.vc_AssimilationRate)
	tr.dump(t, "cropModule.vc_AstronomicDayLenght", cm.vc_AstronomicDayLenght)
	tr.dump(t, "cropModule.vc_BelowgroundBiomass", cm.vc_BelowgroundBiomass)
	tr.dump(t, "cropModule.vc_BelowgroundBiomassOld", cm.vc_BelowgroundBiomassOld)
	tr.dump(t, "cropModule.vc_ClearDayRadiation", cm.vc_ClearDayRadiation)
	tr.dump(t, "cropModule.pc_CO2Method", cm.pc_CO2Method)
	tr.dump(t, "cropModule.vc_CriticalNConcentration", cm.vc_CriticalNConcentration)
	tr.dump(t, "cropModule.vc_CropDiameter", cm.vc_CropDiameter)
	tr.dump(t, "cropModule.vc_CropFrostRedux", cm.vc_CropFrostRedux)
	tr.dump(t, "cropModule.vc_CropHeatRedux", cm.vc_CropHeatRedux)
	tr.dump(t, "cropModule.vc_CropHeight", cm.vc_CropHeight)
	tr.dump(t, "cropModule.vc_CropNDemand", cm.vc_CropNDemand)
	tr.dump(t, "cropModule.vc_CropNRedux", cm.vc_CropNRedux)
	tr.dump(t, "cropModule.vc_CropWaterUptake", cm.vc_CropWaterUptake)
	tr.dump(t, "cropModule.vc_CurrentTemperatureSum", cm.vc_CurrentTemperatureSum)
	tr.dump(t, "cropModule.vc_CurrentTotalTemperatureSum", cm.vc_CurrentTotalTemperatureSum)
	tr.dump(t, "cropModule.vc_CurrentTotalTemperatureSumRoot", cm.vc_CurrentTotalTemperatureSumRoot)
	tr.dump(t, "cropModule.vc_DaylengthFactor", cm.vc_DaylengthFactor)
	tr.dump(t, "cropModule.vc_DaysAfterBeginFlowering", cm.vc_DaysAfterBeginFlowering)
	tr.dump(t, "cropModule.vc_Declination", cm.vc_Declination)
	tr.dump(t, "cropModule.vc_DevelopmentalStage", cm.vc_DevelopmentalStage)
	tr.dump(t, "cropModule.noOfCropSteps", cm.noOfCropSteps)
	tr.dump(t, "cropModule.vc_DroughtImpactOnFertility", cm.vc_DroughtImpactOnFertility)
	tr.dump(t, "cropModule.vc_EffectiveDayLength", cm.vc_EffectiveDayLength)
	tr.dump(t, "cropModule.vc_ErrorStatus", cm.vc_ErrorStatus)
	tr.dump(t, "cropModule.vc_ErrorMessage", cm.vc_ErrorMessage)
	tr.dump(t, "cropModule.vc_EvaporatedFromIntercept", cm.vc_EvaporatedFromIntercept)
	tr.dump(t, "cropModule.vc_ExtraterrestrialRadiation", cm.vc_ExtraterrestrialRadiation)
	tr.dump(t, "cropModule.vc_FinalDevelopmentalStage", cm.vc_FinalDevelopmentalStage)
	tr.dump(t, "cropModule.vc_FixedN", cm.vc_FixedN)
	tr.dump(t, "cropModule.vc_GlobalRadiation", cm.vc_GlobalRadiation)
	tr.dump(t, "cropModule.vc_GreenAreaIndex", cm.vc_GreenAreaIndex)
	tr.dump(t, "cropModule.vc_GrossAssimilates", cm.vc_GrossAssimilates)
	tr.dump(t, "cropModule.vc_GrossPhotosynthesis", cm.vc_GrossPhotosynthesis)
	tr.dump(t, "cropModule.vc_GrossPhotosynthesis_mol", cm.vc_GrossPhotosynthesis_mol)
	tr.dump(t, "cropModule.vc_GrossPhotosynthesisReference_mol", cm.vc_GrossPhotosynthesisReference_mol)
	tr.dump(t, "cropModule.vc_GrossPrimaryProduction", cm.vc_GrossPrimaryProduction)
	tr.dump(t, "cropModule.vc_GrowthCycleEnded", cm.vc_GrowthCycleEnded)
	tr.dump(t, "cropModule.vc_GrowthRespirationAS", cm.vc_GrowthRespirationAS)
	tr.dump(t, "cropModule.vc_InterceptionStorage", cm.vc_InterceptionStorage)
	tr.dump(t, "cropModule.vc_KcFactor", cm.vc_KcFactor)
	tr.dump(t, "cropModule.vc_LeafAreaIndex", cm.vc_LeafAreaIndex)
	tr.dump(t, "cropModule.vc_sunlitLeafAreaIndex", cm.vc_sunlitLeafAreaIndex)
	tr.dump(t, "cropModule.vc_shadedLeafAreaIndex", cm.vc_shadedLeafAreaIndex)
	tr.dump(t, "cropModule.vc_LT50", cm.vc_LT50)
	tr.dump(t, "cropModule.vc_LT50M", cm.vc_LT50M)
	tr.dump(t, "cropModule.vc_MaintenanceRespirationAS", cm.vc_MaintenanceRespirationAS)
	tr.dump(t, "cropModule.vc_MaxNUptake", cm.vc_MaxNUptake)
	tr.dump(t, "cropModule.vc_MaxRootingDepth", cm.vc_MaxRootingDepth)
	tr.dump(t, "cropModule.vc_NetMaintenanceRespiration", cm.vc_NetMaintenanceRespiration)
	tr.dump(t, "cropModule.vc_NetPhotosynthesis", cm.vc_NetPhotosynthesis)
	tr.dump(t, "cropModule.vc_NetPrecipitation", cm.vc_NetPrecipitation)
	tr.dump(t, "cropModule.vc_NetPrimaryProduction", cm.vc_NetPrimaryProduction)
	tr.dump(t, "cropModule.vc_NConcentrationAbovegroundBiomass", cm.vc_NConcentrationAbovegroundBiomass)
	tr.dump(
		t,
		"cropModule.vc_NConcentrationAbovegroundBiomassOld",
		cm.vc_NConcentrationAbovegroundBiomassOld,
	)
	tr.dump(t, "cropModule.vc_NContentDeficit", cm.vc_NContentDeficit)
	tr.dump(t, "cropModule.vc_NConcentrationRoot", cm.vc_NConcentrationRoot)
	tr.dump(t, "cropModule.vc_NConcentrationRootOld", cm.vc_NConcentrationRootOld)
	tr.dump(t, "cropModule.vc_NUptakeFromLayer", cm.vc_NUptakeFromLayer)
	tr.dump(t, "cropModule.vc_OrganBiomass", cm.vc_OrganBiomass)
	tr.dump(t, "cropModule.vc_OrganDeadBiomass", cm.vc_OrganDeadBiomass)
	tr.dump(t, "cropModule.vc_OrganGreenBiomass", cm.vc_OrganGreenBiomass)
	tr.dump(t, "cropModule.vc_OrganGrowthIncrement", cm.vc_OrganGrowthIncrement)
	tr.dump(t, "cropModule.vc_OrganSenescenceIncrement", cm.vc_OrganSenescenceIncrement)
	tr.dump(t, "cropModule.vc_OvercastDayRadiation", cm.vc_OvercastDayRadiation)
	tr.dump(t, "cropModule.vc_OxygenDeficit", cm.vc_OxygenDeficit)
	tr.dump(t, "cropModule.vc_PhotoperiodicDaylength", cm.vc_PhotoperiodicDaylength)
	tr.dump(t, "cropModule.vc_PhotActRadiationMean", cm.vc_PhotActRadiationMean)
	tr.dump(t, "cropModule.vc_PotentialTranspiration", cm.vc_PotentialTranspiration)
	tr.dump(t, "cropModule.vc_ReferenceEvapotranspiration", cm.vc_ReferenceEvapotranspiration)
	tr.dump(t, "cropModule.vc_RelativeTotalDevelopment", cm.vc_RelativeTotalDevelopment)
	tr.dump(t, "cropModule.vc_RemainingEvapotranspiration", cm.vc_RemainingEvapotranspiration)
	tr.dump(t, "cropModule.vc_ReserveAssimilatePool", cm.vc_ReserveAssimilatePool)
	tr.dump(t, "cropModule.vc_RootBiomass", cm.vc_RootBiomass)
	tr.dump(t, "cropModule.vc_RootBiomassOld", cm.vc_RootBiomassOld)
	tr.dump(t, "cropModule.vc_RootDensity", cm.vc_RootDensity)
	tr.dump(t, "cropModule.vc_RootDiameter", cm.vc_RootDiameter)
	tr.dump(t, "cropModule.vc_RootEffectivity", cm.vc_RootEffectivity)
	tr.dump(t, "cropModule.vc_RootingDepth", cm.vc_RootingDepth)
	tr.dump(t, "cropModule.vc_RootingDepth_m", cm.vc_RootingDepth_m)
	tr.dump(t, "cropModule.vc_RootingZone", cm.vc_RootingZone)
	tr.dump(t, "cropModule.vc_SoilCoverage", cm.vc_SoilCoverage)
	tr.dump(t, "cropModule.vs_SoilMineralNContent", cm.vs_SoilMineralNContent)
	tr.dump(t, "cropModule.vc_SoilSpecificMaxRootingDepth", cm.vc_SoilSpecificMaxRootingDepth)
	tr.dump(t, "cropModule.vs_SoilSpecificMaxRootingDepth", cm.vs_SoilSpecificMaxRootingDepth)
	tr.dump(t, "cropModule.vc_KcbFactor", cm.vc_KcbFactor)
	tr.dump(t, "cropModule.vc_Kcb_ini", cm.vc_Kcb_ini)
	tr.dump(t, "cropModule.vc_Kcb_mid", cm.vc_Kcb_mid)
	tr.dump(t, "cropModule.vc_Kcb_end", cm.vc_Kcb_end)
	tr.dump(t, "cropModule.vc_StomataResistance", cm.vc_StomataResistance)
	tr.dump(t, "cropModule.vc_StorageOrgan", cm.vc_StorageOrgan)
	tr.dump(t, "cropModule.vc_TargetNConcentration", cm.vc_TargetNConcentration)
	tr.dump(t, "cropModule.vc_TimeStep", cm.vc_TimeStep)
	tr.dump(t, "cropModule.TimeUnderAnoxiaThresholdDefault", cm.TimeUnderAnoxiaThresholdDefault)
	tr.dump(t, "cropModule.vc_TotalBiomass", cm.vc_TotalBiomass)
	tr.dump(t, "cropModule.vc_TotalBiomassNContent", cm.vc_TotalBiomassNContent)
	tr.dump(t, "cropModule.vc_TotalCropHeatImpact", cm.vc_TotalCropHeatImpact)
	tr.dump(t, "cropModule.vc_TotalNInput", cm.vc_TotalNInput)
	tr.dump(t, "cropModule.vc_TotalNUptake", cm.vc_TotalNUptake)
	tr.dump(t, "cropModule.vc_TotalRespired", cm.vc_TotalRespired)
	tr.dump(t, "cropModule.vc_Respiration", cm.vc_Respiration)
	tr.dump(t, "cropModule.vc_SumTotalNUptake", cm.vc_SumTotalNUptake)
	tr.dump(t, "cropModule.vc_TotalRootLength", cm.vc_TotalRootLength)
	tr.dump(t, "cropModule.vc_TotalTemperatureSum", cm.vc_TotalTemperatureSum)
	tr.dump(t, "cropModule.vc_TemperatureSumToFlowering", cm.vc_TemperatureSumToFlowering)
	tr.dump(t, "cropModule.vc_Transpiration", cm.vc_Transpiration)
	tr.dump(t, "cropModule.vc_TranspirationRedux", cm.vc_TranspirationRedux)
	tr.dump(t, "cropModule.vc_VernalisationDays", cm.vc_VernalisationDays)
	tr.dump(t, "cropModule.vc_VernalisationFactor", cm.vc_VernalisationFactor)
	tr.dump(t, "cropModule.dyingOut", cm.dyingOut)
	tr.dump(t, "cropModule.vc_AccumulatedETa", cm.vc_AccumulatedETa)
	tr.dump(t, "cropModule.vc_AccumulatedTranspiration", cm.vc_AccumulatedTranspiration)
	tr.dump(t, "cropModule.vc_sumExportedCutBiomass", cm.vc_sumExportedCutBiomass)
	tr.dump(t, "cropModule.vc_exportedCutBiomass", cm.vc_exportedCutBiomass)
	tr.dump(t, "cropModule.vc_sumResidueCutBiomass", cm.vc_sumResidueCutBiomass)
	tr.dump(t, "cropModule.vc_residueCutBiomass", cm.vc_residueCutBiomass)
	tr.dump(t, "cropModule.vc_CuttingDelayDays", cm.vc_CuttingDelayDays)
	tr.dump(t, "cropModule.vc_AnthesisDay", cm.vc_AnthesisDay)
	tr.dump(t, "cropModule.vc_MaturityDay", cm.vc_MaturityDay)
	tr.dump(t, "cropModule.vc_MaturityReached", cm.vc_MaturityReached)

	tr.dump(t, "cropModule.stepSize24", cm.stepSize24)
	tr.dump(t, "cropModule.stepSize240", cm.stepSize240)
	tr.dump(t, "cropModule.rad24", cm.rad24)
	tr.dump(t, "cropModule.rad240", cm.rad240)
	tr.dump(t, "cropModule.tfol24", cm.tfol24)
	tr.dump(t, "cropModule.tfol240", cm.tfol240)
	tr.dump(t, "cropModule.index24", cm.index24)
	tr.dump(t, "cropModule.index240", cm.index240)
	tr.dump(t, "cropModule.full24", cm.full24)
	tr.dump(t, "cropModule.full240", cm.full240)

	tr.dump(t, "cropModule.vc_O3_shortTermDamage", cm.vc_O3_shortTermDamage)
	tr.dump(t, "cropModule.vc_O3_longTermDamage", cm.vc_O3_longTermDamage)
	tr.dump(t, "cropModule.vc_O3_senescence", cm.vc_O3_senescence)
	tr.dump(t, "cropModule.vc_O3_sumUptake", cm.vc_O3_sumUptake)
	tr.dump(t, "cropModule.vc_O3_WStomatalClosure", cm.vc_O3_WStomatalClosure)

	tr.dump(t, "cropModule.assimilatePartCoeffsReduced", cm.assimilatePartCoeffsReduced)
	tr.dump(t, "cropModule.vc_KTkc", cm.vc_KTkc)
	tr.dump(t, "cropModule.vc_KTko", cm.vc_KTko)

	tr.dump(t, "cropModule.stemElongationEventFired", cm.stemElongationEventFired)

	tr.dump(t, "cropModule.intercroppingOtherCropHeight", cm.intercroppingOtherCropHeight)
	tr.dump(t, "cropModule.intercroppingOtherLAIt", cm.intercroppingOtherLAIt)

	tr.dump(t, "cropModule.fractionOfInterceptedRadiation1", cm.fractionOfInterceptedRadiation1)
	tr.dump(t, "cropModule.fractionOfInterceptedRadiation2", cm.fractionOfInterceptedRadiation2)
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
