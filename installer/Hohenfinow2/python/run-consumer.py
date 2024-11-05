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
# Copyright (C: Leibniz Centre for Agricultural Landscape Research (ZALF)

import sys
import csv
import os
import json
import zmq
from zalfmas_common.model import monica_io

def run_consumer(path_to_output_dir = None, leave_after_finished_run = True, server = {"server": None, "port": None}, shared_id = None):
    "collect data from workers"

    config = {
        "port": server["port"] if server["port"] else "7777",
        "server": server["server"] if server["server"] else "localhost", 
        "shared_id": shared_id,
        "out": path_to_output_dir if path_to_output_dir else os.path.join(os.path.dirname(__file__), './'),
        "leave_after_finished_run": leave_after_finished_run
    }
    if len(sys.argv) > 1 and __name__ == "__main__":
        for arg in sys.argv[1:]:
            k,v = arg.split("=")
            if k in config:
                if k == "leave_after_finished_run":
                    if (v == 'True' or v == 'true'): 
                        config[k] = True
                    else: 
                        config[k] = False
                else:
                    config[k] = v

    print("consumer config:", config)

    context = zmq.Context()
    if config["shared_id"]:
        socket = context.socket(zmq.DEALER)
        socket.setsockopt(zmq.IDENTITY, config["shared_id"])
    else:
        socket = context.socket(zmq.PULL)
    socket.connect("tcp://" + config["server"] + ":" + config["port"])

    #socket.RCVTIMEO = 1000
    leave = False

    def process_message(msg):

        if not hasattr(process_message, "wnof_count"):
            process_message.received_env_count = 0

        leave = False

        if msg["type"] == "finish":
            print("c: received finish message")
            leave = True

        else:
            print("c: received work result ", process_message.received_env_count, " customId: ", str(msg.get("customId", "")))

            process_message.received_env_count += 1

            # check if message has errors
            if msg.get("errors", []):
                print("c: received errors: ", msg["errors"])


            #with open("out/out-" + str(i) + ".csv", 'wb') as _:
            with open(config["out"] + str(process_message.received_env_count) + ".csv", 'w', newline='') as _:
                writer = csv.writer(_, delimiter=",")

                for data_ in msg.get("data", []):
                    results = data_.get("results", [])
                    orig_spec = data_.get("origSpec", "")
                    output_ids = data_.get("outputIds", [])

                    if len(results) > 0:
                        writer.writerow([orig_spec.replace("\"", "")])
                        for row in monica_io3.write_output_header_rows(output_ids,
                                                                      include_header_row=True,
                                                                      include_units_row=True,
                                                                      include_time_agg=False):
                            writer.writerow(row)

                        for row in monica_io.write_output(output_ids, results):
                            writer.writerow(row)

                    writer.writerow([])
            if config["leave_after_finished_run"] == True :
                leave = True

        return leave

    while not leave:
        try:
            msg = socket.recv_json()
            leave = process_message(msg)
        except:
            print(sys.exc_info())
            continue

    print("c: exiting run_consumer()")
    #debug_file.close()

if __name__ == "__main__":
    run_consumer()
    