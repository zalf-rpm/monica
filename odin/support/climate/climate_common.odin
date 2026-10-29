// Port of the src/-reachable subset of mas_cpp_misc/climate/climate-common.{h,cpp}.
//
// Dropped (confirmed zero references anywhere under src/ - legacy DB-export
// helpers, not reached by any oracle in this port): availableClimateData2{CLM,
// Werex,WettReg}DBColName, availableClimateData2CarbiocialDBColNameAndScaleFactor,
// availableClimateData2UserSqliteDBColNameAndScaleFactor,
// availableClimateData2{Star,}DBColName, availableClimateData2Name,
// availableClimateData2unit, YearRange, snapToRaster.
package climate

import "base:runtime"
import "core:fmt"
import "core:strconv"
import "core:strings"
import d "../date"
import jx "../jsonx"
import tl "../tools"

Allocator :: runtime.Allocator

// C++: enum Climate::AvailableClimateData (aka ACD)
//
// Values are load-bearing: DataAccessor::to_json keys its "data" object by
// `to_string(int(acd))`, so these must match the C++ numbering exactly for the
// dump to be byte-identical.
ACD :: enum int {
	day         = 0,
	month       = 1,
	year        = 2,
	tmin        = 3,
	tavg        = 4,
	tmax        = 5,
	precip      = 6,
	precipOrig  = 7,
	globrad     = 8,
	wind        = 9,
	sunhours    = 10,
	cloudamount = 11,
	relhumid    = 12,
	airpress    = 13,
	vaporpress  = 14,
	isoDate     = 15,
	deDate      = 16,
	co2         = 17,
	o3          = 18,
	et0         = 19,
	skip        = 20,
	patternDate = 21,
	x1          = 22,
	x2          = 23,
	x3          = 24,
	x4          = 25,
	x5          = 26,
	x6          = 27,
	x7          = 28,
	x8          = 29,
	x9          = 30,
	x10         = 31,
	daylength   = 32,
	last        = 33,
}

// Not in the C++: the unit of each element of a day's climate data, for
// outputs that read it by path (climate_data.#last.tavg). The enum's
// counterpart of a field's `unit` tag (support/reflectpath); an enumerated
// array, so a new element doesn't compile without one. From the climate
// service's schema (mas_capnproto_schemas climate.capnp, Element), per day,
// except where MONICA reads an element differently (o3, vaporpress).
@(rodata)
ACD_UNITS := [ACD]string {
	.day         = "",
	.month       = "",
	.year        = "",
	.tmin        = "°C",
	.tavg        = "°C",
	.tmax        = "°C",
	.precip      = "mm",
	.precipOrig  = "mm",
	.globrad     = "MJ m-2",
	.wind        = "m s-1",
	.sunhours    = "h",
	.cloudamount = "%",
	.relhumid    = "%",
	.airpress    = "hPa",
	.vaporpress  = "kPa", // as MONICA's evapotranspiration reads it
	.isoDate     = "",
	.deDate      = "",
	.co2         = "ppm",
	.o3          = "ppb", // atmospheric_o3_concentration's
	.et0         = "mm",
	.skip        = "",
	.patternDate = "",
	.x1          = "",
	.x2          = "",
	.x3          = "",
	.x4          = "",
	.x5          = "",
	.x6          = "",
	.x7          = "",
	.x8          = "",
	.x9          = "",
	.x10         = "",
	.daylength   = "h",
	.last        = "",
}

// C++: unsigned int Climate::availableClimateDataSize()
available_climate_data_size :: proc() -> int {
	return int(ACD.last) + 1
}

// C++: std::map<std::string, ACD> Climate::name2acd()
name2acd :: proc(allocator := context.allocator) -> map[string]ACD {
	m := make(map[string]ACD, allocator)
	m["day"] = .day
	m["month"] = .month
	m["year"] = .year
	m["tmin"] = .tmin
	m["tavg"] = .tavg
	m["tmax"] = .tmax
	m["precip"] = .precip
	m["precip-orig"] = .precipOrig
	m["globrad"] = .globrad
	m["wind"] = .wind
	m["windspeed"] = .wind
	m["sunhours"] = .sunhours
	m["cloudamount"] = .cloudamount
	m["relhumid"] = .relhumid
	m["airpress"] = .airpress
	m["vaporpress"] = .vaporpress
	m["co2"] = .co2
	m["o3"] = .o3
	m["iso-date"] = .isoDate
	m["de-date"] = .deDate
	m["skip"] = .skip
	m["et0"] = .et0
	m["pattern-date"] = .patternDate
	m["x1"] = .x1
	m["x2"] = .x2
	m["x3"] = .x3
	m["x4"] = .x4
	m["x5"] = .x5
	m["x6"] = .x6
	m["x7"] = .x7
	m["x8"] = .x8
	m["x9"] = .x9
	m["x10"] = .x10
	m["daylength"] = .daylength
	return m
}

// ---------------------------------------------------------------------------
// DataAccessor
// ---------------------------------------------------------------------------

// C++: class Climate::DataAccessor
//
// NOTE(simplification): the C++ shares `_data` via `shared_ptr<VVD>`, so
// copies of a DataAccessor (cloneForRange, the copy ctor/assignment, the
// non-splice branches of mergeClimateData) alias the same underlying storage
// and a mutation through one is visible through the others. This port uses a
// plain `[dynamic][dynamic]f64` value field instead - each copy gets its own
// backing storage. Nothing in phases 1-2 exercises that aliasing (the
// Hohenfinow2 fixture reads a single climate.csv, never clones or merges a
// second DataAccessor into it); revisit with the phase 4-6 trace-diff oracle
// if a later fixture needs genuinely shared mutation semantics.
Data_Accessor :: struct {
	start_date:        d.Date,
	end_date:          d.Date,
	data:              [dynamic][dynamic]f64,
	// offsets to actual available climate data enum numbers; -1 = absent
	acd_to_data_index: [dynamic]i16,
	from_step:         int,
	number_of_steps:   int,
	tamp:              f64,
	tav:               f64,
}

// C++: DataAccessor::DataAccessor()
make_data_accessor :: proc(allocator := context.allocator) -> Data_Accessor {
	da: Data_Accessor
	da.data = make([dynamic][dynamic]f64, 0, allocator)
	da.acd_to_data_index = make([dynamic]i16, available_climate_data_size(), allocator)
	for i in 0 ..< len(da.acd_to_data_index) {
		da.acd_to_data_index[i] = -1
	}
	da.tamp = -9999
	da.tav = -9999
	return da
}

// C++: DataAccessor::DataAccessor(Tools::Date startDate, Tools::Date endDate)
make_data_accessor_range :: proc(
	start_date, end_date: d.Date,
	allocator := context.allocator,
) -> Data_Accessor {
	da := make_data_accessor(allocator)
	da.start_date = start_date
	da.end_date = end_date
	return da
}

// C++: bool DataAccessor::isValid() const
data_accessor_is_valid :: proc(da: ^Data_Accessor) -> bool {
	return data_accessor_no_of_steps_possible(da) > 0
}

// C++: size_t DataAccessor::noOfStepsPossible() const
data_accessor_no_of_steps_possible :: proc(da: ^Data_Accessor) -> int {
	return da.number_of_steps
}

data_accessor_start_date :: proc(da: ^Data_Accessor) -> d.Date {
	return da.start_date
}

data_accessor_end_date :: proc(da: ^Data_Accessor) -> d.Date {
	return da.end_date
}

// C++: Tools::Date DataAccessor::dateForStep(size_t) const
data_accessor_date_for_step :: proc(da: ^Data_Accessor, step_no: int) -> d.Date {
	return d.add(da.start_date, u64(step_no))
}

// C++: unsigned int DataAccessor::julianDayForStep(size_t) const
data_accessor_julian_day_for_step :: proc(da: ^Data_Accessor, step_no: int) -> int {
	return int(d.julian_day(data_accessor_date_for_step(da, step_no)))
}

// C++: bool DataAccessor::hasAvailableClimateData(ACD) const
data_accessor_has_available_climate_data :: proc(da: ^Data_Accessor, acd: ACD) -> bool {
	return da.acd_to_data_index[int(acd)] >= 0
}

// C++: double DataAccessor::dataForTimestep(ACD, size_t, double def) const
data_accessor_data_for_timestep :: proc(
	da: ^Data_Accessor,
	acd: ACD,
	step_no: int,
	def: f64 = 0.0,
) -> f64 {
	cache_index := da.acd_to_data_index[int(acd)]
	return cache_index < 0 ? def : da.data[cache_index][da.from_step + step_no]
}

// C++: Tools::Maybe<double> DataAccessor::dataForTimestepM(ACD, size_t) const
data_accessor_data_for_timestep_m :: proc(
	da: ^Data_Accessor,
	acd: ACD,
	step_no: int,
) -> Maybe(f64) {
	cache_index := da.acd_to_data_index[int(acd)]
	if cache_index < 0 {
		return nil
	}
	return da.data[cache_index][da.from_step + step_no]
}

// C++: std::map<ACD, double> DataAccessor::allDataForStep(size_t, double) const
data_accessor_all_data_for_step :: proc(
	da: ^Data_Accessor,
	step_no: int,
	latitude: f64,
	allocator := context.allocator,
) -> map[ACD]f64 {
	m := make(map[ACD]f64, allocator)
	for k in 0 ..< len(da.acd_to_data_index) {
		acd := ACD(k)
		mv := data_accessor_data_for_timestep_m(da, acd, step_no)
		if v, ok := mv.?; ok {
			m[acd] = v
		} else if acd == .globrad {
			mv2 := data_accessor_data_for_timestep_m(da, .sunhours, step_no)
			if v2, ok2 := mv2.?; ok2 {
				m[acd] = tl.sunshine2global_radiation(
					data_accessor_julian_day_for_step(da, step_no),
					v2,
					latitude,
				)
			}
		}
	}
	return m
}

// C++: std::vector<double> DataAccessor::dataAsVector(ACD) const
data_accessor_data_as_vector :: proc(
	da: ^Data_Accessor,
	acd: ACD,
	allocator := context.allocator,
) -> [dynamic]f64 {
	cache_index := da.acd_to_data_index[int(acd)]
	if cache_index < 0 {
		return make([dynamic]f64, 0, allocator)
	}
	src := da.data[cache_index]
	out := make([dynamic]f64, 0, da.number_of_steps, allocator)
	for i in da.from_step ..< da.from_step + da.number_of_steps {
		append(&out, src[i])
	}
	return out
}

// C++: DataAccessor DataAccessor::cloneForRange(size_t, size_t) const
//
// NOTE(c++-quirk): the date shift uses the already-incremented `clone._fromStep`
// (original `_fromStep` + `fromStep`), not just the `fromStep` delta passed in.
// Harmless when called on a freshly-read DataAccessor (`_fromStep` starts at 0),
// which is the only way this is reached anywhere in src/. Reproduced as-is.
data_accessor_clone_for_range :: proc(
	da: ^Data_Accessor,
	from_step: int,
	number_of_steps: int,
) -> Data_Accessor {
	if !data_accessor_is_valid(da) ||
	   from_step > data_accessor_no_of_steps_possible(da) ||
	   (from_step + number_of_steps) > data_accessor_no_of_steps_possible(da) {
		return Data_Accessor{}
	}
	clone := da^
	clone.from_step += from_step
	clone.number_of_steps = number_of_steps
	clone.start_date = d.add(clone.start_date, u64(clone.from_step))
	clone.end_date = d.add(clone.start_date, u64(number_of_steps) - 1)
	return clone
}

// C++: void DataAccessor::addClimateData(ACD, const vector<double>&)
//
// The C++ has `assert(_numberOfSteps = data.size())` here - a single-`=`
// assignment inside assert(), a no-op in release builds - but `_numberOfSteps`
// is unconditionally recomputed below regardless, so the assert has no
// observable effect either way and is not ported.
data_accessor_add_climate_data :: proc(
	da: ^Data_Accessor,
	acd: ACD,
	data: []f64,
	allocator := context.allocator,
) {
	row := make([dynamic]f64, 0, len(data), allocator)
	append(&row, ..data)
	append(&da.data, row)
	da.acd_to_data_index[int(acd)] = i16(len(da.data) - 1)
	da.number_of_steps = len(da.data) == 0 ? 0 : len(da.data[0])
}

// C++: void DataAccessor::mergeClimateData(DataAccessor, bool)
// (mas_cpp_misc 96be5bc: merged by climate element)
//
// Where the ranges overlap, other's climate elements replace this' (if
// replace_overlapping_data), also if other has just some of them; elements only
// in this stay. An element only in other is added if other covers the whole
// resulting range. The range only grows (other starting earlier or ending
// later) if other has all of this' elements, so that every element still
// covers the whole range; else only the part of other within this' range is
// used. A gap between the two ranges changes nothing.
//
// The C++ builds the merged data anew because copies of a DataAccessor share
// it; this port doesn't share storage (see Data_Accessor), but builds it anew
// all the same - it is the simplest form of the algorithm.
data_accessor_merge_climate_data :: proc(
	da: ^Data_Accessor,
	other_in: Data_Accessor,
	replace_overlapping_data := true,
	allocator := context.allocator,
) {
	other := other_in
	if !data_accessor_is_valid(&other) {
		return
	}

	if !data_accessor_is_valid(da) {
		da^ = other
		return
	}

	range :: proc(start, end: d.Date) -> string {
		return fmt.tprintf(
			"(%s - %s)",
			d.to_iso_date_string(start, "", context.temp_allocator),
			d.to_iso_date_string(end, "", context.temp_allocator),
		)
	}

	if d.gt(da.start_date, d.add(other.end_date, 1)) || d.gt(other.start_date, d.add(da.end_date, 1)) {
		fmt.printfln(
			"Can't prepend or append climate data because together data don't give a continuous timeseries. current: %s new: %s. New climate data won't be appended or prepended.",
			range(da.start_date, da.end_date),
			range(other.start_date, other.end_date),
		)
		return
	}

	// Every climate element of the result has to cover its whole time range.
	// So the range can only grow beyond this' range if other has all of this'
	// climate elements (to fill the added days); else only the part of other
	// overlapping this' range is used.
	missing_in_other := strings.builder_make(context.temp_allocator)
	for i in 0 ..< len(da.acd_to_data_index) {
		if da.acd_to_data_index[i] >= 0 && other.acd_to_data_index[i] < 0 {
			if strings.builder_len(missing_in_other) > 0 {
				strings.write_string(&missing_in_other, ", ")
			}
			strings.write_string(&missing_in_other, acd_csv_name(ACD(i)))
		}
	}
	new_start, new_end := da.start_date, da.end_date
	if d.lt(other.start_date, da.start_date) || d.gt(other.end_date, da.end_date) {
		if strings.builder_len(missing_in_other) == 0 {
			if d.lt(other.start_date, new_start) {
				new_start = other.start_date
			}
			if d.gt(other.end_date, new_end) {
				new_end = other.end_date
			}
		} else {
			fmt.printfln(
				"Can't extend climate data %s with %s, because the new data lack %s. Only the new data within %s will be used.",
				range(da.start_date, da.end_date),
				range(other.start_date, other.end_date),
				strings.to_string(missing_in_other),
				range(da.start_date, da.end_date),
			)
		}
	}

	n := d.diff(new_end, new_start) + 1
	// the days [o_from, o_to] of the result other has data for
	o_from := d.gt(other.start_date, new_start) ? other.start_date : new_start
	o_to := d.lt(other.end_date, new_end) ? other.end_date : new_end
	o_from_pos, o_to_pos := d.diff(o_from, new_start), d.diff(o_to, new_start)
	this_from_pos, this_to_pos := d.diff(da.start_date, new_start), d.diff(da.end_date, new_start)
	other_covers_result := d.le(other.start_date, new_start) && d.ge(other.end_date, new_end)

	data := make([dynamic][dynamic]f64, 0, len(da.acd_to_data_index), allocator)
	acd_to_data_index := make([dynamic]i16, len(da.acd_to_data_index), allocator)
	for i in 0 ..< len(da.acd_to_data_index) {
		acd_to_data_index[i] = -1
		ti := da.acd_to_data_index[i]
		oi := other.acd_to_data_index[i]
		if ti < 0 && oi < 0 {
			continue
		}

		if ti < 0 && !other_covers_result {
			fmt.printfln(
				"Can't add climate element %s from %s, because it doesn't cover the whole time range %s.",
				acd_csv_name(ACD(i)),
				range(other.start_date, other.end_date),
				range(new_start, new_end),
			)
			continue
		}

		v := make([dynamic]f64, n, allocator)
		if ti >= 0 {
			td := da.data[ti][da.from_step:]
			copy(v[this_from_pos:this_to_pos + 1], td[:this_to_pos - this_from_pos + 1])
		}
		if oi >= 0 {
			od := other.data[oi][other.from_step + d.diff(o_from, other.start_date):]
			for pos in o_from_pos ..= o_to_pos {
				in_this_range := ti >= 0 && pos >= this_from_pos && pos <= this_to_pos
				if !in_this_range || replace_overlapping_data {
					v[pos] = od[pos - o_from_pos]
				}
			}
		}
		append(&data, v)
		acd_to_data_index[i] = i16(len(data) - 1)
	}

	da.data = data
	da.acd_to_data_index = acd_to_data_index
	da.from_step = 0
	da.number_of_steps = n
	da.start_date = new_start
	da.end_date = new_end
}

// The name a CSV header uses for a climate element (tmin, precip, co2, ...),
// for messages: the reverse of name2acd. C++: in mergeClimateData, the first
// name in name2acd's (std::map, so alphabetical) order - wind, not windspeed.
acd_csv_name :: proc(acd: ACD) -> string {
	best := ""
	for name, a in name2acd(context.temp_allocator) {
		if a == acd && (best == "" || name < best) {
			best = name
		}
	}
	return best != "" ? best : fmt.tprint(acd)
}

// C++: void DataAccessor::addOrReplaceClimateData(ACD, const vector<double>&)
data_accessor_add_or_replace_climate_data :: proc(
	da: ^Data_Accessor,
	acd: ACD,
	data: []f64,
	allocator := context.allocator,
) {
	index := da.acd_to_data_index[int(acd)]
	if index < 0 {
		data_accessor_add_climate_data(da, acd, data, allocator)
	} else {
		row := make([dynamic]f64, 0, len(data), allocator)
		append(&row, ..data)
		da.data[index] = row
	}
}

// C++: pair<double,double> DataAccessor::getTAMPandTAV()
data_accessor_get_tamp_and_tav :: proc(
	da: ^Data_Accessor,
	allocator := context.allocator,
) -> (
	f64,
	f64,
) {
	if int(da.tamp) == -9999 || int(da.tav) == -9999 {
		tamp, tav := data_accessor_calc_tamp_and_tav(da, allocator)
		if int(da.tamp) == -9999 {
			da.tamp = tamp
		}
		if int(da.tav) == -9999 {
			da.tav = tav
		}
	}
	return da.tamp, da.tav
}

// C++: void DataAccessor::setTAMPandTAV(double, double)
data_accessor_set_tamp_and_tav :: proc(da: ^Data_Accessor, tamp, tav: f64) {
	da.tamp = tamp
	da.tav = tav
}

// C++: pair<double,double> DataAccessor::calcTAMPandTAV() const
//
// NOTE(c++-quirk): the accumulated sum/count for the LAST month present in the
// date range is only flushed into month2avgs on a month change mid-loop, never
// after the loop ends - so the final month's partial average never lands in
// month2avgs (unless a later, different year's occurrence of that same month
// number flushes it, which averages across an unrelated stretch of days
// alongside it). Reproduced exactly; this is dead code today (see the package
// comment) but ported faithfully per CONVENTIONS.
data_accessor_calc_tamp_and_tav :: proc(
	da: ^Data_Accessor,
	allocator := context.allocator,
) -> (
	f64,
	f64,
) {
	month2avgs: [13][dynamic]f64 // index 0 unused, months are 1-12
	for i in 1 ..= 12 {
		month2avgs[i] = make([dynamic]f64, 0, allocator)
	}
	sum: f64 = 0
	count := 0
	prev_month := 0
	for i in 0 ..< data_accessor_no_of_steps_possible(da) {
		dt := data_accessor_date_for_step(da, i)
		current_month := int(d.month(dt))
		if prev_month == 0 {
			prev_month = current_month
		}
		if current_month != prev_month {
			if count > 0 {
				append(&month2avgs[prev_month], sum / f64(count))
			} else {
				append(&month2avgs[prev_month], 0)
			}
			sum = 0
			count = 0
			prev_month = current_month
		}
		sum += data_accessor_data_for_timestep(da, .tavg, i)
		count += 1
	}
	monthly_avgs := make([dynamic]f64, 0, 12, allocator)
	for i in 1 ..= 12 {
		append(&monthly_avgs, tl.average(month2avgs[i][:]))
	}
	mn, mx := tl.min_max(monthly_avgs[:])
	return mx - mn, tl.average(monthly_avgs[:])
}

// C++: Errors DataAccessor::merge(json11::Json j)
//
// The C++ calls Json11Serializable::merge(j) first, which unwraps a "DEFAULT"
// or "=" wrapper key by recursing back into (virtually-dispatched) merge();
// inlined here the same way environment_parameters_merge does, since it needs
// the extra allocator parameter. The C++ also try/catches the two
// set_iso_date_value calls (Date::fromIsoDateString can throw on garbage);
// Odin has no exceptions, so d.from_iso_date_string is called directly - it
// never throws.
data_accessor_merge :: proc(
	da: ^Data_Accessor,
	j: jx.Value,
	allocator := context.allocator,
) -> tl.Errors {
	res: tl.Errors
	if jx.is_object(jx.get(j, "DEFAULT")) {
		res = data_accessor_merge(da, jx.get(j, "DEFAULT"), allocator)
	}
	if jx.is_object(jx.get(j, "=")) {
		res = data_accessor_merge(da, jx.get(j, "="), allocator)
	}

	for k, v in jx.object_items(jx.get(j, "data")) {
		if acd_no, ok := strconv.parse_int(k); ok {
			vec := jx.double_vector(v)
			data_accessor_add_or_replace_climate_data(da, ACD(acd_no), vec[:], allocator)
		}
	}

	jx.set_iso_date_value(&da.start_date, j, "startDate")
	jx.set_iso_date_value(&da.end_date, j, "endDate")

	jx.set_double_value(&da.tamp, j, "tamp")
	jx.set_double_value(&da.tav, j, "tav")

	return res
}

// C++: json11::Json DataAccessor::to_json() const
data_accessor_to_json :: proc(da: ^Data_Accessor, a: Allocator) -> jx.Value {
	data := make(jx.Object, 0, a)
	for idx, acd_no in da.acd_to_data_index {
		if idx >= 0 {
			buf: [12]byte
			key := strconv.write_int(buf[:], i64(acd_no), 10)
			vec := data_accessor_data_as_vector(da, ACD(acd_no), a)
			data[strings.clone(key, a)] = prim_arr_f64(vec[:], a)
		}
	}

	start_iso := d.to_iso_date_string(da.start_date, "", a)
	end_iso := d.to_iso_date_string(da.end_date, "", a)

	return jx.obj(
		a,
		{"type", jx.sl("DataAccessor")},
		{"data", jx.Value(data)},
		{"startDate", jx.s(start_iso, a)},
		{"endDate", jx.s(end_iso, a)},
		{"tamp", jx.f(da.tamp)},
		{"tav", jx.f(da.tav)},
	)
}

// ---------------------------------------------------------------------------
// local helpers
// ---------------------------------------------------------------------------

// C++: template<class Collection> J11Array Tools::toPrimJsonArray(const Collection&)
@(private)
prim_arr_f64 :: proc(vals: []f64, a: Allocator) -> jx.Value {
	out := make(jx.Array, 0, len(vals), a)
	for v in vals {
		append(&out, jx.f(v))
	}
	return jx.Value(out)
}
