import zmq

context = zmq.Context.instance()
socket = context.socket(zmq.DEALER)
socket.setsockopt(zmq.IDENTITY, "{da6c4e68-2374-4ac1-a376-397a85696690}")
socket.connect("tcp://cluster1:77774")

while True:
	msg = socket.recv_json()
	print msg

