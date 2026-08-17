#!/usr/bin/env bash
# Phase 7 checkpoint 4 differential test: run/run-monica.cpp's runMonica vs
# odin/monica/run/run_monica.odin's run_monica - the real
# installer/Hohenfinow2/sim-min.json crop rotation, end to end, dumped
# through the checkpoint-3-verified CSV writer. See run_monica_ref_main.cpp's
# header comment.
#
# Usage:  bash odin/tests/cpp_ref/run_monica_run.sh
set -uo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
cd "$REPO"

ODIN="${ODIN:-/c/Users/berg/development/odin-windows-dev-2026-06/dist/odin.exe}"
VCVARS="${VCVARS:-C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat}"
export MONICA_PARAMETERS="${MONICA_PARAMETERS:-$HOME/GitHub/monica-parameters}"
SIM="${SIM:-installer/Hohenfinow2/sim-min.json}"
CC_JSON="${CC_JSON:-build/compile_commands.json}"
VCPKG_LIB="${VCPKG_LIB:-C:\Users\berg\GitHub\vcpkg\installed\x64-windows-static\debug\lib}"

mkdir -p build/ref build/ref/monica_run

[ -f "$CC_JSON" ]               || { echo "need $CC_JSON - configure the CMake build first"; exit 1; }
[ -f build/monica_run_lib.lib ] || { echo "need build/monica_run_lib.lib - build the CMake tree first"; exit 1; }

echo "== deriving compiler flags from $CC_JSON"
FLAGS=$(node -e '
const fs=require("fs");
const cc=JSON.parse(fs.readFileSync(process.argv[1],"utf8"));
const e=cc.find(x=>x.file.includes("create-env-from-json-config"));
if(!e){console.error("create-env-from-json-config not found in compile_commands.json");process.exit(1);}
const cmd=e.command||e.arguments.join(" ");
const keep=cmd.split(/\s+/).filter(s=>/^[-\/][ID]/.test(s)||/^-external:I/.test(s)||/^[-\/](MT|MD)d?$/.test(s));
process.stdout.write(keep.map(s=>s.replace(/^-external:I/,"/I")).join(" "));
' "$CC_JSON") || exit 1

echo "== building C++ reference driver"
cat > build/ref/_build_monica_run.bat <<EOF
@echo off
call "$VCVARS" >nul
cd /d $(cygpath -w "$REPO")
cl /EHsc /std:c++17 /nologo /Zi /D_USE_MATH_DEFINES $FLAGS /I odin\tests\cpp_ref /Fe:build\ref\monica_run_ref.exe /Fo:"build\ref\monica_run\\\\" /Fd:"build\ref\monica_run\\\\" odin\tests\cpp_ref\run_monica_ref_main.cpp /link /LIBPATH:build /LIBPATH:$VCPKG_LIB monica_run_lib.lib monica_lib.lib climate\climate-file-io\climate_file_io_lib.lib climate\climate-file-io\climate_common\climate_common_lib.lib common\common_lib.lib common\json11\json11_lib.lib common\json11\date\date_lib.lib common\tools\helpers\helpers_lib.lib tools\debug\debug_lib.lib soil\soil_lib.lib mas-infrastructure\capnproto_schemas\capnp_schemas_lib.lib capnp-json.lib capnp-rpc.lib capnp.lib capnpc.lib kj-http.lib kj-gzip.lib kj-async.lib kj.lib zlibd.lib libsodium.lib advapi32.lib ws2_32.lib
EOF
cmd //c "$(cygpath -w build/ref/_build_monica_run.bat)" > build/ref/_build_monica_run.log 2>&1
if [ ! -f build/ref/monica_run_ref.exe ]; then
  echo "FAILED to build the C++ driver; see build/ref/_build_monica_run.log"
  grep -iE "error" build/ref/_build_monica_run.log | head -30
  exit 1
fi

echo "== building Odin driver"
"$ODIN" build odin/tests/monica_run_ref -out:build/ref/monica_run_ref_odin.exe || exit 1

echo "== running both on $SIM (full climate file, real crop rotation cycling)"
time ./build/ref/monica_run_ref.exe      "$SIM" > build/ref/monica_run_cpp.txt  2>build/ref/monica_run_cpp.err
time ./build/ref/monica_run_ref_odin.exe "$SIM" > build/ref/monica_run_odin.txt 2>build/ref/monica_run_odin.err

n=$(wc -l < build/ref/monica_run_cpp.txt)
if [ "$n" -lt 10 ]; then
  echo "FAIL - C++ driver produced only $n lines; stderr:"
  head -30 build/ref/monica_run_cpp.err
  exit 1
fi

echo "== diffing (binary - line endings matter, no tr/sort)"
if diff build/ref/monica_run_cpp.txt build/ref/monica_run_odin.txt > build/ref/monica_run_diff.txt; then
  echo "PASS - runMonica output byte-identical ($n lines) - phase 7 checkpoint 4 complete"
  exit 0
else
  echo "FAIL - first differing lines:"
  head -60 build/ref/monica_run_diff.txt
  exit 1
fi
