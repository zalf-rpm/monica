// Odin side of the phase 6 checkpoint 5 (cultivation_method.odin)
// differential test - see
// odin/tests/cpp_ref/cultivation_method_ref_main.cpp's header comment.
// Run odin/tests/cpp_ref/run_cultivation_method.sh to build both and diff them.
package cultivation_method_ref

import core "../../monica/core"
import p "../../monica/params"
import run "../../monica/run"
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

ATM_CO2 :: 380.0
ATM_O3 :: 60.0

dump_cm_basic :: proc(t: ^tr.Tracer, path: string, cm: ^run.Cultivation_Method) {
	tr.dump(t, strings.concatenate({path, ".customId"}), cm.customId)
	tr.dump(t, strings.concatenate({path, ".name"}), cm.name)
	tr.dump(t, strings.concatenate({path, ".canBeSkipped"}), cm.canBeSkipped)
	tr.dump(t, strings.concatenate({path, ".isCoverCrop"}), cm.isCoverCrop)
	tr.dump(t, strings.concatenate({path, ".repeat"}), cm.repeat)
	tr.dump(t, strings.concatenate({path, ".allWorksteps.size"}), len(cm.allWorksteps))
}

dump_model_bits :: proc(t: ^tr.Tracer, path: string, model: ^core.Monica_Model) {
	tr.dump(t, strings.concatenate({path, ".sumFertiliser"}), model.sumFertiliser)
	tr.dump(t, strings.concatenate({path, ".dailySumFertiliser"}), model.dailySumFertiliser)
	tr.dump(t, strings.concatenate({path, ".sumOrgFertiliser"}), model.sumOrgFertiliser)
	tr.dump(
		t,
		strings.concatenate({path, ".dailySumIrrigationWater"}),
		model.dailySumIrrigationWater,
	)
	tr.dump(
		t,
		strings.concatenate({path, ".cultivationMethodCount"}),
		model.cultivationMethodCount,
	)
	tr.dump(t, strings.concatenate({path, ".clearCropUponNextDay"}), model.clearCropUponNextDay)
	tr.dump(
		t,
		strings.concatenate({path, ".currentCropModule"}),
		model.currentCropModule != nil ? "<ptr:set>" : "<ptr:nil>",
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
		fmt.eprintln("usage: cultivation_method_ref <pathToSimJson> <pathToClimateCsv> <numDays>")
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

	species_json := load(monica_parameters_dir, "crops/wheat.json", a)
	cultivar_json := load(monica_parameters_dir, "crops/wheat/winter-wheat.json", a)
	residue_json := load(monica_parameters_dir, "crop-residues/wheat.json", a)
	an_json := load(monica_parameters_dir, "mineral-fertilisers/AN.json", a)
	cam_json := load(monica_parameters_dir, "organic-fertilisers/CAM.json", a)

	crop_json := jx.obj(
		a,
		{"cropParams", jx.obj(a, {"species", species_json}, {"cultivar", cultivar_json})},
		{"residueParams", residue_json},
	)

	cm_json := jx.obj(
		a,
		{"customId", jx.i(1)},
		{"name", jx.sl("WW")},
		{
			"worksteps",
			jx.arr(
				a,
				jx.obj(
					a,
					{"type", jx.sl("AutomaticIrrigation")},
					{"irrigateCrop", jx.b(true)},
					{"startStage", jx.i(4)},
					{"endStage", jx.i(4)},
					{
						"parameters",
						jx.obj(
							a,
							{
								"irrigationParameters",
								jx.obj(
									a,
									{"nitrateConcentration", jx.arr(a, jx.i(0), jx.sl("mg dm-3"))},
								),
							},
							{"amount", jx.arr(a, jx.i(17), jx.sl("mm"))},
							{"trigger_if_nFC_below_%", jx.arr(a, jx.i(90), jx.sl("%"))},
							{"calc_nFC_until_depth_m", jx.arr(a, jx.f(0.3), jx.sl("m"))},
						),
					},
				),
				jx.obj(
					a,
					{"date", jx.sl("0000-09-22")},
					{"type", jx.sl("Sowing")},
					{"crop", crop_json},
				),
				jx.obj(
					a,
					{"type", jx.sl("NDemandFertilization")},
					{"date", jx.sl("0001-03-15")},
					{"N-demand", jx.arr(a, jx.f(40.0), jx.sl("kg"))},
					{"depth", jx.arr(a, jx.f(0.3), jx.sl("m"))},
					{"partition", an_json},
				),
				jx.obj(
					a,
					{"type", jx.sl("NDemandFertilization")},
					{"date", jx.sl("0001-04-15")},
					{"N-demand", jx.arr(a, jx.f(80.0), jx.sl("kg"))},
					{"depth", jx.arr(a, jx.f(0.3), jx.sl("m"))},
					{"partition", an_json},
				),
				jx.obj(
					a,
					{"type", jx.sl("NDemandFertilization")},
					{"date", jx.sl("0001-05-15")},
					{"N-demand", jx.arr(a, jx.f(40.0), jx.sl("kg"))},
					{"depth", jx.arr(a, jx.f(0.3), jx.sl("m"))},
					{"partition", an_json},
				),
				jx.obj(
					a,
					{"type", jx.sl("AutomaticHarvest")},
					{"latest-date", jx.sl("0001-09-05")},
					{"min-%-asw", jx.i(10)},
					{"max-%-asw", jx.f(99.0)},
					{"max-3d-precip-sum", jx.i(2)},
					{"max-curr-day-precip", jx.f(0.1)},
					{"harvest-time", jx.sl("maturity")},
					{"incorporateIntoLayerNo", jx.i(2)},
				),
				jx.obj(
					a,
					{"type", jx.sl("OrganicFertilization")},
					{"days", jx.i(1)},
					{"after", jx.sl("Harvest")},
					{"amount", jx.arr(a, jx.i(15000), jx.sl("kg N"))},
					{"parameters", cam_json},
					{"incorporation", jx.b(true)},
					{"incorporateIntoLayerNo", jx.i(2)},
				),
			),
		},
	)

	t := tr.make_tracer(os.to_stream(os.stdout), a)
	defer tr.destroy_tracer(&t)

	// scenario 0: make_cultivation_method()
	tr.set_day(&t, 0)
	cm := run.make_cultivation_method(cm_json, a)
	dump_cm_basic(&t, "cm", &cm)

	// scenario 1: reinit before season start
	tr.set_day(&t, 1)
	season_start := d.from_iso_date_string("2020-09-22")
	_ = run.cultivation_method_reinit(&cm, season_start)
	tr.dump(&t, "cm.allAbsWorksteps.size", len(cm.allAbsWorksteps))
	tr.dump(&t, "cm.unfinishedDynamicWorksteps.size", len(cm.unfinishedDynamicWorksteps))
	tr.dump(&t, "cm.startDate", d.to_iso_date_string(run.cultivation_method_start_date(&cm, a)))
	tr.dump(
		&t,
		"cm.absStartDate",
		d.to_iso_date_string(run.cultivation_method_abs_start_date(&cm, true, a)),
	)
	tr.dump(
		&t,
		"cm.absLatestSowingDate",
		d.to_iso_date_string(run.cultivation_method_abs_latest_sowing_date(&cm)),
	)
	tr.dump(&t, "cm.endDate", d.to_iso_date_string(run.cultivation_method_end_date(&cm, a)))
	tr.dump(&t, "cm.absEndDate", d.to_iso_date_string(run.cultivation_method_abs_end_date(&cm, a)))
	tr.dump(
		&t,
		"cm.areOnlyAbsoluteWorksteps",
		run.cultivation_method_are_only_absolute_worksteps(&cm),
	)
	tr.dump(
		&t,
		"cm.allDynamicWorkstepsFinished",
		run.cultivation_method_all_dynamic_worksteps_finished(&cm),
	)
	tr.dump(&t, "cm.staticWorksteps.size", len(run.cultivation_method_static_worksteps(&cm, a)))
	tr.dump(
		&t,
		"cm.allDynamicWorksteps.size",
		len(run.cultivation_method_all_dynamic_worksteps(&cm, a)),
	)

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

	harvested := false
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
		model.currentStepDate = current_date

		daily_map := make(map[clim.ACD]f64, 0, a)
		daily_map[.tmin] = tmin
		daily_map[.tmax] = tmax
		daily_map[.tavg] = tavg
		daily_map[.wind] = wind
		daily_map[.globrad] = globrad
		daily_map[.precip] = precip
		daily_map[.relhumid] = relhumid
		append(&model.climateData, daily_map)

		vs_GroundwaterDepth: f64 = (day % 40) < 15 ? 3.0 : 15.0
		et0 := -1.0

		core.soil_temperature_step(
			&model.soilTemperature,
			tmin,
			tmax,
			globrad,
			model.currentCropModule != nil ? model.currentCropModule.soil_coverage : 0.0,
			model.soilMoisture.snow_component.vm_SnowDepth,
			model.soilMoisture.frost_component.temperature_under_snow,
		)
		core.soil_moisture_step(
			&model.soilMoisture,
			vs_GroundwaterDepth,
			precip,
			tmax,
			tmin,
			(relhumid / 100.0),
			tavg,
			wind,
			model.envPs.p_WindSpeedHeight,
			globrad,
			julday,
			et0,
			model.simPs.dualKcMethod,
		)

		run.cultivation_method_apply_at_date(&cm, current_date, model, a)
		run.cultivation_method_apply(&cm, model, true)

		if model.currentCropModule != nil {
			core.crop_module_step(
				model.currentCropModule,
				tavg,
				tmax,
				tmin,
				globrad,
				0.0,
				current_date,
				(relhumid / 100.0),
				wind,
				model.envPs.p_WindSpeedHeight,
				ATM_CO2,
				ATM_O3,
				precip,
				-1.0,
				a,
			)
		}

		run.cultivation_method_apply(&cm, model, false)
		if !harvested {
			if _, ok := model.currentEvents["AutomaticHarvest"]; ok {
				harvested = true
				tr.set_day(&t, 1000 + day)
				dump_model_bits(&t, "model", model)
				tr.dump(&t, "harvestedOnDate", d.to_iso_date_string(current_date))
			}
		}

		core.soil_organic_step(&model.soilOrganic, tavg, precip, wind)
		core.soil_transport_step(&model.soilTransport)

		if day % 60 == 0 || day == n - 1 {
			tr.set_day(&t, day)
			dump_model_bits(&t, "model", model)
		}
		free_all(context.temp_allocator)
	}

	tr.set_day(&t, 9000)
	dump_model_bits(&t, "modelFinal", model)
	tr.dump(&t, "harvested", harvested)
	tr.dump(&t, "cm.unfinishedDynamicWorksteps.sizeFinal", len(cm.unfinishedDynamicWorksteps))
}
