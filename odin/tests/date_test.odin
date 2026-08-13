package tests

import "core:testing"
import d "../support/date"

// Ground truth: a 1:1 port of Tools::testDate() from
// mas_cpp_misc/tools/date.cpp. Every assertion here comes from the C++ source
// unchanged - do not adjust them to make the Odin pass.
@(test)
test_date_cpp_testDate :: proc(t: ^testing.T) {
	D :: d.make_date

	testing.expect_value(t, d.number_of_days_to(D(1, 1, 2001), D(2, 1, 2001)), 1)
	testing.expect_value(t, d.number_of_days_to(D(1, 1, 2001), D(1, 1, 2001)), 0)
	testing.expect_value(t, d.number_of_days_to(D(1, 1, 2001), D(1, 2, 2001)), 31)

	testing.expect(t, d.eq(D(1, 1, 2001), D(1, 1, 2001)))
	testing.expect(t, d.gt(D(1, 1, 2001), D(31, 12, 2000)))
	testing.expect(t, d.lt(D(1, 1, 2001), D(2, 1, 2001)))
	testing.expect(t, d.ge(D(1, 1, 2001), D(1, 1, 2001)))
	testing.expect(t, d.ge(D(1, 1, 2001), D(31, 12, 2000)))
	testing.expect(t, d.le(D(1, 1, 2001), D(1, 1, 2001)))
	testing.expect(t, d.le(D(1, 1, 2001), D(2, 1, 2001)))

	// The remainder of the C++ testDate() starts `Date t(5, 3, 200, true);` - a
	// RELATIVE date with year 200 - and asserts e.g. `t - 5 == Date(29, 2, 2008)`.
	// Those assertions are simply false: operator== compares both the year
	// (200 vs 2008) and isRelativeDate (true vs false). Worked through by hand,
	// `t - 5` is Date(28, 2, 200, relative) - 28, not 29, because year 200 is not
	// a leap year under the `y%4==0 && (y%100!=0 || y%400==0)` test.
	//
	// testDate() is dead code: `assert` compiles out under NDEBUG in the release
	// build, so this was never exercised. It is therefore NOT ported as-is;
	// test_date_arithmetic below re-anchors the same day offsets on absolute
	// dates, where they do hold and do verify the arithmetic.
	tt := D(5, 3, 200, true)
	testing.expect(t, d.is_relative_date(tt))
	testing.expect(t, d.eq(d.sub(tt, 5), D(28, 2, 200, true)))
}

// The date arithmetic from testDate(), restated against same-relativity operands
// so operator== is meaningful. The day/month/year values are exactly the C++ ones.
@(test)
test_date_arithmetic :: proc(t: ^testing.T) {
	D :: d.make_date

	// C++: t = Date(25, 2, 2008); t + N
	tt := D(25, 2, 2008)
	testing.expect(t, d.eq(d.add(tt, 5), D(1, 3, 2008)))
	testing.expect(t, d.eq(d.add(tt, 10), D(6, 3, 2008)))
	testing.expect(t, d.eq(d.add(tt, 20), D(16, 3, 2008)))
	testing.expect(t, d.eq(d.add(tt, 30), D(26, 3, 2008)))
	testing.expect(t, d.eq(d.add(tt, 100), D(4, 6, 2008)))
	testing.expect(t, d.eq(d.add(tt, 300), D(21, 12, 2008)))
	testing.expect(t, d.eq(d.add(tt, 400), D(31, 3, 2009)))
	testing.expect(t, d.eq(d.add(tt, 1000), D(21, 11, 2010)))

	// The subtraction series from testDate(), anchored at the same absolute date
	// the relative 5.3.<year> resolves to in a leap year (2008).
	ts := D(5, 3, 2008)
	testing.expect(t, d.eq(d.sub(ts, 5), D(29, 2, 2008)))
	testing.expect(t, d.eq(d.sub(ts, 10), D(24, 2, 2008)))
	testing.expect(t, d.eq(d.sub(ts, 20), D(14, 2, 2008)))
	testing.expect(t, d.eq(d.sub(ts, 30), D(4, 2, 2008)))
	testing.expect(t, d.eq(d.sub(ts, 100), D(26, 11, 2007)))
	testing.expect(t, d.eq(d.sub(ts, 300), D(10, 5, 2007)))
	testing.expect(t, d.eq(d.sub(ts, 400), D(30, 1, 2007)))
	testing.expect(t, d.eq(d.sub(ts, 1000), D(9, 6, 2005)))
}

// The zero value of Date must equal C++ `Tools::Date()`: invalid, leap years on,
// and the LEAP days-in-month table selected (year 0 passes the leap-year test).
// This is what makes `Date` usable as a plain struct field. See date.odin.
@(test)
test_date_zero_value_matches_cpp_default :: proc(t: ^testing.T) {
	zero: d.Date
	def := d.make_default_date()

	testing.expect(t, !d.is_valid(zero))
	testing.expect_value(t, d.use_leap_years(zero), true)
	testing.expect_value(t, d.use_leap_years(def), true)
	testing.expect_value(t, d.is_relative_date(zero), false)
	// February from the leap table
	testing.expect_value(t, d.days_in_month(zero, 2), 29)
	testing.expect_value(t, d.days_in_month(def, 2), 29)
}

@(test)
test_date_leap_year :: proc(t: ^testing.T) {
	testing.expect(t, d.is_leap_year(d.make_date(1, 1, 2008)))
	testing.expect(t, d.is_leap_year(d.make_date(1, 1, 2000)))
	testing.expect(t, !d.is_leap_year(d.make_date(1, 1, 1900)))
	testing.expect(t, !d.is_leap_year(d.make_date(1, 1, 2007)))
	// year 0 is a leap year under this test - matters for relative dates
	testing.expect(t, d.is_leap_year(d.make_date(1, 1, 0, true)))

	testing.expect_value(t, d.days_in_month(d.make_date(1, 1, 2008), 2), 29)
	testing.expect_value(t, d.days_in_month(d.make_date(1, 1, 2007), 2), 28)
	// leap years disabled -> always the common-year table
	testing.expect_value(t, d.days_in_month(d.make_date(1, 1, 2008, false, false, false), 2), 28)
}

@(test)
test_date_invalid_construction :: proc(t: ^testing.T) {
	// day beyond the month's length invalidates the whole date
	testing.expect(t, !d.is_valid(d.make_date(30, 2, 2007)))
	testing.expect(t, d.is_valid(d.make_date(29, 2, 2008)))
	testing.expect(t, !d.is_valid(d.make_date(29, 2, 2007)))
	testing.expect(t, !d.is_valid(d.make_date(1, 13, 2007)))
	testing.expect(t, !d.is_valid(d.make_date(0, 1, 2007)))

	// create_valid_date clamps instead
	v := d.make_date(30, 2, 2007, false, true)
	testing.expect(t, d.is_valid(v))
	testing.expect_value(t, v.d, 28)
}

@(test)
test_date_iso_roundtrip :: proc(t: ^testing.T) {
	abs := d.from_iso_date_string("1991-09-22")
	testing.expect(t, d.is_valid(abs))
	testing.expect(t, d.is_absolute_date(abs))
	testing.expect_value(t, d.year(abs), 1991)
	testing.expect_value(t, abs.m, 9)
	testing.expect_value(t, abs.d, 22)
	s := d.to_iso_date_string(abs)
	defer delete(s)
	testing.expect_value(t, s, "1991-09-22")

	// a year < 100 yields a RELATIVE date, zero-padded on output
	rel := d.from_iso_date_string("0000-09-23")
	testing.expect(t, d.is_valid(rel))
	testing.expect(t, d.is_relative_date(rel))
	testing.expect_value(t, d.year(rel), 0)
	rs := d.to_iso_date_string(rel)
	defer delete(rs)
	testing.expect_value(t, rs, "0000-09-23")

	rel1 := d.from_iso_date_string("0001-08-25")
	testing.expect(t, d.is_relative_date(rel1))
	r1s := d.to_iso_date_string(rel1)
	defer delete(r1s)
	testing.expect_value(t, r1s, "0001-08-25")

	// a malformed / short string yields the invalid default
	testing.expect(t, !d.is_valid(d.from_iso_date_string("1991-09")))
}

// NOTE(c++-quirk) coverage: the string constructor drops the relative flag that
// the parse produced.
@(test)
test_date_string_ctor_drops_relative_flag :: proc(t: ^testing.T) {
	parsed := d.from_iso_date_string("0000-09-23")
	testing.expect(t, d.is_relative_date(parsed))

	ctor := d.make_date_from_iso_string("0000-09-23")
	testing.expect(t, !d.is_relative_date(ctor)) // the quirk
	testing.expect_value(t, ctor.d, 23)
	testing.expect_value(t, ctor.m, 9)
}

@(test)
test_date_julian_day :: proc(t: ^testing.T) {
	testing.expect_value(t, d.julian_day(d.make_date(1, 1, 1991)), 1)
	testing.expect_value(t, d.julian_day(d.make_date(31, 12, 1991)), 365)
	// 2008 is a leap year
	testing.expect_value(t, d.julian_day(d.make_date(31, 12, 2008)), 366)
	testing.expect_value(t, d.julian_day(d.make_date(1, 3, 2008)), 61)
	testing.expect_value(t, d.julian_day(d.make_date(1, 3, 2007)), 60)

	// julian_date is the inverse
	testing.expect(t, d.eq(d.julian_date(61, 2008), d.make_date(1, 3, 2008)))
	testing.expect(t, d.eq(d.julian_date(1, 1991), d.make_date(1, 1, 1991)))
	testing.expect(t, d.eq(d.julian_date(365, 1991), d.make_date(31, 12, 1991)))
}

// Differential sweep: walk 1990-01-01 .. 1995-12-31 one day at a time and check
// that +1, -1, julianDay and numberOfDaysTo all stay mutually consistent.
@(test)
test_date_sweep_self_consistency :: proc(t: ^testing.T) {
	start := d.make_date(1, 1, 1990)
	cur := start
	expected_doy := 1
	expected_year := 1990

	for i in 0 ..< (6 * 365 + 2) {
		// julianDay must match a manually tracked day-of-year
		if d.year(cur) != expected_year {
			expected_year = d.year(cur)
			expected_doy = 1
		}
		if !testing.expect_value(t, int(d.julian_day(cur)), expected_doy) {
			return
		}
		// distance from start must equal the number of steps taken
		if !testing.expect_value(t, d.number_of_days_to(start, cur), i) {
			return
		}
		// +1 then -1 must round-trip
		if !testing.expect(t, d.eq(d.sub(d.add(cur, 1), 1), cur)) {
			return
		}
		// diff is the inverse of add
		if !testing.expect_value(t, d.diff(d.add(cur, 37), cur), 37) {
			return
		}

		cur = d.add(cur, 1)
		expected_doy += 1
	}
}

@(test)
test_date_day_lengths :: proc(t: ^testing.T) {
	// smoke test: mid-summer at 52.5N should have a longer astronomic day than
	// mid-winter, and all three lengths must stay in a sane range
	summer := d.day_lengths(52.5, 172)
	winter := d.day_lengths(52.5, 355)
	testing.expect(t, summer.astronomic_day_length > winter.astronomic_day_length)
	testing.expect(t, summer.astronomic_day_length > 0 && summer.astronomic_day_length <= 24)
	testing.expect(t, winter.astronomic_day_length > 0 && winter.astronomic_day_length <= 24)
	testing.expect(t, summer.effective_day_length > 0)
	testing.expect(t, summer.photoperiodic_daylength > 0)
}
