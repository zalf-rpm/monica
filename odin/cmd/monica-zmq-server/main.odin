// Port of src/run/monica-zmq-server-main.cpp + src/run/monica-zmq-defaults.h -
// the monica-zmq-server CLI entry point. See run/serve_zmq.odin's file header
// for what serveZmqMonicaFull itself drops (Cap'n Proto sturdy-refs,
// Intercropping). debug()/activateDebug's only remaining effect
// (Env.debugMode, itself unread downstream) is threaded through as a plain
// bool; the "starting/stopped ZeroMQ MONICA server" debug() trace lines are
// dropped, matching monica-run's main.odin (no debug-trace facility ported).
package main

import "core:c"
import "core:fmt"
import "core:os"
import "core:strings"
import run "../../monica/run"
import tl "../../support/tools"
import zmq "../../support/zeromq"

APP_NAME :: "monica-zmq-server"
VERSION :: "3.6.59.0"

// C++: monica-zmq-defaults.h
DEF_PROXY_BACKEND_ADDRESS :: "tcp://localhost:5566"
DEF_CONTROL_ADDRESS :: "tcp://localhost:8888"
DEF_INPUT_ADDRESS :: "tcp://localhost:6666"
DEF_OUTPUT_ADDRESS :: "tcp://localhost:7777"
DEF_SERVE_ADDRESS :: "tcp://*:6666"

// C++: auto printHelp = [=]()
//
// NOTE(c++-quirk): the "-p" line's default value shown is inputAddress, not
// proxyAddress - the C++ prints the wrong variable there. Reproduced.
print_help :: proc(serve_address, input_address, output_address, control_address: string) {
	fmt.printfln("%s[options]", APP_NAME)
	fmt.println()
	fmt.println("options:")
	fmt.println()
	fmt.println(" -h | --help ... this help output")
	fmt.printfln(" -v | --version ... outputs %s version and ZeroMQ version being used", APP_NAME)
	fmt.println()
	fmt.println(" -d | --debug ... show debug outputs")
	fmt.printfln(
		" -s | --serve-address [ADDRESS] (default: %s)] ... serve MONICA on given address",
		serve_address,
	)
	fmt.printfln(
		" -p | --proxy-address [(PROXY-)ADDRESS1[,ADDRESS2,...]] (default: %s)] ... receive work via proxy from given address(es)",
		input_address,
	)
	fmt.println(" -bi | --bind-input ... bind the input port")
	fmt.println(" -ci | --connect-input (default) ... connect the input port")
	fmt.printfln(
		" -i | --input-address [bind|connect]|[ADDRESS1[,ADDRESS2,...]] (default: %s)] ... receive work from given address(es)",
		input_address,
	)
	fmt.println(" -bo | --bind-output ... bind the output port")
	fmt.println(" -co | --connect-output (default) ... connect the output port")
	fmt.printfln(
		" -o | --output-address [ADDRESS1[,ADDRESS2,...]] (default: %s)] ... send results to this address(es)",
		output_address,
	)
	fmt.printfln(
		" -or | --router-output-address [ADDRESS1[,ADDRESS2,...]] (default: %s)] ... send results to this address(es) but use a router socket",
		output_address,
	)
	fmt.printfln(
		" -c | --control-address [ADDRESS] (default: %s)] ... connect MONICA server to this address for control messages",
		control_address,
	)
}

@(private)
single :: proc(s: string, allocator := context.allocator) -> [dynamic]string {
	v := make([dynamic]string, 0, 1, allocator)
	append(&v, s)
	return v
}

main :: proc() {
	args := os.args

	serve_address := DEF_SERVE_ADDRESS
	proxy_address := DEF_PROXY_BACKEND_ADDRESS
	connect_to_zmq_proxy := false
	input_address := DEF_INPUT_ADDRESS
	output_address := DEF_OUTPUT_ADDRESS
	use_pipeline := false
	use_router_output_socket := false
	control_address := DEF_CONTROL_ADDRESS

	input_op := run.Socket_Op.Connect
	output_op := run.Socket_Op.Connect

	activate_debug := false

	major, minor, patch: c.int
	zmq.version(&major, &minor, &patch)

	zmq_context := zmq.ctx_new()
	defer zmq.ctx_term(zmq_context)

	i := 1
	for i < len(args) {
		arg := args[i]
		has_value := i + 1 < len(args) && !strings.has_prefix(args[i + 1], "-")
		switch {
		case arg == "-d" || arg == "--debug":
			activate_debug = true
		case (arg == "-s" || arg == "--serve-address") && has_value:
			i += 1
			serve_address = args[i]
		case (arg == "-p" || arg == "--proxy-address"):
			connect_to_zmq_proxy = true
			if has_value {
				i += 1
				proxy_address = args[i]
			}
		case arg == "-bi" || arg == "--bind-input":
			input_op = .Bind
		case arg == "-ci" || arg == "--connect-input":
			input_op = .Connect
		case (arg == "-i" || arg == "--input-address") && has_value:
			i += 1
			input_address = args[i]
		case arg == "-bo" || arg == "--bind-output":
			output_op = .Bind
		case arg == "-co" || arg == "--connect-output":
			output_op = .Connect
		case (arg == "-o" || arg == "--output-address"):
			use_pipeline = true
			if has_value {
				i += 1
				output_address = args[i]
			}
		case (arg == "-or" || arg == "--router-output-address"):
			use_pipeline = true
			use_router_output_socket = true
			if has_value {
				i += 1
				output_address = args[i]
			}
		case (arg == "-c" || arg == "--control-address") && has_value:
			i += 1
			control_address = args[i]
		case arg == "-h" || arg == "--help":
			print_help(serve_address, input_address, output_address, control_address)
			os.exit(0)
		case arg == "-v" || arg == "--version":
			fmt.printfln("%s version %s ZeroMQ version: %d.%d.%d", APP_NAME, VERSION, major, minor, patch)
			os.exit(0)
		}
		i += 1
	}

	addresses := make(map[run.Socket_Role]run.Socket_Config)

	if use_pipeline {
		addresses[.ReceiveJob] = run.Socket_Config {
			type      = .Pull,
			addresses = tl.split_string(input_address, ","),
			op        = input_op,
		}
		addresses[.SendResult] = run.Socket_Config {
			type      = use_router_output_socket ? .Router : .Push,
			addresses = tl.split_string(output_address, ","),
			op        = output_op,
		}
	} else if connect_to_zmq_proxy {
		addresses[.ReceiveJob] = run.Socket_Config {
			type      = .ProxyReply,
			addresses = tl.split_string(proxy_address, ","),
			op        = .Connect,
		}
	} else {
		addresses[.ReceiveJob] = run.Socket_Config {
			type      = .Reply,
			addresses = tl.split_string(serve_address, ","),
			op        = .Bind,
		}
	}

	addresses[.Control] = run.Socket_Config {
		type      = .Subscribe,
		addresses = single(control_address),
		op        = .Connect,
	}

	run.serve_zmq_monica_full(zmq_context, addresses, activate_debug)
}
