#!/usr/bin/env bash
# The gate for refactorings that must not change a single result
# (plan-component-interfaces.md, "The gate"). Run after every step:
#
#   pixi run bash tests/refactor_gate.sh [--no-drivers]      (from odin/)
#
#   1. build and unit tests
#   2. diff_outputs.sh: the three fixtures byte-identical to $BASE
#   3. diff_vs_cpp.sh: sim-min identical to the C++ build ($CPP_RUN etc., see
#      that script); when the C++ prints Mois with 3 digits, the daily section
#      is compared with tests/compare_mois_digits.py
#   4. tests/extra_scenarios.py: scenarios the fixtures don't cover,
#      byte-identical to a reference binary (skipped if not saved yet)
#   5. tools/component_check.py: the components' declared inputs, and
#      components.md up to date
#   6. tests/compile_drivers.sh: every test driver still compiles
#
# Before the first step, from the commit the refactoring starts from:
#   BASE=build/out-baseline pixi run bash tests/diff_outputs.sh --save
#   pixi run python tests/extra_scenarios.py --save --reference <its monica-run>
#     (copy its build/monica-run somewhere first, e.g. ../build/reference-bin/)
#
# The Crop2ML modules before/after (long, ~30 min each): tests/modules_before_after.sh.
#
# BASE defaults to build/out-baseline (diff_outputs.sh's default).
cd "$(dirname "${BASH_SOURCE[0]}")/.."
R=$(cd .. && pwd)
mkdir -p "$R/build/refactor-gate"
log=$R/build/refactor-gate/gate.log
: > "$log"
ok=1
step() { echo "== $1"; }

step "build + unit tests"
if python tools/odinw.py build >>"$log" 2>&1 && python tools/odinw.py test >>"$log" 2>&1; then
  grep -E "^Finished .* tests" "$log" | tail -1
else
  echo "FAIL (see $log)"; tail -20 "$log"; exit 1
fi

step "outputs vs ${BASE:-build/out-baseline}"
if bash tests/diff_outputs.sh >>"$log" 2>&1; then echo "identical"
else echo "FAIL"; grep -E "DIFF|FAIL|differ" "$log" | head; ok=0; fi

step "vs C++"
NO_BUILD=1 bash tests/diff_vs_cpp.sh > "$R/build/refactor-gate/cpp.log" 2>&1
cpp_rc=$?
if grep -q "^FAIL - no C++" "$R/build/refactor-gate/cpp.log"; then
  echo "skipped: $(grep '^FAIL - no C++' "$R/build/refactor-gate/cpp.log")"
elif [ $cpp_rc = 0 ]; then
  echo "identical"
else
  others=$(grep -E "^  DIFFERS" "$R/build/refactor-gate/cpp.log" | grep -v "section_daily.csv")
  if [ -n "$others" ]; then echo "FAIL: $others"; ok=0
  elif python tests/compare_mois_digits.py "$R/build/out-vs-cpp/cpp/sim-min-out_section_daily.csv" \
         "$R/build/out-vs-cpp/odin/sim-min-out_section_daily.csv"; then :
  else ok=0; fi
fi

step "extra scenarios vs the reference binary"
if [ -d "$R/build/extra-scenarios" ]; then
  python tests/extra_scenarios.py > "$R/build/refactor-gate/extra.log" 2>&1 && tail -1 "$R/build/refactor-gate/extra.log" \
    || { grep -v "^ok" "$R/build/refactor-gate/extra.log"; ok=0; }
else
  echo "skipped: no reference outputs (tests/extra_scenarios.py --save --reference <binary>)"
fi

step "component inputs"
python tools/component_check.py check > "$R/build/refactor-gate/components.log" 2>&1 \
  && tail -1 "$R/build/refactor-gate/components.log" || { cat "$R/build/refactor-gate/components.log"; ok=0; }
python tools/component_check.py doc -o components.md --check || ok=0

if [ "${1:-}" != "--no-drivers" ]; then
  step "test drivers compile"
  bash tests/compile_drivers.sh || ok=0
fi

[ $ok = 1 ] && echo "GATE PASSED" || { echo "GATE FAILED"; exit 1; }
