#!/usr/bin/python
# -*- coding: ISO-8859-15-*-

import zmq
#print zmq.pyzmq_version()
import time
import pprint

def collector():
  context = zmq.Context()
  socket = context.socket(zmq.PULL)
  socket.bind("tcp://127.0.0.1:7777")
  collector_data = []
  leave = False
  while !leave:
    result = socket.recv_json()
    collector_data.append(result)
    if result["type"] == "finish":
      leave = True
    
  print collector_data

collector()

