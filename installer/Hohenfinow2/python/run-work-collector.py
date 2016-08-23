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
      print "received finish message"
      leave = True
    else:
      print "received work results"
      c = result["crop"]
      print c
      for cropName, data in c.iteritems(): 
        collector_data[cropName] = collector_data.setdefault(cropName, 0) + data[2]
    print "intermediate results: ", collector_data
    
  print "final results: ", collector_data

collector()

