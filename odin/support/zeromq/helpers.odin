package zeromq

import "core:c"
import "core:fmt"
import "core:mem"

zmq_error_cstring :: proc(err_number: Maybe(c.int) = nil) -> cstring {
    return strerror(err_number.(c.int) or_else errno())
}

print_zmq_error :: proc() {
    code := errno()
    fmt.eprintfln("Error(%v): %v", code, zmq_error_cstring(code))
}

setsockopt_string :: proc(s: ^Socket, option: Socket_Option, value: string) -> (ok: bool) {
    return setsockopt(s, option, raw_data(value), cast(c.int)len(value)) == 0
}

setsockopt_int :: proc(s: ^Socket, option: Socket_Option, value: c.int) -> (ok: bool) {
    value := value
    return setsockopt(s, option, &value, size_of(c.int)) == 0
}

recv_msg :: proc(s: ^Socket, allocator := context.allocator) -> (data: []byte, more: bool) {
    msg := Message{}
    rc := msg_init(&msg)
    assert(rc == 0)
    defer msg_close(&msg)
    size := msg_recv(&msg, s, .None)
    more = msg_more(&msg)
    if size == -1 do return
    data = make([]byte, size, allocator = allocator)
    mem.copy(raw_data(data), msg_data(&msg), cast(int)size)
    return
}

recv_string :: proc(s: ^Socket, allocator := context.allocator) -> (data: string, more: bool) {
    buf, has_more := recv_msg(s, allocator = allocator)
    return string(buf), has_more
}

send_empty :: proc(s: ^Socket, opt: Send_Recv_Options = .None) -> (ok: bool) {
    msg := Message{}
    rc := msg_init(&msg)
    assert(rc == 0)
    defer msg_close(&msg)
    return msg_send(&msg, s, opt) == 0
}

send_msg :: proc(s: ^Socket, buf: []byte, opt: Send_Recv_Options = .None) -> (ok: bool) {
    msg := Message{}
    rc := msg_init_size(&msg, cast(c.int)len(buf))
    assert(rc == 0)
    defer msg_close(&msg)
    mem.copy(msg_data(&msg), raw_data(buf), len(buf))
    return msg_send(&msg, s, opt) == cast(i32)len(buf)
}

send_string :: proc(s: ^Socket, val: string, opt: Send_Recv_Options = .None) -> (ok: bool) {
    return send_msg(s, transmute([]byte)val, opt)
}

send_string_more :: proc(s: ^Socket, val: string) -> (ok: bool) {
    return send_msg(s, transmute([]byte)val, .SNDMORE)
}
