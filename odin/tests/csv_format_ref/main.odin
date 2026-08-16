// Odin side of the phase 7 checkpoint 3 differential test - mirrors
// odin/tests/cpp_ref/csv_format_ref_main.cpp exactly (same OId values, same
// value rows, same order of calls). Run via
// odin/tests/cpp_ref/run_csv_format.sh.
package csv_format_ref

import "core:io"
import "core:os"
import mio "../../monica/io"
import jx "../../support/jsonx"

// C++'s cout, redirected to a file on Windows, is in text mode, so the '\n'
// in the "==" cout markers also becomes \r\n there - matched explicitly here
// since io.Writer never does that OS-level translation on its own.
marker :: proc(w: io.Writer, s: string) {
	io.write_string(w, s)
	io.write_string(w, "\r\n")
}

main :: proc() {
	w := os.to_stream(os.stdout)

	ids := make([dynamic]mio.OId, 0)

	dateId := mio.make_default_oid()
	dateId.name = "Date"
	dateId.unit = ""
	dateId.jsonInput = "\"Date\""
	append(&ids, dateId)

	yieldId := mio.make_default_oid()
	yieldId.name = "Yield"
	yieldId.unit = "kgDM ha-1"
	yieldId.jsonInput = "\"Yield\""
	append(&ids, yieldId)

	orgBiomId := mio.make_default_oid()
	orgBiomId.name = "OrgBiom"
	orgBiomId.unit = "kgDM ha-1"
	orgBiomId.jsonInput = "[\"OrgBiom\", \"LEAF\"]"
	orgBiomId.organ = .LEAF
	append(&ids, orgBiomId)

	moisRangeId := mio.make_default_oid()
	moisRangeId.name = "Mois"
	moisRangeId.unit = "m3 m-3"
	moisRangeId.jsonInput = "[\"Mois\", [1, 3]]"
	moisRangeId.fromLayer = 0
	moisRangeId.toLayer = 2
	moisRangeId.layerAggOp = .NONE
	append(&ids, moisRangeId)

	moisAvgId := mio.make_default_oid()
	moisAvgId.name = "Mois"
	moisAvgId.unit = "m3 m-3"
	moisAvgId.jsonInput = "[\"Mois\", [1, 3, \"AVG\"]]"
	moisAvgId.fromLayer = 0
	moisAvgId.toLayer = 2
	moisAvgId.layerAggOp = .AVG
	append(&ids, moisAvgId)

	cropId := mio.make_default_oid()
	cropId.name = "Crop"
	cropId.unit = ""
	cropId.jsonInput = "\"Crop\""
	cropId.displayName = "Crop, with, commas"
	append(&ids, cropId)

	marker(w, "== header, csvSep=,")
	mio.write_output_header_rows(w, ids[:], ",", true, true, true)

	marker(w, "== header, csvSep=,, no header/units rows")
	mio.write_output_header_rows(w, ids[:], ",", false, false, true)

	values := make([dynamic][dynamic]jx.Value, len(ids))
	for i in 0 ..< len(ids) {
		values[i] = make([dynamic]jx.Value, 0)
	}
	// day 0
	append(&values[0], jx.s("2020-09-22"))
	append(&values[1], jx.f(1234.5678))
	append(&values[2], jx.f(99.99999))
	append(&values[3], jx.arr(context.allocator, jx.f(0.1234), jx.f(0.000123456), jx.f(15000.0)))
	append(&values[4], jx.f(250.333333))
	append(&values[5], jx.s("Winter wheat, \"WW\""))
	// day 1
	append(&values[0], jx.s("2020-09-23"))
	append(&values[1], jx.f(0.0))
	append(&values[2], jx.f(-0.0))
	append(&values[3], jx.arr(context.allocator, jx.f(1e6), jx.f(1e-5), jx.f(123456.789)))
	append(&values[4], jx.f(3.14159265))
	append(&values[5], jx.s("plain"))
	// day 2
	append(&values[0], jx.s("2020-09-24"))
	append(&values[1], jx.f(12345678.9))
	append(&values[2], jx.b(true))
	append(&values[3], jx.arr(context.allocator, jx.f(100.0), jx.f(0.0), jx.f(1e20)))
	append(&values[4], jx.f(1e-20))
	append(&values[5], jx.b(false))

	marker(w, "== rows, csvSep=,")
	mio.write_output(w, ids[:], values[:], ",")

	marker(w, "== rows, csvSep=\\t")
	mio.write_output(w, ids[:], values[:], "\t")
}
