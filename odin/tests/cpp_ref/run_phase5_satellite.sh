#!/usr/bin/env bash
# Phase 5 checkpoint 1 differential test: the four self-contained satellite
# modules crop-module.cpp depends on - photosynthesis-FvCB, O3-impact,
# voc-guenther, voc-jjv.
#
# Unlike phase 4, none of these take a SoilColumn/climate/MonicaModel, so this
# is a parameter sweep oracle (grid for FvCB, curated scenarios for the
# others), not a daily trace-diff. See phase5_satellite_ref_main.cpp's header
# comment.
#
# Usage:  bash odin/tests/cpp_ref/run_phase5_satellite.sh
#
# Requires a configured AND BUILT CMake tree (for monica_lib.lib, which these
# object files link into, and to derive compiler flags from
# compile_commands.json).
set -uo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
cd "$REPO"

ODIN="${ODIN:-/c/Users/berg/development/odin-windows-dev-2026-06/dist/odin.exe}"
VCVARS="${VCVARS:-C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat}"
CC_JSON="${CC_JSON:-build/compile_commands.json}"
VCPKG_LIB="${VCPKG_LIB:-C:\Users\berg\GitHub\vcpkg\installed\x64-windows-static\debug\lib}"

mkdir -p build/ref build/ref/phase5_satellite

[ -f "$CC_JSON" ]           || { echo "need $CC_JSON - configure the CMake build first"; exit 1; }
[ -f build/monica_lib.lib ] || { echo "need build/monica_lib.lib - build the CMake tree first"; exit 1; }

echo "== deriving compiler flags from $CC_JSON"
FLAGS=$(node -e '
const fs=require("fs");
const cc=JSON.parse(fs.readFileSync(process.argv[1],"utf8"));
const e=cc.find(x=>x.file.includes("crop-module"))||cc.find(x=>x.file.includes("monica-parameters"));
if(!e){console.error("no suitable entry in compile_commands.json");process.exit(1);}
const cmd=e.command||e.arguments.join(" ");
const keep=cmd.split(/\s+/).filter(s=>/^[-\/][ID]/.test(s)||/^-external:I/.test(s)||/^[-\/](MT|MD)d?$/.test(s));
process.stdout.write(keep.map(s=>s.replace(/^-external:I/,"/I")).join(" "));
' "$CC_JSON") || exit 1

echo "== building C++ reference driver"
cat > build/ref/_build_phase5_satellite.bat <<EOF
@echo off
call "$VCVARS" >nul
cd /d $(cygpath -w "$REPO")
cl /EHsc /std:c++17 /nologo /Zi /D_USE_MATH_DEFINES $FLAGS /Fe:build\ref\phase5_satellite_ref.exe /Fo:"build\ref\phase5_satellite\\\\" /Fd:"build\ref\phase5_satellite\\\\" odin\tests\cpp_ref\phase5_satellite_ref_main.cpp /link /LIBPATH:build /LIBPATH:$VCPKG_LIB monica_lib.lib climate\climate-file-io\climate_file_io_lib.lib climate\climate-file-io\climate_common\climate_common_lib.lib common\common_lib.lib common\json11\json11_lib.lib common\json11\date\date_lib.lib common\tools\helpers\helpers_lib.lib tools\debug\debug_lib.lib soil\soil_lib.lib mas-infrastructure\capnproto_schemas\capnp_schemas_lib.lib capnp-json.lib capnp-rpc.lib capnp.lib capnpc.lib kj-http.lib kj-gzip.lib kj-async.lib kj.lib zlibd.lib libsodium.lib advapi32.lib ws2_32.lib
EOF
cmd //c "$(cygpath -w build/ref/_build_phase5_satellite.bat)" > build/ref/_build_phase5_satellite.log 2>&1
if [ ! -f build/ref/phase5_satellite_ref.exe ]; then
  echo "FAILED to build the C++ driver; see build/ref/_build_phase5_satellite.log"
  grep -iE "error" build/ref/_build_phase5_satellite.log | head -20
  exit 1
fi

echo "== building Odin driver"
"$ODIN" build odin/tests/phase5_satellite_ref -out:build/ref/phase5_satellite_ref_odin.exe || exit 1

echo "== running both"
./build/ref/phase5_satellite_ref.exe      2>build/ref/phase5_satellite_cpp.err  | tr -d '\r' | sort > build/ref/phase5_satellite_cpp.txt
./build/ref/phase5_satellite_ref_odin.exe 2>build/ref/phase5_satellite_odin.err | tr -d '\r' | sort > build/ref/phase5_satellite_odin.txt

n=$(wc -l < build/ref/phase5_satellite_cpp.txt)
if [ "$n" -lt 1000 ]; then
  echo "FAIL - C++ driver produced only $n rows; stderr:"
  head -10 build/ref/phase5_satellite_cpp.err
  exit 1
fi

echo "== diffing"
if diff build/ref/phase5_satellite_cpp.txt build/ref/phase5_satellite_odin.txt > build/ref/phase5_satellite_diff.txt; then
  echo "PASS - phase 5 satellite-module sweep identical ($n rows)"
  exit 0
else
  echo "FAIL - diff (truncated to 400 cols):"
  cut -c1-400 build/ref/phase5_satellite_diff.txt | head -20
  exit 1
fi
