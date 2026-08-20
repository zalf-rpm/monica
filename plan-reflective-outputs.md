# Plan: reflection-driven output access

Goal: let a user request **any** field reachable from `MonicaModel` by writing its path in
`sim.json`'s `output.events` section — `"soilMoisture.vm_ActualEvaporation"`,
`"soilColumn.layers.vs_SoilNH4"`, `"soilColumn.layers.vo_AOM_Pool.0.vo_AOM_SlowDecRate_to_SMB_Slow"`
— instead of only the ids that someone pre-registered a lambda for in `build-output.cpp`.

Existing `sim.json` files must keep working **byte-identically**. The legacy names stay valid via a
hardcoded `name -> path` alias table; names whose value is *computed* (a proc call, arithmetic, a
`std::map` lookup) keep their existing lambda.

See `plan-odin.md` §6 phase 7 for what this replaces, and `odin/CONVENTIONS.md` for the rules the
new code has to obey.

---

## STATUS (implemented) — steps 1-7 done, step 8 open

The read side is built, and `bash odin/tests/diff_outputs.sh` passes on both fixtures. What
landed, and where the plan below turned out to be wrong:

| Plan said | Reality |
| --- | --- |
| `build-output.cpp` registers **182** ids | **181**. The 182nd (`Act_ET2`) is entirely inside a `//` comment block, including its `id++`. |
| ~130 become table rows, ~52 stay code | **125** rows, **56** computed. Same split, measured by `odin/tools/gen_output_aliases.py` rather than by hand. |
| `reflect.as_f64` for the leaf | Not usable. Its `Type_Info_Integer` arm is a `switch v in a` over the *sized* integer types with a `case: valid = false` default, so a plain `int` (which `type_info_core` does **not** fold to `i64`) comes back invalid — and `#len` and `cultivationMethodCount` are exactly that. `reflectpath.as_f64` reads the bytes off size/signedness instead, which also skips the `any` type-switch on the daily path. |
| §2.5 tier 1 wins over tier 2 | Held. All 12 fixture-exercised alias rows are byte-identical, and the `use-legacy-output-fns?` A/B confirms it from the other direction. |
| `Step.type` caching is the perf risk | Held, and then some: `resolve` needs no type info at all. `Step.type` is kept for diagnostics only. Wall clock is 0.31s either way — the difference is below measurement noise. |

Also built beyond the plan text:
- `odin/tools/gen_output_aliases.py` — the sweep is a checked-in, re-runnable script, not a
  one-off. Re-run it when `build-output.cpp` changes.
- `installer/Hohenfinow2/sim-min-events2.json` — step 7's second fixture, as a real file rather
  than an edit to `sim-min.json` (which would have invalidated the pre-reflection baseline that
  makes step 3's gate meaningful).
- `reflectpath.map_lookup_scan` — the `iterate_map` oracle, exported so the hashed-vs-scan
  agreement test can live in `odin/tests`.

**Step 8 (the reflection setter fallback, §2.7) is not done.** `set_value_apply` still dispatches
only on `setfs[oid.id]`. Alias-backed oids keep their legacy `id`, so `Stage` and `Mois` still
set correctly; every other field is still unsettable.

---

## 0. Feasibility: verified, not assumed

Odin's `core:reflect` covers every shape the model has, and this tree already proves it:
`odin/monica/trace/trace.odin` walks the whole `Monica_Model` daily through structs, fixed arrays,
slices, dynamic arrays and `Maybe`/unions.

The part `trace.odin` does *not* do — resolving a **path** rather than dumping everything, and
following pointers — was prototyped against the real `core.Monica_Model` before this plan was
written (`/tmp/pathproto/main.odin`, throwaway). Result, with a hand-built 3-layer model:

```
cultivationMethodCount                                         steps=1 open=false leaf=int -> 4
soilMoisture.vm_ActualEvaporation                              steps=2 open=false leaf=f64 -> 1.25
soilMoisture.soilColumn.layers.vs_SoilNO3                      steps=6 open=true  leaf=f64 -> 0.5, 1.5, 2.5
soilColumn.layers.vs_SoilMoisture_m3                           steps=4 open=true  leaf=f64 -> 0.1, 0.15, 0.2
soilColumn.layers.vo_AOM_Pool.0.vo_AOM_SlowDecRate_to_SMB_Slow steps=6 open=true  leaf=f64 -> 7, 8, 9
currentCropModule.vc_LeafAreaIndex                             steps=3 open=false leaf=f64 -> 3.5
currentCropModule.vc_OrganBiomass.1                            steps=4 open=false leaf=f64 -> 555
soilColumn.layers.nope                                         COMPILE FAIL (no field "nope" on Soil_Layer)
simPs.startDate.year                                           COMPILE FAIL (no field "year" on Date)
write test:     before=3.5 after=9.75 (field=9.75)   <- write-through works, so setfs can use this too
nil-deref test: resolved=false                       <- nil currentCropModule is detected, not a crash
```

Every path shape the request named works, including traversal through the
`soilMoisture.soilColumn` **back-pointer** and through nested dynamic arrays. Two things are
confirmed *not* reachable by reflection and therefore define the fallback tier: `Date`'s `year()`
(a proc, not a field) and anything computed.

**Maps were prototyped separately** (`/tmp/mapproto/main.odin`) once they came into scope:

```
enum key from name "tavg" -> tavg ok=true       <- resolved at COMPILE time
climateData.#last resolved=true (len=3)
climateData.#last.tavg  (hashed) = 12 ok=true   <- runtime.__dynamic_map_get, O(1)
climateData.#last.tavg  (scan)   = 12 ok=true   <- reflect.iterate_map, O(n) fallback
missing key 'wind' -> ok=false                  <- degrades to 0.0, matching C++ `ci == cd.end() ? 0.0`
currentEvents.Sowing = true ok=true             <- string-keyed map
currentEvents.Harvest ok=false
```

`reflect.Type_Info_Map.map_info` carries `key_hasher`/`key_equal`, so a genuine hashed lookup is
available — no linear scan on the daily path. A missing key returning not-found is exactly the C++
`cd.find(Climate::tavg) == cd.end() ? 0.0 : round(ci->second, 4)` semantics, for free.

**Key structural insight:** an array segment left *unindexed* (`soilColumn.layers.vs_SoilNH4`) is
exactly the dimension `OId.fromLayer`/`toLayer`/`layerAggOp` already addresses, and that
`get_complex_values` already drives. So the new engine plugs into the *existing* layer-range and
aggregation machinery rather than duplicating it — `["soilColumn.layers.vs_SoilNH4", [1,6,"AVG"], "SUM"]`
works on day one with no new aggregation code.

---

## 1. How much of the legacy table this actually replaces

`src/io/build-output.cpp` registers **182** ids. A mechanical sweep of it (regex over each `build(...)`
body, normalising `monica.soilColumn->layers.at(i).X` -> `soilColumn.layers.X`) classifies them:

| kind | count | example | tier |
| --- | ---: | --- | --- |
| scalar field path, `round(path, D)` | 84 | `TraDef` -> `currentCropModule.vc_TranspirationDeficit`, 2 | alias |
| layer field path via `getComplexValues` | 13 | `NO3` -> `soilColumn.layers.vs_SoilNO3`, 6 | alias |
| reachable with a tightened sweep | ~25 | `IncRoot` -> `vc_OrganGrowthIncrement.0`; `SnowD` -> `soilMoisture.snowComponent.vm_SnowDepth`; `AOM_Fast` -> `soilColumn.layers.vo_AOM_Pool.0.vo_AOM_Fast`; `WaterFlux`, `Silt`, `rootingZone`, `Pwp`, `Fc`, `Sat`, `NFert` | alias |
| climate map lookups | 8 | `Tavg`/`Tmin`/`Tmax`/`Precip`/`Wind`/`Globrad`/`Relhumid`/`Sunhours` -> `climateData.#last.<key>`, 4 | alias |
| **genuinely computed** | ~52 | proc calls (`SOC`/`N`/`Nmin`/`Co`/`OrgN` -> `soilNmin`/`soilOrganicCarbon`/`getOrganicN`), arithmetic (`Pot_ET = ET0*Kc`, `WaterContent`, `SOC-X-Y`, `AWC`, `ETa/ETc`, `Tmax>=40`), `Date` methods (`Date`/`DOY`/`Month`/`Year`), string outputs (`Date`, `Crop`), organ-enum outputs (`OrgBiom`) | lambda |

So roughly **130 of 182 become table rows** and ~52 stay as code. That is the split the request
predicted, measured. All 8 climate ids fall out of the map support (§2.2) — the one place the
`#last` selector earns its keep, since every one of them is `climateData.back()[key]`.

Second, larger consequence: the Odin port currently implements only **21** of the 182 ids
(`build_output_table`, deliberately scoped by `plan-odin.md`). The alias table restores ~110 legacy
output names for the cost of one line each, without writing 110 accessor procs. **Confirmed in
scope:** all of them, with the tier-B honesty caveat in §5.

---

## 2. Design

### 2.1 `odin/support/reflectpath/` — the generic engine (new package, ~350 lines)

No `monica` imports, so it is unit-testable standalone and obeys the `support/` = generic
convention.

```odin
Step_Kind :: enum { FIELD, INDEX, OPEN, DEREF, UNWRAP, LEN, MAP_KEY, LAST, FIRST }
Step :: struct {
    kind:   Step_Kind,
    offset: uintptr,   // FIELD
    index:  int,       // INDEX
    type:   typeid,    // the type *after* this step - resolved at COMPILE time
    key:    []byte,    // MAP_KEY: the key's raw bytes, encoded at COMPILE time
    name:   string,    // diagnostics only
}
Path_Plan :: struct {
    steps:         []Step,
    leaf:          typeid,
    open_step:     int,   // index of the OPEN step, -1 if the path is scalar
    src:           string,
}

compile     :: proc(root: typeid, path: string, allocator := context.allocator) -> (Path_Plan, Compile_Error)
resolve     :: proc(plan: ^Path_Plan, root: any, open_index: int) -> (any, bool)
open_length :: proc(plan: ^Path_Plan, root: any) -> int
```

**The one real performance trap:** the prototype's `FIELD` step looked its field type up by name
(`reflect.struct_field_by_name`) on *every* resolve — a linear scan over the struct's fields, run
per output value per day. `Step.type` must be filled in at compile time so `resolve` is nothing but
`ptr += offset`. This is the single most important detail in the engine; get it wrong and a daily
section with 20 layer outputs over 2,500 days does ~10^6 string comparisons for nothing.

`Step.key` is the same idea applied to maps: an enum key like `tavg` is resolved to its value via
`reflect.enum_from_name_any` **once, at compile time**, and stored as bytes. `resolve` then calls
`info.map_info.key_hasher` + `runtime.__dynamic_map_get` — no string comparison, no
`reflect.iterate_map` scan. (`iterate_map` stays as the fallback for key types with no usable
hasher, and as the oracle in the unit tests; the prototype checked both agree.)

Cost after those two fixes: ~4-6 pointer adds per value, plus one hash for a map segment, versus a
direct call for a lambda. Not measurably different, but §5 pins it with a timing check anyway.

### 2.2 Path syntax (v1)

| form | meaning |
| --- | --- |
| `a.b.c` | struct field traversal |
| `layers.3.field` / `layers[3].field` | fixed index into array / slice / dynamic array (**both** accepted, see below) |
| `layers.field` | array left unindexed = the **open dimension**, driven by `oid.fromLayer..toLayer` (or `oid.organ`) and collapsed by `oid.layerAggOp` — i.e. handed to `get_complex_values` |
| pointer segment | auto-dereferenced; `nil` makes the value *missing* |
| `Maybe(T)` segment | auto-unwrapped; unset makes the value *missing* |
| `...field.#len` | terminal pseudo-segment yielding the length of an array/slice/dynamic array (covers legacy `noOfAOMPools`) |
| `climateData.#last` / `.#first` | selects the last/first element of an array/slice/dynamic array (covers `climateData.back()`) |
| `climateData.#last.tavg` | map segment: the next segment is the **key**. Enum keys resolved by name at compile time (`ACD.tavg`), string keys taken literally (`currentEvents.Sowing`), integer keys parsed. |

Rules:
- **At most one open dimension** per path. A second one is a compile error, not a nested array in
  the CSV — `write_output` only flattens one level.
- A *missing* value (nil pointer, unset `Maybe`, out-of-range index, **absent map key**) yields `0.0`, matching what
  every legacy lambda's `monica.currentCropModule.get() ? ... : 0.0` ternary does. Out-of-range is
  where this is *safer* than the C++, which calls `layers.at(i)` and throws.
- **Maps are in v1** (decided). The model has two: `climateData` (`[dynamic]map[ACD]f64`) and the
  `currentEvents`/`previousDaysEvents` `map[string]bool` sets. Both are reachable with `#last` + a
  key segment, and doing so moves 8 climate ids from the lambda tier to the alias tier. A map may
  **not** be the open dimension in v1 — Odin map iteration order is unstable, so "all keys" would
  emit CSV columns in a nondeterministic order (the same reason `trace.odin` refuses to walk maps).
  Only keyed access is allowed; ambiguity is a compile error, not a surprise.

Accepting `[3]` as well as `.3` is deliberate: it makes **`trace.odin`'s output paths valid output
specs**. Diff the trace, spot the field that diverged, paste that path straight into `sim.json`.
That is worth the ten lines of parser it costs.

### 2.3 `OId` extension (`odin/monica/io/output.odin`)

```odin
OId :: struct {
    id:            int,             // unchanged: still the lambda-table key
    ...
    path:          string,          // "" for lambda-backed oids
    plan:          ^rp.Path_Plan,   // nil for lambda-backed oids
    roundToDigits: int,             // -1 = do not round
    castTo:        enum { NONE, INT },
}
```

`OId` is copied by value all over the port (`oid_in := oid`), so `plan` must be a pointer into a
setup-time allocation, never an owned value. `oid_to_json` gains `path` — additive, so the ZMQ wire
stays backward compatible.

`castTo` exists because a handful of legacy entries truncate (`RootDep` is
`int(vc_RootingDepth)`); without it those CSV columns would gain decimals.

### 2.4 Alias table (`odin/monica/io/output_paths.odin`, new)

```odin
Alias :: struct { path: string, round: int, cast_to: Cast }
g_legacy_aliases := map[string]Alias{
    "Mois"    = {"soilColumn.layers.vs_SoilMoisture_m3", 3, .NONE},
    "TraDef"  = {"currentCropModule.vc_TranspirationDeficit", 2, .NONE},
    "RootDep" = {"currentCropModule.vc_RootingDepth", -1, .INT},
    "Tavg"    = {"climateData.#last.tavg", 4, .NONE},
    // ~130 rows, generated from build-output.cpp then hand-audited
}
```

`round`/`cast_to` are what make byte-identity possible: the CSV writer prints `%.6g`, so
`round(x, 3)` vs raw `x` is a visible difference.

### 2.5 Name resolution in `parse_output_ids` — three tiers, resolved at **setup**, not per day

1. name has an alias -> `compile` the path. On success, reflection-backed oid.
2. name is in `name2metadata` -> lambda-backed oid (today's behaviour).
3. name is not a known id but `compile`s as a path against `Monica_Model` -> reflection-backed oid.
4. otherwise -> skip, **with a warning** (today it is a silent skip, which is how a typo currently
   costs you a column and no message).

**Confirmed: tier 1 wins over tier 2.** Tier 1 before tier 2 means the 21 already-ported ids run through the *new* engine on the existing
fixture — which is what turns §5's CSV diff into a real test of the engine instead of a test of
nothing. A `"use-legacy-output-fns?": true` escape hatch in `sim.json`'s `output` section forces
tier 2 first, for A/B bisection when a column does move.

Failures are reported once at setup with the offending spec, not once per day for 2,500 days.

### 2.6 Object form, for metadata a raw path cannot carry

A raw path has no unit and no sensible display name, and the positional slots in the existing array
form (`[name, layers, timeAgg]`) are taken. So `parse_output_ids` also accepts an object — today it
ignores anything that is not a string or an array, so this is purely additive:

```json
{ "path": "soilMoisture.vm_ActualEvaporation", "name": "ActEvap", "unit": "mm", "round": 3 }
{ "path": "soilColumn.layers.vs_SoilNH4", "unit": "kgN m-3", "round": 6,
  "layers": [1, 6, "AVG"], "agg": "SUM" }
```

`layers`/`agg` map onto `fromLayer`/`toLayer`/`layerAggOp`/`timeAggOp` with the same parsing the
array form uses.

**Confirmed: a raw path is not rounded unless it says so.** `roundToDigits` defaults to `-1` (emit
the full `%.6g`); `"round": N` opts in. Legacy aliases carry their own digits, so compatibility is
unaffected either way. No global rounding knob.

### 2.7 Setters come along for free

`resolve` returns an `any` pointing at the real field, and the prototype confirmed writing through
it works. So `set_value_apply` (`run/worksteps.odin:1244`) can fall back to reflection for any
field, instead of only the two ids (`Stage`, `Mois`) with a registered `setf`. **Read side lands
first**; the setter side is a follow-up in the same shape, gated on the SetValue workstep's own
tests.

---

## 3. Files touched

| file | change |
| --- | --- |
| `odin/support/reflectpath/reflectpath.odin` | **new** — compile/resolve/open_length |
| `odin/monica/io/output_paths.odin` | **new** — alias table, `oid_bind`, `resolve_oid_value` |
| `odin/monica/io/output.odin` | `OId` gains `path`/`plan`/`roundToDigits`/`castTo`; `oid_to_json` gains `path` |
| `odin/monica/io/build_output.odin` | `parse_output_ids` -> three-tier; object form; warn on unknown |
| `odin/monica/run/run_monica.odin` | `store_results`: `oid.plan != nil` ? reflection : `ofs[oid.id]` |
| `odin/monica/run/worksteps.odin` | (follow-up) reflection setter fallback |
| `odin/tests/reflectpath_test.odin` | **new** — engine unit tests |
| `odin/tests/output_paths_test.odin` | **new** — every alias compiles (see §5) |
| `odin/CONVENTIONS.md`, `plan-odin.md` | cross-reference this plan |

---

## 4. Execution order

1. **Baseline snapshot** (§5) — before touching anything.
2. `support/reflectpath` + its unit tests. Nothing else can be written until the `Step.type`
   caching and the open-dimension contract are fixed in code.
3. `OId` extension + `resolve_oid_value` bridging to `get_complex_values`; `store_results`
   dispatch. **Alias table seeded with only `Mois` and `Kc`** — two rows, one layer-open and one
   scalar. Diff the CSVs. This is the go/no-go gate: if two rows are byte-identical, the engine is
   right; if not, nothing downstream is worth writing.
4. Tier-3 raw-path support + the object form + the unknown-name warning.
5. Map segments (`#last`/`#first` + keyed access), gated on step 3 passing. Seed with `Tavg` and
   `Precip` — both are in `sim-min.json`'s `events`, so the CSV diff verifies them immediately.
6. Generate the full alias table from `build-output.cpp`; hand-audit; the compile-every-alias test.
7. Enable `sim-min.json`'s `_events` section (`Pwp`, `Fc`, `Act_ET`, `NFert` are all alias-tier and
   currently unported) as a second, independent fixture.
8. Setter fallback (§2.7).

---

## 5. Verification

**Oracle: the Odin binary against itself.** The C++ differential harness
(`odin/tests/cpp_ref/run_monica_run.sh`) needs MSVC and a CMake build, so it is Windows-only; but
this change must not move a single output value, which makes a before/after self-diff both
sufficient and Linux-runnable. Verified working here:

```sh
mkdir -p /tmp/out-base && cd /tmp/out-base
MONICA_PARAMETERS=$HOME/GitHub/monica-parameters \
  <repo>/odin/build/monica-run -m <repo>/installer/Hohenfinow2/sim-min.json
# -> sim-min-out_section_{daily,crop,yearly,run,OrganicFertilization}.csv
```

`sim-min.json`'s `events` exercises 21 ids, of which **12 move to the reflection path** in this
change: `CM-count`, `Kc`, `Irrig`, `AbBiom`, `LAI`, `Mois`, `RunOff`, `NLeach`, `Recharge` (field
paths) plus `Tavg`, `Precip`, `Globrad` (map paths). Between them they cover a layer range
(`["Mois",[1,3]]`), a layer aggregation (`["Mois",[1,3,"AVG"]]`), time aggregation over both
(`yearly`, `run`), and all three map keys. The 9 that stay on lambdas (`Date`, `Crop`, `Stage`,
`ETa/ETc`, `OrgBiom`, `Yield`, `SOC`, `N`, `Year`) are the computed tier, so the diff also proves
the fallback still works. Every one of those five CSVs must be byte-identical. Wrap it as
`odin/tests/diff_outputs.sh`.

**Every alias compiles.** An `@(test)` proc that walks `g_legacy_aliases` and asserts
`rp.compile(typeid_of(core.Monica_Model), path)` succeeds and the leaf is numeric. This is the part
that makes the ~100 restored names trustworthy without a C++ build: a typo or a field the port
never ported fails the suite instead of silently emitting `0.0` forever.

**Engine unit tests**: nil pointer, unset `Maybe`, out-of-range fixed index, out-of-range open
index, two open dimensions rejected, `#len`, `#last`/`#first` on an empty array, `[i]` and `.i`
equivalence, unknown field, empty path. For maps: enum key by name, unknown enum key rejected at
compile time, string key, absent key at runtime, `#last` on an empty `climateData`, map-as-open-
dimension rejected, and **hashed lookup agreeing with `reflect.iterate_map`** on every key (the
prototype's cross-check, promoted to a test).

**Timing**: `time` the daily section before/after. A regression beyond a few percent means the
`Step.type` caching (§2.1) was missed.

**Tier-B honesty**: aliases not exercised by any fixture are verified only as "path compiles, leaf
is numeric", *not* as "value matches the C++". Mark them as such in the table's header comment
rather than implying 182 verified outputs.

---

## 6. Who executes what

Same split rationale as `plan-odin.md` §9: the design content is concentrated in a few hundred
lines, the volume is in the table.

**Needs Opus (design-setting; every later line copies these decisions):**

| Item | Why |
| --- | --- |
| §2.1 `Path_Plan` compile/resolve, incl. `Step.type` **and** `Step.key` compile-time caching | The one place a wrong decision is a rewrite rather than a fix. Also the one place a plausible implementation is quietly 100x too slow (per-resolve field-name scan, or per-resolve `iterate_map`). |
| §2.2 map segments: hashed lookup via `map_info.key_hasher` + `__dynamic_map_get`, and the "map may not be the open dimension" rule | Reaches into `base:runtime` internals, and the iteration-order rule is what keeps CSV columns deterministic. |
| §2.2 the open-dimension <-> `OId.fromLayer/toLayer/layerAggOp/organ` contract | Decides whether the change reuses `get_complex_values` or accidentally forks the aggregation semantics. |
| §2.3-2.5 `OId` extension + tier ordering + the missing-value/rounding/cast rules | These are exactly what determines byte-identity, and they touch the ZMQ wire and the SetValue workstep. |
| Step 3's two-row go/no-go gate | Hypothesis formation if the CSVs move. |
| Any CSV divergence investigation | `plan-odin.md` §9 already reserves this. |

**Good for Sonnet 5 (high volume, one-shot verification):**

| Item | Volume |
| --- | ---: |
| Generator script over `build-output.cpp` + hand-audit of the ~130-row alias table | the bulk; verified in one shot by the compile-every-alias test |
| §2.6 object-form parsing, once the key set is fixed | ~80 lines |
| The 8 `climateData.#last.<key>` alias rows, once the map engine exists | trivial |
| `#len`, `[i]`-vs-`.i` parsing | ~30 lines |
| The whole test suite (§5) incl. `diff_outputs.sh` | ~300 lines |
| `CONVENTIONS.md` / `plan-odin.md` cross-references | docs |

**Hard sequencing constraint:** the alias table (Sonnet's bulk) must not start before step 3's
go/no-go gate passes. 130 rows written against an engine whose rounding or open-dimension semantics
later change is 130 rows to re-audit.

---

## 7. Explicitly out of scope

| Item | Why / when |
| --- | --- |
| A map as the **open dimension** ("all keys of `climateData.#last`") | Odin map iteration order is unstable -> nondeterministic CSV column order. Keyed access only. |
| Computed expressions in paths (`ET0 * Kc`, `soilNmin(layer)`) | the request defers this; §2.5 tier 2 is the mechanism for adding them under new names, exactly as the C++ allowed |
| More than one open dimension | `write_output` flattens one level only |
| Removing `OId.id` | still the lambda-table key and still on the ZMQ wire |
| Reflection-backed `setfs` | §2.7, follow-up |

## 8. Risk register

| Risk | Mitigation |
| --- | --- |
| A moved CSV column (rounding, `int` truncation, nil-default) | step 3's two-row gate; `roundToDigits`/`castTo` on the alias; `use-legacy-output-fns?` for A/B bisection |
| `reflect.struct_field_by_name` left on the daily path | `Step.type` filled at compile time; timing check in §5 |
| `^Path_Plan` dangling because `OId` is copied by value | plans allocated once at setup in the run's allocator, never stack-owned |
| Alias pointing at a field the port never ported -> silent `0.0` | compile-every-alias `@(test)` |
| Out-of-range layer index panicking instead of the C++ `at()` throw | resolve returns *missing* -> `0.0`; documented as an intentional divergence |
| A typo in a user path silently dropping a column | tier 4 warns instead of skipping silently |
| `reflect.iterate_map` left on the daily path for map segments | `Step.key` + `key_hasher`/`__dynamic_map_get`; the scan stays only as a test oracle and a no-hasher fallback |
| `base:runtime` map internals shifting under a compiler bump | the hashed-vs-scan agreement test fails loudly; `odinw.py` pins the compiler by tag + SHA256 already |
| `climateData` empty on day 0 -> `#last` on a zero-length array | resolves as *missing* -> `0.0`; covered by a unit test |
