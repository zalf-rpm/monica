using ZMQ
using JSON



ctx = Context()
s1 = Socket(ctx, REP)
s2 = Socket(ctx, REQ)

ZMQ.bind(s1, "tcp://*:5555")
ZMQ.connect(s2, "tcp://localhost:5555")

ZMQ.send(s2, Message("test request"))
msg = ZMQ.recv(s1)
out = convert(IOStream, msg)
seek(out, 0)

ZMQ.send(s1, Message("test response"))
ZMQ.close(s1)
ZMQ.close(s2)
ZMQ.close(ctx)
