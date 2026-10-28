# Plan: merge climate data by climate element (port of mas_cpp_misc 96be5bc)

Written 2026-10-28 from the C++ side, for whoever does this in the Odin port.
C++ reference: `mas_cpp_misc` commit `96be5bc` ("mergeClimateData: merge by
climate element, also partial and adding files") - files
`climate/climate-common.cpp` (`DataAccessor::mergeClimateData`),
`climate/climate-file-io.{h,cpp}`. monica commit `89598626` adds the test
cases. Not pushed yet as of writing.

## What changed in C++, and why

`"climate.csv"` in a sim.json can be a list of files; they are read one by
one and merged into the first (`readClimateDataFromCSVFilesViaHeaders` ->
`DataAccessor::mergeClimateData`). Before, the files after the first had to
contain the same climate elements: a file with just a `co2` column, or with
replacement temperatures for some years, didn't work.

New rules of `mergeClimateData(other, replaceOverlappingData)`:

1. Where the ranges overlap, other's climate elements replace this' (if
   `replaceOverlappingData`), also if other has just some of the elements.
   Elements only in this stay.
2. An element only in other is added if other covers the whole *resulting*
   range; else it is skipped (message to stdout).
3. The range is only extended (other starting earlier / ending later, incl.
   pure prepend/append) if other has *all* of this' elements - every element
   must cover the whole range. Otherwise the range stays and only the part of
   other within this' range is used (message).
4. A gap between the ranges: no change (as before).

Two bugs fixed on the way, both present in the Odin port too:

- other's data was read at this' column index (`other._data->at(index)` with
  this' `index`), wrong whenever the two have their elements in a different
  order or set. Odin: `odai := other.data[index]` in
  `data_accessor_merge_climate_data`. Now matched via each one's own
  `acd_to_data_index`.
- `_fromStep` (views from `cloneForRange`) was ignored. Now the data of both
  are read starting at their `from_step`.

The C++ also builds the merged data as new vectors instead of mutating the
shared `shared_ptr<VVD>` in place. The Odin port doesn't share storage (see
the NOTE at `Data_Accessor`), so that part needs no counterpart - just build
the new `data`/`acd_to_data_index`, then set `from_step = 0`,
`number_of_steps`, `start_date`, `end_date`.

CSV reader: `CSVViaHeaderOptions` got a runtime-only (not in the JSON)
`bool supplementary{false}`. `readClimateDataFromCSVFilesViaHeaders` reads
the first file with the given options and all later ones with a copy where
`supplementary = true`. In `readClimateDataFromCSVInputStream`, supplementary
files skip the per-line checks "one of [tmin, tmax, precip] is missing" and
"globrad and sunhours is missing" (and the `vsm.size() >= 5` assert). Also,
`tavg` is now only derived from tmin/tmax if both are present
(`!usedVs[tavg] && usedVs[tmin] && usedVs[tmax]`) - for normal files nothing
changes, as tmin/tmax are required there.

The messages name elements by their CSV header names (reverse of
`name2acd()`, first name per element: `tmin`, `precip`, `wind`, `co2`, ...).
Text (C++, stdout):

- `Can't extend climate data (<this range>) with (<other range>), because the
  new data lack <names>. Only the new data within (<this range>) will be used.`
- `Can't add climate element <name> from (<other range>), because it doesn't
  cover the whole time range (<result range>).`

## Where in the Odin port

- `odin/support/climate/climate_common.odin`: `data_accessor_merge_climate_data`
  - rewrite after the C++ (it is one straight-line algorithm now: gap check,
  decide the result range, then one loop over all climate elements).
- `odin/support/climate/climate_file_io.odin`:
  - `Csv_Via_Header_Options`: add `supplementary: bool` (not read from /
    written to JSON).
  - the stream reader's checks around line 454 (`tmin`/`tmax`/`precip`),
    459 (`tavg` derivation) and 471 (`globrad`): same conditions as above.
  - `read_climate_data_from_csv_files_via_headers`: pass a copy of the options
    with `supplementary = true` for every file after the first valid one
    (`data_accessor_is_valid(&final_da)`).
- Also check `odin/monica/capnp/run_monica_capnp.odin` and
  `odin/monica/run/serve_zmq.odin`, which call the files reader - nothing to
  change there if they go through `read_climate_data_from_csv_files_via_headers`.

## Testing

monica's `installer/Hohenfinow2` has two new suite cases (in its
`monica-test.toml`), both sim-min.json with a second climate file:

- `sim-min-co2.json`: `["climate-min.csv", "climate-min-co2.csv"]` - adds a
  `co2` column (450 ppm), which changes the results (CO2 from the climate
  data is used in `MonicaModel::step`).
- `sim-min-warmer.json`: `["climate-min.csv",
  "climate-min-warmer-1993-1994.csv"]` - tavg/tmin/tmax +2 °C for 1993-1994
  only; outputs first differ on 1993-01-01.

With monica's test runner (`tests/run.py --monica <odin monica-run>` and
`tests/compare.py`) the Odin outputs of both should be identical to the C++
ones. The C++ side was additionally checked against single files containing
the expected merged data (byte-identical outputs) for: adding co2, replacing
temperatures, replacing precip for part of the range, append and prepend of
complete files, co2 for only part of the range (not added), and extending
with a file lacking elements (not extended) - worth repeating in an Odin unit
test of `data_accessor_merge_climate_data`, e.g. with small hand-made
accessors, incl. two whose `acd_to_data_index` order differs.
