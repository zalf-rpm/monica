#!/usr/bin/env bash
# Phase 6 checkpoint 6 differential test - closes out phase 6:
# monicamodel::step/generalStep/cropStep (see monica_model_step_ref_main.cpp's
# header comment).
#
# Usage:  bash odin/tests/cpp_ref/run_monica_model_step.sh
set -uo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
cd "$REPO"

ODIN="${ODIN:-/c/Users/berg/development/odin-windows-dev-2026-06/dist/odin.exe}"
VCVARS="${VCVARS:-C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat}"
export MONICA_PARAMETERS="${MONICA_PARAMETERS:-$HOME/GitHub/monica-parameters}"
SIM="${SIM:-installer/Hohenfinow2/sim-min.json}"
CLIMATE="${CLIMATE:-installer/Hohenfinow2/climate-min.csv}"
NUM_DAYS="${NUM_DAYS:-500}"
CC_JSON="${CC_JSON:-build/compile_commands.json}"
VCPKG_LIB="${VCPKG_LIB:-C:\Users\berg\GitHub\vcpkg\installed\x64-windows-static\debug\lib}"

mkdir -p build/ref build/ref/monica_model_step2

[ -f "$CC_JSON" ]           || { echo "need $CC_JSON - configure the CMake build first"; exit 1; }
[ -f build/monica_lib.lib ] || { echo "need build/monica_lib.lib - build the CMake tree first"; exit 1; }

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
cat > build/ref/_build_monica_model_step2.bat <<EOF
@echo off
call "$VCVARS" >nul
cd /d $(cygpath -w "$REPO")
cl /EHsc /std:c++17 /nologo /Zi /D_USE_MATH_DEFINES $FLAGS /I odin\tests\cpp_ref /Fe:build\ref\monica_model_step_ref.exe /Fo:"build\ref\monica_model_step2\\\\" /Fd:"build\ref\monica_model_step2\\\\" odin\tests\cpp_ref\monica_model_step_ref_main.cpp /link /LIBPATH:build /LIBPATH:$VCPKG_LIB monica_run_lib.lib monica_lib.lib climate\climate-file-io\climate_file_io_lib.lib climate\climate-file-io\climate_common\climate_common_lib.lib common\common_lib.lib common\json11\json11_lib.lib common\json11\date\date_lib.lib common\tools\helpers\helpers_lib.lib tools\debug\debug_lib.lib soil\soil_lib.lib mas-infrastructure\capnproto_schemas\capnp_schemas_lib.lib capnp-json.lib capnp-rpc.lib capnp.lib capnpc.lib kj-http.lib kj-gzip.lib kj-async.lib kj.lib zlibd.lib libsodium.lib advapi32.lib ws2_32.lib
EOF
cmd //c "$(cygpath -w build/ref/_build_monica_model_step2.bat)" > build/ref/_build_monica_model_step2.log 2>&1
if [ ! -f build/ref/monica_model_step_ref.exe ]; then
  echo "FAILED to build the C++ driver; see build/ref/_build_monica_model_step2.log"
  grep -iE "error" build/ref/_build_monica_model_step2.log | head -30
  exit 1
fi

echo "== building Odin driver"
"$ODIN" build odin/tests/monica_model_step_ref -out:build/ref/monica_model_step_ref_odin.exe || exit 1

echo "== running both on $SIM / $CLIMATE ($NUM_DAYS days)"
./build/ref/monica_model_step_ref.exe      "$SIM" "$CLIMATE" "$NUM_DAYS" 2>build/ref/monica_model_step2_cpp.err  | tr -d '\r' | sort > build/ref/monica_model_step2_cpp.txt
./build/ref/monica_model_step_ref_odin.exe "$SIM" "$CLIMATE" "$NUM_DAYS" 2>build/ref/monica_model_step2_odin.err | tr -d '\r' | sort > build/ref/monica_model_step2_odin.txt

n=$(wc -l < build/ref/monica_model_step2_cpp.txt)
if [ "$n" -lt 100 ]; then
  echo "FAIL - C++ driver produced only $n trace lines; stderr:"
  head -30 build/ref/monica_model_step2_cpp.err
  exit 1
fi

echo "== diffing"
if diff build/ref/monica_model_step2_cpp.txt build/ref/monica_model_step2_odin.txt > build/ref/monica_model_step2_diff.txt; then
  echo "PASS - monicamodel::step/generalStep/cropStep trace identical ($n lines) - phase 6 complete"
  exit 0
else
  echo "FAIL - first differing fields:"
  head -60 build/ref/monica_model_step2_diff.txt
  exit 1
fi
