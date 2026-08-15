// Port of the subset of mas_cpp_misc/tools/algorithms.{h,cpp} that src/ actually
// uses. The other ~700 lines of that file are not ported.
//
// Numerics note: the C++ round_to_digits computes in `long double`. On MSVC (the
// reference build) long double == double, so f64 here is exact. See
// ../../CONVENTIONS.md §1.
package tools

import "core:math"
import "core:strings"

// C++: template<typename T> T Tools::bound(T lower, T value, T upper)
bound :: proc(lower, value, upper: $T) -> T {
	if value < lower {
		return lower
	}
	if value > upper {
		return upper
	}
	return value
}

// C++: bool isEven(int)
is_even :: proc(v: int) -> bool {
	return v % 2 == 0
}

// C++: template<typename T, is_floating_point> std::optional<T> round_to_digits(T value, int digits)
//
// Returns (value, false) where the C++ returns nullopt.
round_to_digits :: proc(value: f64, digits: int) -> (f64, bool) {
	if !is_finite(value) {
		return 0, false
	}
	v := value
	factor := math.pow(f64(10), f64(abs(digits)))
	if !is_finite(factor) || factor == 0 {
		return 0, false
	}

	rounded: f64 = 0
	if digits >= 0 {
		if abs(v) > max(f64) / factor {
			return 0, false
		}
		rounded = math.round(v * factor) / factor
	} else {
		rounded = math.round(v / factor) * factor
	}

	if !is_finite(rounded) {
		return 0, false
	}
	return rounded, true
}

// C++: inline double Tools::round(double value, int roundToDigits)
//
// On any failure the C++ returns the input unchanged.
round :: proc(value: f64, round_to_digits_ := 0) -> f64 {
	if v, ok := round_to_digits(value, round_to_digits_); ok {
		return v
	}
	return value
}

// C++: double Tools::floor(double value, int digits, bool trailingDigits)
floor :: proc(value: f64, digits := 0, trailing_digits := true) -> f64 {
	if trailing_digits {
		return math.floor(value * math.pow(f64(10), f64(digits))) / math.pow(f64(10), f64(digits))
	}
	return math.floor(value / math.pow(f64(10), f64(digits))) * math.pow(f64(10), f64(digits))
}

// C++: double Tools::ceil(double value, int digits, bool trailingDigits)
ceil :: proc(value: f64, digits := 0, trailing_digits := true) -> f64 {
	if trailing_digits {
		return math.ceil(value * math.pow(f64(10), f64(digits))) / math.pow(f64(10), f64(digits))
	}
	return math.ceil(value / math.pow(f64(10), f64(digits))) * math.pow(f64(10), f64(digits))
}

// C++: template<typename ReturnType> ReturnType shiftDecimalPointRight(double value, uint8_t digits)
//
// The int specialisation. C++ double->int conversion truncates toward zero, as
// does Odin's, so this matches.
shift_decimal_point_right_int :: proc(value: f64, digits: u8) -> int {
	return int(value * math.pow(f64(10), f64(digits)))
}

// C++: template<typename ReturnType> ReturnType shiftDecimalPointLeft(double value, uint8_t digits)
shift_decimal_point_left_int :: proc(value: f64, digits: u8) -> int {
	return int(value / math.pow(f64(10), f64(digits)))
}

// C++: int Tools::integerRound1stDigit(int value)
//
// C's div() truncates toward zero and the remainder carries the dividend's sign;
// Odin's / and % on signed ints do the same.
integer_round_1st_digit :: proc(value: int) -> int {
	quot := value / 10
	rem := value % 10
	switch rem {
	case 0, 1, 2, 3, 4, -1, -2, -3, -4:
		return quot * 10
	case 5, 6, 7, 8, 9:
		return quot * 10 + 10
	case -5, -6, -7, -8, -9:
		return quot * 10 - 10
	}
	return quot * 10
}

// C++: int Tools::roundShiftedInt(double value, int8_t roundToDigits)
//
// Round to int but keep the value shifted to round_to_digits.
round_shifted_int :: proc(value: f64, round_to_digits_: i8) -> int {
	shift_digits := int(round_to_digits_) + 1

	// round to full integers or digits after the decimal point
	if round_to_digits_ >= 0 {
		return integer_round_1st_digit(shift_decimal_point_right_int(value, u8(shift_digits))) / 10
	}
	// round to full 100s
	if round_to_digits_ < -1 {
		return integer_round_1st_digit(shift_decimal_point_left_int(value, u8(-shift_digits))) / 10
	}
	// round to full 10s
	return integer_round_1st_digit(int(value))
}

// C++: template<class Collection> pair<T,T> Tools::minMax(const Collection& vs)
min_max :: proc(vs: []$T) -> (T, T) {
	if len(vs) == 0 {
		return T{}, T{}
	}
	minv := vs[0]
	maxv := vs[0]
	for v in vs[1:] {
		if v < minv {
			minv = v
		}
		if v > maxv {
			maxv = v
		}
	}
	return minv, maxv
}

// C++: template<class Collection> double Tools::median(const Collection& orderedData, int roundToDigits)
//
// Expects already-ordered input, matching the C++.
median :: proc(ordered_data: []f64, round_to_digits_ := 1) -> f64 {
	size := len(ordered_data)
	if size == 0 {
		return 0
	}
	if is_even(size) {
		return round(
			(ordered_data[(size / 2) - 1] + ordered_data[(size / 2) + 1 - 1]) / 2.0,
			round_to_digits_,
		)
	}
	return ordered_data[int(f64(size) / 2.0) + 1 - 1]
}

// C++: template<class Collection> double Tools::average(const Collection& xs)
average :: proc(xs: []f64) -> f64 {
	if len(xs) == 0 {
		return 0.0
	}
	sum: f64 = 0
	for x in xs {
		sum += x
	}
	return sum / f64(len(xs))
}

// C++: std::string Tools::trim(const std::string& s, const std::string& whitespaces = " \t\f\v\n\r")
trim :: proc(s: string, whitespaces := " \t\f\v\n\r", allocator := context.allocator) -> string {
	return strings.clone(strings.trim(s, whitespaces), allocator)
}

// C++: double Tools::sunshine2globalRadiation(int julianDay, double sunHours, double lat, bool asMJpm2pd)
//
// Returns MJ/m2/d by default; pass as_mj_pm2_pd = false for J/cm2/d.
sunshine2global_radiation :: proc(
	julian_day: int,
	sun_hours: f64,
	lat: f64,
	as_mj_pm2_pd := true,
) -> f64 {
	pi := 4.0 * math.atan(1.0)
	dec := -23.4 * math.cos(2 * pi * f64(julian_day + 10) / 365)
	sinld := math.sin(dec * pi / 180) * math.sin(lat * pi / 180)
	cosld := math.cos(dec * pi / 180) * math.cos(lat * pi / 180)
	dl := 12 * (pi + 2 * math.asin(sinld / cosld)) / pi
	dle := 12 * (pi + 2 * math.asin((-math.sin(8 * pi / 180) + sinld) / cosld)) / pi
	rdn :=
		3600 *
		(sinld * dl + 24 / pi * cosld * math.sqrt(1.0 - (sinld / cosld) * (sinld / cosld)))
	drc := 1300 * rdn * math.exp(-0.14 / (rdn / (dl * 3600)))
	dro := 0.2 * drc
	dtga := sun_hours / dle * drc + (1 - sun_hours / dle) * dro
	t := dtga / 10000.0
	// convert J/cm2/d to MJ/m2/d: (t * 100.0 * 100.0) / 1000000.0 -> t / 100
	return as_mj_pm2_pd ? t / 100.0 : t
}

@(private)
is_finite :: proc(v: f64) -> bool {
	return !math.is_nan(v) && !math.is_inf(v)
}
