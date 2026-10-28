// Output units from the fields' `unit` tags, and the object form's "calc"
// (monica/io/output_calc.odin).
//
// The values are read from a hand-filled Monica_Model: its soil structs are
// held by value, so `new` plus a few fields is enough for the paths used here.
// Everything is allocated from the temp allocator, like the run's arena.
package tests

import "core:fmt"
import "core:math"
import "core:reflect"
import "core:sync"
import "core:testing"
import tl "../support/tools"
import jx "../support/jsonx"
import rp "../support/reflectpath"
import mio "../monica/io"
import mcore "../monica/core"
import et "../monica/modules/evapotranspiration"
import stm "../monica/modules/soil_temperature"

@(private = "file")
Tagged_Inner :: struct {
	t:      f64 `unit:"°C"`,
	untagged: f64,
	values: []f64 `unit:"mm"`,
}

@(private = "file")
Tagged_Outer :: struct {
	inner:  Tagged_Inner `unit:"not this one"`,
	inners: [dynamic]Tagged_Inner,
}

@(test)
test_path_unit_is_the_last_fields_tag :: proc(t: ^testing.T) {
	cases := []struct {
		path:     string,
		unit:     string,
		has_unit: bool,
	} {
		{"inner.t", "°C", true},
		{"inner.untagged", "", false},
		{"inner.values", "mm", true}, // open dimension
		{"inner.values.2", "mm", true}, // index
		{"inners.t", "°C", true},
		{"inners.0.values.#last", "mm", true},
		{"inner.values.#len", "1", true}, // a count
		{"inner", "not this one", true},
	}
	for c in cases {
		plan, err := rp.compile(typeid_of(Tagged_Outer), c.path, context.temp_allocator)
		testing.expectf(t, !rp.failed(err), "%s: %s", c.path, err.msg)
		testing.expectf(t, plan.unit == c.unit && plan.has_unit == c.has_unit, "%s: %q %v", c.path, plan.unit, plan.has_unit)
	}
}

// Held by every test that parses outputs - see set_value_test.odin's
// test_build_primitive_calc_expression for why.
output_tables_lock: sync.Mutex

lock_output_tables :: proc() {
	sync.mutex_lock(&output_tables_lock)
}

// The tables are built lazily on first use and would otherwise be reported as
// a leak by the test runner's tracking allocator.
unlock_output_tables :: proc() {
	mio.destroy_output_tables()
	sync.mutex_unlock(&output_tables_lock)
}

@(private = "file")
parse_spec :: proc(t: ^testing.T, s: string) -> []mio.OId {
	r := jx.parse_json_string(s, context.temp_allocator)
	testing.expectf(t, tl.success(r.errs), "bad JSON in test: %s", s)
	return mio.parse_output_ids(jx.array_items(r.result)[:], allocator = context.temp_allocator)[:]
}

@(test)
test_output_units :: proc(t: ^testing.T) {
	context.allocator = context.temp_allocator
	lock_output_tables()
	defer unlock_output_tables()
	cases := []struct {
		spec: string,
		unit: string,
	} {
		{`"soil_moisture.actual_evapotranspiration"`, "mm"},
		{`["soil_column.layers.soil_temperature", [1, 3]]`, "°C"},
		{`"climate_data.#last.tavg"`, "°C"},
		{`"climate_data.#last.globrad"`, "MJ m-2"},
		{`"soil_column.layers.#len"`, "1"},
		{`"soil_moisture.groundwater_added"`, ""}, // no tag yet
		// parameters: units from monica-parameters and the C++ header
		{`"site_params.n_deposition"`, "kg N ha-1 y-1"},
		{`"crop_mod_params.minimum_available_n"`, "kg m-2"},
		// legacy names keep the output table's unit (byte-identical output)
		{`"Mois"`, "m3 m-3"},
		{`"Tavg"`, ""},
		{`{"path": "soil_moisture.actual_evaporation"}`, "mm"},
		{`{"path": "soil_moisture.actual_evaporation", "unit": "mm d-1"}`, "mm d-1"},
		{`{"path": "soil_moisture.actual_evaporation", "unit": ""}`, ""},
		// a calc may change the unit: none unless given
		{`{"path": "soil_moisture.actual_evaporation", "calc": ["/", 1000]}`, ""},
		{`{"path": "soil_moisture.actual_evaporation", "calc": ["/", 1000], "unit": "m"}`, "m"},
	}
	for c in cases {
		oids := parse_spec(t, tprint_array(c.spec))
		if !testing.expectf(t, len(oids) == 1, "%s: no output", c.spec) {
			continue
		}
		testing.expectf(t, oids[0].unit == c.unit, "%s: unit %q, want %q", c.spec, oids[0].unit, c.unit)
	}
}

@(private = "file")
tprint_array :: proc(spec: string) -> string {
	return fmt.tprintf("[%s]", spec)
}

// Every value a module reads or reports has a unit, so that the
// modules.*.input/output paths print one. Fields that are no quantity
// (flags, categories) carry an explicit `unit:""`.
@(test)
test_module_structs_have_units :: proc(t: ^testing.T) {
	check :: proc(t: ^testing.T, T: typeid) {
		for f in reflect.struct_fields_zipped(T) {
			if f.is_using {
				continue
			}
			ti := reflect.type_info_base(f.type)
			if s, is_slice := ti.variant.(reflect.Type_Info_Slice); is_slice {
				ti = reflect.type_info_base(s.elem)
			}
			#partial switch _ in ti.variant {
			case reflect.Type_Info_Float, reflect.Type_Info_Integer, reflect.Type_Info_Boolean:
				_, ok := reflect.struct_tag_lookup(f.tag, "unit")
				testing.expectf(t, ok, "%v.%s has no unit tag", T, f.name)
			}
		}
	}
	check(t, stm.Input)
	check(t, stm.Daily)
	check(t, stm.Output)
	check(t, et.Input)
	check(t, et.Daily)
	check(t, et.Output)
}

// The fields behind the legacy output names are the ones users read most;
// all of them have a unit, except where nobody wrote one down yet.
@(test)
test_alias_fields_have_units :: proc(t: ^testing.T) {
	context.allocator = context.temp_allocator
	lock_output_tables()
	defer unlock_output_tables()
	unknown := []string{"rootDensity", "NO3disp"}
	for e in mio.g_alias_table {
		if contains(unknown, e.name) {
			continue
		}
		oids := parse_spec(t, fmt.tprintf(`[{{"path": "%s"}}]`, e.alias.path))
		if !testing.expectf(t, len(oids) == 1, "%s: %s doesn't compile", e.name, e.alias.path) {
			continue
		}
		testing.expectf(t, oids[0].unit != "", "%s: %s has no unit", e.name, e.alias.path)
	}
	contains :: proc(xs: []string, x: string) -> bool {
		for y in xs {
			if y == x {
				return true
			}
		}
		return false
	}
}

@(test)
test_calc_operators :: proc(t: ^testing.T) {
	cases := []struct {
		op:   mio.Output_Calc_Op,
		l, r: f64,
		want: f64,
	} {
		{.ADD, 2, 3, 5},
		{.SUB, 2, 3, -1},
		{.MUL, 2, 3, 6},
		{.DIV, 3, 2, 1.5},
		{.POW, 2, 10, 1024},
		{.POW, 9, 0.5, 3},
		{.MOD, 7, 3, 1},
		{.MOD, -7, 3, -1},
		{.MIN, 2, 3, 2},
		{.MAX, 2, 3, 3},
		{.LT, 2, 3, 1},
		{.LE, 3, 3, 1},
		{.GT, 2, 3, 0},
		{.GE, 3, 3, 1},
		{.EQ, 3, 3, 1},
		{.NE, 3, 3, 0},
		{.NEG, 2, 0, -2},
		{.ABS, -2, 0, 2},
		{.SQRT, 16, 0, 4},
		{.EXP, 0, 0, 1},
		{.LN, 1, 0, 0},
		{.LOG10, 1000, 0, 3},
		{.FLOOR, -1.5, 0, -2},
		{.CEIL, -1.5, 0, -1},
	}
	for c in cases {
		got := mio.calc_apply(c.op, c.l, c.r)
		testing.expectf(t, got == c.want, "%v(%v, %v) = %v, want %v", c.op, c.l, c.r, got, c.want)
	}
	testing.expect(t, math.is_inf(mio.calc_apply(.DIV, 1, 0), 1))
	testing.expect(t, math.is_nan(mio.calc_apply(.LN, -1, 0)))
	for op in mio.Output_Calc_Op {
		testing.expectf(t, mio.output_calc_op_names[op] != "", "%v has no name", op)
	}
}

@(test)
test_calc_errors_drop_the_column :: proc(t: ^testing.T) {
	context.allocator = context.temp_allocator
	lock_output_tables()
	defer unlock_output_tables()
	bad := []string {
		`{"path": "soil_moisture.actual_evaporation", "calc": ["**", 2]}`, // unknown operator
		`{"path": "soil_moisture.actual_evaporation", "calc": ["abs", 2]}`, // a function takes no operand
		`{"path": "soil_moisture.actual_evaporation", "calc": ["+"]}`, // an operator needs one
		`{"path": "soil_moisture.actual_evaporation", "calc": [["+", 1], 2]}`, // not a step
		`{"path": "soil_moisture.actual_evaporation", "calc": "x"}`,
		`{"path": "soil_moisture.actual_evaporation", "calc": ["+", "no_such_thing"]}`,
		// 3 layers and 2 layers
		`{"path": "soil_column.layers.soil_temperature", "layers": [1, 3],
		  "calc": ["-", ["soil_column.layers.soil_temperature", [1, 2]]]}`,
	}
	for spec in bad {
		testing.expectf(t, len(parse_spec(t, tprint_array(spec))) == 0, "accepted: %s", spec)
	}
	ok := parse_spec(t, `[{"path": "soil_moisture.actual_evaporation", "calc": ["+", 1]}]`)
	testing.expect(t, len(ok) == 1 && !mio.oid_has_setter(ok[0]), "a calc column can't be written")
}

@(private = "file")
test_model :: proc() -> ^mcore.Monica_Model {
	m := new(mcore.Monica_Model)
	m.soil_moisture.actual_evapotranspiration = 2
	m.soil_moisture.reference_evapotranspiration = 4
	m.soil_moisture.actual_evaporation = 0.5
	for temp in ([]f64{10, 12, 14}) {
		append(&m.soil_column.layers, mcore.Soil_Layer{soil_temperature = temp})
	}
	return m
}

@(test)
test_calc_values :: proc(t: ^testing.T) {
	context.allocator = context.temp_allocator
	lock_output_tables()
	defer unlock_output_tables()
	m := test_model()
	cases := []struct {
		spec: string,
		want: []f64, // one value, or one per layer
	} {
		{`{"path": "soil_moisture.actual_evapotranspiration",
		   "calc": [["/", "soil_moisture.reference_evapotranspiration"]]}`, {0.5}},
		// left to right, no precedence
		{`{"path": "soil_moisture.actual_evapotranspiration", "calc": [["+", 1], ["*", 2]]}`, {6}},
		// brackets: (ETa + Eact) / (ET0 - 1)
		{`{"path": "soil_moisture.actual_evapotranspiration",
		   "calc": [["+", "soil_moisture.actual_evaporation"],
		            ["/", {"path": "soil_moisture.reference_evapotranspiration", "calc": ["-", 1]}]]}`, {2.5 / 3}},
		{`{"path": "soil_column.layers.soil_temperature", "layers": [1, 3], "calc": ["-", 10]}`, {0, 2, 4}},
		{`{"path": "soil_column.layers.soil_temperature", "layers": [1, 3],
		   "calc": ["-", ["soil_column.layers.soil_temperature", [1, 3]]]}`, {0, 0, 0}},
		{`{"path": "soil_moisture.reference_evapotranspiration",
		   "calc": ["-", ["soil_column.layers.soil_temperature", [1, 3]]]}`, {-6, -8, -10}},
		// layer aggregation first, then the calc
		{`{"path": "soil_column.layers.soil_temperature", "layers": [1, 3, "AVG"], "calc": ["*", 2]}`, {24}},
		{`{"path": "soil_column.layers.soil_temperature", "layers": [1, 3], "calc": [">=", 12]}`, {0, 1, 1}},
		// rounding after the calc
		{`{"path": "soil_moisture.actual_evapotranspiration", "calc": ["/", 3], "round": 2}`, {0.67}},
		{`{"path": "soil_moisture.actual_evapotranspiration", "calc": [["^", 3], ["sqrt"], ["neg"]]}`, {-math.sqrt(f64(8))}},
		// a missing value (no crop) reads 0.0, as without a calc
		{`{"path": "current_crop_module.leaf_area_index", "calc": ["+", 1]}`, {1}},
	}
	for c in cases {
		oids := parse_spec(t, tprint_array(c.spec))
		if !testing.expectf(t, len(oids) == 1, "rejected: %s", c.spec) {
			continue
		}
		v, ok := mio.oid_get_value(m, oids[0])
		testing.expectf(t, ok, "no value: %s", c.spec)
		got: [dynamic]f64
		if jx.is_array(v) {
			for x in jx.array_items(v) {
				append(&got, jx.number_value(x))
			}
		} else {
			append(&got, jx.number_value(v))
		}
		same := len(got) == len(c.want)
		for x, i in got {
			same = same && i < len(c.want) && x == c.want[i]
		}
		testing.expectf(t, same, "%s: got %v, want %v", c.spec, got[:], c.want)
	}
}
