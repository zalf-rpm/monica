// monica-module run Evapotranspiration <impl> <input.json> [out.csv]
//
// Runs an evapotranspiration module (monica/modules/evapotranspiration's
// table) day by day on an input file and writes a CSV: date, then the outputs
// the module computes (Module_Entry.outputs; for a module with layers also
// soil_moisture_i, evaporation_i, transpiration_i, evapotranspiration_i), with
// 17 significant digits.
//
// The input file, in MONICA's units; every value is optional (default 0):
//
//   {
//     "Latitude": 52.8, "HeightNN": 50,           // [deg], [m]
//     "ReferenceAlbedo": 0.23,                    // MONICA's crop module parameters ...
//     "StomataConductanceAlpha": 40, "SaturationBeta": 2.5,
//     "EvaporationZeta": 40,                      // ... and soil moisture parameters
//     "MaximumEvaporationImpactDepth": 5, "XSACriticalSoilMoisture": 0.1,
//     "Layers": [{"Thickness": 0.1,               // [m]
//                 "PermanentWiltingPoint": 0.05, "FieldCapacity": 0.2,
//                 "SoilMoisture": 0.15}],         // [m3 m-3]; only for modules that need them
//     "Daily": "input.csv"                        // relative to the input file
//   }
//
// Monica_AMEI_potential_Evapotranspiration needs only HeightNN, and no Layers.
//
// The daily CSV has a header row; columns by name: date (ISO), and the fields
// of monica/modules/evapotranspiration's Daily (julian_day defaults to the
// date's). For Monica_AMEI_potential_Evapotranspiration: tavg [°C], wind
// [m s-1], wind_speed_height [m], kc_factor, developmental_stage (0 without a
// crop), crop_remaining_evapotranspiration [mm], vapor_pressure,
// saturated_vapor_pressure [kPa], stomata_resistance [s m-1], net_radiation
// [MJ m-2]. A missing column is 0 and named on stderr. For modules with
// layers also soil_moisture_1 ... soil_moisture_<n> [m3 m-3] (default: the
// layers' SoilMoisture) and crop_transpiration_1 ... [mm] (default 0).
package monica_module

import etm "../../monica/modules/evapotranspiration"
import d "../../support/date"
import jx "../../support/jsonx"
import tl "../../support/tools"
import "core:bufio"
import "core:fmt"
import "core:io"
import "core:os"
import "core:reflect"
import "core:strconv"
import "core:strings"

// One day of the daily CSV.
Et_Day :: struct {
	date:               d.Date,
	daily:              etm.Daily,
	soil_moisture:      []f64, // nil: the layers' SoilMoisture
	crop_transpiration: []f64, // nil: 0
}

run_evapotranspiration :: proc(impl, input_path, out_path: string) -> (err: string) {
	entry, ok := etm.find_module(impl)
	if !ok {
		names := make([dynamic]string, context.temp_allocator)
		for e in etm.modules {
			append(&names, e.name)
		}
		return fmt.aprintf("unknown impl %q (known: %s)", impl, strings.join(names[:], ", ", context.temp_allocator))
	}
	in_, days, rerr := read_et_input(input_path, entry.inputs)
	if rerr != "" {
		return rerr
	}
	if len(days) == 0 {
		return "no days in the daily CSV"
	}
	n := len(in_.layer_thickness)
	if entry.needs_layers && n == 0 {
		return fmt.aprintf("%s needs the soil profile (Layers)", impl)
	}
	static_moisture := in_.soil_moisture
	no_transpiration := make([]f64, n)

	bw: bufio.Writer
	if out_path == "" {
		bufio.writer_init(&bw, os.to_stream(os.stdout), 1 << 16)
	} else {
		f, ferr := os.open(out_path, os.O_WRONLY | os.O_CREATE | os.O_TRUNC, os.Permissions_Default_File)
		if ferr != nil {
			return fmt.aprintf("can't write %s: %v", out_path, ferr)
		}
		bufio.writer_init(&bw, os.to_stream(f), 1 << 16)
	}
	defer bufio.writer_flush(&bw)
	w := bufio.writer_to_stream(&bw)

	// the columns: the module's scalar outputs, and its layers
	columns := make([dynamic]reflect.Struct_Field, context.allocator)
	for f in reflect.struct_fields_zipped(etm.Output) {
		if f.type.id != f64 {
			continue
		}
		if entry.outputs != nil && !contains(entry.outputs, f.name) {
			continue
		}
		append(&columns, f)
	}
	with_layers := entry.needs_layers && entry.outputs == nil
	layer_columns := []string{"soil_moisture", "evaporation", "transpiration", "evapotranspiration"}
	io.write_string(w, "date")
	for f in columns {
		io.write_string(w, ",")
		io.write_string(w, f.name)
	}
	if with_layers {
		for name in layer_columns {
			for i in 1 ..= n {
				fmt.wprintf(w, ",%s_%d", name, i)
			}
		}
	}
	io.write_string(w, "\n")

	out := etm.make_output(n)
	m := entry.make(context.allocator)
	defer entry.destroy(m, context.allocator)
	for &day, i in days {
		in_.daily = day.daily
		if n > 0 {
			in_.soil_moisture = day.soil_moisture != nil ? day.soil_moisture : static_moisture
			in_.crop_transpiration = day.crop_transpiration != nil ? day.crop_transpiration : no_transpiration
		}
		if i == 0 {
			entry.init(m, &in_, context.allocator)
		}
		entry.step(m, &in_, &out, context.allocator)
		io.write_string(w, d.to_iso_date_string(day.date, "", context.temp_allocator))
		for f in columns {
			write_f64(w, (^f64)(uintptr(&out) + f.offset)^)
		}
		if with_layers {
			for name in layer_columns {
				vs := reflect.struct_field_value_by_name(out, name).([]f64)
				for v in vs {
					write_f64(w, v)
				}
			}
		}
		io.write_string(w, "\n")
		free_all(context.temp_allocator)
	}
	return ""
}

@(private = "file")
contains :: proc(xs: []string, x: string) -> bool {
	for y in xs {
		if y == x {
			return true
		}
	}
	return false
}

// `inputs`: the Daily fields the module reads, nil: all
read_et_input :: proc(path: string, inputs: []string = nil) -> (in_: etm.Input, days: []Et_Day, err: string) {
	jr := jx.read_and_parse_json_file(path)
	if tl.failure(jr.errs) {
		return in_, nil, fmt.aprintf("can't read %s: %v", path, jr.errs.errors)
	}
	j := jr.result
	in_.latitude = jx.double_value(j, "Latitude")
	in_.height_nn = jx.double_value(j, "HeightNN")
	in_.reference_albedo = jx.double_value(j, "ReferenceAlbedo")
	in_.evaporation_zeta = jx.double_value(j, "EvaporationZeta")
	in_.maximum_evaporation_impact_depth = jx.double_value(j, "MaximumEvaporationImpactDepth")
	in_.xsa_critical_soil_moisture = jx.double_value(j, "XSACriticalSoilMoisture")
	in_.stomata_conductance_alpha = jx.double_value(j, "StomataConductanceAlpha")
	in_.saturation_beta = jx.double_value(j, "SaturationBeta")
	layers := jx.array_items(jx.get(j, "Layers"))
	n := len(layers)
	in_.layer_thickness = make([]f64, n)
	in_.permanent_wilting_point = make([]f64, n)
	in_.field_capacity = make([]f64, n)
	in_.soil_moisture = make([]f64, n)
	for l, i in layers {
		in_.layer_thickness[i] = jx.double_value(l, "Thickness")
		in_.permanent_wilting_point[i] = jx.double_value(l, "PermanentWiltingPoint")
		in_.field_capacity[i] = jx.double_value(l, "FieldCapacity")
		in_.soil_moisture[i] = jx.double_value(l, "SoilMoisture")
	}
	daily := jx.string_value(j, "Daily")
	if !tl.is_absolute_path(daily) {
		dir, _ := tl.split_path_to_file(path)
		daily = strings.concatenate({dir, daily})
	}
	days, err = read_et_daily(daily, n, inputs)
	return
}

read_et_daily :: proc(path: string, no_of_layers: int, inputs: []string) -> (days: []Et_Day, err: string) {
	data, rerr := os.read_entire_file(path, context.allocator)
	if rerr != nil {
		return nil, fmt.aprintf("can't read %s: %v", path, rerr)
	}
	lines := strings.split_lines(strings.trim_space(string(data)))
	if len(lines) < 2 {
		return nil, fmt.aprintf("%s: no data rows", path)
	}
	col: map[string]int
	for name, i in strings.split(strings.trim_space(lines[0]), ",") {
		col[strings.trim_space(name)] = i
	}
	if "date" not_in col {
		return nil, fmt.aprintf("%s: no column \"date\"", path)
	}
	missing := make([dynamic]string, context.temp_allocator)
	for f in reflect.struct_fields_zipped(etm.Daily) {
		if f.name not_in col && f.name != "julian_day" && (inputs == nil || contains(inputs, f.name)) {
			append(&missing, f.name)
		}
	}
	if len(missing) > 0 {
		fmt.eprintfln("%s: no column for %s - 0", path, strings.join(missing[:], ", ", context.temp_allocator))
	}
	layer_cols :: proc(col: map[string]int, prefix: string, n: int) -> (cols: []int, err: string) {
		if fmt.tprintf("%s_1", prefix) not_in col {
			return nil, ""
		}
		cols = make([]int, n)
		for i in 0 ..< n {
			c, ok := col[fmt.tprintf("%s_%d", prefix, i + 1)]
			if !ok {
				return nil, fmt.aprintf("no column %s_%d", prefix, i + 1)
			}
			cols[i] = c
		}
		return cols, ""
	}
	sm_cols, serr := layer_cols(col, "soil_moisture", no_of_layers)
	tr_cols, terr := layer_cols(col, "crop_transpiration", no_of_layers)
	if serr != "" || terr != "" {
		return nil, fmt.aprintf("%s: %s%s", path, serr, terr)
	}

	ds := make([dynamic]Et_Day, 0, len(lines) - 1)
	for line, li in lines[1:] {
		if strings.trim_space(line) == "" {
			continue
		}
		fields := strings.split(strings.trim_space(line), ",", context.temp_allocator)
		field :: proc(fields: []string, i: int) -> string {
			return i < len(fields) ? strings.trim_space(fields[i]) : ""
		}
		ok := true
		day: Et_Day
		day.date = d.from_iso_date_string(field(fields, col["date"]))
		ok = ok && d.is_valid(day.date)
		dy := &day.daily
		dy.julian_day = int(d.julian_day(day.date))
		// every field of Daily, by name
		for f in reflect.struct_fields_zipped(etm.Daily) {
			c, has := col[f.name]
			if !has {
				continue
			}
			s := field(fields, c)
			ptr := rawptr(uintptr(dy) + f.offset)
			switch f.type.id {
			case f64:
				v, vok := strconv.parse_f64(s)
				ok = ok && vok
				(^f64)(ptr)^ = v
			case int:
				v, vok := strconv.parse_int(s)
				ok = ok && vok
				(^int)(ptr)^ = v
			case bool:
				(^bool)(ptr)^ = s == "1" || s == "true"
				ok = ok && (s == "1" || s == "true" || s == "0" || s == "false")
			}
		}
		read_layers :: proc(fields: []string, cols: []int, ok: ^bool) -> []f64 {
			if cols == nil {
				return nil
			}
			vs := make([]f64, len(cols))
			for c, i in cols {
				v, vok := strconv.parse_f64(c < len(fields) ? strings.trim_space(fields[c]) : "")
				ok^ = ok^ && vok
				vs[i] = v
			}
			return vs
		}
		day.soil_moisture = read_layers(fields, sm_cols, &ok)
		day.crop_transpiration = read_layers(fields, tr_cols, &ok)
		if !ok {
			return nil, fmt.aprintf("%s:%d: can't read %q", path, li + 2, line)
		}
		append(&ds, day)
	}
	return ds[:], ""
}
