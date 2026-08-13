// Port of mas_cpp_misc/tools/date.{h,cpp} - Tools::Date.
//
// A custom calendar supporting *relative* dates (year is a small delta, e.g.
// "0000-09-23" in crop-min.json) and a per-instance leap-year toggle. This is
// NOT core:time and must not be replaced by it: MONICA's semantics, including
// several quirks marked NOTE(c++-quirk) below, are load-bearing.
package date

import "core:math"
import "core:strconv"
import "core:strings"

// C++: DEFAULT_USE_LEAP_YEARS (DSS_NO_LEAP_YEAR_BY_DEFAULT is not defined in
// this build, so leap years are on by default).
DEFAULT_USE_LEAP_YEARS :: true

// C++: static const uint16_t Date::_aLeapYear = 2008
A_LEAP_YEAR :: 2008

// days in month, 1-indexed; index 0 is a 0 pad
@(private)
DIM := [13]u8{0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}
// days in month in a leap year
@(private)
LDIM := [13]u8{0, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}

// C++: class Tools::Date
//
// IMPORTANT - inverted flag storage. The C++ default constructor `Date()` yields
// useLeapYears = true and selects the *leap* days-in-month table (because year 0
// passes the leap-year test). Odin structs have no default field values, so both
// flags are stored inverted to make the zero value match C++ `Date()` exactly.
// A `Date` field inside any other struct therefore behaves correctly without an
// explicit initialiser. Always go through the procedures below rather than
// reading these two fields.
Date :: struct {
	d:                u8,
	m:                u8,
	y:                u16,
	_no_leap_years:   bool, // == !useLeapYears
	_common_year_dim: bool, // == the selected days-in-month table is the NON-leap one
	_is_relative:     bool,
}

// C++: bool useLeapYears() const
use_leap_years :: proc(d: Date) -> bool {
	return !d._no_leap_years
}

// C++: bool isValid() const { return _d > 0; }
is_valid :: proc(d: Date) -> bool {
	return d.d > 0
}

// C++: bool isRelativeDate() const
is_relative_date :: proc(d: Date) -> bool {
	return d._is_relative
}

// C++: bool isAbsoluteDate() const
is_absolute_date :: proc(d: Date) -> bool {
	return !d._is_relative
}

// C++: uint8_t day() / month() / int year()
day :: proc(d: Date) -> u8 {return d.d}
month :: proc(d: Date) -> u8 {return d.m}
year :: proc(d: Date) -> int {return int(d.y)}

// C++: bool Date::isLeapYear() const
//
// Note year 0 IS a leap year under this test, which matters for relative dates.
is_leap_year :: proc(d: Date) -> bool {
	return d.y % 4 == 0 && (d.y % 100 != 0 || d.y % 400 == 0)
}

@(private)
year_is_leap :: proc(y: u16) -> bool {
	return y % 4 == 0 && (y % 100 != 0 || y % 400 == 0)
}

// C++: uint8_t daysInMonth(uint8_t month = 0) const
//
// NOTE(c++-quirk): the C++ does `_daysInMonth->at(month)` on a 13-element vector,
// which THROWS for month > 12. Returning 0 here instead; the C++ would have
// terminated, so no correct program can depend on the difference.
days_in_month :: proc(d: Date, month_: u8 = 0) -> u8 {
	m := month_ == 0 ? d.m : month_
	if m > 12 {
		return 0
	}
	return d._common_year_dim ? DIM[m] : LDIM[m]
}

// C++: static uint8_t daysInMonth(uint16_t year, uint8_t month, bool useLeapYears)
days_in_month_of :: proc(y: u16, m: u8, use_leap := DEFAULT_USE_LEAP_YEARS) -> u8 {
	return days_in_month(make_date(1, 1, y, false, false, use_leap), m)
}

// C++: Date::Date(uint8_t day, uint8_t month, uint16_t year, bool isRelativeDate,
//                 bool createValidDate, bool useLeapYears)
make_date :: proc(
	day_: u8,
	month_: u8,
	year_: u16,
	is_relative := false,
	create_valid_date := false,
	use_leap := DEFAULT_USE_LEAP_YEARS,
) -> Date {
	d := Date {
		d              = day_,
		m              = month_,
		y              = year_,
		_no_leap_years = !use_leap,
		_is_relative   = is_relative,
	}
	// C++: _daysInMonth = isLeapYear() && _useLeapYears ? _ldim() : _dim();
	d._common_year_dim = !(year_is_leap(year_) && use_leap)

	dim := days_in_month(d, month_)
	if create_valid_date {
		if day_ == 0 {
			d.d = 1
		} else if day_ > dim {
			d.d = dim
		}
		if month_ == 0 {
			d.m = 1
		} else if month_ > 12 {
			d.m = 12
		}
	} else if month_ > 12 || month_ == 0 || day_ > dim || day_ == 0 {
		d.d = 0
		d.m = 0
		d.y = 0
	}
	return d
}

// C++: Date::Date(bool useLeapYears) - the default constructor
make_default_date :: proc(use_leap := DEFAULT_USE_LEAP_YEARS) -> Date {
	d := Date {
		_no_leap_years = !use_leap,
	}
	// _y is 0, which passes the leap-year test
	d._common_year_dim = !(year_is_leap(0) && use_leap)
	return d
}

// C++: static Date Date::relativeDate(uint8_t day, uint8_t month,
//                                     uint16_t deltaYears, bool useLeapYears)
relative_date :: proc(
	day_: u8,
	month_: u8,
	delta_years: u16 = 0,
	use_leap := DEFAULT_USE_LEAP_YEARS,
) -> Date {
	return make_date(day_, month_, delta_years, true, false, use_leap)
}

// C++: static Date Date::fromIsoDateString(const std::string&, bool useLeapYears)
//
// A year < 100 makes the date RELATIVE.
from_iso_date_string :: proc(s: string, use_leap := DEFAULT_USE_LEAP_YEARS) -> Date {
	if len(s) == 10 {
		y, y_ok := strconv.parse_uint(s[0:4])
		m, m_ok := strconv.parse_uint(s[5:7])
		dd, d_ok := strconv.parse_uint(s[8:10])
		if !y_ok || !m_ok || !d_ok {
			// the C++ std::stoul would throw here
			return Date{}
		}
		if y < 100 {
			return relative_date(u8(dd), u8(m), u16(y), use_leap)
		}
		return make_date(u8(dd), u8(m), u16(y), false, false, use_leap)
	}
	return Date{}
}

// C++: Date::Date(const string& isoDateString, bool useLeapYears)
//
// NOTE(c++-quirk): this constructor calls fromIsoDateString WITHOUT forwarding
// useLeapYears (so parsing uses the default), and copies only d/m/y - it drops
// the _isRelativeDate flag the parse produced. Both reproduced.
make_date_from_iso_string :: proc(s: string, use_leap := DEFAULT_USE_LEAP_YEARS) -> Date {
	parsed := from_iso_date_string(s)
	d := Date {
		d              = parsed.d,
		m              = parsed.m,
		y              = parsed.y,
		_no_leap_years = !use_leap,
	}
	d._common_year_dim = !(is_leap_year(d) && use_leap)
	return d
}

// C++: static Date Date::fromPatternDateString(const string& dateString,
//                                              const string& pattern, bool useLeapYears)
from_pattern_date_string :: proc(
	date_string: string,
	pattern: string,
	use_leap := DEFAULT_USE_LEAP_YEARS,
	allocator := context.allocator,
) -> Date {
	year_b := strings.builder_make(allocator)
	month_b := strings.builder_make(allocator)
	day_b := strings.builder_make(allocator)
	defer strings.builder_destroy(&year_b)
	defer strings.builder_destroy(&month_b)
	defer strings.builder_destroy(&day_b)

	is_doy := false
	n := min(len(pattern), len(date_string))
	for i in 0 ..< n {
		switch pattern[i] {
		case 'Y', 'y':
			if is_doy {
				strings.write_byte(&day_b, date_string[i])
			} else {
				strings.write_byte(&year_b, date_string[i])
			}
		case 'M', 'm':
			strings.write_byte(&month_b, date_string[i])
		case 'D', 'd':
			strings.write_byte(&day_b, date_string[i])
		case 'O', 'o':
			strings.write_byte(&day_b, date_string[i])
			is_doy = true
		}
	}

	if is_doy {
		y, _ := strconv.parse_uint(strings.to_string(year_b))
		doy, _ := strconv.parse_uint(strings.to_string(day_b))
		return julian_date(u16(doy), u16(y), y < 100, use_leap)
	}
	y, _ := strconv.parse_uint(strings.to_string(year_b))
	m, _ := strconv.parse_uint(strings.to_string(month_b))
	dd, _ := strconv.parse_uint(strings.to_string(day_b))
	if y < 100 {
		return relative_date(u8(dd), u8(m), u16(y), use_leap)
	}
	return make_date(u8(dd), u8(m), u16(y), false, false, use_leap)
}

// C++: static inline Date Date::julianDate(uint16_t julianDay, uint16_t year,
//                                          bool isRelativeDate, bool useLeapYears)
julian_date :: proc(
	julian_day_: u16,
	year_: u16,
	is_relative := false,
	use_leap := DEFAULT_USE_LEAP_YEARS,
) -> Date {
	// C++: Date(1,1,year,...) + (julianDay - 1) where julianDay-1 is computed in
	// int and then widened to uint64_t - so julianDay == 0 wraps. Reproduced.
	return add(make_date(1, 1, year_, is_relative, false, use_leap), u64(i64(julian_day_) - 1))
}

// C++: bool Date::operator<(const Date& other) const
//
// Transliterated exactly. The structure looks odd but is correct for valid
// dates; do not "simplify" it.
lt :: proc(a, b: Date) -> bool {
	t := a.d < b.d
	if t {
		t = a.m <= b.m
		if t {
			return year(a) <= year(b)
		} else {
			return year(a) < year(b)
		}
	} else {
		t = a.m < b.m
		if t {
			return year(a) <= year(b)
		} else {
			return year(a) < year(b)
		}
	}
}

// C++: bool Date::operator==(const Date& other) const
//
// Note this DOES compare isRelativeDate, while operator< does not.
eq :: proc(a, b: Date) -> bool {
	return year(a) == year(b) && a.m == b.m && a.d == b.d && a._is_relative == b._is_relative
}

ne :: proc(a, b: Date) -> bool {return !eq(a, b)}
le :: proc(a, b: Date) -> bool {return lt(a, b) || eq(a, b)}
gt :: proc(a, b: Date) -> bool {return !le(a, b)}
ge :: proc(a, b: Date) -> bool {return !lt(a, b)}

// C++: void Date::setDay(uint8_t day, bool createValidDate)
set_day :: proc(d: ^Date, day_: u8, create_valid_date := false) {
	d.d = day_
	if create_valid_date {
		if d.d == 0 {
			d.d = 1
		} else if d.d > days_in_month(d^) {
			d.d = days_in_month(d^)
		}
	}
}

// C++: Date Date::withDay(uint8_t d, bool createValidDate)
with_day :: proc(d: Date, day_: u8, create_valid_date := false) -> Date {
	t := d
	set_day(&t, day_, create_valid_date)
	return t
}

// C++: void Date::setMonth(uint8_t month, bool createValidDate)
set_month :: proc(d: ^Date, month_: u8, create_valid_date := false) {
	d.m = month_
	if create_valid_date {
		if d.m == 0 {
			d.m = 1
		} else if d.m > 12 {
			d.m = 12
		}
	}
}

// C++: Date Date::withMonth(uint8_t m, bool createValidDate)
with_month :: proc(d: Date, month_: u8, create_valid_date := false) -> Date {
	t := d
	set_month(&t, month_, create_valid_date)
	return t
}

// C++: void Date::setYear(uint16_t year) { _y = year; }
//
// NOTE(c++-quirk): setYear does NOT reselect the days-in-month table, so after
// changing the year across a leap-year boundary daysInMonth() is stale.
// Reproduced - do not add the reselection.
set_year :: proc(d: ^Date, year_: u16) {
	d.y = year_
}

// C++: Date Date::withYear(uint16_t y)
with_year :: proc(d: Date, year_: u16) -> Date {
	t := d
	set_year(&t, year_)
	return t
}

// C++: void addYears(int years) { setYear(year() + years); }
add_years :: proc(d: ^Date, years: int) {
	set_year(d, u16(year(d^) + years))
}

// C++: Date Date::withAddedYears(int years) const
with_added_years :: proc(d: Date, years: int) -> Date {
	t := d
	set_year(&t, u16(year(d) + years))
	return t
}

// C++: void setUseLeapYears(bool useLeapYears)
//
// NOTE(c++-quirk): unlike the constructor, this ignores isLeapYear() and picks
// the leap table purely from the flag. Reproduced.
set_use_leap_years :: proc(d: ^Date, use_leap: bool) {
	d._no_leap_years = !use_leap
	d._common_year_dim = !use_leap
}

// C++: Date Date::operator+(uint64_t days) const
add :: proc(d: Date, days: u64) -> Date {
	cd := d
	ds := days
	is_rel := cd._is_relative

	for {
		// C++ computes this in int and truncates to uint8_t
		delta := u8(i32(days_in_month(cd, cd.m)) - i32(cd.d) + 1)
		if u64(delta) <= ds {
			ds -= u64(delta)
			if cd.m == 12 {
				cd = make_date(1, 1, cd.y + 1, is_rel, false, use_leap_years(d))
			} else {
				cd = make_date(1, cd.m + 1, cd.y, is_rel, false, use_leap_years(d))
			}
		} else {
			set_day(&cd, cd.d + u8(ds))
			break
		}
	}
	return cd
}

// C++: Date Date::operator-(uint64_t days) const
sub :: proc(d: Date, days: u64) -> Date {
	cd := d
	ds := days
	is_rel := cd._is_relative

	for {
		if u64(cd.d) <= ds {
			ds -= u64(cd.d)
			if cd.m == 1 {
				cd = make_date(31, 12, cd.y - 1, is_rel, false, use_leap_years(d))
			} else {
				cd = make_date(
					days_in_month(cd, cd.m - 1),
					cd.m - 1,
					cd.y,
					is_rel,
					false,
					use_leap_years(d),
				)
			}
		} else {
			set_day(&cd, cd.d - u8(ds))
			break
		}
	}
	return cd
}

// C++: int Date::numberOfDaysTo(const Date& toDate) const
//
// Number of days to the argument date, excluding it
// (01.01.2000 -> 01.01.2000 == 0).
number_of_days_to :: proc(self: Date, to_date: Date) -> int {
	from := self
	to := to_date
	reverse := gt(from, to)
	if reverse {
		from = to_date
		to = self
	}

	nods := 0
	if from.y == to.y && from.m == to.m {
		nods += int(to.d) - int(from.d)
	} else {
		for y := year(from); y <= year(to); y += 1 {
			start_month := 1
			end_month := 12
			if y == year(from) {
				start_month = int(from.m) + 1
				nods += int(days_in_month(from, from.m)) - int(from.d)
			}
			if y == year(to) {
				end_month = int(to.m) - 1
				nods += int(to.d)
			}
			// current year, needed to count months in leap years correctly
			cy := make_date(1, 1, u16(y), false, false, use_leap_years(self))
			for m := start_month; m >= 1 && m <= 12 && m <= end_month; m += 1 {
				nods += int(days_in_month(cy, u8(m)))
			}
		}
	}
	return nods * (reverse ? -1 : 1)
}

// C++: int Date::operator-(const Date& other) const { return other.numberOfDaysTo(*this); }
diff :: proc(a, b: Date) -> int {
	return number_of_days_to(b, a)
}

// C++: static uint16_t Date::dayInYear(uint16_t year, uint8_t day, uint8_t month, bool useLeapYears)
day_in_year_of :: proc(y: u16, d_: u8, m: u8, use_leap := DEFAULT_USE_LEAP_YEARS) -> u16 {
	a := make_date(d_, m, y, false, false, use_leap)
	b := make_date(1, 1, y, false, false, use_leap)
	return u16(diff(a, b) + 1)
}

// C++: uint16_t dayInYear(uint8_t day, uint8_t month) const
day_in_year :: proc(d: Date, day_: u8, month_: u8) -> u16 {
	return day_in_year_of(u16(year(d)), day_, month_, use_leap_years(d))
}

// C++: uint16_t julianDay() const
julian_day :: proc(d: Date) -> u16 {
	return day_in_year_of(u16(year(d)), d.d, d.m, use_leap_years(d))
}

// C++: uint16_t dayOfYear() const { return julianDay(); }
day_of_year :: proc(d: Date) -> u16 {
	return julian_day(d)
}

// C++: Date startOfYear() / endOfYear() / startOfMonth() / endOfMonth()
//
// NOTE(c++-quirk): all four call the 3-argument constructor, so they drop
// isRelativeDate and useLeapYears back to the defaults. Reproduced.
start_of_year :: proc(d: Date) -> Date {return make_date(1, 1, u16(year(d)))}
end_of_year :: proc(d: Date) -> Date {return make_date(31, 12, u16(year(d)))}
start_of_month :: proc(d: Date) -> Date {return make_date(1, d.m, u16(year(d)))}
end_of_month :: proc(d: Date) -> Date {
	return make_date(days_in_month(d, d.m), d.m, u16(year(d)))
}

// C++: Date Date::toAbsoluteDate(uint16_t absYear, bool ignoreDeltaYears) const
//
// NOTE(c++-quirk): the C++ is
//     return Date(day(), month(), <year>, false, useLeapYears());
// which passes only FIVE arguments to the six-parameter constructor - so
// useLeapYears() lands on `createValidDate` and the real useLeapYears falls back
// to the default. Almost certainly a bug, but it is behaviour the baseline
// depends on. Reproduced exactly.
to_absolute_date :: proc(d: Date, abs_year: u16, ignore_delta_years := false) -> Date {
	y := ignore_delta_years ? abs_year : abs_year + u16(year(d))
	return make_date(d.d, d.m, y, false, use_leap_years(d), DEFAULT_USE_LEAP_YEARS)
}

// C++: Date toRelativeDate(int deltaYears, bool useLeapYears)
to_relative_date :: proc(d: Date, delta_years: int = 0, use_leap := DEFAULT_USE_LEAP_YEARS) -> Date {
	return relative_date(d.d, d.m, u16(delta_years), use_leap)
}

// C++: std::string Date::toIsoDateString(const std::string& wrapInto) const
//
// Relative dates get their year zero-padded to 4 digits; absolute ones do not.
to_iso_date_string :: proc(
	d: Date,
	wrap_into := "",
	allocator := context.allocator,
) -> string {
	b := strings.builder_make(allocator)
	y := year(d)
	dd := int(d.d)
	m := int(d.m)

	strings.write_string(&b, wrap_into)
	if is_relative_date(d) {
		if y < 10 {
			strings.write_string(&b, "000")
		} else if y < 100 {
			strings.write_string(&b, "00")
		} else if y < 1000 {
			strings.write_string(&b, "0")
		}
	}
	strings.write_int(&b, y)
	strings.write_string(&b, "-")
	if m < 10 {
		strings.write_string(&b, "0")
	}
	strings.write_int(&b, m)
	strings.write_string(&b, "-")
	if dd < 10 {
		strings.write_string(&b, "0")
	}
	strings.write_int(&b, dd)
	strings.write_string(&b, wrap_into)

	out := strings.clone(strings.to_string(b), allocator)
	strings.builder_destroy(&b)
	return out
}

// C++: std::string toMysqlString(const std::string& wrapInto) const
to_mysql_string :: proc(d: Date, wrap_into := "'", allocator := context.allocator) -> string {
	return to_iso_date_string(d, wrap_into, allocator)
}

// C++: std::string Date::toString(const std::string& separator, bool skipYear) const
to_string :: proc(
	d: Date,
	separator := ".",
	skip_year := false,
	allocator := context.allocator,
) -> string {
	b := strings.builder_make(allocator)
	if d.d < 10 {
		strings.write_string(&b, "0")
	}
	strings.write_int(&b, int(d.d))
	strings.write_string(&b, separator)
	if d.m < 10 {
		strings.write_string(&b, "0")
	}
	strings.write_int(&b, int(d.m))
	if !skip_year {
		if is_relative_date(d) {
			delta_years := d.y
			strings.write_string(&b, separator)
			strings.write_string(&b, "year")
			if delta_years > 0 {
				strings.write_string(&b, "+")
			}
			if delta_years != 0 {
				strings.write_int(&b, int(delta_years))
			}
		} else {
			strings.write_string(&b, separator)
			strings.write_int(&b, year(d))
		}
	}
	out := strings.clone(strings.to_string(b), allocator)
	strings.builder_destroy(&b)
	return out
}

// C++: struct Tools::DayLengths
Day_Lengths :: struct {
	astronomic_day_length:   f64,
	effective_day_length:    f64,
	photoperiodic_daylength: f64,
}

// C++: DayLengths Tools::dayLengths(double latitude, double julianDay)
day_lengths :: proc(latitude: f64, julian_day_: f64) -> Day_Lengths {
	dls := Day_Lengths {
		astronomic_day_length   = -1,
		effective_day_length    = -1,
		photoperiodic_daylength = -1,
	}

	// Calculation of declination - old DEC
	declination: f64 = -23.4 * math.cos(2.0 * math.PI * ((julian_day_ + 10.0) / 365.0))

	// old SINLD
	decl_sin: f64 =
		math.sin(declination * math.PI / 180.0) * math.sin(latitude * math.PI / 180.0)
	// old COSLD
	decl_cos: f64 =
		math.cos(declination * math.PI / 180.0) * math.cos(latitude * math.PI / 180.0)

	// Calculation of the atmospheric day length -> old DL
	astro_day_length: f64 = decl_sin / decl_cos
	// The argument of asin must be in the range of -1 to 1
	astro_day_length = bound_f64(-1.0, astro_day_length, 1.0)
	dls.astronomic_day_length = 12.0 * (math.PI + 2.0 * math.asin(astro_day_length)) / math.PI

	// Calculation of the effective day length = old DLE
	sin8: f64 = math.sin(f64(8.0) * math.PI / 180.0)
	edl_helper := (-sin8 + decl_sin) / decl_cos
	if (edl_helper < -1.0) || (edl_helper > 1.0) {
		dls.effective_day_length = 0.01
	} else {
		dls.effective_day_length = 12.0 * (math.PI + 2.0 * math.asin(edl_helper)) / math.PI
	}

	// old DLP
	sin_neg6: f64 = math.sin(f64(-6.0) * math.PI / 180.0)
	photo_day_length := (-sin_neg6 + decl_sin) / decl_cos
	photo_day_length = bound_f64(-1.0, photo_day_length, 1.0)
	dls.photoperiodic_daylength = 12.0 * (math.PI + 2.0 * math.asin(photo_day_length)) / math.PI

	return dls
}

// local copy to keep this package free of a tools dependency (see CONVENTIONS §6)
@(private)
bound_f64 :: proc(lower, value, upper: f64) -> f64 {
	if value < lower {
		return lower
	}
	if value > upper {
		return upper
	}
	return value
}
