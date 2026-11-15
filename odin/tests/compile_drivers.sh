#!/usr/bin/env bash
# `odin check` every test driver directory (tests/*_ref and other mains) and
# the command packages that aren't part of `pixi run build`, so a changed
# signature can't silently break a driver. Reports failures only.
#
#   pixi run bash tests/compile_drivers.sh        (from odin/)
cd "$(dirname "${BASH_SOURCE[0]}")/.."
fail=0
for d in tests/*/; do
  d=${d%/}
  ls "$d"/*.odin >/dev/null 2>&1 || continue
  out=$(python tools/odinw.py exec -- check "$d" 2>&1) || { echo "FAIL $d"; echo "$out" | grep -v "^==>" | head -5; fail=1; }
done
for c in cmd/monica-run cmd/monica-module; do
  out=$(python tools/odinw.py exec -- check "$c" 2>&1) || { echo "FAIL $c"; echo "$out" | grep -v "^==>" | head -5; fail=1; }
done
[ $fail = 0 ] && echo "all drivers compile"
exit $fail
