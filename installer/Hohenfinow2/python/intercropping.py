#!/usr/bin/python
# -*- coding: UTF-8

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/. */

# Authors:
# Michael Berg-Mohnicke <michael.berg-mohnicke@zalf.de>
#
# Maintainers:
# Currently maintained by the authors.
#
# This file has been created at the Institute of
# Landscape Systems Analysis at the ZALF.
# Copyright (C: Leibniz Centre for Agricultural Landscape Research (ZALF)

import json
import os
from pathlib import Path
import subprocess as sp
import tempfile
import time
from threading import Thread

(fd, path) = tempfile.mkstemp(text=True)

channel = Thread(
    target=sp.run, 
    args=(["python", "/home/berg/GitHub/mas-infrastructure/src/python/common/channel.py", 
        "no_of_channels="+str(2), 
        "buffer_size="+str(2), 
        "use_async=true",
        "type_1=/home/berg/GitHub/mas-infrastructure/capnproto_schemas/model/monica/monica_state.capnp:ICData",
        "type_2=/home/berg/GitHub/mas-infrastructure/capnproto_schemas/model/monica/monica_state.capnp:ICData",
        "store_srs_file="+str(path)],),
    daemon=True
)
channel.start()

# wait for file to be written
time.sleep(2)

# load the sturdy refs from the file written by the channel service
srs = {}
with os.fdopen(fd) as _:
    srs = json.load(_)
print("read sturdy refs:", srs)

# start MONICA 1
mout_1 = open("out_1", "wt")
monica_1 = Thread(
    target=sp.run, 
    args=(["/home/berg/GitHub/monica/_cmake_linux_debug/monica-run", 
        "-icrsr", srs["reader_1"][0],
        "-icwsr", srs["writer_2"][0],
        "-c", "/home/berg/GitHub/monica/installer/Hohenfinow2/crop-inter_w.json",
        "/home/berg/GitHub/monica/installer/Hohenfinow2/sim-min.json"],),
    kwargs={
        "stdout": mout_1, 
        "text": True,
        "env": {"MONICA_PARAMETERS": "/home/berg/GitHub/monica-parameters"}
    }
)
monica_1.start()

# start MONICA 2
mout_2 = open("out_2", "wt")
monica_2 = Thread(
    target=sp.run,
    args=(["/home/berg/GitHub/monica/_cmake_linux_debug/monica-run", 
        "-icrsr", srs["reader_2"][0],
        "-icwsr", srs["writer_1"][0],
        "-c", "/home/berg/GitHub/monica/installer/Hohenfinow2/crop-inter_s.json",
        "/home/berg/GitHub/monica/installer/Hohenfinow2/sim-min.json"],),
    kwargs={
        "stdout": mout_2, 
        "text": True,
        "env": {"MONICA_PARAMETERS": "/home/berg/GitHub/monica-parameters"}
    },
)
monica_2.start()

# wait for MONICAs to finish
monica_1.join()
mout_1.close()
monica_2.join()
mout_2.close()




