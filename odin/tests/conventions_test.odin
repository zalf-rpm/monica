package tests

import "core:os"
import "core:strings"
import "core:testing"

// Guard for CONVENTIONS.md §1 "Transcendental functions: cr_math only":
// every exp/log/pow/trig/... call goes through support/cr_math (CORE-MATH,
// correctly rounded), never through core:math (a pure-Odin implementation)
// or core:c/libc (the platform's libm - glibc and Microsoft's UCRT round
// differently in the last bit, and MONICA amplifies a single last-bit
// difference into percent-level yield differences after a few years).
//
// odin/tests/check_transcendentals.sh checks the same rule as a standalone
// script; this wires it into `odin test odin/tests`, so it runs every time.
//
// Walks every *.odin file under odin/ (relative to CWD - `odin test
// odin/tests` runs from the repo root, see tools/odinw.py) and fails on any
// `math.<name>(` or `libc.<name>(` for a C <math.h> §7.12 trig/hyperbolic/
// exponential/logarithmic/power/gamma function. Not flagged: core:math's
// round/floor/ceil/abs/PI/..., and libc.sqrt and math.sqrt (IEEE 754 requires
// sqrt to be correctly rounded, so every libm agrees; core:math's is the
// intrinsic, the CPU's sqrt instruction). A name matches only as a whole
// identifier, so a cr_math call is not mistaken for core:math's.
//
// Hidden directories are not descended into: only this port's own sources are
// in scope. odin/pixi.toml's build setup puts the bootstrapped Odin compiler in
// odin/.odin/ and its conda environment in odin/.pixi/, and Odin's own core
// library legitimately calls math.atanh/math.pow/... - 1568 false positives
// before this was skipped.
@(test)
test_transcendentals_use_cr_math :: proc(t: ^testing.T) {
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
		if info.type == .Directory {
			// build/: scratch, e.g. gen-amei's working copies (build/crop2ml)
			if strings.has_prefix(info.name, ".") || info.fullpath == "odin/build" {
				os.walker_skip_dir(&w)
			}
			continue
		}
		if !strings.has_suffix(info.name, ".odin") {
			continue
		}

		data, rerr := os.read_entire_file(info.fullpath, context.temp_allocator)
		if rerr != nil {
			testing.expectf(t, false, "failed reading %s: %v", info.fullpath, rerr)
			continue
		}
		content := string(data)

		for pkg in ([]string{"math", "libc"}) {
			for name in names {
				if name == "sqrt" {
					continue
				}
				needle := strings.concatenate({pkg, ".", name, "("}, context.temp_allocator)
				search := content
				offset := 0
				for {
					rel := strings.index(search, needle)
					if rel == -1 {
						break
					}
					at := offset + rel
					// whole identifier only: not the tail of e.g. cr_math.pow(
					if at == 0 || !is_ident_byte(content[at - 1]) {
						testing.expectf(
							t,
							false,
							"%s: `%s` - use cr_math.%s instead (CONVENTIONS.md §1)",
							info.fullpath,
							needle,
							name,
						)
						violations += 1
					}
					search = search[rel + len(needle):]
					offset = at + len(needle)
				}
			}
		}
	}

	if path, err := os.walker_error(&w); err != nil {
		testing.expectf(t, false, "failed walking %s: %v", path, err)
	}

	testing.expect_value(t, violations, 0)
}

@(private = "file")
is_ident_byte :: proc(c: u8) -> bool {
	return c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
}
