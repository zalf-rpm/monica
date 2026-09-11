// Odin side of the phase 6 checkpoint 2 prerequisite (cropmodule:: yield/
// N-content getters + applyCutting) differential test - see
// odin/tests/cpp_ref/crop_module_yield_ref_main.cpp's header comment.
// Run odin/tests/cpp_ref/run_crop_module_yield.sh to build both and diff them.
package crop_module_yield_ref

import core "../../monica/core"
import p "../../monica/params"
import mrun "../../monica/run"
import tr "../../monica/trace"
import clim "../../support/climate"
import jx "../../support/jsonx"
import tl "../../support/tools"
import "core:fmt"
import "core:os"
import "core:slice"
import "core:strconv"
import "core:strings"

ATM_CO2 :: 380.0
ATM_O3 :: 60.0

g_soil_moisture: ^core.Soil_Moisture
g_soil_organic: ^core.Soil_Organic
g_residue_params: ^p.Organic_Matter_Parameters

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

no_fire_event :: proc(event: string) {}

dump_yield_getters :: proc(t: ^tr.Tracer, path: string, cm: ^core.Crop_Module, a: jx.Allocator) {
	ids := core.organ_ids_for_primary_yield(cm, a)
	keys := make([dynamic]int, 0, len(ids), a)
	for k in ids {
		append(&keys, k)
	}
	slice.sort(keys[:])
	for id, i in keys {
		tr.dump(t, fmt.tprintf("%s.organIdsForPrimaryYield[%d]", path, i), id)
	}

	tr.dump(
		t,
		strings.concatenate({path, ".getPrimaryCropYield"}),
		core.get_primary_crop_yield(cm),
	)
	tr.dump(
		t,
		strings.concatenate({path, ".getSecondaryCropYield"}),
		core.get_secondary_crop_yield(cm),
	)
	tr.dump(
		t,
		strings.concatenate({path, ".getResidueBiomass(true,-1)"}),
		core.get_residue_biomass(cm, true, -1),
	)
	tr.dump(
		t,
		strings.concatenate({path, ".getResidueBiomass(false,-1)"}),
		core.get_residue_biomass(cm, false, -1),
	)
	tr.dump(
		t,
		strings.concatenate({path, ".getResidueBiomass(true,500)"}),
		core.get_residue_biomass(cm, true, 500.0),
	)
	tr.dump(
		t,
		strings.concatenate({path, ".getResiduesNConcentration(-1)"}),
		core.get_residues_n_concentration(cm, -1),
	)
	tr.dump(
		t,
		strings.concatenate({path, ".getResiduesNConcentration(500)"}),
		core.get_residues_n_concentration(cm, 500.0),
	)
	tr.dump(
		t,
		strings.concatenate({path, ".getPrimaryYieldNConcentration(-1)"}),
		core.get_primary_yield_n_concentration(cm, -1),
	)
	tr.dump(
		t,
		strings.concatenate({path, ".getResiduesNContent(true,-1,-1)"}),
		core.get_residues_n_content(cm, true, -1, -1),
	)
	tr.dump(
		t,
		strings.concatenate({path, ".getPrimaryYieldNContent(-1)"}),
		core.get_primary_yield_n_content(cm, -1),
	)
	tr.dump(
		t,
		strings.concatenate({path, ".getSecondaryYieldNContent(-1,-1)"}),
		core.get_secondary_yield_n_content(cm, -1, -1),
	)
	tr.dump(
		t,
		strings.concatenate({path, ".getAbovegroundBiomassNContent"}),
		core.get_aboveground_biomass_n_content(cm),
	)
}

dump_cutting_state :: proc(t: ^tr.Tracer, path: string, cm: ^core.Crop_Module) {
	tr.dump(t, strings.concatenate({path, ".vc_AbovegroundBiomass"}), cm.aboveground_biomass)
	tr.dump(t, strings.concatenate({path, ".vc_TotalBiomassNContent"}), cm.total_biomass_n_content)
	tr.dump(t, strings.concatenate({path, ".vc_OrganBiomass"}), cm.organ_biomass)
	tr.dump(t, strings.concatenate({path, ".vc_OrganDeadBiomass"}), cm.organ_dead_biomass)
	tr.dump(t, strings.concatenate({path, ".vc_OrganGreenBiomass"}), cm.organ_green_biomass)
	tr.dump(t, strings.concatenate({path, ".vc_LeafAreaIndex"}), cm.leaf_area_index)
	tr.dump(t, strings.concatenate({path, ".vc_DevelopmentalStage"}), cm.developmental_stage)
	tr.dump(t, strings.concatenate({path, ".vc_CuttingDelayDays"}), cm.cutting_delay_days)
	tr.dump(t, strings.concatenate({path, ".vc_exportedCutBiomass"}), cm.exported_cut_biomass)
	tr.dump(
		t,
		strings.concatenate({path, ".vc_sumExportedCutBiomass"}),
		cm.sum_exported_cut_biomass,
	)
	tr.dump(t, strings.concatenate({path, ".vc_residueCutBiomass"}), cm.residue_cut_biomass)
	tr.dump(t, strings.concatenate({path, ".vc_sumResidueCutBiomass"}), cm.sum_residue_cut_biomass)
	tr.dump(
		t,
		strings.concatenate({path, ".cropParams.cultivarParams.pc_MaxAssimilationRate"}),
		cm.crop_params.cultivarParams.pc_MaxAssimilationRate,
	)
}

load :: proc(dir, name: string, a: jx.Allocator) -> jx.Value {
	path := strings.concatenate({dir, "/", name}, a)
	r := jx.read_and_parse_json_file(path, a)
	if tl.failure(r.errs) {
		tl.print_possible_errors(r.errs)
		return jx.Value{}
	}
	return r.result
}

main :: proc() {
	args := os.args
	if len(args) < 4 {
		fmt.eprintln("usage: crop_module_yield_ref <pathToSimJson> <pathToClimateCsv> <numDays>")
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

	model := core.make_monica_model(&cpp, a)

	monica_parameters_dir := tl.fix_system_separator(
		tl.replace_env_vars("${MONICA_PARAMETERS}", a),
		a,
	)

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

	g_soil_moisture = &model.soil_moisture
	g_soil_organic = &model.soil_organic

	cm := core.make_crop_module(
		&model.soil_column,
		&wheat_crop_params,
		&wheat_residue_params,
		&model.site_ps,
		&model.crop_ps,
		&model.sim_ps,
		no_fire_event,
		real_add_organic_matter,
		real_get_snow_depth,
		nil,
		a,
	)
	model.current_crop_module = &cm
	g_residue_params = &cm.residue_params.base

	model.soil_moisture.crop_module = &cm
	model.soil_organic.crop_module = &cm
	model.soil_transport.cropModule = &cm

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

	// silent growth run
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
		et0 := -1.0

		core.soil_temperature_step(
			&model.soil_temperature,
			tmin,
			tmax,
			globrad,
			cm.soil_coverage,
			model.soil_moisture.snow_component.snow_depth,
			model.soil_moisture.frost_component.temperature_under_snow,
		)
		core.soil_moisture_step(
			&model.soil_moisture,
			vs_GroundwaterDepth,
			precip,
			tmax,
			tmin,
			(relhumid / 100.0),
			tavg,
			wind,
			model.env_ps.p_WindSpeedHeight,
			globrad,
			julday,
			et0,
			model.sim_ps.dualKcMethod,
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
			model.env_ps.p_WindSpeedHeight,
			ATM_CO2,
			ATM_O3,
			precip,
			-1.0,
			a,
		)
		core.soil_organic_step(&model.soil_organic, tavg, precip, wind)
		core.soil_transport_step(&model.soil_transport)
		free_all(context.temp_allocator)
	}

	t := tr.make_tracer(os.to_stream(os.stdout), a)
	defer tr.destroy_tracer(&t)

	// scenario 0: read-only yield getters
	tr.set_day(&t, 0)
	dump_yield_getters(&t, "y", &cm, a)
	dump_cutting_state(&t, "cropModule", &cm)

	// scenario 1: applyCutting, percentage unit, cut, organ 1 (leaf)
	tr.set_day(&t, 1)
	{
		organs := make(map[int]core.Cutting_Value, 0, a)
		organs[1] = core.Cutting_Value {
			value       = 0.3,
			unit        = .Percentage,
			cut_or_left = .Cut,
		}
		exports := make(map[int]f64, 0, a)
		exports[1] = 0.6
		core.apply_cutting(&cm, organs, exports, 0.9, a)
	}
	dump_cutting_state(&t, "cropModule", &cm)

	// scenario 2: applyCutting, biomass unit, left, organ 2 (shoot)
	tr.set_day(&t, 2)
	{
		organs := make(map[int]core.Cutting_Value, 0, a)
		organs[2] = core.Cutting_Value {
			value       = 200.0,
			unit        = .Biomass,
			cut_or_left = .Left,
		}
		exports := make(map[int]f64, 0, a)
		exports[2] = 0.5
		core.apply_cutting(&cm, organs, exports, 1.0, a)
	}
	dump_cutting_state(&t, "cropModule", &cm)

	// scenario 3: applyCutting, LAI unit, organ 1 (leaf)
	tr.set_day(&t, 3)
	{
		organs := make(map[int]core.Cutting_Value, 0, a)
		organs[1] = core.Cutting_Value {
			value       = 1.0,
			unit        = .LAI,
			cut_or_left = .Left,
		}
		exports := make(map[int]f64, 0, a)
		exports[1] = 0.4
		core.apply_cutting(&cm, organs, exports, 0.95, a)
	}
	dump_cutting_state(&t, "cropModule", &cm)

	// scenario 4: empty organs map -> pc_OrganIdsForCutting auto-fill branch
	tr.set_day(&t, 4)
	{
		organs := make(map[int]core.Cutting_Value, 0, a)
		exports := make(map[int]f64, 0, a)
		core.apply_cutting(&cm, organs, exports, 1.0, a)
		tr.dump(&t, "cropModule.autoFilledOrgans.size", len(organs))
	}
	dump_cutting_state(&t, "cropModule", &cm)
}
