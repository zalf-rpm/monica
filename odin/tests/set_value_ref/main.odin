// Odin side of the SetValue workstep differential test - mirrors
// odin/tests/cpp_ref/set_value_ref_main.cpp exactly. Run via
// odin/tests/cpp_ref/run_set_value.sh.
package set_value_ref

import core "../../monica/core"
import p "../../monica/params"
import run "../../monica/run"
import tr "../../monica/trace"
import clim "../../support/climate"
import d "../../support/date"
import jx "../../support/jsonx"
import tl "../../support/tools"
import "core:fmt"
import "core:os"
import "core:strconv"
import "core:strings"

ATM_CO2 :: 380.0
ATM_O3 :: 60.0

load :: proc(dir, name: string, a: jx.Allocator) -> jx.Value {
	path := strings.concatenate({dir, "/", name}, a)
	r := jx.read_and_parse_json_file(path, a)
	if tl.failure(r.errs) {
		tl.print_possible_errors(r.errs)
		return jx.Value{}
	}
	return r.result
}

dump_state :: proc(t: ^tr.Tracer, path: string, model: ^core.Monica_Model) {
	cm := model.currentCropModule
	tr.dump(t, strings.concatenate({path, ".vc_DevelopmentalStage"}), cm.developmental_stage)
	tr.dump(
		t,
		strings.concatenate({path, ".vc_CurrentTotalTemperatureSum"}),
		cm.current_total_temperature_sum,
	)
	tr.dump(t, strings.concatenate({path, ".vc_AbovegroundBiomass"}), cm.aboveground_biomass)
	for i in 0 ..< 3 {
		tr.dump(
			t,
			fmt.tprintf("%s.soilColumn.vs_SoilMoisture_m3[%d]", path, i),
			model.soilColumn.layers[i].soil_moisture_m3,
		)
	}
	_, hasSetValue := model.currentEvents["SetValue"]
	tr.dump(t, strings.concatenate({path, ".currentEvents.hasSetValue"}), hasSetValue)
}

main :: proc() {
	args := os.args
	if len(args) < 4 {
		fmt.eprintln("usage: set_value_ref <pathToSimJson> <pathToClimateCsv> <numDays>")
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

	env := run.create_env_json_from_json_objects(cropr.result, siter.result, sim_v, a)
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

	species_json := load(monica_parameters_dir, "crops/wheat.json", a)
	cultivar_json := load(monica_parameters_dir, "crops/wheat/winter-wheat.json", a)
	residue_json := load(monica_parameters_dir, "crop-residues/wheat.json", a)

	wheatCropParams := p.make_crop_parameters()
	_ = p.crop_parameters_merge_sj_cj(&wheatCropParams, species_json, cultivar_json)

	wheatResidueParams: p.Crop_Residue_Parameters
	_ = p.crop_residue_parameters_merge(&wheatResidueParams, residue_json)

	cm := new(core.Crop_Module, a)
	cm^ = core.make_crop_module(
		&model.soilColumn,
		&wheatCropParams,
		&wheatResidueParams,
		&model.sitePs,
		&model.cropPs,
		&model.simPs,
		core.monica_model_fire_event_cb,
		core.monica_model_add_organic_matter_cb,
		core.monica_model_get_snow_depth_cb,
		nil,
		a,
	)
	model.currentCropModule = cm

	model.soilMoisture.crop_module = cm
	model.soilOrganic.crop_module = cm
	model.soilTransport.cropModule = cm

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

	// silent growth run - only the final state matters for this oracle
	for day in 0 ..< n {
		tmin := clim.data_accessor_data_for_timestep(&da, .tmin, day)
		tmax := clim.data_accessor_data_for_timestep(&da, .tmax, day)
		tavg := clim.data_accessor_data_for_timestep(&da, .tavg, day)
		wind := clim.data_accessor_data_for_timestep(&da, .wind, day)
		globrad := clim.data_accessor_data_for_timestep(&da, .globrad, day)
		precip := clim.data_accessor_data_for_timestep(&da, .precip, day)
		relhumid := clim.data_accessor_data_for_timestep(&da, .relhumid, day)
		current_date := clim.data_accessor_date_for_step(&da, day)

		vs_GroundwaterDepth := day % 40 < 15 ? 3.0 : 15.0
		et0 := -1.0

		// C++'s soiltemperature::step/soilmoisture::step read soil coverage/snow
		// state through SoilTemperature's `monica` back-pointer; this port makes
		// them explicit parameters instead (see monica_model.odin's
		// monica_model_general_step for the production call this mirrors).
		soil_coverage :=
			model.currentCropModule != nil ? model.currentCropModule.soil_coverage : 0.0
		core.soil_temperature_step(
			&model.soilTemperature,
			tmin,
			tmax,
			globrad,
			soil_coverage,
			model.soilMoisture.snow_component.snow_depth,
			model.soilMoisture.frost_component.temperature_under_snow,
		)
		core.soil_moisture_step(
			&model.soilMoisture,
			vs_GroundwaterDepth,
			precip,
			tmax,
			tmin,
			relhumid / 100.0,
			tavg,
			wind,
			model.envPs.p_WindSpeedHeight,
			globrad,
			clim.data_accessor_julian_day_for_step(&da, day),
			et0,
			model.simPs.dualKcMethod,
		)
		core.crop_module_step(
			cm,
			tavg,
			tmax,
			tmin,
			globrad,
			0.0,
			current_date,
			relhumid / 100.0,
			wind,
			model.envPs.p_WindSpeedHeight,
			ATM_CO2,
			ATM_O3,
			precip,
			-1.0,
		)
		core.soil_organic_step(&model.soilOrganic, tavg, precip, wind)
		core.soil_transport_step(&model.soilTransport)
	}

	t := tr.make_tracer(os.to_stream(os.stdout), a)
	defer tr.destroy_tracer(&t)

	// scenario 0: constant scalar -> Stage setf (core.set_stage)
	tr.set_day(&t, 0)
	{
		j := jx.obj(a, {"var", jx.sl("Stage")}, {"value", jx.i(3)})
		ws := run.make_set_value_workstep(j, a)
		run.workstep_apply(ws, model)
	}
	dump_state(&t, "s", model)

	// scenario 1: constant scalar broadcast across a layer range -> Mois setf
	tr.set_day(&t, 1)
	{
		j := jx.obj(
			a,
			{"var", jx.arr(a, jx.sl("Mois"), jx.arr(a, jx.i(1), jx.i(3)))},
			{"value", jx.f(0.28)},
		)
		ws := run.make_set_value_workstep(j, a)
		run.workstep_apply(ws, model)
	}
	dump_state(&t, "s", model)

	// scenario 2: oid-reference value (re-evaluated live at apply time),
	// targeting a single layer
	tr.set_day(&t, 2)
	{
		j := jx.obj(
			a,
			{"var", jx.arr(a, jx.sl("Mois"), jx.arr(a, jx.i(2), jx.i(2)))},
			{"value", jx.arr(a, jx.sl("AbBiom"))},
		)
		ws := run.make_set_value_workstep(j, a)
		run.workstep_apply(ws, model)
	}
	dump_state(&t, "s", model)
}
