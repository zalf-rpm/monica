/* SPDX-License-Identifier: MPL-2.0 */
/* *************************************************************************
    NOTE to contributors. This file comprises the principal public contract
    for ZeroMQ API users. Any change to this file supplied in a stable
    release SHOULD not break existing applications.
    In practice this means that the value of constants must not change, and
    that old values may not be reused for new constants.
    *************************************************************************
 */
// Vendored from https://github.com/qxuken/odin-zmq-bindings (libzmq 4.3.5),
// per plan-odin.md's ZeroMQ server phase. The upstream package expects
// "libzmq.lib" to already be on the linker's LIB path; this fork instead
// vendors the import lib at windows/libzmq.lib (built from vcpkg's
// zeromq:x64-windows port) so the build doesn't depend on machine-local
// linker search paths. The matching runtime DLL is windows/libzmq-mt-4_3_5.dll
// - copy it next to any built .exe that imports this package.
package zeromq

import "core:c"

when ODIN_OS == .Windows {
    foreign import lib "windows/libzmq.lib"
} else when ODIN_OS == .Linux || ODIN_OS == .Darwin {
    foreign import lib "system:zmq"
}

/* Version macros for compile-time API version detection */
VERSION_MAJOR :: 4
VERSION_MINOR :: 3
VERSION_PATCH :: 6
VERSION :: ((VERSION_MAJOR) * 10000 + (VERSION_MINOR) * 100 + (VERSION_PATCH))

/* Define integer types needed for event interface */
DEFINED_STDINT :: 1

/***************************************************************************** */
/* 0MQ errors. */
/***************************************************************************** */

/* A number random enough not to collide with different errno ranges on */
/* different OSes. The assumption is that error_t is at least 32-bit type. */
HAUSNUMERO :: 156384712

/* Native 0MQ error codes. */
EFSM :: (HAUSNUMERO + 51)
ENOCOMPATPROTO :: (HAUSNUMERO + 52)
ETERM :: (HAUSNUMERO + 53)
EMTHREAD :: (HAUSNUMERO + 54)

@(default_calling_convention = "c", link_prefix = "zmq_")
foreign lib {

    /* This function retrieves the errno as it is known to 0MQ library. The goal
       of this function is to make the code 100% portable, including where 0MQ
       compiled with certain CRT library (on Windows) is linked to an
       application that uses different CRT library. */
    errno :: proc() -> c.int ---

    /* Resolves system errors and 0MQ errors to human-readable string. */
    strerror :: proc(errnum: c.int) -> cstring ---

    /* Run-time API version detection */
    version :: proc(major: ^c.int, minor: ^c.int, patch: ^c.int) ---
}

/***************************************************************************** */
/* 0MQ infrastructure (a.k.a. context) initialisation & termination. */
/***************************************************************************** */

/* Context options */
Context_Option :: enum c.int {
    IO_THREADS                 = 1,
    MAX_SOCKETS                = 2,
    SOCKET_LIMIT               = 3,
    THREAD_PRIORITY            = 3,
    THREAD_SCHED_POLICY        = 4,
    MAX_MSGSZ                  = 5,
    MSG_T_SIZE                 = 6,
    THREAD_AFFINITY_CPU_ADD    = 7,
    THREAD_AFFINITY_CPU_REMOVE = 8,
    THREAD_NAME_PREFIX         = 9,
}

/* Default for new contexts */
IO_THREADS_DFLT :: 1
MAX_SOCKETS_DFLT :: 1023
THREAD_PRIORITY_DFLT :: -1
THREAD_SCHED_POLICY_DFLT :: -1

Context :: struct {}

@(default_calling_convention = "c", link_prefix = "zmq_")
foreign lib {
    ctx_new :: proc() -> ^Context ---
    ctx_term :: proc(ctx: ^Context) -> c.int ---
    ctx_shutdown :: proc(ctx: ^Context) -> c.int ---
    ctx_set :: proc(ctx: ^Context, option: Context_Option, optval: c.int) -> c.int ---
    ctx_get :: proc(ctx: ^Context, option: Context_Option) -> c.int ---

    /* Old (legacy) API */
    init :: proc(io_threads: c.int) -> ^Context ---
    /* Old (legacy) API */
    term :: proc(ctx: ^Context) -> c.int ---
    /* Old (legacy) API */
    ctx_destroy :: proc(ctx: ^Context) -> c.int ---
}

/* Some architectures, like sparc64 and some variants of aarch64, enforce pointer
 * alignment and raise sigbus on violations. Make sure applications allocate
 * zmq_msg_t on addresses aligned on a pointer-size boundary to avoid this issue.
 */
Message :: struct {
    _: [64]u8,
}

Free_Proc :: proc "c" (_: rawptr, _: rawptr)

Send_Recv_Options :: enum c.int {
    None     = 0,
    DONTWAIT = 1,
    // Deprecated
    NOBLOCK  = 1,
    SNDMORE  = 2,
}


@(default_calling_convention = "c", link_prefix = "zmq_")
foreign lib {
    msg_init :: proc(msg: ^Message) -> c.int ---
    msg_init_size :: proc(msg: ^Message, size: c.int) -> c.int ---
    msg_init_data :: proc(msg: ^Message, data: rawptr, size: c.int, ffn: ^Free_Proc, hint: rawptr) -> c.int ---
    msg_send :: proc(msg: ^Message, s: rawptr, flags: Send_Recv_Options) -> c.int ---
    msg_recv :: proc(msg: ^Message, s: rawptr, flags: Send_Recv_Options) -> c.int ---
    msg_close :: proc(msg: ^Message) -> c.int ---
    msg_move :: proc(dest: ^Message, src: ^Message) -> c.int ---
    msg_copy :: proc(dest: ^Message, src: ^Message) -> c.int ---
    msg_data :: proc(msg: ^Message) -> rawptr ---
    msg_size :: proc(msg: ^Message) -> c.int ---
    msg_more :: proc(msg: ^Message) -> c.bool ---
    msg_get :: proc(msg: ^Message, property: c.int) -> c.int ---
    msg_set :: proc(msg: ^Message, property: c.int, optval: c.int) -> c.int ---
    msg_gets :: proc(msg: ^Message, property: cstring) -> cstring ---

    msg_set_routing_id :: proc(msg: ^Message, routing_id: u32) -> c.int ---
    msg_routing_id :: proc(msg: ^Message) -> u32 ---
    msg_set_routing_group :: proc(msg: ^Message, group: cstring) -> c.int ---
    msg_routing_group :: proc(msg: ^Message) -> cstring ---
}

/***************************************************************************** */
/* 0MQ socket definition. */
/***************************************************************************** */

Socket_Type :: enum c.int {
    PAIR   = 0,
    PUB    = 1,
    SUB    = 2,
    REQ    = 3,
    REP    = 4,
    DEALER = 5,
    /* Deprecated aliases */
    XREQ   = 5,
    /* Deprecated aliases */
    ROUTER = 6,
    XREP   = 6,
    PULL   = 7,
    PUSH   = 8,
    XPUB   = 9,
    XSUB   = 10,
    Stream = 11,
}

Socket_Option :: enum c.int {
    AFFINITY                          = 4,
    ROUTING_ID                        = 5,
    // Deprecated
    IDENTITY                          = 5,
    SUBSCRIBE                         = 6,
    UNSUBSCRIBE                       = 7,
    RATE                              = 8,
    RECOVERY_IVL                      = 9,
    SNDBUF                            = 11,
    RCVBUF                            = 12,
    RCVMORE                           = 13,
    FD                                = 14,
    EVENTS                            = 15,
    TYPE                              = 16,
    LINGER                            = 17,
    RECONNECT_IVL                     = 18,
    BACKLOG                           = 19,
    RECONNECT_IVL_MAX                 = 21,
    MAXMSGSIZE                        = 22,
    SNDHWM                            = 23,
    RCVHWM                            = 24,
    MULTICAST_HOPS                    = 25,
    RCVTIMEO                          = 27,
    SNDTIMEO                          = 28,
    // Deprecated
    IPV4ONLY                          = 31,
    LAST_ENDPOINT                     = 32,
    ROUTER_MANDATORY                  = 33,
    // Deprecated
    FAIL_UNROUTABLE                   = 33,
    // Deprecated
    ROUTER_BEHAVIOR                   = 33,
    TCP_KEEPALIVE                     = 34,
    TCP_KEEPALIVE_CNT                 = 35,
    TCP_KEEPALIVE_IDLE                = 36,
    TCP_KEEPALIVE_INTVL               = 37,
    // Deprecated
    TCP_ACCEPT_FILTER                 = 38,
    IMMEDIATE                         = 39,
    // Deprecated
    DELAY_ATTACH_ON_CONNECT           = 39,
    XPUB_VERBOSE                      = 40,
    ROUTER_RAW                        = 41,
    IPV6                              = 42,
    MECHANISM                         = 43,
    PLAIN_SERVER                      = 44,
    PLAIN_USERNAME                    = 45,
    PLAIN_PASSWORD                    = 46,
    CURVE_SERVER                      = 47,
    CURVE_PUBLICKEY                   = 48,
    CURVE_SECRETKEY                   = 49,
    CURVE_SERVERKEY                   = 50,
    PROBE_ROUTER                      = 51,
    REQ_CORRELATE                     = 52,
    REQ_RELAXED                       = 53,
    CONFLATE                          = 54,
    ZAP_DOMAIN                        = 55,
    ROUTER_HANDOVER                   = 56,
    TOS                               = 57,
    // Deprecated
    IPC_FILTER_PID                    = 58,
    // Deprecated
    IPC_FILTER_UID                    = 59,
    // Deprecated
    IPC_FILTER_GID                    = 60,
    CONNECT_ROUTING_ID                = 61,
    // Deprecated
    CONNECT_RID                       = 61,
    GSSAPI_SERVER                     = 62,
    GSSAPI_PRINCIPAL                  = 63,
    GSSAPI_SERVICE_PRINCIPAL          = 64,
    GSSAPI_PLAINTEXT                  = 65,
    HANDSHAKE_IVL                     = 66,
    SOCKS_PROXY                       = 68,
    XPUB_NODROP                       = 69,
    BLOCKY                            = 70,
    XPUB_MANUAL                       = 71,
    XPUB_WELCOME_MSG                  = 72,
    STREAM_NOTIFY                     = 73,
    INVERT_MATCHING                   = 74,
    HEARTBEAT_IVL                     = 75,
    HEARTBEAT_TTL                     = 76,
    HEARTBEAT_TIMEOUT                 = 77,
    XPUB_VERBOSER                     = 78,
    CONNECT_TIMEOUT                   = 79,
    TCP_MAXRT                         = 80,
    THREAD_SAFE                       = 81,
    MULTICAST_MAXTPDU                 = 84,
    VMCI_BUFFER_SIZE                  = 85,
    VMCI_BUFFER_MIN_SIZE              = 86,
    VMCI_BUFFER_MAX_SIZE              = 87,
    VMCI_CONNECT_TIMEOUT              = 88,
    USE_FD                            = 89,
    GSSAPI_PRINCIPAL_NAMETYPE         = 90,
    GSSAPI_SERVICE_PRINCIPAL_NAMETYPE = 91,
    BINDTODEVICE                      = 92,
}

/* Message options */
MORE :: 1
SHARED :: 3

/* Security mechanisms */
NULL :: 0
PLAIN :: 1
CURVE :: 2
GSSAPI :: 3

/* RADIO-DISH protocol */
GROUP_MAX_LENGTH :: 255

/* Deprecated Message options */
SRCFD :: 2

/***************************************************************************** */
/* GSSAPI definitions */
/***************************************************************************** */

/* GSSAPI principal name types */
GSSAPI_NT_HOSTBASED :: 0
GSSAPI_NT_USER_NAME :: 1
GSSAPI_NT_KRB5_PRINCIPAL :: 2

/***************************************************************************** */
/* 0MQ socket events and monitoring */
/***************************************************************************** */

/* Socket transport events (TCP, IPC and TIPC only) */
EVENT_CONNECTED :: 0x0001
EVENT_CONNECT_DELAYED :: 0x0002
EVENT_CONNECT_RETRIED :: 0x0004
EVENT_LISTENING :: 0x0008
EVENT_BIND_FAILED :: 0x0010
EVENT_ACCEPTED :: 0x0020
EVENT_ACCEPT_FAILED :: 0x0040
EVENT_CLOSED :: 0x0080
EVENT_CLOSE_FAILED :: 0x0100
EVENT_DISCONNECTED :: 0x0200
EVENT_MONITOR_STOPPED :: 0x0400
EVENT_ALL :: 0xFFFF

/* Unspecified system errors during handshake. Event value is an errno. */
EVENT_HANDSHAKE_FAILED_NO_DETAIL :: 0x0800

/* Handshake complete successfully with successful authentication (if        *
 *  enabled). Event value is unused. */
EVENT_HANDSHAKE_SUCCEEDED :: 0x1000

/* Protocol errors between ZMTP peers or between server and ZAP handler.     *
 *  Event value is one of PROTOCOL_ERROR_* */
EVENT_HANDSHAKE_FAILED_PROTOCOL :: 0x2000

/* Failed authentication requests. Event value is the numeric ZAP status     *
 *  code, i.e. 300, 400 or 500. */
EVENT_HANDSHAKE_FAILED_AUTH :: 0x4000
PROTOCOL_ERROR_ZMTP_UNSPECIFIED :: 0x10000000
PROTOCOL_ERROR_ZMTP_UNEXPECTED_COMMAND :: 0x10000001
PROTOCOL_ERROR_ZMTP_INVALID_SEQUENCE :: 0x10000002
PROTOCOL_ERROR_ZMTP_KEY_EXCHANGE :: 0x10000003
PROTOCOL_ERROR_ZMTP_MALFORMED_COMMAND_UNSPECIFIED :: 0x10000011
PROTOCOL_ERROR_ZMTP_MALFORMED_COMMAND_MESSAGE :: 0x10000012
PROTOCOL_ERROR_ZMTP_MALFORMED_COMMAND_HELLO :: 0x10000013
PROTOCOL_ERROR_ZMTP_MALFORMED_COMMAND_INITIATE :: 0x10000014
PROTOCOL_ERROR_ZMTP_MALFORMED_COMMAND_ERROR :: 0x10000015
PROTOCOL_ERROR_ZMTP_MALFORMED_COMMAND_READY :: 0x10000016
PROTOCOL_ERROR_ZMTP_MALFORMED_COMMAND_WELCOME :: 0x10000017
PROTOCOL_ERROR_ZMTP_INVALID_METADATA :: 0x10000018

// the following two may be due to erroneous configuration of a peer
PROTOCOL_ERROR_ZMTP_CRYPTOGRAPHIC :: 0x11000001
PROTOCOL_ERROR_ZMTP_MECHANISM_MISMATCH :: 0x11000002
PROTOCOL_ERROR_ZAP_UNSPECIFIED :: 0x20000000
PROTOCOL_ERROR_ZAP_MALFORMED_REPLY :: 0x20000001
PROTOCOL_ERROR_ZAP_BAD_REQUEST_ID :: 0x20000002
PROTOCOL_ERROR_ZAP_BAD_VERSION :: 0x20000003
PROTOCOL_ERROR_ZAP_INVALID_STATUS_CODE :: 0x20000004
PROTOCOL_ERROR_ZAP_INVALID_METADATA :: 0x20000005
PROTOCOL_ERROR_WS_UNSPECIFIED :: 0x30000000

Socket :: struct {}

@(default_calling_convention = "c", link_prefix = "zmq_")
foreign lib {
    socket :: proc(s: ^Context, type: Socket_Type) -> ^Socket ---
    close :: proc(s: ^Socket) -> c.int ---
    setsockopt :: proc(s: ^Socket, option: Socket_Option, optval: rawptr, optvallen: c.int) -> c.int ---
    getsockopt :: proc(s: ^Socket, option: Socket_Option, optval: rawptr, optvallen: ^c.int) -> c.int ---
    bind :: proc(s: ^Socket, addr: cstring) -> c.int ---
    connect :: proc(s: ^Socket, addr: cstring) -> c.int ---
    unbind :: proc(s: ^Socket, addr: cstring) -> c.int ---
    disconnect :: proc(s: ^Socket, addr: cstring) -> c.int ---
    send :: proc(s: ^Socket, buf: rawptr, len: c.int, flags: Send_Recv_Options) -> c.int ---
    send_const :: proc(s: ^Socket, buf: rawptr, len: c.int, flags: Send_Recv_Options) -> c.int ---
    recv :: proc(s: ^Socket, buf: rawptr, len: c.int, flags: Send_Recv_Options) -> c.int ---
    socket_monitor :: proc(s: ^Socket, addr: cstring, events: c.int) -> c.int ---

    join :: proc(s: ^Socket, group: cstring) -> c.int ---
    leave :: proc(s: ^Socket, group: cstring) -> c.int ---

    connect_peer :: proc(s: ^Socket, addr: cstring) -> u32 ---
}

/***************************************************************************** */
/* Deprecated I/O multiplexing. Prefer using zmq_poller API */
/***************************************************************************** */
Poll_Event :: enum c.int {
    NONE    = 0,
    POLLIN  = 1,
    POLLOUT = 2,
    POLLERR = 4,
    POLLPRI = 8,
}

// FIX (upstream qxuken/odin-zmq-bindings bug): zmq.h's zmq_pollitem_t /
// zmq_poller_event_t declare `events`/`revents` as `short` (2 bytes) on every
// platform, and `fd` as the platform socket handle - `SOCKET` (an 8-byte
// UINT_PTR) on Windows, plain `int` elsewhere. The upstream binding used
// `c.int` for both `FD` and the events fields, which silently misaligns every
// field after `socket` in the struct (on Windows: fd is 4 bytes short, so
// events/revents land 4 bytes into what libzmq considers padding) - zmq_poll
// then reads garbage revents and poll() never reports POLLIN, hanging
// serve_zmq_monica_full forever waiting on a message that already arrived.
when ODIN_OS == .Windows {
    FD :: uintptr // SOCKET
} else {
    FD :: c.int
}
Poll_Item :: struct {
    socket:  ^Socket,
    fd:      FD,
    events:  i16,
    revents: i16,
}
Poller :: struct {}
Poller_Event :: struct {
    socket:    ^Socket,
    fd:        FD,
    user_data: rawptr,
    events:    i16,
}

POLLITEMS_DFLT :: 16

@(default_calling_convention = "c", link_prefix = "zmq_")
foreign lib {
    poll :: proc(items: ^Poll_Item, nitems: c.int, timeout: c.int) -> c.int ---

    poller_new :: proc() -> ^Poller ---
    poller_destroy :: proc(p: ^^Poller) -> c.int ---
    poller_fd :: proc(p: ^Poller, fd: FD) -> c.int ---
    poller_size :: proc(p: ^^Poller) -> c.int ---
    poller_add :: proc(p: ^Poller, socket: ^Socket, user_data: rawptr, events: Poll_Event) -> c.int ---
    poller_modify :: proc(p: ^Poller, socket: ^Socket, events: Poll_Event) -> c.int ---
    poller_remove :: proc(p: ^Poller, socket: ^Socket) -> c.int ---
    poller_wait :: proc(p: ^Poller, pe: ^Poller_Event, timeout: c.long) -> c.int ---
    poller_wait_all :: proc(p: ^Poller, pe: ^Poller_Event, n_events: c.int, timeout: c.long) -> c.int ---
    poller_add_fd :: proc(p: ^Poller, fd: FD, user_data: rawptr, events: Poll_Event) -> c.int ---
    poller_modify_fd :: proc(p: ^Poller, fd: FD, events: Poll_Event) -> c.int ---
    poller_remove_fd :: proc(p: ^Poller, fd: FD) -> c.int ---
    socket_get_peer_state :: proc(socket: ^Socket, routing_id: rawptr, routing_id_size: uint) -> c.int ---
    /* Message proxying */
    proxy :: proc(frontend: ^Socket, backend: ^Socket, capture: rawptr) -> c.int ---
    proxy_steerable :: proc(frontend: ^Socket, backend: ^Socket, capture: rawptr, control: rawptr) -> c.int ---
}

/***************************************************************************** */
/* Probe library capabilities */
/***************************************************************************** */
HAS_CAPABILITIES :: 1

@(default_calling_convention = "c", link_prefix = "zmq_")
foreign lib {
    has :: proc(capability: cstring) -> c.int ---
}

/* Deprecated aliases */
STREAMER :: 1
FORWARDER :: 2
QUEUE :: 3

@(default_calling_convention = "c", link_prefix = "zmq_")
foreign lib {

    /* Deprecated methods */
    device :: proc(type: c.int, frontend: ^Socket, backend: ^Socket) -> c.int ---
    /* Deprecated methods */
    sendmsg :: proc(s: ^Socket, msg: ^Message, flags: Send_Recv_Options) -> c.int ---
    /* Deprecated methods */
    recvmsg :: proc(s: ^Socket, msg: ^Message, flags: Send_Recv_Options) -> c.int ---
}

Atomic_Counter :: struct {}
IO_Vec :: struct {}
@(default_calling_convention = "c", link_prefix = "zmq_")
foreign lib {
    sendiov :: proc(s: ^Socket, iov: ^IO_Vec, count: c.int, flags: c.int) -> c.int ---
    recviov :: proc(s: ^Socket, iov: ^IO_Vec, count: ^c.int, flags: c.int) -> c.int ---

    /* Encode data with Z85 encoding. Returns encoded data */
    z85_encode :: proc(dest: ^i8, data: ^c.int, size: c.int) -> ^i8 ---

    /* Decode data with Z85 encoding. Returns decoded data */
    z85_decode :: proc(dest: ^c.int, string: cstring) -> ^c.int ---

    /* Generate z85-encoded public and private keypair with libsodium. */
    /* Returns 0 on success. */
    curve_keypair :: proc(z85_public_key: ^i8, z85_secret_key: ^i8) -> c.int ---

    /* Derive the z85-encoded public key from the z85-encoded secret key. */
    /* Returns 0 on success. */
    curve_public :: proc(z85_public_key: ^i8, z85_secret_key: cstring) -> c.int ---

    /***************************************************************************** */
    /* Atomic utility methods */
    /***************************************************************************** */
    atomic_counter_new :: proc() -> ^Atomic_Counter ---
    atomic_counter_set :: proc(counter: ^Atomic_Counter, value: c.int) ---
    atomic_counter_inc :: proc(counter: ^Atomic_Counter) -> c.int ---
    atomic_counter_dec :: proc(counter: ^Atomic_Counter) -> c.int ---
    atomic_counter_value :: proc(counter: ^Atomic_Counter) -> c.int ---
    atomic_counter_destroy :: proc(counter_p: ^^Atomic_Counter) ---
}

Timer :: struct {}
Stopwatch :: struct {}
Timer_Proc :: proc "c" (_: c.int, _: rawptr)

@(default_calling_convention = "c", link_prefix = "zmq_")
foreign lib {
    timers_new :: proc() -> ^Timer ---
    timers_destroy :: proc(timers_p: ^^Timer) -> c.int ---
    timers_add :: proc(timers: ^Timer, interval: c.int, handler: Timer_Proc, arg: rawptr) -> c.int ---
    timers_cancel :: proc(timers: ^Timer, timer_id: c.int) -> c.int ---
    timers_set_interval :: proc(timers: ^Timer, timer_id: c.int, interval: c.int) -> c.int ---
    timers_reset :: proc(timers: ^Timer, timer_id: c.int) -> c.int ---
    timers_timeout :: proc(timers: ^Timer) -> c.int ---
    timers_execute :: proc(timers: ^Timer) -> c.int ---

    /* Starts the stopwatch. Returns the handle to the watch. */
    stopwatch_start :: proc() -> ^Stopwatch ---

    /* Returns the number of microseconds elapsed since the stopwatch was */
    /* started, but does not stop or deallocate the stopwatch. */
    stopwatch_intermediate :: proc(watch: ^Stopwatch) -> c.int ---

    /* Stops the stopwatch. Returns the number of microseconds elapsed since */
    /* the stopwatch was started, and deallocates that watch. */
    stopwatch_stop :: proc(watch: ^Stopwatch) -> c.int ---

    /* Sleeps for specified number of seconds. */
    sleep :: proc(seconds: c.int) ---
}
Thread_Proc :: proc "c" (_: rawptr)

@(default_calling_convention = "c", link_prefix = "zmq_")
foreign lib {

    /* Start a thread. Returns a handle to the thread. */
    threadstart :: proc(func: ^Thread_Proc, arg: rawptr) -> rawptr ---

    /* Wait for thread to complete then free up resources. */
    threadclose :: proc(thread: rawptr) ---
}
