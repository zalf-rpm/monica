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
	_startDate:     d.Date,
	_endDate:       d.Date,
	_data:          [dynamic][dynamic]f64,
	// offsets to actual available climate data enum numbers; -1 = absent
	_acd2dataIndex: [dynamic]i16,
	_fromStep:      int,
	_numberOfSteps: int,
	_tamp:          f64,
	_tav:           f64,
}

// C++: DataAccessor::DataAccessor()
make_data_accessor :: proc(allocator := context.allocator) -> Data_Accessor {
	da: Data_Accessor
	da._data = make([dynamic][dynamic]f64, 0, allocator)
	da._acd2dataIndex = make([dynamic]i16, available_climate_data_size(), allocator)
	for i in 0 ..< len(da._acd2dataIndex) {
		da._acd2dataIndex[i] = -1
	}
	da._tamp = -9999
	da._tav = -9999
	return da
}

// C++: DataAccessor::DataAccessor(Tools::Date startDate, Tools::Date endDate)
make_data_accessor_range :: proc(
	start_date, end_date: d.Date,
	allocator := context.allocator,
) -> Data_Accessor {
	da := make_data_accessor(allocator)
	da._startDate = start_date
	da._endDate = end_date
	return da
}

// C++: bool DataAccessor::isValid() const
data_accessor_is_valid :: proc(da: ^Data_Accessor) -> bool {
	return data_accessor_no_of_steps_possible(da) > 0
}

// C++: size_t DataAccessor::noOfStepsPossible() const
data_accessor_no_of_steps_possible :: proc(da: ^Data_Accessor) -> int {
	return da._numberOfSteps
}

data_accessor_start_date :: proc(da: ^Data_Accessor) -> d.Date {
	return da._startDate
}

data_accessor_end_date :: proc(da: ^Data_Accessor) -> d.Date {
	return da._endDate
}

// C++: Tools::Date DataAccessor::dateForStep(size_t) const
data_accessor_date_for_step :: proc(da: ^Data_Accessor, step_no: int) -> d.Date {
	return d.add(da._startDate, u64(step_no))
}

// C++: unsigned int DataAccessor::julianDayForStep(size_t) const
data_accessor_julian_day_for_step :: proc(da: ^Data_Accessor, step_no: int) -> int {
	return int(d.julian_day(data_accessor_date_for_step(da, step_no)))
}

// C++: bool DataAccessor::hasAvailableClimateData(ACD) const
data_accessor_has_available_climate_data :: proc(da: ^Data_Accessor, acd: ACD) -> bool {
	return da._acd2dataIndex[int(acd)] >= 0
}

// C++: double DataAccessor::dataForTimestep(ACD, size_t, double def) const
data_accessor_data_for_timestep :: proc(
	da: ^Data_Accessor,
	acd: ACD,
	step_no: int,
	def: f64 = 0.0,
) -> f64 {
	cache_index := da._acd2dataIndex[int(acd)]
	return cache_index < 0 ? def : da._data[cache_index][da._fromStep + step_no]
}

// C++: Tools::Maybe<double> DataAccessor::dataForTimestepM(ACD, size_t) const
data_accessor_data_for_timestep_m :: proc(
	da: ^Data_Accessor,
	acd: ACD,
	step_no: int,
) -> Maybe(f64) {
	cache_index := da._acd2dataIndex[int(acd)]
	if cache_index < 0 {
		return nil
	}
	return da._data[cache_index][da._fromStep + step_no]
}

// C++: std::map<ACD, double> DataAccessor::allDataForStep(size_t, double) const
data_accessor_all_data_for_step :: proc(
	da: ^Data_Accessor,
	step_no: int,
	latitude: f64,
	allocator := context.allocator,
) -> map[ACD]f64 {
	m := make(map[ACD]f64, allocator)
	for k in 0 ..< len(da._acd2dataIndex) {
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
	cache_index := da._acd2dataIndex[int(acd)]
	if cache_index < 0 {
		return make([dynamic]f64, 0, allocator)
	}
	src := da._data[cache_index]
	out := make([dynamic]f64, 0, da._numberOfSteps, allocator)
	for i in da._fromStep ..< da._fromStep + da._numberOfSteps {
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
	clone._fromStep += from_step
	clone._numberOfSteps = number_of_steps
	clone._startDate = d.add(clone._startDate, u64(clone._fromStep))
	clone._endDate = d.add(clone._startDate, u64(number_of_steps) - 1)
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
	append(&da._data, row)
	da._acd2dataIndex[int(acd)] = i16(len(da._data) - 1)
	da._numberOfSteps = len(da._data) == 0 ? 0 : len(da._data[0])
}

// C++: void DataAccessor::mergeClimateData(DataAccessor, bool)
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

	if len(da._data) == 0 {
		da^ = other
		return
	}

	if d.gt(da._startDate, d.add(other._endDate, 1)) ||
	   d.lt(da._endDate, d.sub(other._startDate, 1)) {
		// C++ prints a warning to stdout and leaves the data unchanged; that
		// stdout side effect isn't part of any oracle's JSON output, so it is
		// not reproduced here.
		return
	} else if d.eq(da._startDate, d.add(other._endDate, 1)) {
		// insert all of other's data before this' data
		for i in 0 ..< len(da._acd2dataIndex) {
			index := da._acd2dataIndex[i]
			if index < 0 {
				continue
			}
			odai := other._data[index]
			merged := make([dynamic]f64, 0, len(odai) + len(da._data[index]), allocator)
			append(&merged, ..odai[:])
			append(&merged, ..da._data[index][:])
			da._data[index] = merged
		}
		da._startDate = other._startDate
		da._numberOfSteps = len(da._data) == 0 ? 0 : len(da._data[0])
	} else if d.eq(d.add(da._endDate, 1), other._startDate) {
		// insert all of other's data after this' data
		for i in 0 ..< len(da._acd2dataIndex) {
			index := da._acd2dataIndex[i]
			if index < 0 {
				continue
			}
			odai := other._data[index]
			append(&da._data[index], ..odai[:])
		}
		da._endDate = other._endDate
		da._numberOfSteps = len(da._data) == 0 ? 0 : len(da._data[0])
	} else if d.ge(da._startDate, other._startDate) && d.le(da._endDate, other._endDate) {
		// all of this' data will be overwritten
		da^ = other
		return
	} else {
		prepend_count := d.diff(da._startDate, other._startDate)
		append_count := d.diff(other._endDate, da._endDate)

		for i in 0 ..< len(da._acd2dataIndex) {
			index := da._acd2dataIndex[i]
			if index < 0 {
				continue
			}
			odai := other._data[index]

			if prepend_count > 0 {
				prefix := make([dynamic]f64, 0, prepend_count, allocator)
				append(&prefix, ..odai[:prepend_count])
				append(&prefix, ..da._data[index][:])
				da._data[index] = prefix
			}

			if replace_overlapping_data {
				dii := abs_int(prepend_count)
				odaii := prepend_count < 0 ? 0 : prepend_count
				for dii < len(da._data[index]) && odaii < len(odai) {
					da._data[index][dii] = odai[odaii]
					dii += 1
					odaii += 1
				}
			}

			if append_count > 0 {
				append(&da._data[index], ..odai[len(odai) - append_count:])
			}
		}
		if prepend_count > 0 {
			da._startDate = other._startDate
		}
		if append_count > 0 {
			da._endDate = other._endDate
		}
		da._numberOfSteps = len(da._data) == 0 ? 0 : len(da._data[0])
	}
}

// C++: void DataAccessor::addOrReplaceClimateData(ACD, const vector<double>&)
data_accessor_add_or_replace_climate_data :: proc(
	da: ^Data_Accessor,
	acd: ACD,
	data: []f64,
	allocator := context.allocator,
) {
	index := da._acd2dataIndex[int(acd)]
	if index < 0 {
		data_accessor_add_climate_data(da, acd, data, allocator)
	} else {
		row := make([dynamic]f64, 0, len(data), allocator)
		append(&row, ..data)
		da._data[index] = row
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
	if int(da._tamp) == -9999 || int(da._tav) == -9999 {
		tamp, tav := data_accessor_calc_tamp_and_tav(da, allocator)
		if int(da._tamp) == -9999 {
			da._tamp = tamp
		}
		if int(da._tav) == -9999 {
			da._tav = tav
		}
	}
	return da._tamp, da._tav
}

// C++: void DataAccessor::setTAMPandTAV(double, double)
data_accessor_set_tamp_and_tav :: proc(da: ^Data_Accessor, tamp, tav: f64) {
	da._tamp = tamp
	da._tav = tav
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

	jx.set_iso_date_value(&da._startDate, j, "startDate")
	jx.set_iso_date_value(&da._endDate, j, "endDate")

	jx.set_double_value(&da._tamp, j, "tamp")
	jx.set_double_value(&da._tav, j, "tav")

	return res
}

// C++: json11::Json DataAccessor::to_json() const
data_accessor_to_json :: proc(da: ^Data_Accessor, a: Allocator) -> jx.Value {
	data := make(jx.Object, 0, a)
	for idx, acd_no in da._acd2dataIndex {
		if idx >= 0 {
			buf: [12]byte
			key := strconv.write_int(buf[:], i64(acd_no), 10)
			vec := data_accessor_data_as_vector(da, ACD(acd_no), a)
			data[strings.clone(key, a)] = prim_arr_f64(vec[:], a)
		}
	}

	start_iso := d.to_iso_date_string(da._startDate, "", a)
	end_iso := d.to_iso_date_string(da._endDate, "", a)

	return jx.obj(
		a,
		{"type", jx.sl("DataAccessor")},
		{"data", jx.Value(data)},
		{"startDate", jx.s(start_iso, a)},
		{"endDate", jx.s(end_iso, a)},
		{"tamp", jx.f(da._tamp)},
		{"tav", jx.f(da._tav)},
	)
}

// ---------------------------------------------------------------------------
// local helpers
// ---------------------------------------------------------------------------

@(private)
abs_int :: proc(v: int) -> int {
	return v < 0 ? -v : v
}

// C++: template<class Collection> J11Array Tools::toPrimJsonArray(const Collection&)
@(private)
prim_arr_f64 :: proc(vals: []f64, a: Allocator) -> jx.Value {
	out := make(jx.Array, 0, len(vals), a)
	for v in vals {
		append(&out, jx.f(v))
	}
	return jx.Value(out)
}
