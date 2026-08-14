// Forgiving json11-compatible layer over core:encoding/json.
//
// Replaces mas_cpp_misc/json11/json11.{hpp,cpp}. json11 itself is NOT ported;
// this package supplies the semantics MONICA's merge/to_json code relies on:
//
//  1. Forgiving indexing. json11's `j["a"]["b"]` on a missing or mismatched
//     value returns a static null Json rather than failing. The whole merge()
//     codebase depends on that. `get` / `at` below reproduce it.
//  2. Number access that ignores the Integer/Float split. json11's own header
//     claims "all numbers are double", but its parse_number actually returns a
//     JsonInt when the token has no '.'/exponent and <= 9 digits. Reading is
//     transparent there (JsonInt::number_value widens, JsonDouble::int_value
//     truncates), so every accessor here MUST accept both variants. This is the
//     single biggest silent-regression risk in the port - see CONVENTIONS §1.
//  3. json11-compatible `dump`, needed for the phase 1 env_to_json oracle.
//
// Memory: parse into an arena and never free individual values (CONVENTIONS §3).
package jsonx

import "base:runtime"
import "core:encoding/json"
import "core:mem/virtual"
import "core:slice"
import "core:strconv"
import "core:strings"
import tl "../tools"

Value :: json.Value
Object :: json.Object
Array :: json.Array
Null :: json.Null
Integer :: json.Integer
Float :: json.Float
Boolean :: json.Boolean
String :: json.String

// ---------------------------------------------------------------------------
// arena
// ---------------------------------------------------------------------------

// All config JSON for one run lives here and dies at once. json.Value owns its
// Array/Object, and the C++ copies Json values freely (see
// findAndReplaceReferences in create-env-from-json-config.cpp), so individual
// frees are not safe.
Arena :: struct {
	arena: virtual.Arena,
}

arena_init :: proc(a: ^Arena) -> bool {
	return virtual.arena_init_growing(&a.arena) == nil
}

arena_allocator :: proc(a: ^Arena) -> runtime.Allocator {
	return virtual.arena_allocator(&a.arena)
}

arena_destroy :: proc(a: ^Arena) {
	virtual.arena_destroy(&a.arena)
}

// ---------------------------------------------------------------------------
// parsing
// ---------------------------------------------------------------------------

// C++: EResult<Json> Tools::parseJsonString(string jsonString)
//
// parse_integers is true so integer literals stay integers, matching json11's
// JsonInt. Spec is .JSON5, a strict superset of JSON.
//
// On failure core:encoding/json does not unwind the partially built value, so
// whatever it allocated stays allocated - harmless when parsing into the arena,
// which is the only supported usage.
parse_json_string :: proc(
	json_string: string,
	allocator := context.allocator,
) -> tl.EResult(Value) {
	res: tl.EResult(Value)
	res.allocator = allocator

	v, err := json.parse_string(json_string, .JSON5, true, allocator)
	if err != nil {
		tl.append_errorf(&res, "Error parsing JSON object: '%s' ", json_string)
		res.result = v
		return res
	}
	res.result = v
	return res
}

// C++: EResult<Json> Tools::readAndParseJsonFile(string path)
read_and_parse_json_file :: proc(path: string, allocator := context.allocator) -> tl.EResult(Value) {
	r := tl.read_file(path, allocator)
	if tl.success(r.errs) {
		return parse_json_string(r.result, allocator)
	}
	res: tl.EResult(Value)
	res.allocator = allocator
	tl.append_errors(&res, r.errs)
	return res
}

// ---------------------------------------------------------------------------
// type predicates - C++: Json::is_null() etc.
// ---------------------------------------------------------------------------

// A zero-valued Value (no union variant set) is also null, matching a
// default-constructed json11::Json.
is_null :: proc(v: Value) -> bool {
	switch _ in v {
	case json.Null:
		return true
	case json.Integer, json.Float, json.Boolean, json.String, json.Array, json.Object:
		return false
	}
	return true
}

is_number :: proc(v: Value) -> bool {
	switch _ in v {
	case json.Integer, json.Float:
		return true
	case json.Null, json.Boolean, json.String, json.Array, json.Object:
		return false
	}
	return false
}

is_bool :: proc(v: Value) -> bool {
	_, ok := v.(json.Boolean)
	return ok
}

is_string :: proc(v: Value) -> bool {
	_, ok := v.(json.String)
	return ok
}

is_array :: proc(v: Value) -> bool {
	_, ok := v.(json.Array)
	return ok
}

is_object :: proc(v: Value) -> bool {
	_, ok := v.(json.Object)
	return ok
}

// ---------------------------------------------------------------------------
// forgiving access - C++: Json::operator[](key) / operator[](size_t)
// ---------------------------------------------------------------------------

// C++: const Json& Json::operator[](const std::string& key) const
//
// Returns a null Value if `v` is not an object or the key is absent. Chainable:
// get(get(v, "a"), "b") never faults, exactly like json11.
get :: proc(v: Value, key: string) -> Value {
	o, ok := v.(json.Object)
	if !ok {
		return Value{}
	}
	child, found := o[key]
	if !found {
		return Value{}
	}
	return child
}

// Convenience for the common j["a"]["b"]["c"] chain.
get_path :: proc(v: Value, keys: ..string) -> Value {
	cur := v
	for k in keys {
		cur = get(cur, k)
	}
	return cur
}

// C++: const Json& Json::operator[](size_t i) const
at :: proc(v: Value, i: int) -> Value {
	a, ok := v.(json.Array)
	if !ok {
		return Value{}
	}
	if i < 0 || i >= len(a) {
		return Value{}
	}
	return a[i]
}

// C++: const array& Json::array_items() const - empty when not an array
array_items :: proc(v: Value) -> []Value {
	a, ok := v.(json.Array)
	if !ok {
		return nil
	}
	return a[:]
}

// C++: const object& Json::object_items() const - empty when not an object
object_items :: proc(v: Value) -> Object {
	o, ok := v.(json.Object)
	if !ok {
		return nil
	}
	return o
}

// json11's object is a std::map, i.e. iteration is sorted by key; Odin's is a
// hash map. Anywhere iteration order is observable, go through this.
object_keys_sorted :: proc(v: Value, allocator := context.allocator) -> []string {
	o, ok := v.(json.Object)
	if !ok {
		return nil
	}
	keys := make([]string, len(o), allocator)
	i := 0
	for k in o {
		keys[i] = k
		i += 1
	}
	slice.sort(keys)
	return keys
}

// ---------------------------------------------------------------------------
// raw value extraction - C++: Json::number_value() etc.
// ---------------------------------------------------------------------------

// C++: double Json::number_value() const - 0 when not a number.
// Accepts BOTH Integer and Float (see the header note).
number_value :: proc(v: Value) -> f64 {
	switch n in v {
	case json.Integer:
		return f64(n)
	case json.Float:
		return f64(n)
	case json.Null, json.Boolean, json.String, json.Array, json.Object:
		return 0
	}
	return 0
}

// C++: int Json::int_value() const - 0 when not a number.
// JsonDouble::int_value is static_cast<int>, i.e. truncation toward zero, which
// is what Odin's f64 -> int conversion does too.
int_value_of :: proc(v: Value) -> int {
	switch n in v {
	case json.Integer:
		return int(n)
	case json.Float:
		return int(n)
	case json.Null, json.Boolean, json.String, json.Array, json.Object:
		return 0
	}
	return 0
}

// C++: bool Json::bool_value() const - false when not a bool
bool_value_of :: proc(v: Value) -> bool {
	b, ok := v.(json.Boolean)
	return ok ? bool(b) : false
}

// C++: const std::string& Json::string_value() const - "" when not a string
string_value_of :: proc(v: Value) -> string {
	s, ok := v.(json.String)
	return ok ? string(s) : ""
}

// ---------------------------------------------------------------------------
// dump - C++: void Json::dump(string& out) const
// ---------------------------------------------------------------------------

// Reproduces json11's serialisation exactly, which is what makes the phase 1
// env_to_json diff possible:
//   - doubles via "%.17g", non-finite -> null
//   - ints via "%d"
//   - array separator ", ", object "key": value with ", "
//   - object keys sorted (json11's object is a std::map)
dump :: proc(v: Value, allocator := context.allocator) -> string {
	b := strings.builder_make(allocator)
	dump_to(&b, v)
	out := strings.clone(strings.to_string(b), allocator)
	strings.builder_destroy(&b)
	return out
}

dump_to :: proc(b: ^strings.Builder, v: Value) {
	switch n in v {
	case json.Null:
		strings.write_string(b, "null")
	case json.Integer:
		strings.write_i64(b, i64(n))
	case json.Float:
		write_double_json11(b, f64(n))
	case json.Boolean:
		strings.write_string(b, bool(n) ? "true" : "false")
	case json.String:
		write_string_json11(b, string(n))
	case json.Array:
		strings.write_string(b, "[")
		for item, i in n {
			if i > 0 {
				strings.write_string(b, ", ")
			}
			dump_to(b, item)
		}
		strings.write_string(b, "]")
	case json.Object:
		strings.write_string(b, "{")
		keys := make([]string, len(n), context.temp_allocator)
		i := 0
		for k in n {
			keys[i] = k
			i += 1
		}
		slice.sort(keys)
		for k, idx in keys {
			if idx > 0 {
				strings.write_string(b, ", ")
			}
			write_string_json11(b, k)
			strings.write_string(b, ": ")
			dump_to(b, n[k])
		}
		strings.write_string(b, "}")
	case:
		strings.write_string(b, "null")
	}
}

// C++: static void dump(double value, string& out) - snprintf("%.17g")
//
// Odin's generic_ftoa with 'g'/precision 17 emits the same digits and exponent
// form as MSVC's %.17g; the only difference is a leading '+' on non-negative
// values, which is stripped here. Verified against a C reference over a range
// of values including 0.1, 1/3, 1e17, 1e20 and 9.9999999999999995e-08.
@(private)
write_double_json11 :: proc(b: ^strings.Builder, value: f64) {
	if value != value || value == INF || value == -INF { 	// !isfinite -> "null"
		strings.write_string(b, "null")
		return
	}
	buf: [64]byte
	s := strconv.write_float(buf[:], value, 'g', 17, 64)
	if len(s) > 0 && s[0] == '+' {
		s = s[1:]
	}
	strings.write_string(b, s)
}

@(private)
INF :: 0h7FF0000000000000

// C++: static void dump(const string& value, string& out)
@(private)
write_string_json11 :: proc(b: ^strings.Builder, value: string) {
	strings.write_byte(b, '"')
	for i := 0; i < len(value); i += 1 {
		ch := value[i]
		switch {
		case ch == '\\':
			strings.write_string(b, "\\\\")
		case ch == '"':
			strings.write_string(b, "\\\"")
		case ch == '\b':
			strings.write_string(b, "\\b")
		case ch == '\f':
			strings.write_string(b, "\\f")
		case ch == '\n':
			strings.write_string(b, "\\n")
		case ch == '\r':
			strings.write_string(b, "\\r")
		case ch == '\t':
			strings.write_string(b, "\\t")
		case ch <= 0x1f:
			hex := "0123456789abcdef"
			strings.write_string(b, "\\u")
			strings.write_string(b, "00")
			strings.write_byte(b, hex[(ch >> 4) & 0xf])
			strings.write_byte(b, hex[ch & 0xf])
		case:
			// json11 also escapes U+2028/U+2029; those cannot appear in MONICA's
			// parameter files, so the 3-byte lookahead is skipped here.
			strings.write_byte(b, ch)
		}
	}
	strings.write_byte(b, '"')
}
