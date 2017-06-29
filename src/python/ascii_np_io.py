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

import numpy as np

print "local ascii_np_io.py"

def read_ascii_grid_into_numpy_array(path_to_file, no_of_headerlines=6, extract_fn=lambda s: int(s), np_dtype=np.int32, nodata_value=-9999):
    "read an ascii grid into a map, without the no-data values"
    with open(path_to_file) as file_:
        nrows = 0
        ncols = 0
        row = -1
        arr = None
        skip_count = 0
        for line in file_:
            if skip_count < no_of_headerlines:
                skip_count += 1
                sline = filter(lambda s: len(s.strip()) > 0, line.split(" "))
                if len(sline) > 1:
                    key = sline[0].strip().upper()
                    if key == "NCOLS":
                        ncols = int(sline[1].strip())
                    elif key == "NROWS":
                        nrows = int(sline[1].strip())
                continue

            if skip_count == no_of_headerlines:
                arr = np.full((nrows, ncols), nodata_value, dtype=np_dtype)

            row += 1
            col = -1
            for col_str in line.strip().split(" "):
                col += 1
                if int(col_str) == -9999:
                    continue
                arr[row, col] = extract_fn(col_str)

        return arr
