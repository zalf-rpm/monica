// Port of mas_cpp_misc/common/PortConnector.{h,cpp} - resolving an FBP
// component's in/out ports to Channel.Reader / Channel.Writer capabilities,
// either from individual sturdy refs or from a PortInfos message read off a
// channel.
//
// Ports are keyed by name rather than by the C++'s (int id, name) pair of maps:
// the ids exist there only to index std::map<int, StringPtr>, and every lookup
// immediately translates one to the other.
//
// ARRAY_OUT is not ported. PortConnector supports it (outArrayPortCaps), but
// neither MONICA FBP component declares an array port, and an untested code path
// for a shape nothing here produces is worse than not having it.
package fbp

import "core:fmt"
import "core:strings"
import capnp_dyn "../capnp/odin/capnp_dynamic"

// C++: PortConnector::PortType, minus ARRAY_OUT (see the file header).
Port_Type :: enum {
	In,
	Out,
}

// fbp.capnp: Channel(V).Reader / Channel(V).Writer. Nested declarations, which
// the shim can only name since it learned to resolve dot-separated paths.
CHANNEL_READER :: "Channel.Reader"
CHANNEL_WRITER :: "Channel.Writer"

// C++: PortConnector (its Impl's inPortCaps/outPortCaps/...Connected maps)
Port_Connector :: struct {
	schema: Schema_Paths,
	in_:    map[string]Sr_Connection,
	out:    map[string]Sr_Connection,
}

make_port_connector :: proc(schema: Schema_Paths, allocator := context.allocator) -> Port_Connector {
	return Port_Connector {
		schema = schema,
		in_ = make(map[string]Sr_Connection, allocator),
		out = make(map[string]Sr_Connection, allocator),
	}
}

// C++: void PortConnector::connectToSrStr(portName, srStr, portType)
port_connector_connect_sr :: proc(
	pc: ^Port_Connector,
	port_name: string,
	sr: string,
	port_type: Port_Type,
	allocator := context.allocator,
) -> (
	err: string,
	ok: bool,
) {
	iface := port_type == .In ? CHANNEL_READER : CHANNEL_WRITER
	c, connect_err, connect_ok := connect_sturdy_ref(sr, pc.schema, pc.schema.fbp, iface, allocator)
	if !connect_ok {
		return connect_err, false
	}
	key := strings.clone(port_name, allocator)
	switch port_type {
	case .In:
		pc.in_[key] = c
	case .Out:
		pc.out[key] = c
	}
	return "", true
}

// C++: void PortConnector::connectFromPortInfos(kj::StringPtr portInfosReaderSR)
//
// The sturdy ref names a Channel(PortInfos).Reader; one read yields the
// PortInfos message naming every port. Ports the component does not declare are
// ignored, as in the C++ (its inPortName2Id.find simply misses).
port_connector_connect_from_port_infos :: proc(
	pc: ^Port_Connector,
	port_infos_reader_sr: string,
	in_port_names: []string,
	out_port_names: []string,
	allocator := context.allocator,
) -> (
	err: string,
	ok: bool,
) {
	c, connect_err, connect_ok := connect_sturdy_ref(
		port_infos_reader_sr,
		pc.schema,
		pc.schema.fbp,
		CHANNEL_READER,
		allocator,
	)
	if !connect_ok {
		return connect_err, false
	}
	defer sr_connection_close(c)

	msg, read_err, read_ok := capnp_dyn.call(c.cap, "read", nil)
	if !read_ok {
		return read_err, false
	}
	// C++: if (msg.isDone()) return;
	if _, done := capnp_dyn.field_get(msg, "done"); done {
		return "", true
	}
	value, has_value := capnp_dyn.field_get(msg, "value")
	if !has_value {
		return "port infos channel delivered no value", false
	}
	// Channel(V) is generic, so V degrades to AnyPointer - reinterpret it as the
	// PortInfos the sturdy ref promised.
	ap, is_ap := value.(capnp_dyn.Any_Pointer)
	if !is_ap {
		return "port infos message is not an AnyPointer", false
	}
	infos, as_err, as_ok := capnp_dyn.any_pointer_as_struct(ap, pc.schema.fbp, pc.schema.root, "PortInfos")
	if !as_ok {
		return as_err, false
	}

	connect_listed(pc, infos, "inPorts", in_port_names, .In, allocator)
	connect_listed(pc, infos, "outPorts", out_port_names, .Out, allocator)
	return "", true
}

// C++: the two near-identical loops over msg.getValue().getInPorts()/getOutPorts()
@(private)
connect_listed :: proc(
	pc: ^Port_Connector,
	infos: []capnp_dyn.Field,
	field_name: string,
	wanted: []string,
	port_type: Port_Type,
	allocator: Allocator,
) {
	v, has := capnp_dyn.field_get(infos, field_name)
	list, is_list := v.([]capnp_dyn.Value)
	if !has || !is_list {
		return
	}
	for entry_value in list {
		entry, is_struct := entry_value.([]capnp_dyn.Field)
		if !is_struct {
			continue
		}
		name_value, has_name := capnp_dyn.field_get(entry, "name")
		name, is_text := name_value.(string)
		if !has_name || !is_text || !contains_string(wanted, name) {
			continue
		}
		// NameAndSR is a union of `sr` (single port) and `srs` (array port); only
		// the active arm comes through. Array ports are not supported here (see
		// the file header), so only `sr` is looked at.
		sr_value, has_sr := capnp_dyn.field_get(entry, "sr")
		if !has_sr {
			continue
		}
		sr_fields, sr_is_struct := sr_value.([]capnp_dyn.Field)
		if !sr_is_struct {
			continue
		}
		sr_str, sr_ok := sturdy_ref_struct_to_string(sr_fields, allocator)
		if !sr_ok {
			continue
		}
		if err, ok := port_connector_connect_sr(pc, name, sr_str, port_type, allocator); !ok {
			fmt.eprintfln("could not connect %v port '%s': %s", port_type, name, err)
		}
	}
}

// C++: ConnectionManager::connect(SturdyRef::Reader) - builds the same
// capnp://host:port/token URL out of a SturdyRef struct that the string form
// spells directly, so both paths converge on connect_sturdy_ref.
@(private)
sturdy_ref_struct_to_string :: proc(
	sr: []capnp_dyn.Field,
	allocator: Allocator,
) -> (
	result: string,
	ok: bool,
) {
	vat, has_vat := capnp_dyn.field_get(sr, "vat")
	vat_fields, vat_is_struct := vat.([]capnp_dyn.Field)
	if !has_vat || !vat_is_struct {
		return "", false
	}
	addr, has_addr := capnp_dyn.field_get(vat_fields, "address")
	addr_fields, addr_is_struct := addr.([]capnp_dyn.Field)
	if !has_addr || !addr_is_struct {
		return "", false
	}
	host_value, has_host := capnp_dyn.field_get(addr_fields, "host")
	host, host_is_text := host_value.(string)
	if !has_host || !host_is_text {
		return "", false
	}
	port := u64(0)
	if p, has_port := capnp_dyn.field_get(addr_fields, "port"); has_port {
		port, _ = p.(u64)
	}

	token := ""
	if local_ref, has_local_ref := capnp_dyn.field_get(sr, "localRef"); has_local_ref {
		if lr, lr_is_struct := local_ref.([]capnp_dyn.Field); lr_is_struct {
			if t, has_text := capnp_dyn.field_get(lr, "text"); has_text {
				token, _ = t.(string)
			}
		}
	}

	if token == "" {
		return fmt.aprintf("capnp://%s:%d", host, port, allocator = allocator), true
	}
	return fmt.aprintf("capnp://%s:%d/%s", host, port, token, allocator = allocator), true
}

// C++: bool PortConnector::isInConnected(int) / isOutConnected(int)
port_connector_in_connected :: proc(pc: ^Port_Connector, port_name: string) -> bool {
	return port_name in pc.in_
}

port_connector_out_connected :: proc(pc: ^Port_Connector, port_name: string) -> bool {
	return port_name in pc.out
}

// C++: PortConnector::in(int) / out(int)
port_connector_in :: proc(pc: ^Port_Connector, port_name: string) -> capnp_dyn.Capability {
	if c, found := pc.in_[port_name]; found {
		return c.cap
	}
	return nil
}

port_connector_out :: proc(pc: ^Port_Connector, port_name: string) -> capnp_dyn.Capability {
	if c, found := pc.out[port_name]; found {
		return c.cap
	}
	return nil
}

// C++: void PortConnector::closeOutPorts()
//
// Closing the writing end is what tells a channel with fbp close semantics that
// no more data is coming, so downstream components see `done` rather than
// blocking forever. Errors are ignored: the peer may already be gone, which is
// not a failure of this component.
port_connector_close_out_ports :: proc(pc: ^Port_Connector) {
	for _, c in pc.out {
		_, _, _ = capnp_dyn.call(c.cap, "close", nil)
	}
}

// Releases every connection. Not a C++ counterpart - there the ConnectionManager
// owns them and dies with the process.
port_connector_close :: proc(pc: ^Port_Connector) {
	for _, c in pc.in_ {
		sr_connection_close(c)
	}
	for _, c in pc.out {
		sr_connection_close(c)
	}
}

@(private)
contains_string :: proc(haystack: []string, needle: string) -> bool {
	for s in haystack {
		if s == needle {
			return true
		}
	}
	return false
}
