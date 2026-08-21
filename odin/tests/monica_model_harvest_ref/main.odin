// Odin side of the phase 6 checkpoint 2 second prerequisite
// (monica_model_harvest_current_crop/monica_model_incorporate_current_crop)
// differential test - see
// odin/tests/cpp_ref/monica_model_harvest_ref_main.cpp's header comment.
// Run odin/tests/cpp_ref/run_monica_model_harvest.sh to build both and diff them.
package monica_model_harvest_ref

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

dump_model_harvest :: proc(t: ^tr.Tracer, path: string, model: ^core.Monica_Model) {
	tr.dump(
		t,
		strings.concatenate({path, ".optCarbonExportedResidues"}),
		model.optCarbonExportedResidues,
	)
	tr.dump(
		t,
		strings.concatenate({path, ".optCarbonReturnedResidues"}),
		model.optCarbonReturnedResidues,
	)
	tr.dump(t, strings.concatenate({path, ".humusBalanceCarryOver"}), model.humusBalanceCarryOver)
	tr.dump(t, strings.concatenate({path, ".clearCropUponNextDay"}), model.clearCropUponNextDay)
}

dump_soil_organic_top3 :: proc(t: ^tr.Tracer, path: string, so: ^core.Soil_Organic) {
	for i in 0 ..< 3 {
		p2 := fmt.tprintf("%s[%d]", path, i)
		tr.dump(
			&(t^),
			strings.concatenate({p2, ".vo_AOM_Pool.size"}),
			len(so.soilColumn.layers[i].vo_AOM_Pool),
		)
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
	if len(args) < 4 {
		fmt.eprintln(
			"usage: monica_model_harvest_ref <pathToSimJson> <pathToClimateCsv> <numDays>",
		)
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

	g_soil_moisture = &model.soilMoisture
	g_soil_organic = &model.soilOrganic

	cm := core.make_crop_module(
		&model.soilColumn,
		&wheat_crop_params,
		&wheat_residue_params,
		&model.sitePs,
		&model.cropPs,
		&model.simPs,
		no_fire_event,
		real_add_organic_matter,
		real_get_snow_depth,
		nil,
		a,
	)
	model.currentCropModule = &cm
	g_residue_params = &cm.residueParams.base

	model.soilMoisture.crop_module = &cm
	model.soilOrganic.cropModule = &cm
	model.soilTransport.cropModule = &cm

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
			&model.soilTemperature,
			tmin,
			tmax,
			globrad,
			cm.vc_SoilCoverage,
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
			model.envPs.p_WindSpeedHeight,
			ATM_CO2,
			ATM_O3,
			precip,
			-1.0,
			a,
		)
		core.soil_organic_step(&model.soilOrganic, tavg, precip, wind)
		core.soil_transport_step(&model.soilTransport)
		free_all(context.temp_allocator)
	}

	t := tr.make_tracer(os.to_stream(os.stdout), a)
	defer tr.destroy_tracer(&t)

	model.sumOrganicFertilizerDM = 500.0

	// scenario 0: harvestCurrentCrop, exported=true, empty spec, old default behavior
	tr.set_day(&t, 0)
	{
		spec: core.Harvest_Spec
		ocmd := core.Harvest_Opt_Carbon_Management_Data {
			maxResidueRecoverFraction = 1,
			cropUsage                 = .Biomass_Production,
		}
		core.monica_model_harvest_current_crop(model, true, spec, ocmd, 0, a)
	}
	dump_model_harvest(&t, "model", model)
	dump_soil_organic_top3(&t, "so", &model.soilOrganic)

	// scenario 1: harvestCurrentCrop, exported=true, empty spec, optCarbonConservation
	tr.set_day(&t, 1)
	{
		spec: core.Harvest_Spec
		ocmd := core.Harvest_Opt_Carbon_Management_Data {
			optCarbonConservation     = true,
			cropImpactOnHumusBalance  = 100.0,
			maxResidueRecoverFraction = 0.8,
			residueHeq                = 20.0,
			organicFertilizerHeq      = 15.0,
			cropUsage                 = .Biomass_Production,
		}
		core.monica_model_harvest_current_crop(model, true, spec, ocmd, 1, a)
	}
	dump_model_harvest(&t, "model", model)
	dump_soil_organic_top3(&t, "so", &model.soilOrganic)

	// scenario 2: same as 1 but cropUsage=greenManure
	tr.set_day(&t, 2)
	{
		spec: core.Harvest_Spec
		ocmd := core.Harvest_Opt_Carbon_Management_Data {
			optCarbonConservation     = true,
			cropImpactOnHumusBalance  = 100.0,
			maxResidueRecoverFraction = 0.8,
			residueHeq                = 20.0,
			organicFertilizerHeq      = 15.0,
			cropUsage                 = .Green_Manure,
		}
		core.monica_model_harvest_current_crop(model, true, spec, ocmd, 0, a)
	}
	dump_model_harvest(&t, "model", model)
	dump_soil_organic_top3(&t, "so", &model.soilOrganic)

	// scenario 3: detailed spec covering organs 1 (leaf) and 3 (fruit)
	tr.set_day(&t, 3)
	{
		spec: core.Harvest_Spec
		spec.organ2specVal = make(map[int]core.Harvest_Spec_Value, 0, a)
		spec.organ2specVal[1] = core.Harvest_Spec_Value {
			exportPercentage = 70.0,
			incorporate      = true,
		}
		spec.organ2specVal[3] = core.Harvest_Spec_Value {
			exportPercentage = 90.0,
			incorporate      = false,
		}
		ocmd := core.Harvest_Opt_Carbon_Management_Data {
			maxResidueRecoverFraction = 1,
			cropUsage                 = .Biomass_Production,
		}
		core.monica_model_harvest_current_crop(model, true, spec, ocmd, 0, a)
	}
	dump_model_harvest(&t, "model", model)
	dump_soil_organic_top3(&t, "so", &model.soilOrganic)

	// scenario 4: exported=false, empty spec - the "total plant" else-branch
	tr.set_day(&t, 4)
	{
		spec: core.Harvest_Spec
		ocmd := core.Harvest_Opt_Carbon_Management_Data {
			maxResidueRecoverFraction = 1,
			cropUsage                 = .Biomass_Production,
		}
		core.monica_model_harvest_current_crop(model, false, spec, ocmd, 0, a)
	}
	dump_model_harvest(&t, "model", model)
	dump_soil_organic_top3(&t, "so", &model.soilOrganic)

	// scenario 5: incorporateCurrentCrop
	tr.set_day(&t, 5)
	core.monica_model_incorporate_current_crop(model, a)
	dump_model_harvest(&t, "model", model)
	dump_soil_organic_top3(&t, "so", &model.soilOrganic)
}
