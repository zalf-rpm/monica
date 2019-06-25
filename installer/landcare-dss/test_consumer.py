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

import sys
#sys.path.insert(0, "C:\\Users\\berg.ZALF-AD\\GitHub\\monica\\project-files\\Win32\\Release")
#sys.path.insert(0, "C:\\Users\\berg.ZALF-AD\\GitHub\\monica\\src\\python")
#sys.path.insert(0, "C:\\Program Files (x86)\\MONICA")
#print sys.path

import gc
import csv
import types
import os
import json
from datetime import datetime
from collections import defaultdict, OrderedDict
import numpy as np

import zmq
#print "pyzmq version: ", zmq.pyzmq_version(), " zmq version: ", zmq.zmq_version()

import monica_io
#print "path to monica_io: ", monica_io.__file__

def run_consumer():
    "collect data from workers"

    config = {
    }
    if len(sys.argv) > 1:
        for arg in sys.argv[1:]:
            k,v = arg.split("=")
            if k in config:
                config[k] = v

    print "consumer config:", config

    received_env_count = 1
    context = zmq.Context()
    socket = context.socket(zmq.DEALER)
    socket.setsockopt(zmq.IDENTITY, "aaa")
    socket.connect("tcp://cluster1:77774")

    socket.RCVTIMEO = 1000
    leave = False

    while not leave:

        try:
            result = socket.recv_json(encoding="latin-1")
        except:
            continue

        print "received work result ", received_env_count, " customId: ", result.get("customId", "")
        
        with open("out-" + str(received_env_count) + ".csv", 'wb') as _:
            writer = csv.writer(_, delimiter=";")

            for data_ in result.get("data", []):
                results = data_.get("results", [])
                orig_spec = data_.get("origSpec", "")
                output_ids = data_.get("outputIds", [])

                if len(results) > 0:
                    writer.writerow([orig_spec.replace("\"", "")])
                    for row in monica_io.write_output_header_rows(output_ids,
                                                                    include_header_row=True,
                                                                    include_units_row=True,
                                                                    include_time_agg=False):
                        writer.writerow(row)

                    for row in monica_io.write_output(output_ids, results):
                        writer.writerow(row)

                writer.writerow([])

        received_env_count = received_env_count + 1

    print "exiting run_consumer()"
    #debug_file.close()

if __name__ == "__main__":
    run_consumer()
#main()


