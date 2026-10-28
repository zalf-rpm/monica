// The "obj-outputs?" result form: one JSON object per row instead of one array
// per output id. Needs no Monica_Model - aggregation, CSV writing and JSON
// serialisation only. The end-to-end checks are the Hohenfinow2 Python
// producer/consumer pipeline against monica-zmq-server (test_env.json sets
// "obj-outputs?") and monica-run with a sim.json "output": {"obj-outputs": true}.
package tests

import "core:strings"
import "core:testing"
import mio "../monica/io"
import run "../monica/run"
import jx "../support/jsonx"

@(private = "file")
oid :: proc(name: string, agg: mio.OId_Op, display := "") -> mio.OId {
	return mio.OId{name = name, display_name = display, time_agg_op = agg, organ = .UNDEFINED_ORGAN}
}

@(private = "file")
column :: proc(vs: ..jx.Value) -> [dynamic]jx.Value {
	c := make([dynamic]jx.Value, 0, len(vs), context.temp_allocator)
	append(&c, ..vs)
	return c
}

@(test)
test_aggregate_results_obj :: proc(t: ^testing.T) {
	ta := context.temp_allocator
	context.allocator = ta

	sd: run.Store_Data
	append(&sd.output_ids, oid("Yield", .SUM), oid("Date", .LAST), oid("Stage", .AVG, "Phase"))
	append(&sd.intermediate_results, column(jx.f(1), jx.f(2), jx.f(3)))
	append(&sd.intermediate_results, column(jx.sl("1992-01-01"), jx.sl("1992-12-31")))
	append(&sd.intermediate_results, column())

	run.store_data_aggregate_results_obj(&sd, ta)
	testing.expect_value(t, len(sd.results_obj), 1)
	row := jx.object_items(sd.results_obj[0])
	testing.expect_value(t, jx.number_value(row["Yield"]), 6.0)
	testing.expect_value(t, jx.string_value_of(row["Date"]), "1992-12-31")
	// an empty column contributes no key at all (not a null)
	_, has_phase := row["Phase"]
	testing.expect(t, !has_phase)
	// and the aggregated columns were consumed
	testing.expect_value(t, len(sd.intermediate_results[0]), 0)
	// the array form stays untouched
	testing.expect_value(t, len(sd.results), 0)

	// NOTE(c++-quirk) pin: aggregating again with every column empty still
	// appends a row - an empty object - unlike the array form.
	run.store_data_aggregate_results_obj(&sd, ta)
	testing.expect_value(t, len(sd.results_obj), 2)
	testing.expect_value(t, len(jx.object_items(sd.results_obj[1])), 0)

	// no intermediate results at all (at-event-only sections): no row
	empty: run.Store_Data
	run.store_data_aggregate_results_obj(&empty, ta)
	testing.expect_value(t, len(empty.results_obj), 0)
}

@(test)
test_write_output_obj :: proc(t: ^testing.T) {
	ta := context.temp_allocator
	context.allocator = ta

	oids := []mio.OId{oid("Date", .NONE), oid("Mois", .NONE), oid("Crop", .NONE)}

	full := make(jx.Object, 0, ta)
	jx.obj_set(&full, "Date", jx.sl("1992-01-01"), ta)
	mois := make(jx.Array, 0, 2, ta)
	append(&mois, jx.f(0.25), jx.f(0.5))
	jx.obj_set(&full, "Mois", jx.Value(mois), ta)
	jx.obj_set(&full, "Crop", jx.sl("winter wheat, early"), ta)

	// "Mois" missing: NOTE(c++-quirk) - nothing is written for it, not even
	// its separator.
	partial := make(jx.Object, 0, ta)
	jx.obj_set(&partial, "Date", jx.sl("1992-01-02"), ta)
	jx.obj_set(&partial, "Crop", jx.b(true), ta)

	b := strings.builder_make(ta)
	mio.write_output_obj(strings.to_writer(&b), oids, []jx.Value{jx.Value(full), jx.Value(partial)}, ",")
	testing.expect_value(
		t,
		strings.to_string(b),
		"1992-01-01,0.25,0.5,\"winter wheat, early\"\r\n1992-01-02,1\r\n",
	)
}

@(test)
test_output_to_json_obj_rows :: proc(t: ^testing.T) {
	ta := context.temp_allocator
	context.allocator = ta

	row := make(jx.Object, 0, ta)
	jx.obj_set(&row, "Yield", jx.f(7), ta)

	out: mio.Output
	d: mio.Output_Data
	d.orig_spec = "\"crop\""
	append(&d.results_obj, jx.Value(row))
	append(&out.data, d)

	j := mio.output_to_json(&out, ta)
	results := jx.array_items(jx.get(jx.array_items(jx.get(j, "data"))[0], "results"))
	testing.expect_value(t, len(results), 1)
	testing.expect(t, jx.is_object(results[0]))
	testing.expect_value(t, jx.number_value(jx.get(results[0], "Yield")), 7.0)
}

// sim.json's output."obj-outputs" has to land where env_return_obj_outputs
// reads it. The C++ nests it one level too deep (outputs.output), so there it
// never takes effect - this pins the fixed, flat layout.
@(test)
test_sim_json_obj_outputs_flag :: proc(t: ^testing.T) {
	ta := context.temp_allocator
	context.allocator = ta

	for want in ([]bool{true, false}) {
		output := make(jx.Object, 0, ta)
		jx.obj_set(&output, "obj-outputs", jx.b(want), ta)
		jx.obj_set(&output, "use-legacy-output-fns?", jx.b(want), ta)
		sim := make(jx.Object, 0, ta)
		jx.obj_set(&sim, "output", jx.Value(output), ta)

		env_json := run.create_env_json_from_json_objects(
			jx.Value(make(jx.Object, 0, ta)),
			jx.Value(make(jx.Object, 0, ta)),
			jx.Value(sim),
			ta,
		)
		env: run.Env
		env.outputs = jx.get(env_json, "outputs")
		testing.expect_value(t, run.env_return_obj_outputs(&env), want)
		testing.expect_value(t, run.env_use_legacy_output_fns(&env), want)
	}
}
