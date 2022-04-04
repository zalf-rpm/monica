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

import asyncio
#import capnp
import json
import os
from pathlib import Path
import subprocess as sp
import sys
import tempfile
import time
from threading import Thread, Lock
import uuid

#PATH_TO_SCRIPT_DIR = Path(os.path.realpath(__file__)).parent
#PATH_TO_REPO = PATH_TO_SCRIPT_DIR.parent.parent.parent
#if str(PATH_TO_REPO) not in sys.path:
#    sys.path.insert(1, str(PATH_TO_REPO))

#PATH_TO_PYTHON_CODE = PATH_TO_REPO / "src/python"
#if str(PATH_TO_PYTHON_CODE) not in sys.path:
#    sys.path.insert(1, str(PATH_TO_PYTHON_CODE))

#if str(PATH_TO_SCRIPT_DIR) in sys.path:
#    import common as common
#    import service as serv
#    import capnp_async_helpers as async_helpers
#else:
#    import common.common as common
#    import common.service as serv
#    import common.capnp_async_helpers as async_helpers

#PATH_TO_CAPNP_SCHEMAS = PATH_TO_REPO / "capnproto_schemas"
#abs_imports = [str(PATH_TO_CAPNP_SCHEMAS)]
#reg_capnp = capnp.load(str(PATH_TO_CAPNP_SCHEMAS / "registry.capnp"), imports=abs_imports)
#config_capnp = capnp.load(str(PATH_TO_CAPNP_SCHEMAS / "config.capnp"), imports=abs_imports)

(fd, path) = tempfile.mkstemp(text=True)

channel = Thread(
    target=sp.run, 
    args=(["python", "/home/berg/GitHub/mas-infrastructure/src/python/common/channel.py", 
        "no_of_channels="+str(2), "buffer_size="+str(2), 
        "type_1=/home/berg/GitHub/mas-infrastructure/capnproto_schemas/model/monica/monica_state.capnp:ICData",
        "type_1=/home/berg/GitHub/mas-infrastructure/capnproto_schemas/model/monica/monica_state.capnp:ICData",
        "store_srs_file="+str(path)],),
    daemon=True
)
channel.start()

# wait for file to be written
time.sleep(2)

srs = {}
with os.fdopen(fd) as _:
    srs = json.load(_)

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

monica_1.join()
mout_1.close()
monica_2.join()
mout_2.close()




