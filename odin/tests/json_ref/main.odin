// Odin side of the Phase 1 differential test for support/jsonx.
//
// Must emit byte-identical output to odin/tests/cpp_ref/json_ref_main.cpp.
// Run odin/tests/cpp_ref/run_json.sh to build both and diff them over the real
// MONICA parameter files.
package json_ref

import "core:fmt"
import "core:os"
import "core:strings"
import jx "../../support/jsonx"
import tl "../../support/tools"

main :: proc() {
	args := os.args
	if len(args) < 2 {
		fmt.eprintln("usage: json_ref <file.json>...")
		os.exit(2)
	}

	// one arena for everything, as the real run will do
	arena: jx.Arena
	if !jx.arena_init(&arena) {
		fmt.eprintln("arena init failed")
		os.exit(1)
	}
	defer jx.arena_destroy(&arena)
	alloc := jx.arena_allocator(&arena)

	for path in args[1:] {
		r := jx.read_and_parse_json_file(path, alloc)

		// normalise the path separator so the two sides agree
		norm, _ := strings.replace_all(path, "\\", "/", alloc)

		if tl.failure(r.errs) {
			fmt.printf("%s\tERR\t\n", norm)
		} else {
			fmt.printf("%s\tok\t%s\n", norm, jx.dump(r.result, alloc))
		}
	}
}
