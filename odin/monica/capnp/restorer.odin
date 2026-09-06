// Minimal port of mas_cpp_misc/common/restorer.{h,cpp} - just enough of the
// Restorer for monica-capnp-server to be reachable the way the C++ one is:
// a client connects to `capnp://<host>:<port>/<token>`, gets the Restorer as
// the connection's bootstrap capability, calls restore(token), and receives
// MONICA.
//
// WHAT THIS COVERS. The C++ Restorer::restore looks its token up in
// Impl::issuedSRTokens (a map filled by saveStr) and returns the capability
// stored there, leaving the `cap` result unset if the token is unknown. That is
// exactly what restorer_handle_call does. saveStr's token is the caller's fixed
// one (-t/--srt) or a fresh uuid4, and sturdyRefStr assembles the URL - both
// reproduced.
//
// WHAT IT DELIBERATELY DOES NOT COVER, since none of it is needed to reach a
// MONICA instance and each is a substantial piece of infrastructure of its own:
//
//   - the vat id. The C++ SR is `capnp://<base64 curve25519 public key>@host:
//     port/token`, and the key needs libsodium. BOTH client stacks ignore the
//     userinfo part when connecting - rpc-connection-manager.cpp's
//     ConnectionManager::connect never reads url.userInfo except for the
//     HostPortResolver case, and zalfmas_common's Python ConnectionManager has
//     `# vat_id_base64 = url.username` commented out - so a vat-id-less
//     `capnp://host:port/token` restores identically. If a peer ever does need
//     the key (three-party handoff, sealed refs), this is where it goes.
//   - sealing/signing (SaveParams.sealFor, verifySRToken). sealedBy is read and
//     rejected rather than silently ignored, so a client that asks for sealing
//     gets told it is unsupported instead of quietly receiving an unsealed cap.
//   - persistence to a storage service (Store::Container), heartbeats,
//     unsave/ReleaseSturdyRef, and the registrar. Tokens live in memory for the
//     lifetime of the process, which is all a server whose only capability is
//     created at startup needs.
package capnp

import "core:encoding/uuid"
import "core:fmt"
import "core:strings"
import "core:sync"
import capnp_dyn "../../support/capnp/odin/capnp_dynamic"

// C++: class mas::infrastructure::common::Restorer (the subset above)
Restorer :: struct {
	schema: Schema_Paths,
	host:   string, // C++: Impl::host - what goes into the SR, not the bind address
	port:   int, // C++: Impl::port
	lock:   sync.Mutex,
	issued: map[string]capnp_dyn.Capability, // C++: Impl::issuedSRTokens
}

// C++: Restorer::Restorer()
make_restorer :: proc(schema: Schema_Paths, host: string, allocator := context.allocator) -> ^Restorer {
	r := new(Restorer, allocator)
	r.schema = schema
	r.host = host
	r.issued = make(map[string]capnp_dyn.Capability, allocator)
	return r
}

// C++: void Restorer::setPort(int)
restorer_set_port :: proc(r: ^Restorer, port: int) {
	r.port = port
}

// C++: kj::String Restorer::sturdyRefStr(kj::StringPtr srToken) const
//
// Without the `<vatIdBase64>@` prefix - see the file header for why that is
// safe, and the port is omitted when unset exactly as the C++ does.
restorer_sturdy_ref_str :: proc(
	r: ^Restorer,
	sr_token: string,
	allocator := context.allocator,
) -> string {
	port_part := r.port > 0 ? fmt.aprintf(":%d", r.port, allocator = allocator) : ""
	token_part := sr_token == "" ? "" : fmt.aprintf("/%s", sr_token, allocator = allocator)
	return strings.concatenate({"capnp://", r.host, port_part, token_part}, allocator)
}

// C++: Restorer::saveStr(cap, fixedSRToken, sealForOwnerGuid, createUnsave, ...)
//
// Reduced to what monica-capnp-server-main.cpp actually asks for
// (`restorer->saveStr(runMonicaClient, srt, nullptr, false)`): no owner guid, no
// unsave action, no store. Returns both halves the caller needs - the token to
// register and the printable SR.
restorer_save_str :: proc(
	r: ^Restorer,
	cap: capnp_dyn.Capability,
	fixed_sr_token: string,
	allocator := context.allocator,
) -> (
	sr_token: string,
	sturdy_ref: string,
	err: string,
	ok: bool,
) {
	// C++: auto srToken = fixedSRToken == nullptr ? kj::str(sole::uuid4().str())
	//                                             : kj::str(fixedSRToken);
	sr_token = fixed_sr_token
	if sr_token == "" {
		sr_token = uuid.to_string(uuid.generate_v4(), allocator)
	}

	sync.mutex_lock(&r.lock)
	defer sync.mutex_unlock(&r.lock)

	// C++ throws "<token> already used" here, because the user can supply a fixed
	// token; same condition, reported rather than thrown.
	if sr_token in r.issued {
		return "", "", fmt.aprintf("%s already used", sr_token, allocator = allocator), false
	}
	r.issued[strings.clone(sr_token, allocator)] = cap

	return sr_token, restorer_sturdy_ref_str(r, sr_token, allocator), "", true
}

// C++: kj::Promise<void> Restorer::restore(RestoreContext context)
//
// restore @0 RestoreParams -> (cap :Capability), where
//   RestoreParams.localRef :SturdyRef.Token  = union { text :Text, data :Data }
//   RestoreParams.sealedBy :SturdyRef.Owner  = { guid :Text }
//
// A capnp_dyn.Handler rather than an Async_Handler: unlike run(), this answers
// out of a map without calling anything, so there is nothing to defer.
restorer_handle_call :: proc(
	user_data: rawptr,
	method_name: string,
	params: []capnp_dyn.Field,
) -> (
	result: []capnp_dyn.Field,
	err: string,
	ok: bool,
) {
	r := (^Restorer)(user_data)
	// Same reason as handle_call's: the shim's trampoline converts params with
	// the heap allocator and never frees them.
	defer free_params(params)

	if method_name != "restore" {
		return nil, fmt.tprintf("unimplemented method '%s'", method_name), false
	}

	// C++ reads sealedBy and passes it to getCapFromSRToken, which verifies the
	// signature. Nothing here can verify one, so rather than handing out an
	// unsealed capability to a client that asked for a sealed one, say so.
	if sealed_by, has_sealed := capnp_dyn.field_get(params, "sealedBy"); has_sealed {
		if fields, is_struct := sealed_by.([]capnp_dyn.Field); is_struct {
			if guid, has_guid := capnp_dyn.field_get(fields, "guid"); has_guid {
				if s, is_text := guid.(string); is_text && s != "" {
					return nil, "restore: sealed sturdy refs are not supported by this server", false
				}
			}
		}
	}

	token, token_ok := restore_token_of(params)
	if !token_ok {
		return nil, "restore: 'localRef' is missing or not a text token", false
	}

	sync.mutex_lock(&r.lock)
	cap, found := r.issued[token]
	sync.mutex_unlock(&r.lock)

	if !found {
		// C++: KJ_IF_MAYBE(cap, maybeCap) { context.getResults().setCap(*cap); } -
		// an unknown token leaves the result field entirely UNSET rather than
		// erroring, and the client sees a null capability that only fails once it
		// is actually called. Returning no fields at all is that: setting `cap` to
		// a nil Capability instead is NOT the same thing, and fails in the shim's
		// writer ("capability value has no capability payload") - which reaches the
		// client as a server error, not as a null cap.
		return nil, "", true
	}
	fields := make([]capnp_dyn.Field, 1, context.temp_allocator)
	fields[0] = {name = "cap", value = cap}
	return fields, "", true
}

// C++: params.getLocalRef() - a SturdyRef.Token union. Only the `text` arm is
// produced by saveStr (and by both client stacks), so only that one is accepted;
// `data` is the signed form, which goes with the sealing this server does not do.
@(private)
restore_token_of :: proc(params: []capnp_dyn.Field) -> (token: string, ok: bool) {
	local_ref, has_local_ref := capnp_dyn.field_get(params, "localRef")
	if !has_local_ref {
		return "", false
	}
	fields, is_struct := local_ref.([]capnp_dyn.Field)
	if !is_struct {
		return "", false
	}
	// The shim only reports the ACTIVE union member as a field, so "text" being
	// present is exactly "the token was sent as text".
	text, has_text := capnp_dyn.field_get(fields, "text")
	if !has_text {
		return "", false
	}
	s, is_text := text.(string)
	if !is_text {
		return "", false
	}
	return s, true
}
