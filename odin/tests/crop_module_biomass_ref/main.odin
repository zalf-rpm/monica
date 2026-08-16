// Odin side of the phase 5 checkpoint 5 (biomass/dry matter + stress)
// differential test.
//
// Must emit byte-identical output to
// odin/tests/cpp_ref/crop_module_biomass_ref_main.cpp - see that file's
// header comment for the callback-wiring rationale and the day_step this
// driver replicates.
// Run odin/tests/cpp_ref/run_crop_module_biomass.sh to build both and diff them.
package crop_module_biomass_ref

import "core:fmt"
import "core:os"
import "core:strconv"
import "core:strings"
import clim "../../support/climate"
import core "../../monica/core"
import p "../../monica/params"
import tr "../../monica/trace"
import mrun "../../monica/run"
import d "../../support/date"
import jx "../../support/jsonx"
import tl "../../support/tools"

ATM_CO2 :: 380.0 // ppm, illustrative constant
ATM_O3 :: 60.0 // ppb, illustrative constant

noop_fire_event :: proc(event: string) {}

// Odin `proc` values can't capture surrounding state (no closures), unlike
// C++'s capturing lambdas - these test-driver-only globals are the
// workaround, matching the "driver globals for callback wiring" pattern this
// checkpoint's C++ side does natively via lambda capture.
g_soil_moisture: ^core.Soil_Moisture

g_last_organic_matter_total: f64
g_last_organic_matter_nconc: f64
g_organic_matter_call_count: int

real_get_snow_depth :: proc(avgAirTemp: f64) -> (f64, f64) {
	return core.get_snow_depth_and_calc_temperature_under_snow(g_soil_moisture, avgAirTemp)
}

recording_add_organic_matter :: proc(layer2amount: map[int]f64, nConcentration: f64) {
	total := 0.0
	for _, v in layer2amount {
		total += v
	}
	g_last_organic_matter_total = total
	g_last_organic_matter_nconc = nConcentration
	g_organic_matter_call_count += 1
}

@(private)
jn :: proc(path, name: string) -> string {
	return fmt.tprintf("%s.%s", path, name)
}

dump_crop_module_biomass :: proc(t: ^tr.Tracer, path: string, cm: ^core.Crop_Module) {
	tr.dump(t, jn(path, "vc_CropHeatRedux"), cm.vc_CropHeatRedux)
	tr.dump(t, jn(path, "vc_TotalCropHeatImpact"), cm.vc_TotalCropHeatImpact)
	tr.dump(t, jn(path, "vc_DaysAfterBeginFlowering"), cm.vc_DaysAfterBeginFlowering)

	tr.dump(t, jn(path, "vc_LT50"), cm.vc_LT50)
	tr.dump(t, jn(path, "vc_LT50M"), cm.vc_LT50M)
	tr.dump(t, jn(path, "vc_CropFrostRedux"), cm.vc_CropFrostRedux)

	tr.dump(t, jn(path, "vc_DroughtImpactOnFertility"), cm.vc_DroughtImpactOnFertility)

	tr.dump(t, jn(path, "vc_CriticalNConcentration"), cm.vc_CriticalNConcentration)
	tr.dump(t, jn(path, "vc_TargetNConcentration"), cm.vc_TargetNConcentration)
	tr.dump(t, jn(path, "rootNRedux"), cm.rootNRedux)
	tr.dump(t, jn(path, "vc_CropNRedux"), cm.vc_CropNRedux)

	tr.dump(t, jn(path, "vc_AbovegroundBiomass"), cm.vc_AbovegroundBiomass)
	tr.dump(t, jn(path, "vc_BelowgroundBiomass"), cm.vc_BelowgroundBiomass)
	tr.dump(t, jn(path, "vc_TotalBiomass"), cm.vc_TotalBiomass)
	tr.dump(t, jn(path, "vc_OrganBiomass"), cm.vc_OrganBiomass)
	tr.dump(t, jn(path, "vc_OrganDeadBiomass"), cm.vc_OrganDeadBiomass)
	tr.dump(t, jn(path, "vc_OrganGreenBiomass"), cm.vc_OrganGreenBiomass)
	tr.dump(t, jn(path, "vc_OrganGrowthIncrement"), cm.vc_OrganGrowthIncrement)
	tr.dump(t, jn(path, "vc_OrganSenescenceIncrement"), cm.vc_OrganSenescenceIncrement)
	tr.dump(t, jn(path, "vc_RootBiomass"), cm.vc_RootBiomass)
	tr.dump(t, jn(path, "vc_TotalBiomassNContent"), cm.vc_TotalBiomassNContent)
	tr.dump(t, jn(path, "vc_CropNDemand"), cm.vc_CropNDemand)

	tr.dump(t, jn(path, "vc_MaxRootingDepth"), cm.vc_MaxRootingDepth)
	tr.dump(t, jn(path, "vc_RootingDepth_m"), cm.vc_RootingDepth_m)
	tr.dump(t, jn(path, "vc_RootingDepth"), cm.vc_RootingDepth)
	tr.dump(t, jn(path, "vc_RootingZone"), cm.vc_RootingZone)
	tr.dump(t, jn(path, "vc_TotalRootLength"), cm.vc_TotalRootLength)
	tr.dump(t, jn(path, "vc_RootDensity"), cm.vc_RootDensity)
	tr.dump(t, jn(path, "vc_RootDiameter"), cm.vc_RootDiameter)
	tr.dump(t, jn(path, "vc_MaxNUptake"), cm.vc_MaxNUptake)
	tr.dump(t, jn(path, "vc_CurrentTotalTemperatureSumRoot"), cm.vc_CurrentTotalTemperatureSumRoot)

	tr.dump(t, jn(path, "vc_GrossPrimaryProduction"), cm.vc_GrossPrimaryProduction)
	tr.dump(t, jn(path, "vc_NetPrimaryProduction"), cm.vc_NetPrimaryProduction)
}

// checkpoint 3/4's day_step, extended with the checkpoint-5 functions in
// step()'s real order - see crop_module_biomass_ref_main.cpp's day_step.
day_step :: proc(
	cm: ^core.Crop_Module,
	meanAirTemperature, maxAirTemperature, minAirTemperature: f64,
	globalRadiation, sunshineHours: f64,
	currentDate: d.Date,
	frostKillOn: bool,
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
	speciesPs := &cm.cropParams.speciesParams

	vs_JulianDay := int(d.julian_day(currentDate))

	core.fc_radiation(cm, f64(vs_JulianDay), globalRadiation, sunshineHours)

	cm.vc_OxygenDeficit = core.fc_oxygen_deficiency(cm, pc_CriticalOxygenContent[cm.vc_DevelopmentalStage])

	old_DevelopmentalStage := cm.vc_DevelopmentalStage

	if !d.is_valid(cm.perennialCropDormancyPeriodEndDate) {
		if speciesPs.dormancyEndDoy == 0 {
			cm.perennialCropDormancyPeriodEndDate = currentDate
		} else {
			cm.perennialCropDormancyPeriodEndDate = d.add(
				d.make_date(1, 1, u16(d.year(currentDate)), false, false, d.DEFAULT_USE_LEAP_YEARS),
				u64(speciesPs.dormancyEndDoy - 1),
			)
		}
	}
	if !pc_Perennial || d.ge(currentDate, cm.perennialCropDormancyPeriodEndDate) {
		core.fc_crop_developmental_stage(
			cm,
			meanAirTemperature,
			cm.soilColumn.layers[0].vs_SoilMoisture_m3,
			cm.soilColumn.layers[0].vs_FieldCapacity,
			cm.soilColumn.layers[0].vs_PermanentWiltingPoint,
			currentDate,
		)
	}

	if core.is_anthesis_day(cm, old_DevelopmentalStage, cm.vc_DevelopmentalStage) {
		cm.vc_AnthesisDay = vs_JulianDay
	} else if core.is_maturity_day(cm, old_DevelopmentalStage, cm.vc_DevelopmentalStage) {
		cm.vc_MaturityDay = vs_JulianDay
		cm.vc_MaturityReached = true
	}

	cm.vc_DaylengthFactor = core.fc_daylength_factor(
		cm,
		pc_DaylengthRequirement[cm.vc_DevelopmentalStage],
		cm.vc_EffectiveDayLength,
		cm.vc_PhotoperiodicDaylength,
		pc_BaseDaylength[cm.vc_DevelopmentalStage],
	)

	cm.vc_VernalisationFactor, cm.vc_VernalisationDays = core.fc_vernalisation_factor(
		cm,
		meanAirTemperature,
		pc_VernalisationRequirement[cm.vc_DevelopmentalStage],
		cm.vc_VernalisationDays,
	)

	if cm.vc_TotalTemperatureSum == 0.0 {
		cm.vc_RelativeTotalDevelopment = 0.0
	} else {
		cm.vc_RelativeTotalDevelopment = cm.vc_CurrentTotalTemperatureSum / cm.vc_TotalTemperatureSum
	}

	if cm.vc_DevelopmentalStage == 0 {
		cm.vc_KcFactor = cm.siteParams.bareSoilKcFactor
	} else {
		cm.vc_KcFactor = core.fc_kc_factor(
			cm,
			pc_StageTemperatureSum[cm.vc_DevelopmentalStage],
			cm.vc_CurrentTemperatureSum[cm.vc_DevelopmentalStage],
			pc_StageKcFactor[cm.vc_DevelopmentalStage],
			pc_StageKcFactor[cm.vc_DevelopmentalStage - 1],
		)
	}

	if cm.vc_DevelopmentalStage > 0 {
		maxCropHeight := pc_MaxCropHeight

		core.fc_crop_size(cm, maxCropHeight)

		core.fc_crop_green_area(
			cm,
			meanAirTemperature,
			cm.vc_OrganGrowthIncrement[core.Organ_Leaf],
			cm.vc_OrganSenescenceIncrement[core.Organ_Leaf],
			pc_SpecificLeafArea[cm.vc_DevelopmentalStage - 1],
			pc_SpecificLeafArea[cm.vc_DevelopmentalStage],
			pc_SpecificLeafArea[1],
			pc_StageTemperatureSum[cm.vc_DevelopmentalStage],
			cm.vc_CurrentTemperatureSum[cm.vc_DevelopmentalStage],
		)

		cm.vc_SoilCoverage = core.fc_soil_coverage(cm)

		core.fc_crop_photosynthesis(
			cm,
			meanAirTemperature,
			maxAirTemperature,
			minAirTemperature,
			ATM_CO2,
			ATM_O3,
			currentDate,
		)

		core.fc_heat_stress_impact(cm, maxAirTemperature, minAirTemperature)

		if frostKillOn {
			core.fc_frost_kill(cm, maxAirTemperature, minAirTemperature)
		}

		core.fc_drought_impact_on_fertility(cm)

		core.fc_crop_nitrogen(cm)

		core.fc_crop_dry_matter(cm, meanAirTemperature, allocator)

		cm.vc_GrossPrimaryProduction = core.fc_gross_primary_production(cm)
		cm.vc_NetPrimaryProduction = core.fc_net_primary_production(cm, cm.vc_TotalRespired)
	}

	cm.noOfCropSteps += 1
}

main :: proc() {
	args := os.args
	if len(args) < 4 {
		fmt.eprintln("usage: crop_module_biomass_ref <pathToSimJson> <pathToClimateCsv> <numDays>")
		os.exit(2)
	}
	path_to_sim_json := args[1]
	path_to_climate_csv := args[2]
	num_days, _ := strconv.parse_int(args[3])

	arena: jx.Arena
	if !jx.arena_init(&arena) {
		fmt.eprintln("arena init failed")
		os.exit(1)
	}
	defer jx.arena_destroy(&arena)
	a := jx.arena_allocator(&arena)

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

	// --- real SoilColumn/SoilTemperature/SoilMoisture, bare soil (no
	// cropModule wiring - checkpoint 7's job) ---
	sc := core.make_soil_column(
		cpp.simulationParameters.p_LayerThickness,
		cpp.userSoilOrganicParameters.ps_MaxMineralisationDepth,
		cpp.siteParameters.vs_SoilParameters[:],
		a,
	)
	st := core.make_soil_temperature(
		&sc,
		cpp.userSoilTemperatureParameters,
		cpp.userEnvironmentParameters.p_timeStep,
	)
	sm := core.make_soil_moisture(
		&sc,
		&cpp.siteParameters,
		cpp.userSoilMoistureParameters,
		&cpp.userEnvironmentParameters,
		&cpp.userCropParameters,
		cpp.simulationParameters.p_LayerThickness,
		a,
	)
	sm.cropModule = nil
	g_soil_moisture = &sm

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

	cm := core.make_crop_module(
		&sc,
		&wheat_crop_params,
		&wheat_residue_params,
		&cpp.siteParameters,
		&cpp.userCropParameters,
		&cpp.simulationParameters,
		noop_fire_event,
		recording_add_organic_matter,
		real_get_snow_depth,
		nil,
		a,
	)
	core.set_stage(&cm, 1)

	copts := clim.make_csv_via_header_options()
	_ = clim.csv_via_header_options_merge(
		&copts,
		jx.obj(a, {"no-of-climate-file-header-lines", jx.i(2)}, {"csv-separator", jx.s(",", a)}),
		a,
	)
	clim_res := clim.read_climate_data_from_csv_file_via_headers(path_to_climate_csv, copts, true, a)
	if tl.failure(clim_res.errs) {
		tl.print_possible_errors(clim_res.errs)
		os.exit(1)
	}
	da := clim_res.result

	n := clim.data_accessor_no_of_steps_possible(&da)
	num_days_int := int(num_days)
	if num_days_int > 0 && num_days_int < n {
		n = num_days_int
	}

	frost_kill_on := cpp.simulationParameters.pc_FrostKillOn

	t := tr.make_tracer(os.to_stream(os.stdout), a)
	defer tr.destroy_tracer(&t)

	for day in 0 ..< n {
		tmin := clim.data_accessor_data_for_timestep(&da, .tmin, day)
		tmax := clim.data_accessor_data_for_timestep(&da, .tmax, day)
		tavg := clim.data_accessor_data_for_timestep(&da, .tavg, day)
		wind := clim.data_accessor_data_for_timestep(&da, .wind, day)
		globrad := clim.data_accessor_data_for_timestep(&da, .globrad, day)
		precip := clim.data_accessor_data_for_timestep(&da, .precip, day)
		relhumid := clim.data_accessor_data_for_timestep(&da, .relhumid, day)
		current_date := clim.data_accessor_date_for_step(&da, day)
		julday := clim.data_accessor_julian_day_for_step(&da, day)

		vs_GroundwaterDepth: f64 = (day % 40) < 15 ? 3.0 : 15.0
		et0 := -1.0 // climate-min.csv has no et0 column

		core.soil_temperature_step(&st, tmin, tmax, globrad, 0.0, sm.snowComponent.vm_SnowDepth, sm.frostComponent.vm_TemperatureUnderSnow)
		core.soil_moisture_step(
			&sm,
			vs_GroundwaterDepth,
			precip,
			tmax,
			tmin,
			(relhumid / 100.0),
			tavg,
			wind,
			cpp.userEnvironmentParameters.p_WindSpeedHeight,
			globrad,
			julday,
			et0,
		)

		day_step(&cm, tavg, tmax, tmin, globrad, 0.0, current_date, frost_kill_on, a)

		tr.set_day(&t, day)
		dump_crop_module_biomass(&t, "cropModule", &cm)
		tr.write_line_f64(&t, "cropModule.recording.lastOrganicMatterTotal", g_last_organic_matter_total)
		tr.write_line_f64(&t, "cropModule.recording.lastOrganicMatterNConc", g_last_organic_matter_nconc)
		tr.write_line_int(&t, "cropModule.recording.organicMatterCallCount", g_organic_matter_call_count)
		free_all(context.temp_allocator)
	}

	// === Scenario B: synthetic forced root senescence ===
	// Real wheat's pc_OrganSenescenceRate for the root organ is 0 at every
	// stage - scenario A's dailyDeadRootBiomassIncrement is genuinely,
	// correctly always 0 there, and recording_add_organic_matter never fires.
	// This scenario forces a small positive root senescence rate to exercise
	// fc_move_dead_root_biomass_to_soil's real addOrganicMatter call.
	{
		sc2 := core.make_soil_column(
			cpp.simulationParameters.p_LayerThickness,
			cpp.userSoilOrganicParameters.ps_MaxMineralisationDepth,
			cpp.siteParameters.vs_SoilParameters[:],
			a,
		)
		st2 := core.make_soil_temperature(
			&sc2,
			cpp.userSoilTemperatureParameters,
			cpp.userEnvironmentParameters.p_timeStep,
		)
		sm2 := core.make_soil_moisture(
			&sc2,
			&cpp.siteParameters,
			cpp.userSoilMoistureParameters,
			&cpp.userEnvironmentParameters,
			&cpp.userCropParameters,
			cpp.simulationParameters.p_LayerThickness,
			a,
		)
		sm2.cropModule = nil
		g_soil_moisture = &sm2

		senescent_crop_params := wheat_crop_params
		senescent_crop_params.cultivarParams.pc_OrganSenescenceRate = make(
			[dynamic][dynamic]f64,
			len(wheat_crop_params.cultivarParams.pc_OrganSenescenceRate),
			a,
		)
		for row, i in wheat_crop_params.cultivarParams.pc_OrganSenescenceRate {
			senescent_crop_params.cultivarParams.pc_OrganSenescenceRate[i] = make(
				[dynamic]f64,
				len(row),
				a,
			)
			copy(senescent_crop_params.cultivarParams.pc_OrganSenescenceRate[i][:], row[:])
			if len(row) > 0 {
				senescent_crop_params.cultivarParams.pc_OrganSenescenceRate[i][0] = 0.01 // root organ
			}
		}

		g_last_organic_matter_total = 0
		g_last_organic_matter_nconc = 0
		g_organic_matter_call_count = 0

		cm_b := core.make_crop_module(
			&sc2,
			&senescent_crop_params,
			&wheat_residue_params,
			&cpp.siteParameters,
			&cpp.userCropParameters,
			&cpp.simulationParameters,
			noop_fire_event,
			recording_add_organic_matter,
			real_get_snow_depth,
			nil,
			a,
		)
		core.set_stage(&cm_b, 1)

		n_b := min(60, clim.data_accessor_no_of_steps_possible(&da))
		for day in 0 ..< n_b {
			tmin := clim.data_accessor_data_for_timestep(&da, .tmin, day)
			tmax := clim.data_accessor_data_for_timestep(&da, .tmax, day)
			tavg := clim.data_accessor_data_for_timestep(&da, .tavg, day)
			wind := clim.data_accessor_data_for_timestep(&da, .wind, day)
			globrad := clim.data_accessor_data_for_timestep(&da, .globrad, day)
			precip := clim.data_accessor_data_for_timestep(&da, .precip, day)
			relhumid := clim.data_accessor_data_for_timestep(&da, .relhumid, day)
			current_date := clim.data_accessor_date_for_step(&da, day)
			julday := clim.data_accessor_julian_day_for_step(&da, day)

			vs_GroundwaterDepth: f64 = (day % 40) < 15 ? 3.0 : 15.0
			et0 := -1.0

			core.soil_temperature_step(&st2, tmin, tmax, globrad, 0.0, sm2.snowComponent.vm_SnowDepth, sm2.frostComponent.vm_TemperatureUnderSnow)
			core.soil_moisture_step(
				&sm2,
				vs_GroundwaterDepth,
				precip,
				tmax,
				tmin,
				(relhumid / 100.0),
				tavg,
				wind,
				cpp.userEnvironmentParameters.p_WindSpeedHeight,
				globrad,
				julday,
				et0,
			)

			day_step(&cm_b, tavg, tmax, tmin, globrad, 0.0, current_date, frost_kill_on, a)

			tr.set_day(&t, day)
			tr.dump(&t, "cropModuleB.vc_OrganDeadBiomass[0]", cm_b.vc_OrganDeadBiomass[0])
			tr.write_line_f64(&t, "cropModuleB.recording.lastOrganicMatterTotal", g_last_organic_matter_total)
			tr.write_line_f64(&t, "cropModuleB.recording.lastOrganicMatterNConc", g_last_organic_matter_nconc)
			tr.write_line_int(&t, "cropModuleB.recording.organicMatterCallCount", g_organic_matter_call_count)
			free_all(context.temp_allocator)
		}
	}
}
