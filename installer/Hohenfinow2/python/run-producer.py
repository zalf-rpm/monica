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
import monica_io3
import errno

#print(sys.path)

#print("pyzmq version: ", zmq.pyzmq_version(), " zmq version: ", zmq.zmq_version())


def run_producer(server = {"server": None, "port": None}, shared_id = None):

    context = zmq.Context()
    socket = context.socket(zmq.PUSH) # pylint: disable=no-member

    config = {
        "port": server["port"] if server["port"] else "6666",
        "server": server["server"] if server["server"] else "localhost",
        "sim.json": os.path.join(os.path.dirname(__file__), '../sim-min.json'),
        "crop.json": os.path.join(os.path.dirname(__file__), '../crop-min.json'),
        "site.json": os.path.join(os.path.dirname(__file__), '../site-min.json'),
        "climate.csv": os.path.abspath(os.path.join(os.path.dirname(__file__), '../climate-min.csv')),
        "debugout": "debug_out",
        "writenv": True,
        "shared_id": shared_id 
    }
    # read commandline args only if script is invoked directly from commandline
    if len(sys.argv) > 1 and __name__ == "__main__":
        for arg in sys.argv[1:]:
            k, v = arg.split("=")
            if k in config:
                if k == "writenv" :
                    config[k] = bool(v)
                else :
                    config[k] = v

    print("config:", config)
    
    socket.connect("tcp://" + config["server"] + ":" + config["port"])

    with open(config["sim.json"]) as _:
        sim_json = json.load(_)

    with open(config["site.json"]) as _:
        site_json = json.load(_)

    with open(config["crop.json"]) as _:
        crop_json = json.load(_)

    env = monica_io3.create_env_json_from_json_config({
        "crop": crop_json,
        "site": site_json,
        "sim": sim_json,
        "climate": "" #climate_csv
    })
    env["csvViaHeaderOptions"] = sim_json["climate.csv-options"]
    env["pathToClimateCSV"] = config["climate.csv"]

    # add shared ID if env to be sent to routable monicas
    if config["shared_id"]:
        env["sharedId"] = config["shared_id"]

    if config["writenv"] :
        filename = os.path.join(os.path.dirname(__file__), config["debugout"], 'generated_env.json')
        WriteEnv(filename, env) 

    socket.send_json(env)

    print("done")

def WriteEnv(filename, env) :
    if not os.path.exists(os.path.dirname(filename)):
        try:
            os.makedirs(os.path.dirname(filename))
        except OSError as exc: # Guard against race condition
            if exc.errno != errno.EEXIST:
                raise
    with open(filename, 'w') as outfile:
        json.dump(env, outfile)

if __name__ == "__main__":
    run_producer()