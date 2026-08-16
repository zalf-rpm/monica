#!/usr/bin/env bash
# Phase 7 checkpoint 3 differential test: io/csv-format.cpp's
# writeOutputHeaderRows/writeOutput vs odin/monica/io/csv_format.odin - see
# csv_format_ref_main.cpp's header comment for what's exercised.
#
# Usage:  bash odin/tests/cpp_ref/run_csv_format.sh
set -uo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
cd "$REPO"

ODIN="${ODIN:-/c/Users/berg/development/odin-windows-dev-2026-06/dist/odin.exe}"
VCVARS="${VCVARS:-C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat}"
CC_JSON="${CC_JSON:-build/compile_commands.json}"
VCPKG_LIB="${VCPKG_LIB:-C:\Users\berg\GitHub\vcpkg\installed\x64-windows-static\debug\lib}"

mkdir -p build/ref build/ref/csv_format

[ -f "$CC_JSON" ]           || { echo "need $CC_JSON - configure the CMake build first"; exit 1; }
[ -f build/monica_lib.lib ] || { echo "need build/monica_lib.lib - build the CMake tree first"; exit 1; }

echo "== deriving compiler flags from $CC_JSON"
FLAGS=$(node -e '
const fs=require("fs");
const cc=JSON.parse(fs.readFileSync(process.argv[1],"utf8"));
const e=cc.find(x=>x.file.includes("csv-format.cpp"));
if(!e){console.error("csv-format.cpp not found in compile_commands.json");process.exit(1);}
const cmd=e.command||e.arguments.join(" ");
const keep=cmd.split(/\s+/).filter(s=>/^[-\/][ID]/.test(s)||/^-external:I/.test(s)||/^[-\/](MT|MD)d?$/.test(s));
process.stdout.write(keep.map(s=>s.replace(/^-external:I/,"/I")).join(" "));
' "$CC_JSON") || exit 1

echo "== building C++ reference driver"
cat > build/ref/_build_csv_format.bat <<EOF
@echo off
call "$VCVARS" >nul
cd /d $(cygpath -w "$REPO")
cl /EHsc /std:c++17 /nologo /Zi /D_USE_MATH_DEFINES $FLAGS /I odin\tests\cpp_ref /Fe:build\ref\csv_format_ref.exe /Fo:"build\ref\csv_format\\\\" /Fd:"build\ref\csv_format\\\\" odin\tests\cpp_ref\csv_format_ref_main.cpp /link /LIBPATH:build /LIBPATH:$VCPKG_LIB monica_run_lib.lib monica_lib.lib common\common_lib.lib common\json11\json11_lib.lib common\json11\date\date_lib.lib common\tools\helpers\helpers_lib.lib tools\debug\debug_lib.lib soil\soil_lib.lib
EOF
cmd //c "$(cygpath -w build/ref/_build_csv_format.bat)" > build/ref/_build_csv_format.log 2>&1
if [ ! -f build/ref/csv_format_ref.exe ]; then
  echo "FAILED to build the C++ driver; see build/ref/_build_csv_format.log"
  grep -iE "error" build/ref/_build_csv_format.log | head -30
  exit 1
fi

echo "== building Odin driver"
"$ODIN" build odin/tests/csv_format_ref -out:build/ref/csv_format_ref_odin.exe || exit 1

echo "== running both"
./build/ref/csv_format_ref.exe      > build/ref/csv_format_cpp.txt  2>build/ref/csv_format_cpp.err
./build/ref/csv_format_ref_odin.exe > build/ref/csv_format_odin.txt 2>build/ref/csv_format_odin.err

echo "== diffing (binary - line endings matter, no tr/sort)"
if diff build/ref/csv_format_cpp.txt build/ref/csv_format_odin.txt > build/ref/csv_format_diff.txt; then
  echo "PASS - writeOutputHeaderRows/writeOutput byte-identical - phase 7 checkpoint 3 complete"
  exit 0
else
  echo "FAIL - diff:"
  cat build/ref/csv_format_diff.txt
  exit 1
fi
