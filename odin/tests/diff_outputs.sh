#!/usr/bin/env bash
# The reflection-output regression oracle (plan-reflective-outputs.md §5).
#
# The C++ differential harness (cpp_ref/run_monica_run.sh) needs MSVC plus a
# CMake build, so it is Windows-only. But the reflection-driven output path
# must not move a single value, which makes a before/after self-diff of the
# Odin binary both sufficient and runnable anywhere.
#
#   bash odin/tests/diff_outputs.sh --save      # snapshot the current binary
#   bash odin/tests/diff_outputs.sh             # rebuild, run, diff vs snapshot
#
# Snapshots live in build/out-baseline/ (gitignored - regenerate with --save
# from a known-good commit).
#
# TWO fixtures, and they are not equally strong evidence:
#
#   sim-min.json           the pre-existing fixture. Its baseline was taken
#                          BEFORE the reflection tier existed, so a clean diff
#                          here proves the 12 ids that moved onto the path
#                          engine (CM-count, Kc, Irrig, AbBiom, LAI, Mois,
#                          RunOff, NLeach, Recharge + the Tavg/Precip/Globrad
#                          map paths) still produce the exact bytes the
#                          lambdas did. This is the real test.
#
#   sim-min-events2.json   sim-min.json's formerly-disabled "_events" section.
#                          It exercises Pwp, Fc, Act_ET and NFert, which this
#                          port never implemented at all, so there is nothing
#                          to have been byte-identical TO. Its baseline only
#                          pins today's values against future drift; it does
#                          not certify them against the C++.
#
#   sim-min-setvalue.json  the SetValue workstep, which no fixture exercised
#                          before and which the C++ can barely express (only
#                          two of its 181 ids have a setter). Three pokes, all
#                          on the reflection tier: a raw path written from a
#                          literal per-layer array, a layer-ranged alias filled
#                          from a scalar, and an ["=", oid, "*", n] expression
#                          read-modify-writing the field it reads. Same
#                          caveat as events2 - a drift pin, not a C++ oracle -
#                          but its baseline is worth reading: the poked columns
#                          jump on exactly the poke date and stay changed.
set -uo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$REPO"

SIMS="${SIMS:-installer/Hohenfinow2/sim-min.json installer/Hohenfinow2/sim-min-events2.json installer/Hohenfinow2/sim-min-setvalue.json}"
BASE="${BASE:-build/out-baseline}"
WORK="build/out-current"
export MONICA_PARAMETERS="${MONICA_PARAMETERS:-$HOME/GitHub/monica-parameters}"

save=0
[ "${1:-}" = "--save" ] && save=1

echo "== building"
( cd odin && python tools/odinw.py build ) || exit 1

out="$WORK"
[ "$save" = 1 ] && out="$BASE"
rm -rf "$out"; mkdir -p "$out"

for sim in $SIMS; do
  echo "== running $sim"
  ( cd "$out" && "$REPO/odin/build/monica-run" -m "$REPO/$sim" ) || exit 1
done

n=$(ls "$out"/*.csv 2>/dev/null | wc -l)
[ "$n" -gt 0 ] || { echo "FAIL - no CSV produced"; exit 1; }

if [ "$save" = 1 ]; then
  echo "== saved $n baseline CSVs to $BASE"
  exit 0
fi

[ -d "$BASE" ] || { echo "FAIL - no baseline in $BASE; run with --save first"; exit 1; }

echo "== diffing $n sections against $BASE"
rc=0
for f in "$BASE"/*.csv; do
  b="$(basename "$f")"
  if [ ! -f "$out/$b" ]; then
    echo "  MISSING  $b"; rc=1; continue
  fi
  if cmp -s "$f" "$out/$b"; then
    echo "  ok       $b"
  else
    echo "  DIFFERS  $b"
    diff <(cat -A "$f") <(cat -A "$out/$b") | head -12 | sed 's/^/           /'
    rc=1
  fi
done
for f in "$out"/*.csv; do
  b="$(basename "$f")"
  [ -f "$BASE/$b" ] || { echo "  EXTRA    $b"; rc=1; }
done

[ "$rc" = 0 ] && echo "== PASS: every section byte-identical" || echo "== FAIL"
exit $rc
