// The SetValue write side: the ["=", a, op, b] arithmetic form, and the
// tier-agnostic oid get/set entry points it and the workstep share.
//
// These are the pieces that need no Monica_Model. The parts that do - "does
// poking a field actually change the run" - are covered end to end by
// installer/Hohenfinow2/sim-min-setvalue.json under
// odin/tests/diff_outputs.sh, which is the only way to see a write survive a
// model step.
package tests

import "core:testing"
import mio "../monica/io"
import mcore "../monica/core"
import jx "../support/jsonx"
import rp "../support/reflectpath"

@(private = "file")
arr :: proc(vs: ..f64) -> jx.Value {
	a := make(jx.Array, 0, len(vs), context.temp_allocator)
	for v in vs {
		append(&a, jx.f(v))
	}
	return jx.Value(a)
}

@(test)
test_primitive_calc_ops :: proc(t: ^testing.T) {
	testing.expect_value(t, mio.get_primitive_calc_op("+"), mio.Calc_Op.ADD)
	testing.expect_value(t, mio.get_primitive_calc_op("-"), mio.Calc_Op.SUB)
	testing.expect_value(t, mio.get_primitive_calc_op("*"), mio.Calc_Op.MUL)
	testing.expect_value(t, mio.get_primitive_calc_op("/"), mio.Calc_Op.DIV)
	// C++ getPrimitiveCalcOp returns a `[](double,double){ return 0.0; }` for
	// anything else, and buildExpression accepts it - so an unknown operator
	// is a silent constant 0.0, not an error
	testing.expect_value(t, mio.get_primitive_calc_op("^"), mio.Calc_Op.ZERO)
	testing.expect_value(t, mio.get_primitive_calc_op(""), mio.Calc_Op.ZERO)
}

@(test)
test_apply_primitive_calc_op :: proc(t: ^testing.T) {
	ta := context.temp_allocator

	// scalar op scalar
	v := mio.apply_primitive_calc_op(.ADD, jx.f(2), jx.f(3), ta)
	testing.expect_value(t, jx.number_value(v), 5.0)
	v = mio.apply_primitive_calc_op(.SUB, jx.f(2), jx.f(3), ta)
	testing.expect_value(t, jx.number_value(v), -1.0)
	v = mio.apply_primitive_calc_op(.MUL, jx.f(2), jx.f(3), ta)
	testing.expect_value(t, jx.number_value(v), 6.0)
	v = mio.apply_primitive_calc_op(.DIV, jx.f(3), jx.f(2), ta)
	testing.expect_value(t, jx.number_value(v), 1.5)
	// the unknown-operator fall-through
	v = mio.apply_primitive_calc_op(.ZERO, jx.f(2), jx.f(3), ta)
	testing.expect_value(t, jx.number_value(v), 0.0)

	// array op scalar - the layer-range shape, elementwise
	v = mio.apply_primitive_calc_op(.MUL, arr(1, 2, 3), jx.f(10), ta)
	testing.expect(t, jx.is_array(v))
	items := jx.array_items(v)
	testing.expect_value(t, len(items), 3)
	testing.expect_value(t, jx.number_value(items[0]), 10.0)
	testing.expect_value(t, jx.number_value(items[2]), 30.0)

	// scalar op array - note the operand order is preserved, so this is
	// 100 - x, not x - 100
	v = mio.apply_primitive_calc_op(.SUB, jx.f(100), arr(1, 2), ta)
	items = jx.array_items(v)
	testing.expect_value(t, jx.number_value(items[0]), 99.0)
	testing.expect_value(t, jx.number_value(items[1]), 98.0)

	// a non-number inside an array contributes 0.0, matching the C++ ternary
	mixed := make(jx.Array, 0, 2, ta)
	append(&mixed, jx.f(5))
	append(&mixed, jx.sl("nope"))
	v = mio.apply_primitive_calc_op(.ADD, jx.Value(mixed), jx.f(1), ta)
	items = jx.array_items(v)
	testing.expect_value(t, jx.number_value(items[0]), 6.0)
	testing.expect_value(t, jx.number_value(items[1]), 0.0)

	// neither side usable -> 0.0, the C++'s trailing `return 0.0;`
	v = mio.apply_primitive_calc_op(.ADD, jx.sl("a"), jx.sl("b"), ta)
	testing.expect_value(t, jx.number_value(v), 0.0)
}

// NOTE(c++-quirk): applyPrimitiveCalcOp's array/array branch accumulates into
// a `vector<bool>` - a copy-paste slip from the applyCompareOp sibling right
// above it - so every arithmetic result is narrowed to a boolean before it
// becomes JSON. Pinned as a test so that if the C++ is ever fixed, this fails
// loudly and the port is updated deliberately rather than drifting.
@(test)
test_apply_primitive_calc_op_array_array_is_boolean :: proc(t: ^testing.T) {
	ta := context.temp_allocator

	v := mio.apply_primitive_calc_op(.ADD, arr(1, 2), arr(10, 20), ta)
	items := jx.array_items(v)
	testing.expect_value(t, len(items), 2)
	testing.expect(t, jx.is_bool(items[0]))
	testing.expect(t, jx.bool_value_of(items[0])) // 11 -> true, NOT 11
	testing.expect(t, jx.bool_value_of(items[1])) // 22 -> true, NOT 22

	// ... and a result of exactly zero comes back false
	v = mio.apply_primitive_calc_op(.SUB, arr(3, 5), arr(3, 1), ta)
	items = jx.array_items(v)
	testing.expect(t, !jx.bool_value_of(items[0])) // 3-3 = 0 -> false
	testing.expect(t, jx.bool_value_of(items[1])) // 5-1 = 4 -> true

	// the C++ transform() walks the LEFT operand's length while reading the
	// right through an unchecked iterator; this port clamps to the shorter
	// side rather than reproducing the overrun
	v = mio.apply_primitive_calc_op(.ADD, arr(1, 2, 3), arr(10), ta)
	testing.expect_value(t, len(jx.array_items(v)), 1)
}

// One proc on purpose: build_primitive_calc_expression reaches
// parse_output_ids, which lazily builds build_output_table's and the alias
// table's package-global maps. `odin test` runs test procs on 48 threads, so
// two procs racing that one-time init is a real data race - keeping every
// caller in a single proc keeps it single-threaded without adding a mutex to
// production code that monica-run only ever calls single-threaded.
@(test)
test_build_primitive_calc_expression :: proc(t: ^testing.T) {
	ta := context.temp_allocator
	// the two globals are built lazily on first use and would otherwise be
	// reported as a leak by the test runner's tracking allocator
	defer mio.destroy_output_tables()

	// oid op number - the common read-modify-write shape
	e := mio.build_primitive_calc_expression([]jx.Value{jx.sl("Kc"), jx.sl("*"), jx.f(2)}, ta)
	testing.expect(t, e.set)
	testing.expect_value(t, e.op, mio.Calc_Op.MUL)
	testing.expect_value(t, e.left.kind, mio.Calc_Operand_Kind.OID)
	testing.expect_value(t, e.left.oid.name, "Kc")
	testing.expect_value(t, e.right.kind, mio.Calc_Operand_Kind.CONSTANT)

	// number op oid
	e = mio.build_primitive_calc_expression([]jx.Value{jx.f(1), jx.sl("-"), jx.sl("Kc")}, ta)
	testing.expect(t, e.set)
	testing.expect_value(t, e.left.kind, mio.Calc_Operand_Kind.CONSTANT)
	testing.expect_value(t, e.right.kind, mio.Calc_Operand_Kind.OID)

	// oid op oid
	e = mio.build_primitive_calc_expression([]jx.Value{jx.sl("Kc"), jx.sl("+"), jx.sl("LAI")}, ta)
	testing.expect(t, e.set)
	testing.expect_value(t, e.left.kind, mio.Calc_Operand_Kind.OID)
	testing.expect_value(t, e.right.kind, mio.Calc_Operand_Kind.OID)

	// a RAW PATH operand - no table entry on either side. This is what the
	// path tier adds to an expression form the C++ could only feed from its
	// registered ids.
	e = mio.build_primitive_calc_expression(
		[]jx.Value{jx.sl("soil_moisture.actual_evaporation"), jx.sl("*"), jx.f(2)},
		ta,
	)
	testing.expect(t, e.set)
	testing.expect(t, e.left.oid.plan != nil)

	// an unknown operator still BUILDS (and evaluates to 0.0 forever) - the
	// C++ never rejects one
	e = mio.build_primitive_calc_expression([]jx.Value{jx.sl("Kc"), jx.sl("%"), jx.f(2)}, ta)
	testing.expect(t, e.set)
	testing.expect_value(t, e.op, mio.Calc_Op.ZERO)

	// ... but these do NOT build, and the workstep leaves getValue unset
	bad := [][]jx.Value {
		// two literals: buildExpression has no branch for it
		{jx.f(1), jx.sl("+"), jx.f(2)},
		// an operand that names nothing resolvable. This prints two tier-4
		// "ignoring output" warnings on stderr during the test run - that is
		// the feature under test, not noise to be silenced.
		{jx.sl("NoSuchOutputOrPath"), jx.sl("+"), jx.f(2)},
		{jx.f(2), jx.sl("+"), jx.sl("NoSuchOutputOrPath")},
		// wrong arity
		{jx.sl("Kc"), jx.sl("*")},
		{jx.sl("Kc"), jx.sl("*"), jx.f(2), jx.f(3)},
		// the operator slot must be a string
		{jx.sl("Kc"), jx.f(1), jx.f(2)},
	}
	for b, i in bad {
		e2 := mio.build_primitive_calc_expression(b, ta)
		testing.expectf(t, !e2.set, "case %d built an expression it should not have", i)
	}

	// --- the getter/setter tiers, same single-threaded reason ---

	// a lambda-backed id with no registered setf stays unsettable
	eta := oid_named(t, "ETa/ETc", ta)
	testing.expect(t, mio.oid_has_getter(eta))
	testing.expect(t, !mio.oid_has_setter(eta))
	testing.expect(t, eta.plan == nil)

	// an alias-backed id is settable now even though the C++ registers no setf
	// for it - this is the whole point of step 8
	pwp := oid_named(t, "Pwp", ta)
	testing.expect(t, pwp.plan != nil)
	testing.expect(t, mio.oid_has_getter(pwp))
	testing.expect(t, mio.oid_has_setter(pwp))

	// so is a raw path that is in no table at all
	raw := oid_named(t, "soil_column.layers.soil_no3", ta)
	testing.expect(t, raw.plan != nil)
	testing.expect(t, mio.oid_has_setter(raw))

	// a #len path reads but must not write
	length := oid_named(t, "soil_column.layers.#len", ta)
	testing.expect(t, length.plan != nil)
	testing.expect(t, mio.oid_has_getter(length))
	testing.expect(t, !mio.oid_has_setter(length))

	// Stage is computed (vc_DevelopmentalStage + 1), so it must NOT have
	// acquired a plan - its legacy setf, which undoes the +1, has to stay the
	// one that runs
	stage := oid_named(t, "Stage", ta)
	testing.expect(t, stage.plan == nil)
	testing.expect(t, mio.oid_has_setter(stage))
}

@(private = "file")
oid_named :: proc(t: ^testing.T, name: string, allocator := context.allocator) -> mio.OId {
	oids := mio.parse_output_ids([]jx.Value{jx.sl(name)}, allocator = allocator)
	if !testing.expectf(t, len(oids) == 1, "%q did not resolve to an oid", name) {
		return mio.make_default_oid()
	}
	return oids[0]
}

_ :: mcore
_ :: rp
