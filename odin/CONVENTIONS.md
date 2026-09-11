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
`math.PI`, comparisons, ...) — this rule is specifically about the C `<math.h>` §7.12
trig/hyperbolic/exponential/logarithmic/power/gamma families (`pow`, `exp`/`exp2`/`expm1`, `log`/
`log2`/`log10`/`log1p`, `sqrt`/`cbrt`/`hypot`, `sin`/`cos`/`tan`/`asin`/`acos`/`atan`/`atan2`,
`sinh`/`cosh`/`tanh`/`asinh`/`acosh`/`atanh`, `erf`/`erfc`/`lgamma`/`tgamma`), not `round`/`floor`/
`ceil`/`abs`-style functions.

Swept the whole tree onto this rule (not just the sites a diff happened to catch): `support/tools/
algorithms.odin` (`round_to_digits`'s `pow`, `sunshine2global_radiation`'s full trig chain),
`support/date/date.odin` (`day_lengths`'s trig chain), `monica/soil/soil_pwp_fc_sat.odin`
(`calcVanGenuchtenVereeckenParams`/`calcVanGenuchtenTothParams`, the two functions plan-odin.md
checkpoint 3a deliberately left on `core:math` pending an "opportunistic swap if either is touched
again" — this is that swap). `monica/core/soil_column.odin`'s `soilMoisturePF` was already on
`libc.pow`/`libc.log10` from checkpoint 3a, the finding that established the rule.

**Enforced, not just documented, two ways:** `bash odin/tests/check_libc_transcendentals.sh` greps
every `*.odin` file under `odin/` for `math.<name>(` against the full §7.12 family list above and
fails if it finds one - a quick manual check. `odin/tests/conventions_test.odin`'s
`test_no_core_math_transcendentals` does the identical check as a real `@(test)` proc, so it runs
automatically every time `odin test odin/tests` does (the standard command this file's own §7
documents) instead of needing the standalone script remembered and invoked separately.

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
(`"soilColumn.layers.vs_SoilNH4"`), which `support/reflectpath` resolves. Adding an output that is
just "read this field, round to N digits" is one table row, not a new proc - and
`python odin/tools/gen_output_aliases.py` regenerates the whole table from
`src/io/build-output.cpp`. Write a lambda in `build_output.odin` only when the value is genuinely
computed: a proc call, arithmetic over two fields, a `Date` method, or a guard that is not the
array bound. See `../plan-reflective-outputs.md`.

The same table is the *write* side: the SetValue workstep sets any field a path reaches, through
`oid_set_value`. Do not add a `setf` for something a path can already reach.

Two consequences for the rules above. Field names keep mattering for a new reason - path *segments*
are what make the 125 generated alias rows resolve, and `odin/tests/output_paths_test.odin` fails if
one drifts. Since §2's "unchanged" rule was superseded, a renamed struct's alias rows in
`output_paths.odin` must be updated by hand to the new field names (see `odin/NAME_MAP.md`) -
`gen_output_aliases.py` regenerates paths straight from the C++ source and does not know about any
rename, so re-running it on a renamed struct reintroduces the old C++ names and needs the same
manual fix-up again. And a path leaf reached through a nil
pointer or an unset `Maybe` yields *missing*, which the output layer turns into `0.0` - that is
the C++ `... ? ... : 0.0` ternary, not a violation of §5; §5 still governs everything that reads
a `Maybe` in model code.
