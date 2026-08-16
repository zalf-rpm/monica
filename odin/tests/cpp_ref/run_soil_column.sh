#!/usr/bin/env bash
# Phase 3 checkpoint 3b oracle: dump initial per-layer state after
# createEqualSizedSoilPMs + SoilColumn construction over the Hohenfinow2
# site-min.json/site.json fixtures; diff.
#
# site+.json is deliberately NOT used here: its SoilProfileParameters uses
# "bulk-density-class->raw-density"/"sand-and-clay->lambda" reference patterns
# that need the phase-1b find_and_replace_references pipeline to resolve -
# this driver reads SiteParameters straight off the JSON file with no
# reference resolution. Checkpoint 3c (the real siteparameters::merge wiring,
# which does go through that pipeline) is where site+.json's profile gets
# exercised.
#
# This is on top of, not instead of, the phase 3a interpolation sweep
# (run_soil_pwp_fc_sat.sh): that sweep proves fcSatPwpFromKA5textureClass
# agrees across its whole domain, this proves the real fixture's repeat-layer
# logic (createEqualSizedSoilPMs) and SoilLayer/SoilColumn assembly
# (makeSoilLayer/makeSoilColumn) produce the same 20-layer soil column.
#
# Usage:  bash odin/tests/cpp_ref/run_soil_column.sh
#
# Requires a configured AND BUILT CMake tree and $MONICA_PARAMETERS/soil/*.json.
set -uo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
cd "$REPO"

ODIN="${ODIN:-/c/Users/berg/development/odin-windows-dev-2026-06/dist/odin.exe}"
VCVARS="${VCVARS:-C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat}"
export MONICA_PARAMETERS="${MONICA_PARAMETERS:-$HOME/GitHub/monica-parameters}"
SITES="${SITES:-installer/Hohenfinow2/site-min.json installer/Hohenfinow2/site.json}"
CC_JSON="${CC_JSON:-build/compile_commands.json}"
VCPKG_LIB="${VCPKG_LIB:-C:\Users\berg\GitHub\vcpkg\installed\x64-windows-static\debug\lib}"

mkdir -p build/ref build/ref/soil_column

[ -f "$CC_JSON" ]           || { echo "need $CC_JSON - configure the CMake build first"; exit 1; }
[ -f build/monica_lib.lib ] || { echo "need build/monica_lib.lib - build the CMake tree first"; exit 1; }

echo "== deriving compiler flags from $CC_JSON"
FLAGS=$(node -e '
const fs=require("fs");
const cc=JSON.parse(fs.readFileSync(process.argv[1],"utf8"));
const e=cc.find(x=>x.file.includes("soilcolumn.cpp"))||cc.find(x=>x.file.includes("monica-parameters"));
if(!e){console.error("no suitable entry in compile_commands.json");process.exit(1);}
const cmd=e.command||e.arguments.join(" ");
const keep=cmd.split(/\s+/).filter(s=>/^[-\/][ID]/.test(s)||/^-external:I/.test(s)||/^[-\/](MT|MD)d?$/.test(s));
process.stdout.write(keep.map(s=>s.replace(/^-external:I/,"/I")).join(" "));
' "$CC_JSON") || exit 1

echo "== building C++ reference driver"
cat > build/ref/_build_soil_column.bat <<EOF
@echo off
call "$VCVARS" >nul
cd /d $(cygpath -w "$REPO")
cl /EHsc /std:c++17 /nologo /Zi /D_USE_MATH_DEFINES $FLAGS /Fe:build\\ref\\soil_column_ref.exe /Fo:"build\\ref\\soil_column\\\\" /Fd:"build\\ref\\soil_column\\\\" odin\\tests\\cpp_ref\\soil_column_ref_main.cpp /link /LIBPATH:build /LIBPATH:$VCPKG_LIB monica_run_lib.lib monica_lib.lib climate\\climate-file-io\\climate_file_io_lib.lib climate\\climate-file-io\\climate_common\\climate_common_lib.lib common\\common_lib.lib common\\json11\\json11_lib.lib common\\json11\\date\\date_lib.lib common\\tools\\helpers\\helpers_lib.lib tools\\debug\\debug_lib.lib soil\\soil_lib.lib mas-infrastructure\\capnproto_schemas\\capnp_schemas_lib.lib capnp-json.lib capnp-rpc.lib capnp.lib capnpc.lib kj-http.lib kj-gzip.lib kj-async.lib kj.lib zlibd.lib libsodium.lib advapi32.lib ws2_32.lib
EOF
cmd //c "$(cygpath -w build/ref/_build_soil_column.bat)" > build/ref/_build_soil_column.log 2>&1
if [ ! -f build/ref/soil_column_ref.exe ]; then
  echo "FAILED to build the C++ driver; see build/ref/_build_soil_column.log"
  grep -iE "error" build/ref/_build_soil_column.log | head -12
  exit 1
fi

echo "== building Odin driver"
"$ODIN" build odin/tests/soil_column_ref -out:build/ref/soil_column_ref_odin.exe || exit 1

fail=0
total=0
for SITE in $SITES; do
  echo "== running both on $SITE"
  ./build/ref/soil_column_ref.exe      "$SITE" > build/ref/soil_column_cpp.txt  2>build/ref/soil_column_cpp.err
  ./build/ref/soil_column_ref_odin.exe "$SITE" > build/ref/soil_column_odin.txt 2>build/ref/soil_column_odin.err
  n=$(wc -c < build/ref/soil_column_cpp.txt)
  if [ "$n" -lt 100 ]; then
    echo "   FAIL - C++ driver produced almost no output ($n bytes); stderr:"
    head -5 build/ref/soil_column_cpp.err
    fail=1
    continue
  fi
  if diff <(tr -d '\r' < build/ref/soil_column_cpp.txt) <(tr -d '\r' < build/ref/soil_column_odin.txt) > build/ref/soil_column_diff.txt; then
    echo "   ok - $n bytes identical"
    total=$((total + n))
  else
    echo "   FAIL - diff (truncated to 400 cols):"
    cut -c1-400 build/ref/soil_column_diff.txt | head -20
    fail=1
  fi
done

if [ "$fail" -eq 0 ]; then
  echo "PASS - SoilColumn/SoilLayer construction identical for all fixtures ($total bytes)"
  exit 0
fi
exit 1
