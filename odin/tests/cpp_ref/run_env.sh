#!/usr/bin/env bash
# Phase 1b/2 differential test for odin/monica/run (create-env-from-json-config).
#
# Runs the Hohenfinow2 fixture through both implementations up to the assembled
# Env JSON and diffs. This exercises the whole reference-resolution machinery on
# real data: include-from-file (recursive), ref (with its cache), %, the KA5
# texture conversions, humus/bulk-density classes, and the Env assembly - and,
# since phase 2, the whole climate CSV reader too: env["climateData"] is now
# produced (and diffed) by both sides. sim+.json's climate.csv-options exercises
# the header-to-acd-names rename branch, the 3-element convert-tuple branch
# (globrad divided by 100), the deDate column format, and a start/end-date
# window; sim-min.json's is the plain iso-date path.
#
# Requires a configured AND BUILT CMake tree: the C++ driver takes its compiler
# flags from build/compile_commands.json and links against the libraries the real
# build already produced (monica_lib et al). create-env-from-json-config.cpp
# transitively pulls in monica-model.h, the generated Cap'n Proto headers and KJ,
# so recompiling its dependencies standalone is not worth it.
set -uo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
cd "$REPO"

ODIN="${ODIN:-/c/Users/berg/development/odin-windows-dev-2026-06/dist/odin.exe}"
VCVARS="${VCVARS:-C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat}"
export MONICA_PARAMETERS="${MONICA_PARAMETERS:-$HOME/GitHub/monica-parameters}"
# sim-min covers include-from-file (recursive), ref and %; sim+ additionally
# covers KA5-texture-class->clay, bulk-density-class->raw-density and
# sand-and-clay->lambda, which sim-min never reaches.
SIMS="${SIMS:-installer/Hohenfinow2/sim-min.json installer/Hohenfinow2/sim+.json}"
CC_JSON="${CC_JSON:-build/compile_commands.json}"
VCPKG_LIB="${VCPKG_LIB:-C:\Users\berg\GitHub\vcpkg\installed\x64-windows-static\debug\lib}"

mkdir -p build/ref build/ref/env

[ -f "$CC_JSON" ]         || { echo "need $CC_JSON - configure the CMake build first"; exit 1; }
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
cat > build/ref/_build_env.bat <<EOF
@echo off
call "$VCVARS" >nul
cd /d $(cygpath -w "$REPO")
cl /EHsc /std:c++17 /nologo /Zi /D_USE_MATH_DEFINES $FLAGS /Fe:build\ref\env_ref.exe /Fo:"build\ref\env\\\\" /Fd:"build\ref\env\\\\" odin\tests\cpp_ref\env_ref_main.cpp /link /LIBPATH:build /LIBPATH:$VCPKG_LIB monica_run_lib.lib monica_lib.lib climate\climate-file-io\climate_file_io_lib.lib climate\climate-file-io\climate_common\climate_common_lib.lib common\common_lib.lib common\json11\json11_lib.lib common\json11\date\date_lib.lib common\tools\helpers\helpers_lib.lib tools\debug\debug_lib.lib soil\soil_lib.lib mas-infrastructure\capnproto_schemas\capnp_schemas_lib.lib capnp-json.lib capnp-rpc.lib capnp.lib capnpc.lib kj-http.lib kj-gzip.lib kj-async.lib kj.lib zlibd.lib libsodium.lib advapi32.lib ws2_32.lib
EOF
cmd //c "$(cygpath -w build/ref/_build_env.bat)" > build/ref/_build_env.log 2>&1
if [ ! -f build/ref/env_ref.exe ]; then
  echo "FAILED to build the C++ driver; see build/ref/_build_env.log"
  grep -iE "error" build/ref/_build_env.log | head -12
  exit 1
fi

echo "== building Odin driver"
"$ODIN" build odin/tests/env_ref -out:build/ref/env_ref_odin.exe || exit 1

fail=0
total=0
for SIM in $SIMS; do
  echo "== running both on $SIM"
  ./build/ref/env_ref.exe      "$SIM" > build/ref/env_cpp.txt  2>build/ref/env_cpp.err
  ./build/ref/env_ref_odin.exe "$SIM" > build/ref/env_odin.txt 2>build/ref/env_odin.err
  n=$(wc -c < build/ref/env_cpp.txt)
  if diff <(tr -d '\r' < build/ref/env_cpp.txt) <(tr -d '\r' < build/ref/env_odin.txt) > build/ref/env_diff.txt; then
    echo "   ok - $n bytes identical"
    total=$((total + n))
  else
    echo "   FAIL - diff (truncated to 400 cols):"
    cut -c1-400 build/ref/env_diff.txt | head -20
    fail=1
  fi
done

if [ "$fail" -eq 0 ]; then
  echo "PASS - resolved crop/site/sim and the assembled Env identical for all fixtures ($total bytes)"
  exit 0
fi
exit 1
