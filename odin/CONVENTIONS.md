# Odin port conventions

Read this before writing any Odin in this tree. Every later file copies these decisions; changing
one of them later means touching thousands of lines.

See `../plan-odin.md` for the phase plan and `../plan.md` for the C++ refactoring history.

## 1. The prime directive: translate literally

**Do not improve anything.** The acceptance test is byte-identical CSV output against the C++
build, so any "cleanup" is a potential regression with no upside.

- Keep C++ procedure names, argument order and **variable names**, including the `vc_` / `vs_` /
  `vw_` / `pc_` / `vm_` / `vo_` prefixes. A trace diff is only useful if both sides name the same
  thing.
- Keep statement order in numeric routines. Reassociating float arithmetic (`a*b + a*c` ->
  `a*(b+c)`) changes the last bits.
- Port one C++ procedure at a time. Do not merge, split or inline.
- When the C++ has a quirk or an outright bug, **reproduce it and comment it** with
  `// NOTE(c++-quirk): ...`. Several already exist in `support/date` — see `date_to_absolute_date`.

### Mixed int/float arithmetic

C++ promotes `int` to `double` implicitly in mixed expressions; Odin requires explicit conversion.
Casting the wrong operand silently turns float division into truncating integer division. **For
every arithmetic line containing an integer variable or literal, check the C++ promotion rules
before writing the Odin.** This class of bug already bit the C++ branch once.

```odin
// C++: double x = a / b;   with int a, b   -> integer division, THEN widened
x := f64(a / b)     // correct
// C++: double x = a / double(b);
x := f64(a) / f64(b)  // correct
```

Odin's `/` and `%` on signed integers truncate toward zero, matching C/C++. `f64 -> int`
conversion truncates toward zero, matching C++. `math.round` rounds half away from zero, matching
`std::round`.

### `long double`

`mas_cpp_misc/tools/algorithms.h` uses `long double` in `round_to_digits`. On MSVC (the reference
build) `long double == double`, so `f64` is exact. On a GCC/Linux reference build it would be 80-bit
and results could differ in the last bit — if the baseline is ever regenerated on Linux, revisit
`tools.round`.

### Transcendental functions: `core:c/libc`, not `core:math`

`core:math`'s `pow`/`log`/`log10`/`exp`/... are a pure-Odin implementation and are **not always
bit-identical** to the C++ reference build's `<cmath>` calls (which resolve to the MSVC CRT).
Confirmed in phase 3 (`soillayer::soilMoisturePF`, `plan-odin.md`): a `pow(pow(x, 1/m) - 1, 1/n)`
chain diverged in the last 2 ULP against the C++ reference on a real fixture, while every other
value in that oracle (~1,440 of them, plus the unrelated 15,978-row interpolation sweep, which
also calls `pow`/`exp` heavily) matched exactly — so this is a real but *intermittent* risk, not a
translation bug, and not something a differential test on a handful of inputs reliably catches.

**Use `core:c/libc`'s `pow`/`log`/`log10`/`exp`/`sqrt`/... instead of `core:math`'s.** They are FFI
bindings to the platform C runtime, so on Windows they call the exact same function the C++
reference build does, eliminating the risk entirely rather than hoping a given input doesn't hit a
divergent case. This matters far more from phase 4 onward — `crop-module.cpp` alone is ~4,750 lines
of agronomy arithmetic leaning on `exp`/`pow` for photosynthesis, respiration and phenology curves.
`core:math` is still fine for the non-transcendental helpers (`math.round`, `math.floor`, `math.abs`,
comparisons, ...) — this rule is specifically about `pow`/`exp`/`log`-family functions.

## 2. Naming

C++ is `namespace::camelCase`; Odin is `package` + `snake_case`. Map mechanically:

| C++ | Odin |
| --- | --- |
| `Tools::Errors` | `tools.Errors` |
| `Tools::readFile` | `tools.read_file` |
| `Tools::Date` | `date.Date` |
| `monica::soilmoisture::step` | `soilmoisture.step` |
| `monica::makeSoilColumn` | `soilcolumn.make_soil_column` |
| struct field `vs_SoilMoisture_m3` | field `vs_SoilMoisture_m3` (**unchanged**) |

Struct **field** names keep their exact C++ spelling — they are what the trace diff matches on.
Only procedure and package names get snake_cased.

C++ free procedures that live in a lowercase namespace (`monica::soilmoisture::...`) become a
package of the same name, so the call site reads almost identically.

## 3. Memory

- **Config JSON (Phase 1) lives in an arena and is never individually freed.** `json.Value` owns
  its `Array`/`Object`; shallow-copying it — which the C++ does freely, e.g.
  `findAndReplaceReferences` — would double-free under a tracking allocator. One
  `virtual.Arena` for the whole config, destroyed at exit.
- Procedures that allocate take `allocator := context.allocator` as the last parameter.
- `Errors` owns its strings (they are cloned on append). Call `errors_destroy` if you are not
  running under an arena.
- The model state (`MonicaModel` and its submodules) is heap-allocated **once** and never moved:
  submodules hold back-pointers to `SoilColumn`. Take pointers only after allocation.

## 3a. JSON: known `core:encoding/json` divergences from json11

Established in phase 1 by re-dumping 455 real MONICA parameter files through both
implementations (`odin/tests/cpp_ref/run_json.sh`). Two behavioural differences exist; both are
accepted rather than worked around, and both are pinned by tests so a core-library update that
changes them is noticed.

1. **Empty object keys are silently dropped.** `core/encoding/json/parser.odin` has an explicit
   `if key != ""` guard, so `{"": 13.1}` parses without error but loses the entry. Exactly one
   parameter file hits this (`monica-parameters/projects/fnr-voce/maize.json`, evidently a typo),
   it is not part of the Hohenfinow2 fixture, and MONICA never looks up `""`. Only empty keys are
   affected — every other key is inserted normally.
2. **Duplicate object keys are an error, not last-wins.** Odin returns `.Duplicate_Object_Key`;
   json11 overwrites. No MONICA parameter file currently has duplicate keys (all 455 agreed on
   parse success/failure), but a future one would fail loudly in Odin and silently in C++.

Everything else matches byte for byte, including number classification, `%.17g` double formatting,
sorted key order and string escaping.

## 4. Error handling

Port `Tools::Errors` / `Tools::EResult<T>` rather than switching to Odin's `->  (T, Error)`
idiom. MONICA accumulates *multiple* errors and warnings across a merge and inspects them later;
that is not what a single return-value error models.

```odin
errs: tools.Errors
tools.append_error(&errs, "something went wrong")
if tools.failure(errs) { ... }

res := tools.EResult(f64){ result = 1.5 }
```

## 5. `Maybe`

C++ `Tools::Maybe<T>` and `kj::Maybe<T>` both map to Odin's builtin `Maybe(T)` (a `union{T}`).

**The three-state semantics are load-bearing.** `unset` must never collapse to the zero value —
that is exactly the `__enable_vernalisation_factor_fix__` bug documented in `../plan.md`. When the
C++ reads `x.orDefault(fallback)`, the Odin must read
`v, ok := x.?; value := ok ? v : fallback` — never `x.? or_else T{}`.

## 6. Layout

```
odin/
  CONVENTIONS.md      <- this file
  support/            <- port of the needed mas_cpp_misc subset
    tools/            <- Errors/EResult, string, file and algorithm helpers
    date/             <- Tools::Date
    jsonx/            <- (Phase 1) forgiving json11-style accessors
    climate/          <- (Phase 2) DataAccessor + CSV reader
  monica/             <- (Phase 3+) port of src/
  tests/
```

`support/tools` deliberately does **not** import `support/date` (and vice versa) except where the
C++ does, to keep the dependency graph acyclic.

## 7. Testing

`odin test odin/tests`.

Two kinds of test, both required for anything numeric:

1. **Ported C++ assertions.** `Tools::testDate()` and `Tools::testRoundFloorCeil()` are ground
   truth copied verbatim from the C++ source — every assertion there must pass unchanged.
2. **Differential sweeps.** For `Date`, walk a multi-year range day by day and check that
   round-tripping, `julianDay`, `numberOfDaysTo` and `+`/`-` stay self-consistent and agree with
   the C++ where a reference dump exists.

From Phase 4 the primary oracle becomes the daily trace diff (see `../plan-odin.md` §1), not unit
tests.
