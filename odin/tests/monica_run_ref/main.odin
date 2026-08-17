// Odin side of the phase 7 checkpoint 4 differential test - mirrors
// odin/tests/cpp_ref/run_monica_ref_main.cpp exactly: load sim-min.json/
// crop-min.json/site-min.json the same way monica-run's CLI (checkpoint 5)
// will, merge into an Env, run the real run.run_monica, and dump every
// output section through the already-verified write_output_header_rows/
// write_output. Run via odin/tests/cpp_ref/run_monica_run.sh.
package monica_run_ref

import "core:fmt"
import "core:io"
import "core:os"
import "core:strings"
import mio "../../monica/io"
import run "../../monica/run"
import jx "../../support/jsonx"
import tl "../../support/tools"

main :: proc() {
	args := os.args
	if len(args) < 2 {
		fmt.eprintln("usage: monica_run_ref <pathToSimJson>")
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

	env_json := run.create_env_json_from_json_objects(cropr.result, siter.result, sim_v, a)

	path_to_soil_dir := tl.fix_system_separator(tl.replace_env_vars("${MONICA_PARAMETERS}/soil/", a), a)

	env: run.Env
	errs := run.env_merge(&env, env_json, path_to_soil_dir, a)
	tl.print_possible_errors(errs)
	if tl.failure(errs) {
		os.exit(1)
	}

	out := run.run_monica(&env, a)

	output_opts := jx.get_path(sim_v, "output", "csv-options")
	csvSep := jx.string_value_of(jx.get(output_opts, "csv-separator"))
	includeHeaderRow := jx.bool_value_of(jx.get(output_opts, "include-header-row"))
	includeUnitsRow := jx.bool_value_of(jx.get(output_opts, "include-units-row"))
	includeAggRows := jx.bool_value_of(jx.get(output_opts, "include-aggregation-rows"))

	w := os.to_stream(os.stdout)
	for section in out.data {
		sanitized, _ := strings.replace_all(section.origSpec, "\"", "", a)
		// C++'s cout, redirected to a file on Windows, is in text mode, so the
		// '\n' after "== section: ..." also becomes \r\n there - matched
		// explicitly since io.Writer never does that OS-level translation.
		io.write_string(w, "== section: ")
		io.write_string(w, sanitized)
		io.write_string(w, "\r\n")
		mio.write_output_header_rows(w, section.outputIds[:], csvSep, includeHeaderRow, includeUnitsRow, includeAggRows)
		mio.write_output(w, section.outputIds[:], section.results[:], csvSep)
	}
}
