// Port of the Errors / EResult / printPossibleErrors part of
// mas_cpp_misc/tools/helper.h + helper.cpp.
//
// MONICA accumulates multiple errors and warnings across a merge and inspects
// them afterwards, so this is deliberately NOT rewritten into Odin's
// `-> (T, Error)` idiom. See ../../CONVENTIONS.md §4.
package tools

import "base:runtime"
import "core:fmt"
import "core:strings"

// C++: Tools::Errors::Type
Error_Type :: enum {
	ERR,
	WARN,
}

// C++: struct Tools::Errors
//
// Owns its strings: every append clones into the Errors' allocator. Call
// errors_destroy when not running under an arena.
Errors :: struct {
	errors:    [dynamic]string,
	warnings:  [dynamic]string,
	allocator: runtime.Allocator,
}

make_errors :: proc(allocator := context.allocator) -> Errors {
	return Errors {
		errors = make([dynamic]string, allocator),
		warnings = make([dynamic]string, allocator),
		allocator = allocator,
	}
}

// C++: Errors(std::string e)
make_errors_from_error :: proc(e: string, allocator := context.allocator) -> Errors {
	es := make_errors(allocator)
	append_error(&es, e)
	return es
}

// C++: Errors(Type t, std::string m)
make_errors_typed :: proc(t: Error_Type, m: string, allocator := context.allocator) -> Errors {
	es := make_errors(allocator)
	if t == .ERR {
		append_error(&es, m)
	} else {
		append_warning(&es, m)
	}
	return es
}

errors_destroy :: proc(es: ^Errors) {
	for e in es.errors {
		delete(e, es.allocator)
	}
	for w in es.warnings {
		delete(w, es.allocator)
	}
	delete(es.errors)
	delete(es.warnings)
	es.errors = nil
	es.warnings = nil
}

// C++: bool success() const { return errors.empty(); }
success :: proc(es: Errors) -> bool {
	return len(es.errors) == 0
}

// C++: bool failure() const { return !success(); }
failure :: proc(es: Errors) -> bool {
	return !success(es)
}

// Lazily initialise the backing arrays so a zero-valued Errors (the common case
// for a struct field) behaves like a default-constructed C++ Errors.
@(private)
ensure_init :: proc(es: ^Errors) {
	if es.allocator.procedure == nil {
		es.allocator = context.allocator
	}
	if es.errors == nil {
		es.errors = make([dynamic]string, es.allocator)
	}
	if es.warnings == nil {
		es.warnings = make([dynamic]string, es.allocator)
	}
}

// C++: void appendError(std::string err)
append_error :: proc(es: ^Errors, err: string) {
	ensure_init(es)
	append(&es.errors, strings.clone(err, es.allocator))
}

// C++: void appendWarning(std::string warn)
append_warning :: proc(es: ^Errors, warn: string) {
	ensure_init(es)
	append(&es.warnings, strings.clone(warn, es.allocator))
}

append_errorf :: proc(es: ^Errors, format: string, args: ..any) {
	ensure_init(es)
	append(&es.errors, fmt.aprintf(format, ..args, allocator = es.allocator))
}

append_warningf :: proc(es: ^Errors, format: string, args: ..any) {
	ensure_init(es)
	append(&es.warnings, fmt.aprintf(format, ..args, allocator = es.allocator))
}

// C++: void append(const Errors& es)
append_errors :: proc(es: ^Errors, other: Errors) {
	ensure_init(es)
	for e in other.errors {
		append(&es.errors, strings.clone(e, es.allocator))
	}
	for w in other.warnings {
		append(&es.warnings, strings.clone(w, es.allocator))
	}
}

// C++: template<typename T> struct EResult : public Errors
//
// `using errs` mirrors the C++ public inheritance, so success(r.errs) works and
// the result field sits alongside.
EResult :: struct($T: typeid) {
	using errs: Errors,
	result:     T,
}

// C++: bool Tools::printPossibleErrors(const Errors& es, bool includeWarnings)
print_possible_errors :: proc(es: Errors, include_warnings := false) -> bool {
	if failure(es) {
		for e in es.errors {
			fmt.eprintln(e)
		}
	}
	if include_warnings {
		for w in es.warnings {
			fmt.eprintln(w)
		}
	}
	return success(es)
}

// C++: template<typename T> T printPossibleErrors(const EResult<T>&, bool)
//
// NOTE(c++-quirk): on failure the C++ returns a default-constructed T(), not the
// EResult's own result. Reproduced.
print_possible_errors_r :: proc(er: EResult($T), include_warnings := false) -> T {
	if print_possible_errors(er.errs, include_warnings) {
		return er.result
	}
	return T{}
}
