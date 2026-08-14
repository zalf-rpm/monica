#!/usr/bin/env bash
# Phase 1c differential test for odin/monica/params (the parameter structs).
#
# Merges the real monica-parameters/general/*.json files into each parameter
# struct and diffs the to_json dumps against the C++. Each struct is dumped
# twice: once default-constructed (which pins the C++ in-class initialisers) and
# once after the merge.
#
# Usage:  bash odin/tests/cpp_ref/run_params.sh
#
# Requires a configured AND BUILT CMake tree, for the same reason as run_env.sh:
# monica-parameters.h drags in the generated capnp headers and KJ, so the driver
# takes its flags from build/compile_commands.json and links the built libs.
set -uo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
cd "$REPO"

ODIN="${ODIN:-/c/Users/berg/development/odin-windows-dev-2026-06/dist/odin.exe}"
VCVARS="${VCVARS:-C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat}"
export MONICA_PARAMETERS="${MONICA_PARAMETERS:-$HOME/GitHub/monica-parameters}"
GENERAL_DIR="${GENERAL_DIR:-$MONICA_PARAMETERS/general}"
CC_JSON="${CC_JSON:-build/compile_commands.json}"
VCPKG_LIB="${VCPKG_LIB:-C:\\Users\\berg\\GitHub\\vcpkg\\installed\\x64-windows-static\\debug\\lib}"

mkdir -p build/ref build/ref/params

[ -f "$CC_JSON" ]           || { echo "need $CC_JSON - configure the CMake build first"; exit 1; }
[ -f build/monica_lib.lib ] || { echo "need build/monica_lib.lib - build the CMake tree first"; exit 1; }
[ -d "$GENERAL_DIR" ]       || { echo "need $GENERAL_DIR"; exit 1; }

echo "== deriving compiler flags from $CC_JSON"
FLAGS=$(node -e '
const fs=require("fs");
const cc=JSON.parse(fs.readFileSync(process.argv[1],"utf8"));
const e=cc.find(x=>x.file.includes("monica-parameters"))||cc.find(x=>x.file.includes("create-env-from-json-config"));
if(!e){console.error("no suitable entry in compile_commands.json");process.exit(1);}
const cmd=e.command||e.arguments.join(" ");
const keep=cmd.split(/\s+/).filter(s=>/^[-\/][ID]/.test(s)||/^-external:I/.test(s)||/^[-\/](MT|MD)d?$/.test(s));
process.stdout.write(keep.map(s=>s.replace(/^-external:I/,"/I")).join(" "));
' "$CC_JSON") || exit 1

echo "== building C++ reference driver"
cat > build/ref/_build_params.bat <<EOF
@echo off
call "$VCVARS" >nul
cd /d $(cygpath -w "$REPO")
cl /EHsc /std:c++17 /nologo /Zi /D_USE_MATH_DEFINES $FLAGS /Fe:build\\ref\\params_ref.exe /Fo:"build\\ref\\params\\\\" /Fd:"build\\ref\\params\\\\" odin\\tests\\cpp_ref\\params_ref_main.cpp /link /LIBPATH:build /LIBPATH:$VCPKG_LIB monica_run_lib.lib monica_lib.lib climate\\climate-file-io\\climate_file_io_lib.lib climate\\climate-file-io\\climate_common\\climate_common_lib.lib common\\common_lib.lib common\\json11\\json11_lib.lib common\\json11\\date\\date_lib.lib common\\tools\\helpers\\helpers_lib.lib tools\\debug\\debug_lib.lib soil\\soil_lib.lib mas-infrastructure\\capnproto_schemas\\capnp_schemas_lib.lib capnp-json.lib capnp-rpc.lib capnp.lib capnpc.lib kj-http.lib kj-gzip.lib kj-async.lib kj.lib zlibd.lib libsodium.lib advapi32.lib ws2_32.lib
EOF
cmd //c "$(cygpath -w build/ref/_build_params.bat)" > build/ref/_build_params.log 2>&1
if [ ! -f build/ref/params_ref.exe ]; then
  echo "FAILED to build the C++ driver; see build/ref/_build_params.log"
  grep -iE "error" build/ref/_build_params.log | head -12
  exit 1
fi

echo "== building Odin driver"
"$ODIN" build odin/tests/params_ref -out:build/ref/params_ref_odin.exe || exit 1

echo "== running both on $GENERAL_DIR"
./build/ref/params_ref.exe      "$GENERAL_DIR" > build/ref/params_cpp.txt  2>build/ref/params_cpp.err
./build/ref/params_ref_odin.exe "$GENERAL_DIR" > build/ref/params_odin.txt 2>build/ref/params_odin.err

# an empty dump would otherwise diff clean against another empty dump
n=$(wc -c < build/ref/params_cpp.txt)
if [ "$n" -lt 1000 ]; then
  echo "FAIL - C++ driver produced almost no output ($n bytes); stderr:"
  head -5 build/ref/params_cpp.err
  exit 1
fi

echo "== diffing"
if diff <(tr -d '\r' < build/ref/params_cpp.txt) <(tr -d '\r' < build/ref/params_odin.txt) > build/ref/params_diff.txt; then
  echo "PASS - parameter struct defaults and merges are identical ($n bytes)"
  exit 0
else
  echo "FAIL - diff (truncated to 400 cols):"
  cut -c1-400 build/ref/params_diff.txt | head -20
  exit 1
fi
