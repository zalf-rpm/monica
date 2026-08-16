#!/usr/bin/env bash
# Validation of the daily-trace machinery itself
# (odin/monica/trace/trace.odin + odin/tests/cpp_ref/trace_common.h).
#
# Phase 3 already proved both implementations construct an identical SoilColumn
# (run_soil_column.sh). This harness dumps that same SoilColumn through the
# trace machinery on both sides, so a disagreement here is a fault in the
# *harness* - value formatting, path construction, field coverage - rather than
# in the model. Getting that distinction right before phase 4 starts relying on
# the trace is the whole point.
#
# Both outputs are sorted before diffing, so field declaration order is
# irrelevant: the Odin side emits in struct declaration order (reflection) and
# the C++ side in whatever order the dump function lists them. A missing or
# extra field shows up as an added/removed line naming the field.
#
# Usage:  bash odin/tests/cpp_ref/run_trace.sh
#
# Requires a configured AND BUILT CMake tree, same as the other soil harnesses.
set -uo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
cd "$REPO"

ODIN="${ODIN:-/c/Users/berg/development/odin-windows-dev-2026-06/dist/odin.exe}"
VCVARS="${VCVARS:-C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat}"
export MONICA_PARAMETERS="${MONICA_PARAMETERS:-$HOME/GitHub/monica-parameters}"
SITES="${SITES:-installer/Hohenfinow2/site-min.json installer/Hohenfinow2/site+.json}"
CC_JSON="${CC_JSON:-build/compile_commands.json}"
VCPKG_LIB="${VCPKG_LIB:-C:\\Users\\berg\\GitHub\\vcpkg\\installed\\x64-windows-static\\debug\\lib}"

mkdir -p build/ref build/ref/trace

[ -f "$CC_JSON" ]           || { echo "need $CC_JSON - configure the CMake build first"; exit 1; }
[ -f build/monica_lib.lib ] || { echo "need build/monica_lib.lib - build the CMake tree first"; exit 1; }

echo "== deriving compiler flags from $CC_JSON"
FLAGS=$(node -e '
const fs=require("fs");
const cc=JSON.parse(fs.readFileSync(process.argv[1],"utf8"));
const e=cc.find(x=>x.file.includes("soilcolumn"))||cc.find(x=>x.file.includes("monica-parameters"));
if(!e){console.error("no suitable entry in compile_commands.json");process.exit(1);}
const cmd=e.command||e.arguments.join(" ");
const keep=cmd.split(/\s+/).filter(s=>/^[-\/][ID]/.test(s)||/^-external:I/.test(s)||/^[-\/](MT|MD)d?$/.test(s));
process.stdout.write(keep.map(s=>s.replace(/^-external:I/,"/I")).join(" "));
' "$CC_JSON") || exit 1

echo "== building C++ reference driver"
cat > build/ref/_build_trace.bat <<EOF
@echo off
call "$VCVARS" >nul
cd /d $(cygpath -w "$REPO")
cl /EHsc /std:c++17 /nologo /Zi /D_USE_MATH_DEFINES $FLAGS /I odin\\tests\\cpp_ref /Fe:build\\ref\\trace_ref.exe /Fo:"build\\ref\\trace\\\\" /Fd:"build\\ref\\trace\\\\" odin\\tests\\cpp_ref\\trace_ref_main.cpp /link /LIBPATH:build /LIBPATH:$VCPKG_LIB monica_run_lib.lib monica_lib.lib climate\\climate-file-io\\climate_file_io_lib.lib climate\\climate-file-io\\climate_common\\climate_common_lib.lib common\\common_lib.lib common\\json11\\json11_lib.lib common\\json11\\date\\date_lib.lib common\\tools\\helpers\\helpers_lib.lib tools\\debug\\debug_lib.lib soil\\soil_lib.lib mas-infrastructure\\capnproto_schemas\\capnp_schemas_lib.lib capnp-json.lib capnp-rpc.lib capnp.lib capnpc.lib kj-http.lib kj-gzip.lib kj-async.lib kj.lib zlibd.lib libsodium.lib advapi32.lib ws2_32.lib
EOF
cmd //c "$(cygpath -w build/ref/_build_trace.bat)" > build/ref/_build_trace.log 2>&1
if [ ! -f build/ref/trace_ref.exe ]; then
  echo "FAILED to build the C++ driver; see build/ref/_build_trace.log"
  grep -iE "error" build/ref/_build_trace.log | head -12
  exit 1
fi

echo "== building Odin driver"
"$ODIN" build odin/tests/trace_ref -out:build/ref/trace_ref_odin.exe || exit 1

fail=0
total=0
for SITE in $SITES; do
  echo "== running both on $SITE"
  ./build/ref/trace_ref.exe      "$SITE" 2>build/ref/trace_cpp.err  | tr -d '\r' | sort > build/ref/trace_cpp.txt
  ./build/ref/trace_ref_odin.exe "$SITE" 2>build/ref/trace_odin.err | tr -d '\r' | sort > build/ref/trace_odin.txt

  n=$(wc -l < build/ref/trace_cpp.txt)
  # a trace that dumped nothing would diff clean against another empty trace
  if [ "$n" -lt 100 ]; then
    echo "   FAIL - C++ trace produced only $n lines; stderr:"
    head -5 build/ref/trace_cpp.err
    fail=1
    continue
  fi

  if diff build/ref/trace_cpp.txt build/ref/trace_odin.txt > build/ref/trace_diff.txt; then
    echo "   ok - $n trace lines identical"
    total=$((total + n))
  else
    echo "   FAIL - first differing fields:"
    head -20 build/ref/trace_diff.txt
    fail=1
  fi
done

if [ "$fail" -eq 0 ]; then
  echo "PASS - trace machinery agrees across all fixtures ($total lines)"
  exit 0
fi
exit 1
