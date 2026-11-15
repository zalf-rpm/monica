// Odin side of the phase 5 checkpoint 5 (biomass/dry matter + stress)
// differential test.
//
// Must emit byte-identical output to
// odin/tests/cpp_ref/crop_module_biomass_ref_main.cpp - see that file's
// header comment for the callback-wiring rationale and the day_step this
// driver replicates.
// Run odin/tests/cpp_ref/run_crop_module_biomass.sh to build both and diff them.
package crop_module_biomass_ref

import core "../../monica/core"
import p "../../monica/params"
import mrun "../../monica/run"
import tr "../../monica/trace"
import clim "../../support/climate"
import d "../../support/date"
import jx "../../support/jsonx"
import tl "../../support/tools"
import "core:fmt"
import "core:os"
import "core:strconv"
import "core:strings"

ATM_CO2 :: 380.0 // ppm, illustrative constant
ATM_O3 :: 60.0 // ppb, illustrative constant

// Odin `proc` values can't capture surrounding state (no closures), unlike
// C++'s capturing lambdas - these test-driver-only globals are the
// workaround, matching the "driver globals for callback wiring" pattern this
// checkpoint's C++ side does natively via lambda capture.
g_soil_moisture: ^core.Soil_Moisture

g_last_organic_matter_total: f64
g_last_organic_matter_nconc: f64
g_organic_matter_call_count: int

recording_add_organic_matter :: proc(layer2amount: map[int]f64, n_concentration: f64) {
	// Odin map iteration order is unspecified, unlike C++'s std::map (sorted
	// ascending by key) - and floating-point addition is not associative, so
	// summing in map-iteration order can round differently than the C++
	// side's sorted-order sum by a bit or two. Sort keys first, matching the
	// technique odin/monica/trace/trace.odin's dump_map_int_f64 already uses.
	keys := make([dynamic]int, 0, len(layer2amount), context.temp_allocator)
	for k in layer2amount {
		append(&keys, k)
	}
	for i in 1 ..< len(keys) {
		k := keys[i]
		j := i - 1
		for j >= 0 && keys[j] > k {
			keys[j + 1] = keys[j]
			j -= 1
		}
		keys[j + 1] = k
	}

	total := 0.0
	for k in keys {
		total += layer2amount[k]
	}
	g_last_organic_matter_total = total
	g_last_organic_matter_nconc = n_concentration
	g_organic_matter_call_count += 1
}

@(private)
jn :: proc(path, name: string) -> string {
	return fmt.tprintf("%s.%s", path, name)
}

dump_crop_module_biomass :: proc(t: ^tr.Tracer, path: string, cm: ^core.Crop_Module) {
	tr.dump(t, jn(path, "vc_CropHeatRedux"), cm.crop_heat_redux)
	tr.dump(t, jn(path, "vc_TotalCropHeatImpact"), cm.total_crop_heat_impact)
	tr.dump(t, jn(path, "vc_DaysAfterBeginFlowering"), cm.days_after_begin_flowering)

	tr.dump(t, jn(path, "vc_LT50"), cm.lt50)
	tr.dump(t, jn(path, "vc_LT50M"), cm.lt50_m)
	tr.dump(t, jn(path, "vc_CropFrostRedux"), cm.crop_frost_redux)

	tr.dump(t, jn(path, "vc_DroughtImpactOnFertility"), cm.drought_impact_on_fertility)

	tr.dump(t, jn(path, "vc_CriticalNConcentration"), cm.critical_n_concentration)
	tr.dump(t, jn(path, "vc_TargetNConcentration"), cm.target_n_concentration)
	tr.dump(t, jn(path, "rootNRedux"), cm.root_n_redux)
	tr.dump(t, jn(path, "vc_CropNRedux"), cm.crop_n_redux)

	tr.dump(t, jn(path, "vc_AbovegroundBiomass"), cm.aboveground_biomass)
	tr.dump(t, jn(path, "vc_BelowgroundBiomass"), cm.belowground_biomass)
	tr.dump(t, jn(path, "vc_TotalBiomass"), cm.total_biomass)
	tr.dump(t, jn(path, "vc_OrganBiomass"), cm.organ_biomass)
	tr.dump(t, jn(path, "vc_OrganDeadBiomass"), cm.organ_dead_biomass)
	tr.dump(t, jn(path, "vc_OrganGreenBiomass"), cm.organ_green_biomass)
	tr.dump(t, jn(path, "vc_OrganGrowthIncrement"), cm.organ_growth_increment)
	tr.dump(t, jn(path, "vc_OrganSenescenceIncrement"), cm.organ_senescence_increment)
	tr.dump(t, jn(path, "vc_RootBiomass"), cm.root_biomass)
	tr.dump(t, jn(path, "vc_TotalBiomassNContent"), cm.total_biomass_n_content)
	tr.dump(t, jn(path, "vc_CropNDemand"), cm.crop_n_demand)

	tr.dump(t, jn(path, "vc_MaxRootingDepth"), cm.max_rooting_depth)
	tr.dump(t, jn(path, "vc_RootingDepth_m"), cm.rooting_depth_m)
	tr.dump(t, jn(path, "vc_RootingDepth"), cm.rooting_depth)
	tr.dump(t, jn(path, "vc_RootingZone"), cm.rooting_zone)
	tr.dump(t, jn(path, "vc_TotalRootLength"), cm.total_root_length)
	tr.dump(t, jn(path, "vc_RootDensity"), cm.root_density)
	tr.dump(t, jn(path, "vc_RootDiameter"), cm.root_diameter)
	tr.dump(t, jn(path, "vc_MaxNUptake"), cm.max_n_uptake)
	tr.dump(
		t,
		jn(path, "vc_CurrentTotalTemperatureSumRoot"),
		cm.current_total_temperature_sum_root,
	)

	tr.dump(t, jn(path, "vc_GrossPrimaryProduction"), cm.gross_primary_production)
	tr.dump(t, jn(path, "vc_NetPrimaryProduction"), cm.net_primary_production)
}

// checkpoint 3/4's day_step, extended with the checkpoint-5 functions in
// step()'s real order - see crop_module_biomass_ref_main.cpp's day_step.
day_step :: proc(
	cm: ^core.Crop_Module,
	mean_air_temperature, max_air_temperature, min_air_temperature: f64,
	global_radiation, sunshine_hours: f64,
	current_date: d.Date,
	frost_kill_on: bool,
	allocator := context.allocator,
) {
	base_daylength := cm.crop_params.cultivar_params.base_daylength
	critical_oxygen_content := cm.crop_params.species_params.critical_oxygen_content
	daylength_requirement := cm.crop_params.cultivar_params.daylength_requirement
	max_crop_height := cm.crop_params.cultivar_params.max_crop_height
	perennial := cm.crop_params.cultivar_params.perennial
	specific_leaf_area := cm.crop_params.cultivar_params.specific_leaf_area
	stage_kc_factor := cm.crop_params.cultivar_params.stage_kc_factor
	stage_temperature_sum := cm.crop_params.cultivar_params.stage_temperature_sum
	vernalisation_requirement := cm.crop_params.cultivar_params.vernalisation_requirement
	species_ps := &cm.crop_params.species_params

	julian_day := int(d.julian_day(current_date))

	cm.radiation = core.crop_radiation({f64(julian_day), cm.site_params.latitude, global_radiation, sunshine_hours})

	core.stress_oxygen_deficiency(&cm.stress, {critical_oxygen_content = critical_oxygen_content[cm.developmental_stage], developmental_stage = cm.developmental_stage, rooting_depth = cm.rooting_depth, time_under_anoxia_threshold = cm.mod_params.time_under_anoxia_threshold[:], time_step = cm.time_step, soil_layers = cm.soil_column.layers[:]})

	old_developmental_stage := cm.developmental_stage

	if !d.is_valid(cm.perennial_crop_dormancy_period_end_date) {
		if species_ps.dormancy_end_doy == 0 {
			cm.perennial_crop_dormancy_period_end_date = current_date
		} else {
			cm.perennial_crop_dormancy_period_end_date = d.add(
				d.make_date(
					1,
					1,
					u16(d.year(current_date)),
					false,
					false,
					d.DEFAULT_USE_LEAP_YEARS,
				),
				u64(species_ps.dormancy_end_doy - 1),
			)
		}
	}
	if !perennial || d.ge(current_date, cm.perennial_crop_dormancy_period_end_date) {
		{
			reset_perennial, irregular := core.phenology_development(
				&cm.phenology,
				&cm.crop_params,
				core.phenology_in(cm, mean_air_temperature, current_date, cm.transpiration_deficit, cm.crop_n_redux, cm.daylength_factor, cm.vernalisation_factor),
			)
			if irregular {cm.error_status = true; cm.error_message = "irregular developmental stage"}
			if reset_perennial {
				core.fc_update_crop_parameters_for_perennial(cm)
				core.phenology_reset_perennial(&cm.phenology, &cm.crop_params, cm.no_of_dev_stages, current_date)
			}
		}
	}

	if core.is_anthesis_day(cm, old_developmental_stage, cm.developmental_stage) {
		cm.anthesis_day = julian_day
	} else if core.is_maturity_day(cm, old_developmental_stage, cm.developmental_stage) {
		cm.maturity_day = julian_day
		cm.maturity_reached = true
	}

	cm.daylength_factor = core.crop_daylength_factor({daylength_requirement = daylength_requirement[cm.developmental_stage], base_daylength = base_daylength[cm.developmental_stage], effective_day_length = cm.effective_day_length, photoperiodic_daylength = cm.photoperiodic_daylength})

	cm.vernalisation_factor, cm.vernalisation_days = core.crop_vernalisation_factor({mean_air_temperature = mean_air_temperature, vernalisation_requirement = vernalisation_requirement[cm.developmental_stage], vernalisation_days = cm.vernalisation_days, time_step = cm.time_step, enable_fix = core.vernalisation_factor_fix_enabled(cm)})

	if cm.total_temperature_sum == 0.0 {
		cm.relative_total_development = 0.0
	} else {
		cm.relative_total_development = cm.current_total_temperature_sum / cm.total_temperature_sum
	}

	core.canopy_kc_factor(
		&cm.canopy,
		&cm.crop_params,
		{cm.developmental_stage, cm.current_temperature_sum[:], cm.site_params.bare_soil_kc_factor},
	)

	if cm.developmental_stage > 0 {
		max_crop_height := max_crop_height

		core.canopy_growth(
			&cm.canopy,
			&cm.crop_params,
			{
				developmental_stage = cm.developmental_stage,
				mean_air_temperature = mean_air_temperature,
				max_crop_height = max_crop_height,
				current_total_temperature_sum = cm.current_total_temperature_sum,
				current_temperature_sum = cm.current_temperature_sum[:],
				leaf_growth_increment = cm.organ_growth_increment[core.Organ_Leaf],
				leaf_senescence_increment = cm.organ_senescence_increment[core.Organ_Leaf],
				enable_t_response_leaf_expansion = cm.mod_params.__enable_T_response_leaf_expansion__,
				time_step = cm.time_step,
			},
		)

		core.photosynthesis_step(&cm.photosynthesis, &cm.o3, &cm.voc, &cm.crop_params, cm.mod_params, core.photosynthesis_in(cm, mean_air_temperature, max_air_temperature, min_air_temperature, ATM_CO2, ATM_O3, current_date, cm.transpiration_deficit, cm.crop_n_redux, cm.crop_frost_redux, cm.rooting_depth, cm.reference_evapotranspiration))

		core.stress_heat(&cm.stress, &cm.crop_params, {max_air_temperature, min_air_temperature, cm.current_total_temperature_sum})

		if frost_kill_on {
			core.stress_frost_kill(&cm.stress, &cm.crop_params, core.frost_kill_in(cm, max_air_temperature, min_air_temperature, core.snow_for_crop(g_soil_moisture)))
		}

		if cm.transpiration_deficit < 0.0 {cm.transpiration_deficit = 0.0} // the C++ clamp, now in the step
		core.stress_drought_impact_on_fertility(&cm.stress, &cm.crop_params, {cm.transpiration_deficit, cm.developmental_stage, cm.storage_organ})

		core.n_status_step(
			&cm.n_status,
			&cm.crop_params,
			{cm.aboveground_biomass, cm.belowground_biomass, cm.n_concentration_aboveground_biomass,
			 cm.n_concentration_root, cm.sim_params.nitrogen_response_on},
		)

		{
			// the three parts of the C++ fcCropDryMatter, as in crop_module_step
			dead_root_biomass, total_biomass_n_content, growth_cycle_ended := core.growth_step(
				&cm.growth,
				&cm.crop_params,
				core.growth_in(cm, cm.n_concentration_root),
			)
			cm.total_biomass_n_content = total_biomass_n_content
			if growth_cycle_ended {cm.growth_cycle_ended = true}
			dead_roots := core.root_system_step(
				&cm.roots,
				&cm.crop_params,
				{
					root_biomass = cm.organ_biomass[core.Organ_Root],
					dead_root_biomass = dead_root_biomass,
					n_concentration_root = cm.n_concentration_root,
					water_stress = cm.transpiration_deficit,
					developmental_stage = cm.developmental_stage,
					no_of_dev_stages = cm.no_of_dev_stages,
					mean_air_temperature = mean_air_temperature,
					soil_layers = cm.soil_column.layers[:],
					number_of_organic_layers = cm.soil_column.number_of_organic_layers,
					impenetrable_layer_depth = cm.site_params.impenetrable_layer_depth,
					max_effective_rooting_depth = cm.site_params.max_effective_rooting_depth,
					enable_pasw_root_penetration = cm.mod_params.__enable_PASW_root_penetration__,
					disable_daily_root_biomass_to_soil = cm.mod_params.__disable_daily_root_biomass_to_soil__,
				},
				allocator,
			)
			if len(dead_roots) > 0 {
				recording_add_organic_matter(dead_roots, cm.n_concentration_root)
			}
			core.growth_limit_n_demand(&cm.growth, &cm.crop_params, {cm.total_root_length, cm.current_total_temperature_sum, cm.total_temperature_sum, cm.time_step})
		}

		cm.gross_primary_production = core.fc_gross_primary_production(cm)
		cm.net_primary_production = core.fc_net_primary_production(cm, cm.total_respired)
	}

	cm.no_of_crop_steps += 1
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
		cpp.sim_params.layer_thickness,
		cpp.soil_organic_mod_params.max_mineralisation_depth,
		cpp.site_params.soil_parameters[:],
		a,
	)
	st := core.make_soil_temperature(
		&sc,
		cpp.soil_temperature_mod_params,
		cpp.env_params.time_step,
	)
	sm := core.make_soil_moisture(
		&sc,
		&cpp.site_params,
		cpp.soil_moisture_mod_params,
		&cpp.env_params,
		&cpp.crop_params,
		cpp.sim_params.layer_thickness,
		a,
	)
	g_soil_moisture = &sm

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
		&cpp.site_params,
		&cpp.crop_params,
		&cpp.sim_params,
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
	num_days_int := int(num_days)
	if num_days_int > 0 && num_days_int < n {
		n = num_days_int
	}

	frost_kill_on := cpp.sim_params.frost_kill_on

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

		groundwater_depth: f64 = (day % 40) < 15 ? 3.0 : 15.0
		et0 := -1.0 // climate-min.csv has no et0 column

		core.soil_temperature_step(
			&st,
			tmin,
			tmax,
			globrad,
			0.0,
			sm.snow_component.snow_depth,
			sm.frost_component.temperature_under_snow,
		)
		core.soil_moisture_step(
			&sm,
			core.crop_to_soil_of(nil, false),
			groundwater_depth,
			precip,
			tmax,
			tmin,
			(relhumid / 100.0),
			tavg,
			wind,
			cpp.env_params.wind_speed_height_m,
			globrad,
			julday,
			et0,
		)

		day_step(&cm, tavg, tmax, tmin, globrad, 0.0, current_date, frost_kill_on, a)

		tr.set_day(&t, day)
		dump_crop_module_biomass(&t, "cropModule", &cm)
		tr.write_line_f64(
			&t,
			"cropModule.recording.lastOrganicMatterTotal",
			g_last_organic_matter_total,
		)
		tr.write_line_f64(
			&t,
			"cropModule.recording.lastOrganicMatterNConc",
			g_last_organic_matter_nconc,
		)
		tr.write_line_int(
			&t,
			"cropModule.recording.organicMatterCallCount",
			g_organic_matter_call_count,
		)
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
			cpp.sim_params.layer_thickness,
			cpp.soil_organic_mod_params.max_mineralisation_depth,
			cpp.site_params.soil_parameters[:],
			a,
		)
		st2 := core.make_soil_temperature(
			&sc2,
			cpp.soil_temperature_mod_params,
			cpp.env_params.time_step,
		)
		sm2 := core.make_soil_moisture(
			&sc2,
			&cpp.site_params,
			cpp.soil_moisture_mod_params,
			&cpp.env_params,
			&cpp.crop_params,
			cpp.sim_params.layer_thickness,
			a,
		)
		g_soil_moisture = &sm2

		senescent_crop_params := wheat_crop_params
		senescent_crop_params.cultivar_params.organ_senescence_rate = make(
			[dynamic][dynamic]f64,
			len(wheat_crop_params.cultivar_params.organ_senescence_rate),
			a,
		)
		for row, i in wheat_crop_params.cultivar_params.organ_senescence_rate {
			senescent_crop_params.cultivar_params.organ_senescence_rate[i] = make(
				[dynamic]f64,
				len(row),
				a,
			)
			copy(senescent_crop_params.cultivar_params.organ_senescence_rate[i][:], row[:])
			if len(row) > 0 {
				senescent_crop_params.cultivar_params.organ_senescence_rate[i][0] = 0.01 // root organ
			}
		}

		g_last_organic_matter_total = 0
		g_last_organic_matter_nconc = 0
		g_organic_matter_call_count = 0

		cm_b := core.make_crop_module(
			&sc2,
			&senescent_crop_params,
			&wheat_residue_params,
			&cpp.site_params,
			&cpp.crop_params,
			&cpp.sim_params,
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

			groundwater_depth: f64 = (day % 40) < 15 ? 3.0 : 15.0
			et0 := -1.0

			core.soil_temperature_step(
				&st2,
				tmin,
				tmax,
				globrad,
				0.0,
				sm2.snow_component.snow_depth,
				sm2.frost_component.temperature_under_snow,
			)
			core.soil_moisture_step(
				&sm2,
				core.crop_to_soil_of(nil, false),
				groundwater_depth,
				precip,
				tmax,
				tmin,
				(relhumid / 100.0),
				tavg,
				wind,
				cpp.env_params.wind_speed_height_m,
				globrad,
				julday,
				et0,
			)

			day_step(&cm_b, tavg, tmax, tmin, globrad, 0.0, current_date, frost_kill_on, a)

			tr.set_day(&t, day)
			tr.dump(&t, "cropModuleB.vc_OrganDeadBiomass[0]", cm_b.organ_dead_biomass[0])
			tr.write_line_f64(
				&t,
				"cropModuleB.recording.lastOrganicMatterTotal",
				g_last_organic_matter_total,
			)
			tr.write_line_f64(
				&t,
				"cropModuleB.recording.lastOrganicMatterNConc",
				g_last_organic_matter_nconc,
			)
			tr.write_line_int(
				&t,
				"cropModuleB.recording.organicMatterCallCount",
				g_organic_matter_call_count,
			)
			free_all(context.temp_allocator)
		}
	}
}
