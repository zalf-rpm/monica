// Port of the file/path helpers from mas_cpp_misc/tools/helper.h + helper.cpp.
package tools

import "core:os"
import "core:strings"

PATH_SEPARATOR :: "\\" when ODIN_OS == .Windows else "/"

// C++: EResult<string> Tools::readFile(string path)
//
// NOTE(c++-quirk): the C++ reads the file line by line and concatenates the
// lines WITHOUT re-inserting newlines (`res.result += line;`). For JSON that is
// harmless, but it is observable for anything line-oriented, so it is
// reproduced exactly here. Do not "fix" this - use os.read_entire_file directly
// if you want the raw bytes.
read_file :: proc(path: string, allocator := context.allocator) -> EResult(string) {
	res: EResult(string)
	res.allocator = allocator

	fixed := fix_system_separator(path, allocator)
	defer delete(fixed, allocator)

	data, rerr := os.read_entire_file(fixed, allocator)
	if rerr != nil {
		append_errorf(&res, "Couldn't open file: '%s'", fixed)
		res.result = strings.clone("", allocator)
		return res
	}
	defer delete(data, allocator)

	// strip \n and \r, matching getline() + concatenation
	b := strings.builder_make(allocator)
	for c in data {
		if c != '\n' && c != '\r' {
			strings.write_byte(&b, c)
		}
	}
	res.result = strings.clone(strings.to_string(b), allocator)
	strings.builder_destroy(&b)
	return res
}

// C++: pair<string,string> Tools::splitPathToFile(const string& pathToFile)
//
// Returns (path-including-trailing-separator, filename).
split_path_to_file :: proc(
	path_to_file: string,
	allocator := context.allocator,
) -> (
	path: string,
	filename: string,
) {
	found := -1
	for i := len(path_to_file) - 1; i >= 0; i -= 1 {
		if path_to_file[i] == '/' || path_to_file[i] == '\\' {
			found = i
			break
		}
	}
	if found == -1 {
		return strings.clone("", allocator), strings.clone(path_to_file, allocator)
	}
	return strings.clone(path_to_file[:found + 1], allocator),
		strings.clone(path_to_file[found + 1:], allocator)
}

// C++: bool Tools::isAbsolutePath(const std::string& path)
//
// NOTE(c++-quirk): the C++ indexes path.at(2) unguarded, which throws for a
// 2-character path like "C:". Guarded here (returns false) - the C++ would have
// terminated, so no correct program can depend on the difference.
is_absolute_path :: proc(path: string) -> bool {
	found := -1
	for i in 0 ..< len(path) {
		if path[i] == '/' || path[i] == ':' {
			found = i
			break
		}
	}
	if found == 0 && len(path) > 0 && path[0] == '/' {
		return true
	} else if found == 1 &&
	   len(path) > 2 &&
	   path[1] == ':' &&
	   (path[2] == '\\' || path[2] == '/') {
		return true
	}
	return false
}

// C++: string Tools::fixSystemSeparator(string path)
//
// On Windows: '/' -> '\', then collapse '\\' -> '\'.
// Elsewhere: collapse '//' -> '/'.
fix_system_separator :: proc(path: string, allocator := context.allocator) -> string {
	when ODIN_OS == .Windows {
		// strings.replace_all returns the input `path` itself, unallocated, when
		// there's no "/" to replace (e.g. path == "."). Only delete step1 when it
		// was actually a fresh allocation - otherwise this frees the caller's own
		// `path` string out from under it, a real use-after-free this port hit in
		// the phase 7 checkpoint 5 CLI (ensure_dir_exists -> fix_system_separator
		// on a "." directory, corrupting the caller's still-live path variable).
		step1, was_allocation := strings.replace_all(path, "/", "\\", allocator)
		defer if was_allocation {
			delete(step1, allocator)
		}
		out := collapse_doubles(step1, "\\\\", "\\", allocator)
		return out
	} else {
		return collapse_doubles(path, "//", "/", allocator)
	}
}

// The C++ loops `pos = path.find(needle, pos + 1)` after each replacement, which
// does NOT rescan from the start; a run of N separators therefore collapses
// pairwise left to right rather than fully. Reproduced.
@(private)
collapse_doubles :: proc(
	s: string,
	needle, repl: string,
	allocator := context.allocator,
) -> string {
	b := strings.builder_make(allocator)
	i := 0
	for i < len(s) {
		if i + len(needle) <= len(s) && s[i:i + len(needle)] == needle {
			strings.write_string(&b, repl)
			i += len(needle)
		} else {
			strings.write_byte(&b, s[i])
			i += 1
		}
	}
	out := strings.clone(strings.to_string(b), allocator)
	strings.builder_destroy(&b)
	return out
}

// C++: bool Tools::directoryExist(const std::string& path)
directory_exist :: proc(path: string, allocator := context.allocator) -> bool {
	fullpath := fix_system_separator(path, allocator)
	defer delete(fullpath, allocator)
	return os.is_dir(fullpath)
}

// C++: bool Tools::ensureDirExists(const string& path)
//
// Creates the directory, recursively creating parents as needed.
ensure_dir_exists :: proc(path: string, allocator := context.allocator) -> bool {
	fullpath := fix_system_separator(path, allocator)
	defer delete(fullpath, allocator)

	if len(fullpath) == 0 {
		return false
	}
	if os.is_dir(fullpath) {
		return true
	}

	trimmed := rim_right(fullpath, "/\\", allocator)
	defer delete(trimmed, allocator)
	if len(trimmed) == 0 {
		return false
	}

	pos := -1
	for i := len(trimmed) - 1; i >= 0; i -= 1 {
		if trimmed[i] == '/' || trimmed[i] == '\\' {
			pos = i
			break
		}
	}
	if pos > 0 {
		if !ensure_dir_exists(trimmed[:pos], allocator) {
			return false
		}
	}
	return os.make_directory(trimmed) == nil || os.is_dir(trimmed)
}

// C++: std::string Tools::replaceEnvVars(std::string path)
//
// Replaces every ${NAME} with the environment variable's value. An unset
// variable is left in place and the scan continues after it.
replace_env_vars :: proc(path: string, allocator := context.allocator) -> string {
	START_TOKEN :: "${"
	END_TOKEN :: "}"

	cur := strings.clone(path, allocator)
	start_pos := strings.index(cur, START_TOKEN)
	for start_pos != -1 {
		rel := strings.index(cur[start_pos + 1:], END_TOKEN)
		if rel == -1 {
			break
		}
		end_pos := start_pos + 1 + rel

		name_start := start_pos + 2
		env_var_name := cur[name_start:end_pos]
		content, found := os.lookup_env(env_var_name, allocator)
		if found {
			next := strings.concatenate({cur[:start_pos], content, cur[end_pos + 1:]}, allocator)
			delete(content, allocator)
			delete(cur, allocator)
			cur = next
			start_pos = strings.index(cur, START_TOKEN)
		} else {
			rel2 := strings.index(cur[end_pos + 1:], START_TOKEN)
			start_pos = rel2 == -1 ? -1 : end_pos + 1 + rel2
		}
	}
	return cur
}
