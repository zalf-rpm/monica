# Plan: `monica-parameters.h` proceduralization

Companion plan to `plan.md`, scoped to just `src/core/monica-parameters.h` /
`monica-parameters.cpp`. Kept as a separate file because this is a big, multi-session piece of
work: convert one struct, build, run the regression check, commit, push, tick it off below, then
**stop and ask the user before starting the next one**. That way work can be resumed cleanly
across sessions/usage-limit resets.

## Goals

Same overall philosophy as `plan.md`, applied to the 24 `Tools::Json11Serializable`-derived
structs in `monica-parameters.h`:

1. Drop the `: public Tools::Json11Serializable` inheritance; the struct becomes a plain
   aggregate (data members only, default member initializers instead of constructor bodies where
   possible).
2. Replace every non-default constructor (reader-based, field-based, json-based) with a
   `makeXxx(...)` free function, **declared directly in `monica`** (leaner pattern from `plan.md`
   goal #6 — no nesting, no alias).
3. Convert the former `virtual`/`override` interface methods (`merge`, `to_json`) and the plain
   methods (`deserialize`, `serialize`, plus any non-trivial member method like
   `pc_NumberOfDevelopmentalStages()`, `getGroundwaterInformation(...)`) into free functions taking
   `Xxx*` / `const Xxx*` as first argument.
4. Move *only those free procedures* (not the struct, not `makeXxx`) into a lower-case namespace
   named after the struct (e.g. `SoilOrganicModuleParameters` -> `monica::soilorganicmoduleparameters`),
   matching the convention already used for `soilorganic`, `cropmodule`, `soilcolumn`, etc.
5. Where a struct's `merge()` currently starts with `Json11Serializable::merge(j)` (the
   "DEFAULT"/"=" unwrap), replace that with the free-function equivalent already scaffolded for
   this in `mas_cpp_misc/json11/json11-helper.h`: `Tools::defaultMerge(j, [x](json11::Json j2){
   return merge(x, j2); })`. (`Tools::defaultToJson()` / `Tools::defaultToString(...)` exist too
   but none of these 24 structs currently need them — none override `toString()`.)
6. Struct-level (not interface-level) inheritance is fine to keep as plain-struct inheritance — it
   already works for `SoilColumn : public std::vector<SoilLayer>`. `AutomaticIrrigationParameters
   : public IrrigationParameters`, `OrganicFertilizerParameters : public OrganicMatterParameters`,
   and `CropResidueParameters : public OrganicMatterParameters` keep their base class; their free
   `merge`/`serialize`/`deserialize`/`to_json` call into the base's free functions via the implicit
   upcast (mirrors today's `Base::merge(j)` calls, just non-virtual now).
7. Trivial one-line accessors get inlined at call sites and removed, per the usual rule; only
   genuinely non-trivial methods get a free-function equivalent.
8. `DLL_API` currently expands to nothing unless `USE_DLL` is defined (see
   `mas_cpp_misc/common/dll-exports.h`) — true in this build (static lib) — so it's low-risk either
   way, but keep it on whichever declarations are the new public API surface (the free function
   declarations in the header) for correctness in a hypothetical DLL build, rather than dropping it.
9. A handful of these structs are read via `.toString()` from *outside* `monica-parameters.h`
   (`MineralFertilizerParameters` in `soilcolumn.cpp`, `OrganicMatterParameters` in
   `soilorganic.cpp`, `CropParameters`/`CropResidueParameters` in `crop.cpp`). Since none of them
   need a real `toString` override, just inline those call sites to
   `namespacename::to_json(&x).dump()` instead of adding a `toString` free function nobody else needs.
10. **(discovered during item 5, applies to every remaining item)** `json11::Json` has an implicit
    `template<class T, class = decltype(&T::to_json)> Json(const T& t) : Json(t.to_json()) {}`
    constructor (`mas_cpp_misc/json11/json11.hpp`), gated by SFINAE on `T::to_json` existing as a
    *member*. Once a struct's `to_json` becomes a free function, any place that dropped a bare
    struct instance into a `json11::Json`/`J11Object`/`Json::object` context relying on that
    implicit conversion (e.g. `{"key", someStructInstance}`) stops compiling — fails loudly (no
    viable conversion), so the build catches it, but it can be in a file far from the one just
    converted. Same story for the generic `set_value_obj_value(var, j, key)` helper (same header),
    which calls `var.merge(j[key])` as a member call — grep for `set_value_obj_value(<name>` too,
    not just the four `.merge/.to_json/.serialize/.deserialize(` member-call patterns already
    covered by goal-derived checks. Both `daily-monica-fbp-component-main.cpp` and
    `cultivation-method.cpp`'s `MineralFertilization`/`NDemandFertilization` worksteps hit this for
    `MineralFertilizerParameters` and only turned up on the *second* build attempt because the
    first grep pass covered `.merge(`/`.to_json(` etc. but not bare-instance-in-JSON-literal or
    `set_value_obj_value`.

## Not in scope

- `Tools::Json11Serializable` itself (`mas_cpp_misc/json11/json11-helper.h`) — shared base used by
  other unrelated types (`Workstep`, `CultivationMethod`, `Output`, `Soil::SoilParameters`,
  `Crop`, `DataAccessor`, `CSVViaHeaderOptions`); not touched by this plan.
- `Soil::SoilParameters` / `Soil::SoilPMs` (`mas_cpp_misc/soil/soil.h`) — used as an opaque value
  member of `SiteParameters`; lives in the shared `mas_cpp_misc` library, out of scope here.
- `struct Intercropping` at the bottom of the file — already a plain struct with no
  constructor/merge/serialize/deserialize to convert; nothing to do.
- Confirmed (via grep across `src/` and `mas_cpp_misc/`) that none of these 24 structs are ever
  used polymorphically through a `Json11Serializable*`/`&` — dropping the inheritance is safe.

## Conversion order (dependency-ordered)

Composite structs (that hold another struct from this file as a value member) must be converted
*after* the structs they contain, since their `deserialize`/`merge`/`to_json` construct/traverse
those members. Check off each once it's built, regression-tested, committed, and pushed.

1. [x] `YieldComponent` — leaf; used by `CultivarParameters`. Note: `CultivarParameters`'s
   `deserialize`/`serialize`/`merge`/`to_json` used the generic `setComplexCapnpList` /
   `setFromComplexCapnpList` / `toVector<T>` / `toJsonArray` templates on its
   `std::vector<YieldComponent> pc_OrganIdsForXxx` members — those templates call `.merge()` /
   `.to_json()` / `.serialize()` / `.deserialize()` as *member* functions, so they no longer compile
   for a plain-struct `YieldComponent`. Replaced with small local lambdas calling the new
   `yieldcomponent::...` free functions, in both `CultivarParameters` (still otherwise
   member/OOP-based, unconverted) and `CropModule` (`crop-module.cpp`, which keeps its own copies
   of the same three vectors). This same pattern (generic template call sites touching a
   just-converted struct's vector members) will recur for any other struct held in a
   `std::vector<T>` processed via these templates — check for it before converting.
2. [x] `SpeciesParameters` — leaf; used by `CropParameters`. Dropped the dead
   `SpeciesParameters(json11::Json j)` constructor declaration (no definition anywhere). Same
   leak-forward issue as `YieldComponent`: `CultivarParameters::deserialize`/`merge` (still
   unconverted) call `cps->speciesParams.pc_NumberOfDevelopmentalStages()`/`.pc_NumberOfOrgans()`
   (now free `speciesparameters::numberOfDevelopmentalStages`/`numberOfOrgans`), and
   `CropParameters::deserialize`/`serialize`/`merge`/`to_json` (also unconverted) call
   `speciesParams.deserialize/serialize/merge/to_json(...)` as member functions — rewired those
   call sites to the new free functions without otherwise converting `CultivarParameters`/
   `CropParameters` yet. Also touched two `crop-module.cpp` sites doing the same
   `speciesParams.pc_NumberOfDevelopmentalStages()`/`.pc_NumberOfOrgans()` calls directly.
3. [x] `CultivarParameters` — needs `YieldComponent` done (holds
   `std::vector<YieldComponent>` members). `pc_NumberOfDevelopmentalStages()` was a trivial
   one-liner (`return pc_BaseDaylength.size();`) with zero external callers, but kept it as an
   `inline` free function `cultivarparameters::numberOfDevelopmentalStages` for interface
   completeness (matches `soilcolumn::numberOfLayers` precedent: computed-but-trivial methods stay
   as free functions rather than being deleted or inlined at call sites). Same leak-forward pattern
   as before: `CropParameters::deserialize`/`serialize`/`merge`/`to_json` (still unconverted) called
   `cultivarParams.deserialize/serialize/merge/to_json(...)` as member functions — rewired to
   `cultivarparameters::...` free functions without otherwise converting `CropParameters` yet.
4. [x] `CropParameters` — needs `SpeciesParameters` + `CultivarParameters` done (holds both by
   value). Has two `merge` overloads (`merge(j)` and `merge(sj, cj)`) — kept as two
   `cropparameters::merge` overloads. `pc_CropName()` (real computation, string concat) became
   `inline cropparameters::cropName(...)`. Fixed `.toString()` call site in `crop.cpp` (replaced
   with `cropparameters::to_json(...).dump()` since no other caller needs a `toString` override).
   This struct had noticeably more external call sites than the previous three combined — besides
   the usual leak-forward into still-unconverted `Crop`/`Sowing`/`Cutting`/`Harvest`
   (`crop.cpp`, `cultivation-method.cpp`), two direct-construction call sites
   (`CropParameters cps(reader.getCropParams());` in `monica-model.cpp`,
   `kj::heap<CropParameters>(reader.getPerennialCropParams())` in `crop.cpp`) weren't caught by the
   initial `CropParameters(` grep because the type name wasn't immediately followed by `(` — worth
   remembering for future items: also grep for `Type varname(` and `kj::heap<Type>(readerOrJson)`
   patterns, not just `Type(`.
5. [x] `MineralFertilizerParameters` — leaf; already had its trivial accessors inlined in an
   earlier pass (see `plan.md`). Fixed `.toString()` call site in `soilcolumn.cpp` (now
   `mineralfertilizerparameters::to_json(&fp).dump()`). This is where the implicit-`json11::Json`-
   conversion and `set_value_obj_value` gotchas (goal #10 above) were discovered: leak-forward
   sites turned up in `SoilColumn::DelayedNMinApplicationParams`/`SoilColumn` (`.deserialize`/
   `.serialize` on the `fp`/`_vf_TopDressingPartition` members), `SimulationParameters::to_json`
   (`p_NMinFertiliserPartition` bare in a `J11Object`), and — only surfacing on rebuild —
   `cultivation-method.cpp`'s `MineralFertilization`/`NDemandFertilization::merge`/`to_json`
   (`set_value_obj_value(_partition, ...)` and bare `_partition` in JSON literals) and
   `daily-monica-fbp-component-main.cpp` (`applyMineralFertiliser(monica.get(), mf.getPartition(),
   ...)` relying on the now-removed implicit reader-constructor).
6. [x] `NMinApplicationParameters` — leaf. Straightforward: only one external usage
   (`monica-model.cpp`, a plain const-ref field-access alias, no method calls), plus the usual
   leak-forward into `SimulationParameters::deserialize`/`serialize`/`merge`/`to_json`
   (`p_NMinUserParams`, including the bare-in-`J11Object` case from goal #10). Note:
   `SoilColumn::DelayedNMinApplicationParams` (`soilcolumn.h`) is an unrelated, differently-named
   type despite the similar name — not touched by this item.
7. [x] `IrrigationParameters` — leaf; base of `AutomaticIrrigationParameters` (still unconverted,
   item 8). Since `AutomaticIrrigationParameters : public IrrigationParameters` and previously
   inherited `Json11Serializable` only transitively (through `IrrigationParameters`), converting
   the base meant `AutomaticIrrigationParameters` lost that base too — its own `virtual merge`/
   `to_json` are now non-overriding (harmless, no polymorphic use anywhere) but its bodies had to
   be rewired: `IrrigationParameters::deserialize/serialize/merge/to_json(...)` (explicit
   base-class-qualified calls, not instance calls, so a different grep shape than usual) became
   `irrigationparameters::...(this, ...)`, `Json11Serializable::merge(j)` became
   `defaultMerge(j, [this](json11::Json j2){ return merge(j2); })`, and the base-class constructor
   initializer `: IrrigationParameters(nc, sc)` became `: IrrigationParameters{nc, sc}` (brace
   aggregate-init instead of a removed 2-arg constructor call). Also fixed `Irrigation::merge`/
   `to_json` in `cultivation-method.cpp` (`set_value_obj_value(_params, ...)` and bare `_params` in
   a JSON literal, goal #10) — `AutomaticIrrigation::params` in the same file is
   `AutomaticIrrigationParameters` (item 8's type), left untouched.
8. [x] `AutomaticIrrigationParameters` — needed `IrrigationParameters` done (inherits it, kept as
   plain-struct inheritance). Mostly mechanical since item 7 had already rewired its internal calls
   to the base class's free functions; this step converted the shell (constructors ->
   `makeAutomaticIrrigationParameters(...)`, `this->` becomes `aip->`). Leak-forward fixed in
   `SimulationParameters` (`p_AutoIrrigationParams`, all four usual spots) and
   `AutomaticIrrigation` in `cultivation-method.cpp` (`set_value_obj_value(params, ...)` and
   `params.to_json()`, goal #10).
9. [x] `MeasuredGroundwaterTableInformation` — leaf; `getGroundwaterInformation(Tools::Date)` became
   `measuredgroundwatertableinformation::getGroundwaterInformation(gwi, date)`. `merge()` doesn't
   call the base `Json11Serializable::merge(j)` (like `YieldComponent`), so no `defaultMerge`
   wrapper needed. The dead, commented-out `readInGroundwaterInformation(std::string path)` member
   (declaration already commented in the header, body still in a `/* ... */` block in the .cpp) was
   left untouched. Leak-forward fixed in `MonicaModel` (`monica-model.cpp`:
   `deserialize`/`serialize`/the `getGroundwaterInformation` call) and
   `CentralParameterProvider::merge` (item 24, still unconverted).
10. [x] `SiteParameters` — leaf (holds `Soil::SoilPMs` / `Soil::SoilParameters` opaquely,
    unconverted, that's fine). `calculateAndSetPwpFcSatFunctions` is populated externally via
    `operator[]` assignment in the various run-main files, unaffected either way. Leak-forward fixed
    in `MonicaModel` (`monica-model.cpp`), `run-monica-capnp.cpp`, and
    `CentralParameterProvider::merge`/`to_json` (item 24, still unconverted). Oddity noted but not
    chased: `serve-monica-zmq.cpp:245` has `SiteParameters site(initMsg["site"]);` — direct-init
    from a `json11::Json` with a single paren-arg — which compiled *before* this conversion too
    (verified by isolating just that one object file), so whatever it actually does (likely MSVC's
    permissive aggregate-paren-init, since the project builds with `/std:c++17` where that's
    nonstandard) is unchanged behavior, not something this conversion introduced or fixed; left
    alone since it's outside anything the `monica-run` regression check exercises.
11. [x] `AutomaticHarvestParameters` — leaf; nested `enum HarvestTime` stays inside the struct
    (unaffected by the struct/namespace split — `AutomaticHarvestParameters::HarvestTime` and
    `AutomaticHarvestParameters::maturity`/`::unknown` remain valid). The `HarvestTime yt`
    constructor had no live callers anywhere but was still ported to
    `makeAutomaticHarvestParameters(HarvestTime)` for interface completeness. Leak-forward fixed in
    `Crop` (`crop.cpp`: `deserialize`/`serialize`/bare-in-`J11Object` `to_json`, goal #10); `Crop`
    doesn't actually call `.merge()` on this member in `Crop::merge` (pre-existing, unrelated to
    this conversion).
12. [x] `NMinCropParameters` — leaf; not held by any other struct in this file and no
    `.merge`/`.to_json`/`.serialize`/`.deserialize` call sites anywhere. Only fix needed was three
    field-based constructor call sites (`NMinCropParameters(a, b, c)` -> `makeNMinCropParameters(a,
    b, c)`) in `monica-model.cpp` (x2) and `cultivation-method.cpp`.
13. [x] `OrganicMatterParameters` — leaf; base of `OrganicFertilizerParameters` and
    `CropResidueParameters` (both still unconverted, items 14/15). Same transitive-base-loss pattern
    as `IrrigationParameters`/`AutomaticIrrigationParameters` (items 7/8): both derived structs'
    `deserialize`/`serialize`/`merge`/`to_json` had `OrganicMatterParameters::method(...)`
    base-class-qualified calls rewired to `organicmatterparameters::method(this, ...)`, and their
    `Json11Serializable::merge(j)` calls became `defaultMerge(j, [this](json11::Json j2){ return
    merge(j2); })`. New wrinkle this time: `Crop::toString()` in `crop.cpp` calls
    `residueParameters().toString()` — since `Json11Serializable::toString()`'s *default*
    implementation (`return to_json().dump();`) was inherited transitively through
    `CropResidueParameters -> OrganicMatterParameters -> Json11Serializable`, losing the base broke
    that too, even though `CropResidueParameters::to_json()` itself is untouched (still a member,
    item 15's job). Fixed by calling `.to_json().dump()` directly instead of the now-gone
    `.toString()`. Also fixed the known `soilorganic.cpp` `.toString()` site (goal #9) and
    leak-forward in `OrganicFertilization` (`cultivation-method.cpp`) plus a reader-based
    direct-construction site in `daily-monica-fbp-component-main.cpp`.
14. [x] `OrganicFertilizerParameters` — needed `OrganicMatterParameters` done (inherits it, kept as
    plain-struct inheritance). Not referenced anywhere outside `monica-parameters.h`/`.cpp` — item
    13 had already rewired its internal base-class calls, so this step was purely mechanical
    (constructors -> `makeOrganicFertilizerParameters(reader)`, `this->` becomes `ofp->`); no
    external call sites to fix.
15. [x] `CropResidueParameters` — needed `OrganicMatterParameters` done (inherits it, kept as
    plain-struct inheritance). Item 13 already rewired its internal base-class calls, so this step
    converted the shell. Unlike `OrganicFertilizerParameters`, this one has real external usage:
    fixed leak-forward in `Crop`/`Sowing` (`crop.cpp`, `cultivation-method.cpp` — including the
    `residueParameters().toString()`/`.to_json()` sites from item 13's discovery) and
    `CropModule::residuePs` (`crop-module.cpp`); also a direct-init constructor call
    (`CropResidueParameters rps(reader.getResidueParams());`) in `monica-model.cpp` caught by the
    "also grep `Type varname(`" lesson from item 4.
16. [x] `SimulationParameters` — needed `AutomaticIrrigationParameters`,
    `MineralFertilizerParameters`, `NMinApplicationParameters` done (holds all three by value; those
    three sub-members' calls were already rewired during items 8/11/12's leak-forward fixes, so this
    step only had to convert the shell itself). Two field names collided with their own JSON key
    strings (`{"startDate", startDate...}`, `{"serializeMonicaStateAtEnd", serializeMonicaStateAtEnd}`)
    so the blind `pc_`/`p_`-prefix sed pass (safe, no collisions) was followed by careful manual
    `sp->` edits for the handful of non-prefixed fields (`startDate`, `endDate`, `dualKcMethod`,
    `serializeMonicaStateAtEnd`/`...ToJson`, `pathToSerializationAtEndFile`,
    `loadSerializedMonicaStateAtStart`, `deserializedMonicaStateFromJson`,
    `pathToLoadSerializationFile`, `noOfPreviousDaysSerializedClimateData`) rather than risk
    corrupting a JSON string literal. Leak-forward fixed in `MonicaModel::simPs`
    (`monica-model.cpp`) and `CentralParameterProvider::simulationParameters` (item 24, still
    unconverted).
17. [x] `CropModuleParameters` — leaf. Straightforward; only external usage was `MonicaModel::cropPs`
    (`monica-model.cpp`: `deserialize`/`serialize` member calls, plain field member so no aggregate-init
    concerns) and the still-unconverted `CentralParameterProvider::userCropParameters` leak-forward
    (item 24, both `merge`/`to_json`). Also fixed a `.to_json()` call site in
    `create-env-from-json-config.cpp` (`readUserCropParametersFromDatabase(...).to_json()` ->
    `cropmoduleparameters::to_json(&...)`- style temporary handling not needed there since that
    function itself is unrelated/pre-existing and wasn't touched; only the member-call shape mattered).
18. [x] `EnvironmentParameters` — leaf. Same shape as item 17: external usage was `MonicaModel::envPs`
    (`monica-model.cpp` `deserialize`/`serialize`) plus the `CentralParameterProvider` leak-forward
    (item 24). `SoilMoisture::envPs` (`soilmoisture.h`) holds a `const EnvironmentParameters&` but only
    does field reads, unaffected.
19. [x] `SoilMoistureModuleParameters` — leaf. Folded the non-inline default constructor's
    `getCapillaryRiseRate` lambda into a default member initializer as planned, so the struct needs no
    custom default construction. External usage: `SoilMoisture::params` (`soilmoisture.h`/`.cpp`
    `deserialize`/`serialize` member calls) and the `CentralParameterProvider` leak-forward (item 24).
    `makeSoilMoisture(...)`'s two overloads in `soilmoisture.cpp` aggregate-brace-init a `SoilMoisture`
    with a `SoilMoistureModuleParameters` value member (by name `params` or via `{}`
    default-construction) — both continue to work unchanged as plain-aggregate brace-init, no fix
    needed. **Found via full build, not grep**: `CentralParameterProvider::merge`/`to_json`
    (`monica-parameters.cpp`, item 24, still unconverted) call `.merge(`/`.to_json(` directly on all
    three of this batch's structs' `userXxxParameters` members — this leak-forward shape (bare
    `memberOfThisFile.method(...)` inside another struct in the *same* file, not an external file) isn't
    caught by a repo-wide grep restricted to "outside this file", so for future items also double-check
    `CentralParameterProvider`'s own `merge`/`to_json` bodies directly after converting any struct it
    holds by value, even before item 24 itself is converted.
20. [ ] `SoilTemperatureModuleParameters` — leaf.
21. [ ] `SoilTransportModuleParameters` — leaf.
22. [ ] `SticsParameters` — leaf; used by `SoilOrganicModuleParameters`.
23. [ ] `SoilOrganicModuleParameters` — needs `SticsParameters` done (holds it by value).
24. [ ] `CentralParameterProvider` — convert **last**; holds almost every struct above by value.
    Has real methods `getPrecipCorrectionValue`/`setPrecipCorrectionValue`/`pathToOutputDir()` that
    become free functions (or get inlined if trivial enough once looked at directly).

## Per-item workflow (repeat for every checklist entry)

1. Convert the one struct in `monica-parameters.h` + `monica-parameters.cpp` per the goals above.
2. Grep the whole repo for the struct's constructors / `.merge(` / `.to_json(` / `.serialize(` /
   `.deserialize(` / `.toString(` call sites and rewire them to the new free functions.
3. Build (`cmake --build build --parallel`, via the VS dev shell as usual).
4. Run the `monica-run` task and diff `sim-min-out_section_crop.csv` +
   `sim-min-out_section_daily.csv` against the `_3.6.60` baselines — must be identical.
5. Tick the item off above (`[ ]` -> `[x]`).
6. Commit and push just that struct's change.
7. **Stop and ask the user whether to start the next item** — do not chain multiple structs in one
   go.

## Validation baseline

Same as `plan.md`:

`cmake --build build --parallel` (via the VS dev shell), then run the **"Run monica-run"** task
(`.zed/tasks.json`), then compare `sim-min-out_section_crop.csv` / `sim-min-out_section_daily.csv`
against `sim-min-out_section_crop_3.6.60.csv` / `sim-min-out_section_daily_3.6.60.csv` — must be
byte-identical.
