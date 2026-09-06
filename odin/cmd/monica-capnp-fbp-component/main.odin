// Port of src/run/monica-capnp-fbp-component-main.cpp - MONICA as a Flow-Based
// Programming component: read Env information packets off an `env` channel, run
// MONICA, write the result JSON to a `result` channel.
//
// The channel plumbing (sturdy refs, PortConnector, the IP attribute helpers)
// lives in support/fbp, ported from mas_cpp_misc/common; this file is the
// component logic and its CLI, matching the C++ FBPMain.
//
// LOCAL VS REMOTE MONICA. The C++ has one code path: with a `monica_sr` config
// entry it connects to a remote EnvInstance, otherwise it constructs a local
// `kj::heap<RunMonica>` - and calls `runRequest()` on either, since both are the
// same Client type. This port keeps both behaviours but not that uniformity: the
// shim can only call a capability that has a connection behind it
// (capnp_dyn_call rejects a locally hosted one with "no associated connection to
// wait on"), so the inline case calls the implementation directly, through
// monica/capnp's run_monica_env_blocking. Same work, same inputs, one less layer
// of indirection - and the inline case is genuinely inline either way, which is
// the point of it.
//
// NOT PORTED: -l/--log_level (no logging facility in this port, as in
// monica-run), and -n/--name, which the C++ accepts but never reads.
package main

import "core:fmt"
import "core:os"
import "core:path/filepath"
import "core:strings"
import mcapnp "../../monica/capnp"
import mio "../../monica/io"
import capnp_dyn "../../support/capnp/odin/capnp_dynamic"
import "../../support/fbp"
import jx "../../support/jsonx"

APP_NAME :: "monica-capnp-fbp-component"
VERSION :: "3.6.59.0"

SCHEMA_DIR_ENV :: "MONICA_CAPNP_SCHEMA_DIR"
SCHEMA_DIR_REL :: "../support/capnp/shim/external/mas_capnproto_schemas/zalfmas_capnp_schemas"

// C++: FBPMain::inPortNames / outPortNames (the PORTS enum's names)
PORT_CONFIG :: "conf"
PORT_ENV :: "env"
PORT_RESULT :: "result"

// C++: const std::string DEFAULT_CONFIG - printed verbatim by
// -O/--output_json_default_config, to be used as an IIP on the 'conf' port.
DEFAULT_CONFIG :: `{
  "category": {
    "id": "models/monica",
    "name": "Models/MONICA"
  },
  "info": {
    "id": "66f86dbb-efb1-4b16-8837-3a2aa1eca30e",
    "name": "MONICA",
    "description": "MONICA FBP component"
  },
  "type": "standard",
  "inPorts": [
    {
      "name": "conf",
      "contentType": "@0xed6c098b67cad454 = common/common.capnp:StructuredText[JSON | TOML]"
    },
    {
      "name": "env",
      "contentType": "@0xb7fc866ef1127f7c = model/model.capnp:Env[@0xed6c098b67cad454 = common/common.capnp:StructuredText[JSON]]",
      "desc": "Env IP content will be used as MONICA input. Forwards open/close brackets downstream."
    }
  ],
  "outPorts": [
    {
      "name": "result",
      "contentType": "Text (JSON object)",
      "desc": "MONICA output."
    }
  ],
  "defaultConfig": {
    "from_attr": {
      "value": null,
      "type": "string",
      "desc": "Get Env from this attribute instead of content of env IP."
    },
    "to_attr": {
      "value": null,
      "type": "string",
      "desc": "Send content instead in 'to_attr'"
    },
    "monica_sr": {
      "value": null,
      "type": "string",
      "desc": "Use this sturdy ref to connect to an external MONICA, else a MONICA instance will be created."
    }
  }
}`

print_help :: proc() {
	fmt.printfln("%s [options] [port_infos_reader_SR]", APP_NAME)
	fmt.println()
	fmt.println("Offers a MONICA service.")
	fmt.println()
	fmt.println("options:")
	fmt.println()
	fmt.println("      --help ... this help output")
	fmt.printfln(" -v | --version ... outputs %s version", APP_NAME)
	fmt.println()
	fmt.println(
		" -O | --output_json_default_config ... output JSON configuration file with default settings",
	)
	fmt.println("                                       at commandline. To be used with IIP at 'conf' port.")
	fmt.println("      --env_in_sr [SR] ... sturdy ref to input channel")
	fmt.println("      --config_in_sr [SR] ... sturdy ref to config channel")
	fmt.println("      --result_out_sr [SR] ... sturdy ref to output channel")
	fmt.printfln(
		"      --schemas [DIR] (default: $%s, else <exe-dir>/%s) ... zalfmas_capnp_schemas directory",
		SCHEMA_DIR_ENV,
		SCHEMA_DIR_REL,
	)
	fmt.println()
	fmt.println("Accepted but ignored: -n | --name, -l | --log_level (see the file header).")
}

// Where MONICA runs: a capability on some other vat, or this process.
Monica :: union {
	capnp_dyn.Capability, // C++: runMonicaClient after tryConnectB(monicaSr)
	^mcapnp.Run_Monica, // C++: kj::heap<RunMonica>(startedServerInDebugMode)
}

main :: proc() {
	args := os.args

	// C++: FBPMain's members
	portInfosReaderSr := ""
	monicaSr := ""
	fromAttr := ""
	toAttr := ""
	envInSr := ""
	configInSr := ""
	resultOutSr := ""
	outputJsonDefaultConfig := false
	schema_root := os.get_env(SCHEMA_DIR_ENV, context.allocator)

	i := 1
	for i < len(args) {
		arg := args[i]
		has_value := i + 1 < len(args)
		switch {
		case arg == "-O" || arg == "--output_json_default_config":
			outputJsonDefaultConfig = true
		case arg == "--env_in_sr" && has_value:
			i += 1
			envInSr = args[i]
		case arg == "--config_in_sr" && has_value:
			i += 1
			configInSr = args[i]
		case arg == "--result_out_sr" && has_value:
			i += 1
			resultOutSr = args[i]
		case (arg == "-n" || arg == "--name") && has_value:
			i += 1 // accepted and ignored, as in the C++ (it never reads `name`)
		case (arg == "-l" || arg == "--log_level") && has_value:
			i += 1 // accepted and ignored - no logging facility in this port
		case arg == "--schemas" && has_value:
			i += 1
			schema_root = args[i]
		case arg == "--help":
			print_help()
			os.exit(0)
		case arg == "-v" || arg == "--version":
			fmt.printfln("%s version %s", APP_NAME, VERSION)
			os.exit(0)
		case strings.has_prefix(arg, "-"):
			fmt.eprintfln("%s: unrecognized option '%s'; try --help", APP_NAME, arg)
			os.exit(1)
		case:
			// C++: .expectOptionalArg("port_infos_reader_SR", ...)
			portInfosReaderSr = arg
		}
		i += 1
	}

	// C++: if (outputJsonDefaultConfig) { cout << DEFAULT_CONFIG << endl; return true; }
	if outputJsonDefaultConfig {
		fmt.println(DEFAULT_CONFIG)
		os.exit(0)
	}

	if schema_root == "" {
		schema_root = fmt.aprintf("%s/%s", filepath.dir(os.args[0]), SCHEMA_DIR_REL)
	}
	fbp_schema := fbp.make_schema_paths(schema_root)
	monica_schema := mcapnp.make_schema_paths(schema_root)
	if !os.exists(fbp_schema.fbp) {
		fmt.eprintfln(
			"%s: Cap'n Proto schema not found at %s.\n  Point --schemas (or $%s) at the zalfmas_capnp_schemas directory.",
			APP_NAME,
			fbp_schema.fbp,
			SCHEMA_DIR_ENV,
		)
		os.exit(1)
	}

	ports := fbp.make_port_connector(fbp_schema)
	defer fbp.port_connector_close(&ports)

	// C++: the portInfosReaderSr / individual-SR branch
	if len(portInfosReaderSr) > 0 {
		if err, ok := fbp.port_connector_connect_from_port_infos(
			&ports,
			portInfosReaderSr,
			{PORT_CONFIG, PORT_ENV},
			{PORT_RESULT},
		); !ok {
			fmt.eprintfln("%s: could not connect from port infos: %s", APP_NAME, err)
			os.exit(1)
		}
	} else if len(envInSr) > 0 && len(resultOutSr) > 0 {
		if err, ok := fbp.port_connector_connect_sr(&ports, PORT_ENV, envInSr, .In); !ok {
			fmt.eprintfln("%s: could not connect the env IN port: %s", APP_NAME, err)
			os.exit(1)
		}
		if len(configInSr) > 0 {
			if err, ok := fbp.port_connector_connect_sr(&ports, PORT_CONFIG, configInSr, .In); !ok {
				fmt.eprintfln("%s: could not connect the conf IN port: %s", APP_NAME, err)
				os.exit(1)
			}
		}
		if err, ok := fbp.port_connector_connect_sr(&ports, PORT_RESULT, resultOutSr, .Out); !ok {
			fmt.eprintfln("%s: could not connect the result OUT port: %s", APP_NAME, err)
			os.exit(1)
		}
	} else {
		// C++: KJ_LOG(ERROR, "At least env_in_sr and result_out_sr has to be supplied.")
		fmt.eprintfln("%s: at least --env_in_sr and --result_out_sr have to be supplied.", APP_NAME)
		os.exit(1)
	}

	// C++: read the config IP from the CONFIG port, if connected
	config: jx.Value
	if fbp.port_connector_in_connected(&ports, PORT_CONFIG) {
		config = read_config(&ports, fbp_schema)
	}
	// NOTE(c++-quirk): the C++ reads "fromAttr"/"toAttr" here but its own
	// DEFAULT_CONFIG documents "from_attr"/"to_attr", so the documented spelling
	// never takes effect. Both are accepted rather than reproducing that.
	fromAttr = first_string(config, "fromAttr", "from_attr")
	toAttr = first_string(config, "toAttr", "to_attr")
	monicaSr = first_string(config, "monica_sr")

	// C++: MonicaEnvInstance::Client runMonicaClient - remote or local
	monica: Monica
	monica_conn: fbp.Sr_Connection
	if len(monicaSr) > 0 {
		c, err, ok := fbp.connect_sturdy_ref(monicaSr, fbp_schema, monica_schema.model, "EnvInstance")
		if !ok {
			fmt.eprintfln("%s: could not connect to MONICA at '%s': %s", APP_NAME, monicaSr, err)
			os.exit(1)
		}
		monica_conn = c
		monica = c.cap
	} else {
		// startedServerInDebugMode is `false` in the C++ too - it is a local, never
		// assigned, and only reaches Env.debugMode, which nothing reads.
		monica = mcapnp.make_run_monica(false, monica_schema)
	}
	defer if monica_conn.conn != nil {fbp.sr_connection_close(monica_conn)}

	run_loop(&ports, monica, fbp_schema, monica_schema, fromAttr, toAttr)

	// C++: ports.closeOutPorts();
	fbp.port_connector_close_out_ports(&ports)
}

// C++: the `if (ports.isInConnected(CONFIG))` block
read_config :: proc(ports: ^fbp.Port_Connector, schema: fbp.Schema_Paths) -> jx.Value {
	msg, err, ok := capnp_dyn.call(fbp.port_connector_in(ports, PORT_CONFIG), "read", nil)
	if !ok {
		fmt.eprintfln("%s: could not read the conf port: %s", APP_NAME, err)
		return nil
	}
	// C++: if (!configMsg.isDone())
	if _, done := capnp_dyn.field_get(msg, "done"); done {
		return nil
	}
	ip, has_ip := msg_value_as_ip(msg, schema)
	if !has_ip {
		return nil
	}
	content, has_content := capnp_dyn.field_get(ip, "content")
	ap, is_ap := content.(capnp_dyn.Any_Pointer)
	if !has_content || !is_ap {
		return nil
	}
	// C++: configIp.getContent().getAs<StructuredText>()
	st, as_err, as_ok := capnp_dyn.any_pointer_as_struct(ap, schema.common, schema.root, "StructuredText")
	if !as_ok {
		fmt.eprintfln("%s: conf IP content is not a StructuredText: %s", APP_NAME, as_err)
		return nil
	}
	text := ""
	if v, got := capnp_dyn.field_get(st, "value"); got {
		text, _ = v.(string)
	}
	// C++: parseJsonString(...); if (configJson.success()) config = configJson.result;
	pr := jx.parse_json_string(text, context.allocator)
	if len(pr.errs.errors) > 0 {
		return nil
	}
	return pr.result
}

// C++: the `while (ports.isInConnected(ENV) && ports.isOutConnected(RESULT))` loop
run_loop :: proc(
	ports: ^fbp.Port_Connector,
	monica: Monica,
	schema: fbp.Schema_Paths,
	monica_schema: mcapnp.Schema_Paths,
	fromAttr: string,
	toAttr: string,
) {
	for fbp.port_connector_in_connected(ports, PORT_ENV) &&
	    fbp.port_connector_out_connected(ports, PORT_RESULT) {
		msg, err, ok := capnp_dyn.call(fbp.port_connector_in(ports, PORT_ENV), "read", nil)
		if !ok {
			// C++: catch (const kj::Exception &e) around the whole loop
			fmt.eprintfln("%s: exception reading the env port: %s", APP_NAME, err)
			return
		}
		// C++: if (msg.isDone()) break;
		if _, done := capnp_dyn.field_get(msg, "done"); done {
			return
		}
		in_ip, has_ip := msg_value_as_ip(msg, schema)
		if !has_ip {
			continue
		}

		// C++: simply forward open and close brackets downstream
		ip_t := fbp.ip_type(in_ip)
		if ip_t == fbp.IP_OPEN_BRACKET || ip_t == fbp.IP_CLOSE_BRACKET {
			forward_bracket(ports, in_ip, ip_t, schema)
			continue
		}

		// C++: auto attr = getIPAttr(inIp, fromAttr);
		//      auto env = attr.orDefault(inIp.getContent()).getAs<Env>();
		env_value, has_env := capnp_dyn.Value(nil), false
		if v, found := fbp.ip_get_attr(in_ip, fromAttr); found {
			env_value, has_env = v, true
		} else if v, found := capnp_dyn.field_get(in_ip, "content"); found {
			env_value, has_env = v, true
		}
		if !has_env {
			continue
		}
		env_ap, is_ap := env_value.(capnp_dyn.Any_Pointer)
		if !is_ap {
			fmt.eprintfln("%s: env IP content is not an AnyPointer", APP_NAME)
			continue
		}
		env, as_err, as_ok := capnp_dyn.any_pointer_as_struct(
			env_ap,
			monica_schema.model,
			monica_schema.root,
			"Env",
		)
		if !as_ok {
			fmt.eprintfln("%s: env IP content is not an Env: %s", APP_NAME, as_err)
			continue
		}

		result_json, produced := run_one(monica, env, monica_schema)
		if !produced {
			continue
		}
		write_result(ports, in_ip, result_json, toAttr, schema)
	}
}

// C++: rreq.setEnv(env); rreq.send() - or, for a local instance, the same run
// through RunMonica itself (see the file header).
run_one :: proc(
	monica: Monica,
	env: []capnp_dyn.Field,
	monica_schema: mcapnp.Schema_Paths,
) -> (
	result_json: string,
	ok: bool,
) {
	switch m in monica {
	case capnp_dyn.Capability:
		params := []capnp_dyn.Field{{name = "env", value = env}}
		res, err, call_ok := capnp_dyn.call(m, "run", params)
		if !call_ok {
			fmt.eprintfln("%s: MONICA run failed: %s", APP_NAME, err)
			return "", false
		}
		// C++: if (res.hasResult() && res.getResult().hasValue())
		result, has_result := capnp_dyn.field_get(res, "result")
		ap, is_ap := result.(capnp_dyn.Any_Pointer)
		if !has_result || !is_ap {
			return "", false
		}
		st, _, as_ok := capnp_dyn.any_pointer_as_struct(
			ap,
			monica_schema.common,
			monica_schema.root,
			"StructuredText",
		)
		if !as_ok {
			return "", false
		}
		v, got := capnp_dyn.field_get(st, "value")
		text, is_text := v.(string)
		if !got || !is_text || len(text) == 0 {
			return "", false
		}
		return text, true

	case ^mcapnp.Run_Monica:
		out := mcapnp.run_monica_env_blocking(m, env, context.allocator)
		text := jx.dump(mio.output_to_json(&out, context.allocator), context.allocator)
		if len(text) == 0 {
			return "", false
		}
		return text, true
	}
	return "", false
}

// C++: the OPEN_BRACKET/CLOSE_BRACKET forwarding branch
forward_bracket :: proc(
	ports: ^fbp.Port_Connector,
	in_ip: []capnp_dyn.Field,
	ip_t: string,
	schema: fbp.Schema_Paths,
) {
	out_ip := make([dynamic]capnp_dyn.Field, 0, 3, context.temp_allocator)
	append(&out_ip, capnp_dyn.Field{name = "type", value = capnp_dyn.Enum_Value{name = ip_t}})
	if content, has := capnp_dyn.field_get(in_ip, "content"); has {
		append(&out_ip, capnp_dyn.Field{name = "content", value = content})
	}
	if attrs, has := fbp.ip_copy_and_set_attrs(in_ip, "", nil, context.temp_allocator); has {
		append(&out_ip, capnp_dyn.Field{name = "attributes", value = attrs})
	}
	write_ip(ports, out_ip[:], schema)
}

// C++: the result-writing block - content unless to_attr was requested, plus the
// copied attributes with the result set as one if it was.
write_result :: proc(
	ports: ^fbp.Port_Connector,
	in_ip: []capnp_dyn.Field,
	result_json: string,
	toAttr: string,
	schema: fbp.Schema_Paths,
) {
	out_ip := make([dynamic]capnp_dyn.Field, 0, 2, context.temp_allocator)
	// C++: if (kj::size(toAttr) == 0) outIp.initContent().setAs<capnp::Text>(resJsonStr);
	if len(toAttr) == 0 {
		append(&out_ip, capnp_dyn.Field{name = "content", value = result_json})
	}
	if attrs, has := fbp.ip_copy_and_set_attrs(in_ip, toAttr, result_json, context.temp_allocator);
	   has {
		append(&out_ip, capnp_dyn.Field{name = "attributes", value = attrs})
	}
	write_ip(ports, out_ip[:], schema)
}

write_ip :: proc(ports: ^fbp.Port_Connector, ip: []capnp_dyn.Field, schema: fbp.Schema_Paths) {
	// Channel(V).Writer.write takes a Msg whose `value` is V - unbound, so an
	// AnyPointer, which needs the IP schema to allocate the struct's layout (see
	// any_pointer_from_struct's comment in capnp_dynamic.odin).
	ap, err, ok := capnp_dyn.any_pointer_from_struct(schema.fbp, schema.root, "IP", ip)
	if !ok {
		fmt.eprintfln("%s: could not build the result IP: %s", APP_NAME, err)
		return
	}
	defer capnp_dyn.any_pointer_free(ap)

	params := []capnp_dyn.Field{{name = "value", value = ap}}
	if _, werr, wok := capnp_dyn.call(fbp.port_connector_out(ports, PORT_RESULT), "write", params);
	   !wok {
		fmt.eprintfln("%s: could not write to the result port: %s", APP_NAME, werr)
	}
}

// Channel(V) is generic and this component never binds V, so `Msg.value` comes
// back as an AnyPointer rather than a struct - reinterpret it as the fbp.capnp
// IP the channel is actually carrying.
@(private)
msg_value_as_ip :: proc(
	msg: []capnp_dyn.Field,
	schema: fbp.Schema_Paths,
) -> (
	ip: []capnp_dyn.Field,
	ok: bool,
) {
	v, has := capnp_dyn.field_get(msg, "value")
	if !has {
		return nil, false
	}
	ap, is_ap := v.(capnp_dyn.Any_Pointer)
	if !is_ap {
		return nil, false
	}
	fields, err, as_ok := capnp_dyn.any_pointer_as_struct(ap, schema.fbp, schema.root, "IP")
	if !as_ok {
		fmt.eprintfln("%s: channel message is not an IP: %s", APP_NAME, err)
		return nil, false
	}
	return fields, true
}

// Reads the first of `keys` present as a string - see the c++-quirk note at the
// call site.
@(private)
first_string :: proc(config: jx.Value, keys: ..string) -> string {
	for k in keys {
		if s := jx.string_value_of(jx.get(config, k)); s != "" {
			return s
		}
	}
	return ""
}
