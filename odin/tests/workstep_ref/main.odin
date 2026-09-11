// Odin side of the phase 6 checkpoint 2 (workstep.odin + all 12 concrete
// worksteps) differential test - see
// odin/tests/cpp_ref/workstep_ref_main.cpp's header comment for the full
// scenario list.
// Run odin/tests/cpp_ref/run_workstep.sh to build both and diff them.
package workstep_ref

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

dump_ws_common :: proc(t: ^tr.Tracer, path: string, ws: ^run.Workstep) {
	tr.dump(t, strings.concatenate({path, ".date.isValid"}), d.is_valid(ws.date))
	tr.dump(t, strings.concatenate({path, ".date.toIsoDateString"}), d.to_iso_date_string(ws.date))
	tr.dump(t, strings.concatenate({path, ".applyNoOfDaysAfterEvent"}), ws.applyNoOfDaysAfterEvent)
	tr.dump(t, strings.concatenate({path, ".afterEvent"}), ws.afterEvent)
	tr.dump(t, strings.concatenate({path, ".daysAfterEventCount"}), ws.daysAfterEventCount)
	tr.dump(
		t,
		strings.concatenate({path, ".daysAfterEventCountActivated"}),
		ws.daysAfterEventCountActivated,
	)
	tr.dump(t, strings.concatenate({path, ".isActive"}), ws.isActive)
	tr.dump(t, strings.concatenate({path, ".runAtStartOfDay"}), ws.runAtStartOfDay)
	tr.dump(t, strings.concatenate({path, ".errors.errors.size"}), len(ws.errors.errors))
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
		fmt.eprintln("usage: workstep_ref <pathToSimJson> <pathToClimateCsv> <numDays>")
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

	// --- the real crop-min.json rotation, "include-from-file"/"ref" pre-resolved ---
	ai_json := jx.obj(
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
					jx.obj(a, {"nitrateConcentration", jx.arr(a, jx.i(0), jx.sl("mg dm-3"))}),
				},
				{"amount", jx.arr(a, jx.i(17), jx.sl("mm"))},
				{"trigger_if_nFC_below_%", jx.arr(a, jx.i(90), jx.sl("%"))},
				{"calc_nFC_until_depth_m", jx.arr(a, jx.f(0.3), jx.sl("m"))},
			),
		},
	)
	sowing_json := jx.obj(
		a,
		{"date", jx.sl("0000-09-22")},
		{"type", jx.sl("Sowing")},
		{"crop", crop_json},
	)
	nd1_json := jx.obj(
		a,
		{"type", jx.sl("NDemandFertilization")},
		{"date", jx.sl("0001-03-15")},
		{"N-demand", jx.arr(a, jx.f(40.0), jx.sl("kg"))},
		{"depth", jx.arr(a, jx.f(0.3), jx.sl("m"))},
		{"partition", an_json},
	)
	nd2_json := jx.obj(
		a,
		{"type", jx.sl("NDemandFertilization")},
		{"date", jx.sl("0001-04-15")},
		{"N-demand", jx.arr(a, jx.f(80.0), jx.sl("kg"))},
		{"depth", jx.arr(a, jx.f(0.3), jx.sl("m"))},
		{"partition", an_json},
	)
	nd3_json := jx.obj(
		a,
		{"type", jx.sl("NDemandFertilization")},
		{"date", jx.sl("0001-05-15")},
		{"N-demand", jx.arr(a, jx.f(40.0), jx.sl("kg"))},
		{"depth", jx.arr(a, jx.f(0.3), jx.sl("m"))},
		{"partition", an_json},
	)
	ah_json := jx.obj(
		a,
		{"type", jx.sl("AutomaticHarvest")},
		{"latest-date", jx.sl("0001-09-05")},
		{"min-%-asw", jx.i(10)},
		{"max-%-asw", jx.f(99.0)},
		{"max-3d-precip-sum", jx.i(2)},
		{"max-curr-day-precip", jx.f(0.1)},
		{"harvest-time", jx.sl("maturity")},
		{"incorporateIntoLayerNo", jx.i(2)},
	)
	of_json := jx.obj(
		a,
		{"type", jx.sl("OrganicFertilization")},
		{"days", jx.i(1)},
		{"after", jx.sl("Harvest")},
		{"amount", jx.arr(a, jx.i(15000), jx.sl("kg N"))},
		{"parameters", cam_json},
		{"incorporation", jx.b(true)},
		{"incorporateIntoLayerNo", jx.i(2)},
	)

	// --- synthetic JSON for the 5 types crop-min.json doesn't use ---
	transplant_json := jx.obj(
		a,
		{"date", jx.sl("0000-05-01")},
		{"type", jx.sl("Transplant")},
		{"crop", crop_json},
		{"initialStage", jx.i(3)},
		{"initialTemperatureSum", jx.f(120.0)},
		{"initialLAI", jx.f(0.3)},
		{"postTransplantDelay", jx.i(5)},
	)
	auto_sowing_json := jx.obj(
		a,
		{"type", jx.sl("AutomaticSowing")},
		{"crop", crop_json},
		{"earliest-date", jx.sl("0000-09-01")},
		{"latest-date", jx.sl("0000-10-15")},
		{"min-temp", jx.f(5.0)},
		{"days-in-temp-window", jx.i(5)},
		{"temp-sum-above-base-temp", jx.f(50.0)},
		{"base-temp", jx.f(0.0)},
	)
	harvest_json := jx.obj(
		a,
		{"type", jx.sl("Harvest")},
		{"exported", jx.b(true)},
		{"incorporateIntoLayerNo", jx.i(1)},
	)
	cutting_json := jx.obj(
		a,
		{"type", jx.sl("Cutting")},
		{"organs", jx.obj(a, {"leaf", jx.arr(a, jx.i(40), jx.sl("%"), jx.sl("cut"))})},
		{"export", jx.obj(a, {"leaf", jx.i(60)})},
		{"cut-max-assimilation-rate", jx.i(90)},
	)
	mineral_fert_json := jx.obj(
		a,
		{"type", jx.sl("MineralFertilization")},
		{"amount", jx.f(50.0)},
		{"partition", an_json},
	)
	tillage_json := jx.obj(a, {"type", jx.sl("Tillage")}, {"depth", jx.f(0.25)})
	irrigation_json := jx.obj(
		a,
		{"type", jx.sl("Irrigation")},
		{"amount", jx.f(20.0)},
		{"parameters", jx.obj(a, {"nitrateConcentration", jx.f(5.0)})},
	)

	// scenario 0: merge()/make_workstep() for all 12 types
	t := tr.make_tracer(os.to_stream(os.stdout), a)
	defer tr.destroy_tracer(&t)
	tr.set_day(&t, 0)

	ws_ai := run.make_workstep(ai_json, a)
	ws_sow := run.make_workstep(sowing_json, a)
	ws_nd1 := run.make_workstep(nd1_json, a)
	ws_nd2 := run.make_workstep(nd2_json, a)
	ws_nd3 := run.make_workstep(nd3_json, a)
	ws_ah := run.make_workstep(ah_json, a)
	ws_of := run.make_workstep(of_json, a)
	ws_transplant := run.make_workstep(transplant_json, a)
	ws_auto_sow := run.make_workstep(auto_sowing_json, a)
	ws_harvest := run.make_workstep(harvest_json, a)
	ws_cutting := run.make_workstep(cutting_json, a)
	ws_min_fert := run.make_workstep(mineral_fert_json, a)
	ws_tillage := run.make_workstep(tillage_json, a)
	ws_irrig := run.make_workstep(irrigation_json, a)

	dump_ws_common(&t, "wsAI", ws_ai)
	dump_ws_common(&t, "wsSow", ws_sow)
	dump_ws_common(&t, "wsND1", ws_nd1)
	dump_ws_common(&t, "wsAH", ws_ah)
	dump_ws_common(&t, "wsOF", ws_of)
	dump_ws_common(&t, "wsTransplant", ws_transplant)
	dump_ws_common(&t, "wsAutoSow", ws_auto_sow)
	dump_ws_common(&t, "wsHarvest", ws_harvest)
	dump_ws_common(&t, "wsCutting", ws_cutting)
	dump_ws_common(&t, "wsMinFert", ws_min_fert)
	dump_ws_common(&t, "wsTillage", ws_tillage)
	dump_ws_common(&t, "wsIrrig", ws_irrig)
	{
		sd := &ws_sow.data.(run.Sowing_Data)
		tr.dump(&t, "wsSow.cropName", sd.cropParams.speciesParams.pc_SpeciesId)
		tr.dump(&t, "wsSow.initialKcb", sd.initialKcb)
		tr.dump(&t, "wsSow.isValid", sd.isValid)
	}
	{
		td := &ws_transplant.data.(run.Transplant_Data)
		tr.dump(&t, "wsTransplant.initialGDD", td.initialGDD)
		tr.dump(&t, "wsTransplant.initialStage", td.initialStage)
	}
	{
		cd := &ws_cutting.data.(run.Cutting_Data)
		tr.dump(&t, "wsCutting.organId2cuttingSpec.size", len(cd.organId2cuttingSpec))
		for _, v in cd.organId2cuttingSpec {
			tr.dump(&t, "wsCutting.spec.value", v.value)
			tr.dump(&t, "wsCutting.spec.unit", int(v.unit))
			tr.dump(&t, "wsCutting.spec.cut_or_left", int(v.cut_or_left))
		}
		tr.dump(&t, "wsCutting.cutMaxAssimilationRateFraction", cd.cutMaxAssimilationRateFraction)
	}
	{
		aid := &ws_ai.data.(run.Automatic_Irrigation_Data)
		tr.dump(&t, "wsAI.startStage", aid.startStage)
		tr.dump(&t, "wsAI.endStage", aid.endStage)
		tr.dump(&t, "wsAI.irrigateCrop", aid.irrigateCrop)
		tr.dump(&t, "wsAI.params.amount", aid.params.amount)
		tr.dump(&t, "wsAI.params.threshold", aid.params.threshold)
	}
	{
		nd := &ws_nd1.data.(run.N_Demand_Fertilization_Data)
		tr.dump(&t, "wsND1.Ndemand", nd.Ndemand)
		tr.dump(&t, "wsND1.depth", nd.depth)
	}

	// reinit AI/AH before the season starts (mirrors cultivation-method::reinit)
	season_start := d.from_iso_date_string("2020-09-22")
	_ = run.workstep_reinit(ws_ai, season_start)
	_ = run.workstep_reinit(ws_ah, season_start)

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
			model.soilMoisture.frost_component.vm_TemperatureUnderSnow,
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

		// dated worksteps: apply on their exact matching day
		if d.eq(run.workstep_abs_date(ws_sow), current_date) {
			run.workstep_apply(ws_sow, model)
		}
		if d.eq(run.workstep_abs_date(ws_nd1), current_date) {
			run.workstep_apply(ws_nd1, model)
		}
		if d.eq(run.workstep_abs_date(ws_nd2), current_date) {
			run.workstep_apply(ws_nd2, model)
		}
		if d.eq(run.workstep_abs_date(ws_nd3), current_date) {
			run.workstep_apply(ws_nd3, model)
		}

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

		// Cutting - applied mid-season on the still-growing primary crop (the
		// realistic use case for this workstep), well before automatic harvest
		// normally fires.
		if day == 100 && model.currentCropModule != nil {
			tr.set_day(&t, 9003)
			tr.dump(
				&t,
				"cropModule.vc_LeafAreaIndex.beforeCut",
				model.currentCropModule.leaf_area_index,
			)
			run.workstep_apply(ws_cutting, model)
			tr.dump(
				&t,
				"cropModule.vc_LeafAreaIndex.afterCut",
				model.currentCropModule.leaf_area_index,
			)
			tr.dump(
				&t,
				"cropModule.vc_exportedCutBiomass",
				model.currentCropModule.exported_cut_biomass,
			)
			tr.set_day(&t, day)
		}

		// dynamic worksteps: every day via applyWithPossibleCondition
		_ = run.workstep_apply_with_possible_condition(ws_ai, model)
		if !harvested && run.workstep_apply_with_possible_condition(ws_ah, model) {
			harvested = true
			tr.set_day(&t, 1000 + day)
			dump_model_bits(&t, "model", model)
			tr.dump(&t, "harvestedOnDate", d.to_iso_date_string(current_date))
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

	// OrganicFertilization - mirrors the fixture's "after: Harvest" trigger
	run.workstep_apply(ws_of, model)
	tr.set_day(&t, 9001)
	dump_model_bits(&t, "model", model)

	// Tillage / Irrigation / MineralFertilization on the now bare-soil model
	run.workstep_apply(ws_tillage, model)
	run.workstep_apply(ws_irrig, model)
	run.workstep_apply(ws_min_fert, model)
	tr.set_day(&t, 9002)
	dump_model_bits(&t, "model", model)
	tr.dump(&t, "sc.vs_SurfaceWaterStorage", model.soilColumn.vs_SurfaceWaterStorage)
	tr.dump(&t, "sc.layers0.vs_SoilNO3", model.soilColumn.layers[0].vs_SoilNO3)

	// Harvest - dump then apply on whatever crop module is currently present
	// (the fixture's own crop, possibly already automatic-harvested above -
	// dailyReset, which would actually clear currentCropModule to nil after a
	// harvest, is cultivation-method/step() territory, phase 6 checkpoints
	// 5-6, not yet ported - so this exercises Harvest's own logic on the
	// still-present CropModule regardless).
	tr.set_day(&t, 9004)
	run.workstep_apply(ws_harvest, model)
	dump_model_bits(&t, "model", model)
}
