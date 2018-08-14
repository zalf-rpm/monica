#!/usr/bin/python
# -*- coding: UTF-8

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/. */

# Authors:
# Michael Berg-Mohnicke <michael.berg@zalf.de>
#
# Maintainers:
# Currently maintained by the authors.
#
# This file has been created at the Institute of
# Landscape Systems Analysis at the ZALF.
# Copyright (C: Leibniz Centre for Agricultural Landscape Research (ZALF)

import json
import sys
import zmq
import os
import monica_io

#print sys.path

#print "pyzmq version: ", zmq.pyzmq_version(), " zmq version: ", zmq.zmq_version()


def run_producer(server = {"server": None, "port": None}, shared_id = None):

  context = zmq.Context()
  socket = context.socket(zmq.PUSH)

  config = {
      "port": server["port"] if server["port"] else "6666",
      "server": server["server"] if server["server"] else "localhost",
      "sim.json": os.path.join(os.path.dirname(__file__), '../sim-min.json'),
      "crop.json": os.path.join(os.path.dirname(__file__), '../crop-min.json'),
      "site.json": os.path.join(os.path.dirname(__file__), '../site-min.json'),
      "climate.csv": os.path.join(os.path.dirname(__file__), '../climate-min.csv'),
      "shared_id": shared_id 
  }
  # read commandline args only if script is invoked directly from commandline
  if len(sys.argv) > 1 and __name__ == "__main__":
      for arg in sys.argv[1:]:
          k, v = arg.split("=")
          if k in config:
              config[k] = v

  print "config:", config
  
  socket.connect("tcp://" + config["server"] + ":" + config["port"])

  with open(config["sim.json"]) as _:
    sim_json = json.load(_)

  with open(config["site.json"]) as _:
    site_json = json.load(_)

  with open(config["crop.json"]) as _:
    crop_json = json.load(_)

  with open(config["climate.csv"]) as _:
    climate_csv = _.read()

  env = monica_io.create_env_json_from_json_config({
      "crop": crop_json,
      "site": site_json,
      "sim": sim_json,
      "climate": climate_csv
  })
  #print env

  # add shared ID if env to be sent to routable monicas
  if config["shared_id"]:
      env["sharedId"] = config["shared_id"]

  socket.send_json(env)

  print "done"


if __name__ == "__main__":
    run_producer()