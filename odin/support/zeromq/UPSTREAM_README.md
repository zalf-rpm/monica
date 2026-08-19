# Odin ZeroMQ (libzmq) Bindings

Minimal, low-level Odin bindings for ZeroMQ (libzmq). The package exposes the libzmq 4.3.5 C API (enums, structs, and functions) and includes a few convenience helpers for common tasks. Example PUB/SUB programs are provided.

## Requirements
- Odin compiler
- libzmq installed and discoverable by the system linker
  - macOS/Linux: links against `zmq`
  - Windows: links against `libzmq.lib`

### Installing libzmq
- macOS (Homebrew): `brew install zeromq`
- Ubuntu/Debian: `sudo apt-get install libzmq3-dev`
- Fedora: `sudo dnf install zeromq zeromq-devel`
- Windows: install ZeroMQ and ensure `libzmq.lib` is on your LIB path (e.g., via prebuilt binaries or vcpkg)

## Layout
- `zeromq/bindings*.odin`: Low-level FFI declarations for libzmq
- `zeromq/helpers.odin`: Small helpers (send/recv strings, errors, etc.)
- `examples/`: Tiny PUB/SUB demo programs

## Quick Start
Import the bindings into your Odin code (adjust the path to where `zeromq/` lives in your project):

```odin
import zmq "./zeromq" // or a relative/absolute path appropriate for your project
```

Create a publisher and send a message:

```odin
ctx := zmq.ctx_new()
defer zmq.ctx_term(ctx)

pub := zmq.socket(ctx, .PUB)
defer zmq.close(pub)

assert(zmq.bind(pub, "tcp://127.0.0.1:5555") == 0)

ok := zmq.send_string(pub, "hello from odin")
assert(ok)
```

Create a subscriber and receive a message:

```odin
ctx := zmq.ctx_new()
defer zmq.ctx_term(ctx)

sub := zmq.socket(ctx, .SUB)
defer zmq.close(sub)

assert(zmq.connect(sub, "tcp://127.0.0.1:5555") == 0)
// Subscribe to all topics (empty filter) or a specific prefix
ok := zmq.setsockopt_string(sub, .SUBSCRIBE, "")
assert(ok)

msg := zmq.recv_string(sub, allocator = context.temp_allocator)
fmt.println("got:", msg)
```

## Run the Examples
Two small PUB/SUB samples are included.

Example server (publisher):

```bash
odin run examples/server.odin -file
```

Example client (subscriber):

```bash
# Optional argument is the subscription prefix (default: "10001 ")
odin run examples/client.odin -file -- 10000
```

## Helpers
Convenience procs in `zeromq/helpers.odin`:
- `setsockopt_string`: set string socket options (e.g., `.SUBSCRIBE`)
- `send_string`, `send_string_more`, `send_empty`: convenience message senders
- `recv_string`, `recv_msg`: receive into a string or byte slice
- `print_zmq_error`: print last ZeroMQ error with a readable message

## Notes
- Target library version: libzmq 4.3.5 (see version macros in `zeromq/bindings.odin`).
- The bindings are intentionally thin; use the libzmq documentation for socket patterns and semantics.
- On macOS/Linux, the foreign import links to `system:zmq`; ensure your linker can find it.
- On Windows, ensure `libzmq.lib` is available to the linker.

## Links
- Bindgen used as a starting point: https://github.com/karl-zylinski/odin-c-bindgen
- Original Odin ZeroMQ bindings (outdated compiler support): https://github.com/zpl-zak/odin-zeromq
- libzmq: https://github.com/zeromq/libzmq
