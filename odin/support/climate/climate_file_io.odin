// Port of mas_cpp_misc/climate/climate-file-io.{h,cpp}.
//
// No istream abstraction in Odin: every reader operates on `[]string` lines
// (1-based line numbers, matching CSVViaHeaderOptions.lineNoOf{HeaderLine,
// DataStart,DataEnd}) instead of streaming getline() calls. Files are read
// whole via os.read_entire_file (NOT tl.read_file, which strips \n/\r and
// concatenates lines - useless for a line-oriented CSV parser) and split on
// "\n"; a trailing '\r' is stripped per-field below, matching what the C++
// does defensively after splitting each line.
//
// Dropped: gzip (.gz) support (needs kj::GzipInputStream; no .gz fixture is in
// scope) and the two `extern "C"` DLL exports (Climate_readClimateDataFrom...,
// Climate_freeCString - confirmed zero references anywhere under src/).
package climate

import "core:os"
import "core:strconv"
import "core:strings"
import d "../date"
import jx "../jsonx"
import tl "../tools"

// C++: std::map<Climate::ACD, std::function<double(double)>> CSVViaHeaderOptions::convertFn
//
// Per plan-odin.md phase 2 ("Watch for") and CONVENTIONS §... (std::function is
// not ported): the same enum+apply treatment jsonx.Transform already uses.
Convert_Op :: enum {
	NONE,
	MUL,
	DIV,
	ADD,
	SUB,
}

apply_csv_convert :: proc(c: Csv_Convert, v: f64) -> f64 {
	switch c.op {
	case .NONE:
		return v
	case .MUL:
		return v * c.value
	case .DIV:
		return v / c.value
	case .ADD:
		return v + c.value
	case .SUB:
		return v - c.value
	}
	return v
}

Csv_Convert :: struct {
	op:    Convert_Op,
	value: f64,
}

// C++: std::map<std::string, std::pair<std::string, double>> CSVViaHeaderOptions::convert
//
// Keyed by header column name (unlike convertFn, which is keyed by ACD and
// only populated once the header row has been read).
Header_Convert_Spec :: struct {
	op:    string,
	value: f64,
}

// C++: struct Climate::CSVViaHeaderOptions : public Tools::Json11Serializable
Csv_Via_Header_Options :: struct {
	separator:          string,
	startDate:          d.Date,
	endDate:            d.Date,
	noOfHeaderLines:    int,
	lineNoOfHeaderLine: int,
	lineNoOfDataStart:  int,
	lineNoOfDataEnd:    int,
	latitude:           f64,
	header:             [dynamic]string,
	headerName2ACDName: map[string]string,
	convert:            map[string]Header_Convert_Spec,
	convertFn:          map[ACD]Csv_Convert,
	datePattern:        string,
}

// C++ in-class initialisers: separator(","), noOfHeaderLines{1},
// lineNoOfHeaderLine{-1}, lineNoOfDataStart{-2}, lineNoOfDataEnd{-1}, latitude{0}
make_csv_via_header_options :: proc() -> Csv_Via_Header_Options {
	return Csv_Via_Header_Options {
		separator = ",",
		noOfHeaderLines = 1,
		lineNoOfHeaderLine = -1,
		lineNoOfDataStart = -2,
		lineNoOfDataEnd = -1,
	}
}

// C++: Tools::Errors CSVViaHeaderOptions::merge(json11::Json j)
//
// NOTE(c++-quirk): the C++ accumulates a warning into a local `errors` (for the
// lineNoOfDataEnd check below) but always `return {};` at the end, discarding
// it. Reproduced: this always returns a fresh empty Errors too.
csv_via_header_options_merge :: proc(
	o: ^Csv_Via_Header_Options,
	j: jx.Value,
	allocator := context.allocator,
) -> tl.Errors {
	header_names := make(map[string]string, allocator)
	for k, v in jx.object_items(jx.get(j, "header-to-acd-names")) {
		if jx.is_array(v) && len(jx.array_items(v)) == 3 {
			items := jx.array_items(v)
			header_names[k] = jx.string_value_of(items[0])
			if o.convert == nil {
				o.convert = make(map[string]Header_Convert_Spec, allocator)
			}
			o.convert[k] = Header_Convert_Spec{jx.string_value_of(items[1]), jx.number_value(items[2])}
		} else if jx.is_array(v) && len(jx.array_items(v)) == 2 {
			items := jx.array_items(v)
			if jx.string_value_of(items[0]) == "pattern-date" {
				header_names[k] = jx.string_value_of(items[0])
				o.datePattern = jx.string_value_of(items[1])
			}
		} else {
			header_names[k] = jx.string_value_of(v)
		}
	}
	o.headerName2ACDName = header_names
	jx.set_string_value_d(&o.separator, j, "csv-separator", ",")

	jx.set_iso_date_value(&o.startDate, j, "start-date")
	jx.set_iso_date_value(&o.endDate, j, "end-date")

	jx.set_double_value(&o.latitude, j, "latitude")

	o.lineNoOfDataStart = jx.int_value_key_d(j, "line-no-of-data-start", o.lineNoOfDataStart)

	jx.set_string_vector(&o.header, j, "header")
	if len(o.header) == 0 {
		o.noOfHeaderLines = jx.int_value_key_d(
			j,
			"no-of-climate-file-header-lines",
			o.noOfHeaderLines,
		)
		o.lineNoOfHeaderLine = jx.int_value_key_d(j, "line-no-of-header-line", o.lineNoOfHeaderLine)
		if o.lineNoOfHeaderLine < 0 {
			o.lineNoOfHeaderLine = 1
		}
		if o.lineNoOfDataStart < 0 {
			o.lineNoOfDataStart = o.lineNoOfHeaderLine + o.noOfHeaderLines
		}
	} else {
		o.lineNoOfHeaderLine = -1
		if o.lineNoOfDataStart < 0 {
			o.lineNoOfDataStart = 1
		}
	}

	o.lineNoOfDataEnd = jx.int_value_key_d(j, "line-no-of-data-end", o.lineNoOfDataEnd)
	if o.lineNoOfDataEnd > 0 && o.lineNoOfDataEnd < o.lineNoOfDataStart {
		o.lineNoOfDataEnd = -1
	}

	return tl.Errors{}
}

// C++: json11::Json CSVViaHeaderOptions::to_json() const
//
// NOTE(c++-quirk): the C++ builds a local `convert_` J11Object from `convert`
// but never puts it in the returned object - dead code. Not reproduced (there
// is nothing to reproduce: the "convert" key is simply never emitted).
csv_via_header_options_to_json :: proc(o: ^Csv_Via_Header_Options, a: Allocator) -> jx.Value {
	header_names := make(jx.Object, 0, a)
	for k, v in o.headerName2ACDName {
		if spec, ok := o.convert[k]; ok {
			header_names[strings.clone(k, a)] = jx.arr(a, jx.s(v, a), jx.s(spec.op, a), jx.f(spec.value))
		} else {
			header_names[strings.clone(k, a)] = jx.s(v, a)
		}
	}

	start_iso := d.to_iso_date_string(o.startDate, "", a)
	end_iso := d.to_iso_date_string(o.endDate, "", a)

	return jx.obj(
		a,
		{"type", jx.sl("CSVViaHeaderOptions")},
		{"csv-separator", jx.s(o.separator, a)},
		{"start-date", jx.s(start_iso, a)},
		{"end-date", jx.s(end_iso, a)},
		{"no-of-climate-file-header-lines", jx.i(o.noOfHeaderLines)},
		{"line-no-of-header-line", jx.i(o.lineNoOfHeaderLine)},
		{"line-no-of-data-start", jx.i(o.lineNoOfDataStart)},
		{"line-no-of-data-end", jx.i(o.lineNoOfDataEnd)},
		{"header-to-acd-names", jx.Value(header_names)},
		{"latitude", jx.f(o.latitude)},
		{"header", prim_arr_string(o.header[:], a)},
	)
}

// C++: Tools::EResult<Climate::DataAccessor> Climate::readClimateDataFromCSVInputStreamViaHeaders(
//        istream&, CSVViaHeaderOptions, bool)
read_climate_data_from_csv_lines_via_headers :: proc(
	lines: []string,
	options_in: Csv_Via_Header_Options,
	strict_date_checking := true,
	allocator := context.allocator,
) -> tl.EResult(Data_Accessor) {
	options := options_in

	header := make([dynamic]ACD, 0, allocator)
	s := ""

	if options.noOfHeaderLines > 0 || len(options.header) > 0 {
		if options.lineNoOfHeaderLine >= 1 && options.lineNoOfHeaderLine <= len(lines) {
			s = lines[options.lineNoOfHeaderLine - 1]
		}

		r: [dynamic]string
		if len(options.header) == 0 {
			r = tl.split_string(s, options.separator, true, allocator)
		} else {
			r = make([dynamic]string, 0, len(options.header), allocator)
			append(&r, ..options.header[:])
		}

		// C++: if (r.back().empty()) r.pop_back(); - guarded here against an
		// empty `r` (undefined behaviour in the C++, unreachable for any real
		// header line).
		if len(r) > 0 && len(r[len(r) - 1]) == 0 {
			pop(&r)
		}
		if len(r) > 0 {
			last := r[len(r) - 1]
			if len(last) > 0 && last[len(last) - 1] == '\r' {
				r[len(r) - 1] = last[:len(last) - 1]
			}
		}

		n2acd := name2acd(allocator)
		for col_name in r {
			tcn := tl.trim(col_name, " \t\f\v\n\r", allocator)
			repl_col_name := tcn in options.headerName2ACDName ? options.headerName2ACDName[tcn] : ""
			lookup_name := len(repl_col_name) == 0 ? tcn : repl_col_name
			acd_found, found := n2acd[lookup_name]
			append(&header, found ? acd_found : ACD.skip)

			if len(options.convert) > 0 {
				if spec, ok := options.convert[tcn]; ok {
					// C++: `n2acd[replColName.empty() ? tcn : replColName]` uses
					// operator[], which auto-vivifies ACD(0) == day for a name not
					// already in the map - unlike the header.push_back lookup just
					// above, which uses .find() and falls back to `skip`.
					// Reproduced: this second lookup defaults to .day, not .skip.
					acd_for_convert := found ? acd_found : ACD.day
					if options.convertFn == nil {
						options.convertFn = make(map[ACD]Csv_Convert, allocator)
					}
					switch spec.op {
					case "*":
						options.convertFn[acd_for_convert] = Csv_Convert{.MUL, spec.value}
					case "/":
						options.convertFn[acd_for_convert] = Csv_Convert{.DIV, spec.value}
					case "+":
						options.convertFn[acd_for_convert] = Csv_Convert{.ADD, spec.value}
					case "-":
						options.convertFn[acd_for_convert] = Csv_Convert{.SUB, spec.value}
					}
				}
			}
		}
	}

	if len(header) == 0 {
		res: tl.EResult(Data_Accessor)
		res.allocator = allocator
		res.result = make_data_accessor(allocator)
		tl.append_errorf(
			&res,
			"Couldn't match any column names to internally used names. Read CSV header line was: %s",
			s,
		)
		return res
	}

	return read_climate_data_from_csv_lines(
		lines,
		header[:],
		options,
		strict_date_checking,
		allocator,
	)
}

// ACD.last + 1, as a compile-time constant - the size of the per-line scratch
// arrays in read_climate_data_from_csv_lines.
ACD_SIZE :: int(ACD.last) + 1

// C++: Date::operator==(other) compares only year()/month()/day()/isRelativeDate()
// - NOT the leap-year-table-selection flag. Two Dates built through different
// paths (date arithmetic vs field-by-field parsing) can carry different flag
// bits while representing the same logical date, so using d.Date directly as
// an Odin map key (which hashes/compares ALL struct fields) silently drops
// entries whose flags happen to differ. Keying by this normalised tuple instead
// reproduces the C++ map<Date,...> equivalence exactly.
@(private)
Date_Key :: struct {
	y:           u16,
	m:           u8,
	dy:          u8,
	is_relative: bool,
}

@(private)
date_key :: proc(dt: d.Date) -> Date_Key {
	return Date_Key {
		y = u16(d.year(dt)),
		m = d.month(dt),
		dy = d.day(dt),
		is_relative = d.is_relative_date(dt),
	}
}

// C++: Tools::EResult<Climate::DataAccessor> Climate::readClimateDataFromCSVInputStream(
//        istream&, vector<ACD>, const CSVViaHeaderOptions&, bool)
read_climate_data_from_csv_lines :: proc(
	lines: []string,
	header: []ACD,
	options: Csv_Via_Header_Options,
	strict_date_checking := true,
	allocator := context.allocator,
) -> tl.EResult(Data_Accessor) {
	res: tl.EResult(Data_Accessor)
	res.allocator = allocator

	separator := options.separator
	start_date := options.startDate
	end_date := options.endDate
	convert := options.convertFn

	is_start_date_valid := d.is_valid(start_date)
	is_end_date_valid := d.is_valid(end_date)

	// we store all data in a map to also manage csv files with wrong order
	data := make(map[Date_Key]map[ACD]f64, allocator)
	have_min_max := false
	min_date, max_date: d.Date

	line_no := 1
	for s in lines {
		if line_no < options.lineNoOfDataStart {
			line_no += 1
			continue
		}
		if options.lineNoOfDataEnd > 0 && line_no > options.lineNoOfDataEnd {
			break
		}
		line_no += 1

		r := tl.split_string(s, separator, false, allocator)
		if len(r) == 0 {
			continue
		}
		// remove a possible trailing \r, when reading Windows-line-ended files
		last := r[len(r) - 1]
		if len(last) > 0 && last[len(last) - 1] == '\r' {
			r[len(r) - 1] = last[:len(last) - 1]
		}
		if len(r) < len(header) {
			// "Skipping line ... because of less elements than expected" - a
			// debug/warning message only, not reproduced (see the package note
			// on stdout side effects); the line is skipped either way.
			continue
		}

		date: d.Date
		used_vs: [ACD_SIZE]bool
		vs: [ACD_SIZE]f64

		parse_ok := true
		field_loop: for i in 0 ..< len(header) {
			acdi := header[i]
			field := r[i]
			#partial switch acdi {
			case .day:
				v, ok := strconv.parse_int(field)
				if !ok {
					parse_ok = false
					break field_loop
				}
				d.set_day(&date, u8(v))
			case .month:
				v, ok := strconv.parse_int(field)
				if !ok {
					parse_ok = false
					break field_loop
				}
				d.set_month(&date, u8(v))
			case .year:
				v, ok := strconv.parse_int(field)
				if !ok {
					parse_ok = false
					break field_loop
				}
				d.set_year(&date, u16(v))
			case .isoDate:
				date = d.from_iso_date_string(field)
			case .patternDate:
				date = d.from_pattern_date_string(field, options.datePattern, allocator = allocator)
			case .deDate:
				dmy := tl.split_string(field, ".", true, allocator)
				if len(dmy) == 3 {
					dv, dok := strconv.parse_int(dmy[0])
					mv, mok := strconv.parse_int(dmy[1])
					yv, yok := strconv.parse_int(dmy[2])
					if !dok || !mok || !yok {
						parse_ok = false
						break field_loop
					}
					d.set_day(&date, u8(dv))
					d.set_month(&date, u8(mv))
					d.set_year(&date, u16(yv))
				}
			case .skip:
			// ignore element
			case:
				v, ok := strconv.parse_f64(field)
				if !ok {
					parse_ok = false
					break field_loop
				}
				cv := v
				if spec, cok := convert[acdi]; cok {
					cv = apply_csv_convert(spec, v)
				}
				vs[int(acdi)] = cv
				used_vs[int(acdi)] = true
			}
		}
		if !parse_ok {
			tl.append_errorf(
				&res,
				"Climate data error: Error converting one of the (climate) elements in the line: \n%s",
				s,
			)
			res.result = make_data_accessor(allocator)
			return res
		}

		if is_start_date_valid && d.lt(date, start_date) {
			continue
		}
		if is_end_date_valid && d.gt(date, end_date) {
			continue
		}
		if !d.is_valid(date) {
			continue
		}
		if !used_vs[int(ACD.tmin)] || !used_vs[int(ACD.tmax)] || !used_vs[int(ACD.precip)] {
			continue
		}

		// if we miss the average air temperature but have min/max, average them
		if !used_vs[int(ACD.tavg)] {
			vs[int(ACD.tavg)] = (vs[int(ACD.tmin)] + vs[int(ACD.tmax)]) / 2.0
			used_vs[int(ACD.tavg)] = true
		}

		if !used_vs[int(ACD.globrad)] && used_vs[int(ACD.sunhours)] {
			vs[int(ACD.globrad)] = tl.sunshine2global_radiation(
				int(d.julian_day(date)),
				vs[int(ACD.sunhours)],
				options.latitude,
			)
			used_vs[int(ACD.globrad)] = true
		} else if !used_vs[int(ACD.globrad)] {
			continue
		}

		vsm := make(map[ACD]f64, allocator)
		for k in 0 ..< ACD_SIZE {
			if used_vs[k] {
				vsm[ACD(k)] = vs[k]
			}
		}
		data[date_key(date)] = vsm

		if !have_min_max {
			min_date = date
			max_date = date
			have_min_max = true
		} else {
			if d.lt(date, min_date) {
				min_date = date
			}
			if d.gt(date, max_date) {
				max_date = date
			}
		}
	}

	if len(data) == 0 {
		tl.append_error(&res, "Climate data error: No data could be read from file!")
		res.result = make_data_accessor(allocator)
		return res
	}

	// if we have no dates or don't do strict date checking, set start/end from data
	if !is_start_date_valid || !strict_date_checking {
		start_date = min_date
	}
	if !is_end_date_valid || !strict_date_checking {
		end_date = max_date
	}

	no_of_days := d.diff(end_date, start_date) + 1
	if strict_date_checking && len(data) < no_of_days {
		tl.append_errorf(
			&res,
			"Climate data error: Read timeseries data between %s and %s (%d days) is incomplete. There are just %d days in read dataset!",
			d.to_iso_date_string(start_date, "", allocator),
			d.to_iso_date_string(end_date, "", allocator),
			no_of_days,
			len(data),
		)
		res.result = make_data_accessor(allocator)
		return res
	}

	// rewrite data into vectors of single elements
	da_data := make(map[ACD][dynamic]f64, allocator)
	for dt := start_date; d.le(dt, end_date); dt = d.add(dt, 1) {
		day_vals, ok := data[date_key(dt)]
		if !ok {
			continue
		}
		for acd, v in day_vals {
			if acd not_in da_data {
				da_data[acd] = make([dynamic]f64, 0, allocator)
			}
			row := da_data[acd]
			append(&row, v)
			da_data[acd] = row
		}
	}

	// check if all vectors have the same length
	sizes := 0
	for _, v in da_data {
		sizes += len(v)
	}
	if len(da_data) > 0 && sizes % len(da_data) != 0 {
		tl.append_error(
			&res,
			"Climate data error: At least one of the climate elements has less elements than the others!",
		)
		res.result = make_data_accessor(allocator)
		return res
	}

	da := make_data_accessor_range(start_date, end_date, allocator)
	for acd, v in da_data {
		if len(v) > 0 {
			data_accessor_add_climate_data(&da, acd, v[:], allocator)
		}
	}

	res.result = da
	return res
}

// C++: Tools::EResult<Climate::DataAccessor> Climate::readClimateDataFromCSVFileViaHeaders(
//        std::string, const CSVViaHeaderOptions&, bool)
//
// NOTE: gzip (.gz) support is dropped - it needs kj::GzipInputStream, and no
// gzip'd climate file is part of this port's scope. A .gz path fails with an
// explicit error instead of silently mis-reading it as plain text.
read_climate_data_from_csv_file_via_headers :: proc(
	path_to_file: string,
	options: Csv_Via_Header_Options,
	strict_date_checking := true,
	allocator := context.allocator,
) -> tl.EResult(Data_Accessor) {
	res: tl.EResult(Data_Accessor)
	res.allocator = allocator

	fixed := tl.fix_system_separator(path_to_file, allocator)
	if strings.has_suffix(fixed, ".gz") {
		tl.append_errorf(
			&res,
			"Could not open climate file %s. (gzip climate files are not supported by this port.)",
			fixed,
		)
		res.result = make_data_accessor(allocator)
		return res
	}

	raw, rerr := os.read_entire_file(fixed, allocator)
	if rerr != nil {
		tl.append_errorf(&res, "Could not open climate file %s.", fixed)
		res.result = make_data_accessor(allocator)
		return res
	}
	content := string(raw)
	lines := tl.split_string(content, "\n", false, allocator)

	return read_climate_data_from_csv_lines_via_headers(
		lines[:],
		options,
		strict_date_checking,
		allocator,
	)
}

// C++: Tools::EResult<Climate::DataAccessor> Climate::readClimateDataFromCSVFilesViaHeaders(
//        const vector<string>&, const CSVViaHeaderOptions&)
read_climate_data_from_csv_files_via_headers :: proc(
	paths_to_files: []string,
	options: Csv_Via_Header_Options,
	allocator := context.allocator,
) -> tl.EResult(Data_Accessor) {
	res: tl.EResult(Data_Accessor)
	res.allocator = allocator
	final_da := make_data_accessor(allocator)

	for path_to_file in paths_to_files {
		if strings.has_prefix(path_to_file, "capnp") {
			continue // skip capnproto sturdy refs
		}

		eda := read_climate_data_from_csv_file_via_headers(path_to_file, options, false, allocator)

		if !data_accessor_is_valid(&final_da) {
			final_da = eda.result
		} else {
			data_accessor_merge_climate_data(&final_da, eda.result, true, allocator)
		}
		if tl.failure(eda.errs) {
			tl.append_errors(&res, eda.errs)
		}
	}

	if d.is_valid(options.startDate) && d.is_valid(options.endDate) {
		no_of_days := d.diff(options.endDate, options.startDate) + 1
		if data_accessor_no_of_steps_possible(&final_da) < no_of_days {
			tl.append_errorf(
				&res,
				"Read time-series data between %s and %s (%d days) is incomplete. There are just %d days in read dataset.",
				d.to_iso_date_string(options.startDate, "", allocator),
				d.to_iso_date_string(options.endDate, "", allocator),
				no_of_days,
				data_accessor_no_of_steps_possible(&final_da),
			)
			res.result = make_data_accessor(allocator)
			return res
		}
	}

	res.result = final_da
	return res
}

// C++: Tools::EResult<Climate::DataAccessor> Climate::readClimateDataFromCSVStringViaHeaders(
//        const std::string&, CSVViaHeaderOptions)
read_climate_data_from_csv_string_via_headers :: proc(
	csv_string: string,
	options: Csv_Via_Header_Options,
	allocator := context.allocator,
) -> tl.EResult(Data_Accessor) {
	lines := tl.split_string(csv_string, "\n", false, allocator)
	return read_climate_data_from_csv_lines_via_headers(lines[:], options, true, allocator)
}

// C++: template<class Collection> J11Array Tools::toPrimJsonArray(const Collection&)
@(private)
prim_arr_string :: proc(vals: []string, a: Allocator) -> jx.Value {
	out := make(jx.Array, 0, len(vals), a)
	for v in vals {
		append(&out, jx.s(v, a))
	}
	return jx.Value(out)
}
