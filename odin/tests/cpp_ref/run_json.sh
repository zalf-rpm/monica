#!/usr/bin/env bash
# Phase 1 differential test for odin/support/jsonx.
#
# Parses and re-dumps every real MONICA parameter file through both json11 (C++)
# and support/jsonx (Odin) and diffs the results. This exercises, on real data:
#   - Tools::readFile's newline-stripping concatenation
#   - number classification (json11's JsonInt vs JsonDouble)
#   - "%.17g" double formatting
#   - sorted object key order (json11's object is a std::map)
#   - string escaping
#
# Usage:  bash odin/tests/cpp_ref/run_json.sh
#
# KNOWN DIVERGENCE (see KNOWN_DIVERGENCES below): Odin's core:encoding/json
# silently drops object entries whose key is the empty string - parser.odin has
# an explicit `if key != ""` guard. MONICA never looks up "", and the single
# affected file is not part of the Hohenfinow2 regression fixture, so this is
# accepted rather than worked around.
set -uo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
cd "$REPO"

ODIN="${ODIN:-/c/Users/berg/development/odin-windows-dev-2026-06/dist/odin.exe}"
VCVARS="${VCVARS:-C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat}"
MONICA_PARAMETERS_DIR="${MONICA_PARAMETERS_DIR:-$HOME/GitHub/monica-parameters}"

# Files where the two implementations are known and accepted to differ.
KNOWN_DIVERGENCES='projects/fnr-voce/maize.json'

mkdir -p build/ref build/ref/json

echo "== building C++ (json11) reference driver"
cat > build/ref/_build_json.bat <<EOF
@echo off
call "$VCVARS" >nul
cd /d $(cygpath -w "$REPO")
cl /EHsc /std:c++17 /nologo /D_USE_MATH_DEFINES /Fe:build\\ref\\json_ref.exe /Fo:"build\\ref\\json\\\\" odin\\tests\\cpp_ref\\json_ref_main.cpp mas_cpp_misc\\json11\\json11.cpp mas_cpp_misc\\json11\\json11-helper.cpp mas_cpp_misc\\tools\\helper.cpp mas_cpp_misc\\tools\\date.cpp mas_cpp_misc\\tools\\algorithms.cpp /I mas_cpp_misc /I mas_cpp_misc\\tools /I mas_cpp_misc\\json11
EOF
cmd //c "$(cygpath -w build/ref/_build_json.bat)" > build/ref/_build_json.log 2>&1
if [ ! -f build/ref/json_ref.exe ]; then
  echo "FAILED to build the C++ driver; see build/ref/_build_json.log"; exit 1
fi

echo "== building Odin dumper"
"$ODIN" build odin/tests/json_ref -out:build/ref/json_ref_odin.exe || exit 1

echo "== collecting input files"
: > build/ref/json_files.txt
[ -d "$MONICA_PARAMETERS_DIR" ] && find "$MONICA_PARAMETERS_DIR" -name '*.json' | sort >> build/ref/json_files.txt
find installer/Hohenfinow2 -name '*.json' | sort >> build/ref/json_files.txt
ls -1 ./*.json 2>/dev/null | sort >> build/ref/json_files.txt
N=$(wc -l < build/ref/json_files.txt)
echo "   $N files"
if [ "$N" -eq 0 ]; then echo "no input files found"; exit 1; fi

echo "== running both"
# batched: the full list overflows the command-line length limit
xargs -a build/ref/json_files.txt -n 40 ./build/ref/json_ref.exe      > build/ref/json_cpp.txt  2>&1
xargs -a build/ref/json_files.txt -n 40 ./build/ref/json_ref_odin.exe > build/ref/json_odin.txt 2>&1

echo "== diffing"
filter() { tr -d '\r' < "$1" | grep -v -F "$KNOWN_DIVERGENCES"; }
if diff <(filter build/ref/json_cpp.txt) <(filter build/ref/json_odin.txt) > build/ref/json_diff.txt; then
  echo "PASS - jsonx parse+dump matches json11 over $N files"
  echo "       (excluding known divergence: $KNOWN_DIVERGENCES)"
  exit 0
else
  echo "FAIL - first differences:"
  cut -c1-300 build/ref/json_diff.txt | head -40
  exit 1
fi
