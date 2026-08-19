// Port of src/run/serve-monica-zmq.{h,cpp} (serveZmqMonicaFull), plus the
// Msg struct and receiveMsg/s_send/s_sendmore helpers from
// mas_cpp_misc/zeromq/zmq-helper.{h,cpp} that it depends on.
//
// Dropped, per plan-odin.md's "Explicitly dropped" table and run_monica.odin's
// own file header: Cap'n Proto sturdy-ref support (INCLUDE_SR_SUPPORT - the
// kj::setupAsyncIo/ConnectionManager, soil-profile and climate-timeseries
// sturdy-ref branches) and Intercropping (isIC, the second MonicaModel/out2
// half, the "1"/"2" object reply). The debug() trace calls are dropped too -
// no debug-trace facility is ported (see monica-run's main.odin); the
// env.debugMode assignment itself is kept for structural parity even though
// nothing downstream reads Env.debugMode.
//
// env.climateCSV/pathsToClimateCSV/csvViaHeaderOptions were dropped from the
// Odin Env struct (run_monica.odin) because monica-run's CLI path always
// resolves climate data into env["climateData"] before env_merge runs. The
// ZMQ server doesn't have that luxury: real producers (see
// installer/Hohenfinow2/python/run-producer.py) send pathToClimateCSV and
// csvViaHeaderOptions and expect the server to read the CSV itself. So this
// file reads those three keys directly off the incoming jx.Value message
// rather than threading them through Env, mirroring what run-monica.cpp's
// env_merge used to do with them.
//
// zmq::error_t exceptions (thrown by cppzmq on every failed bind/connect/
// send/recv) have no Odin analogue; the vendored bindings return C rc codes
// instead, so the C++'s nested try/catch cascade is flattened into sequential
// rc checks with early returns/continues, preserving which failures are fatal
// to serve_zmq_monica_full (receive/send/control socket bind|connect) versus
// which are merely logged and retried (a single message's recv/send).
package run

import "core:c"
import "core:fmt"
import "core:mem"
import mio "../io"
import clim "../../support/climate"
import jx "../../support/jsonx"
import tl "../../support/tools"
import zmq "../../support/zeromq"
import "core:strings"

// ---------------------------------------------------------------------------
// Msg / zmq-helper - port of mas_cpp_misc/zeromq/zmq-helper.{h,cpp}
// ---------------------------------------------------------------------------

// C++: struct Tools::Msg
Msg :: struct {
	json:  jx.Value,
	err:   string,
	topic: string,
	msg:   string,
	valid: bool,
}

// C++: string Msg::type() const
msg_type :: proc(m: ^Msg) -> string {
	return jx.string_value_of(jx.get(m.json, "type"))
}

// C++: Msg Tools::receiveMsg(zmq::socket_t&, int, bool)
receive_msg :: proc(
	socket: ^zmq.Socket,
	topic_char_count: int = 0,
	non_blocking_mode: bool = false,
	allocator := context.allocator,
) -> Msg {
	zmsg: zmq.Message
	rc := zmq.msg_init(&zmsg)
	assert(rc == 0)
	defer zmq.msg_close(&zmsg)

	flags := zmq.Send_Recv_Options.None
	if non_blocking_mode {
		flags = .DONTWAIT
	}
	size := zmq.msg_recv(&zmsg, socket, flags)
	if size == -1 {
		return Msg{valid = false}
	}

	raw := make([]byte, int(size), allocator)
	mem.copy(raw_data(raw), zmq.msg_data(&zmsg), int(size))
	str_msg := string(raw)

	topic := ""
	if topic_char_count > 0 && topic_char_count <= len(str_msg) {
		topic = str_msg[:topic_char_count]
		str_msg = str_msg[topic_char_count:]
	}

	pr := jx.parse_json_string(str_msg, allocator)
	if tl.success(pr.errs) {
		return Msg{json = pr.result, topic = topic, valid = true}
	}

	err := ""
	if len(pr.errs.errors) > 0 {
		err = pr.errs.errors[0]
	}
	return Msg{json = jx.Value{}, err = err, topic = topic, msg = str_msg, valid = true}
}

// C++: bool Tools::s_send(zmq::socket_t&, const std::string&)
s_send :: proc(socket: ^zmq.Socket, s: string) -> bool {
	return zmq.send_string(socket, s)
}

// C++: bool Tools::s_sendmore(zmq::socket_t&, const std::string&)
s_sendmore :: proc(socket: ^zmq.Socket, s: string) -> bool {
	return zmq.send_string_more(socket, s)
}

// ---------------------------------------------------------------------------
// socket configuration - port of serve-monica-zmq.h
// ---------------------------------------------------------------------------

// C++: enum monica::SocketType
Socket_Kind :: enum {
	Reply,
	ProxyReply,
	Pull,
	Push,
	Subscribe,
	Router,
	Dealer,
}

// C++: enum monica::SocketRole
Socket_Role :: enum {
	ReceiveJob,
	SendResult,
	Control,
}

// C++: enum monica::SocketOp
Socket_Op :: enum {
	Bind,
	Connect,
}

// C++: struct monica::SocketConfig
Socket_Config :: struct {
	type:      Socket_Kind,
	addresses: [dynamic]string,
	op:        Socket_Op,
}

@(private)
addresses_equal :: proc(a, b: []string) -> bool {
	if len(a) != len(b) {
		return false
	}
	for i in 0 ..< len(a) {
		if a[i] != b[i] {
			return false
		}
	}
	return true
}

@(private)
bind_or_connect_all :: proc(socket: ^zmq.Socket, addresses: []string, op: Socket_Op) -> bool {
	for addr in addresses {
		caddr := strings.clone_to_cstring(addr, context.temp_allocator)
		rc: c.int = op == .Connect ? zmq.connect(socket, caddr) : zmq.bind(socket, caddr)
		if rc != 0 {
			return false
		}
	}
	return true
}

// C++: (inline, run-monica.cpp:131-139) the pathToClimateCSV -> pathsToClimateCSV
// extraction that used to live in Env::merge.
@(private)
extract_paths_to_climate_csv :: proc(j: jx.Value, allocator := context.allocator) -> [dynamic]string {
	paths := make([dynamic]string, 0, allocator)
	v := jx.get(j, "pathToClimateCSV")
	if jx.is_string(v) {
		s := jx.string_value_of(v)
		if s != "" {
			append(&paths, s)
		}
	} else if jx.is_array(v) {
		for item in jx.array_items(v) {
			s := jx.string_value_of(item)
			if s != "" {
				append(&paths, s)
			}
		}
	}
	return paths
}

// C++: the "Env" branch of serveZmqMonicaFull's message loop
// (serve-monica-zmq.cpp:154-260), Intercropping/out2 already dropped.
@(private)
handle_env_message :: proc(
	msg_json: jx.Value,
	path_to_soil_dir: string,
	started_server_in_debug_mode: bool,
	allocator := context.allocator,
) -> mio.Output {
	out: mio.Output
	custom_id := jx.get(msg_json, "customId")
	out.customId = custom_id
	is_no_data_pass_through := jx.is_object(custom_id) && jx.bool_value_of(jx.get(custom_id, "nodata"))

	if is_no_data_pass_through {
		return out
	}

	env: Env
	merge_errs := env_merge(&env, msg_json, path_to_soil_dir, allocator)

	if !tl.success(merge_errs) {
		out.errors = merge_errs.errors
		out.warnings = merge_errs.warnings
		return out
	}

	// C++: EResult<DataAccessor> eda; - default-constructed, i.e. success()
	// stays true unless a climate read is actually attempted and fails.
	eda: tl.EResult(clim.Data_Accessor)
	eda.allocator = allocator

	if !clim.data_accessor_is_valid(&env.climateData) {
		csv_opts := clim.make_csv_via_header_options()
		_ = clim.csv_via_header_options_merge(&csv_opts, jx.get(msg_json, "csvViaHeaderOptions"), allocator)

		climate_csv := jx.string_value_of(jx.get(msg_json, "climateCSV"))
		if climate_csv != "" {
			eda = clim.read_climate_data_from_csv_string_via_headers(climate_csv, csv_opts, allocator)
		} else {
			paths := extract_paths_to_climate_csv(msg_json, allocator)
			if len(paths) > 0 {
				eda = clim.read_climate_data_from_csv_files_via_headers(paths[:], csv_opts, allocator)
			}
		}
	}

	if tl.success(eda.errs) {
		if !clim.data_accessor_is_valid(&env.climateData) {
			env.climateData = eda.result
		}
		env.debugMode = started_server_in_debug_mode && env.debugMode
		out = run_monica(&env, allocator)
		out.customId = custom_id
	}
	out.errors = eda.errs.errors
	out.warnings = eda.errs.warnings

	return out
}

// C++: void monica::serveZmqMonicaFull(zmq::context_t*, map<SocketRole, SocketConfig>)
serve_zmq_monica_full :: proc(
	zmq_context: ^zmq.Context,
	socket_addresses: map[Socket_Role]Socket_Config,
	activate_debug: bool,
) {
	started_server_in_debug_mode := activate_debug

	if len(socket_addresses) == 0 {
		fmt.eprintln("No supplied address for a receiving zmq socket! Exiting.")
		return
	}

	path_to_soil_dir := tl.fix_system_separator(
		tl.replace_env_vars("${MONICA_PARAMETERS}/soil/", context.allocator),
		context.allocator,
	)

	rconfig: Socket_Config
	if cfg, ok := socket_addresses[.ReceiveJob]; ok {
		rconfig = cfg
	}
	r_addresses := rconfig.addresses[:]
	receive_socket_type := zmq.Socket_Type.REP
	if rconfig.type == .Pull {
		receive_socket_type = .PULL
	}
	socket := zmq.socket(zmq_context, receive_socket_type)

	if !bind_or_connect_all(socket, r_addresses, rconfig.op) {
		fmt.eprintf(
			"Couldn't %s zmq socket to address: %s! Error: %s\n",
			rconfig.op == .Bind ? "bind" : "connect",
			strings.join(r_addresses, ",", context.temp_allocator),
			zmq.zmq_error_cstring(),
		)
		return
	}

	s_addresses := r_addresses
	sconfig: Socket_Config
	if cfg, ok := socket_addresses[.SendResult]; ok {
		sconfig = cfg
		s_addresses = sconfig.addresses[:]
	}
	send_socket_type := zmq.Socket_Type.PUSH
	if sconfig.type == .Router {
		send_socket_type = .ROUTER
	}
	send_socket := zmq.socket(zmq_context, send_socket_type)
	distinct_send_socket := !addresses_equal(s_addresses, r_addresses)

	c_addresses := r_addresses
	cconfig: Socket_Config
	if cfg, ok := socket_addresses[.Control]; ok {
		cconfig = cfg
		c_addresses = cconfig.addresses[:]
	}
	control_socket := zmq.socket(zmq_context, .SUB)
	distinct_control_socket := !addresses_equal(c_addresses, r_addresses)

	if distinct_send_socket {
		if !bind_or_connect_all(send_socket, s_addresses, sconfig.op) {
			fmt.eprintf(
				"Couldn't %s zmq push socket to address: %s! Error: %s\n",
				sconfig.op == .Bind ? "bind" : "connect",
				strings.join(s_addresses, ",", context.temp_allocator),
				zmq.zmq_error_cstring(),
			)
			return
		}
	}

	topic_char_count := 0
	if distinct_control_socket {
		topic := "finish"
		topic_char_count = len(topic)
		if !bind_or_connect_all(control_socket, c_addresses, cconfig.op) {
			fmt.eprintf(
				"Couldn't %s zmq subscribe socket to address: %s! Error: %s\n",
				cconfig.op == .Bind ? "bind" : "connect",
				strings.join(c_addresses, ",", context.temp_allocator),
				zmq.zmq_error_cstring(),
			)
			return
		}
		zmq.setsockopt_string(control_socket, .SUBSCRIBE, topic)
	}

	for {
		poll_items := [2]zmq.Poll_Item {
			{socket = socket, events = i16(zmq.Poll_Event.POLLIN)},
			{socket = control_socket, events = i16(zmq.Poll_Event.POLLIN)},
		}
		n_items: c.int = distinct_control_socket ? 2 : 1
		prc := zmq.poll(&poll_items[0], n_items, -1)
		if prc == -1 {
			fmt.eprintf(
				"Exception on trying to receive request message on zmq socket with address: %s! Will continue to receive requests! Error: [%s]\n",
				strings.join(r_addresses, ",", context.temp_allocator),
				zmq.zmq_error_cstring(),
			)
			continue
		}

		msg_arena: jx.Arena
		jx.arena_init(&msg_arena)
		defer jx.arena_destroy(&msg_arena)
		msg_allocator := jx.arena_allocator(&msg_arena)

		msg: Msg
		if int(poll_items[0].revents) & int(zmq.Poll_Event.POLLIN) != 0 {
			msg = receive_msg(socket, 0, false, msg_allocator)
		}
		if distinct_control_socket && int(poll_items[1].revents) & int(zmq.Poll_Event.POLLIN) != 0 {
			msg = receive_msg(control_socket, topic_char_count, false, msg_allocator)
		}

		msg_t := msg_type(&msg)
		if msg_t == "finish" {
			if rconfig.type != .Pull {
				result_msg := jx.obj(msg_allocator, {"type", jx.sl("ack")})
				reply_socket := distinct_send_socket ? send_socket : socket
				if !s_send(reply_socket, jx.dump(result_msg, msg_allocator)) {
					fmt.eprintf(
						"Exception on trying to reply to 'finish' request with 'ack' message on zmq socket with address(es): %s! Still will finish MONICA process!\n",
						strings.join(s_addresses, ",", context.temp_allocator),
					)
				}
			}
			zmq.setsockopt_int(send_socket, .LINGER, 0)
			zmq.close(send_socket)

			zmq.setsockopt_int(control_socket, .LINGER, 0)
			zmq.close(control_socket)

			zmq.setsockopt_int(socket, .LINGER, 0)
			zmq.close(socket)

			break
		} else if msg_t == "Env" {
			shared_id := jx.string_value_of(jx.get(msg.json, "sharedId"))
			out := handle_env_message(msg.json, path_to_soil_dir, started_server_in_debug_mode, msg_allocator)

			reply_socket := distinct_send_socket ? send_socket : socket
			ok := true
			if shared_id != "" {
				ok = s_sendmore(reply_socket, shared_id)
			}
			if ok {
				ok = s_send(reply_socket, jx.dump(mio.output_to_json(&out, msg_allocator), msg_allocator))
			}
			if !ok {
				fmt.eprintf(
					"Exception on trying to reply with result message on zmq socket with address: %s! Will continue to receive requests!\n",
					strings.join(s_addresses, ",", context.temp_allocator),
				)
			}
		} else {
			result_msg := jx.obj(msg_allocator, {"type", jx.sl("error")})
			reply_socket := distinct_send_socket ? send_socket : socket
			if !s_send(reply_socket, jx.dump(result_msg, msg_allocator)) {
				fmt.eprintf(
					"Exception on trying to reply to '%s' request with 'error' message on zmq socket with address: %s! Still will finish MONICA process!\n",
					msg_t,
					strings.join(s_addresses, ",", context.temp_allocator),
				)
			}
		}
	}
}
