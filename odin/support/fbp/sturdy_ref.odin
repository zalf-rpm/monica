// Port of the sturdy-ref-resolving subset of
// mas_cpp_misc/common/rpc-connection-manager.{h,cpp} - ConnectionManager::connect
// and the restoreSR/connectTo pair behind it.
//
// The C++ ConnectionManager also caches connections per host:port, retries
// (tryConnect/tryConnectB), resolves aliases through a HostPortResolver, and
// serves the local-server shortcut. None of that is ported: an FBP component
// connects to a handful of channels once, at startup, and the retry loop belongs
// with the flow runtime that starts it.
package fbp

import "core:strconv"
import "core:strings"
import capnp_dyn "../capnp/odin/capnp_dynamic"

// C++: the URL shape ConnectionManager::connect documents,
//   capnp://vat-id_base64-curve25519-public-key@host:port/sturdy-ref-token
// minus the parts nothing here reads. The vat id is skipped for the same reason
// the C++ skips it (it never reads url.userInfo outside the HostPortResolver
// case) - see monica/capnp/restorer.odin's header.
Sturdy_Ref :: struct {
	host_port: string,
	token:     string, // empty when the SR names a bootstrap capability directly
}

// C++: kj::Url::tryParse plus connect(kj::Url)'s own destructuring.
parse_sturdy_ref :: proc(sr: string, allocator := context.allocator) -> (ref: Sturdy_Ref, ok: bool) {
	rest := sr
	if strings.has_prefix(rest, "capnp://") {
		rest = rest[len("capnp://"):]
	}
	// Drop the query string (owner_guid / b_iid / sr_iid); none is supported here.
	if q := strings.index_byte(rest, '?'); q >= 0 {
		rest = rest[:q]
	}
	// Drop the vat id, if present.
	if at := strings.index_byte(rest, '@'); at >= 0 {
		rest = rest[at + 1:]
	}

	token := ""
	if slash := strings.index_byte(rest, '/'); slash >= 0 {
		token = rest[slash + 1:]
		rest = rest[:slash]
	}
	if rest == "" {
		return {}, false
	}
	// A host:port is required - the C++ gets a default port from kj's address
	// parser, which has no equivalent here, so demand it explicitly.
	colon := strings.last_index_byte(rest, ':')
	if colon < 0 {
		return {}, false
	}
	if _, port_ok := strconv.parse_int(rest[colon + 1:]); !port_ok {
		return {}, false
	}
	return Sturdy_Ref {
			host_port = strings.clone(rest, allocator),
			token = strings.clone(token, allocator),
		},
		true
}

// A live connection plus the capability the sturdy ref names. Both have to be
// released, and in this order: every capability is only valid while the
// connection it came from is (see capnp_dynamic.odin's lifetime note).
Sr_Connection :: struct {
	conn: capnp_dyn.Connection,
	cap:  capnp_dyn.Capability,
}

// C++: ConnectionManager::connect(url) -> connectTo -> restoreSR.
//
// With a token the connection's bootstrap is a Restorer and the named capability
// comes out of restore(); without one the bootstrap IS the capability. Either
// way the result is recast to interface_name, exactly as the C++ castAs<>()es
// whatever it gets.
connect_sturdy_ref :: proc(
	sr: string,
	schema: Schema_Paths,
	interface_schema_path: string, // which .capnp declares interface_name
	interface_name: string,
	allocator := context.allocator,
) -> (
	result: Sr_Connection,
	err: string,
	ok: bool,
) {
	ref, parsed := parse_sturdy_ref(sr, allocator)
	if !parsed {
		return {}, strings.concatenate({"not a usable sturdy ref: '", sr, "'"}, allocator), false
	}

	// The bootstrap interface has to be named when connecting, so which schema to
	// parse depends on whether a restore step is coming.
	bootstrap_schema := ref.token == "" ? interface_schema_path : schema.persistence
	bootstrap_iface := ref.token == "" ? interface_name : "Restorer"

	conn, conn_err, conn_ok := capnp_dyn.connect(
		ref.host_port,
		bootstrap_schema,
		schema.root,
		bootstrap_iface,
	)
	if !conn_ok {
		return {}, conn_err, false
	}

	bootstrap, bs_err, bs_ok := capnp_dyn.bootstrap(conn)
	if !bs_ok {
		capnp_dyn.disconnect(conn)
		return {}, bs_err, false
	}

	if ref.token == "" {
		return Sr_Connection{conn = conn, cap = bootstrap}, "", true
	}

	// C++: restorerClient.restoreRequest(); req.initLocalRef().setText(srToken);
	params := []capnp_dyn.Field {
		{name = "localRef", value = []capnp_dyn.Field{{name = "text", value = ref.token}}},
	}
	res, call_err, call_ok := capnp_dyn.call(bootstrap, "restore", params)
	if !call_ok {
		capnp_dyn.release_capability(bootstrap)
		capnp_dyn.disconnect(conn)
		return {}, call_err, false
	}

	cap_value, has_cap := capnp_dyn.field_get(res, "cap")
	restored, is_cap := cap_value.(capnp_dyn.Capability)
	capnp_dyn.release_capability(bootstrap)
	if !has_cap || !is_cap || restored == nil {
		// The Restorer leaves the field unset for an unknown token - see
		// monica/capnp/restorer.odin.
		capnp_dyn.disconnect(conn)
		return {}, strings.concatenate({"sturdy ref token was not restored: '", ref.token, "'"}, allocator), false
	}

	// restore returns the generic `Capability` type, so it has no callable
	// methods until recast - purely local, no round trip.
	typed, as_err, as_ok := capnp_dyn.capability_as(restored, interface_schema_path, schema.root, interface_name)
	capnp_dyn.release_capability(restored)
	if !as_ok {
		capnp_dyn.disconnect(conn)
		return {}, as_err, false
	}
	return Sr_Connection{conn = conn, cap = typed}, "", true
}

sr_connection_close :: proc(c: Sr_Connection) {
	if c.cap != nil {
		capnp_dyn.release_capability(c.cap)
	}
	if c.conn != nil {
		capnp_dyn.disconnect(c.conn)
	}
}

