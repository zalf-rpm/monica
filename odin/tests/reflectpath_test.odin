// Unit tests for support/reflectpath - the generic path engine.
//
// These run against a hand-built fixture rather than against Monica_Model on
// purpose: the engine has to be correct for shapes the model happens not to
// contain today (empty containers, unset Maybes, out-of-range indices), and a
// fixture is the only way to put those states on the table. The "does every
// legacy alias actually resolve on the real model" question is a separate
// test - output_paths_test.odin.
package tests

import "base:runtime"
import "core:reflect"
import "core:testing"
import rp "../support/reflectpath"

@(private = "file")
Key :: enum {
	tavg,
	precip,
	globrad,
}

@(private = "file")
Pool :: struct {
	rate: f64,
}

@(private = "file")
Layer :: struct {
	moisture: f64,
	nh4:      f64,
	pools:    [dynamic]Pool,
	organs:   [3]f64,
}

@(private = "file")
Column :: struct {
	layers: [dynamic]Layer,
}

@(private = "file")
Crop :: struct {
	lai:    f64,
	column: ^Column, // back-pointer, the shape soilMoisture.soilColumn has
}

@(private = "file")
Model :: struct {
	count:   int,
	evap:    f64,
	column:  Column,
	crop:    ^Crop, // nil when nothing is planted
	opt:     Maybe(f64),
	climate: [dynamic]map[Key]f64,
	events:  map[string]bool,
	slice:   []f64,
}

@(private = "file")
make_model :: proc(allocator := context.allocator) -> ^Model {
	m := new(Model, allocator)
	m.count = 4
	m.evap = 1.25

	m.column.layers = make([dynamic]Layer, 3, allocator)
	for i in 0 ..< 3 {
		l := &m.column.layers[i]
		l.moisture = 0.1 + 0.05 * f64(i)
		l.nh4 = 0.5 + f64(i)
		l.pools = make([dynamic]Pool, 2, allocator)
		l.pools[0].rate = 7 + f64(i)
		l.pools[1].rate = 70 + f64(i)
		l.organs = {1 + f64(i), 10 + f64(i), 100 + f64(i)}
	}

	m.crop = new(Crop, allocator)
	m.crop.lai = 3.5
	m.crop.column = &m.column

	m.climate = make([dynamic]map[Key]f64, 0, 4, allocator)
	for day in 0 ..< 3 {
		cd := make(map[Key]f64, allocator)
		cd[.tavg] = 10 + f64(day)
		cd[.precip] = f64(day)
		append(&m.climate, cd)
	}

	m.events = make(map[string]bool, allocator)
	m.events["Sowing"] = true

	sl := make([]f64, 2, allocator)
	sl[0], sl[1] = 11, 22
	m.slice = sl
	return m
}

@(private = "file")
must_compile :: proc(t: ^testing.T, path: string) -> ^rp.Path_Plan {
	plan, err := rp.compile(typeid_of(Model), path, context.temp_allocator)
	if !testing.expect(t, !rp.failed(err), path) {
		return nil
	}
	p := new_clone(plan, context.temp_allocator)
	return p
}

// value at `path` with the open dimension at `i`, or a sentinel if missing
@(private = "file")
get :: proc(t: ^testing.T, m: ^Model, path: string, i := 0) -> (f64, bool) {
	plan := must_compile(t, path)
	if plan == nil {
		return 0, false
	}
	return rp.resolve_f64(plan, m^, i)
}

@(test)
test_reflectpath_scalar_fields :: proc(t: ^testing.T) {
	m := make_model(context.temp_allocator)

	v, ok := get(t, m, "count")
	testing.expect(t, ok)
	testing.expect_value(t, v, 4.0)

	v, ok = get(t, m, "evap")
	testing.expect(t, ok)
	testing.expect_value(t, v, 1.25)

	// through a pointer
	v, ok = get(t, m, "crop.lai")
	testing.expect(t, ok)
	testing.expect_value(t, v, 3.5)

	// through a pointer and back into the owner - the soilMoisture.soilColumn
	// back-pointer shape. Compared against the field itself, not against a
	// recomputed literal: 0.1 + 0.05*2 is not 0.15 in binary floating point.
	v, ok = get(t, m, "crop.column.layers.1.moisture")
	testing.expect(t, ok)
	testing.expect_value(t, v, m.column.layers[1].moisture)
}

@(test)
test_reflectpath_index_forms_are_equivalent :: proc(t: ^testing.T) {
	m := make_model(context.temp_allocator)
	a, ok_a := get(t, m, "column.layers.2.nh4")
	b, ok_b := get(t, m, "column.layers[2].nh4")
	testing.expect(t, ok_a && ok_b)
	testing.expect_value(t, a, 2.5)
	testing.expect_value(t, b, a)

	// nested: dynamic array inside the element of a dynamic array
	v, ok := get(t, m, "column.layers[0].pools[1].rate")
	testing.expect(t, ok)
	testing.expect_value(t, v, 70.0)

	// fixed array
	v, ok = get(t, m, "column.layers.0.organs.2")
	testing.expect(t, ok)
	testing.expect_value(t, v, 100.0)
}

@(test)
test_reflectpath_open_dimension :: proc(t: ^testing.T) {
	m := make_model(context.temp_allocator)

	plan := must_compile(t, "column.layers.nh4")
	testing.expect(t, plan != nil)
	testing.expect(t, plan.open_step >= 0)
	testing.expect_value(t, rp.open_length(plan, m^), 3)
	for i in 0 ..< 3 {
		v, ok := rp.resolve_f64(plan, m^, i)
		testing.expect(t, ok)
		testing.expect_value(t, v, m.column.layers[i].nh4)
	}
	// out of range is *missing*, not a panic - this is where the engine is
	// deliberately safer than the C++ `layers.at(i)`, which throws
	_, ok := rp.resolve_f64(plan, m^, 3)
	testing.expect(t, !ok)
	_, ok = rp.resolve_f64(plan, m^, -1)
	testing.expect(t, !ok)

	// the open segment may be mid-path, with a fixed index after it
	deep := must_compile(t, "column.layers.pools.0.rate")
	testing.expect(t, deep != nil)
	testing.expect_value(t, rp.open_length(deep, m^), 3)
	for i in 0 ..< 3 {
		v, vok := rp.resolve_f64(deep, m^, i)
		testing.expect(t, vok)
		testing.expect_value(t, v, 7 + f64(i))
	}

	// a path ENDING on an array opens it - the OrgBiom/organ shape
	organs := must_compile(t, "column.layers.0.organs")
	testing.expect(t, organs != nil)
	testing.expect_value(t, rp.open_length(organs, m^), 3)
	v, vok := rp.resolve_f64(organs, m^, 1)
	testing.expect(t, vok)
	testing.expect_value(t, v, 10.0)

	// a scalar path has no open dimension
	scalar := must_compile(t, "evap")
	testing.expect_value(t, scalar.open_step, -1)
	testing.expect_value(t, rp.open_length(scalar, m^), 0)
}

@(test)
test_reflectpath_two_open_dimensions_rejected :: proc(t: ^testing.T) {
	// write_output flattens one level only, so two unindexed arrays must be
	// a setup-time error rather than a nested CSV cell
	_, err := rp.compile(typeid_of(Model), "column.layers.pools.rate", context.temp_allocator)
	testing.expect_value(t, err.kind, rp.Error_Kind.TWO_OPEN_DIMENSIONS)

	// ... including when the second one is the trailing container
	_, err2 := rp.compile(typeid_of(Model), "column.layers.pools", context.temp_allocator)
	testing.expect_value(t, err2.kind, rp.Error_Kind.TWO_OPEN_DIMENSIONS)
}

@(test)
test_reflectpath_missing_values :: proc(t: ^testing.T) {
	m := make_model(context.temp_allocator)

	// nil pointer: detected, not dereferenced
	m.crop = nil
	_, ok := get(t, m, "crop.lai")
	testing.expect(t, !ok)

	// unset Maybe must not collapse to the zero value (CONVENTIONS §5)
	_, ok = get(t, m, "opt")
	testing.expect(t, !ok)
	m.opt = 2.5
	v, ok2 := get(t, m, "opt")
	testing.expect(t, ok2)
	testing.expect_value(t, v, 2.5)

	// out-of-range fixed index
	_, ok = get(t, m, "column.layers.9.nh4")
	testing.expect(t, !ok)
	_, ok = get(t, m, "column.layers.0.organs.7")
	testing.expect(t, !ok)
}

@(test)
test_reflectpath_len_first_last :: proc(t: ^testing.T) {
	m := make_model(context.temp_allocator)

	v, ok := get(t, m, "column.layers.#len")
	testing.expect(t, ok)
	testing.expect_value(t, v, 3.0)

	// the legacy noOfAOMPools shape: #len of a nested array under a fixed index
	v, ok = get(t, m, "column.layers.0.pools.#len")
	testing.expect(t, ok)
	testing.expect_value(t, v, 2.0)

	v, ok = get(t, m, "column.layers.#first.nh4")
	testing.expect(t, ok)
	testing.expect_value(t, v, 0.5)

	v, ok = get(t, m, "column.layers.#last.nh4")
	testing.expect(t, ok)
	testing.expect_value(t, v, 2.5)

	// slices work the same as dynamic arrays
	v, ok = get(t, m, "slice.#last")
	testing.expect(t, ok)
	testing.expect_value(t, v, 22.0)
	v, ok = get(t, m, "slice.#len")
	testing.expect(t, ok)
	testing.expect_value(t, v, 2.0)

	// #first/#last on an empty container is missing, not a fault - this is
	// the climateData-on-day-0 case
	clear(&m.column.layers)
	_, ok = get(t, m, "column.layers.#last.nh4")
	testing.expect(t, !ok)
	_, ok = get(t, m, "column.layers.#first.nh4")
	testing.expect(t, !ok)
	v, ok = get(t, m, "column.layers.#len")
	testing.expect(t, ok)
	testing.expect_value(t, v, 0.0)
}

@(test)
test_reflectpath_len_must_be_terminal :: proc(t: ^testing.T) {
	_, err := rp.compile(typeid_of(Model), "column.layers.#len.nh4", context.temp_allocator)
	testing.expect_value(t, err.kind, rp.Error_Kind.LEN_NOT_TERMINAL)
}

@(test)
test_reflectpath_maps :: proc(t: ^testing.T) {
	m := make_model(context.temp_allocator)

	// enum key, resolved by name at compile time
	v, ok := get(t, m, "climate.#last.tavg")
	testing.expect(t, ok)
	testing.expect_value(t, v, 12.0)
	v, ok = get(t, m, "climate.#first.tavg")
	testing.expect(t, ok)
	testing.expect_value(t, v, 10.0)

	// absent key degrades to missing -> the caller's 0.0, which is exactly
	// the C++ `cd.find(k) == cd.end() ? 0.0 : ...`
	_, ok = get(t, m, "climate.#last.globrad")
	testing.expect(t, !ok)

	// string key
	v, ok = get(t, m, "events.Sowing")
	testing.expect(t, ok)
	testing.expect_value(t, v, 1.0)
	_, ok = get(t, m, "events.Harvest")
	testing.expect(t, !ok)

	// #last on an empty climate series
	clear(&m.climate)
	_, ok = get(t, m, "climate.#last.tavg")
	testing.expect(t, !ok)
}

@(test)
test_reflectpath_map_compile_errors :: proc(t: ^testing.T) {
	// an enum key that is not a member of the key enum is a SETUP error, not
	// a silent 0.0 every day
	_, err := rp.compile(typeid_of(Model), "climate.#last.wind", context.temp_allocator)
	testing.expect_value(t, err.kind, rp.Error_Kind.UNKNOWN_MAP_KEY)

	// a map may not be the open dimension: Odin map iteration order is
	// unstable, so "all keys" would emit CSV columns in a random order
	_, err2 := rp.compile(typeid_of(Model), "climate.#last", context.temp_allocator)
	testing.expect_value(t, err2.kind, rp.Error_Kind.MAP_NEEDS_KEY)
	_, err3 := rp.compile(typeid_of(Model), "events", context.temp_allocator)
	testing.expect_value(t, err3.kind, rp.Error_Kind.MAP_NEEDS_KEY)
}

// The hashed lookup reaches into base:runtime map internals, so it is pinned
// against the reflection-level linear scan: if a compiler bump changes the
// hasher or the cell layout, this fails loudly instead of silently returning
// "key absent" for every climate value.
@(test)
test_reflectpath_hashed_lookup_agrees_with_scan :: proc(t: ^testing.T) {
	m := make_model(context.temp_allocator)
	cd := &m.climate[len(m.climate) - 1]

	for k in Key {
		want, want_ok := rp.map_lookup_scan(cd^, k)
		got, got_ok := get(
			t,
			m,
			k == .tavg ? "climate.#last.tavg" : k == .precip ? "climate.#last.precip" : "climate.#last.globrad",
		)
		testing.expect_value(t, got_ok, want_ok)
		if want_ok && got_ok {
			wf, _ := reflect.as_f64(want)
			testing.expect_value(t, got, wf)
		}
	}

	// and for the string-keyed map
	for key in ([]string{"Sowing", "Harvest"}) {
		want, want_ok := rp.map_lookup_scan(m.events, key)
		plan, cerr := rp.compile(
			typeid_of(Model),
			key == "Sowing" ? "events.Sowing" : "events.Harvest",
			context.temp_allocator,
		)
		testing.expect(t, !rp.failed(cerr))
		_, got_ok := rp.resolve(&plan, m^)
		testing.expect_value(t, got_ok, want_ok)
		if want_ok {
			wb, _ := reflect.as_bool(want)
			testing.expect(t, wb)
		}
	}
}

@(test)
test_reflectpath_syntax_errors :: proc(t: ^testing.T) {
	bad := []struct {
		path: string,
		kind: rp.Error_Kind,
	} {
		{"", .EMPTY_PATH},
		{"evap.", .BAD_SYNTAX},
		{".evap", .BAD_SYNTAX},
		{"column..layers", .BAD_SYNTAX},
		{"column.layers[", .BAD_SYNTAX},
		{"column.layers[]", .BAD_SYNTAX},
		{"nope", .UNKNOWN_FIELD},
		{"column.layers.nope", .UNKNOWN_FIELD},
		{"evap.nope", .NOT_TRAVERSABLE},
	}
	for b in bad {
		_, err := rp.compile(typeid_of(Model), b.path, context.temp_allocator)
		testing.expectf(t, err.kind == b.kind, "%q: got %v, want %v", b.path, err.kind, b.kind)
	}
}

// resolve() returns an `any` aimed at the real field, so the SetValue
// workstep can write through the same plan the output path reads through
// (plan-reflective-outputs.md §2.7).
@(test)
test_reflectpath_write_through :: proc(t: ^testing.T) {
	m := make_model(context.temp_allocator)
	plan := must_compile(t, "column.layers.moisture")
	testing.expect(t, plan != nil)

	a, ok := rp.resolve(plan, m^, 2)
	testing.expect(t, ok)
	(^f64)(a.data)^ = 9.75
	testing.expect_value(t, m.column.layers[2].moisture, 9.75)

	v, vok := rp.resolve_f64(plan, m^, 2)
	testing.expect(t, vok)
	testing.expect_value(t, v, 9.75)
}

@(test)
test_reflectpath_leaf_typing :: proc(t: ^testing.T) {
	numeric := []string {
		"count",
		"evap",
		"column.layers.nh4",
		"column.layers.#len",
		"climate.#last.tavg",
		"events.Sowing",
	}
	for p in numeric {
		plan := must_compile(t, p)
		if plan != nil {
			testing.expectf(t, rp.leaf_is_numeric(plan), "%q leaf %v", p, plan.leaf)
		}
	}
	// the whole struct is reachable but is not something the CSV can print
	plan := must_compile(t, "column.layers.0")
	testing.expect(t, plan != nil)
	testing.expect(t, !rp.leaf_is_numeric(plan))
}

_ :: runtime
