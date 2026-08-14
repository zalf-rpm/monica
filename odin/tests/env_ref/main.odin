// Odin side of the Phase 1b differential test for monica/run.
//
// Must emit byte-identical output to odin/tests/cpp_ref/env_ref_main.cpp.
// Run odin/tests/cpp_ref/run_env.sh to build both and diff them.
package env_ref

import "core:fmt"
import "core:os"
import "base:runtime"
import "core:strings"
import mrun "../../monica/run"
import jx "../../support/jsonx"
import tl "../../support/tools"

main :: proc() {
	args := os.args
	if len(args) < 2 {
		fmt.eprintln("usage: env_ref <pathToSimJson>")
		os.exit(2)
	}

	arena: jx.Arena
	if !jx.arena_init(&arena) {
		fmt.eprintln("arena init failed")
		os.exit(1)
	}
	defer jx.arena_destroy(&arena)
	alloc := jx.arena_allocator(&arena)

	path_to_sim_json := args[1]
	path_of_sim_json, _ := tl.split_path_to_file(path_to_sim_json, alloc)

	simr := jx.read_and_parse_json_file(path_to_sim_json, alloc)
	if tl.failure(simr.errs) {
		tl.print_possible_errors(simr.errs)
		os.exit(1)
	}

	// rebuild the sim object with the path fixups monica-run-main.cpp applies
	simm := make(jx.Object, 0, alloc)
	for k, v in jx.object_items(simr.result) {
		simm[strings.clone(k, alloc)] = v
	}
	simm[strings.clone("sim.json", alloc)] = jx.Value(
		jx.String(strings.clone(path_to_sim_json, alloc)),
	)

	fixup :: proc(m: ^jx.Object, key: string, base: string, alloc: runtime.Allocator) {
		p := jx.string_value_of(m[key] or_else jx.Value{})
		if !tl.is_absolute_path(p) {
			m[strings.clone(key, alloc)] = jx.Value(
				jx.String(strings.concatenate({base, p}, alloc)),
			)
		}
	}
	fixup(&simm, "crop.json", path_of_sim_json, alloc)
	fixup(&simm, "site.json", path_of_sim_json, alloc)
	fixup(&simm, "climate.csv", path_of_sim_json, alloc)

	sim_v := jx.Value(simm)

	cropr := jx.read_and_parse_json_file(jx.string_value(sim_v, "crop.json"), alloc)
	tl.print_possible_errors(cropr.errs)
	siter := jx.read_and_parse_json_file(jx.string_value(sim_v, "site.json"), alloc)
	tl.print_possible_errors(siter.errs)

	base_path := jx.get(sim_v, "include-file-base-path")

	// --- 1. the three documents after reference resolution --------------------
	names := []string{"crop", "site", "sim"}
	docs := []jx.Value{cropr.result, siter.result, sim_v}
	for name, i in names {
		j := docs[i]
		if !(jx.is_object(j) && jx.is_string(jx.get(j, "include-file-base-path"))) {
			m := make(jx.Object, 0, alloc)
			for k, v in jx.object_items(j) {
				m[strings.clone(k, alloc)] = v
			}
			m[strings.clone("include-file-base-path", alloc)] = base_path
			j = jx.Value(m)
		}
		r := mrun.find_and_replace_references(j, j, alloc)
		fmt.printf("RESOLVED-%s\t%s\n", name, jx.dump(r.result, alloc))
	}

	// --- 2. the assembled Env -------------------------------------------------
	// the ref cache is a process-wide static in the C++ too, but reset it here so
	// this second pass starts from the same state the C++ pass does
	env := mrun.create_env_json_from_json_objects(cropr.result, siter.result, sim_v, alloc)
	fmt.printf("ENV\t%s\n", jx.dump(env, alloc))
}
