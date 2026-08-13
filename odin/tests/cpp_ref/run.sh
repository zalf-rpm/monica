#!/usr/bin/env bash
# Phase 0 differential test for odin/support/date.
#
# Builds the C++ reference driver (against the real mas_cpp_misc/tools/date.cpp)
# and the Odin dumper, runs both, and diffs their output.
#
# Both must produce identical CSV. C++ printf on Windows emits CRLF while Odin
# emits LF, so carriage returns are stripped before diffing - that is the ONLY
# permitted difference.
#
# Usage:  bash odin/tests/cpp_ref/run.sh
set -uo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
cd "$REPO"

ODIN="${ODIN:-/c/Users/berg/development/odin-windows-dev-2026-06/dist/odin.exe}"
VCVARS="${VCVARS:-C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat}"

mkdir -p build/ref

echo "== building C++ reference driver"
cat > build/ref/_build.bat <<EOF
@echo off
call "$VCVARS" >nul
cd /d $(cygpath -w "$REPO")
cl /EHsc /std:c++17 /nologo /D_USE_MATH_DEFINES /Fe:build\\ref\\date_ref.exe /Fo:build\\ref\\ odin\\tests\\cpp_ref\\date_ref_main.cpp mas_cpp_misc\\tools\\date.cpp mas_cpp_misc\\tools\\algorithms.cpp /I mas_cpp_misc /I mas_cpp_misc\\tools
EOF
cmd //c "$(cygpath -w build/ref/_build.bat)" > build/ref/_build.log 2>&1
if [ ! -f build/ref/date_ref.exe ]; then
  echo "FAILED to build the C++ driver; see build/ref/_build.log"; exit 1
fi

echo "== building Odin dumper"
"$ODIN" build odin/tests/date_ref -out:build/ref/date_ref_odin.exe || exit 1

echo "== running both"
./build/ref/date_ref.exe      > build/ref/date_cpp.csv  || { echo "C++ driver aborted"; exit 1; }
./build/ref/date_ref_odin.exe > build/ref/date_odin.csv || { echo "Odin dumper aborted"; exit 1; }

echo "== diffing ($(wc -l < build/ref/date_cpp.csv) lines)"
if diff <(tr -d '\r' < build/ref/date_cpp.csv) <(tr -d '\r' < build/ref/date_odin.csv) > build/ref/date_diff.txt; then
  echo "PASS - Date output is identical"
  exit 0
else
  echo "FAIL - first differences:"
  head -40 build/ref/date_diff.txt
  exit 1
fi
