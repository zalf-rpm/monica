package fbp

import "base:runtime"
import "core:fmt"

Allocator :: runtime.Allocator

// Disk locations of the raw .capnp files the shim parses at runtime. The C++
// links generated code and needs none of this; the dynamic API reads the schema
// files themselves, so a running component has to be able to find them.
//
// Deliberately separate from monica/capnp's own Schema_Paths: this package is a
// port of the mas_cpp_misc subset and must not depend on the monica packages
// (CONVENTIONS.md section 6).
Schema_Paths :: struct {
	root:        string, // zalfmas_capnp_schemas/ - import root for "/common/common.capnp" etc
	fbp:         string, // fbp/fbp.capnp                 - IP, Channel.Reader/Writer, PortInfos
	persistence: string, // persistence/persistence.capnp - Restorer, SturdyRef
	common:      string, // common/common.capnp           - StructuredText
}

make_schema_paths :: proc(root: string, allocator := context.allocator) -> Schema_Paths {
	return Schema_Paths {
		root = root,
		fbp = fmt.aprintf("%s/fbp/fbp.capnp", root, allocator = allocator),
		persistence = fmt.aprintf("%s/persistence/persistence.capnp", root, allocator = allocator),
		common = fmt.aprintf("%s/common/common.capnp", root, allocator = allocator),
	}
}
