# MONICA test suites

Runs simulation suites with a MONICA build and compares the results of
different builds - Linux against Windows, Nix against pixi, a new version
against an old one - and compares the simulations with measurements. Plain Python (>= 3.11, standard library only); `pixi run`
provides one.

## Running

    pixi run test --monica _cmake_release/monica-run --engine linux --out results/linux
    pixi run compare results/linux results/windows
    pixi run deviation results/master results/fix   # how much, for intended changes

`run.py` runs every simulation of every suite in parallel, each in its own
directory `<out>/<suite>/<sim>/` (`out.csv`, any further output files,
`monica.log`), and writes `<out>/metadata.json`: engine label, platform,
MONICA version and commit, the commits of monica-parameters and of each suite,
return codes and run times. Useful options:

- `--params DIR`: another monica-parameters checkout (default: the submodule),
  e.g. to see what a parameter change does
- `--only SUITE ...`: just these suites
- `--use SUITE=DIR`: a local directory instead of the suite's pinned source,
  e.g. to try out changes to a scenario repository before committing them
- `--monica` takes anything accepting `monica-run`'s arguments. For a Docker
  image: `MONICA_IMAGE=<image> pixi run test --monica tests/docker-monica-run.sh ...`

`compare.py` compares all output files section by section ("daily", "crop",
...), values numerically with the suite's tolerances. It prints a Markdown
report (also to the GitHub job summary in CI) listing, per column, how many
values differ, the largest absolute and relative difference and where it first
occurs, and exits with 1 if anything differs. It also notes when the two runs
used different inputs (MONICA source, parameters, suite commits).
Columns are matched by name: if the two builds write different column sets,
that is listed, and the columns both write are still compared. `--ignore
SUITE/SIM ...` skips simulations a build is known not to support (listed in
the report), e.g. `hohenfinow2/sim-min-ic` for a build without intercropping.

`deviation.py` is for changes that are *meant* to change results - a bug fix,
a parameter change, a new model version: not whether, but by how much. A
row-by-row comparison of daily values mostly measures timing there (a crop
maturing a day earlier shows its whole biomass as the difference on that
day), so it compares quantities that don't depend on single days:

- sections without daily rows (`crop`, `yearly`, `monthly`, events) value by
  value - per harvest, year or event - grouped by crop where there is a `Crop`
  column: mean and mean absolute relative change, range, change of the total,
  and where the largest change occurs
- daily time series (a `Date` column with one row per day), per simulation:
  the change of a column's mean over the run (equivalent to its total) and of
  its end value (the meaningful one for cumulative columns and pools)

Values that are 0 in A are counted separately, layer columns (`Mois_1`,
`Mois_2`, ...) are pooled unless `--per-layer` is given, and each suite gets
its own tables, largest changes first (`--top N`, `--all` also lists
unchanged columns). It also takes `--ignore` and always exits with 0.

## Simulated vs measured

    pixi run validate results/linux --out validation
    pixi run validate results/linux --out validation --baseline old/validation.json

`validate.py` compares the simulations with measurements, for the suites
whose `monica-test.toml` has `[[observed]]` entries (muencheberg_v140: yields
and soil organic carbon; microlysimeter: N and water leaching). It writes to
`--out`:

- `report.md`: per measured quantity a table with n, measured and simulated
  mean, bias (mean of sim − obs), MAE, RMSE, nRMSE (RMSE / measured mean), R²,
  Willmott's index of agreement d and the Nash-Sutcliffe efficiency NSE - for
  all measurements and per group (crop, treatment, lysimeter, ...)
- `report.html`: the same plus the plots - per simulation the simulated time
  series as a line and the measurements as dots (with error bars where there
  is a standard deviation), and simulated vs measured per group with the 1:1
  line - and the statistics per simulation
- `validation.json`: the statistics. Given as `--baseline` to a later run, the
  tables also show the change of the RMSE, e.g. before and after a change.

It reads the measurements from the suites at the commits the results were
made with (`--use SUITE=DIR` for a local directory). The plots need
matplotlib, which is why the task runs in pixi's `validate` environment;
`--no-plots` works with plain Python. It always exits with 0 (unless something
can't be read): the statistics are information, not a test.

## In CI

`.github/workflows/build.yml` builds MONICA with pixi (Linux, Windows), with
Nix and as the Docker image, runs the suites with each and compares all
results against the Nix build's, which is what the Docker image ships. The
results are kept as workflow artifacts for 14 days.

The `validate` job runs `validate.py` on the Nix build's results. The run
page shows the statistics, with the change of the RMSE since the previous
master build (whose `validation-json` artifact it downloads), and links the
plots: the `report.html` artifact, a single HTML file.

## Adding a suite

1. Put a `monica-test.toml` into the scenario directory or repository:

   ```toml
   # simulations to run, globs relative to this file
   sims = ["sim_files/sim*.json"]

   [compare]
   # exact equality unless a column below says otherwise
   default = { atol = 0.0, rtol = 0.0 }
   # SOC_0_30_g_m2 = { rtol = 1e-9 }   # per-column exceptions, with a reason

   # optional: measurements to compare the simulations with (validate.py)
   [[observed]]
   name = "Yield"                 # title in the report, key for --baseline
   file = "yield_measured.csv"    # CSV with a header row
   sim-map = "sim-map.csv"        # "sim" column + columns matched to the row,
   # sim = "*/{site}/sim-min"     #   or a pattern: {column} from the row, glob
   key = "year"                   # matched against output.key, or instead
   # date = "date"                #   a date column (YYYY-MM-DD) for daily output
   value = "yield_dm_kg_ha"
   # sd = "sd"                    # error bars
   unit = "kg DM/ha"
   group = "crop"                 # statistics per crop
   label = "T{treatment} {plot_no}"  # plot title per simulation
   output = { section = "crop", column = "Yield", key = "harv-year" }
   ```

   `output` can also have `scale` (factor for the simulated values) and
   `accumulate`: `"since-first"` sums the daily values after the first
   measurement date up to each measurement (cumulative measurements),
   `"since-previous"` since the previous measurement (amounts per sampling
   period). The details are at the top of `validate.py`; the
   [microlysimeter](https://github.com/zalf-rpm/microlysimeter) and
   [muencheberg_v140](https://github.com/zalf-rpm/muencheberg_v140)
   manifests are examples.

   The simulations' `sim.json` must use relative paths and
   `${MONICA_PARAMETERS}` (no machine-specific paths). Each runs in its own
   output directory and gets its main output file via `monica-run -o`.

2. Add it to `suites.toml`: `path` for a directory in this repository, or
   `git` and `rev` (a full commit hash) for a scenario repository. Updating a
   scenario means bumping its `rev`, so every MONICA commit is tested against
   fixed scenarios.
