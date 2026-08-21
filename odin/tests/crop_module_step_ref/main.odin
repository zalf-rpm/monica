// Odin side of the phase 5 checkpoint 7 (step() orchestration) differential
// test - the final checkpoint of phase 5.
//
// Must emit byte-identical output to
// odin/tests/cpp_ref/crop_module_step_ref_main.cpp - see that file's header
// comment for the full-chain / real-callback / natural-germination
// rationale.
// Run odin/tests/cpp_ref/run_crop_module_step.sh to build both and diff them.
package crop_module_step_ref

import "core:fmt"
import "core:os"
import "core:strconv"
import "core:strings"
import clim "../../support/climate"
import core "../../monica/core"
import p "../../monica/params"
import tr "../../monica/trace"
import mrun "../../monica/run"
import jx "../../support/jsonx"
import tl "../../support/tools"

ATM_CO2 :: 380.0 // ppm, illustrative constant
ATM_O3 :: 60.0 // ppb, illustrative constant

// Odin `proc` values can't capture - see checkpoints 5/6's file comments.
g_soil_moisture: ^core.Soil_Moisture
g_soil_organic: ^core.Soil_Organic
g_residue_params: ^p.Organic_Matter_Parameters
g_tracer: ^tr.Tracer

real_get_snow_depth :: proc(avgAirTemp: f64) -> (f64, f64) {
	return core.get_snow_depth_and_calc_temperature_under_snow(g_soil_moisture, avgAirTemp)
}

real_add_organic_matter :: proc(layer2amount: map[int]f64, nConcentration: f64) {
	core.soil_organic_add_organic_matter(g_soil_organic, g_residue_params, layer2amount, nConcentration)
}

recording_fire_event :: proc(event: string) {
	tr.write_line_str(g_tracer, "cropModule.event", event)
}

dump_crop_module :: proc(t: ^tr.Tracer, cm: ^core.Crop_Module) {
	P :: "cropModule"

	// --- phenology (checkpoint 3) ---
	tr.dump(t, P + ".vc_DevelopmentalStage", cm.vc_DevelopmentalStage)
	tr.dump(t, P + ".vc_CurrentTemperatureSum", cm.vc_CurrentTemperatureSum)
	tr.dump(t, P + ".vc_CurrentTotalTemperatureSum", cm.vc_CurrentTotalTemperatureSum)
	tr.dump(t, P + ".vc_ErrorStatus", cm.vc_ErrorStatus)
	tr.dump(t, P + ".vc_AnthesisDay", cm.vc_AnthesisDay)
	tr.dump(t, P + ".vc_MaturityDay", cm.vc_MaturityDay)
	tr.dump(t, P + ".vc_MaturityReached", cm.vc_MaturityReached)
	tr.dump(t, P + ".vc_DaylengthFactor", cm.vc_DaylengthFactor)
	tr.dump(t, P + ".vc_VernalisationFactor", cm.vc_VernalisationFactor)
	tr.dump(t, P + ".vc_VernalisationDays", cm.vc_VernalisationDays)
	tr.dump(t, P + ".vc_RelativeTotalDevelopment", cm.vc_RelativeTotalDevelopment)
	tr.dump(t, P + ".vc_KcFactor", cm.vc_KcFactor)
	tr.dump(t, P + ".vc_KcbFactor", cm.vc_KcbFactor)
	tr.dump(t, P + ".vc_CropHeight", cm.vc_CropHeight)
	tr.dump(t, P + ".vc_CropDiameter", cm.vc_CropDiameter)
	tr.dump(t, P + ".vc_LeafAreaIndex", cm.vc_LeafAreaIndex)
	tr.dump(t, P + ".vc_GreenAreaIndex", cm.vc_GreenAreaIndex)
	tr.dump(t, P + ".vc_SoilCoverage", cm.vc_SoilCoverage)

	// --- stress (checkpoint 5) ---
	tr.dump(t, P + ".vc_CropHeatRedux", cm.vc_CropHeatRedux)
	tr.dump(t, P + ".vc_TotalCropHeatImpact", cm.vc_TotalCropHeatImpact)
	tr.dump(t, P + ".vc_LT50", cm.vc_LT50)
	tr.dump(t, P + ".vc_LT50M", cm.vc_LT50M)
	tr.dump(t, P + ".vc_CropFrostRedux", cm.vc_CropFrostRedux)
	tr.dump(t, P + ".vc_DroughtImpactOnFertility", cm.vc_DroughtImpactOnFertility)

	// --- nitrogen / biomass (checkpoints 5-6) ---
	tr.dump(t, P + ".vc_CriticalNConcentration", cm.vc_CriticalNConcentration)
	tr.dump(t, P + ".vc_TargetNConcentration", cm.vc_TargetNConcentration)
	tr.dump(t, P + ".rootNRedux", cm.rootNRedux)
	tr.dump(t, P + ".vc_CropNRedux", cm.vc_CropNRedux)
	tr.dump(t, P + ".vc_AbovegroundBiomass", cm.vc_AbovegroundBiomass)
	tr.dump(t, P + ".vc_BelowgroundBiomass", cm.vc_BelowgroundBiomass)
	tr.dump(t, P + ".vc_TotalBiomass", cm.vc_TotalBiomass)
	tr.dump(t, P + ".vc_OrganBiomass", cm.vc_OrganBiomass)
	tr.dump(t, P + ".vc_OrganGreenBiomass", cm.vc_OrganGreenBiomass)
	tr.dump(t, P + ".vc_RootBiomass", cm.vc_RootBiomass)
	tr.dump(t, P + ".vc_TotalBiomassNContent", cm.vc_TotalBiomassNContent)
	tr.dump(t, P + ".vc_CropNDemand", cm.vc_CropNDemand)
	tr.dump(t, P + ".vc_MaxRootingDepth", cm.vc_MaxRootingDepth)
	tr.dump(t, P + ".vc_RootingDepth_m", cm.vc_RootingDepth_m)
	tr.dump(t, P + ".vc_RootingDepth", cm.vc_RootingDepth)
	tr.dump(t, P + ".vc_RootingZone", cm.vc_RootingZone)
	tr.dump(t, P + ".vc_TotalRootLength", cm.vc_TotalRootLength)

	// --- water (checkpoint 6) ---
	tr.dump(t, P + ".vc_ReferenceEvapotranspiration", cm.vc_ReferenceEvapotranspiration)
	tr.dump(t, P + ".vc_OxygenDeficit", cm.vc_OxygenDeficit)
	tr.dump(t, P + ".vc_RootEffectivity", cm.vc_RootEffectivity)
	tr.dump(t, P + ".vc_RootDensity", cm.vc_RootDensity)
	tr.dump(
		t,
		"cropModule.soilColumn->layers[0].vs_SoilMoisture_m3",
		cm.soilColumn.layers[0].vs_SoilMoisture_m3,
	)
	tr.dump(
		t,
		"cropModule.soilColumn->layers[0].vs_FieldCapacity",
		cm.soilColumn.layers[0].vs_FieldCapacity,
	)
	tr.dump(
		t,
		"cropModule.soilColumn->layers[0].vs_PermanentWiltingPoint",
		cm.soilColumn.layers[0].vs_PermanentWiltingPoint,
	)
	tr.dump(
		t,
		"cropModule.soilColumn->vm_GroundwaterTableLayer",
		cm.soilColumn.vm_GroundwaterTableLayer,
	)
	tr.dump(t, P + ".vc_ActualTranspiration", cm.vc_ActualTranspiration)
	tr.dump(t, P + ".vc_TranspirationDeficit", cm.vc_TranspirationDeficit)
	tr.dump(t, P + ".vc_PotentialTranspiration", cm.vc_PotentialTranspiration)
	tr.dump(t, P + ".vc_Transpiration", cm.vc_Transpiration)
	tr.dump(t, P + ".vc_NetPrecipitation", cm.vc_NetPrecipitation)
	tr.dump(t, P + ".vc_InterceptionStorage", cm.vc_InterceptionStorage)

	// --- nitrogen uptake (checkpoint 6) ---
	tr.dump(t, P + ".vc_TotalNUptake", cm.vc_TotalNUptake)
	tr.dump(t, P + ".vc_TotalNInput", cm.vc_TotalNInput)
	tr.dump(t, P + ".vc_FixedN", cm.vc_FixedN)
	tr.dump(t, P + ".vc_SumTotalNUptake", cm.vc_SumTotalNUptake)
	tr.dump(t, P + ".vc_NUptakeFromLayer", cm.vc_NUptakeFromLayer)

	// --- photosynthesis (checkpoint 4) ---
	tr.dump(t, P + ".vc_AssimilationRate", cm.vc_AssimilationRate)
	tr.dump(t, P + ".vc_GrossPhotosynthesis", cm.vc_GrossPhotosynthesis)
	tr.dump(t, P + ".vc_Assimilates", cm.vc_Assimilates)
	tr.dump(t, P + ".vc_TotalRespired", cm.vc_TotalRespired)

	// --- production (checkpoints 4/6) ---
	tr.dump(t, P + ".vc_GrossPrimaryProduction", cm.vc_GrossPrimaryProduction)
	tr.dump(t, P + ".vc_NetPrimaryProduction", cm.vc_NetPrimaryProduction)

	// --- transplant / cutting bookkeeping (checkpoint 7) ---
	tr.dump(t, P + ".vc_TransplantEfficiency", cm.vc_TransplantEfficiency)
	tr.dump(t, P + ".vc_DaysSinceTransplant", cm.vc_DaysSinceTransplant)
	tr.dump(t, P + ".noOfCropSteps", cm.noOfCropSteps)
}

main :: proc() {
	args := os.args
	if len(args) < 4 {
		fmt.eprintln("usage: crop_module_step_ref <pathToSimJson> <pathToClimateCsv> <numDays>")
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

	// --- real, live, fully-wired soil-module chain ---
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
	so := core.make_soil_organic(&sc, cpp.userSoilOrganicParameters)
	str := core.make_soil_transport(
		cpp.userSoilTransportParameters,
		&sc,
		&cpp.siteParameters,
		&cpp.userEnvironmentParameters,
		&cpp.userCropParameters,
	)
	g_soil_moisture = &sm
	g_soil_organic = &so

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
		recording_fire_event,
		real_add_organic_matter,
		real_get_snow_depth,
		nil,
		a,
	)
	g_residue_params = &cm.residueParams.base

	// widen phase 4's bare-soil coupling to this real, live crop - matches
	// production soilcolumn::putCrop/soiltransport::putCrop wiring.
	sm.cropModule = &cm
	so.cropModule = &cm
	str.cropModule = &cm

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

	dual_kc_method := cpp.simulationParameters.dualKcMethod

	t := tr.make_tracer(os.to_stream(os.stdout), a)
	defer tr.destroy_tracer(&t)
	g_tracer = &t

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

		tr.set_day(&t, day)

		// C++: calcSoilSurfaceTemperature reads st->monica->currentCropModule ?
		// ->vc_SoilCoverage : 0.0 - now that a live crop is wired in (unlike
		// every earlier checkpoint, all bare soil), pass its real coverage
		// instead of the hardcoded 0.0 those checkpoints used.
		core.soil_temperature_step(&st, tmin, tmax, globrad, cm.vc_SoilCoverage, sm.snowComponent.vm_SnowDepth, sm.frostComponent.vm_TemperatureUnderSnow)
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
			dual_kc_method,
		)

		core.crop_module_step(
			&cm,
			tavg,
			tmax,
			tmin,
			globrad,
			0.0,
			current_date,
			(relhumid / 100.0),
			wind,
			cpp.userEnvironmentParameters.p_WindSpeedHeight,
			ATM_CO2,
			ATM_O3,
			precip,
			-1.0,
			a,
		)

		core.soil_organic_step(&so, tavg, precip, wind)
		core.soil_transport_step(&str)

		dump_crop_module(&t, &cm)
		tr.dump(&t, "soilMoistureDiag.vc_PercentageSoilCoverage", sm.vc_PercentageSoilCoverage)
		tr.dump(&t, "soilMoistureDiag.vc_KcFactor", sm.vc_KcFactor)
		tr.dump(&t, "soilMoistureDiag.vc_NetPrecipitation", sm.vc_NetPrecipitation)
		free_all(context.temp_allocator)
	}
}
