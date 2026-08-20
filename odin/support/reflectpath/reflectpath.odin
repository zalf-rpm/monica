// A compiled field-path resolver over core:reflect.
//
// `compile` turns a textual path like "soilColumn.layers.vs_SoilNH4" into a
// flat list of pointer-arithmetic steps against a known root type; `resolve`
// then walks that list to the field's address on a live value. The point is
// output specs: a sim.json can name any field reachable from MonicaModel
// instead of only the ids someone pre-registered a lambda for.
//
// This package deliberately knows nothing about monica - it takes a `typeid`
// and a string - so it is unit-testable standalone (CONVENTIONS §6: support/
// is the generic tier).
//
// THE PERFORMANCE CONTRACT, and the reason `compile` exists at all:
// everything that needs a *name* is done once, at compile time. A resolve is
// pointer adds, one load per pointer/slice segment, and at most one map hash.
// Specifically, NOTHING in `resolve` may call reflect.struct_field_by_name
// (a linear scan over the struct's field names) or reflect.iterate_map (a
// linear scan over the map's capacity). A daily section with 20 layer
// outputs over 2,500 days resolves ~10^6 times; a name scan in there is
// ~10^6 string comparisons for a value that never changes.
//
// Path syntax (see plan-reflective-outputs.md §2.2):
//
//   a.b.c              struct field traversal
//   layers.3.field     fixed index into a fixed array / slice / dynamic array
//   layers[3].field    ... same thing; both spellings accepted so that
//                      trace.odin's output paths are valid output specs
//   layers.field       the array is left unindexed - the OPEN dimension,
//                      driven by the caller (OId.fromLayer..toLayer)
//   ...field.#len      length of an array/slice/dynamic array
//   climateData.#last  last (or .#first) element of an array/slice/dyn array
//   climateData.#last.tavg
//                      map segment: the next segment IS the key. Enum keys
//                      are resolved by name at compile time, string keys are
//                      taken literally, integer/bool keys are parsed.
//
// Pointer and Maybe(T) segments are traversed automatically. A missing value
// - nil pointer, unset Maybe, out-of-range index, absent map key - is
// reported as `ok = false` rather than trapping, matching what every legacy
// output lambda's `... ? ... : 0.0` ternary does.
//
// Rules enforced at compile time:
//   - at most one OPEN dimension per path (write_output flattens one level)
//   - a map may NOT be the open dimension: Odin's map iteration order is
//     unstable, so "all keys" would emit CSV columns in a nondeterministic
//     order. Keyed access only - the same reason trace.odin refuses to walk
//     maps.
//   - #len must be the last segment.
package reflectpath

import "base:intrinsics"
import "base:runtime"
import "core:fmt"
import "core:reflect"
import "core:strconv"
import "core:strings"

// How a step's container stores its data+length. Precomputed so `resolve`
// never has to look at a Type_Info.
Container :: enum u8 {
	NONE,
	FIXED, // [N]T   - data is the pointer itself, length is Step.count
	SLICE, // []T    - runtime.Raw_Slice
	DYNAMIC, // [dynamic]T - runtime.Raw_Dynamic_Array
}

Step_Kind :: enum u8 {
	FIELD, // ptr += offset
	INDEX, // ptr = data + index*elem_size, fixed index
	OPEN, // ptr = data + open_index*elem_size, index supplied per resolve
	DEREF, // ptr = ptr^        (nil -> missing)
	UNWRAP, // Maybe(T)          (unset -> missing)
	LEN, // terminal: yields the container's length as an int
	MAP_KEY, // hashed lookup with a compile-time-encoded key
	LAST, // ptr = data + (n-1)*elem_size
	FIRST, // ptr = data + 0
}

Step :: struct {
	kind:       Step_Kind,
	container:  Container, // INDEX/OPEN/LAST/FIRST/LEN
	offset:     uintptr, // FIELD
	index:      int, // INDEX
	elem_size:  uintptr, // INDEX/OPEN/LAST/FIRST
	count:      int, // FIXED container length
	tag_offset: uintptr, // UNWRAP
	tag_size:   int, // UNWRAP
	// The type *after* this step, resolved at COMPILE time. resolve() never
	// needs it (it is pure pointer arithmetic); it is kept for diagnostics
	// and so callers can introspect a plan without re-walking the types.
	type:       typeid,
	key:        []byte, // MAP_KEY: the key's raw bytes, encoded at compile time
	map_info:   ^runtime.Map_Info, // MAP_KEY
	name:       string, // diagnostics only; points into Path_Plan.src
}

Path_Plan :: struct {
	steps:       []Step,
	root:        typeid,
	leaf:        typeid,
	open_step:   int, // index of the OPEN step, -1 if the path is scalar
	src:         string, // the path, owned by the plan
	// Backing store for a #len result, so `resolve` can hand back an `any`
	// that points at real memory rather than at one of its own locals. This
	// is why resolve takes ^Path_Plan and not Path_Plan.
	len_scratch: int,
}

Error_Kind :: enum {
	NONE,
	EMPTY_PATH,
	BAD_SYNTAX,
	UNKNOWN_FIELD,
	NOT_TRAVERSABLE,
	BAD_INDEX,
	TWO_OPEN_DIMENSIONS,
	LEN_NOT_TERMINAL,
	MAP_NEEDS_KEY,
	UNKNOWN_MAP_KEY,
	UNSUPPORTED_KEY_TYPE,
	OUT_OF_MEMORY,
}

Compile_Error :: struct {
	kind: Error_Kind,
	at:   string, // the offending segment, "" if the whole path is at fault
	msg:  string, // human-readable, allocated in compile's allocator
}

@(require_results)
failed :: proc(e: Compile_Error) -> bool {
	return e.kind != .NONE
}

// ---------------------------------------------------------------------------
// compile
// ---------------------------------------------------------------------------

// Turns `path` into a Path_Plan against `root`. Everything the plan owns -
// the step slice, the path text, the encoded map keys - comes from
// `allocator`; nothing is individually freed (CONVENTIONS §3), so pass the
// run's arena.
compile :: proc(
	root: typeid,
	path: string,
	allocator := context.allocator,
) -> (
	plan: Path_Plan,
	err: Compile_Error,
) {
	plan.root = root
	plan.open_step = -1
	plan.leaf = root

	if len(path) == 0 {
		return plan, mkerr(.EMPTY_PATH, "", "empty path", allocator)
	}

	// Clone once, up front: every Step.name and plan.src is a slice into
	// this, so the caller's string can be temp-allocated.
	src := strings.clone(path, allocator)
	plan.src = src

	segs, serr := split_path(src, allocator)
	if serr != .NONE {
		return plan, mkerr(serr, src, "malformed path", allocator)
	}

	steps := make([dynamic]Step, 0, len(segs) + 4, allocator)
	cur := root
	terminated := false

	seg_loop: for seg in segs {
		if terminated {
			return plan, mkerr(.LEN_NOT_TERMINAL, seg, "#len must be the last segment", allocator)
		}

		// A single segment can consume more than one step: pointers and
		// Maybes are peeled first, and an unindexed array opens a dimension
		// and then re-applies the segment to the element type.
		for {
			cur = peel(&steps, cur)
			ti := runtime.type_info_base(type_info_of(cur))

			ck, elem, esz, cnt := container_of(ti)
			if ck != .NONE {
				switch {
				case seg == "#len":
					append(
						&steps,
						Step {
							kind = .LEN,
							container = ck,
							count = cnt,
							type = typeid_of(int),
							name = seg,
						},
					)
					cur = typeid_of(int)
					terminated = true
					continue seg_loop
				case seg == "#last", seg == "#first":
					append(
						&steps,
						Step {
							kind = seg == "#last" ? .LAST : .FIRST,
							container = ck,
							elem_size = esz,
							count = cnt,
							type = elem.id,
							name = seg,
						},
					)
					cur = elem.id
					continue seg_loop
				case is_index(seg):
					n, _ := strconv.parse_int(seg, 10)
					append(
						&steps,
						Step {
							kind = .INDEX,
							container = ck,
							index = n,
							elem_size = esz,
							count = cnt,
							type = elem.id,
							name = seg,
						},
					)
					cur = elem.id
					continue seg_loop
				case:
					// Unindexed: this array IS the open dimension. Emit the
					// OPEN and re-interpret the same segment against the
					// element type.
					if plan.open_step >= 0 {
						return plan, mkerr(
							.TWO_OPEN_DIMENSIONS,
							seg,
							"a path may leave at most one array unindexed",
							allocator,
						)
					}
					plan.open_step = len(steps)
					append(
						&steps,
						Step {
							kind = .OPEN,
							container = ck,
							elem_size = esz,
							count = cnt,
							type = elem.id,
							name = "",
						},
					)
					cur = elem.id
					continue
				}
			}

			#partial switch info in ti.variant {
			case runtime.Type_Info_Struct:
				f := reflect.struct_field_by_name(cur, seg)
				if f.type == nil {
					return plan, mkerr(
						.UNKNOWN_FIELD,
						seg,
						fmt.aprintf("no field %q on %v", seg, cur, allocator = allocator),
						allocator,
					)
				}
				append(
					&steps,
					Step{kind = .FIELD, offset = f.offset, type = f.type.id, name = seg},
				)
				cur = f.type.id
				continue seg_loop

			case runtime.Type_Info_Map:
				key, kerr := encode_map_key(info.key, seg, allocator)
				if kerr != .NONE {
					return plan, mkerr(
						kerr,
						seg,
						fmt.aprintf(
							"%q is not a usable key for %v",
							seg,
							cur,
							allocator = allocator,
						),
						allocator,
					)
				}
				append(
					&steps,
					Step {
						kind = .MAP_KEY,
						key = key,
						map_info = info.map_info,
						type = info.value.id,
						name = seg,
					},
				)
				cur = info.value.id
				continue seg_loop
			}

			return plan, mkerr(
				.NOT_TRAVERSABLE,
				seg,
				fmt.aprintf("cannot take %q from %v", seg, cur, allocator = allocator),
				allocator,
			)
		}
	}

	if !terminated {
		// A path that ends on a container leaves that container open, which
		// is what makes "soilColumn.layers.vs_SoilNH4" and
		// "currentCropModule.vc_OrganBiomass" (organ-indexed) work with the
		// same OId machinery.
		cur = peel(&steps, cur)
		ti := runtime.type_info_base(type_info_of(cur))
		if _, is_map := ti.variant.(runtime.Type_Info_Map); is_map {
			return plan, mkerr(
				.MAP_NEEDS_KEY,
				"",
				"a map may not be the open dimension - name a key",
				allocator,
			)
		}
		if ck, elem, esz, cnt := container_of(ti); ck != .NONE {
			if plan.open_step >= 0 {
				return plan, mkerr(
					.TWO_OPEN_DIMENSIONS,
					"",
					"a path may leave at most one array unindexed",
					allocator,
				)
			}
			plan.open_step = len(steps)
			append(
				&steps,
				Step {
					kind = .OPEN,
					container = ck,
					elem_size = esz,
					count = cnt,
					type = elem.id,
				},
			)
			cur = peel(&steps, elem.id)
		}
	}

	plan.steps = steps[:]
	plan.leaf = cur
	return plan, Compile_Error{}
}

// True if the plan's leaf is something the CSV writer can print as a number.
@(require_results)
leaf_is_numeric :: proc(plan: ^Path_Plan) -> bool {
	ti := runtime.type_info_base(type_info_of(plan.leaf))
	#partial switch _ in ti.variant {
	case runtime.Type_Info_Float, runtime.Type_Info_Integer, runtime.Type_Info_Boolean, runtime.Type_Info_Enum:
		return true
	}
	return false
}

// ---------------------------------------------------------------------------
// resolve
// ---------------------------------------------------------------------------

// Walks `plan` against `root` and returns an `any` pointing at the real
// field - so writing through it writes the model, which is what lets the
// SetValue workstep reuse this.
//
// `open_index` is ignored by scalar plans. `ok = false` means the value is
// *missing* (nil pointer, unset Maybe, index out of range, absent map key);
// callers substitute 0.0, matching the legacy lambdas.
resolve :: proc(plan: ^Path_Plan, root: any, open_index := 0) -> (out: any, ok: bool) {
	if root == nil || root.data == nil {
		return
	}
	ptr := root.data
	for i in 0 ..< len(plan.steps) {
		s := &plan.steps[i]
		if s.kind == .LEN {
			_, n := container_data_len(ptr, s.container, s.count)
			plan.len_scratch = n
			return any{&plan.len_scratch, plan.leaf}, true
		}
		p, step_ok := apply_step(s, ptr, open_index)
		if !step_ok {
			return
		}
		ptr = p
	}
	return any{ptr, plan.leaf}, true
}

// The convenience the output path actually uses. A missing value is (0, false)
// so the caller can decide between "emit 0.0" and "skip".
resolve_f64 :: proc(plan: ^Path_Plan, root: any, open_index := 0) -> (v: f64, ok: bool) {
	a := resolve(plan, root, open_index) or_return
	return as_f64(a)
}

// core:reflect's own as_f64 is not usable here: its Type_Info_Integer arm is a
// `switch v in a` over the *sized* integer types (i8..i128, u8..u128, and the
// endian-tagged variants) with a `case: valid = false` default, so a plain
// `int` - which type_info_core leaves as `int`, not i64 - comes back invalid.
// `#len` and `cultivationMethodCount` are exactly that. Reading the bytes off
// the size/signedness also skips the `any` type-switch on the daily path.
//
// The big-endian-tagged types (f64be, i32be, ...) would be misread; the model
// has none, and a path that reached one would be a translation bug anyway.
@(require_results)
as_f64 :: proc(a: any) -> (f64, bool) {
	if a == nil || a.data == nil {
		return 0, false
	}
	// type_info_core strips Named AND Enum, so an enum leaf arrives here as
	// its underlying integer - matching the `(int)enum` cast the C++ prints.
	ti := runtime.type_info_core(type_info_of(a.id))
	#partial switch info in ti.variant {
	case runtime.Type_Info_Float:
		switch ti.size {
		case 2:
			return f64((^f16)(a.data)^), true
		case 4:
			return f64((^f32)(a.data)^), true
		case 8:
			return (^f64)(a.data)^, true
		}
	case runtime.Type_Info_Integer:
		switch ti.size {
		case 1:
			return info.signed ? f64((^i8)(a.data)^) : f64((^u8)(a.data)^), true
		case 2:
			return info.signed ? f64((^i16)(a.data)^) : f64((^u16)(a.data)^), true
		case 4:
			return info.signed ? f64((^i32)(a.data)^) : f64((^u32)(a.data)^), true
		case 8:
			return info.signed ? f64((^i64)(a.data)^) : f64((^u64)(a.data)^), true
		case 16:
			return info.signed ? f64((^i128)(a.data)^) : f64((^u128)(a.data)^), true
		}
	case runtime.Type_Info_Boolean:
		switch ti.size {
		case 1:
			return (^b8)(a.data)^ ? 1 : 0, true
		case 2:
			return (^b16)(a.data)^ ? 1 : 0, true
		case 4:
			return (^b32)(a.data)^ ? 1 : 0, true
		case 8:
			return (^b64)(a.data)^ ? 1 : 0, true
		}
	}
	return 0, false
}

// Length of the open dimension on this particular value - i.e. how many
// layers/organs the caller may iterate. 0 for a scalar plan, and 0 if the
// path to the open array is itself missing.
open_length :: proc(plan: ^Path_Plan, root: any) -> int {
	if plan.open_step < 0 || root == nil || root.data == nil {
		return 0
	}
	ptr := root.data
	for i in 0 ..< plan.open_step {
		p, ok := apply_step(&plan.steps[i], ptr, 0)
		if !ok {
			return 0
		}
		ptr = p
	}
	s := &plan.steps[plan.open_step]
	_, n := container_data_len(ptr, s.container, s.count)
	return n
}

@(private)
apply_step :: proc(s: ^Step, ptr: rawptr, open_index: int) -> (rawptr, bool) {
	switch s.kind {
	case .FIELD:
		return rawptr(uintptr(ptr) + s.offset), true

	case .DEREF:
		p := (^rawptr)(ptr)^
		return p, p != nil

	case .UNWRAP:
		// Odin lays a union's variant data at offset 0 and its tag at
		// tag_offset, so an set Maybe(T) needs no pointer adjustment at all -
		// only the tag test. Tag 0 is the nil variant (CONVENTIONS §5: unset
		// must never collapse to the zero value).
		if read_tag(rawptr(uintptr(ptr) + s.tag_offset), s.tag_size) == 0 {
			return nil, false
		}
		return ptr, true

	case .INDEX, .OPEN, .FIRST, .LAST:
		data, n := container_data_len(ptr, s.container, s.count)
		idx: int
		#partial switch s.kind {
		case .INDEX:
			idx = s.index
		case .OPEN:
			idx = open_index
		case .FIRST:
			idx = 0
		case .LAST:
			idx = n - 1
		}
		if idx < 0 || idx >= n {
			return nil, false
		}
		return rawptr(uintptr(data) + uintptr(idx) * s.elem_size), true

	case .MAP_KEY:
		rm := (^runtime.Raw_Map)(ptr)
		if rm.len == 0 {
			return nil, false
		}
		kp := raw_data(s.key)
		h := s.map_info.key_hasher(kp, runtime.map_seed(rm^))
		v := runtime.__dynamic_map_get(rm, s.map_info, h, kp)
		return v, v != nil

	case .LEN:
	// handled by the caller, which owns the scratch slot
	}
	return nil, false
}

@(private)
container_data_len :: proc(ptr: rawptr, kind: Container, count: int) -> (rawptr, int) {
	switch kind {
	case .FIXED:
		return ptr, count
	case .SLICE:
		r := (^runtime.Raw_Slice)(ptr)^
		return r.data, r.len
	case .DYNAMIC:
		r := (^runtime.Raw_Dynamic_Array)(ptr)^
		return r.data, r.len
	case .NONE:
	}
	return nil, 0
}

@(private)
read_tag :: proc(p: rawptr, size: int) -> i64 {
	switch size {
	case 1:
		return i64((^u8)(p)^)
	case 2:
		return i64((^u16)(p)^)
	case 4:
		return i64((^u32)(p)^)
	case 8:
		return i64((^u64)(p)^)
	}
	return 0
}

// ---------------------------------------------------------------------------
// the reflect.iterate_map oracle
// ---------------------------------------------------------------------------

// A linear scan equivalent of the MAP_KEY step. This is NOT on the daily
// path - it exists as the cross-check the hashed lookup is tested against
// (plan-reflective-outputs.md §5), and as a diagnostic when a key that
// obviously exists fails to resolve.
map_lookup_scan :: proc(m: any, key: any) -> (out: any, ok: bool) {
	it := 0
	for {
		k, v, more := reflect.iterate_map(m, &it)
		if !more {
			return
		}
		if k.id == key.id && runtime.memory_equal(k.data, key.data, reflect.size_of_typeid(k.id)) {
			return v, true
		}
	}
}

// ---------------------------------------------------------------------------
// compile-time helpers
// ---------------------------------------------------------------------------

// Emits DEREF/UNWRAP steps until `cur` is something a segment can be applied
// to. Traversing pointers is what makes soilMoisture.soilColumn (a back-
// pointer into the model) and currentCropModule (nil when nothing is
// planted) reachable.
@(private)
peel :: proc(steps: ^[dynamic]Step, cur: typeid) -> typeid {
	cur := cur
	for {
		ti := runtime.type_info_base(type_info_of(cur))
		#partial switch info in ti.variant {
		case runtime.Type_Info_Pointer:
			if info.elem == nil {
				return cur // rawptr: nothing to traverse to
			}
			append(steps, Step{kind = .DEREF, type = info.elem.id})
			cur = info.elem.id
			continue
		case runtime.Type_Info_Union:
			// Maybe(T)-shaped only: one variant plus the nil state. A
			// general union has no single "the value" to traverse into.
			if len(info.variants) != 1 || info.no_nil || info.tag_type == nil {
				return cur
			}
			append(
				steps,
				Step {
					kind = .UNWRAP,
					tag_offset = info.tag_offset,
					tag_size = info.tag_type.size,
					type = info.variants[0].id,
				},
			)
			cur = info.variants[0].id
			continue
		}
		return cur
	}
}

@(private)
container_of :: proc(
	ti: ^runtime.Type_Info,
) -> (
	kind: Container,
	elem: ^runtime.Type_Info,
	elem_size: uintptr,
	count: int,
) {
	#partial switch info in ti.variant {
	case runtime.Type_Info_Array:
		return .FIXED, info.elem, uintptr(info.elem_size), info.count
	case runtime.Type_Info_Slice:
		return .SLICE, info.elem, uintptr(info.elem_size), 0
	case runtime.Type_Info_Dynamic_Array:
		return .DYNAMIC, info.elem, uintptr(info.elem_size), 0
	}
	return .NONE, nil, 0, 0
}

@(private)
is_index :: proc(s: string) -> bool {
	if len(s) == 0 {
		return false
	}
	for c in transmute([]byte)s {
		if c < '0' || c > '9' {
			return false
		}
	}
	return true
}

// Encodes a key segment into the map's key representation, once, so that
// `resolve` can hand the bytes straight to key_hasher/key_equal.
//
// NOTE: integer and enum keys are written as native-endian bytes, which is
// the same thing the compiler emits for a literal key. Correct on every
// target this port builds for (x86-64 / arm64, both little-endian).
@(private)
encode_map_key :: proc(
	key_ti: ^runtime.Type_Info,
	seg: string,
	allocator: runtime.Allocator,
) -> (
	[]byte,
	Error_Kind,
) {
	if key_ti == nil {
		return nil, .UNSUPPORTED_KEY_TYPE
	}
	buf, aerr := runtime.mem_alloc_bytes(key_ti.size, key_ti.align, allocator)
	if aerr != nil || len(buf) < key_ti.size {
		return nil, .OUT_OF_MEMORY
	}

	base := runtime.type_info_base(key_ti)
	#partial switch info in base.variant {
	case runtime.Type_Info_Enum:
		// Resolve the name against the *named* type, once. This is the whole
		// reason a map key never costs a string comparison at runtime.
		ev, ok := reflect.enum_from_name_any(key_ti.id, seg)
		if !ok {
			return nil, .UNKNOWN_MAP_KEY
		}
		v := i64(ev)
		intrinsics.mem_copy_non_overlapping(raw_data(buf), &v, min(key_ti.size, size_of(i64)))
		return buf, .NONE

	case runtime.Type_Info_Integer:
		v, ok := strconv.parse_i64_of_base(seg, 10)
		if !ok {
			return nil, .UNKNOWN_MAP_KEY
		}
		intrinsics.mem_copy_non_overlapping(raw_data(buf), &v, min(key_ti.size, size_of(i64)))
		return buf, .NONE

	case runtime.Type_Info_Boolean:
		if seg != "true" && seg != "false" {
			return nil, .UNKNOWN_MAP_KEY
		}
		if seg == "true" {
			(^bool)(raw_data(buf))^ = true
		}
		return buf, .NONE

	case runtime.Type_Info_String:
		if info.is_cstring {
			return nil, .UNSUPPORTED_KEY_TYPE
		}
		(^string)(raw_data(buf))^ = strings.clone(seg, allocator)
		return buf, .NONE
	}
	return nil, .UNSUPPORTED_KEY_TYPE
}

// Splits "a.b[3].c" into ["a", "b", "3", "c"]. Accepting both `[3]` and `.3`
// is what makes a trace.odin path a valid output spec: diff the trace, paste
// the diverging path straight into sim.json.
@(private)
split_path :: proc(
	path: string,
	allocator: runtime.Allocator,
) -> (
	[]string,
	Error_Kind,
) {
	out := make([dynamic]string, 0, 8, allocator)
	i, start := 0, 0
	for i < len(path) {
		switch path[i] {
		case '.':
			if i == start {
				// "a[0].b": the separator right after a ']' has nothing to
				// close, which is legal. A genuinely empty segment is not.
				if i > 0 && path[i - 1] == ']' {
					i += 1
					start = i
					continue
				}
				return nil, .BAD_SYNTAX
			}
			append(&out, path[start:i])
			i += 1
			start = i
		case '[':
			if i > start {
				append(&out, path[start:i])
			}
			j := i + 1
			for j < len(path) && path[j] != ']' {
				j += 1
			}
			if j >= len(path) || j == i + 1 {
				return nil, .BAD_SYNTAX
			}
			append(&out, path[i + 1:j])
			i = j + 1
			start = i
		case:
			i += 1
		}
	}
	if start < len(path) {
		append(&out, path[start:])
	} else if len(path) > 0 && path[len(path) - 1] != ']' {
		return nil, .BAD_SYNTAX // trailing '.'
	}
	if len(out) == 0 {
		return nil, .EMPTY_PATH
	}
	return out[:], .NONE
}

@(private)
mkerr :: proc(
	kind: Error_Kind,
	at: string,
	msg: string,
	allocator: runtime.Allocator,
) -> Compile_Error {
	return Compile_Error{kind = kind, at = at, msg = msg}
}
