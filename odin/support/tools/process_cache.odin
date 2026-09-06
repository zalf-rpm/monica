package tools

import "base:runtime"

// Allocator for module-level caches that stand in for the C++'s function-local
// `static` variables - the parameter tables in monica/soil/soil_tables.odin,
// monica/io/build_output.odin and monica/io/output_paths.odin.
//
// Those caches are built once and then read for the rest of the process, so
// they must NOT come out of whatever allocator the first caller happened to
// pass. monica-run gets away with it (one arena for the whole process), but a
// server does not: with a per-request arena, request 1 populates the cache from
// its arena, the arena is destroyed at the end of the request, and request 2
// reads freed memory. That is a silent access violation that takes the whole
// server down - and it did, for both monica-zmq-server and monica-capnp-server,
// on their SECOND request, until this was introduced.
//
// The heap here is deliberate and matches the C++ exactly: these caches are
// never freed, because a process-lifetime static never is. CONVENTIONS.md §3's
// "config JSON lives in an arena and is never individually freed" is about the
// per-run data, not about these.
process_cache_allocator :: proc() -> runtime.Allocator {
	return runtime.heap_allocator()
}
