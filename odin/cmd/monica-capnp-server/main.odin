// Port of src/run/monica-capnp-server-main.cpp - the monica-capnp-server CLI
// entry point. The capability it serves lives in monica/capnp (port of
// run/run-monica-capnp.{h,cpp}); read that file's header first, it explains why
// this goes through Cap'n Proto's dynamic API rather than generated code.
//
// MVP SCOPE: DIRECT BOOTSTRAP. The C++ builds on kj::MainBuilder and
// mas_cpp_misc/common/restorable-service-main.{h,cpp}, whose startRestorerSetup
// serves a *Restorer* as the connection's bootstrap capability; clients then
// reach MONICA through capnp://<vatId>@<host>:<port>/<token> -> restore(token).
// That Restorer is a large piece of infrastructure of its own (vat ids, ed25519
// sealing, a storage-service container, registrar heartbeats), so this serves
// RunMonica *itself* as the bootstrap instead - the same thing the C++ does when
// startRestorerSetup is passed serviceAsBootstrap = true, just unconditionally.
// So: `capnp://host:port` works, sturdy refs do not (yet).
//
// Consequently these RestorableServiceMain options are accepted-and-ignored
// rather than silently dropped, since they only mean something with a Restorer:
// --restorer_container_sr, --service_container_sr, --registrar_sr, --reg_name,
// --reg_category, --local_host, --sr_host, --sr_port, --check_IP, --check_port,
// --startup_info_writer_sr, --startup_info_id, --init_from_storage, and
// monica-capnp-server-main.cpp's own -t/--srt.
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
	fmt.println("      --output_srs ... print the address clients should connect to, to stdout")
	fmt.printfln(
		"      --schemas [DIR] (default: $%s, else <exe-dir>/%s) ... zalfmas_capnp_schemas directory",
		SCHEMA_DIR_ENV,
		SCHEMA_DIR_REL,
	)
	fmt.println()
	fmt.println("Restorer-only options, accepted but ignored by this build (see the file header):")
	fmt.println(
		"      -t | --srt, --restorer_container_sr, --service_container_sr, --registrar_sr, --reg_name,",
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
	"-t",
	"--srt",
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
		case arg == "--output_srs":
			outputSturdyRefs = true
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

	// C++: startRestorerSetup(runMonicaClient) - here the service itself is the
	// bootstrap, see the file header.
	address := strings.concatenate({host, ":", port})
	server, listen_err, listen_ok := capnp_dyn.listen(address, client)
	if !listen_ok {
		fmt.eprintfln("%s: could not listen on %s: %s", APP_NAME, address, listen_err)
		os.exit(1)
	}

	// C++: if (outputSturdyRefs && monicaSR.size() > 0) cout << "monicaSR=" << ...
	// Without a Restorer there is no token to put in the ref, so this is the plain
	// bootstrap address a client connects to.
	if outputSturdyRefs {
		fmt.printfln("monicaSR=capnp://%s:%d", host, capnp_dyn.server_port(server))
	}

	// C++: kj::NEVER_DONE.wait(ioContext.waitScope) - the shim runs the event loop
	// on the server's own background thread, so this thread just has to stay alive.
	for {
		time.sleep(1 * time.Second)
	}
}
