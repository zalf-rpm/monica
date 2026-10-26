#!/usr/bin/env bash
# The checks of the Crop2ML modules (plan-crop2ml-modules.md).
#
# Standalone (phase 2), on an input written from Hohenfinow2's sim-min.json and
# on a variant of it with crop cover, snow and changing soil moisture:
#
#   - Monica_SoilTemp gives exactly MONICA's own soil temperature ("internal")
#   - every module's Odin code gives the numbers of its generated Python code
#     (identical, or within 1e-12 relative: Odin uses CORE-MATH, Python libm)
#
# MONICA as driver (phase 3): tests/check_drivers.py.
#
# Needs `pixi run gen-amei` first (the generated Python and PyCrop2ML) and
# MONICA's parameters for from-sim. Run from odin/ through pixi:
#
#   MONICA_PARAMETERS=... pixi run bash tests/check_modules.sh
set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")/.."
export MONICA_PARAMETERS="${MONICA_PARAMETERS:-$HOME/GitHub/monica-parameters}"
PIXI="${PIXI_EXE:-pixi}"
PYCROPML_ENV=(--manifest-path build/crop2ml/PyCrop2ML/pyproject.toml)
MODULES="Monica_SoilTemp DSSAT_ST_standalone"
W=build/check-modules
EXE=build/monica-module
[ "${OS:-}" = Windows_NT ] && EXE=$EXE.exe

python tools/odinw.py build monica-run monica-module
mkdir -p "$W"

"$EXE" from-sim ../installer/Hohenfinow2/sim-min.json "$W/h2.json"
python tools/module_reference.py --dynamic "$W/h2.json" "$W/h2-dyn.json"

rc=0
for input in h2 h2-dyn; do
  echo "== $input"
  for impl in internal $MODULES; do
    "$EXE" run SoilTemperature "$impl" "$W/$input.json" "$W/$input-$impl.csv"
  done
  echo -n "Monica_SoilTemp vs internal: "
  python tools/module_reference.py --compare "$W/$input-internal.csv" "$W/$input-Monica_SoilTemp.csv" \
    --tolerance 0 || rc=1
  for m in $MODULES; do
    "$PIXI" run "${PYCROPML_ENV[@]}" python tools/module_reference.py "$m" "$W/$input.json" "$W/$input-$m-py.csv"
    echo -n "$m Odin vs Python: "
    python tools/module_reference.py --compare "$W/$input-$m-py.csv" "$W/$input-$m.csv" || rc=1
  done
done
[ "$rc" = 0 ] && echo "== PASS" || echo "== FAIL"

RUN=build/monica-run
[ "${OS:-}" = Windows_NT ] && RUN=$RUN.exe
python tests/check_drivers.py --monica-run "$RUN" --monica-module "$EXE" --work build/check-drivers || rc=1
exit "$rc"
