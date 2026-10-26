// monica-module: runs one implementation of one MONICA process standalone,
// without MONICA (plan-crop2ml-modules.md §1.4, "standalone").
//
//   monica-module run SoilTemperature <impl> <input.json> [out.csv]
//   monica-module from-sim <sim.json> <input.json>
//
// `run` reads an input file, runs <impl> day by day and writes a CSV (to
// stdout without out.csv): date, soil surface temperature, then one column
// per layer, all with 17 significant digits so that the numbers round-trip.
// <impl> is a module name (monica/modules/soil_temperature's table) or
// "internal", MONICA's own soil temperature (core/soil_temperature.odin) on
// the same input.
//
// `from-sim` writes an input file from a MONICA sim.json (with its crop.json,
// site.json and climate), as MONICA's own soil temperature sees the site: the
// soil profile after MONICA's pedotransfer functions, the initial soil
// moisture, the climate. Next to it, <input>.csv with the daily values.
//
// The input file, in MONICA's units:
//
//   {
//     "Latitude": 52.8, "Albedo": 0.23, "TimeStep": 1,
//     "TAV": 8.9, "TAMP": 18.6,             // annual average / amplitude of monthly averages [°C]
//     "SoilTemperatureParameters": {...},   // MONICA's, optional
//     "Layers": [{"Thickness": 0.1,         // [m]
//                 "BulkDensity": 1500,      // [kg m-3]
//                 "Saturation": 0.4, "FieldCapacity": 0.2,
//                 "PermanentWiltingPoint": 0.05, "SoilMoisture": 0.15,  // [m3 m-3]
//                 "OrganicMatter": 0.013}], // [kg kg-1]
//     "Daily": "input.csv"                  // relative to the input file
//   }
//
// The daily CSV has a header row; columns by name: date (ISO), tmin, tavg,
// tmax [°C], globrad [MJ m-2 d-1], and optionally soil_coverage [0-1],
// snow_depth [mm], temperature_under_snow [°C] (default 0) and
// soil_moisture_1 ... soil_moisture_<n> [m3 m-3] (default: the layers'
// SoilMoisture).
package monica_module

import "../../monica/core"
import stm "../../monica/modules/soil_temperature"
import p "../../monica/params"
import "../../monica/run"
import clim "../../support/climate"
import d "../../support/date"
import jx "../../support/jsonx"
import tl "../../support/tools"
import "core:bufio"
import "core:fmt"
import "core:io"
import "core:os"
import "core:strconv"
import "core:strings"

usage :: proc() {
	fmt.eprintln("usage: monica-module run SoilTemperature <impl> <input.json> [out.csv]")
	fmt.eprintln("       monica-module from-sim <sim.json> <input.json>")
	names := make([dynamic]string, context.temp_allocator)
	append(&names, p.MODULE_IMPL_INTERNAL)
	for e in stm.modules {
		append(&names, e.name)
	}
	fmt.eprintfln("impls: %s", strings.join(names[:], ", ", context.temp_allocator))
}

main :: proc() {
	args := os.args
	switch {
	case len(args) >= 5 && args[1] == "run":
		if args[2] != "SoilTemperature" {
			fmt.eprintfln("unknown process %q (known: SoilTemperature)", args[2])
			os.exit(2)
		}
		out_path := len(args) >= 6 ? args[5] : ""
		if err := run_soil_temperature(args[3], args[4], out_path); err != "" {
			fmt.eprintln(err)
			os.exit(1)
		}
	case len(args) == 4 && args[1] == "from-sim":
		if err := from_sim(args[2], args[3]); err != "" {
			fmt.eprintln(err)
			os.exit(1)
		}
	case:
		usage()
		os.exit(2)
	}
}

// One day of the daily CSV.
Day :: struct {
	date:                   d.Date,
	tmin, tavg, tmax:       f64,
	globrad:                f64,
	soil_coverage:          f64,
	snow_depth:             f64,
	temperature_under_snow: f64,
	soil_moisture:          []f64, // nil: the layers' SoilMoisture
}

run_soil_temperature :: proc(impl, input_path, out_path: string) -> (err: string) {
	in_, days, rerr := read_input(input_path)
	if rerr != "" {
		return rerr
	}
	if len(days) == 0 {
		return "no days in the daily CSV"
	}
	n := len(in_.layer_thickness)
	static_moisture := in_.soil_moisture

	w: io.Writer
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
	w = bufio.writer_to_stream(&bw)
	io.write_string(w, "date,soil_surface_temperature")
	for i in 1 ..= n {
		fmt.wprintf(w, ",layer_%d", i)
	}
	io.write_string(w, "\n")

	out := stm.make_output(n)
	set_day :: proc(in_: ^stm.Input, day: ^Day, static_moisture: []f64) {
		in_.julian_day = int(d.julian_day(day.date))
		in_.tmin, in_.tavg, in_.tmax = day.tmin, day.tavg, day.tmax
		in_.globrad = day.globrad
		in_.soil_coverage = day.soil_coverage
		in_.snow_depth = day.snow_depth
		in_.temperature_under_snow = day.temperature_under_snow
		in_.soil_moisture = day.soil_moisture != nil ? day.soil_moisture : static_moisture
	}

	if impl == p.MODULE_IMPL_INTERNAL {
		// MONICA's own needs a soil column; only these fields are read
		sc: core.Soil_Column
		sc.layers = make([dynamic]core.Soil_Layer, n)
		for &l, i in sc.layers {
			l = core.make_default_soil_layer()
			l.layer_thickness_m = in_.layer_thickness[i]
			l.soil_bulk_density = in_.bulk_density[i]
			l.saturation = in_.saturation[i]
			l.soil_organic_matter = in_.organic_matter[i]
		}
		st := core.make_soil_temperature(&sc, in_.params, in_.time_step)
		for &day in days {
			set_day(&in_, &day, static_moisture)
			core.soil_temperature_step(
				&st,
				in_.tmin,
				in_.tmax,
				in_.globrad,
				in_.soil_coverage,
				in_.snow_depth,
				in_.temperature_under_snow,
			)
			out.soil_surface_temperature = st.soil_surface_temperature
			for l, i in sc.layers {
				out.layer_temperature[i] = l.soil_temperature
			}
			write_row(w, day.date, &out)
		}
		return ""
	}

	entry, ok := stm.find_module(impl)
	if !ok {
		return fmt.aprintf("unknown impl %q", impl)
	}
	m := entry.make()
	defer stm.module_destroy(m)
	for &day, i in days {
		set_day(&in_, &day, static_moisture)
		if i == 0 {
			if ierr := stm.module_init(m, &in_); ierr != "" {
				return ierr
			}
		}
		stm.module_step(m, &in_, &out)
		write_row(w, day.date, &out)
		free_all(context.temp_allocator)
	}
	return ""
}

write_row :: proc(w: io.Writer, date: d.Date, out: ^stm.Output) {
	io.write_string(w, d.to_iso_date_string(date, "", context.temp_allocator))
	write_f64(w, out.soil_surface_temperature)
	for t in out.layer_temperature {
		write_f64(w, t)
	}
	io.write_string(w, "\n")
}

write_f64 :: proc(w: io.Writer, v: f64) {
	buf: [64]byte
	s := strconv.write_float(buf[:], v, 'g', 17, 64)
	if len(s) > 0 && s[0] == '+' {
		s = s[1:]
	}
	io.write_string(w, ",")
	io.write_string(w, s)
}

read_input :: proc(path: string) -> (in_: stm.Input, days: []Day, err: string) {
	jr := jx.read_and_parse_json_file(path)
	if tl.failure(jr.errs) {
		return in_, nil, fmt.aprintf("can't read %s: %v", path, jr.errs.errors)
	}
	j := jr.result
	in_.latitude = jx.double_value(j, "Latitude")
	in_.albedo = jx.double_value(j, "Albedo")
	in_.tav = jx.double_value(j, "TAV")
	in_.tamp = jx.double_value(j, "TAMP")
	in_.time_step = jx.double_value_key_d(j, "TimeStep", 1.0)
	in_.params = p.make_soil_temperature_module_parameters()
	if jx.is_object(jx.get(j, "SoilTemperatureParameters")) {
		_ = p.soil_temperature_module_parameters_merge(&in_.params, jx.get(j, "SoilTemperatureParameters"))
	}
	layers := jx.array_items(jx.get(j, "Layers"))
	n := len(layers)
	if n == 0 {
		return in_, nil, "the input has no Layers"
	}
	in_.layer_thickness = make([]f64, n)
	in_.bulk_density = make([]f64, n)
	in_.saturation = make([]f64, n)
	in_.organic_matter = make([]f64, n)
	in_.field_capacity = make([]f64, n)
	in_.permanent_wilting_point = make([]f64, n)
	in_.soil_moisture = make([]f64, n)
	for l, i in layers {
		in_.layer_thickness[i] = jx.double_value(l, "Thickness")
		in_.bulk_density[i] = jx.double_value(l, "BulkDensity")
		in_.saturation[i] = jx.double_value(l, "Saturation")
		in_.organic_matter[i] = jx.double_value(l, "OrganicMatter")
		in_.field_capacity[i] = jx.double_value(l, "FieldCapacity")
		in_.permanent_wilting_point[i] = jx.double_value(l, "PermanentWiltingPoint")
		in_.soil_moisture[i] = jx.double_value(l, "SoilMoisture")
	}
	daily := jx.string_value(j, "Daily")
	if !tl.is_absolute_path(daily) {
		dir, _ := tl.split_path_to_file(path)
		daily = strings.concatenate({dir, daily})
	}
	days, err = read_daily(daily, n)
	return
}

read_daily :: proc(path: string, no_of_layers: int) -> (days: []Day, err: string) {
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
	for name in ([]string{"date", "tmin", "tavg", "tmax", "globrad"}) {
		if name not_in col {
			return nil, fmt.aprintf("%s: no column %q", path, name)
		}
	}
	sm_cols: []int
	if "soil_moisture_1" in col {
		sm_cols = make([]int, no_of_layers)
		for i in 0 ..< no_of_layers {
			c, ok := col[fmt.tprintf("soil_moisture_%d", i + 1)]
			if !ok {
				return nil, fmt.aprintf("%s: no column soil_moisture_%d", path, i + 1)
			}
			sm_cols[i] = c
		}
	}
	ds := make([dynamic]Day, 0, len(lines) - 1)
	for line, li in lines[1:] {
		if strings.trim_space(line) == "" {
			continue
		}
		fields := strings.split(strings.trim_space(line), ",", context.temp_allocator)
		num :: proc(fields: []string, col: map[string]int, name: string, ok: ^bool) -> f64 {
			i, has := col[name]
			if !has {
				return 0
			}
			if i >= len(fields) {
				ok^ = false
				return 0
			}
			v, vok := strconv.parse_f64(strings.trim_space(fields[i]))
			if !vok {
				ok^ = false
			}
			return v
		}
		ok := true
		day: Day
		if col["date"] < len(fields) {
			day.date = d.from_iso_date_string(strings.trim_space(fields[col["date"]]))
		}
		ok = ok && d.is_valid(day.date)
		day.tmin = num(fields, col, "tmin", &ok)
		day.tavg = num(fields, col, "tavg", &ok)
		day.tmax = num(fields, col, "tmax", &ok)
		day.globrad = num(fields, col, "globrad", &ok)
		day.soil_coverage = num(fields, col, "soil_coverage", &ok)
		day.snow_depth = num(fields, col, "snow_depth", &ok)
		day.temperature_under_snow = num(fields, col, "temperature_under_snow", &ok)
		if sm_cols != nil {
			day.soil_moisture = make([]f64, no_of_layers)
			for c, i in sm_cols {
				v, vok := strconv.parse_f64(c < len(fields) ? strings.trim_space(fields[c]) : "")
				ok = ok && vok
				day.soil_moisture[i] = v
			}
		}
		if !ok {
			return nil, fmt.aprintf("%s:%d: can't read %q", path, li + 2, line)
		}
		append(&ds, day)
	}
	return ds[:], ""
}

// The sim.json handling of cmd/monica-run (paths relative to the sim.json),
// then the model as make_monica_model leaves it.
from_sim :: proc(sim_path, input_path: string) -> (err: string) {
	a := context.allocator
	sim_dir, _ := tl.split_path_to_file(sim_path, a)
	simr := jx.read_and_parse_json_file(sim_path, a)
	if tl.failure(simr.errs) {
		return fmt.aprintf("can't read %s: %v", sim_path, simr.errs.errors)
	}
	simm := make(jx.Object, 0, a)
	for k, v in jx.object_items(simr.result) {
		simm[strings.clone(k, a)] = v
	}
	for key in ([]string{"crop.json", "site.json", "climate.csv"}) {
		pth := jx.string_value_of(simm[key] or_else jx.Value{})
		if pth != "" && !tl.is_absolute_path(pth) {
			simm[strings.clone(key, a)] = jx.s(strings.concatenate({sim_dir, pth}, a), a)
		}
	}
	sim_v := jx.Value(simm)
	cropr := jx.read_and_parse_json_file(jx.string_value(sim_v, "crop.json"), a)
	siter := jx.read_and_parse_json_file(jx.string_value(sim_v, "site.json"), a)
	if tl.failure(cropr.errs) || tl.failure(siter.errs) {
		return "can't read the sim's crop.json or site.json"
	}
	env_json := run.create_env_json_from_json_objects(cropr.result, siter.result, sim_v, a)
	path_to_soil_dir := tl.fix_system_separator(tl.replace_env_vars("${MONICA_PARAMETERS}/soil/", a), a)
	env: run.Env
	if errs := run.env_merge(&env, env_json, path_to_soil_dir, a); tl.failure(errs) {
		return fmt.aprintf("%v", errs.errors)
	}
	if len(env.params.site_params.soil_parameters) == 0 {
		return "the sim has no soil profile"
	}

	// The input a driver gets: gathered where MONICA's own soil temperature
	// reads the soil column, which make_monica_model alone knows.
	env.params.sim_params.modules.soil_temperature_drivers = make([dynamic]string, a)
	append(&env.params.sim_params.modules.soil_temperature_drivers, stm.modules[0].name)
	model := core.make_monica_model(&env.params, a)
	in_ := model.modules.soil_temperature_stepping[0].input
	da := &env.climateData
	in_.tamp, in_.tav = clim.data_accessor_calc_tamp_and_tav(da, a)

	csv_path := strings.concatenate({strings.trim_suffix(input_path, ".json"), ".csv"}, a)
	_, csv_name := tl.split_path_to_file(csv_path, a)
	layers := make([dynamic]jx.Value, 0, len(in_.layer_thickness), a)
	for i in 0 ..< len(in_.layer_thickness) {
		append(
			&layers,
			jx.obj(
				a,
				{"Thickness", jx.f(in_.layer_thickness[i])},
				{"BulkDensity", jx.f(in_.bulk_density[i])},
				{"Saturation", jx.f(in_.saturation[i])},
				{"OrganicMatter", jx.f(in_.organic_matter[i])},
				{"FieldCapacity", jx.f(in_.field_capacity[i])},
				{"PermanentWiltingPoint", jx.f(in_.permanent_wilting_point[i])},
				{"SoilMoisture", jx.f(in_.soil_moisture[i])},
			),
		)
	}
	j := jx.obj(
		a,
		{"Latitude", jx.f(in_.latitude)},
		{"Albedo", jx.f(in_.albedo)},
		{"TimeStep", jx.f(in_.time_step)},
		{"TAV", jx.f(in_.tav)},
		{"TAMP", jx.f(in_.tamp)},
		{"SoilTemperatureParameters", p.soil_temperature_module_parameters_to_json(&in_.params, a)},
		{"Layers", jx.arr(a, ..layers[:])},
		{"Daily", jx.s(csv_name, a)},
	)
	if werr := os.write_entire_file(input_path, transmute([]byte)jx.dump(j, a)); werr != nil {
		return fmt.aprintf("can't write %s: %v", input_path, werr)
	}

	b := strings.builder_make(a)
	strings.write_string(&b, "date,tmin,tavg,tmax,globrad\n")
	buf: [64]byte
	for i in 0 ..< clim.data_accessor_no_of_steps_possible(da) {
		strings.write_string(&b, d.to_iso_date_string(clim.data_accessor_date_for_step(da, i), "", a))
		for acd in ([]clim.ACD{.tmin, .tavg, .tmax, .globrad}) {
			s := strconv.write_float(buf[:], clim.data_accessor_data_for_timestep(da, acd, i), 'g', 17, 64)
			strings.write_string(&b, ",")
			strings.write_string(&b, s[0] == '+' ? s[1:] : s)
		}
		strings.write_string(&b, "\n")
	}
	if werr := os.write_entire_file(csv_path, transmute([]byte)strings.to_string(b)); werr != nil {
		return fmt.aprintf("can't write %s: %v", csv_path, werr)
	}
	return ""
}
