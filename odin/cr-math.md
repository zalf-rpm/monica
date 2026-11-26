# Why CORE-MATH: correct rounding and the "last bit"

The rule is in `CONVENTIONS.md` §1 ("Transcendental functions: `cr_math` only"): every `exp`,
`log`, `pow`, `sin`, ... goes through `support/cr_math`, i.e. CORE-MATH, the library the C++
MONICA calls as well (`cr::exp` ..., `mas_cpp_misc/cr_math`). How the switch was done is in
`../plan-cr-math.md`. This document explains *why*: what "rounding" and "the last bit" mean here,
and why one bit matters for MONICA.

## 1. Doubles are a grid of numbers

An `f64` (a C++ `double`) has 53 binary digits for the digits of the number (the significand),
plus an exponent. So it can't represent every real number, only a grid of them. Near 1.0
neighbouring doubles are 2⁻⁵² ≈ 2.2·10⁻¹⁶ apart, near 1000 about 1.1·10⁻¹³ - the spacing grows
with the magnitude. That spacing is one step in the **last bit** of the 53, also called one
**ulp** (unit in the last place).

The exact result of e.g. `exp(1)` = 2.71828182845904523536... almost never lies on the grid. It
falls between two doubles:

```
2.718281828459045 090795...   <- double A
2.718281828459045 235360...   <- exact exp(1)
2.718281828459045 534884...   <- double B (A + 1 ulp)
```

The function has to return A or B. **Correctly rounded** means: always the nearer one, here A.

## 2. Why ordinary math libraries sometimes return the other neighbour

A math library doesn't compute `exp` exactly. It uses a fast approximation (polynomials, tables)
that is very accurate, but not perfect - the error is typically a fraction of an ulp.

That is harmless when the exact value is clearly closer to one neighbour. It matters when the
exact value lies very close to the **midpoint** between A and B. A decimal analogy, with only 3
decimal places:

- the exact result is 2.7184999..., so the correct answer is **2.718**;
- library X's approximation gives 2.7184998 -> rounds to 2.718 (correct);
- library Y's approximation gives 2.7185001 -> rounds to **2.719** (one step off).

Both libraries are "accurate to within a tiny error", yet they return different numbers, one
step apart in the last digit. glibc (Linux), Microsoft's UCRT (Windows), Odin's `core:math` and
Python's libm all use different approximations, so for a small fraction of arguments they
disagree - always by one step in the last bit. Even the same library can differ between
compilers: GCC evaluates math calls with constant arguments at compile time, with its own
arithmetic, which made GCC 16 builds differ from GCC 15 builds.

## 3. What CORE-MATH does differently

CORE-MATH guarantees the nearer neighbour for every argument. It first computes a fast
approximation together with a bound on its error. If the exact value is provably on one side of
the midpoint - almost always - it is done. If the exact value is too close to the midpoint to
decide, it recomputes with much higher precision until it can. The authors proved for each
function that this holds for every possible input.

So there is only one right answer per argument, and every platform and compiler gives it.

## 4. Why only the transcendental functions

The IEEE 754 standard already **requires** `+ - * /` and `sqrt` to be correctly rounded, so they
give identical bits everywhere. For `exp`, `log`, `pow`, `sin`, ... correct rounding is only
*recommended* - it used to be considered too expensive. That gap is what CORE-MATH fills, and why
only these functions go through `cr_math`; `sqrt`, `fabs`, `floor`, `round` stay as they are
(`CONVENTIONS.md` §1).

What correct rounding of single functions does *not* cover: the order of operations. `a*b + a*c`
and `a*(b+c)` are rounded at different steps and can differ in the last bit too, and so can a
fused multiply-add (`a*b + c` rounded once instead of twice) that a compiler uses on its own.
Hence the other rules in `CONVENTIONS.md` §1: keep the C++'s statement order, and no FMA
contraction in MONICA's own code (FMA instructions only inside CORE-MATH, which is built with
`-ffp-contract=off` and gives the same results with or without them).

## 5. Why one bit matters for MONICA

A relative difference of 10⁻¹⁶ is physically meaningless, but it doesn't stay small or harmless:

- **It grows.** A value feeds the next day's state, thousands of days in a row, through many
  nonlinear formulas. A last-bit difference can grow until it shows up in the printed digits.
- **It flips decisions.** A comparison like `if soil_moisture > threshold`, or reaching a
  temperature sum for the next developmental stage, can go the other way, and from then on the
  runs really differ.
- **It hides real errors.** If outputs may differ "a little", byte-identical comparisons are
  useless, and a harmless rounding difference can't be told from a real porting bug.

Measured before the switch: C++ builds on Windows (MSVC + UCRT) and Linux (GCC + glibc) differed
in all 48 muencheberg_v140 simulations, in 8-21% of all output values, harvest yields by up to
3.3%, starting after a few simulated years - the model state diverged, not just the output
rounding (`../plan-cr-math.md`).

With correctly rounded functions on both sides, Odin and C++, on Linux and Windows, produce the
same bits, and any difference in the test suites points to a real change in the code.

## 6. Crop2ML modules

PyCrop2ML's Odin generator uses Odin's `core:math` by default: a generated package should run on
its own, without MONICA's source tree or the CORE-MATH C library. It imports `cr_math` instead
when the environment variable `CYML_ODIN_MATH` gives the path to it; `pixi run gen-amei` sets it
(`../../support/cr_math`, as seen from `amei/<package>/`). That's why the packages in `amei/`
differ from the Odin code in the Crop2ML repositories only in the math import and calls - and
why a module like Monica_SoilTemp, integrated, reproduces MONICA's own results bit for bit.

The generated Python uses the platform's libm, so `tests/check_modules.sh` compares Odin and
Python with a relative tolerance of 10⁻¹², not exactly.
