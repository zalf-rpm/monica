#!/usr/bin/env bash
# End-to-end oracle against the real C++ MONICA: runs the C++ monica-run on
# the C++ checkout's own sim-min.json and the Odin monica-run on this repo's,
# then byte-diffs every output section.
#
#   bash odin/tests/diff_vs_cpp.sh            # rebuild Odin, run both, diff
#   NO_BUILD=1 bash odin/tests/diff_vs_cpp.sh # reuse the existing Odin binary
#
# The reference is the C++ MONICA checkout (default ../monica, built in its
# build/ dir) - NOT this repo's src/, which is the C-style intermediate the
# port started from. The odin/tests/cpp_ref drivers are tied to that
# intermediate API and can't be pointed at the reference checkout.
#
# Each side runs its own sim-min.json because this repo's copy uses the
# Odin-only {"path": ...} output spec (plan-reflective-outputs.md), which the
# C++ silently skips; the C++ checkout's copy spells the same columns the
# classic way (["Mois", [1, 3]]). Only sim-min is compared: the
# sim-min-events2/-setvalue fixtures are Odin-only drift pins
# (diff_outputs.sh).
#
# NOTE: the Odin side rounds Mois to 6 digits; the C++ build-output.cpp
# rounds it to 3. Matching output currently relies on an uncommitted local
# change in the C++ checkout that bumps Mois to 6 digits there too - a Mois-
# only mismatch here most likely means that change isn't in the C++ build.
set -uo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$REPO"

CPP_REPO="${CPP_REPO:-$(cd "$REPO/../monica" 2>/dev/null && pwd)}"
CPP_RUN="${CPP_RUN:-$CPP_REPO/build/monica-run.exe}"
CPP_SIM="${CPP_SIM:-$CPP_REPO/installer/Hohenfinow2/sim-min.json}"
ODIN_SIM="${ODIN_SIM:-$REPO/installer/Hohenfinow2/sim-min.json}"
OUT="build/out-vs-cpp"
export MONICA_PARAMETERS="${MONICA_PARAMETERS:-$HOME/GitHub/monica-parameters}"

[ -x "$CPP_RUN" ] || { echo "FAIL - no C++ monica-run at $CPP_RUN (set CPP_REPO or CPP_RUN)"; exit 1; }
[ -f "$CPP_SIM" ] || { echo "FAIL - no C++ sim json at $CPP_SIM (set CPP_SIM)"; exit 1; }

if [ "${NO_BUILD:-0}" != 1 ]; then
  echo "== building Odin"
  ( cd odin && python tools/odinw.py build ) || exit 1
fi

rm -rf "$OUT"; mkdir -p "$OUT/cpp" "$OUT/odin"
echo "== C++:  $CPP_RUN  ($(git -C "$CPP_REPO" log -1 --format='%h %s' 2>/dev/null))"
( cd "$OUT/cpp" && "$CPP_RUN" -m "$CPP_SIM" > run.log 2>&1 ) || { echo "FAIL - C++ run failed, see $OUT/cpp/run.log"; exit 1; }
echo "== Odin: odin/build/monica-run"
( cd "$OUT/odin" && "$REPO/odin/build/monica-run" -m "$ODIN_SIM" > run.log 2>&1 ) || { echo "FAIL - Odin run failed, see $OUT/odin/run.log"; exit 1; }

n=$(ls "$OUT/cpp"/*.csv 2>/dev/null | wc -l)
[ "$n" -gt 0 ] || { echo "FAIL - C++ produced no CSV"; exit 1; }

echo "== diffing $n sections"
rc=0
for f in "$OUT/cpp"/*.csv; do
  b="$(basename "$f")"
  if [ ! -f "$OUT/odin/$b" ]; then
    echo "  MISSING  $b (Odin)"; rc=1; continue
  fi
  if cmp -s "$f" "$OUT/odin/$b"; then
    echo "  ok       $b"
  else
    echo "  DIFFERS  $b"
    diff "$f" "$OUT/odin/$b" | head -6 | cut -c1-160 | sed 's/^/           /'
    rc=1
  fi
done
for f in "$OUT/odin"/*.csv; do
  b="$(basename "$f")"
  [ -f "$OUT/cpp/$b" ] || { echo "  EXTRA    $b (Odin only)"; rc=1; }
done

[ "$rc" = 0 ] && echo "== PASS: every section byte-identical to the C++ reference" || echo "== FAIL"
exit $rc
