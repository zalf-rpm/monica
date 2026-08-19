#!/usr/bin/env bash
# Guard for CONVENTIONS.md §1 "Transcendental functions: core:c/libc, not core:math":
# core:math's pow/exp/log/trig/... are a pure-Odin implementation and are not always
# bit-identical to the C++ reference build's <cmath> calls (MSVC CRT). Confirmed once
# already in phase 3 (soillayer::soilMoisturePF, see plan-odin.md) as a last-2-ULP
# divergence that a differential test on finitely many inputs did not catch. The fix
# there was switching to core:c/libc, which FFI-calls the same CRT function the C++
# build does. This script makes that rule mechanically enforceable instead of relying
# on remembering it every time a new module calls a transcendental.
#
# Flags any `math.<name>(` for the C 7.12 trig/hyperbolic/exp/log/power/gamma families.
# core:math is still fine - and not flagged - for round/floor/ceil/abs/is_nan/is_inf/
# comparisons/PI/etc, since those aren't FFI-risk transcendentals.
#
# Usage: bash odin/tests/check_libc_transcendentals.sh
set -uo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$REPO"

PATTERN='math\.(pow|exp2?|expm1|ln|log|log2|log10|log1p|sqrt|cbrt|sin|cos|tan|asin|acos|atan2?|sinh|cosh|tanh|asinh|acosh|atanh|hypot|erf|erfc|lgamma|tgamma)\('

# --exclude-dir='.*' keeps this to the port's own sources: odin/pixi.toml's build
# setup puts the bootstrapped Odin compiler in odin/.odin/ and its conda env in
# odin/.pixi/, and Odin's own core library legitimately calls math.sin/pow/...
# Mirrors the same skip in tests/conventions_test.odin.
if hits=$(grep -rnE "$PATTERN" --include='*.odin' --exclude-dir='.*' odin/) && [ -n "$hits" ]; then
  echo "FAIL - stray core:math transcendental call(s); use core:c/libc instead (CONVENTIONS.md §1):"
  echo "$hits"
  exit 1
fi

echo "PASS - no core:math transcendentals outside core:c/libc"
exit 0
