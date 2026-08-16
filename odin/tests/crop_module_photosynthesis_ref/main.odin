// Odin side of the phase 5 checkpoint 4 (photosynthesis + assimilation)
// differential test.
//
// Must emit byte-identical output to
// odin/tests/cpp_ref/crop_module_photosynthesis_ref_main.cpp - see that
// file's header comment for the four-scenario rationale and the day_step
// this driver replicates.
// Run odin/tests/cpp_ref/run_crop_module_photosynthesis.sh to build both and diff them.
package crop_module_photosynthesis_ref

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
noop_add_organic_matter :: proc(layer2amount: map[int]f64, nConcentration: f64) {}
noop_get_snow_depth :: proc(avgAirTemp: f64) -> (f64, f64) {return 0.0, 0.0}

@(private)
jn :: proc(path, name: string) -> string {
	return fmt.tprintf("%s.%s", path, name)
}

dump_crop_module_photosynthesis :: proc(t: ^tr.Tracer, path: string, cm: ^core.Crop_Module) {
	tr.dump(t, jn(path, "vc_AssimilationRate"), cm.vc_AssimilationRate)
	tr.dump(t, jn(path, "vc_KTkc"), cm.vc_KTkc)
	tr.dump(t, jn(path, "vc_KTko"), cm.vc_KTko)
	// cropPhotosynthesisResults' kc/ko/oi/ci/comp/vcMax/jMax/jj/jj1000/jv
	// fields (and, downstream, jjvEmissions) are not dumped - see the comment
	// in crop_module_photosynthesis_ref_main.cpp's dump_crop_module_photosynthesis.
	tr.dump(t, jn(path, "vc_GrossPhotosynthesis"), cm.vc_GrossPhotosynthesis)
	tr.dump(t, jn(path, "vc_GrossPhotosynthesis_mol"), cm.vc_GrossPhotosynthesis_mol)
	tr.dump(t, jn(path, "vc_GrossPhotosynthesisReference_mol"), cm.vc_GrossPhotosynthesisReference_mol)
	tr.dump(t, jn(path, "vc_Assimilates"), cm.vc_Assimilates)
	tr.dump(t, jn(path, "vc_GrossAssimilates"), cm.vc_GrossAssimilates)
	tr.dump(t, jn(path, "vc_MaintenanceRespirationAS"), cm.vc_MaintenanceRespirationAS)
	tr.dump(t, jn(path, "vc_GrowthRespirationAS"), cm.vc_GrowthRespirationAS)
	tr.dump(t, jn(path, "vc_TotalRespired"), cm.vc_TotalRespired)
	tr.dump(t, jn(path, "vc_NetMaintenanceRespiration"), cm.vc_NetMaintenanceRespiration)
	tr.dump(t, jn(path, "fractionOfInterceptedRadiation1"), cm.fractionOfInterceptedRadiation1)

	tr.dump(t, jn(path, "vc_GrossPrimaryProduction"), cm.vc_GrossPrimaryProduction)
	tr.dump(t, jn(path, "vc_NetPrimaryProduction"), cm.vc_NetPrimaryProduction)
	tr.dump(t, jn(path, "vc_Respiration"), cm.vc_Respiration)

	tr.dump(t, jn(path, "vc_O3_shortTermDamage"), cm.vc_O3_shortTermDamage)
	tr.dump(t, jn(path, "vc_O3_longTermDamage"), cm.vc_O3_longTermDamage)
	tr.dump(t, jn(path, "vc_O3_senescence"), cm.vc_O3_senescence)
	tr.dump(t, jn(path, "vc_O3_sumUptake"), cm.vc_O3_sumUptake)
	tr.dump(t, jn(path, "vc_O3_WStomatalClosure"), cm.vc_O3_WStomatalClosure)

	tr.dump(t, jn(path, "guentherEmissions.isoprene_emission"), cm.guentherEmissions.isoprene_emission)
	tr.dump(
		t,
		jn(path, "guentherEmissions.monoterpene_emission"),
		cm.guentherEmissions.monoterpene_emission,
	)

	tr.dump(t, jn(path, "vc_sunlitLeafAreaIndex"), cm.vc_sunlitLeafAreaIndex)
	tr.dump(t, jn(path, "vc_shadedLeafAreaIndex"), cm.vc_shadedLeafAreaIndex)
}

// checkpoint 3's phenology_day_step extended with fc_crop_photosynthesis/
// fc_gross_primary_production/fc_net_primary_production, matching step()'s
// real order - see crop_module_photosynthesis_ref_main.cpp's day_step.
day_step :: proc(
	cm: ^core.Crop_Module,
	meanAirTemperature, maxAirTemperature, minAirTemperature: f64,
	globalRadiation, sunshineHours: f64,
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

		cm.vc_GrossPrimaryProduction = core.fc_gross_primary_production(cm)
		cm.vc_NetPrimaryProduction = core.fc_net_primary_production(cm, cm.vc_TotalRespired)
	}

	cm.noOfCropSteps += 1
}

main :: proc() {
	args := os.args
	if len(args) < 4 {
		fmt.eprintln("usage: crop_module_photosynthesis_ref <pathToSimJson> <pathToClimateCsv> <numDaysA>")
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

	t := tr.make_tracer(os.to_stream(os.stdout), a)
	defer tr.destroy_tracer(&t)

	// === Scenario A: real wheat, default flags, real climate ===
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

		n := clim.data_accessor_no_of_steps_possible(&da)
		num_days_a_int := int(num_days_a)
		if num_days_a_int > 0 && num_days_a_int < n {
			n = num_days_a_int
		}

		for day in 0 ..< n {
			tmin := clim.data_accessor_data_for_timestep(&da, .tmin, day)
			tmax := clim.data_accessor_data_for_timestep(&da, .tmax, day)
			tavg := clim.data_accessor_data_for_timestep(&da, .tavg, day)
			globrad := clim.data_accessor_data_for_timestep(&da, .globrad, day)
			current_date := clim.data_accessor_date_for_step(&da, day)

			day_step(&cm, tavg, tmax, tmin, globrad, 0.0, current_date)

			tr.set_day(&t, day)
			dump_crop_module_photosynthesis(&t, "cropModuleA", &cm)
			free_all(context.temp_allocator)
		}
	}

	// === Scenario B: real wheat, hourly FvCB forced on, rooted ===
	{
		hourly_crop_mod_params := cpp.userCropParameters
		hourly_crop_mod_params.__enable_hourly_FvCB_photosynthesis__ = true

		cm := core.make_crop_module(
			&sc,
			&wheat_crop_params,
			&wheat_residue_params,
			&cpp.siteParameters,
			&hourly_crop_mod_params,
			&cpp.simulationParameters,
			noop_fire_event,
			noop_add_organic_matter,
			noop_get_snow_depth,
			nil,
			a,
		)
		core.set_stage(&cm, 1)
		cm.vc_RootingDepth = 3 // root distribution is checkpoint 6, not yet ported

		n := min(15, clim.data_accessor_no_of_steps_possible(&da))

		for day in 0 ..< n {
			tmin := clim.data_accessor_data_for_timestep(&da, .tmin, day)
			tmax := clim.data_accessor_data_for_timestep(&da, .tmax, day)
			tavg := clim.data_accessor_data_for_timestep(&da, .tavg, day)
			globrad := clim.data_accessor_data_for_timestep(&da, .globrad, day)
			current_date := clim.data_accessor_date_for_step(&da, day)

			day_step(&cm, tavg, tmax, tmin, globrad, 0.0, current_date)

			tr.set_day(&t, day)
			dump_crop_module_photosynthesis(&t, "cropModuleB", &cm)
			free_all(context.temp_allocator)
		}
	}

	// === Scenario C: real wheat, pc_CO2Method forced to 2 (Hoffmann) ===
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
		cm.pc_CO2Method = 2

		n := min(20, clim.data_accessor_no_of_steps_possible(&da))

		for day in 0 ..< n {
			tmin := clim.data_accessor_data_for_timestep(&da, .tmin, day)
			tmax := clim.data_accessor_data_for_timestep(&da, .tmax, day)
			tavg := clim.data_accessor_data_for_timestep(&da, .tavg, day)
			globrad := clim.data_accessor_data_for_timestep(&da, .globrad, day)
			current_date := clim.data_accessor_date_for_step(&da, day)

			day_step(&cm, tavg, tmax, tmin, globrad, 0.0, current_date)

			tr.set_day(&t, day)
			dump_crop_module_photosynthesis(&t, "cropModuleC", &cm)
			free_all(context.temp_allocator)
		}
	}

	// === Scenario D: synthetic C4 cultivar (pc_CarboxylationPathway=2) ===
	{
		c4_crop_params := wheat_crop_params
		c4_crop_params.speciesParams.pc_CarboxylationPathway = 2

		cm := core.make_crop_module(
			&sc,
			&c4_crop_params,
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

		n := min(20, clim.data_accessor_no_of_steps_possible(&da))

		for day in 0 ..< n {
			tmin := clim.data_accessor_data_for_timestep(&da, .tmin, day)
			tmax := clim.data_accessor_data_for_timestep(&da, .tmax, day)
			tavg := clim.data_accessor_data_for_timestep(&da, .tavg, day)
			globrad := clim.data_accessor_data_for_timestep(&da, .globrad, day)
			current_date := clim.data_accessor_date_for_step(&da, day)

			day_step(&cm, tavg, tmax, tmin, globrad, 0.0, current_date)

			tr.set_day(&t, day)
			dump_crop_module_photosynthesis(&t, "cropModuleD", &cm)
			free_all(context.temp_allocator)
		}
	}
}
