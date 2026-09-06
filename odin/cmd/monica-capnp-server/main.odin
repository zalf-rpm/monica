// Port of src/run/monica-capnp-server-main.cpp - the monica-capnp-server CLI
// entry point. The capability it serves lives in monica/capnp (port of
// run/run-monica-capnp.{h,cpp}); read that file's header first, it explains why
// this goes through Cap'n Proto's dynamic API rather than generated code.
//
// BOOTSTRAP. Like the C++ (RestorableServiceMain::startRestorerSetup), the
// connection's bootstrap capability is a *Restorer*, and clients reach MONICA
// through capnp://<host>:<port>/<token> -> restore(token). See
// monica/capnp/restorer.odin for which parts of the C++ Restorer that covers and
// which it deliberately leaves out (vat ids, sealing, storage, registrar).
// --serve-as-bootstrap serves RunMonica directly instead, which is the C++'s own
// startRestorerSetup(serviceAsBootstrap = true) - handy for a client that just
// wants `capnp://host:port` with no token.
//
// These RestorableServiceMain options are still accepted-and-ignored rather than
// silently dropped, since they only mean something for the parts of the Restorer
// that are not ported: --restorer_container_sr, --service_container_sr,
// --registrar_sr, --reg_name, --reg_category, --local_host, --sr_host,
// --sr_port, --check_IP, --check_port, --startup_info_writer_sr,
// --startup_info_id, --init_from_storage.
//
// NOTE(c++-quirk): -h is --host, NOT help - RestorableServiceMain binds it that
// way and kj::MainBuilder spells help --help. Reproduced.
package main

import "core:fmt"
import "core:os"
import "core:path/filepath"
import "core:strings"
import "core:time"
import mcapnp "../../monica/capnp"
import capnp_dyn "../../support/capnp/odin/capnp_dynamic"

APP_NAME :: "monica-capnp-server"
VERSION :: "3.6.59.0"

// C++: RestorableServiceMain's defaults, narrowed to what a direct bootstrap uses.
DEF_HOST :: "localhost"
DEF_PORT :: "6666"

// Not a C++ option: the C++ links generated code, so it needs no schema files at
// runtime; the dynamic API parses the raw .capnp instead and has to find them.
SCHEMA_DIR_ENV :: "MONICA_CAPNP_SCHEMA_DIR"
SCHEMA_DIR_REL :: "../support/capnp/shim/external/mas_capnproto_schemas/zalfmas_capnp_schemas"

print_help :: proc() {
	fmt.printfln("%s [options]", APP_NAME)
	fmt.println()
	fmt.println("Offers a MONICA as a Cap'n Proto service.")
	fmt.println()
	fmt.println("options:")
	fmt.println()
	fmt.println("     --help ... this help output")
	fmt.printfln(" -v | --version ... outputs %s version", APP_NAME)
	fmt.println()
	fmt.println(" -d | --debug ... activate debug output")
	fmt.println(" -n | --name [NAME] ... name of service")
	fmt.println("      --description [TEXT] ... description of service")
	fmt.printfln(" -h | --host [HOST] (default: %s) ... host/IP to bind to (NOT help)", DEF_HOST)
	fmt.printfln(" -p | --port [PORT] (default: %s) ... port to bind to, 0 picks a free one", DEF_PORT)
	fmt.println(" -t | --srt [TOKEN] ... use a fixed sturdy ref token instead of a fresh UUID4")
	fmt.println(
		"      --serve-as-bootstrap ... serve MONICA itself as the bootstrap capability, so a client",
	)
	fmt.println(
		"                               can connect to capnp://HOST:PORT with no token (default: a",
	)
	fmt.println("                               Restorer is the bootstrap, as in the C++ server)")
	fmt.println("      --output_srs ... print the sturdy ref clients should connect to, to stdout")
	fmt.printfln(
		"      --schemas [DIR] (default: $%s, else <exe-dir>/%s) ... zalfmas_capnp_schemas directory",
		SCHEMA_DIR_ENV,
		SCHEMA_DIR_REL,
	)
	fmt.println()
	fmt.println("Options accepted but ignored, for the Restorer parts that are not ported:")
	fmt.println(
		"      --restorer_container_sr, --service_container_sr, --registrar_sr, --reg_name,",
	)
	fmt.println(
		"      --reg_category, --local_host, --sr_host, --sr_port, --check_IP, --check_port,",
	)
	fmt.println("      --startup_info_writer_sr, --startup_info_id, --init_from_storage")
}

// Options that take an argument and are accepted purely for command-line
// compatibility with the C++ - see the file header.
@(private)
IGNORED_WITH_ARG :: []string {
	"--restorer_container_sr",
	"--service_container_sr",
	"--registrar_sr",
	"--reg_name",
	"--reg_category",
	"--local_host",
	"--sr_host",
	"--sr_port",
	"--check_IP",
	"--check_port",
	"--startup_info_writer_sr",
	"--startup_info_id",
	"--init_from_storage",
}

main :: proc() {
	args := os.args

	// C++: MonicaCapnpServerMain's members plus RestorableServiceMain's.
	startedServerInDebugMode := false
	name := ""
	description := ""
	host := DEF_HOST
	port := DEF_PORT
	outputSturdyRefs := false
	srt := "" // C++: MonicaCapnpServerMain::srt, the -t/--srt fixed token
	serviceAsBootstrap := false

	schema_root := os.get_env(SCHEMA_DIR_ENV, context.allocator)

	i := 1
	for i < len(args) {
		arg := args[i]
		has_value := i + 1 < len(args)

		ignored := false
		for opt in IGNORED_WITH_ARG {
			if arg == opt {
				ignored = true
				if has_value {
					i += 1
				}
				break
			}
		}
		if ignored {
			i += 1
			continue
		}

		switch {
		case arg == "-d" || arg == "--debug":
			startedServerInDebugMode = true
		case (arg == "-n" || arg == "--name") && has_value:
			i += 1
			name = args[i]
		case arg == "--description" && has_value:
			i += 1
			description = args[i]
		case (arg == "-h" || arg == "--host") && has_value:
			i += 1
			host = args[i]
		case (arg == "-p" || arg == "--port") && has_value:
			i += 1
			port = args[i]
		case (arg == "-t" || arg == "--srt") && has_value:
			i += 1
			srt = args[i]
		case arg == "--output_srs":
			outputSturdyRefs = true
		case arg == "--serve-as-bootstrap":
			serviceAsBootstrap = true
		case arg == "--schemas" && has_value:
			i += 1
			schema_root = args[i]
		case arg == "--help":
			print_help()
			os.exit(0)
		case arg == "-v" || arg == "--version":
			fmt.printfln("%s version %s", APP_NAME, VERSION)
			os.exit(0)
		case:
			fmt.eprintfln("%s: unrecognized option '%s'; try --help", APP_NAME, arg)
			os.exit(1)
		}
		i += 1
	}

	if schema_root == "" {
		schema_root = fmt.aprintf("%s/%s", filepath.dir(os.args[0]), SCHEMA_DIR_REL)
	}
	schema := mcapnp.make_schema_paths(schema_root)
	if !os.exists(schema.model) {
		fmt.eprintfln(
			"%s: Cap'n Proto schema not found at %s.\n  Point --schemas (or $%s) at the zalfmas_capnp_schemas directory.",
			APP_NAME,
			schema.model,
			SCHEMA_DIR_ENV,
		)
		os.exit(1)
	}

	// C++: auto ownedRunMonica = kj::heap<RunMonica>(startedServerInDebugMode);
	//      if (name.size() > 0) runMonica->setName(name);
	runMonica := mcapnp.make_run_monica(startedServerInDebugMode, schema)
	if len(name) > 0 {
		runMonica._name = name
	}
	if len(description) > 0 {
		runMonica._description = description
	}

	// C++: MonicaEnvInstance::Client runMonicaClient = kj::mv(ownedRunMonica);
	//
	// host_async, not host: run() has to call back into the timeSeries/soilProfile
	// capabilities it is handed, which the shim only permits from a deferred
	// handler - see monica/capnp/run_monica_capnp.odin's header.
	client, host_err, host_ok := capnp_dyn.host_async(
		schema.model,
		schema.root,
		"EnvInstance",
		mcapnp.handle_call,
		runMonica,
	)
	if !host_ok {
		fmt.eprintfln("%s: could not host EnvInstance: %s", APP_NAME, host_err)
		os.exit(1)
	}

	// C++: startRestorerSetup(runMonicaClient) - the Restorer becomes the
	// connection's bootstrap capability and MONICA is reached by restoring a
	// token, unless --serve-as-bootstrap asks for the C++'s serviceAsBootstrap
	// behaviour instead.
	restorer: ^mcapnp.Restorer
	bootstrap := client
	if !serviceAsBootstrap {
		restorer = mcapnp.make_restorer(schema, host)
		bootstrap, host_err, host_ok = capnp_dyn.host(
			schema.persistence,
			schema.root,
			"Restorer",
			mcapnp.restorer_handle_call,
			restorer,
		)
		if !host_ok {
			fmt.eprintfln("%s: could not host Restorer: %s", APP_NAME, host_err)
			os.exit(1)
		}
	}

	address := strings.concatenate({host, ":", port})
	server, listen_err, listen_ok := capnp_dyn.listen(address, bootstrap)
	if !listen_ok {
		fmt.eprintfln("%s: could not listen on %s: %s", APP_NAME, address, listen_err)
		os.exit(1)
	}

	// C++: auto monicaSR = restorer->saveStr(runMonicaClient, srt, nullptr, false)
	//        .wait(...).sturdyRef;
	//      if (outputSturdyRefs && monicaSR.size() > 0) cout << "monicaSR=" << ...
	//
	// The port has to be known first (--port 0 asks the OS to pick one), which is
	// what the C++'s Restorer::setPort does after the bind resolves.
	monicaSR := ""
	if restorer != nil {
		mcapnp.restorer_set_port(restorer, capnp_dyn.server_port(server))
		sr_err: string
		sr_ok: bool
		_, monicaSR, sr_err, sr_ok = mcapnp.restorer_save_str(restorer, client, srt)
		if !sr_ok {
			fmt.eprintfln("%s: could not register the MONICA sturdy ref: %s", APP_NAME, sr_err)
			os.exit(1)
		}
	} else {
		// No Restorer, so no token: the bootstrap address IS the reference.
		monicaSR = fmt.aprintf("capnp://%s:%d", host, capnp_dyn.server_port(server))
	}
	if outputSturdyRefs && len(monicaSR) > 0 {
		fmt.printfln("monicaSR=%s", monicaSR)
	}

	// C++: kj::NEVER_DONE.wait(ioContext.waitScope) - the shim runs the event loop
	// on the server's own background thread, so this thread just has to stay alive.
	for {
		time.sleep(1 * time.Second)
	}
}
