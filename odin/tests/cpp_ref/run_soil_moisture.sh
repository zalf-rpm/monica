#!/usr/bin/env bash
# Phase 4 module 2b differential test: soilmoisture.cpp itself (module 2a
# already proved snow-component/frost-component in isolation).
#
# Builds a real MonicaModel from sim-min.json via makeMonicaModel, drives
# soilmoisture::step directly over NUM_DAYS days of the real Hohenfinow2
# climate-min.csv record with a synthetic deterministic groundwater-depth
# sequence and bare soil (no live crop) - see soil_moisture_ref_main.cpp's
# header comment for the full rationale.
#
# Usage:  bash odin/tests/cpp_ref/run_soil_moisture.sh
#
# Requires a configured AND BUILT CMake tree, same as run_soil_temperature.sh.
set -uo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
cd "$REPO"

ODIN="${ODIN:-/c/Users/berg/development/odin-windows-dev-2026-06/dist/odin.exe}"
VCVARS="${VCVARS:-C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat}"
export MONICA_PARAMETERS="${MONICA_PARAMETERS:-$HOME/GitHub/monica-parameters}"
SIM="${SIM:-installer/Hohenfinow2/sim-min.json}"
CLIMATE="${CLIMATE:-installer/Hohenfinow2/climate-min.csv}"
NUM_DAYS="${NUM_DAYS:-365}"
CC_JSON="${CC_JSON:-build/compile_commands.json}"
VCPKG_LIB="${VCPKG_LIB:-C:\Users\berg\GitHub\vcpkg\installed\x64-windows-static\debug\lib}"

mkdir -p build/ref build/ref/soil_moisture

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
cat > build/ref/_build_soil_moisture.bat <<EOF
@echo off
call "$VCVARS" >nul
cd /d $(cygpath -w "$REPO")
cl /EHsc /std:c++17 /nologo /Zi /D_USE_MATH_DEFINES $FLAGS /I odin\tests\cpp_ref /Fe:build\ref\soil_moisture_ref.exe /Fo:"build\ref\soil_moisture\\\\" /Fd:"build\ref\soil_moisture\\\\" odin\tests\cpp_ref\soil_moisture_ref_main.cpp /link /LIBPATH:build /LIBPATH:$VCPKG_LIB monica_run_lib.lib monica_lib.lib climate\climate-file-io\climate_file_io_lib.lib climate\climate-file-io\climate_common\climate_common_lib.lib common\common_lib.lib common\json11\json11_lib.lib common\json11\date\date_lib.lib common\tools\helpers\helpers_lib.lib tools\debug\debug_lib.lib soil\soil_lib.lib mas-infrastructure\capnproto_schemas\capnp_schemas_lib.lib capnp-json.lib capnp-rpc.lib capnp.lib capnpc.lib kj-http.lib kj-gzip.lib kj-async.lib kj.lib zlibd.lib libsodium.lib advapi32.lib ws2_32.lib
EOF
cmd //c "$(cygpath -w build/ref/_build_soil_moisture.bat)" > build/ref/_build_soil_moisture.log 2>&1
if [ ! -f build/ref/soil_moisture_ref.exe ]; then
  echo "FAILED to build the C++ driver; see build/ref/_build_soil_moisture.log"
  grep -iE "error" build/ref/_build_soil_moisture.log | head -12
  exit 1
fi

echo "== building Odin driver"
"$ODIN" build odin/tests/soil_moisture_ref -out:build/ref/soil_moisture_ref_odin.exe || exit 1

echo "== running both on $SIM / $CLIMATE ($NUM_DAYS days)"
./build/ref/soil_moisture_ref.exe      "$SIM" "$CLIMATE" "$NUM_DAYS" 2>build/ref/soil_moisture_cpp.err  | tr -d '\r' | sort > build/ref/soil_moisture_cpp.txt
./build/ref/soil_moisture_ref_odin.exe "$SIM" "$CLIMATE" "$NUM_DAYS" 2>build/ref/soil_moisture_odin.err | tr -d '\r' | sort > build/ref/soil_moisture_odin.txt

n=$(wc -l < build/ref/soil_moisture_cpp.txt)
if [ "$n" -lt 1000 ]; then
  echo "FAIL - C++ driver produced only $n trace lines; stderr:"
  head -10 build/ref/soil_moisture_cpp.err
  exit 1
fi

echo "== diffing"
if diff build/ref/soil_moisture_cpp.txt build/ref/soil_moisture_odin.txt > build/ref/soil_moisture_diff.txt; then
  echo "PASS - soilmoisture daily trace identical over $NUM_DAYS days ($n lines)"
  exit 0
else
  echo "FAIL - first differing fields:"
  head -20 build/ref/soil_moisture_diff.txt
  exit 1
fi
