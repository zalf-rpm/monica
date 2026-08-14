package tests

import "core:testing"
import d "../support/date"
import jx "../support/jsonx"
import tl "../support/tools"

// Parsed values go into the temp allocator: in real use they live in a jsonx
// arena and are never individually freed (CONVENTIONS §3), so freeing them
// per-value here would be misleading - and leaving them on context.allocator
// would fill the leak report and mask genuine leaks.
@(private = "file")
parse :: proc(t: ^testing.T, s: string) -> jx.Value {
	r := jx.parse_json_string(s, context.temp_allocator)
	testing.expect(t, tl.success(r.errs))
	return r.result
}

// The forgiving indexing that MONICA's whole merge() codebase depends on:
// j["a"]["b"] on a missing or mismatched value must yield null, never fault.
@(test)
test_jsonx_forgiving_access :: proc(t: ^testing.T) {
	v := parse(t, `{"a": {"b": 42}, "arr": [1, 2, 3], "s": "x"}`)

	testing.expect_value(t, jx.int_value_of(jx.get(jx.get(v, "a"), "b")), 42)
	testing.expect(t, jx.is_null(jx.get(v, "missing")))
	// chains through missing keys
	testing.expect(t, jx.is_null(jx.get(jx.get(v, "missing"), "alsoMissing")))
	// indexing a non-object
	testing.expect(t, jx.is_null(jx.get(jx.get(v, "s"), "b")))
	testing.expect(t, jx.is_null(jx.get(jx.get(v, "arr"), "b")))
	// get_path convenience
	testing.expect_value(t, jx.int_value_of(jx.get_path(v, "a", "b")), 42)
	testing.expect(t, jx.is_null(jx.get_path(v, "a", "nope", "deeper")))

	// array indexing, incl. out of range and non-arrays
	testing.expect_value(t, jx.int_value_of(jx.at(jx.get(v, "arr"), 1)), 2)
	testing.expect(t, jx.is_null(jx.at(jx.get(v, "arr"), 99)))
	testing.expect(t, jx.is_null(jx.at(jx.get(v, "arr"), -1)))
	testing.expect(t, jx.is_null(jx.at(jx.get(v, "s"), 0)))

	// a zero-valued Value is null, like a default-constructed json11::Json
	zero: jx.Value
	testing.expect(t, jx.is_null(zero))
	testing.expect(t, jx.is_null(jx.get(zero, "anything")))
}

// The single biggest silent-regression risk in the port: json11 reads Integer
// and Float transparently, so every accessor must accept both.
@(test)
test_jsonx_integer_float_transparency :: proc(t: ^testing.T) {
	v := parse(t, `{"i": 2, "f": 2.0, "fr": 3.7, "neg": -3.7, "big": 1234567890}`)

	// written as an int, read as a double
	testing.expect_value(t, jx.number_value(jx.get(v, "i")), 2.0)
	testing.expect_value(t, jx.double_value(v, "i"), 2.0)
	// written as a double, read as an int
	testing.expect_value(t, jx.int_value_of(jx.get(v, "f")), 2)
	testing.expect_value(t, jx.int_value(v, "f"), 2)
	// both are "numbers"
	testing.expect(t, jx.is_number(jx.get(v, "i")))
	testing.expect(t, jx.is_number(jx.get(v, "f")))

	// int_value truncates toward zero, matching static_cast<int>
	testing.expect_value(t, jx.int_value_of(jx.get(v, "fr")), 3)
	testing.expect_value(t, jx.int_value_of(jx.get(v, "neg")), -3)

	testing.expect_value(t, jx.number_value(jx.get(v, "big")), 1234567890.0)
}

// MONICA's parameter files carry [value, "unit"] pairs everywhere; scalar
// accessors take element 0, and also unwrap {"value": ...}.
@(test)
test_jsonx_value_unit_pairs :: proc(t: ^testing.T) {
	v := parse(
		t,
		`{"amount": [17, "mm"], "plain": 5, "obj": {"value": 9},
		  "flag": [true, "-"], "name": ["wheat", "-"], "objs": {"value": "deep"}}`,
	)

	testing.expect_value(t, jx.double_value(v, "amount"), 17.0)
	testing.expect_value(t, jx.int_value(v, "amount"), 17)
	testing.expect_value(t, jx.double_value(v, "plain"), 5.0)
	testing.expect_value(t, jx.int_value(v, "obj"), 9)
	testing.expect_value(t, jx.bool_value(v, "flag"), true)
	testing.expect_value(t, jx.string_value(v, "name"), "wheat")
	testing.expect_value(t, jx.string_value(v, "objs"), "deep")

	// absent key -> default
	testing.expect_value(t, jx.double_value_key_d(v, "nope", 1.25), 1.25)
	testing.expect_value(t, jx.int_value_key_d(v, "nope", 7), 7)
	testing.expect_value(t, jx.string_value_key_d(v, "nope", "def"), "def")
	testing.expect_value(t, jx.bool_value_key_d(v, "nope", true), true)
}

// set_*_value (no D suffix) passes useDefault=false, so an absent key must leave
// the target untouched rather than resetting it.
@(test)
test_jsonx_set_value_leaves_untouched :: proc(t: ^testing.T) {
	v := parse(t, `{"present": 3}`)

	dv := 99.0
	jx.set_double_value(&dv, v, "absent")
	testing.expect_value(t, dv, 99.0)
	jx.set_double_value(&dv, v, "present")
	testing.expect_value(t, dv, 3.0)

	iv := 99
	jx.set_int_value(&iv, v, "absent")
	testing.expect_value(t, iv, 99)

	sv := "keep"
	jx.set_string_value(&sv, v, "absent")
	testing.expect_value(t, sv, "keep")

	bv := true
	jx.set_bool_value(&bv, v, "absent")
	testing.expect_value(t, bv, true)

	// ... whereas the D form with useDefault=true does reset
	dv2 := 99.0
	jx.set_double_value_d(&dv2, v, "absent", 1.5)
	testing.expect_value(t, dv2, 1.5)
}

// NOTE(c++-quirk): a string that is not exactly TRUE/FALSE leaves the target
// untouched in set_bool_valueD, and falls through to `def` in bool_valueD.
@(test)
test_jsonx_bool_string_quirk :: proc(t: ^testing.T) {
	v := parse(t, `{"yes": "true", "no": "FALSE", "junk": "maybe"}`)

	testing.expect_value(t, jx.bool_value(v, "yes"), true)
	testing.expect_value(t, jx.bool_value(v, "no"), false)

	bv := true
	jx.set_bool_value_d(&bv, v, "junk", false)
	testing.expect_value(t, bv, true) // untouched, NOT set to the default

	testing.expect_value(t, jx.bool_value_key_d(v, "junk", true), true)
	testing.expect_value(t, jx.bool_value_d(jx.get(v, "junk"), false), false)
}

@(test)
test_jsonx_unit_transforms :: proc(t: ^testing.T) {
	v := parse(t, `{"pct": [90, "%"], "mm": [300, "mm"], "cm": [30, "cm"],
	                "dm": [3, "dm"], "m": [0.3, "m"], "bare": 5}`)

	testing.expect_value(t, jx.transform_if_percent(v, "pct"), jx.Transform.PERCENT)
	testing.expect_value(t, jx.transform_if_percent(v, "mm"), jx.Transform.IDENTITY)
	testing.expect_value(t, jx.transform_if_not_meters(v, "mm"), jx.Transform.MM)
	testing.expect_value(t, jx.transform_if_not_meters(v, "cm"), jx.Transform.CM)
	testing.expect_value(t, jx.transform_if_not_meters(v, "dm"), jx.Transform.DM)
	testing.expect_value(t, jx.transform_if_not_meters(v, "m"), jx.Transform.IDENTITY)
	testing.expect_value(t, jx.transform_if_not_meters(v, "bare"), jx.Transform.IDENTITY)

	// applied through set_double_value
	pv: f64
	jx.set_double_value(&pv, v, "pct", jx.transform_if_percent(v, "pct"))
	testing.expect_value(t, pv, 0.9)

	mv: f64
	jx.set_double_value(&mv, v, "mm", jx.transform_if_not_meters(v, "mm"))
	testing.expect_value(t, mv, 0.3)
}

@(test)
test_jsonx_vectors :: proc(t: ^testing.T) {
	v := parse(
		t,
		`{"ds": [1, 2.5, 3], "withUnit": [[1, 2, 3], "kg ha-1"],
		  "is": [1, 2, 3], "bs": [true, false], "ss": ["a", "b"],
		  "obj": {"value": [4, 5]}}`,
	)

	ds := jx.double_vector_d(jx.get(v, "ds"), nil)
	defer delete(ds)
	testing.expect_value(t, len(ds), 3)
	testing.expect_value(t, ds[1], 2.5)

	// [[values], "unit"] unwraps to the inner array
	wu := jx.double_vector_d(jx.get(v, "withUnit"), nil)
	defer delete(wu)
	testing.expect_value(t, len(wu), 3)
	testing.expect_value(t, wu[2], 3.0)

	// {"value": [...]} unwraps too
	ov := jx.double_vector_d(jx.get(v, "obj"), nil)
	defer delete(ov)
	testing.expect_value(t, len(ov), 2)

	is := jx.int_vector_d(jx.get(v, "is"), nil)
	defer delete(is)
	testing.expect_value(t, len(is), 3)

	bs := jx.bool_vector_d(jx.get(v, "bs"), nil)
	defer delete(bs)
	testing.expect_value(t, len(bs), 2)
	testing.expect_value(t, bs[0], true)

	ss := jx.string_vector_d(jx.get(v, "ss"), nil)
	defer delete(ss)
	testing.expect_value(t, len(ss), 2)
	testing.expect_value(t, ss[1], "b")

	// a non-array falls back to the supplied default
	def := []f64{9, 9}
	fb := jx.double_vector_d(jx.get(v, "missing"), def)
	defer delete(fb)
	testing.expect_value(t, len(fb), 2)
	testing.expect_value(t, fb[0], 9.0)
}

@(test)
test_jsonx_iso_date_value :: proc(t: ^testing.T) {
	v := parse(t, `{"seedDate": "1991-09-22", "rel": "0000-09-23"}`)

	sd := jx.iso_date_value(v, "seedDate")
	testing.expect(t, d.is_valid(sd))
	testing.expect_value(t, d.year(sd), 1991)
	testing.expect(t, d.is_absolute_date(sd))

	rel := jx.iso_date_value(v, "rel")
	testing.expect(t, d.is_relative_date(rel))

	// an absent key must leave the target untouched (the __DEFAULT__USED__ marker)
	keep := d.make_date(1, 1, 2000)
	jx.set_iso_date_value(&keep, v, "absent")
	testing.expect(t, d.eq(keep, d.make_date(1, 1, 2000)))
}

// dump must match json11 byte for byte - this is what makes the phase 1
// env_to_json oracle work. The heavy verification is run_json.sh over 455 real
// parameter files; these are the shape-level checks.
@(test)
test_jsonx_dump :: proc(t: ^testing.T) {
	// keys sorted (json11's object is a std::map), ", " and ": " separators
	v := parse(t, `{"b": 1, "a": 2, "c": [1, 2]}`)
	s := jx.dump(v)
	defer delete(s)
	testing.expect_value(t, s, `{"a": 2, "b": 1, "c": [1, 2]}`)

	// %.17g for doubles: 2.0 prints as "2", 0.1 keeps all 17 digits
	v2 := parse(t, `{"x": 2.0, "y": 0.1, "z": 6950.8}`)
	s2 := jx.dump(v2)
	defer delete(s2)
	testing.expect_value(t, s2, `{"x": 2, "y": 0.10000000000000001, "z": 6950.8000000000002}`)

	// escaping
	v3 := parse(t, `{"s": "a\"b\\c\nd\te"}`)
	s3 := jx.dump(v3)
	defer delete(s3)
	testing.expect_value(t, s3, `{"s": "a\"b\\c\nd\te"}`)

	// null / bool
	v4 := parse(t, `{"n": null, "t": true, "f": false}`)
	s4 := jx.dump(v4)
	defer delete(s4)
	testing.expect_value(t, s4, `{"f": false, "n": null, "t": true}`)
}

// Documented divergence: Odin's core:encoding/json drops empty-string keys
// (parser.odin has an explicit `if key != ""` guard). MONICA never looks up "",
// so this is accepted. This test pins the behaviour so a future core update
// that changes it is noticed.
@(test)
test_jsonx_empty_key_is_dropped :: proc(t: ^testing.T) {
	v := parse(t, `{"VCMAX25": 140.0, "": 13.1, "a": 1}`)
	testing.expect_value(t, len(jx.object_items(v)), 2)
	testing.expect(t, jx.is_null(jx.get(v, "")))
	testing.expect_value(t, jx.number_value(jx.get(v, "VCMAX25")), 140.0)
}

@(test)
test_jsonx_object_keys_sorted :: proc(t: ^testing.T) {
	v := parse(t, `{"zeta": 1, "alpha": 2, "Mid": 3}`)
	keys := jx.object_keys_sorted(v)
	defer delete(keys)
	testing.expect_value(t, len(keys), 3)
	// byte-wise ordering, matching std::less<std::string>: uppercase first
	testing.expect_value(t, keys[0], "Mid")
	testing.expect_value(t, keys[1], "alpha")
	testing.expect_value(t, keys[2], "zeta")
}

@(test)
test_jsonx_has_object_shape :: proc(t: ^testing.T) {
	v := parse(t, `{"cropParams": {"species": {}}, "notObj": 5}`)
	testing.expect(t, jx.has_object_shape(v, "cropParams"))
	testing.expect(t, !jx.has_object_shape(v, "notObj"))
	testing.expect(t, !jx.has_object_shape(v, "absent"))
	testing.expect(t, jx.has_object_shape(jx.get(v, "cropParams"), "species"))
}

// Temp allocator here too: a failed parse leaves the partially built value
// allocated (core:encoding/json does not unwind on error), which is harmless
// under the arena but would show up as a leak on context.allocator.
@(test)
test_jsonx_parse_failure :: proc(t: ^testing.T) {
	r := jx.parse_json_string(`{"a": `, context.temp_allocator)
	testing.expect(t, tl.failure(r.errs))
}
