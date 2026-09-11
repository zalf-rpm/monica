// Odin side of the phase 5 checkpoint 7 (step() orchestration) differential
// test - the final checkpoint of phase 5.
//
// Must emit byte-identical output to
// odin/tests/cpp_ref/crop_module_step_ref_main.cpp - see that file's header
// comment for the full-chain / real-callback / natural-germination
// rationale.
// Run odin/tests/cpp_ref/run_crop_module_step.sh to build both and diff them.
package crop_module_step_ref

import core "../../monica/core"
import p "../../monica/params"
import mrun "../../monica/run"
import tr "../../monica/trace"
import clim "../../support/climate"
import jx "../../support/jsonx"
import tl "../../support/tools"
import "core:fmt"
import "core:os"
import "core:strconv"
import "core:strings"

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
	core.soil_organic_add_organic_matter(
		g_soil_organic,
		g_residue_params,
		layer2amount,
		nConcentration,
	)
}

recording_fire_event :: proc(event: string) {
	tr.write_line_str(g_tracer, "cropModule.event", event)
}

dump_crop_module :: proc(t: ^tr.Tracer, cm: ^core.Crop_Module) {
	P :: "cropModule"

	// --- phenology (checkpoint 3) ---
	tr.dump(t, P + ".vc_DevelopmentalStage", cm.developmental_stage)
	tr.dump(t, P + ".vc_CurrentTemperatureSum", cm.current_temperature_sum)
	tr.dump(t, P + ".vc_CurrentTotalTemperatureSum", cm.current_total_temperature_sum)
	tr.dump(t, P + ".vc_ErrorStatus", cm.error_status)
	tr.dump(t, P + ".vc_AnthesisDay", cm.anthesis_day)
	tr.dump(t, P + ".vc_MaturityDay", cm.maturity_day)
	tr.dump(t, P + ".vc_MaturityReached", cm.maturity_reached)
	tr.dump(t, P + ".vc_DaylengthFactor", cm.daylength_factor)
	tr.dump(t, P + ".vc_VernalisationFactor", cm.vernalisation_factor)
	tr.dump(t, P + ".vc_VernalisationDays", cm.vernalisation_days)
	tr.dump(t, P + ".vc_RelativeTotalDevelopment", cm.relative_total_development)
	tr.dump(t, P + ".vc_KcFactor", cm.kc_factor)
	tr.dump(t, P + ".vc_KcbFactor", cm.kcb_factor)
	tr.dump(t, P + ".vc_CropHeight", cm.crop_height)
	tr.dump(t, P + ".vc_CropDiameter", cm.crop_diameter)
	tr.dump(t, P + ".vc_LeafAreaIndex", cm.leaf_area_index)
	tr.dump(t, P + ".vc_GreenAreaIndex", cm.green_area_index)
	tr.dump(t, P + ".vc_SoilCoverage", cm.soil_coverage)

	// --- stress (checkpoint 5) ---
	tr.dump(t, P + ".vc_CropHeatRedux", cm.crop_heat_redux)
	tr.dump(t, P + ".vc_TotalCropHeatImpact", cm.total_crop_heat_impact)
	tr.dump(t, P + ".vc_LT50", cm.lt50)
	tr.dump(t, P + ".vc_LT50M", cm.lt50_m)
	tr.dump(t, P + ".vc_CropFrostRedux", cm.crop_frost_redux)
	tr.dump(t, P + ".vc_DroughtImpactOnFertility", cm.drought_impact_on_fertility)

	// --- nitrogen / biomass (checkpoints 5-6) ---
	tr.dump(t, P + ".vc_CriticalNConcentration", cm.critical_n_concentration)
	tr.dump(t, P + ".vc_TargetNConcentration", cm.target_n_concentration)
	tr.dump(t, P + ".rootNRedux", cm.root_n_redux)
	tr.dump(t, P + ".vc_CropNRedux", cm.crop_n_redux)
	tr.dump(t, P + ".vc_AbovegroundBiomass", cm.aboveground_biomass)
	tr.dump(t, P + ".vc_BelowgroundBiomass", cm.belowground_biomass)
	tr.dump(t, P + ".vc_TotalBiomass", cm.total_biomass)
	tr.dump(t, P + ".vc_OrganBiomass", cm.organ_biomass)
	tr.dump(t, P + ".vc_OrganGreenBiomass", cm.organ_green_biomass)
	tr.dump(t, P + ".vc_RootBiomass", cm.root_biomass)
	tr.dump(t, P + ".vc_TotalBiomassNContent", cm.total_biomass_n_content)
	tr.dump(t, P + ".vc_CropNDemand", cm.crop_n_demand)
	tr.dump(t, P + ".vc_MaxRootingDepth", cm.max_rooting_depth)
	tr.dump(t, P + ".vc_RootingDepth_m", cm.rooting_depth_m)
	tr.dump(t, P + ".vc_RootingDepth", cm.rooting_depth)
	tr.dump(t, P + ".vc_RootingZone", cm.rooting_zone)
	tr.dump(t, P + ".vc_TotalRootLength", cm.total_root_length)

	// --- water (checkpoint 6) ---
	tr.dump(t, P + ".vc_ReferenceEvapotranspiration", cm.reference_evapotranspiration)
	tr.dump(t, P + ".vc_OxygenDeficit", cm.oxygen_deficit)
	tr.dump(t, P + ".vc_RootEffectivity", cm.root_effectivity)
	tr.dump(t, P + ".vc_RootDensity", cm.root_density)
	tr.dump(
		t,
		"cropModule.soilColumn->layers[0].vs_SoilMoisture_m3",
		cm.soil_column.layers[0].soil_moisture_m3,
	)
	tr.dump(
		t,
		"cropModule.soilColumn->layers[0].vs_FieldCapacity",
		cm.soil_column.layers[0].field_capacity,
	)
	tr.dump(
		t,
		"cropModule.soilColumn->layers[0].vs_PermanentWiltingPoint",
		cm.soil_column.layers[0].permanent_wilting_point,
	)
	tr.dump(
		t,
		"cropModule.soilColumn->vm_GroundwaterTableLayer",
		cm.soil_column.vm_GroundwaterTableLayer,
	)
	tr.dump(t, P + ".vc_ActualTranspiration", cm.actual_transpiration)
	tr.dump(t, P + ".vc_TranspirationDeficit", cm.transpiration_deficit)
	tr.dump(t, P + ".vc_PotentialTranspiration", cm.potential_transpiration)
	tr.dump(t, P + ".vc_Transpiration", cm.transpiration)
	tr.dump(t, P + ".vc_NetPrecipitation", cm.net_precipitation)
	tr.dump(t, P + ".vc_InterceptionStorage", cm.interception_storage)

	// --- nitrogen uptake (checkpoint 6) ---
	tr.dump(t, P + ".vc_TotalNUptake", cm.total_n_uptake)
	tr.dump(t, P + ".vc_TotalNInput", cm.total_n_input)
	tr.dump(t, P + ".vc_FixedN", cm.fixed_n)
	tr.dump(t, P + ".vc_SumTotalNUptake", cm.sum_total_n_uptake)
	tr.dump(t, P + ".vc_NUptakeFromLayer", cm.n_uptake_from_layer)

	// --- photosynthesis (checkpoint 4) ---
	tr.dump(t, P + ".vc_AssimilationRate", cm.assimilation_rate)
	tr.dump(t, P + ".vc_GrossPhotosynthesis", cm.gross_photosynthesis)
	tr.dump(t, P + ".vc_Assimilates", cm.assimilates)
	tr.dump(t, P + ".vc_TotalRespired", cm.total_respired)

	// --- production (checkpoints 4/6) ---
	tr.dump(t, P + ".vc_GrossPrimaryProduction", cm.gross_primary_production)
	tr.dump(t, P + ".vc_NetPrimaryProduction", cm.net_primary_production)

	// --- transplant / cutting bookkeeping (checkpoint 7) ---
	tr.dump(t, P + ".vc_TransplantEfficiency", cm.transplant_efficiency)
	tr.dump(t, P + ".vc_DaysSinceTransplant", cm.days_since_transplant)
	tr.dump(t, P + ".noOfCropSteps", cm.no_of_crop_steps)
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

	monica_parameters_dir := tl.fix_system_separator(
		tl.replace_env_vars("${MONICA_PARAMETERS}", a),
		a,
	)

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
	g_residue_params = &cm.residue_params.base

	// widen phase 4's bare-soil coupling to this real, live crop - matches
	// production soilcolumn::putCrop/soiltransport::putCrop wiring.
	sm.crop_module = &cm
	so.crop_module = &cm
	str.cropModule = &cm

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
		core.soil_temperature_step(
			&st,
			tmin,
			tmax,
			globrad,
			cm.soil_coverage,
			sm.snow_component.snow_depth,
			sm.frost_component.temperature_under_snow,
		)
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
		tr.dump(&t, "soilMoistureDiag.vc_PercentageSoilCoverage", sm.soil_coverage_percent)
		tr.dump(&t, "soilMoistureDiag.vc_KcFactor", sm.kc_factor)
		tr.dump(&t, "soilMoistureDiag.vc_NetPrecipitation", sm.net_precipitation_mm)
		free_all(context.temp_allocator)
	}
}
