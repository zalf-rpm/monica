// Port of src/run/create-env-from-json-config.{h,cpp}.
//
// Two pieces:
//   - find_and_replace_references: the recursive machinery that expands the
//     ["include-from-file", ...] / ["ref", ...] / ["%", ...] / KA5 patterns that
//     MONICA's crop/site/sim JSON is written in.
//   - create_env_json_from_json_objects: assembles the Env JSON from the three
//     resolved documents.
//
// The C++ supportedPatterns() is a map<string, std::function<...>>; none of its
// lambdas capture anything, so this is a map of plain procedure values rather
// than a closure table (plan-odin.md prep 2).
package run

import "core:strings"
import "base:runtime"
import soil "../soil"
import jx "../../support/jsonx"
import tl "../../support/tools"

// C++: function<EResult<Json>(const Json&, const Json&)>
Pattern_Proc :: proc(root: jx.Value, j: jx.Value, allocator: Allocator) -> tl.EResult(jx.Value)

Allocator :: runtime.Allocator

// ---------------------------------------------------------------------------
// the patterns
// ---------------------------------------------------------------------------

// C++: auto ref = [](const Json& root, const Json& j) -> EResult<Json>
//
// NOTE(c++-quirk): the cache is a function-local `static` keyed only on the two
// key strings, NOT on `root`. Resolving two different configs in one process
// would therefore serve stale entries. monica-run only ever handles one config,
// so this is reproduced as-is rather than fixed.
@(private)
ref_cache: map[[2]string]jx.Value

@(private)
ref_cache_valid: map[[2]string]bool

@(private)
pattern_ref :: proc(root: jx.Value, j: jx.Value, allocator: Allocator) -> tl.EResult(jx.Value) {
	res: tl.EResult(jx.Value)
	res.allocator = allocator

	items := jx.array_items(j)
	if len(items) == 3 && jx.is_string(jx.at(j, 1)) && jx.is_string(jx.at(j, 2)) {
		key1 := jx.string_value_of(jx.at(j, 1))
		key2 := jx.string_value_of(jx.at(j, 2))
		ck := [2]string{key1, key2}

		if cached, ok := ref_cache[ck]; ok {
			res.result = cached
			return res
		}

		r := find_and_replace_references(root, jx.get(jx.get(root, key1), key2), allocator)
		ref_cache[ck] = r.result
		ref_cache_valid[ck] = tl.success(r.errs)
		res.result = r.result
		tl.append_errors(&res, r.errs)
		return res
	}

	res.result = j
	d := jx.dump(j, allocator)
	tl.append_errorf(&res, "Couldn't resolve reference: %s!", d)
	return res
}

// C++: auto fromFile = [](const Json& root, const Json& j) -> EResult<Json>
@(private)
pattern_from_file :: proc(
	root: jx.Value,
	j: jx.Value,
	allocator: Allocator,
) -> tl.EResult(jx.Value) {
	res: tl.EResult(jx.Value)
	res.allocator = allocator

	items := jx.array_items(j)
	if len(items) == 2 && jx.is_string(jx.at(j, 1)) {
		base_path := jx.string_value_key_d(root, "include-file-base-path", ".")
		path_to_file := jx.string_value_of(jx.at(j, 1))
		if !tl.is_absolute_path(path_to_file) {
			path_to_file = strings.concatenate({base_path, "/", path_to_file}, allocator)
		}
		path_to_file = tl.replace_env_vars(path_to_file, allocator)
		path_to_file = tl.fix_system_separator(path_to_file, allocator)

		jo := jx.read_and_parse_json_file(path_to_file, allocator)
		if tl.success(jo.errs) && !jx.is_null(jo.result) {
			res.result = jo.result
			return res
		}

		res.result = j
		tl.append_errorf(&res, "Couldn't include file with path: '%s'!", path_to_file)
		return res
	}

	res.result = j
	d := jx.dump(j, allocator)
	tl.append_errorf(&res, "Couldn't include file with function: %s!", d)
	return res
}

// C++: auto humus2corg = [](const Json&, const Json& j) -> EResult<Json>
@(private)
pattern_humus2corg :: proc(
	root: jx.Value,
	j: jx.Value,
	allocator: Allocator,
) -> tl.EResult(jx.Value) {
	res: tl.EResult(jx.Value)
	res.allocator = allocator

	if len(jx.array_items(j)) == 2 && jx.is_number(jx.at(j, 1)) {
		ecorg := soil.humus_class_2_corg(jx.int_value_of(jx.at(j, 1)), allocator)
		if tl.success(ecorg.errs) {
			res.result = jx.Value(jx.Float(ecorg.result))
			return res
		}
		res.result = j
		tl.append_errors(&res, ecorg.errs)
		return res
	}

	res.result = j
	d := jx.dump(j, allocator)
	tl.append_errorf(&res, "Couldn't convert humus level to corg: %s!", d)
	return res
}

// C++: auto bdc2rd = [](const Json&, const Json& j) -> EResult<Json>
@(private)
pattern_bdc2rd :: proc(root: jx.Value, j: jx.Value, allocator: Allocator) -> tl.EResult(jx.Value) {
	res: tl.EResult(jx.Value)
	res.allocator = allocator

	if len(jx.array_items(j)) == 3 && jx.is_number(jx.at(j, 1)) && jx.is_number(jx.at(j, 2)) {
		erd := soil.bulk_density_class_2_raw_density(
			jx.int_value_of(jx.at(j, 1)),
			jx.number_value(jx.at(j, 2)),
			allocator,
		)
		if tl.success(erd.errs) {
			res.result = jx.Value(jx.Float(erd.result))
			return res
		}
		res.result = j
		tl.append_errors(&res, erd.errs)
		return res
	}

	res.result = j
	d := jx.dump(j, allocator)
	tl.append_errorf(&res, "Couldn't convert bulk density class to raw density using function: %s!", d)
	return res
}

// C++: auto KA52clay = [](const Json&, const Json& j) -> EResult<Json>
@(private)
pattern_ka5_2_clay :: proc(
	root: jx.Value,
	j: jx.Value,
	allocator: Allocator,
) -> tl.EResult(jx.Value) {
	res: tl.EResult(jx.Value)
	res.allocator = allocator

	if len(jx.array_items(j)) == 2 && jx.is_string(jx.at(j, 1)) {
		ec := soil.ka5_texture_2_clay(jx.string_value_of(jx.at(j, 1)), allocator)
		if tl.success(ec.errs) {
			res.result = jx.Value(jx.Float(ec.result))
			return res
		}
		res.result = j
		tl.append_errors(&res, ec.errs)
		return res
	}

	res.result = j
	d := jx.dump(j, allocator)
	tl.append_errorf(&res, "Couldn't get soil clay content from KA5 soil class: %s!", d)
	return res
}

// C++: auto KA52sand = [](const Json&, const Json& j) -> EResult<Json>
@(private)
pattern_ka5_2_sand :: proc(
	root: jx.Value,
	j: jx.Value,
	allocator: Allocator,
) -> tl.EResult(jx.Value) {
	res: tl.EResult(jx.Value)
	res.allocator = allocator

	if len(jx.array_items(j)) == 2 && jx.is_string(jx.at(j, 1)) {
		es := soil.ka5_texture_2_sand(jx.string_value_of(jx.at(j, 1)), allocator)
		if tl.success(es.errs) {
			res.result = jx.Value(jx.Float(es.result))
			return res
		}
		res.result = j
		tl.append_errors(&res, es.errs)
		return res
	}

	res.result = j
	d := jx.dump(j, allocator)
	tl.append_errorf(&res, "Couldn't get soil sand content from KA5 soil class: %s!", d)
	return res
}

// C++: auto sandClay2lambda = [](const Json&, const Json& j) -> EResult<Json>
@(private)
pattern_sand_clay_2_lambda :: proc(
	root: jx.Value,
	j: jx.Value,
	allocator: Allocator,
) -> tl.EResult(jx.Value) {
	res: tl.EResult(jx.Value)
	res.allocator = allocator

	if len(jx.array_items(j)) == 3 && jx.is_number(jx.at(j, 1)) && jx.is_number(jx.at(j, 2)) {
		res.result = jx.Value(
			jx.Float(
				soil.sand_and_clay_2_lambda(
					jx.number_value(jx.at(j, 1)),
					jx.number_value(jx.at(j, 2)),
				),
			),
		)
		return res
	}

	res.result = j
	d := jx.dump(j, allocator)
	tl.append_errorf(&res, "Couldn't get lambda value from soil sand and clay content: %s!", d)
	return res
}

// C++: auto percent = [](const Json&, const Json& j) -> EResult<Json>
@(private)
pattern_percent :: proc(root: jx.Value, j: jx.Value, allocator: Allocator) -> tl.EResult(jx.Value) {
	res: tl.EResult(jx.Value)
	res.allocator = allocator

	if len(jx.array_items(j)) == 2 && jx.is_number(jx.at(j, 1)) {
		res.result = jx.Value(jx.Float(jx.number_value(jx.at(j, 1)) / 100.0))
		return res
	}

	res.result = j
	d := jx.dump(j, allocator)
	tl.append_errorf(&res, "Couldn't convert percent to decimal percent value: %s!", d)
	return res
}

// C++: const map<string, function<EResult<Json>(const Json&, const Json&)>>& supportedPatterns()
//
// The commented-out "include-from-db" entry is not ported.
@(private)
supported_patterns :: proc() -> map[string]Pattern_Proc {
	@(static) m: map[string]Pattern_Proc
	@(static) built: bool
	if !built {
		m["include-from-file"] = pattern_from_file
		m["ref"] = pattern_ref
		m["humus_st2corg"] = pattern_humus2corg
		m["humus-class->corg"] = pattern_humus2corg
		m["ld_eff2trd"] = pattern_bdc2rd
		m["bulk-density-class->raw-density"] = pattern_bdc2rd
		m["KA5TextureClass2clay"] = pattern_ka5_2_clay
		m["KA5-texture-class->clay"] = pattern_ka5_2_clay
		m["KA5TextureClass2sand"] = pattern_ka5_2_sand
		m["KA5-texture-class->sand"] = pattern_ka5_2_sand
		m["sandAndClay2lambda"] = pattern_sand_clay_2_lambda
		m["sand-and-clay->lambda"] = pattern_sand_clay_2_lambda
		m["%"] = pattern_percent
		built = true
	}
	return m
}

// Resets the module-level state between independent runs. No C++ counterpart -
// there the caches are function-local statics that live for the process.
//
// Assigns nil rather than clear()ing: the cached values (and the maps' own
// backing storage) were allocated with whatever allocator the previous run
// passed in. For monica-run that is the one process-wide arena and either would
// do, but a server hands each request its own arena and destroys it afterwards,
// so clear() would leave both maps pointing at freed memory for the next
// request to write into. Nothing leaks - the arena owned it and is already gone.
//
// The other module-level caches (soil tables, the output table, the legacy
// alias map) are NOT reset here: they hold no per-run data, so they are built
// from tools.process_cache_allocator instead and simply live for the process,
// exactly like their C++ statics.
reset_caches :: proc() {
	ref_cache = nil
	ref_cache_valid = nil
}

// ---------------------------------------------------------------------------
// the recursive walk
// ---------------------------------------------------------------------------

// C++: EResult<Json> monica::findAndReplaceReferences(const Json& root, const Json& j)
find_and_replace_references :: proc(
	root: jx.Value,
	j: jx.Value,
	allocator := context.allocator,
) -> tl.EResult(jx.Value) {
	sp := supported_patterns()

	res: tl.EResult(jx.Value)
	res.allocator = allocator

	if jx.is_array(j) && len(jx.array_items(j)) > 0 {
		array_is_reference_function := false

		if jx.is_string(jx.at(j, 0)) {
			if p, found := sp[jx.string_value_of(jx.at(j, 0))]; found {
				array_is_reference_function = true

				// check for nested function invocations in the arguments
				func_arr := make(jx.Array, 0, len(jx.array_items(j)), allocator)
				for item in jx.array_items(j) {
					r := find_and_replace_references(root, item, allocator)
					tl.append_errors(&res, r.errs)
					append(&func_arr, r.result)
				}

				// invoke function
				jaes := p(root, jx.Value(func_arr), allocator)
				tl.append_errors(&res, jaes.errs)

				// if successful try to recurse into the result for functions in it
				if tl.success(jaes.errs) {
					r := find_and_replace_references(root, jaes.result, allocator)
					tl.append_errors(&res, r.errs)
					res.result = r.result
					return res
				} else {
					// C++ returns an empty J11Object here, not the original value
					res.result = jx.Value(make(jx.Object, 0, allocator))
					return res
				}
			}
		}

		if !array_is_reference_function {
			arr := make(jx.Array, 0, len(jx.array_items(j)), allocator)
			for jv in jx.array_items(j) {
				r := find_and_replace_references(root, jv, allocator)
				tl.append_errors(&res, r.errs)
				append(&arr, r.result)
			}
			res.result = jx.Value(arr)
			return res
		}
		// unreachable: array_is_reference_function always returns above
		res.result = jx.Value(make(jx.Array, 0, allocator))
		return res
	} else if jx.is_object(j) {
		obj := make(jx.Object, 0, allocator)
		for k in jx.object_keys_sorted(j, context.temp_allocator) {
			r := find_and_replace_references(root, jx.get(j, k), allocator)
			tl.append_errors(&res, r.errs)
			obj[strings.clone(k, allocator)] = r.result
		}
		res.result = jx.Value(obj)
		return res
	}

	res.result = j
	return res
}
