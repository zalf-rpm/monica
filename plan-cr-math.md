# Plan: switch the Odin port's transcendental functions to CORE-MATH (cr_math)

Written 2026-10-20 from the C++ side (zalf-rpm/monica), for whoever does this
in the Odin port. It replaces the rule in `odin/CONVENTIONS.md` §1
"Transcendental functions: `core:c/libc`, not `core:math`".

## Why

The C++ MONICA now gets a test suite (`monica/tests/`, runs Hohenfinow2 and
the 48 muencheberg_v140 simulations with any `monica-run` and compares
outputs value by value). Its first runs showed:

- **Windows (MSVC + UCRT) vs. Linux (GCC + glibc) differ in all 48 v140
  simulations** - 8-21% of all output values, harvest yields up to 3.3%,
  starting after a few simulated years. Not output rounding: the model state
  diverges.
- The cause is the platform math library: glibc and Microsoft's UCRT both
  return results within ~1 ULP, but for different arguments a different
  neighbour of the exact value. The model is sensitive enough that a single
  last-bit difference in an `exp`/`pow` result moves yields by several percent
  a few years later.
- The same mechanism explained why GCC 16 builds differed from GCC 15 builds
  (compile-time evaluation of math calls with constant arguments).

The fix: all of `exp`, `log`, `log2`, `log10`, `pow`, `sin`, `cos`, `tan`,
`asin`, `acos`, `atan` go to **CORE-MATH**, which is *correctly rounded*:
every call returns the double nearest to the exact result, so any platform
and compiler gives bit-identical results. Verified on Linux: GCC 15 and GCC 16
builds are identical with it; results change once vs. glibc (expected).

For the Odin port this means: `libc.exp` etc. only matched the C++ reference
*on Windows* (both called the MSVC CRT). With cr_math on both sides, Odin and
C++ can match on Windows **and** Linux.

## State of the C++ side

- `mas_cpp_misc`, branch `cr-math` (commits `25cd290`, `a33decc`; merge into
  `main` pending): new directory `cr_math/` with
  - `core-math/` - the 11 CORE-MATH sources, vendored unchanged (MIT),
  - `cr_math.h` - C declarations `double cr_exp(double)` ... plus C++
    wrappers `cr::exp` ...,
  - `CMakeLists.txt` - static library `cr_math_lib`. GCC/Clang compile the
    sources directly; under MSVC they are compiled with **clang-cl** (MSVC
    can't compile CORE-MATH's GNU C: `__int128`, `x86intrin.h`) into
    MSVC-compatible objects with `/Zl`. Option `CR_MATH_FMA` (default ON)
    adds `-mfma`: same results, ~30% faster MONICA, but needs an x86-64 CPU
    with FMA (Intel 2013+, AMD 2012+); see `cr_math/README.md`.
- `monica`, branch `cr-math`: every call in `src/` replaced by `cr::...`;
  pixi's Windows environment gets conda-forge `clang` (provides clang-cl).
  `sqrt`, `fabs`, `floor`, ... stay with `<cmath>` - they are exact anyway.
  The `long double` `std::pow` in `tools/algorithms.h`'s integer rounding
  stays too (exact for those arguments).

- `mas_cpp_misc` `cr-math` also changes `Tools::round_to_digits` (behind
  `Tools::round(x, digits)`) for floating point from `long double` to
  `double`: with GCC on x86-64 `long double` is 80 bits, so Linux rounded
  ties differently from MSVC (0.015 -> 0.01 instead of 0.02, seen in RelDev).
  This resolves the open point in CONVENTIONS.md §1 about `tools.round` and
  80-bit `long double`: C++ now computes in `double` everywhere, like Odin's
  `f64`.

With both changes, the C++ Linux build is bit-identical to the Windows build
in all 52 simulations of monica's test suites (Windows result from CI,
2026-10-20).

## What to do in the Odin port

1. **Get the sources**: move the `mas_cpp_misc` submodule to the `cr-math`
   branch (or `main` once merged).

2. **Build `cr_math_lib`** as part of the Odin build (`tools/odinw.py`),
   like the Cap'n Proto shim: e.g.
   `cmake -S mas_cpp_misc/cr_math -B build/cr_math -DCMAKE_BUILD_TYPE=Release`
   then build. Linux: any GCC/Clang. Windows: the CMakeLists uses clang-cl
   when the generator's compiler is MSVC (also with the portable MSVC
   toolchain, if CMake identifies it as MSVC); clang-cl comes from LLVM, VS's
   "C++ Clang tools", or conda-forge `clang` in pixi. Keep `CR_MATH_FMA` at
   its default (ON) so Odin and C++ binaries have the same CPU requirement.

3. **Bind it**, e.g. `odin/support/cr_math/cr_math.odin`:

   ```odin
   package cr_math

   when ODIN_OS == .Windows {
       foreign import lib "<path>/cr_math_lib.lib"
   } else {
       foreign import lib "<path>/libcr_math_lib.a"
   }

   @(default_calling_convention = "c", link_prefix = "cr_")
   foreign lib {
       exp   :: proc(x: f64) -> f64 ---
       log   :: proc(x: f64) -> f64 ---
       log2  :: proc(x: f64) -> f64 ---
       log10 :: proc(x: f64) -> f64 ---
       pow   :: proc(x, y: f64) -> f64 ---
       sin   :: proc(x: f64) -> f64 ---
       cos   :: proc(x: f64) -> f64 ---
       tan   :: proc(x: f64) -> f64 ---
       asin  :: proc(x: f64) -> f64 ---
       acos  :: proc(x: f64) -> f64 ---
       atan  :: proc(x: f64) -> f64 ---
   }
   ```

4. **Replace the calls**: today (2026-10-20, branch `simplify_copilot`) there
   are `libc.exp` 104x, `libc.pow` 84x, `libc.sin` 29x, `libc.cos` 11x,
   `libc.log` 10x, `libc.asin` 6x, `libc.tan` 4x, `libc.acos` 3x,
   `libc.log10` 2x and one `math.pow`. Keep `libc.sqrt`/`fabs` (exact).
   Mirror exactly what the C++ side calls: wherever C++ now has `cr::f`, Odin
   needs `cr_math.f`.

5. **Update the rule and its enforcement**: rewrite the CONVENTIONS.md
   section (cr_math instead of libc, with the reasoning above), and extend
   `odin/tests/check_libc_transcendentals.sh` and
   `conventions_test.odin`'s `test_no_core_math_transcendentals` to also
   reject `libc.<transcendental>(` - only `cr_math.` is allowed for that
   family (the functions above; others like `atan2`, `sinh`, `exp2` aren't
   in cr_math yet - if Odin needs one, add it on the C++ side first, see
   `mas_cpp_misc/cr_math/README.md`).

6. **Regenerate the C++ reference data**: `odin/tests/cpp_ref`, the `*_ref`
   fixtures and `build/out-baseline` come from C++ builds using the MSVC CRT.
   They must be regenerated from the C++ `cr-math` build - expect them to
   change. After that, Odin vs. C++ should be identical on both OSes.

7. **Arithmetic beyond the math library**: with the same math functions,
   remaining differences can only come from code generation. Make sure no FMA
   contraction happens in the Odin build (LLVM must not fuse `a*b+c`), and
   keep evaluation order identical to the C++ code. GCC (Linux C++) doesn't
   contract either (no `-mfma` for MONICA's own code), nor does MSVC by
   default.

8. **Compare with monica's test suite**: `monica/tests/run.py --monica
   <odin monica-run> --engine odin-linux --out ...` and `tests/compare.py`
   against a C++ run. It only needs `monica-run -o <out.csv> <sim.json>` and
   `--version`.
