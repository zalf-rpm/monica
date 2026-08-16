// Odin side of the trace-machinery proof driver.
//
// Must emit the same lines as odin/tests/cpp_ref/trace_ref_main.cpp (both sides
// are sorted before diffing, so field declaration order does not matter).
//
// Phase 3 already proved both implementations construct an identical
// SoilColumn, so any disagreement here is a fault in the trace machinery -
// value formatting, path construction, field coverage - rather than in the
// model. That is the point: validate the harness before phase 4 depends on it.
package trace_ref

import "core:bufio"
import "core:fmt"
import "core:io"
import "core:os"
import core "../../monica/core"
import soil "../../monica/soil"
import tr "../../monica/trace"
import jx "../../support/jsonx"
import tl "../../support/tools"

main :: proc() {
	args := os.args
	if len(args) < 2 {
		fmt.eprintln("usage: trace_ref <pathToSiteJson>")
		os.exit(2)
	}
	path_to_site_json := args[1]

	arena: jx.Arena
	if !jx.arena_init(&arena) {
		fmt.eprintln("arena init failed")
		os.exit(1)
	}
	defer jx.arena_destroy(&arena)
	a := jx.arena_allocator(&arena)

	site_r := jx.read_and_parse_json_file(path_to_site_json, a)
	if tl.failure(site_r.errs) {
		tl.print_possible_errors(site_r.errs)
		os.exit(1)
	}

	site_params := jx.get(site_r.result, "SiteParameters")
	soil_profile_params := jx.array_items(jx.get(site_params, "SoilProfileParameters"))

	path_to_soil_dir := tl.fix_system_separator(
		tl.replace_env_vars("${MONICA_PARAMETERS}/soil/", a),
		a,
	)

	layer_thickness := jx.double_value_key_d(site_params, "LayerThickness", 0.1)
	number_of_layers := jx.int_value_key_d(site_params, "NumberOfLayers", 20)

	pms_res := soil.create_equal_sized_soil_pms(
		.WESSOLEK2009,
		path_to_soil_dir,
		soil_profile_params,
		layer_thickness,
		number_of_layers,
		a,
	)
	if tl.failure(pms_res.errs) {
		tl.print_possible_errors(pms_res.errs)
	}

	sc := core.make_soil_column(layer_thickness, 0.4, pms_res.result[:], a)

	// buffered: a full-season trace is millions of lines
	bw: bufio.Writer
	bufio.writer_init(&bw, os.to_stream(os.stdout), 1 << 16, a)
	defer bufio.writer_flush(&bw)
	w := bufio.writer_to_stream(&bw)

	t := tr.make_tracer(w, a)
	defer tr.destroy_tracer(&t)
	tr.set_day(&t, 0)

	for i in 0 ..< len(sc.layers) {
		path := fmt.tprintf("soilColumn.layers[%d]", i)
		tr.dump(&t, path, sc.layers[i])
		free_all(context.temp_allocator)
	}

	_ = io.Writer{}
}
