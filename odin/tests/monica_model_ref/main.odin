// Odin side of the phase 6 checkpoint 1 (MonicaModel scaffolding +
// standalone helpers) differential test - see
// odin/tests/cpp_ref/monica_model_ref_main.cpp's header comment for the full
// scenario list.
// Run odin/tests/cpp_ref/run_monica_model.sh to build both and diff them.
package monica_model_ref

import core "../../monica/core"
import p "../../monica/params"
import mrun "../../monica/run"
import tr "../../monica/trace"
import d "../../support/date"
import jx "../../support/jsonx"
import tl "../../support/tools"
import "core:fmt"
import "core:os"
import "core:slice"
import "core:strings"

dump_model :: proc(t: ^tr.Tracer, path: string, model: ^core.Monica_Model) {
	tr.dump(t, strings.concatenate({path, ".sumFertiliser"}), model.sum_fertiliser)
	tr.dump(t, strings.concatenate({path, ".sumOrgFertiliser"}), model.sum_org_fertiliser)
	tr.dump(t, strings.concatenate({path, ".dailySumFertiliser"}), model.daily_sum_fertiliser)
	tr.dump(t, strings.concatenate({path, ".dailySumOrgFertiliser"}), model.daily_sum_org_fertiliser)
	tr.dump(
		t,
		strings.concatenate({path, ".dailySumOrganicFertilizerDM"}),
		model.daily_sum_organic_fertilizer_dm,
	)
	tr.dump(
		t,
		strings.concatenate({path, ".sumOrganicFertilizerDM"}),
		model.sum_organic_fertilizer_dm,
	)
	tr.dump(t, strings.concatenate({path, ".humusBalanceCarryOver"}), model.humus_balance_carry_over)
	tr.dump(
		t,
		strings.concatenate({path, ".dailySumIrrigationWater"}),
		model.daily_sum_irrigation_water,
	)
	tr.dump(t, strings.concatenate({path, ".clearCropUponNextDay"}), model.clear_crop_upon_next_day)
	tr.dump(
		t,
		strings.concatenate({path, ".cultivationMethodCount"}),
		model.cultivation_method_count,
	)
}

dump_soil_column_top3 :: proc(t: ^tr.Tracer, path: string, sc: ^core.Soil_Column) {
	tr.dump(t, strings.concatenate({path, ".vs_SurfaceWaterStorage"}), sc.vs_SurfaceWaterStorage)
	tr.dump(t, strings.concatenate({path, ".vf_TopDressing"}), sc.vf_TopDressing)
	tr.dump(t, strings.concatenate({path, ".vf_TopDressingDelay"}), sc.vf_TopDressingDelay)
	for i in 0 ..< 3 {
		p2 := fmt.tprintf("%s[%d]", path, i)
		li := &sc.layers[i]
		tr.dump(&(t^), strings.concatenate({p2, ".vs_SoilNO3"}), li.soil_no3)
		tr.dump(&(t^), strings.concatenate({p2, ".vs_SoilNH4"}), li.soil_nh4)
		tr.dump(&(t^), strings.concatenate({p2, ".vs_SoilCarbamid"}), li.soil_carbamid)
		tr.dump(&(t^), strings.concatenate({p2, ".vs_SoilTemperature"}), li.soil_temperature)
		tr.dump(&(t^), strings.concatenate({p2, ".vs_SoilMoisture_m3"}), li.soil_moisture_m3)
		tr.dump(&(t^), strings.concatenate({p2, ".vs_SoilOrganicCarbon"}), li.soil_organic_carbon)
		tr.dump(&(t^), strings.concatenate({p2, ".vo_AOM_Pool.size"}), len(li.vo_AOM_Pool))
	}
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
	if len(args) < 2 {
		fmt.eprintln("usage: monica_model_ref <pathToSimJson>")
		os.exit(2)
	}
	path_to_sim_json := args[1]

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

	t := tr.make_tracer(os.to_stream(os.stdout), a)
	defer tr.destroy_tracer(&t)

	// scenario 0: right after construction
	tr.set_day(&t, 0)
	dump_model(&t, "model", model)
	dump_soil_column_top3(&t, "sc", &model.soil_column)

	// scenario 1: applyMineralFertiliser
	tr.set_day(&t, 1)
	mfp := p.Mineral_Fertilizer_Parameters {
		vo_NO3      = 50.0,
		vo_NH4      = 30.0,
		vo_Carbamid = 20.0,
	}
	core.monica_model_apply_mineral_fertiliser(model, mfp, 40.0)
	dump_model(&t, "model", model)
	dump_soil_column_top3(&t, "sc", &model.soil_column)

	// scenario 2: applyOrganicFertiliser
	tr.set_day(&t, 2)
	omp := p.Organic_Matter_Parameters {
		vo_AOM_DryMatterContent = 0.35,
		vo_AOM_NH4Content       = 0.002,
		vo_AOM_NO3Content       = 0.0005,
		vo_AOM_CarbamidContent  = 0.0,
		vo_PartAOM_to_AOM_Slow  = 0.5,
		vo_PartAOM_to_AOM_Fast  = 0.3,
		vo_CN_Ratio_AOM_Slow    = 20.0,
		vo_CN_Ratio_AOM_Fast    = 10.0,
		vo_NConcentration       = 0.01,
	}
	core.monica_model_apply_organic_fertiliser(model, &omp, 1000.0, true, 0)
	dump_model(&t, "model", model)
	dump_soil_column_top3(&t, "sc", &model.soil_column)

	// scenario 3: applyMineralFertiliserViaNMinMethod, soil too wet -> delayed
	tr.set_day(&t, 3)
	model.soil_column.layers[0].soil_moisture_m3 =
		model.soil_column.layers[0].field_capacity + 0.01
	fertAmount1 := core.monica_model_apply_mineral_fertiliser_via_n_min_method(
		model,
		mfp,
		p.NMin_Crop_Parameters{samplingDepth = 0.3, nTarget = 80.0, nTarget30 = 40.0},
	)
	tr.dump(&t, "fertAmount1", fertAmount1)
	dump_model(&t, "model", model)
	dump_soil_column_top3(&t, "sc", &model.soil_column)
	drained := core.apply_possible_delayed_fertilizer(&model.soil_column)
	core.monica_model_add_daily_sum_fertiliser(model, drained)
	tr.dump(&t, "drained", drained)
	dump_model(&t, "model", model)
	dump_soil_column_top3(&t, "sc", &model.soil_column)

	// scenario 4: applyMineralFertiliserViaNMinMethod, soil dry -> immediate + top-dressing split
	tr.set_day(&t, 4)
	model.soil_column.layers[0].soil_moisture_m3 =
		model.soil_column.layers[0].field_capacity - 0.05
	fertAmount2 := core.monica_model_apply_mineral_fertiliser_via_n_min_method(
		model,
		mfp,
		p.NMin_Crop_Parameters{samplingDepth = 0.3, nTarget = 400.0, nTarget30 = 400.0},
	)
	tr.dump(&t, "fertAmount2", fertAmount2)
	dump_model(&t, "model", model)
	dump_soil_column_top3(&t, "sc", &model.soil_column)
	for i in 0 ..< 3 {
		tr.set_day(&t, 4)
		topDressed := core.apply_possible_top_dressing(&model.soil_column)
		tr.dump(&t, fmt.tprintf("topDressed[%d]", i), topDressed)
	}
	dump_model(&t, "model", model)
	dump_soil_column_top3(&t, "sc", &model.soil_column)
	core.clear_top_dressing_params(&model.soil_column)
	dump_soil_column_top3(&t, "sc", &model.soil_column)

	// scenario 5: applyMineralFertiliserViaNDemand
	tr.set_day(&t, 5)
	demandFert := core.apply_mineral_fertiliser_via_n_demand(&model.soil_column, mfp, 0.25, 60.0)
	tr.dump(&t, "demandFert", demandFert)
	dump_soil_column_top3(&t, "sc", &model.soil_column)

	// scenario 6: applyIrrigation
	tr.set_day(&t, 6)
	core.monica_model_apply_irrigation(model, 20.0, 5.0)
	dump_model(&t, "model", model)
	tr.dump(&t, "sc.vs_SurfaceWaterStorage", model.soil_column.vs_SurfaceWaterStorage)
	tr.dump(&t, "sc.layers[0].vs_SoilNO3", model.soil_column.layers[0].soil_no3)
	tr.dump(&t, "so.irrigationAmount", model.soil_organic.irrigation_amount)

	// scenario 7: applyIrrigationViaTrigger - needs a live cropModule
	tr.set_day(&t, 7)
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
	no_fire_event :: proc(_: string) {}
	no_add_organic_matter :: proc(_: map[int]f64, _: f64) {}
	no_snow :: proc(_: f64) -> (f64, f64) {return 0, 0}
	cm := core.make_crop_module(
		&model.soil_column,
		&wheat_crop_params,
		&wheat_residue_params,
		&model.site_ps,
		&model.crop_ps,
		&model.sim_ps,
		no_fire_event,
		no_add_organic_matter,
		no_snow,
		nil,
		a,
	)
	core.put_crop(&model.soil_column, &cm)
	cm.current_total_temperature_sum =
		(cm.crop_params.cultivarParams.pc_HeatSumIrrigationStart +
			cm.crop_params.cultivarParams.pc_HeatSumIrrigationEnd) /
		2.0
	aip := p.Automatic_Irrigation_Parameters {
		base = p.Irrigation_Parameters{nitrateConcentration = 3.0, fw = 1.0},
		amount = 15.0,
		threshold = 0.99,
		criticalMoistureDepthM = 0.3,
	}
	triggered, triggeredAmount := core.apply_irrigation_via_trigger(&model.soil_column, &aip)
	tr.dump(&t, "triggered", triggered)
	tr.dump(&t, "triggeredAmount", triggeredAmount)
	tr.dump(&t, "sc.vs_SurfaceWaterStorage", model.soil_column.vs_SurfaceWaterStorage)
	core.remove_crop(&model.soil_column)

	// scenario 8: applyTillage
	tr.set_day(&t, 8)
	for i in 0 ..< 3 {
		model.soil_column.layers[i].soil_no3 = 0.001 * f64(i + 1)
		model.soil_column.layers[i].soil_temperature = 5.0 + f64(i)
		model.soil_column.layers[i].soil_moisture_m3 = 0.2 + 0.01 * f64(i)
	}
	core.monica_model_apply_tillage(model, 0.25)
	dump_soil_column_top3(&t, "sc", &model.soil_column)

	// scenario 9: deleteAOMPool
	tr.set_day(&t, 9)
	poolCountBefore := len(model.soil_column.layers[0].vo_AOM_Pool)
	core.delete_aom_pool(&model.soil_column)
	poolCountAfter := len(model.soil_column.layers[0].vo_AOM_Pool)
	tr.dump(&t, "poolCountBefore", poolCountBefore)
	tr.dump(&t, "poolCountAfter", poolCountAfter)

	// scenario 10: daily-sum accumulators + clearEvents + dailyReset (bare soil)
	tr.set_day(&t, 10)
	core.monica_model_add_daily_sum_fertiliser(model, 5.0)
	core.monica_model_add_daily_sum_organic_fertilizer_dm(model, 7.0)
	core.monica_model_add_daily_sum_irrigation_water(model, 3.0)
	dump_model(&t, "model", model)
	core.monica_model_reset_fertiliser_counter(model)
	dump_model(&t, "model", model)
	model.current_events["Sowing"] = true
	model.current_events["Irrigation"] = true
	core.monica_model_clear_events(model, a)
	{
		keys := make([dynamic]string, 0, a)
		for k in model.previous_days_events {
			append(&keys, k)
		}
		slice.sort(keys[:])
		joined := strings.builder_make(a)
		for k in keys {
			strings.write_string(&joined, k)
			strings.write_string(&joined, ",")
		}
		tr.dump(&t, "previousDaysEvents", strings.to_string(joined))

		keys2 := make([dynamic]string, 0, a)
		for k in model.current_events {
			append(&keys2, k)
		}
		slice.sort(keys2[:])
		joined2 := strings.builder_make(a)
		for k in keys2 {
			strings.write_string(&joined2, k)
			strings.write_string(&joined2, ",")
		}
		tr.dump(&t, "currentEvents", strings.to_string(joined2))
	}
	model.clear_crop_upon_next_day = false
	core.monica_model_daily_reset(model, a)
	dump_model(&t, "model", model)

	// scenario 11: pure CO2ForDate / groundwaterDepthForDate sweeps
	tr.set_day(&t, 11)
	rcps := []p.RCP{.RCP19, .RCP26, .RCP34, .RCP45, .RCP60, .RCP70, .RCP85}
	years := []f64{1990.0, 2020.0, 2050.0, 2100.0}
	jdays := []f64{1.0, 100.0, 200.0, 365.0}
	idx := 0
	for year in years {
		for jday in jdays {
			for leap in ([]bool{false, true}) {
				for rcp in rcps {
					co2 := core.co2_for_date(year, jday, leap, rcp)
					tr.dump(&t, fmt.tprintf("co2Sweep[%d]", idx), co2)
					idx += 1
				}
			}
		}
	}
	d1 := d.julian_date(150, 2021)
	co2FromDate := core.co2_for_date_from_date(d1, .RCP45)
	tr.dump(&t, "co2FromDate", co2FromDate)

	idx2 := 0
	maxGw := []f64{10.0, 18.0}
	minGw := []f64{5.0, 20.0}
	months := []int{1, 3, 7, 12}
	jdays2 := []f64{1.0, 91.0, 182.0, 273.0, 365.0}
	for mx in maxGw {
		for mn in minGw {
			for mo in months {
				for jd in jdays2 {
					for leap in ([]bool{false, true}) {
						gw := core.groundwater_depth_for_date(mx, mn, mo, jd, leap)
						tr.dump(&t, fmt.tprintf("gwSweep[%d]", idx2), gw)
						idx2 += 1
					}
				}
			}
		}
	}
}
