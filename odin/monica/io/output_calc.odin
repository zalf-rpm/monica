// Not in the C++: arithmetic on an output column, the object form's "calc".
//
//   {"path": "soil_moisture.actual_evapotranspiration", "name": "ETa/ET0",
//    "unit": "1", "calc": [["/", "soil_moisture.reference_evapotranspiration"]]}
//   {"path": "climate_data.#last.tmax", "name": "hot days", "unit": "d",
//    "calc": [">=", 30], "agg": "SUM"}
//
// "calc" is a list of steps applied left to right, without precedence: a step
// is [op, operand] or, for a function, [op]. A single step may be written
// without the outer list. An operand is a number or another output, in any
// form an output column takes (a name, a path, the array form, the object
// form - which may have a "calc" of its own, the way to write brackets:
// (a + b) / (c + d) is "a" with [["+", "b"], ["/", {"path": "c", "calc":
// ["+", "d"]}]]).
//
// Per day, in this order: each output (the column's and every operand) is
// read with its own layers and layer aggregation; the steps are applied;
// the column's "round"/"cast" are applied; time aggregation ("agg") then
// works on the results like on any other column. A layered value and a
// number combine per layer, two layered values layer by layer; they must
// have the same number of layers (checked at setup where the paths say how
// many, otherwise a mismatch gives NaN).
//
// Arithmetic is IEEE: x / 0 is ±Inf, ln of a negative number is NaN, and
// the CSV shows them as such rather than hiding them. A missing value (no
// crop yet) reads as 0.0, as everywhere in the output (CONVENTIONS §9).
//
// Unrelated to the ["=", a, op, b] expression of a SetValue workstep
// (build_output.odin), which reproduces the C++'s quirks; this is checked
// at setup instead.
package monica_io

import cr "../../support/cr_math"
import jx "../../support/jsonx"
import core "../core"
import "core:fmt"
import "core:math"

Output_Calc_Op :: enum {
	// binary: [op, operand]
	ADD,
	SUB,
	MUL,
	DIV,
	POW,
	MOD,
	MIN,
	MAX,
	LT,
	LE,
	GT,
	GE,
	EQ,
	NE,
	// unary: [op]
	NEG,
	ABS,
	SQRT,
	EXP,
	LN,
	LOG10,
	FLOOR,
	CEIL,
}

// The names a sim.json writes. Comparisons give 1 or 0, so that "agg": "SUM"
// counts the days a condition holds.
@(rodata)
output_calc_op_names := [Output_Calc_Op]string {
	.ADD   = "+",
	.SUB   = "-",
	.MUL   = "*",
	.DIV   = "/",
	.POW   = "^",
	.MOD   = "%", // remainder with the sign of the dividend, like C's fmod
	.MIN   = "min",
	.MAX   = "max",
	.LT    = "<",
	.LE    = "<=",
	.GT    = ">",
	.GE    = ">=",
	.EQ    = "==",
	.NE    = "!=",
	.NEG   = "neg",
	.ABS   = "abs",
	.SQRT  = "sqrt",
	.EXP   = "exp",
	.LN    = "ln",
	.LOG10 = "log10",
	.FLOOR = "floor",
	.CEIL  = "ceil",
}

output_calc_op_is_unary :: proc(op: Output_Calc_Op) -> bool {
	return op >= .NEG
}

Calc_Step :: struct {
	op:      Output_Calc_Op,
	// another output, read on the same day; nil for a number or a function
	operand: ^OId,
	number:  f64,
}

// Parses a "calc" value onto `oid`, whose path, layers and aggregation are
// already set. Returns a message on error, "" otherwise.
@(private)
oid_parse_calc :: proc(oid: ^OId, j: jx.Value, allocator: jx.Allocator) -> string {
	if !jx.is_array(j) {
		return "calc: expected a list of steps like [[\"*\", 2], [\"+\", 1]]"
	}
	items := jx.array_items(j)
	step_specs := items
	if len(items) > 0 && jx.is_string(items[0]) {
		step_specs = []jx.Value{j}
	}

	steps := make([dynamic]Calc_Step, 0, len(step_specs), allocator)
	shape := oid_shape(oid^)
	for sj in step_specs {
		a := jx.array_items(sj)
		if !jx.is_array(sj) || len(a) == 0 || !jx.is_string(a[0]) {
			return fmt.tprintf("calc: a step is [op] or [op, operand], not %s", jx.dump(sj, context.temp_allocator))
		}
		name := jx.string_value_of(a[0])
		op, known := output_calc_op_by_name(name)
		if !known {
			return fmt.tprintf("calc: unknown operator %q (known: %v)", name, output_calc_op_names)
		}
		step := Calc_Step {
			op = op,
		}
		if output_calc_op_is_unary(op) {
			if len(a) != 1 {
				return fmt.tprintf("calc: %q takes no operand", name)
			}
		} else {
			if len(a) != 2 {
				return fmt.tprintf("calc: %q needs one operand", name)
			}
			if jx.is_number(a[1]) {
				step.number = jx.number_value(a[1])
			} else {
				oids := parse_output_ids(a[1:2], allocator = allocator)
				if len(oids) == 0 || !oid_has_getter(oids[0]) {
					return fmt.tprintf(
						"calc: operand %s is neither a number nor an output",
						jx.dump(a[1], context.temp_allocator),
					)
				}
				step.operand = new_clone(oids[0], allocator)
				ok: bool
				if shape, ok = combine_shapes(shape, oid_shape(step.operand^)); !ok {
					return fmt.tprintf(
						"calc: operand %s has %d layers, the value before it %d",
						jx.dump(a[1], context.temp_allocator),
						oid_shape(step.operand^),
						shape,
					)
				}
			}
		}
		append(&steps, step)
	}
	oid.calc = steps[:]
	return ""
}

@(private)
output_calc_op_by_name :: proc(name: string) -> (Output_Calc_Op, bool) {
	for n, op in output_calc_op_names {
		if n == name {
			return op, true
		}
	}
	return {}, false
}

// How many values an output gives per day: 0 for a number, n for n layers
// (or organs), -1 if only the day will tell (a lambda-backed name).
@(private)
oid_shape :: proc(oid: OId) -> int {
	shape := -1
	if oid.plan != nil {
		o := oid
		switch {
		case o.plan.open_step < 0 || o.layerAggOp != .NONE:
			shape = 0
		case oid_is_organ(&o):
			shape = 1
		case:
			shape = max(o.toLayer - o.fromLayer + 1, 0)
		}
	}
	for s in oid.calc {
		if s.operand != nil {
			shape, _ = combine_shapes(shape, oid_shape(s.operand^))
		}
	}
	return shape
}

// A number combines with anything; two layered values need the same count.
@(private)
combine_shapes :: proc(a, b: int) -> (int, bool) {
	switch {
	case a == 0:
		return b, true
	case b == 0:
		return a, true
	case a < 0 || b < 0:
		return -1, true
	case a == b:
		return a, true
	}
	return a, false
}

// The daily read of a column with a "calc" (oid_get_value's): the column's
// own value unrounded, the steps, then the column's rounding and cast.
@(private)
calc_oid_value :: proc(model: ^core.Monica_Model, oid: OId, allocator := context.allocator) -> (jx.Value, bool) {
	base := oid
	base.calc = nil
	base.roundToDigits = -1
	base.castTo = .NONE
	v, ok := oid_get_value(model, base, allocator)
	if !ok {
		return v, false
	}
	for s in oid.calc {
		r := jx.f(s.number)
		if s.operand != nil {
			r, _ = oid_get_value(model, s.operand^, allocator)
		}
		v = calc_apply_json(s.op, v, r, allocator)
	}
	return calc_finish(v, oid.roundToDigits, oid.castTo, allocator), true
}

@(private)
calc_apply_json :: proc(op: Output_Calc_Op, l, r: jx.Value, allocator: jx.Allocator) -> jx.Value {
	switch {
	case jx.is_array(l):
		ls := jx.array_items(l)
		rs := jx.array_items(r)
		out := make(jx.Array, len(ls), allocator)
		for x, i in ls {
			y := r
			if jx.is_array(r) {
				y = len(rs) == len(ls) ? rs[i] : jx.f(math.nan_f64())
			}
			out[i] = jx.f(calc_apply(op, calc_number(x), calc_number(y)))
		}
		return jx.Value(out)
	case jx.is_array(r):
		rs := jx.array_items(r)
		out := make(jx.Array, len(rs), allocator)
		for y, i in rs {
			out[i] = jx.f(calc_apply(op, calc_number(l), calc_number(y)))
		}
		return jx.Value(out)
	}
	return jx.f(calc_apply(op, calc_number(l), calc_number(r)))
}

// Bools as 1/0; a string (Date, Crop) is not a number.
@(private)
calc_number :: proc(v: jx.Value) -> f64 {
	if jx.is_number(v) {
		return jx.number_value(v)
	}
	if jx.is_bool(v) {
		return jx.bool_value_of(v) ? 1 : 0
	}
	return math.nan_f64()
}

calc_apply :: proc(op: Output_Calc_Op, l, r: f64) -> f64 {
	b :: proc(c: bool) -> f64 {return c ? 1 : 0}
	switch op {
	case .ADD:
		return l + r
	case .SUB:
		return l - r
	case .MUL:
		return l * r
	case .DIV:
		return l / r
	case .POW:
		return cr.pow(l, r)
	case .MOD:
		return math.mod(l, r)
	case .MIN:
		return min(l, r)
	case .MAX:
		return max(l, r)
	case .LT:
		return b(l < r)
	case .LE:
		return b(l <= r)
	case .GT:
		return b(l > r)
	case .GE:
		return b(l >= r)
	case .EQ:
		return b(l == r)
	case .NE:
		return b(l != r)
	case .NEG:
		return -l
	case .ABS:
		return abs(l)
	case .SQRT:
		return math.sqrt(l)
	case .EXP:
		return cr.exp(l)
	case .LN:
		return cr.log(l)
	case .LOG10:
		return cr.log10(l)
	case .FLOOR:
		return math.floor(l)
	case .CEIL:
		return math.ceil(l)
	}
	return math.nan_f64()
}

@(private)
calc_finish :: proc(v: jx.Value, digits: int, cast_to: OId_Cast, allocator: jx.Allocator) -> jx.Value {
	one :: proc(x: f64, digits: int, cast_to: OId_Cast) -> jx.Value {
		if cast_to == .INT && !math.is_nan(x) && !math.is_inf(x) {
			return jx.i(int(x))
		}
		return jx.f(round_or_not(x, digits))
	}
	if jx.is_array(v) {
		vs := jx.array_items(v)
		out := make(jx.Array, len(vs), allocator)
		for x, i in vs {
			out[i] = one(jx.number_value(x), digits, cast_to)
		}
		return jx.Value(out)
	}
	return one(jx.number_value(v), digits, cast_to)
}
