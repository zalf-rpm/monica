// Odin side of the phase 5 checkpoint 3 (phenology + canopy geometry)
// differential test.
//
// Must emit byte-identical output to
// odin/tests/cpp_ref/crop_module_phenology_ref_main.cpp - see that file's
// header comment for the two-scenario rationale and the step()-excerpt this
// driver replicates.
// Run odin/tests/cpp_ref/run_crop_module_phenology.sh to build both and diff them.
package crop_module_phenology_ref

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

noop_fire_event :: proc(event: string) {}
noop_add_organic_matter :: proc(layer2amount: map[int]f64, nConcentration: f64) {}
noop_get_snow_depth :: proc(avgAirTemp: f64) -> (f64, f64) {return 0.0, 0.0}

@(private)
jn :: proc(path, name: string) -> string {
	return fmt.tprintf("%s.%s", path, name)
}

dump_crop_module_phenology :: proc(t: ^tr.Tracer, path: string, cm: ^core.Crop_Module) {
	tr.dump(t, jn(path, "vc_Declination"), cm.vc_Declination)
	tr.dump(t, jn(path, "vc_AstronomicDayLenght"), cm.vc_AstronomicDayLenght)
	tr.dump(t, jn(path, "vc_EffectiveDayLength"), cm.vc_EffectiveDayLength)
	tr.dump(t, jn(path, "vc_PhotoperiodicDaylength"), cm.vc_PhotoperiodicDaylength)
	tr.dump(t, jn(path, "vc_PhotActRadiationMean"), cm.vc_PhotActRadiationMean)
	tr.dump(t, jn(path, "vc_ClearDayRadiation"), cm.vc_ClearDayRadiation)
	tr.dump(t, jn(path, "vc_OvercastDayRadiation"), cm.vc_OvercastDayRadiation)
	tr.dump(t, jn(path, "vc_ExtraterrestrialRadiation"), cm.vc_ExtraterrestrialRadiation)
	tr.dump(t, jn(path, "vc_GlobalRadiation"), cm.vc_GlobalRadiation)

	tr.dump(t, jn(path, "vc_OxygenDeficit"), cm.vc_OxygenDeficit)
	tr.dump(t, jn(path, "vc_TimeUnderAnoxia"), cm.vc_TimeUnderAnoxia)

	tr.dump(t, jn(path, "vc_DevelopmentalStage"), cm.vc_DevelopmentalStage)
	tr.dump(t, jn(path, "vc_CurrentTemperatureSum"), cm.vc_CurrentTemperatureSum)
	tr.dump(t, jn(path, "vc_CurrentTotalTemperatureSum"), cm.vc_CurrentTotalTemperatureSum)
	tr.dump(t, jn(path, "vc_ErrorStatus"), cm.vc_ErrorStatus)
	tr.dump(t, jn(path, "vc_ErrorMessage"), cm.vc_ErrorMessage)
	tr.dump(t, jn(path, "vc_GrowthCycleEnded"), cm.vc_GrowthCycleEnded)
	tr.dump(t, jn(path, "noOfOrgans"), cm.noOfOrgans)
	tr.dump(t, jn(path, "noOfDevStages"), cm.noOfDevStages)
	tr.dump(t, jn(path, "cropParams.cultivarParams.pc_CultivarId"), cm.cropParams.cultivarParams.pc_CultivarId)

	tr.dump(t, jn(path, "vc_AnthesisDay"), cm.vc_AnthesisDay)
	tr.dump(t, jn(path, "vc_MaturityDay"), cm.vc_MaturityDay)
	tr.dump(t, jn(path, "vc_MaturityReached"), cm.vc_MaturityReached)

	tr.dump(t, jn(path, "vc_DaylengthFactor"), cm.vc_DaylengthFactor)

	tr.dump(t, jn(path, "vc_VernalisationFactor"), cm.vc_VernalisationFactor)
	tr.dump(t, jn(path, "vc_VernalisationDays"), cm.vc_VernalisationDays)

	tr.dump(t, jn(path, "vc_RelativeTotalDevelopment"), cm.vc_RelativeTotalDevelopment)

	tr.dump(t, jn(path, "vc_KcFactor"), cm.vc_KcFactor)

	tr.dump(t, jn(path, "vc_CropHeight"), cm.vc_CropHeight)
	tr.dump(t, jn(path, "vc_CropDiameter"), cm.vc_CropDiameter)

	tr.dump(t, jn(path, "vc_LeafAreaIndex"), cm.vc_LeafAreaIndex)
	tr.dump(t, jn(path, "vc_GreenAreaIndex"), cm.vc_GreenAreaIndex)

	tr.dump(t, jn(path, "vc_SoilCoverage"), cm.vc_SoilCoverage)
}

// Replicates just the step()-body excerpt this checkpoint needs, for one day.
// Matches step()'s call order exactly, minus everything not yet ported - see
// crop_module_phenology_ref_main.cpp's phenology_day_step.
phenology_day_step :: proc(
	cm: ^core.Crop_Module,
	meanAirTemperature, globalRadiation, sunshineHours: f64,
	currentDate: d.Date,
	julianDayOverride: int = -1,
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

	vs_JulianDay := julianDayOverride >= 0 ? julianDayOverride : int(d.julian_day(currentDate))

	core.fc_radiation(cm, f64(vs_JulianDay), globalRadiation, sunshineHours)

	cm.vc_OxygenDeficit = core.fc_oxygen_deficiency(cm, pc_CriticalOxygenContent[cm.vc_DevelopmentalStage])

	old_DevelopmentalStage := cm.vc_DevelopmentalStage

	// start accumulating temperature sums only after dormancy
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
	}

	cm.noOfCropSteps += 1
}

main :: proc() {
	args := os.args
	if len(args) < 4 {
		fmt.eprintln("usage: crop_module_phenology_ref <pathToSimJson> <pathToClimateCsv> <numDaysA>")
		os.exit(2)
	}
	path_to_sim_json := args[1]
	path_to_climate_csv := args[2]
	num_days_a, _ := strconv.parse_int(args[3])

	arena: jx.Arena
	if !jx.arena_init(&arena) {
		fmt.eprintln("arena init failed")
		os.exit(1)
	}
	defer jx.arena_destroy(&arena)
	a := jx.arena_allocator(&arena)

	// --- build CentralParameterProvider, exactly like crop_module_ref/main.odin ---
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

	sc := core.make_soil_column(
		cpp.simulationParameters.p_LayerThickness,
		cpp.userSoilOrganicParameters.ps_MaxMineralisationDepth,
		cpp.siteParameters.vs_SoilParameters[:],
		a,
	)

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

	// === Scenario A: real wheat, real climate, started past germination ===
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
		core.set_stage(&cm, 1)

		copts := clim.make_csv_via_header_options()
		_ = clim.csv_via_header_options_merge(
			&copts,
			jx.obj(a, {"no-of-climate-file-header-lines", jx.i(2)}, {"csv-separator", jx.s(",", a)}),
			a,
		)
		clim_res := clim.read_climate_data_from_csv_file_via_headers(
			path_to_climate_csv,
			copts,
			true,
			a,
		)
		if tl.failure(clim_res.errs) {
			tl.print_possible_errors(clim_res.errs)
			os.exit(1)
		}
		da := clim_res.result

		n := clim.data_accessor_no_of_steps_possible(&da)
		num_days_a_int := int(num_days_a)
		if num_days_a_int > 0 && num_days_a_int < n {
			n = num_days_a_int
		}

		for day in 0 ..< n {
			tavg := clim.data_accessor_data_for_timestep(&da, .tavg, day)
			globrad := clim.data_accessor_data_for_timestep(&da, .globrad, day)
			current_date := clim.data_accessor_date_for_step(&da, day)

			phenology_day_step(&cm, tavg, globrad, 0.0, current_date)

			tr.set_day(&t, day)
			dump_crop_module_phenology(&t, "cropModuleA", &cm)
			free_all(context.temp_allocator)
		}
	}

	// === Scenario B: synthetic perennial/short-day/WangEngel/germination ===
	{
		synth_crop_params := wheat_crop_params
		synth_crop_params.cultivarParams.pc_Perennial = true
		synth_crop_params.cultivarParams.pc_MinTempDev_WE = 0.0
		synth_crop_params.cultivarParams.pc_OptTempDev_WE = 20.0
		synth_crop_params.cultivarParams.pc_MaxTempDev_WE = 35.0
		synth_crop_params.__enable_vernalisation_factor_fix__ = false
		synth_crop_params.speciesParams.dormancyStartDoy = 6
		synth_crop_params.speciesParams.dormancyEndDoy = 3
		synth_crop_params.cultivarParams.pc_StageTemperatureSum = make(
			[dynamic]f64,
			len(wheat_crop_params.cultivarParams.pc_StageTemperatureSum),
			a,
		)
		for v, i in wheat_crop_params.cultivarParams.pc_StageTemperatureSum {
			synth_crop_params.cultivarParams.pc_StageTemperatureSum[i] = min(v, 5.0)
		}
		synth_crop_params.cultivarParams.pc_DaylengthRequirement = make(
			[dynamic]f64,
			len(wheat_crop_params.cultivarParams.pc_DaylengthRequirement),
			a,
		)
		for v, i in wheat_crop_params.cultivarParams.pc_DaylengthRequirement {
			synth_crop_params.cultivarParams.pc_DaylengthRequirement[i] = -abs(v) - 1.0
		}

		perennial_next_season := synth_crop_params
		perennial_next_season.cultivarParams.pc_CultivarId = "synthetic-next-season"

		synth_crop_mod_params := cpp.userCropParameters
		synth_crop_mod_params.__enable_Phenology_WangEngelTemperatureResponse__ = true
		synth_crop_mod_params.__enable_vernalisation_factor_fix__ = true

		cm := core.make_crop_module(
			&sc,
			&synth_crop_params,
			&wheat_residue_params,
			&cpp.siteParameters,
			&synth_crop_mod_params,
			&cpp.simulationParameters,
			noop_fire_event,
			noop_add_organic_matter,
			noop_get_snow_depth,
			nil,
			a,
		)
		perennial_next_season_ptr := new(p.Crop_Parameters, a)
		perennial_next_season_ptr^ = perennial_next_season
		cm.perennialCropParams = perennial_next_season_ptr
		cm.cropParams.cultivarParams.pc_CultivarId = "synthetic-season-1"

		Day :: struct {
			meanAirTemperature, globalRadiation, sunshineHours: f64,
			julianDay:                                          int,
		}
		days := []Day {
			{-2.0, 8.0, 0.0, 1},
			{-1.0, 8.0, 0.0, 2},
			{0.0, 0.0, 6.0, 3},
			{5.0, 10.0, 0.0, 4},
			{10.0, 12.0, 0.0, 5},
			{15.0, 14.0, 0.0, 6},
			{16.0, 14.0, 0.0, 7},
			{17.0, 15.0, 0.0, 8},
			{18.0, 15.0, 0.0, 9},
			{19.0, 16.0, 0.0, 10},
			{20.0, 16.0, 0.0, 11},
			{21.0, 17.0, 0.0, 12},
			{22.0, 17.0, 0.0, 13},
			{20.0, 16.0, 0.0, 14},
			{18.0, 15.0, 0.0, 15},
			{16.0, 14.0, 0.0, 16},
			{14.0, 13.0, 0.0, 17},
			{12.0, 12.0, 0.0, 18},
			{10.0, 11.0, 0.0, 19},
			{8.0, 10.0, 0.0, 20},
		}

		for day_data, day in days {
			// rises above pc_BaseTemperature[0]=0 partway through
			cm.soilColumn.layers[0].vs_SoilTemperature = -3.0 + f64(day)

			current_date := d.make_date(u8(day_data.julianDay), 1, 2000, false, false, true)
			phenology_day_step(
				&cm,
				day_data.meanAirTemperature,
				day_data.globalRadiation,
				day_data.sunshineHours,
				current_date,
				day_data.julianDay,
			)

			tr.set_day(&t, day)
			dump_crop_module_phenology(&t, "cropModuleB", &cm)
			free_all(context.temp_allocator)
		}
	}
}
