// Odin side of the phase 4 soiltemperature differential test.
//
// Must emit byte-identical output to
// odin/tests/cpp_ref/soil_temperature_ref_main.cpp - see that file's header
// comment for why the two drivers poke synthetic snow-depth/temperature-under-
// snow values directly rather than running a live SoilMoisture, and why
// currentCropModule stays "absent" (soil_coverage always 0) on both sides.
// Run odin/tests/cpp_ref/run_soil_temperature.sh to build both and diff them.
package soil_temperature_ref

import "core:bufio"
import "core:fmt"
import "core:io"
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

main :: proc() {
	args := os.args
	if len(args) < 4 {
		fmt.eprintln("usage: soil_temperature_ref <pathToSimJson> <pathToClimateCsv> <numDays>")
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

	// --- build CentralParameterProvider, exactly like central_params_ref ---
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

	// --- build SoilColumn + SoilTemperature, mirroring
	// initializeMonicaModelFromParams (monica-model.cpp) minus the modules
	// soiltemperature doesn't need ---
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

	// --- climate ---
	// Built via csv_via_header_options_merge (which derives lineNoOfDataStart
	// from lineNoOfHeaderLine+noOfHeaderLines), not by hand-setting fields on a
	// zero-valued Csv_Via_Header_Options: skipping merge leaves a stale
	// default that crashes the CSV parser. Mirrors sim-min.json's
	// "climate.csv-options" keys exactly.
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
	if num_days > 0 && int(num_days) < n {
		n = int(num_days)
	}

	// buffered: a multi-year daily trace is millions of lines
	bw: bufio.Writer
	bufio.writer_init(&bw, os.to_stream(os.stdout), 1 << 16, a)
	defer bufio.writer_flush(&bw)
	w := bufio.writer_to_stream(&bw)

	t := tr.make_tracer(w, a)
	defer tr.destroy_tracer(&t)

	for day in 0 ..< n {
		tmin := clim.data_accessor_data_for_timestep(&da, .tmin, day)
		tmax := clim.data_accessor_data_for_timestep(&da, .tmax, day)
		globrad := clim.data_accessor_data_for_timestep(&da, .globrad, day)

		// synthetic, deterministic snow sequence - identical on both sides, not
		// derived from any model. Exercises both branches of
		// calc_soil_surface_temperature's snow check repeatedly over the run.
		snow_depth: f64 = (day % 30) < 10 ? 50.0 : 0.0
		temperature_under_snow: f64 = -2.0 - f64(day % 5)

		core.step(&st, tmin, tmax, globrad, 0.0, snow_depth, temperature_under_snow)

		tr.set_day(&t, day)
		tr.dump(&t, "soilTemperature", st)
		free_all(context.temp_allocator)
	}

	_ = io.Writer{}
}
