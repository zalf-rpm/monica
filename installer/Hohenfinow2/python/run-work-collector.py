import zmq
#print zmq.pyzmq_version()
import time
import pprint

def collector():
  context = zmq.Context()
  socket = context.socket(zmq.PULL)
  socket.bind("tcp://127.0.0.1:7777")
  collector_data = {}
  leave = False
  while not leave:
    result = socket.recv_json()
    if result["type"] == "finish":
      leave = True
    else:
      c = result["crop"]
      collector_data[c["Crop"]] += c["Yield"]
    print "received message"
    print "intermediate: ", collector_data
    
  print collector_data

collector()

