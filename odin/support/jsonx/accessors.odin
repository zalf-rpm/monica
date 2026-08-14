// Port of mas_cpp_misc/json11/json11-helper.{h,cpp} - the convenience accessors
// every MONICA merge() call site uses.
//
// The recurring shape in MONICA's parameter JSON is a [value, "unit"] pair, e.g.
//   "amount": [17, "mm"]
//   "trigger_if_nFC_below_%": [90, "%"]
// so every scalar accessor also accepts an array and takes element 0, and an
// object with a "value" key. Reproduced exactly, including the odd cases marked
// NOTE(c++-quirk).
package jsonx

import "core:strconv"
import "core:strings"
import d "../date"
import tl "../tools"

// ---------------------------------------------------------------------------
// unit transforms
// ---------------------------------------------------------------------------

// C++: std::function<double(double)> Tools::transformIfPercent(const Json& j, const string& key)
//
// Returned as an enum + apply pair rather than a closure (CONVENTIONS §4 of
// plan-odin.md: std::function is not ported).
Transform :: enum {
	IDENTITY,
	PERCENT, // /100
	MM, // /1000
	CM, // /100
	DM, // /10
}

apply_transform :: proc(t: Transform, v: f64) -> f64 {
	switch t {
	case .IDENTITY:
		return v
	case .PERCENT:
		return v / 100.0
	case .MM:
		return v / 1000.0
	case .CM:
		return v / 100.0
	case .DM:
		return v / 10.0
	}
	return v
}

// C++: transformIfPercent
transform_if_percent :: proc(j: Value, key: string) -> Transform {
	value := get(j, key)
	if is_array(value) &&
	   len(array_items(value)) > 1 &&
	   is_string(at(value, 1)) &&
	   strings.trim_space(string_value_of(at(value, 1))) == "%" {
		return .PERCENT
	}
	return .IDENTITY
}

// C++: transformIfNotMeters
transform_if_not_meters :: proc(j: Value, key: string) -> Transform {
	value := get(j, key)
	if is_array(value) && len(array_items(value)) > 1 && is_string(at(value, 1)) {
		unit := strings.trim_space(string_value_of(at(value, 1)))
		switch unit {
		case "mm":
			return .MM
		case "cm":
			return .CM
		case "dm":
			return .DM
		}
		return .IDENTITY
	}
	return .IDENTITY
}

// ---------------------------------------------------------------------------
// scalar accessors
// ---------------------------------------------------------------------------

// C++: double Tools::double_valueD(const Json& j, double def)
double_value_d :: proc(j: Value, def: f64) -> f64 {
	if is_number(j) {
		return number_value(j)
	} else if is_string(j) {
		// C++ uses std::stod, which throws on garbage; we fall back to def
		if v, ok := strconv.parse_f64(string_value_of(j)); ok {
			return v
		}
		return def
	} else if is_array(j) && len(array_items(j)) > 0 && is_number(at(j, 0)) {
		return number_value(at(j, 0))
	} else if is_object(j) {
		return double_value_key_d(j, "value", def)
	}
	return def
}

// C++: void Tools::set_double_valueD(double& var, const Json& j, const string& key,
//                                    double def, const function<double(double)>& transf,
//                                    bool useDefault)
set_double_value_d :: proc(
	var: ^f64,
	j: Value,
	key: string,
	def: f64 = 0.0,
	transf: Transform = .IDENTITY,
	use_default := true,
) {
	v := get(j, key)
	if is_null(v) && use_default {
		var^ = apply_transform(transf, def)
	} else if is_number(v) {
		var^ = apply_transform(transf, number_value(v))
	} else if is_string(v) {
		if pv, ok := strconv.parse_f64(string_value_of(v)); ok {
			var^ = apply_transform(transf, pv)
		}
	} else if is_array(v) && len(array_items(v)) > 0 && is_number(at(v, 0)) {
		var^ = apply_transform(transf, number_value(at(v, 0)))
	} else if is_object(v) {
		set_double_value_d(var, v, "value", def, transf)
	}
}

// C++: inline void Tools::set_double_value(double& var, const Json& j, const string& key, transf)
set_double_value :: proc(var: ^f64, j: Value, key: string, transf: Transform = .IDENTITY) {
	set_double_value_d(var, j, key, 0.0, transf, false)
}

// C++: double Tools::double_valueD(const Json& j, const string& key, double def)
double_value_key_d :: proc(j: Value, key: string, def: f64) -> f64 {
	res := def
	set_double_value_d(&res, j, key, def)
	return res
}

// C++: inline double Tools::double_value(const Json& j, const string& key)
double_value :: proc(j: Value, key: string) -> f64 {
	return double_value_key_d(j, key, 0.0)
}

// C++: int Tools::int_valueD(const Json& j, int def)
int_value_d :: proc(j: Value, def: int) -> int {
	if is_number(j) {
		return int_value_of(j)
	} else if is_string(j) {
		if v, ok := strconv.parse_int(string_value_of(j)); ok {
			return v
		}
		return def
	} else if is_array(j) && len(array_items(j)) > 0 && is_number(at(j, 0)) {
		return int_value_of(at(j, 0))
	} else if is_object(j) {
		return int_value_key_d(j, "value", def)
	}
	return def
}

// C++: void Tools::set_int_valueD(int& var, const Json& j, const string& key, int def, bool useDefault)
set_int_value_d :: proc(var: ^int, j: Value, key: string, def: int = 0, use_default := true) {
	v := get(j, key)
	if is_null(v) && use_default {
		var^ = def
	} else if is_number(v) {
		var^ = int_value_of(v)
	} else if is_string(v) {
		if pv, ok := strconv.parse_int(string_value_of(v)); ok {
			var^ = pv
		}
	} else if is_array(v) && len(array_items(v)) > 0 && is_number(at(v, 0)) {
		var^ = int_value_of(at(v, 0))
	} else if is_object(v) {
		var^ = int_value_key_d(v, "value", def)
	}
}

// C++: inline void Tools::set_int_value(int& var, const Json& j, const string& key)
set_int_value :: proc(var: ^int, j: Value, key: string) {
	set_int_value_d(var, j, key, 0, false)
}

// C++: int Tools::int_valueD(const Json& j, const string& key, int def)
int_value_key_d :: proc(j: Value, key: string, def: int) -> int {
	res := def
	set_int_value_d(&res, j, key, def)
	return res
}

// C++: inline int Tools::int_value(const Json& j, const string& key)
int_value :: proc(j: Value, key: string) -> int {
	return int_value_key_d(j, key, 0)
}

// C++: bool Tools::bool_valueD(const Json& j, bool def)
//
// NOTE(c++-quirk): a string that is not exactly "TRUE"/"FALSE" (case-insensitive)
// falls through to `def` rather than being coerced. Reproduced.
bool_value_d :: proc(j: Value, def: bool) -> bool {
	if is_bool(j) {
		return bool_value_of(j)
	} else if is_string(j) {
		bs := to_upper_ascii(string_value_of(j))
		if bs == "TRUE" || bs == "FALSE" {
			return bs == "TRUE"
		}
	} else if is_array(j) && len(array_items(j)) > 0 && is_bool(at(j, 0)) {
		return bool_value_of(at(j, 0))
	} else if is_object(j) {
		return bool_value_key_d(j, "value", def)
	}
	return def
}

// C++: void Tools::set_bool_valueD(bool& var, const Json& j, const string& key, bool def, bool useDefault)
//
// NOTE(c++-quirk): for a string that is neither "TRUE" nor "FALSE", `var` is left
// completely untouched (not set to def). Reproduced.
set_bool_value_d :: proc(var: ^bool, j: Value, key: string, def: bool = false, use_default := true) {
	v := get(j, key)
	if is_null(v) && use_default {
		var^ = def
	} else if is_bool(v) {
		var^ = bool_value_of(v)
	} else if is_string(v) {
		bs := to_upper_ascii(string_value_of(v))
		if bs == "TRUE" || bs == "FALSE" {
			var^ = bs == "TRUE"
		}
	} else if is_array(v) && len(array_items(v)) > 0 && is_bool(at(v, 0)) {
		var^ = bool_value_of(at(v, 0))
	} else if is_object(v) {
		var^ = bool_value_key_d(v, "value", def)
	}
}

// C++: inline void Tools::set_bool_value(bool& var, const Json& j, const string& key)
set_bool_value :: proc(var: ^bool, j: Value, key: string) {
	set_bool_value_d(var, j, key, false, false)
}

// C++: bool Tools::bool_valueD(const Json& j, const string& key, bool def)
bool_value_key_d :: proc(j: Value, key: string, def: bool) -> bool {
	res := def
	set_bool_value_d(&res, j, key, def)
	return res
}

// C++: inline bool Tools::bool_value(const Json& j, const string& key)
bool_value :: proc(j: Value, key: string) -> bool {
	return bool_value_key_d(j, key, false)
}

// C++: string Tools::string_valueD(const Json& j, const string& def)
string_value_d :: proc(j: Value, def: string) -> string {
	if is_string(j) {
		return string_value_of(j)
	} else if is_array(j) && len(array_items(j)) > 0 && is_string(at(j, 0)) {
		return string_value_of(at(j, 0))
	} else if is_object(j) {
		return string_value_key_d(j, "value", def)
	}
	return def
}

// C++: void Tools::set_string_valueD(string& var, const Json& j, const string& key,
//                                    const string& def, bool useDefault)
set_string_value_d :: proc(
	var: ^string,
	j: Value,
	key: string,
	def: string = "",
	use_default := true,
) {
	v := get(j, key)
	if is_null(v) && use_default {
		var^ = def
	} else if is_string(v) {
		var^ = string_value_of(v)
	} else if is_array(v) && len(array_items(v)) > 0 && is_string(at(v, 0)) {
		var^ = string_value_of(at(v, 0))
	} else if is_object(v) {
		var^ = string_value_key_d(v, "value", def)
	}
}

// C++: inline void Tools::set_string_value(string& var, const Json& j, const string& key)
set_string_value :: proc(var: ^string, j: Value, key: string) {
	set_string_value_d(var, j, key, "", false)
}

// C++: string Tools::string_valueD(const Json& j, const string& key, const string& def)
string_value_key_d :: proc(j: Value, key: string, def: string) -> string {
	res := def
	set_string_value_d(&res, j, key, def)
	return res
}

// C++: inline string Tools::string_value(const Json& j, const string& key)
string_value :: proc(j: Value, key: string) -> string {
	return string_value_key_d(j, key, "")
}

// ---------------------------------------------------------------------------
// vector accessors
// ---------------------------------------------------------------------------

// C++: vector<double> Tools::double_vectorD(const Json& j, const vector<double>& def, double defaultValue)
double_vector_d :: proc(
	j: Value,
	def: []f64,
	default_value: f64 = 0.0,
	allocator := context.allocator,
) -> [dynamic]f64 {
	if is_array(j) {
		items := array_items(j)
		if len(items) > 1 && is_string(at(j, 1)) && is_array(at(j, 0)) {
			return double_vector_d(at(j, 0), def, default_value, allocator)
		}
		out := make([dynamic]f64, 0, len(items), allocator)
		for v in items {
			append(&out, double_value_d(v, default_value))
		}
		return out
	} else if is_object(j) && is_array(get(j, "value")) {
		return double_vector_d(get(j, "value"), def, default_value, allocator)
	}
	out := make([dynamic]f64, 0, len(def), allocator)
	append(&out, ..def)
	return out
}

// C++: void Tools::set_double_vectorD(vector<double>& var, const Json& j, const string& key,
//                                     const vector<double>& def, double defaultValue, bool useDefault)
//
// NOTE(c++-quirk): the null/useDefault branch is a plain `if`, not an `else if`,
// unlike every other set_*_vectorD. It therefore cannot fall through to the
// array branch (a null is not an array), so behaviour is the same - but the
// asymmetry is preserved here for line-by-line comparability.
set_double_vector_d :: proc(
	var: ^[dynamic]f64,
	j: Value,
	key: string,
	def: []f64,
	default_value: f64 = 0.0,
	use_default := true,
	allocator := context.allocator,
) {
	v := get(j, key)
	if is_null(v) && use_default {
		clear(var)
		append(var, ..def)
	}
	if is_array(v) {
		if len(array_items(v)) > 1 && is_string(at(v, 1)) && is_array(at(v, 0)) {
			assign_dyn_f64(var, double_vector_d(at(v, 0), def, default_value, allocator))
		} else {
			assign_dyn_f64(var, double_vector_d(v, def, default_value, allocator))
		}
	} else if is_object(v) && is_array(get(v, "value")) {
		assign_dyn_f64(var, double_vector_d(get(v, "value"), def, default_value, allocator))
	}
}

// C++: inline void Tools::set_double_vector(vector<double>& var, const Json& j, const string& key)
set_double_vector :: proc(
	var: ^[dynamic]f64,
	j: Value,
	key: string,
	allocator := context.allocator,
) {
	set_double_vector_d(var, j, key, nil, 0.0, false, allocator)
}

// C++: vector<int> Tools::int_vectorD(const Json& j, const vector<int>& def, int defaultValue)
int_vector_d :: proc(
	j: Value,
	def: []int,
	default_value: int = 0,
	allocator := context.allocator,
) -> [dynamic]int {
	if is_array(j) {
		items := array_items(j)
		if len(items) > 1 && is_string(at(j, 1)) && is_array(at(j, 0)) {
			return int_vector_d(at(j, 0), def, default_value, allocator)
		}
		out := make([dynamic]int, 0, len(items), allocator)
		for v in items {
			append(&out, int_value_d(v, default_value))
		}
		return out
	} else if is_object(j) && is_array(get(j, "value")) {
		return int_vector_d(get(j, "value"), def, default_value, allocator)
	}
	out := make([dynamic]int, 0, len(def), allocator)
	append(&out, ..def)
	return out
}

// C++: vector<bool> Tools::bool_vectorD(const Json& j, const vector<bool>& def, bool defaultValue)
bool_vector_d :: proc(
	j: Value,
	def: []bool,
	default_value: bool = false,
	allocator := context.allocator,
) -> [dynamic]bool {
	if is_array(j) {
		items := array_items(j)
		if len(items) > 1 && is_string(at(j, 1)) && is_array(at(j, 0)) {
			return bool_vector_d(at(j, 0), def, default_value, allocator)
		}
		out := make([dynamic]bool, 0, len(items), allocator)
		for v in items {
			append(&out, bool_value_d(v, default_value))
		}
		return out
	} else if is_object(j) && is_array(get(j, "value")) {
		return bool_vector_d(get(j, "value"), def, default_value, allocator)
	}
	out := make([dynamic]bool, 0, len(def), allocator)
	append(&out, ..def)
	return out
}

// C++: vector<string> Tools::string_vectorD(const Json& j, const vector<string>& def, const string& defaultValue)
string_vector_d :: proc(
	j: Value,
	def: []string,
	default_value: string = "",
	allocator := context.allocator,
) -> [dynamic]string {
	if is_array(j) {
		items := array_items(j)
		if len(items) > 1 && is_string(at(j, 1)) && is_array(at(j, 0)) {
			return string_vector_d(at(j, 0), def, default_value, allocator)
		}
		out := make([dynamic]string, 0, len(items), allocator)
		for v in items {
			append(&out, string_value_d(v, default_value))
		}
		return out
	} else if is_object(j) && is_array(get(j, "value")) {
		return string_vector_d(get(j, "value"), def, default_value, allocator)
	}
	out := make([dynamic]string, 0, len(def), allocator)
	append(&out, ..def)
	return out
}

// C++: inline vector<double> Tools::toDoubleVector(const Json& arr)
to_double_vector :: proc(arr: Value, allocator := context.allocator) -> [dynamic]f64 {
	out := make([dynamic]f64, 0, len(array_items(arr)), allocator)
	for v in array_items(arr) {
		append(&out, number_value(v))
	}
	return out
}

// C++: inline vector<int> Tools::toIntVector(const Json& arr)
to_int_vector :: proc(arr: Value, allocator := context.allocator) -> [dynamic]int {
	out := make([dynamic]int, 0, len(array_items(arr)), allocator)
	for v in array_items(arr) {
		append(&out, int_value_of(v))
	}
	return out
}

// C++: inline vector<string> Tools::toStringVector(const Json& arr)
to_string_vector :: proc(arr: Value, allocator := context.allocator) -> [dynamic]string {
	out := make([dynamic]string, 0, len(array_items(arr)), allocator)
	for v in array_items(arr) {
		append(&out, string_value_of(v))
	}
	return out
}

// ---------------------------------------------------------------------------
// dates
// ---------------------------------------------------------------------------

// C++: void Tools::set_iso_date_value(Date& var, const Json& j, const string& key)
//
// The __DEFAULT__USED__ marker means an absent key leaves `var` untouched rather
// than resetting it to an invalid Date.
set_iso_date_value :: proc(var: ^d.Date, j: Value, key: string) {
	DEFAULT_MARKER :: "__DEFAULT__USED__"
	date_str := string_value_key_d(j, key, DEFAULT_MARKER)
	if date_str != DEFAULT_MARKER {
		var^ = d.from_iso_date_string(date_str)
	}
}

// C++: Date Tools::iso_date_value(const Json& j, const string& key)
iso_date_value :: proc(j: Value, key: string) -> d.Date {
	res: d.Date
	set_iso_date_value(&res, j, key)
	return res
}

// ---------------------------------------------------------------------------
// merge helpers
// ---------------------------------------------------------------------------

// C++: Errors Tools::defaultMerge(Json j, const function<Errors(Json)>& merge)
//
// If j wraps its content under a "DEFAULT" or "=" key, unwrap and re-merge.
// NOTE(c++-quirk): the second branch ASSIGNS to res rather than appending, so a
// value carrying both keys reports only the "=" errors. Reproduced.
default_merge :: proc(j: Value, merge: proc(_: Value) -> tl.Errors) -> tl.Errors {
	res: tl.Errors
	if is_object(get(j, "DEFAULT")) {
		res = merge(get(j, "DEFAULT"))
	}
	if is_object(get(j, "=")) {
		res = merge(get(j, "="))
	}
	return res
}

// C++: bool Json::has_shape(const shape& types, string& err) const
//
// MONICA only ever uses the single-key OBJECT form, e.g.
//   j.has_shape({{"cropParams", Json::OBJECT}}, err)
has_object_shape :: proc(j: Value, key: string) -> bool {
	if !is_object(j) {
		return false
	}
	return is_object(get(j, key))
}

// ---------------------------------------------------------------------------
// local helpers
// ---------------------------------------------------------------------------

@(private)
assign_dyn_f64 :: proc(dst: ^[dynamic]f64, src: [dynamic]f64) {
	clear(dst)
	for v in src {
		append(dst, v)
	}
	delete(src)
}

// ASCII-only upper, matching what the C++ toUpper does for these comparisons
@(private)
to_upper_ascii :: proc(s: string) -> string {
	if len(s) == 0 || len(s) > 16 {
		return s
	}
	@(static) buf: [16]byte
	for i in 0 ..< len(s) {
		c := s[i]
		if c >= 'a' && c <= 'z' {
			c -= 32
		}
		buf[i] = c
	}
	return string(buf[:len(s)])
}
