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

	date_id := mio.make_default_oid()
	date_id.name = "Date"
	date_id.unit = ""
	date_id.json_input = "\"Date\""
	append(&ids, date_id)

	yield_id := mio.make_default_oid()
	yield_id.name = "Yield"
	yield_id.unit = "kgDM ha-1"
	yield_id.json_input = "\"Yield\""
	append(&ids, yield_id)

	org_biom_id := mio.make_default_oid()
	org_biom_id.name = "OrgBiom"
	org_biom_id.unit = "kgDM ha-1"
	org_biom_id.json_input = "[\"OrgBiom\", \"LEAF\"]"
	org_biom_id.organ = .LEAF
	append(&ids, org_biom_id)

	mois_range_id := mio.make_default_oid()
	mois_range_id.name = "Mois"
	mois_range_id.unit = "m3 m-3"
	mois_range_id.json_input = "[\"Mois\", [1, 3]]"
	mois_range_id.from_layer = 0
	mois_range_id.to_layer = 2
	mois_range_id.layer_agg_op = .NONE
	append(&ids, mois_range_id)

	mois_avg_id := mio.make_default_oid()
	mois_avg_id.name = "Mois"
	mois_avg_id.unit = "m3 m-3"
	mois_avg_id.json_input = "[\"Mois\", [1, 3, \"AVG\"]]"
	mois_avg_id.from_layer = 0
	mois_avg_id.to_layer = 2
	mois_avg_id.layer_agg_op = .AVG
	append(&ids, mois_avg_id)

	crop_id := mio.make_default_oid()
	crop_id.name = "Crop"
	crop_id.unit = ""
	crop_id.json_input = "\"Crop\""
	crop_id.display_name = "Crop, with, commas"
	append(&ids, crop_id)

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
