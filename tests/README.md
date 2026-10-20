# MONICA test suites

Runs simulation suites with a MONICA build and compares the results of
different builds - Linux against Windows, Nix against pixi, a new version
against an old one. Plain Python (>= 3.11, standard library only); `pixi run`
provides one.

## Running

    pixi run test --monica _cmake_release/monica-run --engine linux --out results/linux
    pixi run compare results/linux results/windows

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

## In CI

`.github/workflows/build.yml` builds MONICA with pixi (Linux, Windows), with
Nix and as the Docker image, runs the suites with each and compares all
results against the Nix build's, which is what the Docker image ships. The
results are kept as workflow artifacts for 14 days.

## Adding a suite

1. Put a `monica-test.toml` into the scenario directory or repository:

   ```toml
   # simulations to run, globs relative to this file
   sims = ["sim_files/sim*.json"]

   [compare]
   # exact equality unless a column below says otherwise
   default = { atol = 0.0, rtol = 0.0 }
   # SOC_0_30_g_m2 = { rtol = 1e-9 }   # per-column exceptions, with a reason
   ```

   The simulations' `sim.json` must use relative paths and
   `${MONICA_PARAMETERS}` (no machine-specific paths). Each runs in its own
   output directory and gets its main output file via `monica-run -o`.

2. Add it to `suites.toml`: `path` for a directory in this repository, or
   `git` and `rev` (a full commit hash) for a scenario repository. Updating a
   scenario means bumping its `rev`, so every MONICA commit is tested against
   fixed scenarios.
