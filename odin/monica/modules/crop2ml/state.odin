// Helpers for the bindings of generated Crop2ML packages (monica/modules/*,
// the packages themselves are in amei/).
//
// The previous-day state. A Crop2ML model gets its state twice: `s`, today's,
// which it writes, and `s1`, the previous day's, which it reads where its
// metadata names an input `<x>_t1` (the generator turns `x_t1` into `s1.x`,
// e.g. SiriusQuality's LeafNumber: `s.leaf_number = s1.leaf_number + ...`).
// Filling `s1` is the host's job: before each calculate_model, `s` - still
// the end state of the previous day - is copied into `s1`. That is what
// copy_state does, and every binding calls it before calculate_model.
// (architecture.md, section 11.)
//
// The packages bound so far have no `_t1` inputs and never read `s1`; for
// them the copy changes nothing but costs a few hundred bytes a day.
package crop2ml

import "base:runtime"
import "core:fmt"
import "core:mem"

// Copies the state `src` (today's `s`) into `dst` (the previous day's `s1`),
// deeply: `dst`'s lists get their own memory, so a model writing `s.x[i]`
// can't change `s1.x[i]`. The lists of `dst` are allocated with `allocator`
// the first time and then reused (grown if `src`'s get longer), so a daily
// copy doesn't allocate; `destroy_<..>_state(dst)` frees them as usual.
// A dynamic array keeps the allocator of its first allocation and grows with
// that one: `allocator` only matters on the first call. MONICA's first module
// step gets the model's allocator and the later ones a daily scratch arena
// (core/soil_temperature_impl.odin) - so `s1`'s lists live in the model's.
//
// Supported, as the Odin generator emits them: numbers, bools, enums,
// strings and dates (copied as values - strings are shared, the generated
// destroy procs never free them), fixed arrays and structs of those, and
// lists (`[dynamic]T`) of plain values. Anything else panics, naming the type.
copy_state :: proc(dst, src: ^$T, allocator := context.allocator, loc := #caller_location) {
	context.allocator = allocator
	copy_value(dst, src, type_info_of(T), loc)
}

@(private)
copy_value :: proc(dst, src: rawptr, ti: ^runtime.Type_Info, loc: runtime.Source_Code_Location) {
	base := runtime.type_info_base(ti)
	#partial switch info in base.variant {
	case runtime.Type_Info_Struct:
		if is_plain(base) {
			mem.copy(dst, src, base.size)
			return
		}
		for i in 0 ..< int(info.field_count) {
			off := info.offsets[i]
			copy_value(rawptr(uintptr(dst) + off), rawptr(uintptr(src) + off), info.types[i], loc)
		}
	case runtime.Type_Info_Array:
		if is_plain(base) {
			mem.copy(dst, src, base.size)
			return
		}
		for i in 0 ..< info.count {
			off := uintptr(i * info.elem_size)
			copy_value(rawptr(uintptr(dst) + off), rawptr(uintptr(src) + off), info.elem, loc)
		}
	case runtime.Type_Info_Dynamic_Array:
		if !is_plain(info.elem) {
			fmt.panicf("crop2ml.copy_state: lists of %v are not supported", info.elem.id, loc = loc)
		}
		s := (^runtime.Raw_Dynamic_Array)(src)
		d := (^runtime.Raw_Dynamic_Array)(dst)
		if !runtime.__dynamic_array_resize(d, info.elem_size, info.elem.align, s.len, loc) {
			fmt.panicf("crop2ml.copy_state: can't allocate %d %v", s.len, info.elem.id, loc = loc)
		}
		if s.len > 0 {
			mem.copy(d.data, s.data, s.len * info.elem_size)
		}
	case:
		if !is_plain(base) {
			fmt.panicf("crop2ml.copy_state: %v is not supported", ti.id, loc = loc)
		}
		mem.copy(dst, src, base.size)
	}
}

// A value that a byte copy copies completely (strings: shared, see above)
@(private)
is_plain :: proc(ti: ^runtime.Type_Info) -> bool {
	base := runtime.type_info_base(ti)
	#partial switch info in base.variant {
	case runtime.Type_Info_Integer, runtime.Type_Info_Float, runtime.Type_Info_Boolean,
	     runtime.Type_Info_Enum, runtime.Type_Info_Rune, runtime.Type_Info_String,
	     runtime.Type_Info_Bit_Set:
		return true
	case runtime.Type_Info_Array:
		return is_plain(info.elem)
	case runtime.Type_Info_Struct:
		for i in 0 ..< int(info.field_count) {
			if !is_plain(info.types[i]) {
				return false
			}
		}
		return true
	}
	return false
}
