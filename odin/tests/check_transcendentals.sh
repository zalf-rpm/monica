#!/usr/bin/env bash
# Guard for CONVENTIONS.md §1 "Transcendental functions: cr_math only": every
# exp/log/pow/trig/... call goes through support/cr_math (CORE-MATH, correctly
# rounded) - never core:math (pure Odin) or core:c/libc (the platform's libm;
# glibc and Microsoft's UCRT round differently in the last bit, which MONICA
# amplifies into percent-level differences). The same check runs in
# `odin test odin/tests` as conventions_test.odin's
# test_transcendentals_use_cr_math; this is the quick standalone version.
#
# Flags `math.<name>(` and `libc.<name>(` for the C 7.12 trig/hyperbolic/exp/
# log/power/gamma families, as whole identifiers (so `cr_math.pow(` doesn't
# count as `math.pow(`). Not flagged: core:math's round/floor/ceil/abs/PI/...,
# and libc.sqrt/math.sqrt (correctly rounded by IEEE 754 in every libm, and
# core:math's is the CPU instruction).
#
# Usage: bash odin/tests/check_transcendentals.sh
set -uo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$REPO"

NAMES='pow|exp2?|expm1|ln|log|log2|log10|log1p|sqrt|cbrt|sin|cos|tan|asin|acos|atan2?|sinh|cosh|tanh|asinh|acosh|atanh|hypot|erf|erfc|lgamma|tgamma'
PATTERN="(^|[^A-Za-z0-9_])(math|libc)\\.(${NAMES/|sqrt/})\\("

# --exclude-dir='.*' keeps this to the port's own sources: odin/pixi.toml's build
# setup puts the bootstrapped Odin compiler in odin/.odin/ and its conda env in
# odin/.pixi/, and Odin's own core library legitimately calls math.sin/pow/...
# Mirrors the same skip in tests/conventions_test.odin.
if hits=$(grep -rnE "$PATTERN" --include='*.odin' --exclude-dir='.*' --exclude-dir=build odin/) && [ -n "$hits" ]; then
  echo "FAIL - transcendental call(s) not through cr_math (CONVENTIONS.md §1):"
  echo "$hits"
  exit 1
fi

echo "PASS - all transcendental calls go through cr_math"
exit 0
