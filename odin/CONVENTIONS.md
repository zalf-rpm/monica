# Odin port conventions

Read this before writing any Odin in this tree. Every later file copies these decisions; changing
one of them later means touching thousands of lines.

See `../plan-odin.md` for the phase plan and `../plan.md` for the C++ refactoring history.

## 1. The prime directive: translate literally

**Do not improve anything.** The acceptance test is byte-identical CSV output against the C++
build, so any "cleanup" is a potential regression with no upside.

- Keep C++ procedure names and argument order. Variable names originally kept their C++ spelling,
  including the `vc_` / `vs_` / `vw_` / `pc_` / `vm_` / `vo_` prefixes; the post-port cleanup
  snake_cased them and dropped the prefixes, everywhere since 2026-10-28: struct fields, locals,
  parameters and procedures, leading underscores dropped and scientific symbols lowercased
  (`EF_ISO` -> `ef_iso`). The one exception are the `__enable_..._fix__` feature flags, named
  after their JSON keys. New code follows the same rule. The trace oracle doesn't depend on local
  names (every `tr.dump` label is a separate string literal), and `NAME_MAP.md` maps struct fields
  back to C++.
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

### Rounding: `double`, like the C++

`mas_cpp_misc`'s `round_to_digits` (behind `Tools::round(x, digits)`) computes in `double` on
every platform. It used `long double`, which is 80 bits with GCC on x86-64 but 64 bits with MSVC,
so Linux and Windows rounded ties differently (0.015 -> 0.01 vs. 0.02, seen in RelDev).
`tools.round`'s `f64` matches it exactly.

### Transcendental functions: `cr_math` only

**Every `exp`, `log`, `log2`, `log10`, `pow`, `sin`, `cos`, `tan`, `asin`, `acos` and `atan` call
goes through `support/cr_math`** - CORE-MATH, the same library the C++ MONICA calls (`cr::exp` ...,
`mas_cpp_misc/cr_math`). Never `core:math`'s and never `core:c/libc`'s.

Why: the platform math libraries (glibc, Microsoft's UCRT) return results within about one unit
in the last place, but for different arguments a different neighbour of the exact value - and
`core:math` is a third, pure-Odin implementation with its own last bits. MONICA is sensitive
enough that one last-bit difference in an `exp`/`pow` result moves harvest yields by several
percent a few years later: C++ builds on Windows and Linux differed in 8-21% of all output values
over 48 simulations until the switch. The same mechanism made GCC 16 builds differ from GCC 15
(compile-time evaluation of math calls with constant arguments). CORE-MATH is *correctly
rounded*: every call returns the double nearest to the exact result, so every platform and
compiler gives bit-identical results. History: the port first used `core:c/libc` here, after a
`pow(pow(x, 1/m) - 1, 1/n)` chain in `soilMoisturePF` diverged from the C++ in the last 2 ULP
(phase 3, `plan-odin.md`) - but libc only matched the C++ on Windows, where both called the MSVC
CRT. With `cr_math` on both sides, Odin and C++ can match on Windows and Linux.

What stays: `libc.sqrt`, `math.sqrt` and `libc.fabs` (IEEE 754 makes `sqrt` correctly rounded
everywhere, and `core:math`'s is the intrinsic, the CPU's instruction; `fabs` is exact), and `core:math`'s non-transcendental helpers (`math.round`, `math.floor`,
`math.abs`, `math.PI`, ...). A function `cr_math` doesn't have yet (`atan2`, `sinh`, `exp2`, ...)
gets added on the C++ side first (`mas_cpp_misc/cr_math/README.md`), then to
`support/cr_math/cr_math.odin` - so both sides keep calling the same code.

`tools/odinw.py` builds the library from the `mas_cpp_misc` submodule into `build/cr_math/` (on
demand in `build`/`test`, explicitly with `pixi run python tools/odinw.py cr-math`), with
`mas_cpp_misc`'s own CMake flags: `-ffp-contract=off` and FMA instructions (`CR_MATH_FMA`, as in
the C++ build), so the executables need an x86-64 CPU with FMA (Intel 2013+, AMD 2012+).

**No FMA contraction in the port's own code.** With identical math functions, remaining
differences can only come from code generation, e.g. LLVM fusing `a*b+c` into one rounding. Odin
targets a generic x86-64 by default, which has no FMA instruction, so it can't; keep it that way
(no `-microarch:native` or FMA target features for MONICA's code). FMA instructions in a binary
must come only from `cr_math`'s objects. And keep the evaluation order of expressions identical to
the C++ - floating-point addition and multiplication are not associative.

**Enforced, not just documented:** `odin/tests/conventions_test.odin`'s
`test_transcendentals_use_cr_math` fails on any `math.<name>(` or `libc.<name>(` (except
`sqrt`) for the C `<math.h>` §7.12 trig/hyperbolic/exponential/logarithmic/power/gamma
families, as part of `odin test odin/tests`; `bash odin/tests/check_transcendentals.sh` is the
same check as a quick standalone script.

## 2. Naming

C++ is `namespace::camelCase`; Odin is `package` + `snake_case`. Map mechanically:

| C++ | Odin |
| --- | --- |
| `Tools::Errors` | `tools.Errors` |
| `Tools::readFile` | `tools.read_file` |
| `Tools::Date` | `date.Date` |
| `monica::makeSoilColumn` | `core.make_soil_column` |
| struct field `vs_SoilMoisture_m3` | field `vs_soil_moisture_m3` |

**Superseded:** struct fields used to keep their exact C++ spelling unchanged, specifically so the
trace diff and the output-path aliases (§9) could match on name alone. As of the `Crop_Module`
cleanup, fields are snake_cased like everything else instead — see `odin/NAME_MAP.md` for the
C++-name -> Odin-name lookup this displaces, one table per renamed struct. The trace diff needs no
extra plumbing for this: every `_ref` test's `tr.dump(t, "struct.OldCppName", cm.new_odin_name)` call
already hardcodes the C++-matching label as a string literal, independent of the Odin identifier
actually being read (see `trace.odin`'s "the walk" section) — so only the output alias paths (§9)
and any new `_ref` fixture need the mapping table, not the trace mechanism itself.

**Correction to an earlier plan (superseded by what phase 3-4 actually built):** `src/core/`
becomes **one** Odin package, `core` — per C++ *directory*, not per C++ *namespace* — not a separate
Odin package per `monica::soilcolumn`/`monica::soiltemperature`/`monica::soilmoisture`/... the way
`monica::soilmoisture::step -> soilmoisture.step` might suggest. `SoilColumn`, `SoilTemperature`,
`SoilMoisture`, `SnowComponent`, `FrostComponent` and their free procedures all live in
`odin/monica/core/`. Two C++ namespaces both declaring a same-named free function (`soiltemperature::
step` and `soilmoisture::step`) therefore collide in the flat package — disambiguate by prefixing
with the owning struct's snake_case name (`soil_temperature_step`, `soil_moisture_step`), not by
picking one to leave bare. Check for a name collision before adding any new free procedure to `core`.
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
    reflectpath/      <- (Phase 10) compiled field-path resolver; no monica import
  monica/             <- (Phase 3+) port of src/
  tests/
```

`support/tools` deliberately does **not** import `support/date` (and vice versa) except where the
C++ does, to keep the dependency graph acyclic.

## 7. Testing

`odin test odin/tests` — run it **from the repo root**, not from `odin/`:
`tests/conventions_test.odin` walks the literal relative path `odin`, so it fails with `ENOENT`
from anywhere else.

Or `pixi run test` (see §8), which sets that working directory for you.

Two kinds of test, both required for anything numeric:

1. **Ported C++ assertions.** `Tools::testDate()` and `Tools::testRoundFloorCeil()` are ground
   truth copied verbatim from the C++ source — every assertion there must pass unchanged.
2. **Differential sweeps.** For `Date`, walk a multi-year range day by day and check that
   round-tripping, `julianDay`, `numberOfDaysTo` and `+`/`-` stay self-consistent and agree with
   the C++ where a reference dump exists.

From Phase 4 the primary oracle becomes the daily trace diff (see `../plan-odin.md` §1), not unit
tests.

## 8. Building reproducibly (`pixi.toml`)

`pixi run build` produces `build/monica-run` and `build/monica-zmq-server`; `pixi run test` runs the
suite above. Verified byte-identical `sim-min.json` output against a system-toolchain build.

Two halves are pinned separately, because **Odin is not packaged on conda-forge**:

- the conda-forge dependencies (`clang` on Linux, `zeromq` 4.3.5 everywhere) via `pixi.lock`;
- the compiler itself by release tag **and SHA256** in `tools/odinw.py` (`ODIN_TAG`). Bump all
  three fields there together.

**Windows additionally needs an MSVC toolchain, and always will.** Odin cannot link a PE binary
without MSVC's `lib\x64` plus the Windows SDK `um\x64` / `ucrt\x64` import libraries; conda-forge
ships none of those, and the Windows SDK EULA permits redistributing only "the results of running
such Distributable Code through a linker", never the `.lib` files. `-linker:lld` does **not** avoid
this — Odin ships its own `bin/lld-link.exe` and still hard-fails with `VS library path not found`,
a check that is not gated on the linker choice. `tools/odinw.py` resolves a toolchain in this order:
an already-active developer environment → `$VCVARS` (the same knob `tests/cpp_ref/run*.sh` use) → a
locally installed Visual Studio via `vswhere` → a portable toolchain from
`pixi run setup-msvc -- --accept-license`. That last one downloads Microsoft's compiler under the
Visual Studio license into `msvc/`, which is gitignored and **must not be redistributed**.

Anything under a hidden directory (`.odin/`, `.pixi/`) is out of scope for the §1 transcendentals
guard — the bootstrapped compiler's own `core` library legitimately calls `math.pow`/`math.atanh`.

## 9. Output ids: prefer a path over a lambda

`monica/io/output_paths.odin` maps a legacy output name to a *path* into `Monica_Model`
(`"soil_column.layers.soil_nh4"`), which `support/reflectpath` resolves. Adding an output that is
just "read this field, round to N digits" is one table row, not a new proc - and
`python odin/tools/gen_output_aliases.py` regenerates the whole table from
`src/io/build-output.cpp`. Write a lambda in `build_output.odin` only when the value is genuinely
computed: a proc call, arithmetic over two fields, a `Date` method, or a guard that is not the
array bound. See `../plan-reflective-outputs.md`.

The same table is the *write* side: the SetValue workstep sets any field a path reaches, through
`oid_set_value`. Do not add a `setf` for something a path can already reach.

Two consequences for the rules above. Field names keep mattering for a new reason - path *segments*
are what make the 125 generated alias rows resolve, and `odin/tests/output_paths_test.odin` fails if
one drifts. Since §2's "unchanged" rule was superseded, `gen_output_aliases.py` walks each C++
path through the Odin struct definitions and renames every segment: `odin/NAME_MAP.md` first, then
the port's convention (C++ prefix dropped, snake_cased), accepting a name only if that field
exists. So after renaming a field, add its row to `NAME_MAP.md` (needed only when the convention
doesn't produce the new name) and regenerate. The script exits non-zero and lists every path it
can't resolve, and it never emits a guessed name. A field that moved to a different struct goes
in the script's `MOVED` table. Never rename an alias's *key*: that is the C++ output name a
sim.json asks for, and `test_every_alias_is_a_cpp_output_name` fails if one stops matching
`src/io/build-output.cpp`. And a path leaf reached through a nil
pointer or an unset `Maybe` yields *missing*, which the output layer turns into `0.0` - that is
the C++ `... ? ... : 0.0` ternary, not a violation of §5; §5 still governs everything that reads
a `Maybe` in model code.

## 9a. Units: a `unit` tag on the field

A field's unit is a struct tag on its declaration, so it changes together with the field:

```odin
actual_evaporation: f64 `unit:"mm"`,
layer_temperature:  []f64 `unit:"°C"`, // each element
```

`support/reflectpath` reads the tag of the last field a path names, once when the path is
compiled, and the output prints it in the units row (`output."csv-options"."include-units-row"`).
A day's climate data is a map, not fields: its units are `climate.ACD_UNITS`, next to the `ACD`
enum. Write units like MONICA's output table (`"kgN ha-1"`, `"m3 m-3"`, `"mm"` for a day's amount),
`"0-1"` for fractions, `"1"` for other dimensionless numbers, and `unit:""` for what is no
quantity (flags, categories). Don't repeat the unit in the comment.

Legacy output names keep the unit of the output table (`build_output.odin`, `output_paths.odin`),
so existing outputs stay byte-identical; some of those are wrong (e.g. `RootWaUptak` in
`KgN ha-1`), the tags aren't. `tests/output_units_calc_test.odin` checks that every field behind a
legacy alias and every field of the modules' `Input`/`Output` structs has a tag. The parameter
structs (`monica/params`) carry the units documented in `monica-parameters` (`[value, "unit"]`)
or in the C++ header `monica-parameters.h` - 84 of 287 numeric fields; the rest, mostly crop species
and cultivar parameters, have no documented unit yet. Not tagged yet either: most other fields. The generated Crop2ML packages (`odin/amei/`) get theirs from the
XML (`plan-crop2ml-modules.md` §2).

An object-form output column may override the unit (`"unit"`) and transform the value
(`"calc"`, `monica/io/output_calc.odin`); a `"calc"` without `"unit"` prints no unit.
