// The "obj-outputs?" result form: one JSON object per row instead of one array
// per output id. Needs no Monica_Model - aggregation, CSV writing and JSON
// serialisation only. The end-to-end check is the Hohenfinow2 Python
// producer/consumer pipeline against monica-zmq-server (test_env.json sets
// "obj-outputs?"); monica-run can't reach this path from a sim.json, see
// create_env_json.odin.
package tests

import "core:strings"
import "core:testing"
import mio "../monica/io"
import run "../monica/run"
import jx "../support/jsonx"

@(private = "file")
oid :: proc(name: string, agg: mio.OId_Op, display := "") -> mio.OId {
	return mio.OId{name = name, displayName = display, timeAggOp = agg, organ = .UNDEFINED_ORGAN}
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
	append(&sd.outputIds, oid("Yield", .SUM), oid("Date", .LAST), oid("Stage", .AVG, "Phase"))
	append(&sd.intermediateResults, column(jx.f(1), jx.f(2), jx.f(3)))
	append(&sd.intermediateResults, column(jx.sl("1992-01-01"), jx.sl("1992-12-31")))
	append(&sd.intermediateResults, column())

	run.store_data_aggregate_results_obj(&sd, ta)
	testing.expect_value(t, len(sd.resultsObj), 1)
	row := jx.object_items(sd.resultsObj[0])
	testing.expect_value(t, jx.number_value(row["Yield"]), 6.0)
	testing.expect_value(t, jx.string_value_of(row["Date"]), "1992-12-31")
	// an empty column contributes no key at all (not a null)
	_, has_phase := row["Phase"]
	testing.expect(t, !has_phase)
	// and the aggregated columns were consumed
	testing.expect_value(t, len(sd.intermediateResults[0]), 0)
	// the array form stays untouched
	testing.expect_value(t, len(sd.results), 0)

	// NOTE(c++-quirk) pin: aggregating again with every column empty still
	// appends a row - an empty object - unlike the array form.
	run.store_data_aggregate_results_obj(&sd, ta)
	testing.expect_value(t, len(sd.resultsObj), 2)
	testing.expect_value(t, len(jx.object_items(sd.resultsObj[1])), 0)

	// no intermediate results at all (at-event-only sections): no row
	empty: run.Store_Data
	run.store_data_aggregate_results_obj(&empty, ta)
	testing.expect_value(t, len(empty.resultsObj), 0)
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
	d.origSpec = "\"crop\""
	append(&d.resultsObj, jx.Value(row))
	append(&out.data, d)

	j := mio.output_to_json(&out, ta)
	results := jx.array_items(jx.get(jx.array_items(jx.get(j, "data"))[0], "results"))
	testing.expect_value(t, len(results), 1)
	testing.expect(t, jx.is_object(results[0]))
	testing.expect_value(t, jx.number_value(jx.get(results[0], "Yield")), 7.0)
}
