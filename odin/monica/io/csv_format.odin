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
import "core:strconv"
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
	output_ids: []OId,
	csv_sep: string,
	include_header_row: bool,
	include_units_row: bool,
	include_time_agg: bool = true,
	allocator := context.allocator,
) {
	oss1 := strings.builder_make(allocator)
	oss2 := strings.builder_make(allocator)
	oss3 := strings.builder_make(allocator)
	oss4 := strings.builder_make(allocator)

	oids_size := len(output_ids)
	for oid_in, j in output_ids {
		oid := oid_in
		from_layer := oid.from_layer
		to_layer := oid.to_layer
		is_organ := oid_is_organ(&oid)
		is_range := oid_is_range(&oid) && oid.layer_agg_op == .NONE
		if is_organ {
			// organ is represented just by the value of fromLayer currently
			to_layer = int(oid.organ)
			from_layer = int(oid.organ)
		} else if is_range {
			from_layer += 1 // display 1-indexed layer numbers to users
			to_layer += 1
		} else {
			to_layer = from_layer // aggregated ranges aren't displayed as a range
		}

		for i := from_layer; i <= to_layer; i += 1 {
			name: string
			if is_organ {
				name =
					oid.display_name != "" \
					? oid.display_name \
					: strings.concatenate({oid.name, "/", oid_organ_to_string(oid.organ)}, allocator)
			} else if is_range {
				name =
					oid.display_name != "" \
					? oid.display_name \
					: fmt.aprintf("%s_%d", oid.name, i, allocator = allocator)
			} else {
				name = oid.display_name != "" ? oid.display_name : oid.name
			}
			csv_sep_ := j + 1 == oids_size && i == to_layer ? "" : csv_sep

			write_escaped_to_builder(&oss1, name, csv_sep)
			strings.write_string(&oss1, csv_sep_)

			os2 := strings.concatenate({"[", oid.unit, "]"}, allocator)
			write_escaped_to_builder(&oss2, os2, csv_sep)
			strings.write_string(&oss2, csv_sep_)

			os3 := strings.concatenate({"m:", oid_to_string(&oid, include_time_agg, allocator)}, allocator)
			write_escaped_to_builder(&oss3, os3, csv_sep)
			strings.write_string(&oss3, csv_sep_)

			json_input_no_quotes, _ := strings.replace_all(oid.json_input, "\"", "", allocator)
			os4 := strings.concatenate({"j:", json_input_no_quotes}, allocator)
			write_escaped_to_builder(&oss4, os4, csv_sep)
			strings.write_string(&oss4, csv_sep_)
		}
	}

	if include_header_row {
		io.write_string(w, strings.to_string(oss1))
		io.write_string(w, "\r\n")
	}
	if include_units_row {
		io.write_string(w, strings.to_string(oss2))
		io.write_string(w, "\r\n")
	}
	if include_time_agg {
		io.write_string(w, strings.to_string(oss3))
		io.write_string(w, "\r\n")
		io.write_string(w, strings.to_string(oss4))
		io.write_string(w, "\r\n")
	}
}

// `csvSep` decides quoting, `trailing` is what gets written after the value
// ("" for the last column/element). The C++ quotes against csvSep in both
// cases (escapeTokens = "\n\"" + csvSep); deciding by `trailing` instead left
// a last-column string containing the separator unquoted.
//
// NOTE(deviation): `precision` (significant digits) is sim.json's
// output.csv-options.precision, not in the C++. The default 6 keeps the
// C++'s formatting above; 17 makes every number round-trip, for checks that
// compare values exactly.
@(private)
write_json_scalar :: proc(w: io.Writer, v: jx.Value, csv_sep, trailing: string, precision := 6) {
	switch {
	case jx.is_number(v) && precision == 6:
		fmt.wprintf(w, "%.6g", jx.number_value(v))
	case jx.is_number(v):
		buf: [64]byte
		s := strconv.write_float(buf[:], jx.number_value(v), 'g', precision, 64)
		io.write_string(w, s[0] == '+' ? s[1:] : s)
	case jx.is_string(v):
		s := jx.string_value_of(v)
		if needs_quote(s, csv_sep) {
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
	io.write_string(w, trailing)
}

// C++: void monica::writeOutput(ostream&, const vector<OId>&, const vector<J11Array>&, string)
write_output :: proc(
	w: io.Writer,
	output_ids: []OId,
	values: [][dynamic]jx.Value,
	csv_sep: string,
	precision := 6,
) {
	if len(values) == 0 {
		return
	}

	size := len(values[0])
	oids_size := len(output_ids)
	for k in 0 ..< size {
		for i in 0 ..< oids_size {
			csv_sep_ := i + 1 == oids_size ? "" : csv_sep
			j := values[i][k]
			if jx.is_array(j) {
				items := jx.array_items(j)
				j_size := len(items)
				for jv, jvi in items {
					csv_sep__ := jvi + 1 == j_size ? "" : csv_sep
					write_json_scalar(w, jv, csv_sep, csv_sep__, precision)
				}
				io.write_string(w, csv_sep_)
			} else {
				write_json_scalar(w, j, csv_sep, csv_sep_, precision)
			}
		}
		io.write_string(w, "\r\n")
	}
	io.flush(w)
}

// C++: void monica::writeOutputObj(ostream&, const vector<OId>&, const vector<J11Object>&, string)
//
// NOTE(c++-quirk): an output id missing from a row's object writes nothing at
// all - not even its separator - so that row's later columns shift left.
// Reproduced.
write_output_obj :: proc(
	w: io.Writer,
	output_ids: []OId,
	values: []jx.Value,
	csv_sep: string,
	precision := 6,
) {
	oids_size := len(output_ids)
	for o in values {
		obj := jx.object_items(o)
		for &oid, i in output_ids {
			csv_sep_ := i + 1 == oids_size ? "" : csv_sep
			j, found := obj[oid_output_name(&oid, context.temp_allocator)]
			if !found {
				continue
			}
			if jx.is_array(j) {
				items := jx.array_items(j)
				j_size := len(items)
				for jv, jvi in items {
					csv_sep__ := jvi + 1 == j_size ? "" : csv_sep
					write_json_scalar(w, jv, csv_sep, csv_sep__, precision)
				}
				io.write_string(w, csv_sep_)
			} else {
				write_json_scalar(w, j, csv_sep, csv_sep_, precision)
			}
		}
		io.write_string(w, "\r\n")
	}
	io.flush(w)
}
