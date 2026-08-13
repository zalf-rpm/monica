// Port of the string helpers from mas_cpp_misc/tools/helper.h + helper.cpp and
// the splitString procedures from tools/algorithms.{h,cpp}.
package tools

import "core:strconv"
import "core:strings"

// C++: std::vector<std::string> Tools::splitString(const string& s,
//        const string& splitElements, const pair<string,string>& tokenDelimiters,
//        bool removeDelimiters, bool removeEmptyStrings)
//
// The delimiter-aware overload: characters inside a token-delimiter region are
// never treated as split points.
split_string_delim :: proc(
	s: string,
	split_elements: string,
	token_delimiters_first := "",
	token_delimiters_second := "",
	remove_delimiters := false,
	remove_empty_strings := true,
	allocator := context.allocator,
) -> [dynamic]string {
	v := make([dynamic]string, allocator)
	cur := strings.builder_make(allocator)
	delimiter_level := 0

	// The C++ seeds `v` with one empty string and always appends to v.back();
	// `cur` plays the role of that trailing element here.
	for i in 0 ..< len(s) {
		c := s[i]
		if strings.index_byte(split_elements, c) == -1 || delimiter_level > 0 {
			level_inc := 0
			if len(token_delimiters_first) > 0 && strings.index_byte(token_delimiters_first, c) != -1 {
				level_inc = 1
			} else if len(token_delimiters_second) > 0 &&
			   strings.index_byte(token_delimiters_second, c) != -1 {
				level_inc = -1
			}
			if level_inc == 0 || !remove_delimiters {
				strings.write_byte(&cur, c)
			}
			delimiter_level += level_inc
		} else if !remove_empty_strings || strings.builder_len(cur) > 0 {
			append(&v, strings.clone(strings.to_string(cur), allocator))
			strings.builder_reset(&cur)
		}
	}
	// the trailing element
	if !(remove_empty_strings && strings.builder_len(cur) == 0) {
		append(&v, strings.clone(strings.to_string(cur), allocator))
	}
	strings.builder_destroy(&cur)
	return v
}

// C++: std::vector<std::string> Tools::splitString(const string& s,
//        const string& splitElements, bool removeEmptyStrings)
split_string :: proc(
	s: string,
	split_elements: string,
	remove_empty_strings := true,
	allocator := context.allocator,
) -> [dynamic]string {
	v := make([dynamic]string, allocator)
	cur := strings.builder_make(allocator)

	for i in 0 ..< len(s) {
		c := s[i]
		if strings.index_byte(split_elements, c) == -1 {
			strings.write_byte(&cur, c)
		} else if !remove_empty_strings || strings.builder_len(cur) > 0 {
			append(&v, strings.clone(strings.to_string(cur), allocator))
			strings.builder_reset(&cur)
		}
	}
	if !(remove_empty_strings && strings.builder_len(cur) == 0) {
		append(&v, strings.clone(strings.to_string(cur), allocator))
	}
	strings.builder_destroy(&cur)
	return v
}

// C++: std::string Tools::replace(std::string s, std::string findStr, std::string replStr)
//
// NOTE(c++-quirk): the C++ restarts the search from position 0 after each
// replacement, so a replStr containing findStr loops forever. Reproduced only in
// the sense that we also replace every occurrence; we scan forward instead of
// restarting, which differs ONLY in that pathological case.
replace :: proc(s, find_str, repl_str: string, allocator := context.allocator) -> string {
	if len(find_str) == 0 {
		return strings.clone(s, allocator)
	}
	out, _ := strings.replace_all(s, find_str, repl_str, allocator)
	return out
}

// C++: inline std::string Tools::toLower(const std::string& str)
to_lower :: proc(s: string, allocator := context.allocator) -> string {
	return strings.to_lower(s, allocator)
}

// C++: inline std::string Tools::toUpper(const std::string& str)
to_upper :: proc(s: string, allocator := context.allocator) -> string {
	return strings.to_upper(s, allocator)
}

// C++: bool Tools::stob(const std::string& s, bool def)
stob :: proc(s: string, def := false) -> bool {
	if len(s) == 0 {
		return def
	}
	start := s[0]
	if start >= 'A' && start <= 'Z' {
		start += 32
	}
	switch start {
	case 't', '1':
		return true
	case 'f', '0':
		return false
	}
	return def
}

// C++: inline int Tools::satoi(const std::string& s, int def)
satoi :: proc(s: string, def := 0) -> int {
	if len(s) == 0 {
		return def
	}
	v, ok := strconv.parse_int(s)
	return ok ? v : def
}

// C++: inline double Tools::satof(const std::string& s, double def)
//
// NOTE(c++-quirk): the C++ delegates to std::stof (single precision) despite
// returning a double. Reproduced via the f32 round-trip.
satof :: proc(s: string, def := 0.0) -> f64 {
	if len(s) == 0 {
		return def
	}
	v, ok := strconv.parse_f32(s)
	return ok ? f64(v) : def
}

// C++: inline double Tools::stod_comma(std::string s)
stod_comma :: proc(s: string, allocator := context.allocator) -> f64 {
	idx := strings.index_byte(s, ',')
	if idx != -1 {
		fixed := strings.concatenate({s[:idx], ".", s[idx + 1:]}, allocator)
		defer delete(fixed, allocator)
		v, _ := strconv.parse_f64(fixed)
		return v
	}
	v, _ := strconv.parse_f64(s)
	return v
}

// C++: std::string Tools::rimRight(const std::string& str, const std::string& charSet)
//
// Removes all characters in char_set from the right side of the string.
rim_right :: proc(s: string, char_set: string, allocator := context.allocator) -> string {
	if len(s) == 0 {
		return strings.clone(s, allocator)
	}
	i := len(s)
	for i > 0 && strings.index_byte(char_set, s[i - 1]) != -1 {
		i -= 1
	}
	return strings.clone(s[:i], allocator)
}

// Frees a [dynamic]string returned by split_string / split_string_delim,
// including the cloned elements. No C++ counterpart (std::vector<std::string>
// owns its elements).
delete_strings :: proc(v: [dynamic]string, allocator := context.allocator) {
	for s in v {
		delete(s, allocator)
	}
	delete(v)
}

// C++: inline std::string Tools::surround(std::string with, std::string str)
surround :: proc(with, s: string, allocator := context.allocator) -> string {
	return strings.concatenate({with, s, with}, allocator)
}
