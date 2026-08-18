package tests

import "core:os"
import "core:strings"
import "core:testing"

// Guard for CONVENTIONS.md §1 "Transcendental functions: core:c/libc, not
// core:math": core:math's pow/exp/log/trig/... are a pure-Odin
// implementation and are not always bit-identical to the C++ reference
// build's <cmath> calls (MSVC CRT) - confirmed once already in phase 3
// (soillayer::soilMoisturePF) as a last-2-ULP divergence a differential
// test on finitely many inputs didn't catch.
//
// odin/tests/check_libc_transcendentals.sh already checks this same rule,
// but as a standalone script it needs a second, easy-to-forget invocation
// alongside `odin test odin/tests`. This wires the identical check into the
// test suite itself, so it's enforced every time that one documented
// command runs instead of only when someone remembers the script exists.
//
// Walks every *.odin file under odin/ (relative to CWD - `odin test
// odin/tests` is meant to run from the repo root, same assumption every
// odin/tests/cpp_ref/run_*.sh script already makes) and fails if any calls
// a C <math.h> §7.12 trig/hyperbolic/exponential/logarithmic/power/gamma
// function through core:math. core:math is still fine - and not flagged -
// for round/floor/ceil/abs/is_nan/is_inf/comparisons/PI/etc, since those
// aren't FFI-risk transcendentals.
@(test)
test_no_core_math_transcendentals :: proc(t: ^testing.T) {
	names := []string {
		"pow",
		"exp",
		"exp2",
		"expm1",
		"ln",
		"log",
		"log2",
		"log10",
		"log1p",
		"sqrt",
		"cbrt",
		"sin",
		"cos",
		"tan",
		"asin",
		"acos",
		"atan",
		"atan2",
		"sinh",
		"cosh",
		"tanh",
		"asinh",
		"acosh",
		"atanh",
		"hypot",
		"erf",
		"erfc",
		"lgamma",
		"tgamma",
	}
	defer free_all(context.temp_allocator)

	violations := 0

	w := os.walker_create("odin")
	defer os.walker_destroy(&w)

	for info in os.walker_walk(&w) {
		if path, err := os.walker_error(&w); err != nil {
			testing.expectf(t, false, "failed walking %s: %v", path, err)
			continue
		}
		if info.type == .Directory || !strings.has_suffix(info.name, ".odin") {
			continue
		}

		data, rerr := os.read_entire_file(info.fullpath, context.temp_allocator)
		if rerr != nil {
			testing.expectf(t, false, "failed reading %s: %v", info.fullpath, rerr)
			continue
		}
		content := string(data)

		for name in names {
			needle := strings.concatenate({"math.", name, "("}, context.temp_allocator)
			search := content
			for {
				rel := strings.index(search, needle)
				if rel == -1 {
					break
				}
				testing.expectf(
					t,
					false,
					"%s: stray core:math transcendental `%s` - use core:c/libc instead (CONVENTIONS.md §1)",
					info.fullpath,
					needle,
				)
				violations += 1
				search = search[rel + len(needle):]
			}
		}
	}

	if path, err := os.walker_error(&w); err != nil {
		testing.expectf(t, false, "failed walking %s: %v", path, err)
	}

	testing.expect_value(t, violations, 0)
}
