package tests

import "core:testing"
import soil "../monica/soil"
import tl "../support/tools"

// The KA5/bulk-density/lambda patterns are covered end to end by
// odin/tests/cpp_ref/run_env.sh via the sim+ fixture, but humusClass2corg and
// KA5texture2sand are not reached by any config in the repo, so they are pinned
// here. Values are read straight off src/soil/conversion.cpp.
@(test)
test_humus_class_2_corg :: proc(t: ^testing.T) {
	check :: proc(t: ^testing.T, cls: int, expected: f64) {
		r := soil.humus_class_2_corg(cls)
		defer tl.errors_destroy(&r.errs)
		testing.expect(t, tl.success(r.errs))
		testing.expect_value(t, r.result, expected)
	}
	check(t, 0, 0.0)
	check(t, 1, 0.5 / 1.72)
	check(t, 2, 1.5 / 1.72)
	check(t, 3, 3.0 / 1.72)
	check(t, 4, 6.0 / 1.72)
	check(t, 5, 11.5 / 2.0)
	check(t, 6, 17.5 / 2.0)
	check(t, 7, 30.0 / 2.0)

	// an unknown class yields 0 plus an error
	bad := soil.humus_class_2_corg(99)
	defer tl.errors_destroy(&bad.errs)
	testing.expect(t, tl.failure(bad.errs))
	testing.expect_value(t, bad.result, 0.0)
}

@(test)
test_bulk_density_class_2_raw_density :: proc(t: ^testing.T) {
	// Expected values must mirror the C++ expression `(x - (0.9 * clay)) * 1000.0`
	// term for term: 0.9 * 0.1 is 0.09000000000000001, not 0.09, so folding it to
	// a literal here would compare against a different number. This is the
	// reassociation trap from CONVENTIONS §1, in a test rather than in the port.
	r := soil.bulk_density_class_2_raw_density(1, 0.1)
	defer tl.errors_destroy(&r.errs)
	testing.expect(t, tl.success(r.errs))
	testing.expect_value(t, r.result, (1.3 - (0.9 * 0.1)) * 1000.0)

	r5 := soil.bulk_density_class_2_raw_density(5, 0.2)
	defer tl.errors_destroy(&r5.errs)
	testing.expect_value(t, r5.result, (2.1 - (0.9 * 0.2)) * 1000.0)

	// NOTE(c++-quirk): an unknown class still computes a result, with x == 0
	bad := soil.bulk_density_class_2_raw_density(9, 0.1)
	defer tl.errors_destroy(&bad.errs)
	testing.expect(t, tl.failure(bad.errs))
	testing.expect_value(t, bad.result, (0.0 - (0.9 * 0.1)) * 1000.0)
}

@(test)
test_ka5_texture_lookups :: proc(t: ^testing.T) {
	s := soil.ka5_texture_2_sand("Sl2")
	defer tl.errors_destroy(&s.errs)
	testing.expect(t, tl.success(s.errs))
	testing.expect_value(t, s.result, 0.76) // input is uppercased first

	c := soil.ka5_texture_2_clay("Sl2")
	defer tl.errors_destroy(&c.errs)
	testing.expect_value(t, c.result, 0.06)

	// a few more spread across the table
	tt := soil.ka5_texture_2_clay("TT")
	defer tl.errors_destroy(&tt.errs)
	testing.expect_value(t, tt.result, 0.82)

	hn := soil.ka5_texture_2_sand("HN")
	defer tl.errors_destroy(&hn.errs)
	testing.expect_value(t, hn.result, 0.15)

	// unknown types: sand falls back to 0.66, clay to 0.0, both with an error
	us := soil.ka5_texture_2_sand("nonsense")
	defer tl.errors_destroy(&us.errs)
	testing.expect(t, tl.failure(us.errs))
	testing.expect_value(t, us.result, 0.66)

	uc := soil.ka5_texture_2_clay("nonsense")
	defer tl.errors_destroy(&uc.errs)
	testing.expect(t, tl.failure(uc.errs))
	testing.expect_value(t, uc.result, 0.0)
}

@(test)
test_sand_and_clay_2_lambda :: proc(t: ^testing.T) {
	sand := 0.76
	clay := 0.06
	expected := (2.0 * (sand * sand * 0.575)) + (clay * 0.1) + ((1.0 - sand - clay) * 0.35)
	testing.expect_value(t, soil.sand_and_clay_2_lambda(sand, clay), expected)
}
