// Port of src/run/monica-zmq-proxy-main.cpp - the monica-zmq-proxy CLI: binds
// a frontend and a backend socket and runs zmq_proxy between them. The Docker
// image's supervisord.conf runs two of these (jobs in, results out) in front
// of the monica-zmq-server workers.
//
// NOTE(c++-quirk): -c/--start-control-node is parsed but not acted on. The C++
// shells out to a `monica-zmq-control` binary via system(), which neither the
// C++ CMake build nor this port produces; a warning is printed instead.
// debug() lines are dropped, matching monica-zmq-server's main.odin.
package main

import "core:c"
import "core:fmt"
import "core:os"
import "core:strings"
import tl "../../support/tools"
import zmq "../../support/zeromq"

APP_NAME :: "monica-zmq-proxy"
VERSION :: "3.6.59.0"

// C++: monica-zmq-defaults.h
DEF_PROXY_FRONTEND_PORT :: 5555
DEF_PROXY_BACKEND_PORT :: 5566
DEF_CONTROL_PORT :: 8888

// C++: auto printHelp = [=]()
print_help :: proc(frontend_port, backend_port, control_port: int) {
	fmt.printfln("%s [options] ", APP_NAME)
	fmt.println()
	fmt.println("options:")
	fmt.println()
	fmt.println(" -h | --help ... this help output")
	fmt.printfln(" -v | --version ... outputs %s version", APP_NAME)
	fmt.println()
	fmt.println(" -p | --pipeline-ports (use PULL/PUSH sockets for frontend/backend) ... deprecated")
	fmt.println(" -pps | --pull-push-sockets (use PULL/PUSH sockets for frontend/backend)")
	fmt.println(" -prs | --pull-router-sockets (use PULL/ROUTER sockets for frontend/backend)")
	fmt.println(
		" -fst | --frontend-socket-type FRONTEND_SOCKET_TYPE (default: ROUTER) ... use given frontend socket type",
	)
	fmt.println(
		" -bst | --backend-type BACKEND_SOCKET_TYPE (default: DEALER) ... use given backend socket type",
	)
	fmt.printfln(
		" -f | --frontend-port FRONTEND-PORT (default: %d) ... run %s with given frontend port",
		frontend_port,
		APP_NAME,
	)
	fmt.printfln(
		" -b | --backend-port BACKEND-PORT (default: %d) ... run %s with given backend port",
		backend_port,
		APP_NAME,
	)
	fmt.printfln(
		" -c | --start-control-node [CONTROL-NODE-PORT] (default: %d) ... start control node, connected to proxy, on given port",
		control_port,
	)
	fmt.println(" -d | --debug ... enable debug outputs")
}

// C++: auto parseSocketType = [](const string &s)
parse_socket_type :: proc(s: string) -> (zmq.Socket_Type, bool) {
	switch strings.to_lower(s, context.temp_allocator) {
	case "router": return .ROUTER, true
	case "dealer": return .DEALER, true
	case "push":   return .PUSH, true
	case "pull":   return .PULL, true
	case "req":    return .REQ, true
	case "rep":    return .REP, true
	}
	return {}, false
}

bind_or_exit :: proc(s: ^zmq.Socket, port: int) {
	address := fmt.ctprintf("tcp://*:%d", port)
	if zmq.bind(s, address) != 0 {
		fmt.eprintfln(
			"Couldn't bind %s socket to address: %s! Error: [%s]",
			APP_NAME,
			address,
			zmq.strerror(zmq.errno()),
		)
		os.exit(1)
	}
}

main :: proc() {
	args := os.args

	frontend_port := DEF_PROXY_FRONTEND_PORT
	backend_port := DEF_PROXY_BACKEND_PORT
	start_control_node := false
	control_port := DEF_CONTROL_PORT
	frontend_type := zmq.Socket_Type.ROUTER
	backend_type := zmq.Socket_Type.DEALER

	// C++ stoi() throws (and so aborts) on a malformed port; satoi falls back
	// to the default instead.
	i := 1
	for i < len(args) {
		arg := args[i]
		has_next := i + 1 < len(args)
		switch {
		case (arg == "-f" || arg == "--frontend-port") && has_next:
			i += 1
			frontend_port = tl.satoi(args[i], frontend_port)
		case (arg == "-b" || arg == "--backend-port") && has_next:
			i += 1
			backend_port = tl.satoi(args[i], backend_port)
		case arg == "-c" || arg == "--start-control-node":
			start_control_node = true
			if has_next && !strings.has_prefix(args[i + 1], "-") {
				i += 1
				control_port = tl.satoi(args[i], control_port)
			}
		case arg == "-p" || arg == "--pipeline-ports" || arg == "-pps" || arg == "--pull-push-sockets":
			frontend_type, backend_type = .PULL, .PUSH
		case arg == "-prs" || arg == "--pull-router-sockets":
			frontend_type, backend_type = .PULL, .ROUTER
		case (arg == "-fst" || arg == "--frontend-socket-type") && has_next:
			i += 1
			ok: bool
			if frontend_type, ok = parse_socket_type(args[i]); !ok {
				fmt.eprintln("invalid frontend-socket-type parameter")
				os.exit(1)
			}
		case (arg == "-bst" || arg == "--backend-socket-type") && has_next:
			i += 1
			ok: bool
			if backend_type, ok = parse_socket_type(args[i]); !ok {
				fmt.eprintln("invalid backend-socket-type parameter")
				os.exit(1)
			}
		case arg == "-d" || arg == "--debug":
		// no debug-trace facility ported
		case arg == "-h" || arg == "--help":
			print_help(frontend_port, backend_port, control_port)
			os.exit(0)
		case arg == "-v" || arg == "--version":
			fmt.printfln("%s version %s", APP_NAME, VERSION)
			os.exit(0)
		}
		i += 1
	}

	zmq_context := zmq.ctx_new()

	// socket facing clients
	frontend := zmq.socket(zmq_context, frontend_type)
	bind_or_exit(frontend, frontend_port)

	// socket facing services
	backend := zmq.socket(zmq_context, backend_type)
	if backend_type == .ROUTER {
		mandatory: c.int = 1
		zmq.setsockopt(backend, .ROUTER_MANDATORY, &mandatory, size_of(mandatory))
	}
	bind_or_exit(backend, backend_port)

	if start_control_node {
		fmt.eprintfln(
			"%s: -c/--start-control-node is not supported by the Odin port (no monica-zmq-control binary), ignoring",
			APP_NAME,
		)
	}

	// Only returns on error (e.g. the context being terminated).
	if zmq.proxy(frontend, backend, nil) != 0 {
		fmt.eprintfln("Couldn't start proxy! Error: [%s]", zmq.strerror(zmq.errno()))
		os.exit(1)
	}
}
