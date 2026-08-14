// Construction helpers for building json11-compatible values.
//
// The C++ writes `json11::Json::object{{"key", doubleField}, ...}` and relies on
// json11's implicit constructors to pick JsonDouble/JsonInt/JsonBoolean/JsonString.
// Odin has no implicit conversions, so the type must be stated - and stating it
// is a feature here: it keeps the Integer/Float distinction (which `dump`
// formats differently, %d vs %.17g) explicit at every call site.
package jsonx

import "base:runtime"
import "core:strings"

// C++: json11::Json(double)
f :: proc(v: f64) -> Value {return Value(Float(v))}

// C++: json11::Json(int)
i :: proc(v: int) -> Value {return Value(Integer(i64(v)))}

// C++: json11::Json(bool)
b :: proc(v: bool) -> Value {return Value(Boolean(v))}

// C++: json11::Json(const std::string&) - clones into the allocator
s :: proc(v: string, allocator := context.allocator) -> Value {
	return Value(String(strings.clone(v, allocator)))
}

// A string that is known to outlive the value (a literal), stored without cloning.
sl :: proc(v: string) -> Value {return Value(String(v))}

// C++: Tools::J11Array{...}
arr :: proc(allocator: Allocator, items: ..Value) -> Value {
	a := make(Array, 0, len(items), allocator)
	append(&a, ..items)
	return Value(a)
}

// The pervasive MONICA [value, "unit"] pair, e.g. J11Array{sop->po_Denit1, ""}.
vu :: proc(v: f64, unit: string, allocator: Allocator) -> Value {
	return arr(allocator, f(v), sl(unit))
}

// The integer flavour, e.g. J11Array{sp->code_vnit, ""}.
vu_int :: proc(v: int, unit: string, allocator: Allocator) -> Value {
	return arr(allocator, i(v), sl(unit))
}

Allocator :: runtime.Allocator

// Builds an object from key/value pairs. Keys are cloned; the dump sorts them,
// so insertion order is irrelevant.
Pair :: struct {
	key: string,
	val: Value,
}

obj :: proc(allocator: Allocator, pairs: ..Pair) -> Value {
	o := make(Object, 0, allocator)
	for p in pairs {
		o[strings.clone(p.key, allocator)] = p.val
	}
	return Value(o)
}

// Sets one key on an existing object.
obj_set :: proc(o: ^Object, key: string, v: Value, allocator: Allocator) {
	o[strings.clone(key, allocator)] = v
}
