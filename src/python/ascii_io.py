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

print "local ascii_io.py"

def read_ascii_grid(path_to_file, include_no_data=False, row_offset=0, col_offset=0):
    "read an ascii grid into a map, without the no-data values"
    with open(path_to_file) as file_:
        data = {}
        #skip the header (first 6 lines)
        for _ in range(0, 6):
            file_.next()
        row = 0
        for line in file_:
            col = 0
            for col_str in line.strip().split(" "):
                if not include_no_data and int(col_str) == -9999:
                    continue
                data[(row_offset+row, col_offset+col)] = int(col_str)
                col += 1
            row += 1
        return data
