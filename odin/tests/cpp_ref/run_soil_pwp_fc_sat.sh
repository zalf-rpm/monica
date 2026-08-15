#!/usr/bin/env bash
# Phase 3's "required second oracle" (plan-odin.md): an interpolation sweep over
# fcSatPwpFromKA5textureClass (the highest-risk part of phase 3) and its
# fcSatPwpFromVanGenuchten{Vereecken,Toth}/fcSatPwpFromToth siblings, reached
# through the public updateUnsetPwpFcSatFrom* entry points.
#
# The per-layer state diff (a separate phase-3 checkpoint, over the Hohenfinow2
# fixture) only exercises the soil types/densities that fixture happens to
# contain - not enough to catch a wrong interpolation in an unexercised branch.
# This sweeps every KA5 texture class x raw densities straddling the table
# breakpoints x organic-matter values straddling their breakpoints (~7,000
# rows), plus fallback-resolution combos, plus a grid for the three closed-form
# Van Genuchten / Toth variants (~8,000 rows).
#
# Usage:  bash odin/tests/cpp_ref/run_soil_pwp_fc_sat.sh
#
# Requires a configured AND BUILT CMake tree and $MONICA_PARAMETERS/soil/{
# SoilCharacteristicData,SoilCharacteristicModifier}.json (CapillaryRiseRates
# isn't touched by this sweep).
set -uo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
cd "$REPO"

ODIN="${ODIN:-/c/Users/berg/development/odin-windows-dev-2026-06/dist/odin.exe}"
VCVARS="${VCVARS:-C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat}"
export MONICA_PARAMETERS="${MONICA_PARAMETERS:-$HOME/GitHub/monica-parameters}"
CC_JSON="${CC_JSON:-build/compile_commands.json}"
VCPKG_LIB="${VCPKG_LIB:-C:\Users\berg\GitHub\vcpkg\installed\x64-windows-static\debug\lib}"

mkdir -p build/ref build/ref/soil_sweep

[ -f "$CC_JSON" ]           || { echo "need $CC_JSON - configure the CMake build first"; exit 1; }
[ -f build/monica_lib.lib ] || { echo "need build/monica_lib.lib - build the CMake tree first"; exit 1; }
[ -f "$MONICA_PARAMETERS/soil/SoilCharacteristicData.json" ] || { echo "need $MONICA_PARAMETERS/soil/SoilCharacteristicData.json"; exit 1; }

echo "== deriving compiler flags from $CC_JSON"
FLAGS=$(node -e '
const fs=require("fs");
const cc=JSON.parse(fs.readFileSync(process.argv[1],"utf8"));
const e=cc.find(x=>x.file.includes("monica-parameters"))||cc.find(x=>x.file.includes("soil.cpp"));
if(!e){console.error("no suitable entry in compile_commands.json");process.exit(1);}
const cmd=e.command||e.arguments.join(" ");
const keep=cmd.split(/\s+/).filter(s=>/^[-\/][ID]/.test(s)||/^-external:I/.test(s)||/^[-\/](MT|MD)d?$/.test(s));
process.stdout.write(keep.map(s=>s.replace(/^-external:I/,"/I")).join(" "));
' "$CC_JSON") || exit 1

echo "== building C++ reference driver"
cat > build/ref/_build_soil_sweep.bat <<EOF
@echo off
call "$VCVARS" >nul
cd /d $(cygpath -w "$REPO")
cl /EHsc /std:c++17 /nologo /Zi /D_USE_MATH_DEFINES $FLAGS /Fe:build\\ref\\soil_sweep_ref.exe /Fo:"build\\ref\\soil_sweep\\\\" /Fd:"build\\ref\\soil_sweep\\\\" odin\\tests\\cpp_ref\\soil_pwp_fc_sat_ref_main.cpp /link /LIBPATH:build /LIBPATH:$VCPKG_LIB monica_run_lib.lib monica_lib.lib climate\\climate-file-io\\climate_file_io_lib.lib climate\\climate-file-io\\climate_common\\climate_common_lib.lib common\\common_lib.lib common\\json11\\json11_lib.lib common\\json11\\date\\date_lib.lib common\\tools\\helpers\\helpers_lib.lib tools\\debug\\debug_lib.lib soil\\soil_lib.lib mas-infrastructure\\capnproto_schemas\\capnp_schemas_lib.lib capnp-json.lib capnp-rpc.lib capnp.lib capnpc.lib kj-http.lib kj-gzip.lib kj-async.lib kj.lib zlibd.lib libsodium.lib advapi32.lib ws2_32.lib
EOF
cmd //c "$(cygpath -w build/ref/_build_soil_sweep.bat)" > build/ref/_build_soil_sweep.log 2>&1
if [ ! -f build/ref/soil_sweep_ref.exe ]; then
  echo "FAILED to build the C++ driver; see build/ref/_build_soil_sweep.log"
  grep -iE "error" build/ref/_build_soil_sweep.log | head -12
  exit 1
fi

echo "== building Odin driver"
"$ODIN" build odin/tests/soil_pwp_fc_sat_ref -out:build/ref/soil_sweep_ref_odin.exe || exit 1

echo "== running both"
./build/ref/soil_sweep_ref.exe      > build/ref/soil_sweep_cpp.txt  2>build/ref/soil_sweep_cpp.err
./build/ref/soil_sweep_ref_odin.exe > build/ref/soil_sweep_odin.txt 2>build/ref/soil_sweep_odin.err

n=$(wc -l < build/ref/soil_sweep_cpp.txt)
if [ "$n" -lt 1000 ]; then
  echo "FAIL - C++ driver produced almost no output ($n rows); stderr:"
  head -5 build/ref/soil_sweep_cpp.err
  exit 1
fi

echo "== diffing"
if diff <(tr -d '\r' < build/ref/soil_sweep_cpp.txt) <(tr -d '\r' < build/ref/soil_sweep_odin.txt) > build/ref/soil_sweep_diff.txt; then
  echo "PASS - pwp/fc/sat interpolation sweep identical ($n rows)"
  exit 0
else
  echo "FAIL - diff (truncated to 400 cols):"
  cut -c1-400 build/ref/soil_sweep_diff.txt | head -20
  exit 1
fi
