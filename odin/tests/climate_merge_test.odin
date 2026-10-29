// data_accessor_merge_climate_data (support/climate), merged by climate
// element as in the C++ (mas_cpp_misc 96be5bc, plan-climate-merge.md).
//
// Small hand-made accessors. A value encodes where it comes from:
// base + 100 * element + day, with day 0 = 2000-01-10, base 0 for the data
// merged into and 10000 for the merged-in data - so a value read at the wrong
// column or day shows up as such.
package tests

import "core:testing"
import clim "../support/climate"
import d "../support/date"

@(private = "file")
day0 :: proc() -> d.Date {
	return d.make_date(10, 1, 2000)
}

@(private = "file")
date_of :: proc(day: int) -> d.Date {
	return day >= 0 ? d.add(day0(), u64(day)) : d.sub(day0(), u64(-day))
}

@(private = "file")
value :: proc(base: f64, acd: clim.ACD, day: int) -> f64 {
	return base + 100 * f64(int(acd)) + f64(day)
}

// An accessor for days [from, from + n), with the elements in the given order.
@(private = "file")
mk :: proc(from, n: int, acds: []clim.ACD, base: f64) -> clim.Data_Accessor {
	da := clim.make_data_accessor_range(date_of(from), date_of(from + n - 1))
	for acd in acds {
		vs := make([]f64, n)
		for i in 0 ..< n {
			vs[i] = value(base, acd, from + i)
		}
		clim.data_accessor_add_climate_data(&da, acd, vs)
	}
	return da
}

@(private = "file")
ALL := []clim.ACD{.tmin, .tavg, .tmax, .precip, .globrad}

// The expectation for one element: days [from, to] from `base` (0 or 10000);
// `from_other` lists the day ranges taken from the merged-in data.
@(private = "file")
expect_element :: proc(
	t: ^testing.T,
	da: ^clim.Data_Accessor,
	acd: clim.ACD,
	from, to: int,
	from_other: [][2]int = nil,
	loc := #caller_location,
) {
	got := clim.data_accessor_data_as_vector(da, acd)
	if !testing.expectf(t, len(got) == to - from + 1, "%v: %d values, want %d", acd, len(got), to - from + 1, loc = loc) {
		return
	}
	for day in from ..= to {
		base := 0.0
		for r in from_other {
			if day >= r[0] && day <= r[1] {
				base = 10000
			}
		}
		want := value(base, acd, day)
		if got[day - from] != want {
			testing.expectf(t, false, "%v day %d: %v, want %v", acd, day, got[day - from], want, loc = loc)
			return
		}
	}
}

@(private = "file")
expect_range :: proc(t: ^testing.T, da: ^clim.Data_Accessor, from, to: int, loc := #caller_location) {
	testing.expectf(
		t,
		d.eq(da.start_date, date_of(from)) && d.eq(da.end_date, date_of(to)) && da.number_of_steps == to - from + 1,
		"range %v - %v (%d steps), want days %d - %d",
		da.start_date,
		da.end_date,
		da.number_of_steps,
		from,
		to,
		loc = loc,
	)
}

@(test)
test_merge_adds_an_element_covering_the_range :: proc(t: ^testing.T) {
	context.allocator = context.temp_allocator
	da := mk(0, 10, ALL, 0)
	clim.data_accessor_merge_climate_data(&da, mk(0, 10, {.co2}, 10000))
	expect_range(t, &da, 0, 9)
	expect_element(t, &da, .co2, 0, 9, {{0, 9}})
	for acd in ALL {
		expect_element(t, &da, acd, 0, 9)
	}
}

@(test)
test_merge_replaces_some_elements_for_part_of_the_range :: proc(t: ^testing.T) {
	context.allocator = context.temp_allocator
	da := mk(0, 10, ALL, 0)
	clim.data_accessor_merge_climate_data(&da, mk(3, 3, {.tmin, .tavg, .tmax}, 10000))
	expect_range(t, &da, 0, 9)
	for acd in ([]clim.ACD{.tmin, .tavg, .tmax}) {
		expect_element(t, &da, acd, 0, 9, {{3, 5}})
	}
	expect_element(t, &da, .precip, 0, 9)
	expect_element(t, &da, .globrad, 0, 9)
}

@(test)
test_merge_appends_and_prepends_complete_data :: proc(t: ^testing.T) {
	context.allocator = context.temp_allocator
	da := mk(0, 10, ALL, 0)
	clim.data_accessor_merge_climate_data(&da, mk(10, 5, ALL, 10000))
	expect_range(t, &da, 0, 14)
	clim.data_accessor_merge_climate_data(&da, mk(-4, 4, ALL, 10000))
	expect_range(t, &da, -4, 14)
	for acd in ALL {
		expect_element(t, &da, acd, -4, 14, {{-4, -1}, {10, 14}})
	}
}

@(test)
test_merge_extends_over_and_beyond_the_range :: proc(t: ^testing.T) {
	context.allocator = context.temp_allocator
	da := mk(0, 10, ALL, 0)
	// overlapping both ends, with all elements: other replaces and extends
	clim.data_accessor_merge_climate_data(&da, mk(-2, 15, ALL, 10000))
	expect_range(t, &da, -2, 12)
	for acd in ALL {
		expect_element(t, &da, acd, -2, 12, {{-2, 12}})
	}
}

@(test)
test_merge_skips_an_element_not_covering_the_range :: proc(t: ^testing.T) {
	context.allocator = context.temp_allocator
	da := mk(0, 10, ALL, 0)
	clim.data_accessor_merge_climate_data(&da, mk(3, 3, {.co2}, 10000))
	expect_range(t, &da, 0, 9)
	testing.expect(t, !clim.data_accessor_has_available_climate_data(&da, .co2))
	for acd in ALL {
		expect_element(t, &da, acd, 0, 9)
	}
}

@(test)
test_merge_doesnt_extend_with_data_lacking_elements :: proc(t: ^testing.T) {
	context.allocator = context.temp_allocator
	da := mk(0, 10, ALL, 0)
	// days 6..15, tmin only: no extension, but days 6..9 of tmin are replaced
	clim.data_accessor_merge_climate_data(&da, mk(6, 10, {.tmin}, 10000))
	expect_range(t, &da, 0, 9)
	expect_element(t, &da, .tmin, 0, 9, {{6, 9}})
	expect_element(t, &da, .tmax, 0, 9)
}

@(test)
test_merge_matches_elements_not_columns :: proc(t: ^testing.T) {
	context.allocator = context.temp_allocator
	da := mk(0, 10, ALL, 0)
	// the same elements in a different column order, and one more
	other := mk(0, 10, {.globrad, .co2, .precip, .tmax, .tavg, .tmin}, 10000)
	clim.data_accessor_merge_climate_data(&da, other)
	for acd in ([]clim.ACD{.tmin, .tavg, .tmax, .precip, .globrad, .co2}) {
		expect_element(t, &da, acd, 0, 9, {{0, 9}})
	}
}

@(test)
test_merge_honours_views :: proc(t: ^testing.T) {
	context.allocator = context.temp_allocator
	full := mk(0, 10, ALL, 0)
	view := clim.data_accessor_clone_for_range(&full, 2, 5) // days 2..6
	expect_range(t, &view, 2, 6)
	other_full := mk(-3, 20, {.co2, .tmin}, 10000)
	other := clim.data_accessor_clone_for_range(&other_full, 7, 3) // days 4..6
	clim.data_accessor_merge_climate_data(&view, other)
	expect_range(t, &view, 2, 6)
	expect_element(t, &view, .tmin, 2, 6, {{4, 6}})
	expect_element(t, &view, .tmax, 2, 6)
	// co2 covers only days 4..6 of 2..6
	testing.expect(t, !clim.data_accessor_has_available_climate_data(&view, .co2))
	// the original isn't changed
	expect_element(t, &full, .tmin, 0, 9)
}

@(test)
test_merge_keeps_overlapping_data_if_asked :: proc(t: ^testing.T) {
	context.allocator = context.temp_allocator
	da := mk(0, 10, ALL, 0)
	clim.data_accessor_merge_climate_data(&da, mk(5, 10, ALL, 10000), replace_overlapping_data = false)
	expect_range(t, &da, 0, 14)
	for acd in ALL {
		expect_element(t, &da, acd, 0, 14, {{10, 14}})
	}
}

@(test)
test_merge_leaves_a_gap_alone :: proc(t: ^testing.T) {
	context.allocator = context.temp_allocator
	da := mk(0, 10, ALL, 0)
	clim.data_accessor_merge_climate_data(&da, mk(11, 5, ALL, 10000))
	expect_range(t, &da, 0, 9)
	clim.data_accessor_merge_climate_data(&da, mk(-6, 5, ALL, 10000))
	expect_range(t, &da, 0, 9)
	for acd in ALL {
		expect_element(t, &da, acd, 0, 9)
	}
}

@(test)
test_merge_into_nothing_takes_the_other :: proc(t: ^testing.T) {
	context.allocator = context.temp_allocator
	da := clim.make_data_accessor()
	clim.data_accessor_merge_climate_data(&da, mk(0, 10, {.tmin}, 10000))
	expect_range(t, &da, 0, 9)
	expect_element(t, &da, .tmin, 0, 9, {{0, 9}})
}
