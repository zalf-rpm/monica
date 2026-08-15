#!/usr/bin/env bash
# Phase 1 capstone differential test: the CentralParameterProvider checkpoint.
#
# A *full* env_to_json diff isn't achievable yet - Env::to_json also emits
# cropRotation (-> CultivationMethod -> Workstep, phase 6) and climateData
# (-> DataAccessor, phase 2), neither of which exists. Instead this takes the
# "params" sub-object of the assembled Env (exactly what run_env.sh already
# proves identical between the two implementations), merges it into
# CentralParameterProvider on both sides, and diffs centralparameterprovider::
# to_json. That wires all 26 tranche-1c parameter structs together against
# real fixture data in one shot - the thing per-struct tests (run_params.sh)
# cannot catch: a field read under the wrong key, or a sub-struct never
# reached because its parent key is misspelled.
#
# Usage:  bash odin/tests/cpp_ref/run_central_params.sh
#
# Requires a configured AND BUILT CMake tree, for the same reason as
# run_env.sh: create-env-from-json-config.h drags in monica-model.h, the
# generated Cap'n Proto headers and KJ, so the driver takes its compiler flags
# from build/compile_commands.json and links the built libs.
set -uo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
cd "$REPO"

ODIN="${ODIN:-/c/Users/berg/development/odin-windows-dev-2026-06/dist/odin.exe}"
VCVARS="${VCVARS:-C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat}"
export MONICA_PARAMETERS="${MONICA_PARAMETERS:-$HOME/GitHub/monica-parameters}"
SIMS="${SIMS:-installer/Hohenfinow2/sim-min.json installer/Hohenfinow2/sim+.json}"
CC_JSON="${CC_JSON:-build/compile_commands.json}"
VCPKG_LIB="${VCPKG_LIB:-C:\Users\berg\GitHub\vcpkg\installed\x64-windows-static\debug\lib}"

mkdir -p build/ref build/ref/central_params

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
cat > build/ref/_build_central_params.bat <<EOF
@echo off
call "$VCVARS" >nul
cd /d $(cygpath -w "$REPO")
cl /EHsc /std:c++17 /nologo /Zi /D_USE_MATH_DEFINES $FLAGS /Fe:build\ref\central_params_ref.exe /Fo:"build\ref\central_params\\\\" /Fd:"build\ref\central_params\\\\" odin\tests\cpp_ref\central_params_ref_main.cpp /link /LIBPATH:build /LIBPATH:$VCPKG_LIB monica_run_lib.lib monica_lib.lib climate\climate-file-io\climate_file_io_lib.lib climate\climate-file-io\climate_common\climate_common_lib.lib common\common_lib.lib common\json11\json11_lib.lib common\json11\date\date_lib.lib common\tools\helpers\helpers_lib.lib tools\debug\debug_lib.lib soil\soil_lib.lib mas-infrastructure\capnproto_schemas\capnp_schemas_lib.lib capnp-json.lib capnp-rpc.lib capnp.lib capnpc.lib kj-http.lib kj-gzip.lib kj-async.lib kj.lib zlibd.lib libsodium.lib advapi32.lib ws2_32.lib
EOF
cmd //c "$(cygpath -w build/ref/_build_central_params.bat)" > build/ref/_build_central_params.log 2>&1
if [ ! -f build/ref/central_params_ref.exe ]; then
  echo "FAILED to build the C++ driver; see build/ref/_build_central_params.log"
  grep -iE "error" build/ref/_build_central_params.log | head -12
  exit 1
fi

echo "== building Odin driver"
"$ODIN" build odin/tests/central_params_ref -out:build/ref/central_params_ref_odin.exe || exit 1

fail=0
total=0
for SIM in $SIMS; do
  echo "== running both on $SIM"
  ./build/ref/central_params_ref.exe      "$SIM" > build/ref/central_params_cpp.txt  2>build/ref/central_params_cpp.err
  ./build/ref/central_params_ref_odin.exe "$SIM" > build/ref/central_params_odin.txt 2>build/ref/central_params_odin.err
  n=$(wc -c < build/ref/central_params_cpp.txt)
  if [ "$n" -lt 1000 ]; then
    echo "   FAIL - C++ driver produced almost no output ($n bytes); stderr:"
    head -5 build/ref/central_params_cpp.err
    fail=1
    continue
  fi
  if diff <(tr -d '\r' < build/ref/central_params_cpp.txt) <(tr -d '\r' < build/ref/central_params_odin.txt) > build/ref/central_params_diff.txt; then
    echo "   ok - $n bytes identical"
    total=$((total + n))
  else
    echo "   FAIL - diff (truncated to 400 cols):"
    cut -c1-400 build/ref/central_params_diff.txt | head -20
    fail=1
  fi
done

if [ "$fail" -eq 0 ]; then
  echo "PASS - CentralParameterProvider merged from real fixtures identical for all fixtures ($total bytes)"
  exit 0
fi
exit 1
