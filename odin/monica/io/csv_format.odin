// Phase 7 checkpoint 3: src/io/csv-format.h/.cpp - writeOutputHeaderRows and
// writeOutput. writeOutputObj is not ported: it's the "obj-outputs?" sibling
// of writeOutput, and sim-min.json never sets "obj-outputs?" (see
// output.odin's Output_Data comment) - "port on demand".
//
// Number formatting: C++'s `ostream << double` with no precision/flags set
// anywhere in csv-format.cpp uses the stream's default - 6 significant
// digits, %g-style (scientific below 1e-4 or at/above 1e6, trailing zeros
// trimmed). Verified empirically (build/ref/fmt_test.cpp vs a throwaway Odin
// probe) that Odin's `fmt` "%.6g" verb reproduces this byte-for-byte across
// representative magnitudes, including the exponent digit count (e+06, not
// e+006) and negative zero ("-0"). That's the formatting this file uses for
// every NUMBER value.
//
// Line endings: the checked-in sim-min-out_section_*.csv baselines use \r\n
// (the C++ side writes through a text-mode ofstream, which translates `endl`
// / '\n' to \r\n on Windows). io.Writer does no such translation, so every
// line here ends with an explicit "\r\n".
package monica_io

import "core:fmt"
import "core:io"
import "core:strings"
import jx "../../support/jsonx"

@(private)
needs_quote :: proc(s: string, sep: string) -> bool {
	return strings.contains(s, "\n") || strings.contains(s, "\"") || (sep != "" && strings.contains(s, sep))
}

@(private)
write_escaped_to_builder :: proc(b: ^strings.Builder, s: string, sep: string) {
	if needs_quote(s, sep) {
		strings.write_byte(b, '"')
		strings.write_string(b, s)
		strings.write_byte(b, '"')
	} else {
		strings.write_string(b, s)
	}
}

// C++: void monica::writeOutputHeaderRows(ostream&, const vector<OId>&, string, bool, bool, bool)
write_output_header_rows :: proc(
	w: io.Writer,
	outputIds: []OId,
	csvSep: string,
	includeHeaderRow: bool,
	includeUnitsRow: bool,
	includeTimeAgg: bool = true,
	allocator := context.allocator,
) {
	oss1 := strings.builder_make(allocator)
	oss2 := strings.builder_make(allocator)
	oss3 := strings.builder_make(allocator)
	oss4 := strings.builder_make(allocator)

	oidsSize := len(outputIds)
	for oid_in, j in outputIds {
		oid := oid_in
		fromLayer := oid.fromLayer
		toLayer := oid.toLayer
		isOrgan := oid_is_organ(&oid)
		isRange := oid_is_range(&oid) && oid.layerAggOp == .NONE
		if isOrgan {
			// organ is represented just by the value of fromLayer currently
			toLayer = int(oid.organ)
			fromLayer = int(oid.organ)
		} else if isRange {
			fromLayer += 1 // display 1-indexed layer numbers to users
			toLayer += 1
		} else {
			toLayer = fromLayer // aggregated ranges aren't displayed as a range
		}

		for i := fromLayer; i <= toLayer; i += 1 {
			name: string
			if isOrgan {
				name =
					oid.displayName != "" \
					? oid.displayName \
					: strings.concatenate({oid.name, "/", oid_organ_to_string(oid.organ)}, allocator)
			} else if isRange {
				name =
					oid.displayName != "" \
					? oid.displayName \
					: fmt.aprintf("%s_%d", oid.name, i, allocator = allocator)
			} else {
				name = oid.displayName != "" ? oid.displayName : oid.name
			}
			csvSep_ := j + 1 == oidsSize && i == toLayer ? "" : csvSep

			write_escaped_to_builder(&oss1, name, csvSep)
			strings.write_string(&oss1, csvSep_)

			os2 := strings.concatenate({"[", oid.unit, "]"}, allocator)
			write_escaped_to_builder(&oss2, os2, csvSep)
			strings.write_string(&oss2, csvSep_)

			os3 := strings.concatenate({"m:", oid_to_string(&oid, includeTimeAgg, allocator)}, allocator)
			write_escaped_to_builder(&oss3, os3, csvSep)
			strings.write_string(&oss3, csvSep_)

			jsonInputNoQuotes, _ := strings.replace_all(oid.jsonInput, "\"", "", allocator)
			os4 := strings.concatenate({"j:", jsonInputNoQuotes}, allocator)
			write_escaped_to_builder(&oss4, os4, csvSep)
			strings.write_string(&oss4, csvSep_)
		}
	}

	if includeHeaderRow {
		io.write_string(w, strings.to_string(oss1))
		io.write_string(w, "\r\n")
	}
	if includeUnitsRow {
		io.write_string(w, strings.to_string(oss2))
		io.write_string(w, "\r\n")
	}
	if includeTimeAgg {
		io.write_string(w, strings.to_string(oss3))
		io.write_string(w, "\r\n")
		io.write_string(w, strings.to_string(oss4))
		io.write_string(w, "\r\n")
	}
}

@(private)
write_json_scalar :: proc(w: io.Writer, v: jx.Value, sep: string) {
	switch {
	case jx.is_number(v):
		fmt.wprintf(w, "%.6g", jx.number_value(v))
	case jx.is_string(v):
		s := jx.string_value_of(v)
		if needs_quote(s, sep) {
			io.write_byte(w, '"')
			io.write_string(w, s)
			io.write_byte(w, '"')
		} else {
			io.write_string(w, s)
		}
	case jx.is_bool(v):
		io.write_string(w, jx.bool_value_of(v) ? "1" : "0")
	case:
		io.write_string(w, "UNKNOWN")
	}
	io.write_string(w, sep)
}

// C++: void monica::writeOutput(ostream&, const vector<OId>&, const vector<J11Array>&, string)
write_output :: proc(w: io.Writer, outputIds: []OId, values: [][dynamic]jx.Value, csvSep: string) {
	if len(values) == 0 {
		return
	}

	size := len(values[0])
	oidsSize := len(outputIds)
	for k in 0 ..< size {
		for i in 0 ..< oidsSize {
			csvSep_ := i + 1 == oidsSize ? "" : csvSep
			j := values[i][k]
			if jx.is_array(j) {
				items := jx.array_items(j)
				jSize := len(items)
				for jv, jvi in items {
					csvSep__ := jvi + 1 == jSize ? "" : csvSep
					write_json_scalar(w, jv, csvSep__)
				}
				io.write_string(w, csvSep_)
			} else {
				write_json_scalar(w, j, csvSep_)
			}
		}
		io.write_string(w, "\r\n")
	}
	io.flush(w)
}
