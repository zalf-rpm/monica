// The daily trace dumper - the primary oracle from phase 4 onward.
//
// Comparing only the final CSVs tells you *that* something diverged, not
// *where*. This walks the whole model state with core:reflect and emits one
// line per scalar field per day:
//
//     <day>\t<path.to.field>\t<value>
//
// The C++ side emits the same format via the macros in
// odin/tests/cpp_ref/trace_common.h. Diffing the two files gives the first
// (day, field) pair that disagrees, which is what turns a multi-day bug hunt
// into a one-line answer. This is exactly how the ff0f0fc regression was
// eventually cornered - see plan.md.
//
// Field paths match automatically because CONVENTIONS §2 requires Odin struct
// fields to keep their exact C++ spelling, and the C++ macros stringify the
// field name with #. If one side gains or loses a field the diff shows it as an
// added/removed line rather than as a silent mismatch, so the two stay honest
// without any shared manifest.
package trace

import "base:runtime"
import "core:c/libc"
import "core:fmt"
import "core:io"
import "core:reflect"
import "core:strconv"
import "core:strings"

// Emitting through a writer rather than fmt.print keeps the dump usable from a
// test as well as from the command line.
Tracer :: struct {
	w:       io.Writer,
	day:     int,
	// scratch, reused per line to avoid churning the allocator on a 2500-day run
	path:    strings.Builder,
	enabled: bool,
}

make_tracer :: proc(w: io.Writer, allocator := context.allocator) -> Tracer {
	return Tracer{w = w, path = strings.builder_make(allocator), enabled = true}
}

destroy_tracer :: proc(t: ^Tracer) {
	strings.builder_destroy(&t.path)
}

set_day :: proc(t: ^Tracer, day: int) {
	t.day = day
}

// ---------------------------------------------------------------------------
// value formatting - must match the C++ side exactly
// ---------------------------------------------------------------------------

// C's "%.17g", identical to what trace_common.h's DD macro prints.
// Odin's strconv 'g'/17 emits the same digits as MSVC's %.17g; only a leading
// '+' has to be stripped (established in phase 1a, see support/jsonx).
@(private)
write_f64 :: proc(t: ^Tracer, v: f64) {
	buf: [64]byte
	s := strconv.write_float(buf[:], v, 'g', 17, 64)
	if len(s) > 0 && s[0] == '+' {
		s = s[1:]
	}
	io.write_string(t.w, s)
}

@(private)
emit :: proc(t: ^Tracer, path: string, write_value: proc(_: ^Tracer, _: any), v: any) {
	if !t.enabled {
		return
	}
	io.write_int(t.w, t.day)
	io.write_byte(t.w, '\t')
	io.write_string(t.w, path)
	io.write_byte(t.w, '\t')
	write_value(t, v)
	io.write_byte(t.w, '\n')
}

// ---------------------------------------------------------------------------
// the walk
// ---------------------------------------------------------------------------

// Dumps every scalar reachable from `v` under `root_path`.
//
// Handled: all int/float kinds, bool, string, enums (as their underlying
// integer, matching the C++ `(int)enum` cast), nested structs, fixed arrays,
// slices, dynamic arrays, and Maybe/union.
//
// Deliberately skipped, with a marker line so the omission is visible rather
// than silent:
//   - pointers: the model is a graph (submodules hold back-pointers to
//     SoilColumn); following them would recurse forever and duplicate state
//     that is dumped at its owner. A "<ptr>" marker records nil vs non-nil,
//     which is the only part that can meaningfully diverge.
//   - maps: iteration order is unstable in Odin and the C++ std::map is
//     ordered, so a raw walk would produce spurious diffs. The few maps in the
//     model are dumped explicitly by their owner via dump_map_* below.
//   - procedure values.
dump :: proc(t: ^Tracer, root_path: string, v: any) {
	if !t.enabled {
		return
	}
	walk(t, root_path, v)
}

@(private)
walk :: proc(t: ^Tracer, path: string, v: any) {
	if v == nil {
		return
	}
	ti := reflect.type_info_base(type_info_of(v.id))

	#partial switch info in ti.variant {
	case reflect.Type_Info_Struct:
		fields := reflect.struct_fields_zipped(v.id)
		for f in fields {
			fv := reflect.struct_field_value(v, f)
			sub := join(t, path, f.name)
			walk(t, sub, fv)
		}
		return

	case reflect.Type_Info_Array:
		for i in 0 ..< info.count {
			ev := index_any(v, i, info.elem, info.elem_size)
			sub := join_index(t, path, i)
			walk(t, sub, ev)
		}
		return

	case reflect.Type_Info_Slice:
		s := (^runtime.Raw_Slice)(v.data)^
		for i in 0 ..< s.len {
			ev := any{rawptr(uintptr(s.data) + uintptr(i * info.elem_size)), info.elem.id}
			sub := join_index(t, path, i)
			walk(t, sub, ev)
		}
		return

	case reflect.Type_Info_Dynamic_Array:
		d := (^runtime.Raw_Dynamic_Array)(v.data)^
		for i in 0 ..< d.len {
			ev := any{rawptr(uintptr(d.data) + uintptr(i * info.elem_size)), info.elem.id}
			sub := join_index(t, path, i)
			walk(t, sub, ev)
		}
		return

	case reflect.Type_Info_Union:
		// Maybe(T) and friends. An unset Maybe must print as "unset", never as
		// the zero value - CONVENTIONS §5.
		tag := reflect.get_union_variant_raw_tag(v)
		if tag == 0 && len(info.variants) > 0 && !info.no_nil {
			write_line_str(t, path, "unset")
			return
		}
		inner := reflect.get_union_variant(v)
		if inner == nil {
			write_line_str(t, path, "unset")
			return
		}
		walk(t, path, inner)
		return

	case reflect.Type_Info_Pointer, reflect.Type_Info_Multi_Pointer:
		p := (^rawptr)(v.data)^
		write_line_str(t, path, p == nil ? "<ptr:nil>" : "<ptr:set>")
		return

	case reflect.Type_Info_Map:
		write_line_str(t, path, "<map:skipped>")
		return

	case reflect.Type_Info_Procedure:
		return

	case reflect.Type_Info_Enum:
		// the C++ prints enums via an (int) cast
		if iv, ok := reflect.as_i64(v); ok {
			write_line_int(t, path, int(iv))
		}
		return

	case reflect.Type_Info_String:
		s, _ := reflect.as_string(v)
		write_line_str(t, path, s)
		return

	case reflect.Type_Info_Boolean:
		b, _ := reflect.as_bool(v)
		write_line_str(t, path, b ? "true" : "false")
		return

	case reflect.Type_Info_Float:
		f, _ := reflect.as_f64(v)
		write_line_f64(t, path, f)
		return

	case reflect.Type_Info_Integer:
		if iv, ok := reflect.as_i64(v); ok {
			write_line_int(t, path, int(iv))
		}
		return
	}
}

// ---------------------------------------------------------------------------
// explicit emitters, for the cases the walk deliberately skips and for call
// sites that want to dump a derived value (a resolved getter, say) alongside
// the raw fields
// ---------------------------------------------------------------------------

write_line_f64 :: proc(t: ^Tracer, path: string, v: f64) {
	if !t.enabled {return}
	io.write_int(t.w, t.day)
	io.write_byte(t.w, '\t')
	io.write_string(t.w, path)
	io.write_byte(t.w, '\t')
	write_f64(t, v)
	io.write_byte(t.w, '\n')
}

write_line_int :: proc(t: ^Tracer, path: string, v: int) {
	if !t.enabled {return}
	io.write_int(t.w, t.day)
	io.write_byte(t.w, '\t')
	io.write_string(t.w, path)
	io.write_byte(t.w, '\t')
	io.write_int(t.w, v)
	io.write_byte(t.w, '\n')
}

write_line_str :: proc(t: ^Tracer, path: string, v: string) {
	if !t.enabled {return}
	io.write_int(t.w, t.day)
	io.write_byte(t.w, '\t')
	io.write_string(t.w, path)
	io.write_byte(t.w, '\t')
	io.write_string(t.w, v)
	io.write_byte(t.w, '\n')
}

// A std::map<int,double> / map[int]f64, dumped in ascending key order so it
// matches the C++ std::map iteration.
dump_map_int_f64 :: proc(t: ^Tracer, path: string, m: map[int]f64, allocator := context.allocator) {
	if !t.enabled {return}
	keys := make([dynamic]int, 0, len(m), allocator)
	defer delete(keys)
	for k in m {
		append(&keys, k)
	}
	// insertion sort: these maps have a handful of entries
	for i in 1 ..< len(keys) {
		k := keys[i]
		j := i - 1
		for j >= 0 && keys[j] > k {
			keys[j + 1] = keys[j]
			j -= 1
		}
		keys[j + 1] = k
	}
	for k in keys {
		sub := fmt.tprintf("%s[%d]", path, k)
		write_line_f64(t, sub, m[k])
	}
}

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------

// Paths are built in the temp allocator and are never freed individually - the
// caller does one free_all(context.temp_allocator) per day. Do NOT add a
// `defer delete(...)` on these: delete() uses context.allocator, so freeing
// temp memory through it corrupts the arena and silently truncates the walk
// after the first field (which is exactly what happened while bringing this up).
@(private)
join :: proc(t: ^Tracer, path, name: string) -> string {
	if len(path) == 0 {
		return name
	}
	return strings.concatenate({path, ".", name}, context.temp_allocator)
}

@(private)
join_index :: proc(t: ^Tracer, path: string, i: int) -> string {
	return fmt.tprintf("%s[%d]", path, i)
}

@(private)
index_any :: proc(v: any, i: int, elem: ^reflect.Type_Info, elem_size: int) -> any {
	return any{rawptr(uintptr(v.data) + uintptr(i * elem_size)), elem.id}
}

_ :: libc
_ :: emit
