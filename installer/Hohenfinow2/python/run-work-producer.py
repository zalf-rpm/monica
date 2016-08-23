#!/usr/bin/python
# -*- coding: ISO-8859-15-*-

import zmq
import time
import json
import types
import sys
#sys.path.append('C:/Users/berg.ZALF-AD/GitHub/monica/project-files/Win32/Release')	 # path to monica_python.pyd or monica_python.so
#sys.path.append('C:/Users/berg.ZALF-AD/GitHub/util/soil')
#from soil_conversion import *
import monica_python
from env_json_from_json_config import *

#print "pyzmq version: ", zmq.pyzmq_version()
#print "sys.path: ", sys.path
#print "sys.version: ", sys.version

def main():
	with open("../sim.json") as f:
		sim = json.load(f)

	with open("../site.json") as f:
		site = json.load(f)

	with open("../crop.json") as f:
		crop = json.load(f)

	sim["include-file-base-path"] = "../../../../"
	sim["climate.csv"] = "../climate.csv"

	env = createEnvJsonFromJsonConfig({"crop": crop, "site": site, "sim": sim})
	#print env

	producer(env)


def producer(env):
	context = zmq.Context()
	socket = context.socket(zmq.PUSH)
	socket.bind("tcp://127.0.0.1:5560")

	consumerSocket = context.socket(zmq.PUSH)
	consumerSocket.connect("tcp://127.0.0.1:7777");
	
	workerControlSocket = context.socket(zmq.PUB)
	consumerSocket.bind("tcp://127.0.0.1:6666");

	time.sleep(2)

	for i in range(1):
		socket.send_json(env)

	# Start your result manager and workers before you start your producers
	
	time.sleep(10)
	#workerControlSocket.send("finish " + json.dumps({"type": "finish"}))
	#consumerSocket.send_json({"type": "finish"})



main()