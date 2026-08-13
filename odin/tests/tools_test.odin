package tests

import "core:testing"
import tl "../support/tools"

// Ground truth: a 1:1 port of the round/roundShiftedInt assertions from
// Tools::testRoundFloorCeil() in mas_cpp_misc/tools/algorithms.cpp.
@(test)
test_round_cpp_testRoundFloorCeil :: proc(t: ^testing.T) {
	testing.expect_value(t, tl.round(1.5), 2.0)
	testing.expect_value(t, tl.round(1.4), 1.0)
	testing.expect_value(t, tl.round(2.0), 2.0)

	testing.expect_value(t, tl.round(1.55, 1), 1.6)
	testing.expect_value(t, tl.round(1.54, 1), 1.5)
	testing.expect_value(t, tl.round(1.5, 1), 1.5)

	testing.expect_value(t, tl.round(1111111.123456, 3), 1111111.123)
	testing.expect_value(t, tl.round(1111111.123556, 3), 1111111.124)
	testing.expect_value(t, tl.round(1111111.123656, 3), 1111111.124)
	testing.expect_value(t, tl.round(111111111.123656, 5), 111111111.12366)

	testing.expect_value(t, tl.round_shifted_int(1.5555, 3), 1556)
	testing.expect_value(t, tl.round_shifted_int(1.5551, 3), 1555)
	testing.expect_value(t, tl.round_shifted_int(1.5559, 3), 1556)
}

@(test)
test_round_negative_digits :: proc(t: ^testing.T) {
	testing.expect_value(t, tl.round(123.0, -1), 120.0)
	testing.expect_value(t, tl.round(125.0, -1), 130.0)
	testing.expect_value(t, tl.round(130.0, -1), 130.0)
	testing.expect_value(t, tl.round(123456.0, -4), 120000.0)
	testing.expect_value(t, tl.round(125456.0, -4), 130000.0)
	testing.expect_value(t, tl.round(120456.0, -4), 120000.0)
}

@(test)
test_bound :: proc(t: ^testing.T) {
	testing.expect_value(t, tl.bound(0.0, 0.5, 1.0), 0.5)
	testing.expect_value(t, tl.bound(0.0, -0.5, 1.0), 0.0)
	testing.expect_value(t, tl.bound(0.0, 1.5, 1.0), 1.0)
	testing.expect_value(t, tl.bound(-1.0, -2.0, 1.0), -1.0)
	testing.expect_value(t, tl.bound(0, 5, 10), 5)
}

@(test)
test_floor_ceil :: proc(t: ^testing.T) {
	testing.expect_value(t, tl.floor(1.567, 2), 1.56)
	testing.expect_value(t, tl.ceil(1.561, 2), 1.57)
	testing.expect_value(t, tl.floor(1.9), 1.0)
	testing.expect_value(t, tl.ceil(1.1), 2.0)
}

@(test)
test_integer_round_1st_digit :: proc(t: ^testing.T) {
	testing.expect_value(t, tl.integer_round_1st_digit(123), 120)
	testing.expect_value(t, tl.integer_round_1st_digit(125), 130)
	testing.expect_value(t, tl.integer_round_1st_digit(-123), -120)
	testing.expect_value(t, tl.integer_round_1st_digit(-125), -130)
	testing.expect_value(t, tl.integer_round_1st_digit(0), 0)
}

@(test)
test_median_and_average :: proc(t: ^testing.T) {
	odd := []f64{1, 2, 3}
	even := []f64{1, 2, 3, 4}
	testing.expect_value(t, tl.median(odd), 2.0)
	testing.expect_value(t, tl.median(even), 2.5)
	testing.expect_value(t, tl.median([]f64{}), 0.0)
	testing.expect_value(t, tl.average(odd), 2.0)
	testing.expect_value(t, tl.average([]f64{}), 0.0)
}

@(test)
test_min_max :: proc(t: ^testing.T) {
	lo, hi := tl.min_max([]f64{3, 1, 4, 1, 5})
	testing.expect_value(t, lo, 1.0)
	testing.expect_value(t, hi, 5.0)
}

@(test)
test_split_string :: proc(t: ^testing.T) {
	v := tl.split_string("a,b,c", ",")
	defer tl.delete_strings(v)
	testing.expect_value(t, len(v), 3)
	testing.expect_value(t, v[0], "a")
	testing.expect_value(t, v[2], "c")

	// empty strings removed by default
	v2 := tl.split_string("a,,b", ",")
	defer tl.delete_strings(v2)
	testing.expect_value(t, len(v2), 2)

	// ... and kept when asked
	v3 := tl.split_string("a,,b", ",", false)
	defer tl.delete_strings(v3)
	testing.expect_value(t, len(v3), 3)
	testing.expect_value(t, v3[1], "")

	// trailing separator: the trailing empty element is dropped
	v4 := tl.split_string("a,b,", ",")
	defer tl.delete_strings(v4)
	testing.expect_value(t, len(v4), 2)
}

@(test)
test_split_string_delim :: proc(t: ^testing.T) {
	// separators inside a delimiter region are not split points
	v := tl.split_string_delim("a,[b,c],d", ",", "[", "]")
	defer tl.delete_strings(v)
	testing.expect_value(t, len(v), 3)
	testing.expect_value(t, v[1], "[b,c]")

	v2 := tl.split_string_delim("a,[b,c],d", ",", "[", "]", true)
	defer tl.delete_strings(v2)
	testing.expect_value(t, v2[1], "b,c")
}

@(test)
test_stob :: proc(t: ^testing.T) {
	testing.expect_value(t, tl.stob("true"), true)
	testing.expect_value(t, tl.stob("TRUE"), true)
	testing.expect_value(t, tl.stob("1"), true)
	testing.expect_value(t, tl.stob("false"), false)
	testing.expect_value(t, tl.stob("0"), false)
	testing.expect_value(t, tl.stob(""), false)
	testing.expect_value(t, tl.stob("", true), true)
	testing.expect_value(t, tl.stob("xyz", true), true)
}

@(test)
test_rim_right :: proc(t: ^testing.T) {
	a := tl.rim_right("a/b/c///", "/\\")
	defer delete(a)
	testing.expect_value(t, a, "a/b/c")

	b := tl.rim_right("abc", "/\\")
	defer delete(b)
	testing.expect_value(t, b, "abc")

	c := tl.rim_right("", "/\\")
	defer delete(c)
	testing.expect_value(t, c, "")
}

@(test)
test_is_absolute_path :: proc(t: ^testing.T) {
	testing.expect(t, tl.is_absolute_path("/usr/local"))
	testing.expect(t, tl.is_absolute_path("C:\\Users"))
	testing.expect(t, tl.is_absolute_path("C:/Users"))
	testing.expect(t, !tl.is_absolute_path("relative/path"))
	testing.expect(t, !tl.is_absolute_path("./relative"))
	// the C++ would throw on this one (path.at(2) out of range); we return false
	testing.expect(t, !tl.is_absolute_path("C:"))
	testing.expect(t, !tl.is_absolute_path(""))
}

@(test)
test_split_path_to_file :: proc(t: ^testing.T) {
	p, f := tl.split_path_to_file("installer/Hohenfinow2/sim-min.json")
	defer delete(p)
	defer delete(f)
	testing.expect_value(t, p, "installer/Hohenfinow2/")
	testing.expect_value(t, f, "sim-min.json")

	p2, f2 := tl.split_path_to_file("sim-min.json")
	defer delete(p2)
	defer delete(f2)
	testing.expect_value(t, p2, "")
	testing.expect_value(t, f2, "sim-min.json")
}

@(test)
test_fix_system_separator :: proc(t: ^testing.T) {
	s := tl.fix_system_separator("a/b//c")
	defer delete(s)
	when ODIN_OS == .Windows {
		testing.expect_value(t, s, "a\\b\\c")
	} else {
		testing.expect_value(t, s, "a/b/c")
	}
}

@(test)
test_replace_env_vars :: proc(t: ^testing.T) {
	// an unset variable is left in place and the scan continues past it
	s := tl.replace_env_vars("${__MONICA_DEFINITELY_UNSET__}/soil/")
	defer delete(s)
	testing.expect_value(t, s, "${__MONICA_DEFINITELY_UNSET__}/soil/")

	// no variables at all
	s2 := tl.replace_env_vars("plain/path")
	defer delete(s2)
	testing.expect_value(t, s2, "plain/path")
}

@(test)
test_errors :: proc(t: ^testing.T) {
	es: tl.Errors // zero value must behave like a default-constructed C++ Errors
	testing.expect(t, tl.success(es))
	testing.expect(t, !tl.failure(es))

	tl.append_warning(&es, "just a warning")
	testing.expect(t, tl.success(es)) // warnings do not make it a failure

	tl.append_error(&es, "a real error")
	testing.expect(t, tl.failure(es))
	testing.expect_value(t, len(es.errors), 1)
	testing.expect_value(t, len(es.warnings), 1)

	other: tl.Errors
	tl.append_error(&other, "another")
	tl.append_errors(&es, other)
	testing.expect_value(t, len(es.errors), 2)

	tl.errors_destroy(&es)
	tl.errors_destroy(&other)
}

@(test)
test_eresult :: proc(t: ^testing.T) {
	r: tl.EResult(f64)
	r.result = 1.5
	testing.expect(t, tl.success(r.errs))
	testing.expect_value(t, r.result, 1.5)

	tl.append_error(&r.errs, "boom")
	testing.expect(t, tl.failure(r.errs))
	tl.errors_destroy(&r.errs)
}
