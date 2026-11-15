#!/usr/bin/env bash
# The Crop2ML modules (soil temperature, evapotranspiration) as drivers and
# integrated, before and after a change: tests/check_drivers.py with a
# reference monica-run/monica-module and with the current build, then every
# CSV of the two work directories byte-compared. ~30 min per run.
#
#   pixi run bash tests/modules_before_after.sh <reference bin dir>     (from odin/)
#
# <reference bin dir> holds monica-run and monica-module built from the
# commit to compare with. The reference run is reused if its work directory
# ($MODULES_REFERENCE, default ../build/modules-reference) exists; delete it to redo it.
set -u
cd "$(dirname "${BASH_SOURCE[0]}")/.."
R=$(cd .. && pwd)
REF=${1:?usage: modules_before_after.sh <reference bin dir>}
REF_WORK=${MODULES_REFERENCE:-$R/build/modules-reference}
EXE=""; [ "${OS:-}" = Windows_NT ] && EXE=.exe
export MONICA_PARAMETERS="${MONICA_PARAMETERS:-$HOME/GitHub/monica-parameters}"

python tools/odinw.py build monica-run monica-module > /dev/null || exit 1
NEW="$R/build/modules-new-bin"; mkdir -p "$NEW"
cp "build/monica-run$EXE" "build/monica-module$EXE" "$NEW/"

if [ ! -d "$REF_WORK" ]; then
  echo "== reference run"
  python tests/check_drivers.py --monica-run "$REF/monica-run$EXE" --monica-module "$REF/monica-module$EXE" \
    --work "$REF_WORK" > "$R/build/modules-reference.log" 2>&1 \
    || { echo "reference run failed, see $R/build/modules-reference.log"; exit 1; }
fi
echo "== current build"
rm -rf "$R/build/modules-new"
python tests/check_drivers.py --monica-run "$NEW/monica-run$EXE" --monica-module "$NEW/monica-module$EXE" \
  --work "$R/build/modules-new" > "$R/build/modules-new.log" 2>&1
rc=$?
tail -1 "$R/build/modules-new.log"
[ $rc = 0 ] || { echo "check_drivers.py failed, see $R/build/modules-new.log"; exit 1; }
python tests/compare_csv_dirs.py "$REF_WORK" "$R/build/modules-new"
