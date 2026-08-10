# Plan: `cultivation-method.h`/`.cpp` proceduralization (Workstep tagged union + CultivationMethod)

Companion plan to `plan.md` and `plan-monica-parameters.md`, scoped to `src/run/cultivation-method.h` /
`cultivation-method.cpp`. Same overall philosophy (plain structs + free procedures in a lowercase
namespace, `makeXxx(...)` factory free functions declared directly in `monica`), but with a new twist:
`Workstep` is a real runtime-polymorphic base class with 14 concrete subclasses, so this plan converts
it into a single plain `Workstep` struct holding a **tagged union** (`std::variant` + explicit enum tag)
of per-subtype payload structs, with the old virtual methods becoming free functions that switch on the
tag. `CultivationMethod` (which owns a `vector<WSPtr>` of worksteps) is converted last, once every
Workstep subtype has been ported.

Read `plan-monica-parameters.md` first for the shared vocabulary/conventions (`makeXxx`, lowercase
per-struct namespace, `Tools::defaultMerge` wrapper for the old `Json11Serializable::merge(j)` DEFAULT
unwrap, the implicit-`json11::Json`-conversion / `set_value_obj_value` / `toVector<T>` gotchas, etc.).
This document only calls out what's *different* for the union case.

## Big design decisions (resolved 2026-08-10 — do not re-litigate without a good reason)

1. **Union backing: `std::variant`, not a raw C `union`.** Several Workstep subtypes own non-trivial
   members (`std::string`, `std::vector`, `std::map`, `std::function`, `kj::Own<Crop>`,
   `kj::Own<CropParameters>`). A raw union would need hand-written placement-new/destructor/copy/move
   bookkeeping across ~14 variants, at every construct/destroy/copy/reassign site — high bug risk for
   no payoff, since Odin's tagged unions don't have C++'s automatic-special-member-function problem to
   begin with (Odin uses explicit/deferred cleanup, not RAII destructors) — the eventual Odin port is
   equally mechanical either way. User confirmed this explicitly (2026-08-10) after being asked.
2. **The tag is *computed*, not redundantly stored.** `WorkstepType type(const Workstep*)` returns
   `static_cast<WorkstepType>(ws->data.index())`. The `WorkstepType` enum's declaration order **must
   exactly match** the `WorkstepData` variant's alternative order — this is the one invariant to
   maintain, in exactly one place (the type alias declaration in `workstep.h`). No separate stored tag
   field to go out of sync. If this turns out to be inconvenient once more of the codebase leans on it,
   switching to a redundantly-stored field is a small, localized change — not blocking.
3. **New files `src/run/workstep.h` / `workstep.cpp`** hold `WorkstepType`, all 14 `*Data` payload
   structs, the `WorkstepData` variant alias, the `Workstep` struct itself, `WSPtr`, and all
   `workstep::...` free functions + `make*Workstep(...)` factories. `cultivation-method.h/.cpp` keep
   the *old* OOP `Workstep`/subclasses/`CultivationMethod` untouched until the final cutover step, at
   which point the old content is deleted and `cultivation-method.h/.cpp` become the new
   `CultivationMethod` struct + free functions only (matching the file's actual name/concern),
   `#include`-ing `workstep.h`. This mirrors how `output.h` and `monica-parameters.h` are already split
   by concern.
   **Correction discovered at step 2**: the new struct is actually named `WorkstepV2` (and `WSPtr` ->
   `WSPtrV2`), not `Workstep`/`WSPtr`, for the *entire* phase 0-3 duration — not just `CultivationMethod`.
   Reason: `workstep.cpp` needs `MonicaModel`'s full definition (to write `apply`/`condition`/etc bodies
   that touch `model->soilOrganic`/`model->currentEvents`/etc.), so it must `#include
   "../core/monica-model.h"` — which transitively `#include`s the *old* `cultivation-method.h` (needed
   there for `Harvest::Spec` in `harvestCurrentCrop`'s signature and other old-Workstep-family types).
   That makes both the old `class Workstep` and the new `struct Workstep` visible as `monica::Workstep`
   in the *same translation unit* (`workstep.cpp`) — a hard redefinition error, not just a shadowing
   concern like the `oid`/`output` namespace-collision from the previous conversion. Renamed the new
   struct to `WorkstepV2` / `WSPtrV2` throughout `workstep.h`/`.cpp` (step 1's file was corrected
   retroactively as part of step 2) to route around this; rename back to `Workstep`/`WSPtr` at final
   cutover (step 18) once the old class is deleted and the collision no longer exists. All references
   below in this document that say `Workstep`/`WSPtr` for the *new* type should be mentally read as
   `WorkstepV2`/`WSPtrV2` until step 18 renames them back — not rewriting every occurrence in this
   already-long document, but flagging it once, prominently, here.
4. **`WSPtr` stays `std::shared_ptr<Workstep>`**, now pointing at the new plain struct instead of the
   old polymorphic base. This preserves pointer/reference stability for the one place that needs it:
   `HarvestData::sowing` (formerly `Harvest::_sowing`) is a raw, non-owning `SowingData*` pointing into
   *another* Workstep's variant payload, set once in `CultivationMethod::merge` after all worksteps are
   already heap-allocated via `shared_ptr`. As long as nothing reallocates/moves a `Workstep` out from
   under an already-taken pointer into its variant (nothing currently does — worksteps are mutated in
   place via their `shared_ptr`, never reseated), this is safe and needs no new indirection scheme.
5. **The 3 real "is-a" chains become plain-struct inheritance for the payload structs** (goal #6 from
   `plan-monica-parameters.md`: struct-level inheritance without a virtual interface is fine to keep).
   `Workstep`'s *common* fields (date, absDate, applyNoOfDaysAfterEvent, afterEvent,
   daysAfterEventCount, daysAfterEventCountActivated, isActive, runAtStartOfDay, errors) live directly
   on the outer `Workstep` struct — **not** duplicated into every `*Data` payload — so no struct needs
   to inherit from a "WorkstepCommon" base. But 3 payload structs still inherit from another payload
   struct, because they genuinely need that struct's *extra* fields to reuse its logic:
   - `struct AutomaticSowingData : SowingData { ... }` — `AutomaticSowing::apply()` sows a real crop
     using `SowingData`'s `cropParams`/`residueParams`/etc., same as plain `Sowing::apply()`.
   - `struct TransplantData : SowingData { ... }` — same reasoning, `Transplant::apply()` calls
     `Sowing::apply()` first.
   - `struct AutomaticHarvestData : HarvestData { ... }` — needs `HarvestData`'s `sowing`/`spec`/
     `optCarbMgmtData`/`incorporateIntoLayerNo`.
   Their free `merge`/`to_json`/`apply`/etc. call into the parent payload struct's free function via
   upcast (`merge(static_cast<SowingData*>(as), j)`), exactly like
   `OrganicFertilizerParameters`/`CropResidueParameters` called into `OrganicMatterParameters`'s free
   functions in the monica-parameters.h conversion.
6. **Field names drop the leading underscore** (they were `_`-prefixed because they were
   protected/private with trivial accessor methods; as plain public struct fields the underscore
   convention no longer applies, matching every other struct in this codebase). E.g. `Sowing::_sowingDate`
   -> `SowingData::sowingDate`. `AutomaticIrrigation`'s fields were *already* unprefixed in the original
   (`absStartDate`, `irrigateCrop`, `params`, `done`, `cropPlanted`) — keep those as-is.
7. **Trivial one-line accessors get inlined at call sites and removed**, per the usual rule (e.g.
   `Harvest::sowing()`/`MineralFertilization::amount()`/`Tillage::depth()` etc. all just returned a
   field — those call sites become direct field reads: `std::get<HarvestData>(ws->data).sowing` etc.,
   or more conveniently a small inline accessor `workstep::asHarvest(Workstep*)` — decide per call site
   when doing the final cutover step, don't over-design this now).
8. **RESOLVED: `clone()` is dropped entirely, `Workstep` is move-only, no free-function equivalent.**
   The 2026-08-10 research pass (category 3) confirmed **zero** external callers of `.clone()` anywhere
   in the repo, and a follow-up grep confirmed `CultivationMethod::addApplication<T>` (the only internal
   caller of `Sowing`'s/`AutomaticSowing`'s copy constructor, via `std::make_shared<Application>(a)`) is
   itself **never called anywhere in the whole repo**, not even inside `cultivation-method.cpp` — it's
   fully dead code. So nothing anywhere relies on deep-copying a `Workstep`/`Sowing`/etc. The suspicion
   that the hand-rolled `Sowing(const Sowing& other)` copy constructor was a lossy workaround (it only
   copies `_plantDensity`, dropping everything else including `_cropParams`/`_residueParams`) for
   `kj::Own<CropParameters> _separatePerennialCropParams` being move-only turned out not to matter either
   way — just let the new `Workstep` be naturally move-only (its variant contains `kj::Own<...>` members
   in `SowingData`/`TransplantData`, which is exactly the same move-only-ness the original class hierarchy
   effectively had once you account for the copy ctor's lossiness). Every place a `Workstep` needs to move
   between owners, use `std::move`/pass-by-value; every place today's code aliases the *same* Workstep via
   a raw/shared pointer (e.g. `HarvestData::sowing`, `CultivationMethod::_allWorksteps` itself), keep doing
   that (shared_ptr aliasing, not copying).
9. **`WorkstepType` enum values are UPPER_SNAKE_CASE versions of the class names**, in the same order as
   today's declaration order in `cultivation-method.h` (which is already roughly dependency-ordered):
   `SOWING, AUTOMATIC_SOWING, TRANSPLANT, HARVEST, AUTOMATIC_HARVEST, CUTTING, MINERAL_FERTILIZATION,
   N_DEMAND_FERTILIZATION, ORGANIC_FERTILIZATION, TILLAGE, SET_VALUE, SAVE_MONICA_STATE, IRRIGATION,
   AUTOMATIC_IRRIGATION`. The `WorkstepData` variant's alternative order must match exactly (design
   decision #2).
10. **`type()`'s string form is kept separately** as `workstep::typeName(const Workstep*)` (or
    `typeName(WorkstepType)`), returning the exact same strings as today (`"Sowing"`, `"AutomaticSowing"`,
    ...) for JSON `to_json()`'s `"type"` key. The enum is for internal dispatch/comparison; the string is
    for the JSON wire format and stays byte-identical to preserve the regression baseline. The
    deprecated input aliases MONICA already accepts (`"Seed"` for Sowing, `"MineralFertiliserApplication"`,
    `"OrganicFertiliserApplication"`, `"TillageApplication"`, `"IrrigationApplication"`) are an input-only
    concern of `makeWorkstep(json11::Json)`'s dispatch, not of `typeName` (today's code never *emits*
    those aliases either — confirmed each subclass's own `type()` always returns the canonical name).
11. **`Tools::defaultMerge` wrapping happens exactly once**, in the top-level `workstep::merge(Workstep*,
    json11::Json)` (the dispatcher, called with an already-tagged `Workstep`), *not* repeated in every
    per-payload `merge(XxxData*, json11::Json)` function. This mirrors the *effect* of today's code
    (every subclass chains to `Workstep::merge` which does the DEFAULT-unwrap) without redundantly
    wrapping at every level of the 3 inheritance chains.
12. **`Harvest::Spec`/`Harvest::OptCarbonManagementData`/`Harvest::CropUsage` (incl. the
    `Harvest::greenManure`/`Harvest::biomassProduction` enumerators) and `Cutting::Value`/`Cutting::CL`/
    `Cutting::Unit` are referenced from *outside* `cultivation-method.h` today** — confirmed by the
    2026-08-10 research pass (see below): `src/core/monica-model.h`'s `harvestCurrentCrop(...)` takes
    `const Harvest::Spec&` and `Harvest::OptCarbonManagementData` (with a default arg
    `Harvest::OptCarbonManagementData()`) and reads `Harvest::greenManure` in `monica-model.cpp`;
    `src/core/crop-module.h`'s `applyCutting(...)` takes `std::map<int, Cutting::Value>&`. Both headers
    already `#include` `cultivation-method.h`/(transitively) `workstep.h`, so these nested types just
    need to keep being nameable — **keep them nested inside `HarvestData`/`CuttingData` in `workstep.h`**
    (`HarvestData::Spec`, `HarvestData::OptCarbonManagementData`, `HarvestData::CropUsage`,
    `HarvestData::greenManure`, `CuttingData::Value`, `CuttingData::CL`, `CuttingData::Unit`) rather than
    hoisting them to free-standing top-level types — smallest possible diff at the two external call
    sites (just a `Harvest::` -> `HarvestData::` / `Cutting::` -> `CuttingData::` rename), and matches
    "straight translation." **This is a step-18 (final cutover) change, not a phase-1 change** — during
    phase 1 the new `HarvestData`/`CuttingData` nested types coexist unused alongside the still-live old
    `Harvest`/`Cutting` classes that `monica-model.h`/`crop-module.h` continue to reference; only at
    cutover do those two headers' signatures/bodies switch over.

## Not in scope

- `Tools::Json11Serializable` itself — unaffected, still used elsewhere (`Crop`, `DataAccessor`,
  `CSVViaHeaderOptions`, `Soil::SoilParameters`).
- `CropParameters`, `CropResidueParameters`, `MineralFertilizerParameters`, `OrganicMatterParameters`,
  `IrrigationParameters`, `AutomaticIrrigationParameters`, `OId` — already converted
  (`plan-monica-parameters.md` / the `output.h` conversion). Used as-is via their existing free
  functions (`cropparameters::merge`, etc.).
- `Crop` (`src/core/crop.h`) — still OOP, used opaquely via `kj::Own<Crop>` inside `SowingData`/
  `TransplantData`. Out of scope for this plan; only touched by pointer/value, not converted itself.
- `MonicaModel` — only ever taken as `MonicaModel*`/`MonicaModel&` by Workstep/CultivationMethod code;
  not converted here (separate, much bigger effort, not started).
- `parseOutputIds`/`buildOutputTable`/`OId` machinery used by `SetValue` — already free-function style
  from the `output.h`/`build-output.h` conversion; used as-is.

## Per-subtype field/override reference (compiled from the current header, so future steps don't need to
re-read the whole header from scratch — but *do* re-read the actual `.cpp` bodies at each step, this
table is fields/signatures only, not behavior)

| Subtype | Inherits (payload) | Overrides beyond base | Extra fields (name only, drop leading `_`) |
|---|---|---|---|
| `Sowing` | — | clone, merge, to_json(+bool overload), type, apply, setDate | isValid, sowingDate, harvestDate, isPerennialCrop (`Tools::Maybe<bool>`), cropParams, separatePerennialCropParams (`kj::Own<CropParameters>`), residueParams, plantDensity, initialKcb |
| `AutomaticSowing` | `SowingData` | clone, merge, to_json(+overload), type, apply, condition, isActive, reinit, earliestDate, absEarliestDate, latestDate, absLatestDate, registerDailyFunction | absEarliestDate, earliestDate, latestDate, absLatestDate, minTempThreshold, daysInTempWindow, minPercentASW, maxPercentASW, max3dayPrecipSum, maxCurrentDayPrecipSum, tempSumAboveBaseTemp, baseTemp, checkForSoilTemperature, soilDepthForAveraging, daysInSoilTempWindow, sowingIfAboveAvgSoilTemp, getAvgSoilTemps (`std::function<vector<double>&()>`), inSowingRange, cropSeeded |
| `Transplant` | `SowingData` | clone, merge, to_json(+overload), type, apply, setDate | cropToPlant (`kj::Own<Crop>`), initialStage, initialGDD, initRootMass, initLeafMass, initShootMass, initLAI, postTransplantDelay, initialKcb (own copy, shadows Sowing's) |
| `Harvest` | — | clone, merge, to_json(+overload), type, apply, setDate | sowing (`Sowing*` -> becomes `SowingData*`, non-owning), exported, spec (`Spec`), optCarbMgmtData (`OptCarbonManagementData`), incorporateIntoLayerNo. Nested: `enum CropUsage{greenManure,biomassProduction}`, `struct OptCarbonManagementData`, `struct Spec{struct Value; map<int,Value> organ2specVal;}` — nested types carry over unchanged, still fine nested inside a plain struct. |
| `AutomaticHarvest` | `HarvestData` | clone, merge, to_json(+overload), type, apply, condition, isActive, reinit, latestDate, absLatestDate | harvestTime (string), latestDate, absLatestDate, minPercentASW, maxPercentASW, max3dayPrecipSum, maxCurrentDayPrecipSum, cropHarvested |
| `Cutting` | — | clone, merge, to_json, type, apply | organId2cuttingSpec (`map<int,Value>`), organId2biomAfterCutting (`map<int,double>`), organId2exportFraction (`map<int,double>`), cutMaxAssimilationRateFraction. Nested: `enum CL{cut,left,none}`, `enum Unit{percentage,biomass,LAI}`, `struct Value{double value; Unit unit; CL cut_or_left;}` |
| `MineralFertilization` | — | clone, merge, to_json, type, apply | partition (`MineralFertilizerParameters`), amount |
| `NDemandFertilization` | — | clone, merge, to_json, type, apply, condition, isActive, reinit | initialDate, partition, Ndemand, depth, stage, appliedFertilizer |
| `OrganicFertilization` | — | clone, merge, to_json, type, apply | params (`OrganicMatterParameters`), amount, incorporation, incorporateIntoLayerNo |
| `Tillage` | — | clone, merge, to_json, type, apply | depth |
| `SetValue` | — | clone, merge, to_json, type, apply | oid (`OId`), value (`json11::Json`), getValue (`std::function<json11::Json(const MonicaModel*)>`) |
| `SaveMonicaState` | — | clone, merge, to_json, type, apply | pathToFile, toJson, noOfPreviousDaysSerializedClimateData |
| `Irrigation` | — | clone, merge, to_json, type, apply | amount, params (`IrrigationParameters`) |
| `AutomaticIrrigation` | — | clone, merge, to_json, type, apply, condition, reinit | absStartDate, absEndDate, irrigateCrop, startStage, endStage, params (`AutomaticIrrigationParameters`), done, cropPlanted (no leading underscore already in original) |

Base `Workstep` virtual surface (becomes `workstep::...` free functions dispatching on `type(ws)`,
falling through to the common-field behavior when a subtype doesn't override): `clone` (see decision
#8), `merge`, `to_json`, `type`/`typeName`, `date`, `absDate`, `earliestDate`, `absEarliestDate`,
`latestDate`, `absLatestDate`, `setDate`, `noOfDaysAfterEvent` (trivial, no override — just field read),
`afterEvent` (trivial, no override), `apply`, `applyWithPossibleCondition` (non-virtual, real logic,
calls `isActive`/`isDynamicWorkstep`/`condition`/`apply` — straight port), `condition` (base has real
"after event" logic, used directly by most subtypes; overridden by `AutomaticSowing`/`AutomaticHarvest`/
`NDemandFertilization`/`AutomaticIrrigation`), `isDynamicWorkstep` (trivial: `!date.isValid()`),
`isActive` (overridden by `AutomaticSowing`/`AutomaticHarvest`/`NDemandFertilization`), `reinit` (base
has real logic; overridden by `AutomaticSowing`/`AutomaticHarvest`/`NDemandFertilization`/
`AutomaticIrrigation`), `registerDailyFunction` (only overridden by `AutomaticSowing`), `runAtStartOfDay`
(trivial field read, non-virtual already), `errors` (trivial field read, non-virtual already).

## Conversion order

### Phase 0 — scaffolding

1. [x] Create `src/run/workstep.h` / `workstep.cpp`. Added to `CMakeLists.txt`'s `monica_lib` sources
   list right before the existing `src/run/cultivation-method.h`/`.cpp` lines. Defined
   `enum class WorkstepType` (14 values, order per decision #9), all 14 `*Data` payload structs (fields
   only, mechanically transcribed from the table above with leading underscores dropped, the 3
   struct-inheritance relationships kept: `AutomaticSowingData : SowingData`, `TransplantData :
   SowingData`, `AutomaticHarvestData : HarvestData`), the `WorkstepData` variant alias (order matches
   the enum), the `Workstep` struct (common fields + `WorkstepData data`), `WSPtr`, and the one trivial
   free function `inline WorkstepType type(const Workstep*)` (decision #2 — `typeName(...)` deliberately
   deferred to step 16/central-dispatch since it needs a full 14-case switch, not a fit for this
   data-only step). `workstep.cpp` is currently just `#include "workstep.h"` (no logic yet). Kept the
   exact same include list `cultivation-method.h` already uses (`<functional>`, `<map>`, `<memory>`,
   `<string>`, `<vector>`, `+<variant>`, `json11/json11.hpp`, `../core/crop.h`,
   `../core/monica-parameters.h`, `../io/output.h`, `common/dll-exports.h`, `json11/json11-helper.h`,
   `tools/date.h`) — `Tools::Maybe<bool>` (needed by `SowingData::isPerennialCrop`) comes transitively
   through `json11/json11-helper.h` -> `tools/helper.h`, verified. `cmake --build build --parallel`
   succeeds cleanly (`workstep.cpp.obj` compiles, `monica_lib` links, all executables still link) —
   confirms the new file compiles standalone with zero knock-on breakage, since nothing includes
   `workstep.h` yet. **Build-only validation**, as planned — no regression run yet.

### Phase 1 — one step per subtype, fully self-contained, nothing wired to a central dispatcher yet

For each subtype: add `make<Type>Workstep(json11::Json)` (+ any other live non-JSON constructor
overloads the original had, e.g. `Cutting(const Tools::Date&)`, `MineralFertilization(const Date&,
MineralFertilizerParameters, double)`, `Tillage(const Date&, double)`, etc. — check the header's public
constructor list per subtype) as free functions returning `Workstep` (tagged + payload set), plus
`merge(XxxData*, json11::Json)`, `to_json(const XxxData*)` (+ the `bool includeFullCropParameters`
overload where the original had one — `Sowing`/`AutomaticSowing`/`Transplant`/`Harvest`/
`AutomaticHarvest`), `apply(XxxData*, Workstep*, MonicaModel*)`, and `condition(...)`/`reinit(...)` only
for the subtypes that override them (see table). Read the *actual* `.cpp` body for each (this plan
doesn't transcribe full logic, just structure) and translate 1:1 — these functions call into
already-converted free-function subsystems (`cropparameters::`, `cropresidueparameters::`,
`cropmodule::`, `soilorganic::`, `soiltransport::`, `soilcolumn::`, `soilmoisture::`, `monicamodel::`,
`automaticirrigationparameters::`, `oid::`, `output::`) so this should read very naturally. None of these
new functions are called from anywhere yet (dead code, only reachable once phase 2 wires them in) — so
validation per step is **build-only**, not a regression run.

**Signature conventions settled at steps 2/3 (apply to every remaining phase-1 step)**:
- All per-payload free functions live in `namespace workstep`, overloaded by first-parameter type (14
  types' worth of `merge`/`to_json`/`apply`/etc. coexist there via ordinary C++ overload resolution — no
  need for 14 more per-type namespaces; the struct + `make*Workstep` factories stay bare in `monica`,
  matching the plan.md goal #6 convention).
- `merge(XxxData*, json11::Json)` — **no** `Workstep*` parameter; subtype-specific field merging never
  touches common Workstep fields (those are handled once, separately, by `workstep::mergeCommon`).
- `to_json(const XxxData*, const WorkstepV2*, bool includeFullCropParameters = true)` — **does** need
  `ws` (every subtype's `to_json` embeds `"date"` from the common field). The `includeFullCropParameters`
  bool turned out to be a real (if mostly-inert) parameter, not dead: `Sowing`/`AutomaticSowing`/
  `Harvest`/`AutomaticHarvest`'s own bodies never read it, but **`Transplant::to_json` does** — forwards
  it into `Crop::to_json(bool)` (`crop.cpp:238`) which genuinely branches on it. Kept the parameter on
  all 5 `to_json` overloads that originally had it, for exact fidelity, even where currently a no-op.
- `apply`/`reinit(XxxData*, WorkstepV2*, ...)` — need `ws` whenever the body chains to
  `workstep::applyCommon`/`workstep::reinitCommon`/`workstep::setDate` (i.e. whenever the original called
  `Workstep::apply(model)`/`Workstep::reinit(...)`/`setDate(...)` as part of its own override) — check
  the actual body per subtype rather than assuming.
- `condition`/`registerDailyFunction` — only take `WorkstepV2*` if the body actually touches a common
  field; `AutomaticSowingData`'s versions of both don't (verified by reading the body), so they're
  `(AutomaticSowingData*, MonicaModel*)` / `(AutomaticSowingData*, std::function<...>)` with no `ws` at
  all. Don't force a uniform signature shape across functions that don't need it.
- Byte-fidelity gotcha: `AutomaticSowing::to_json`'s unit strings for `min-temp`/`temp-sum-above-base-
  temp`/`base-temp`/`avg-soil-temp.Tavg` contain what *looks* like "°C" but is actually the 3-byte UTF-8
  encoding of U+FFFD (the REPLACEMENT CHARACTER, `EF BF BD`) followed by `C` — verified via raw hex dump
  of `cultivation-method.cpp`, not a real degree sign (probably a historical mojibake artifact from a
  bad encoding round-trip). Preserved the exact same bytes in the port (`"\xEF\xBF\xBD" "C"` — note the
  string-literal-concatenation split is required, `"\xEF\xBF\xBDC"` mis-parses as a single 3-hex-digit
  `\xBDC` escape which is out of `char` range and fails to compile) rather than "fixing" it to a real °,
  since this is meant to be a straight translation.
- **Discovered at step 2, corrects step 1 retroactively**: the "Workstep-level common infrastructure"
  (`workstep::mergeCommon`, `workstep::applyCommon`, `workstep::conditionCommon`, `workstep::reinitCommon`,
  `workstep::setDate`) was originally planned for phase 2 (step 16), but `AutomaticSowingData::reinit`
  genuinely needs `workstep::reinitCommon` (chains to old `Workstep::reinit`) and `workstep::setDate`
  (chains to old `Sowing::setDate`, called by `AutomaticSowing::reinit`) *right now* — these don't
  require every subtype to exist first (the struct types were all already fully defined in step 1, only
  their merge/apply/etc. *functions* are added incrementally), so they were built early, in step 2, as
  shared infrastructure. `setDate` in particular is now the *actual, permanent* central dispatcher (not
  a "Common" stand-in) — unlike merge/apply/condition/reinit, it has no single shared body since 3 of 14
  subtypes override it, so it was always going to be a switch; it's just populated incrementally
  (currently: `SOWING`/`AUTOMATIC_SOWING`/`TRANSPLANT` cases + `default:`, after step 4) rather than
  written all at once in step 16.
  `workstep::mergeCommon`/`applyCommon`/`conditionCommon`/`reinitCommon` remain as planned: true "common
  body" helpers, reused by both phase-1 factories and step 16's dispatcher.

2. [x] `SowingData` — leaf (no struct-inheritance dependency). Constructor: JSON only
   (`makeSowingWorkstep(json11::Json)`, factory sets `ws.data = SowingData{}` then calls
   `workstep::mergeCommon` + `workstep::merge(&sowingData, j)`). The *dead* commented-out reader-based
   constructor/deserialize/serialize stubs at the top of the old `Sowing` class were dropped (not
   ported), matching how other dead code has been handled throughout this codebase's conversions.
   `workstep::merge`/`to_json`/`apply` all ported 1:1 from `Sowing::merge`/`to_json(bool)`/`apply`.
3. [x] `AutomaticSowingData` — needed `SowingData` done (item 2). `workstep::merge(AutomaticSowingData*,
   ...)` calls `workstep::merge(static_cast<SowingData*>(as), ...)` first (decision #5). Ported
   `apply`/`condition`/`reinit`/`registerDailyFunction` plus the 3 file-local helper functions
   (`isSoilMoistureOk`, `isPrecipitationOk`, `isSoilTemperatureOk` — already free functions in the
   original, not methods, moved unchanged into an anonymous namespace in `workstep.cpp`, shared by both
   `AutomaticSowing` and — per the original code — `AutomaticHarvest`, which will need them again at
   step 6). `AutomaticSowing::reinit`'s two subtleties preserved exactly: it calls the base reinit with
   only `(date, addYear)` — never forwarding `forceInitYear` to it even though `AutomaticSowing::reinit`
   itself receives one — and it computes `absLatestDate` *before* `absEarliestDate`, feeding
   `forceInitYear || !addedYear1` (not just `forceInitYear`) into the earliest-date computation.
4. [x] `TransplantData` — needed `SowingData` done (item 2). `apply` calls
   `workstep::apply(static_cast<SowingData*>(t), ws, model)` first, then Transplant-specific logic
   (`cropmodule::forceTransplantState`, verified signature `(CropModule*, double temperatureSum, double
   lai, size_t stage, double rootMass, double leafMass, double shootMass, int postTransplantDelay)`
   against `crop-module.h:500` before writing the call). Added the `TRANSPLANT` case to the
   `workstep::setDate` dispatcher (`cropToPlant->setSeedDate(date)`, guarded by `if (cropToPlant)` — see
   below). Two things worth flagging for later steps: (1) `Transplant::to_json` never embeds `"date"`
   (unlike `Sowing`/`AutomaticSowing`) — a genuine discrepancy in the original, preserved exactly, so
   `workstep::to_json(const TransplantData*, bool)` takes **no** `WorkstepV2*` parameter, unlike every
   other subtype's `to_json` so far. (2) `TransplantData::cropToPlant` (`kj::Own<Crop>`) is **never
   actually assigned anywhere** in the live code — grepped `_cropToPlant` across the whole file and the
   only non-comment hit is the `to_json` null-check itself; the `kj::heap<Crop>()`/`deserialize` init
   that would populate it is inside the same dead, commented-out reader-based block already noted for
   `Sowing` (item 2). So `cropToPlant` is always null today and both the `to_json` ternary and the new
   `setDate` guard are currently dead branches — preserved as-is (not my job to fix or simplify a
   pre-existing unused code path during a straight-translation pass).
5. [x] `HarvestData` — leaf. `sowing` field type is `SowingData*` (raw, non-owning — decision #4). Note
   `setSowing`/`sowing()` trivial accessors get inlined (decision #7) — just set/read the field directly
   from `CultivationMethod::merge`'s new version (phase 3). `setDate` does **not** need an explicit
   `HARVEST` case in the `workstep::setDate` dispatcher: `Harvest::setDate` was already a no-op beyond
   the common `_date = date` assignment (the extra `_sowing->crop()->setHarvestDate(date)` line was
   already commented out in the original), so it's behaviorally identical to `default:` — left the
   dispatcher as-is (`SOWING`/`AUTOMATIC_SOWING`/`TRANSPLANT` + `default:`).
   **Real blocker hit and resolved**: `Harvest::apply` calls `monicamodel::harvestCurrentCrop(model,
   exported, spec, optCarbMgmtData, layerIdx)`, and that function's signature (`monica-model.h:140-144`)
   still takes the *old* `Harvest::Spec`/`Harvest::OptCarbonManagementData` types — can't change that
   signature yet without breaking the still-live old `Harvest::apply()` too (final cutover's job, per
   decision #12). Since `HarvestData::Spec`/`OptCarbonManagementData` are structurally identical but a
   *different, unrelated* C++ type with no implicit conversion, added two small anonymous-namespace
   conversion helpers in `workstep.cpp` (`toOldHarvestSpec`, `toOldOptCarbMgmtData`) that build the old
   types from the new ones, purely as a temporary bridge — deleted at step 18 once
   `harvestCurrentCrop`'s signature itself switches to the new types (no more conversion needed then).
   This works with **zero new includes**: the old `Harvest` class is already visible in `workstep.cpp`
   transitively via `monica-model.h` -> `cultivation-method.h` (the same include that originally forced
   the `Workstep`->`WorkstepV2` rename), and `Harvest`/`Harvest::Spec`/etc. don't collide with any new
   name (only bare `Workstep`/`WSPtr` did — every subtype class name like `Harvest`/`Sowing`/etc. was
   never reused, since the new payload structs are named `HarvestData`/`SowingData`/etc.). Also moved
   the file-local `organIdFromName`/`organNameFromId` helpers (used by both `Harvest`'s and, from step 7,
   `Cutting`'s merge/to_json) into `workstep.cpp`'s anonymous namespace — the *originals* in
   `cultivation-method.cpp` have external linkage at global scope (not wrapped in an anonymous namespace
   or `static`), so bare copies in `workstep.cpp` would have been a duplicate-symbol **link** error once
   both files are compiled into `monica_lib`; the anonymous-namespace wrapping (already used for
   `makeInitAbsDate`/`isSoilMoistureOk`/etc. since step 2-3) sidesteps this via internal linkage.
   Build succeeded with no duplicate-symbol errors, confirming this.
6. [x] `AutomaticHarvestData` — needed `HarvestData` done (item 5). `apply`/`condition`/`reinit` call
   into `cropmodule::maturityReached`, `isSoilMoistureOk`/`isPrecipitationOk` (the file-local helpers
   moved into `workstep.cpp`'s anonymous namespace back at item 3 — confirmed shared verbatim between
   `AutomaticSowing` and `AutomaticHarvest` in the original, no changes needed to reuse them here).
   Two things worth flagging:
   - **Constructor-only default, not a header default member initializer**: unlike every other field
     seen so far, `_harvestTime` had no inline default in the original header — both
     `AutomaticHarvest()`/`AutomaticHarvest(json11::Json)` constructors set it to `"maturity"`
     explicitly in their member-initializer-lists instead. Folded this into a default member
     initializer on `AutomaticHarvestData::harvestTime` in `workstep.h` (`{"maturity"}`) rather than
     setting it imperatively in `makeAutomaticHarvestWorkstep` — same simplification precedent as
     `plan-monica-parameters.md` item 19 (`SoilMoistureModuleParameters`'s default-lambda constructor).
   - **Preserved a real bug, not a typo of mine**: `AutomaticHarvest::to_json`'s `o["max-%-asw"]`/
     `o["max-3d-precip-sum"]`/`o["max-curr-day-precip"]` lines use the **comma operator**, not `=`, in
     the original (`o["max-%-asw"], J11Array{...};` rather than `o["max-%-asw"] = J11Array{...};`) —
     each line computes and discards a `J11Array`, leaving that JSON key holding a default/null
     `json11::Json` (inserted only as a side effect of `operator[]`) instead of the intended array.
     Verified by re-reading the raw source a second time before transcribing. Preserved byte-for-byte
     (literally kept the comma-operator syntax, with an explanatory comment) rather than "fixing" it to
     `=` — straight translation, not a cleanup pass; flagging clearly in case a future simplification
     pass wants to decide whether to fix this real bug or intentionally keep matching legacy output.
7. [x] `CuttingData` — leaf. Kept `enum CL`/`enum Unit`/`struct Value` nested inside `CuttingData`
   (matching the original's scoping, avoids any name clash with `HarvestData::Spec::Value`, a different
   `Value`). Reused `organIdFromName`/`organNameFromId` from item 5's anonymous namespace.
   **Same cross-file old/new-type bridging problem as item 5's `harvestCurrentCrop`, one level trickier**:
   `cropmodule::applyCutting(CropModule*, std::map<int, Cutting::Value>&, std::map<int, double>&,
   double)` (`crop-module.h:503`) still takes the old `Cutting::Value` type, **by mutable reference** —
   and unlike `harvestCurrentCrop`'s `Harvest::Spec` (read-only), `applyCutting`'s implementation
   actually *writes back* into the `organs` map (fills it from `pc_OrganIdsForCutting` when the caller
   passes it empty — `crop-module.cpp:5539-5545`), and the original `Cutting::apply` passes its own
   `_organId2cuttingSpec` member by reference, so that mutation persists on the object. A one-way
   `toOldCuttingSpec` conversion (mirroring item 5's `toOldHarvestSpec`) would silently drop that
   persistence. Added `toOldCuttingSpec`/`fromOldCuttingSpec` (both in the anonymous namespace) and
   round-tripped through them in `workstep::apply(CuttingData*, ...)`: convert to the old type, call
   `applyCutting`, convert the (possibly now-filled-in) result back into `c->organId2cuttingSpec`. The
   `exports` parameter (`std::map<int, double>&`) needed **no** conversion at all — same concrete type
   in old and new code, passed straight through by reference (mutations naturally propagate). Both
   bridging helpers get deleted at step 18 alongside item 5's, once `applyCutting`'s signature itself
   switches to `CuttingData::Value`.
   **Another preserved-not-fixed discrepancy, found by close re-reading**: `Cutting::to_json()` builds a
   full `organsBiomAfterCutting` J11Object from `_organId2biomAfterCutting` but **never includes it** in
   the JSON object literal it returns — the computation is dead. Preserved exactly (computed, discarded),
   with a comment flagging it, same treatment as item 6's comma-operator bug.
   `set_double_value(...)`'s 4-arg transform-lambda overload (`json11-helper.h:267-272`) verified before
   use for `cut-max-assimilation-rate`'s `/100.0` conversion.
8. [x] `MineralFertilizationData` — leaf. No cross-file bridging needed here (unlike items 5/7):
   `monicamodel::applyMineralFertiliser(MonicaModel*, MineralFertilizerParameters, double)`
   (`monica-model.h:146-148`) already takes the *already-converted* plain-struct
   `MineralFertilizerParameters` from the `monica-parameters.h` work, by value — straightforward
   pass-through. Ported both constructors: the JSON one and the field-based
   `(const Tools::Date&, MineralFertilizerParameters, double)` one (`makeMineralFertilizationWorkstep`
   overloaded on parameter shape) — kept for interface completeness even though, per the step-4/5/7
   research, nothing outside `cultivation-method.cpp` constructs any subtype directly today (matches the
   `plan-monica-parameters.md` precedent of porting zero-caller overloads, e.g. its item 11). Another
   leak-forward `toString()` case, same shape as `plan-monica-parameters.md`'s `CropModuleParameters`
   items: `MineralFertilization::apply` calls `debug() << toString() << endl;`, and `MineralFertilization`
   never overrides `toString()`, so this was always really `to_json().dump()` (the `Json11Serializable`
   default) — **not** the CultivationMethod-style real/human-readable `toString()` override (that one's
   still coming in phase 3, don't confuse the two) — became
   `debug() << workstep::to_json(mf, ws).dump() << endl;`, calling this step's own `to_json` directly (no
   need for the not-yet-built top-level `workstep::to_json(const WorkstepV2*)` dispatcher). Added
   `<iostream>` to `workstep.cpp`'s includes (needed for `cerr`, used by the partition-merge error log;
   was previously arriving only transitively).
9. [x] `NDemandFertilizationData` — leaf. First subtype where `merge` genuinely needs `WorkstepV2*`
   (breaking the "merge never needs ws" pattern established by items 2-8): `_initialDate = date();` in
   the original copies the just-parsed common date into the subtype's own field, so
   `workstep::merge(NDemandFertilizationData*, WorkstepV2*, json11::Json)` reads `ws->date` (set by
   `mergeCommon`, called first in the factory, before this). `to_json`, conversely, needs **no** `ws` at
   all — it only ever emits its own `initialDate`/`stage` fields (never the common date), so
   `workstep::to_json(const NDemandFertilizationData*)` stayed ws-less like `Transplant`'s. Two more
   preserved-not-fixed discrepancies:
   - `reinit` computes `addedYear` from `workstep::reinitCommon(...)` but the function **unconditionally
     `return`s `false`**, discarding it — confirmed by re-reading twice. Preserved with a comment, same
     treatment as the recurring "computed but discarded" pattern from items 6/7.
   - `reinit` also calls `setDate(_initialDate)` **before** `Workstep::reinit(...)`, and forwards
     `forceInitYear` through to the common reinit — both opposite of `AutomaticSowing`/`AutomaticHarvest`'s
     `reinit` (item 3/6), which called the common reinit *first* and never forwarded `forceInitYear`.
     Confirms these per-subtype quirks really do vary and must be read individually each time, not
     assumed from a sibling's pattern.
   Ported all 3 original constructors (JSON, `(int stage, double depth, MineralFertilizerParameters,
   double Ndemand)`, `(Tools::Date date, double depth, MineralFertilizerParameters, double Ndemand)`) as
   overloaded `makeNDemandFertilizationWorkstep(...)` factories.
10. [x] `OrganicFertilizationData` — leaf. Straightforward, no quirks found — `merge`/`to_json` both
   ws-shaped exactly like `MineralFertilizationData` (item 8: `to_json` needs `ws` for the common date,
   `merge` doesn't). `monicamodel::applyOrganicFertiliser(MonicaModel*, const OrganicMatterParameters&,
   double, bool, int)` (`monica-model.h:149-152`) already takes the converted plain-struct
   `OrganicMatterParameters` — no cross-file bridging needed, same story as item 8. Same
   `toString()`-is-really-`to_json().dump()` leak-forward in `apply` as items 8/9. Ported both
   constructors (JSON and field-based).
11. [x] `TillageData` — leaf, the simplest one so far (single `depth` field, no cross-file bridging,
   no quirks). `monicamodel::applyTillage(MonicaModel*, double)` takes a plain `double`, nothing to
   convert. Ported both constructors.
12. [x] `SetValueData` — leaf. Uses `parseOutputIds`/`buildOutputTable`/`buildPrimitiveCalcExpression`
    (already free-function style, from `build-output.h`) — added `#include "../io/build-output.h"` to
    `workstep.cpp`. No leak-forward concerns, just called them directly with the same signatures.
    **Real bug caught and fixed before it could bite**: the original `SetValue::merge`'s third branch
    (`_getValue = [=](const MonicaModel *) { return _value; };`) captures `this` implicitly via `[=]` and
    reads `this->_value` live at call time — safe in the original because `SetValue` objects are always
    heap-allocated via `shared_ptr` (`this` never moves after construction). Transcribing this literally
    as `s->getValue = [s](const MonicaModel *) { return s->value; };` would **not** be safe: `s` here is
    `&std::get<SetValueData>(ws.data)` where `ws` is still a local `WorkstepV2` inside
    `makeSetValueWorkstep`, about to be `return`ed by value — NRVO isn't guaranteed, so a move could
    relocate the variant's storage to a new address between `merge()` returning and the caller receiving
    the value, leaving `s` dangling. Fixed by capturing a **value copy** of `s->value` instead of the
    pointer (`[value = s->value](const MonicaModel *) { return value; }`) — behaviorally identical since
    `value` is never reassigned again after this `merge()` call for the object's lifetime, but with no
    dangling-pointer risk. **Worth remembering for any future step (or the phase-2/3 central-dispatch and
    `CultivationMethod` work) that stores a lambda capturing a pointer into a payload struct**: only safe
    if the lambda is created *after* the `Workstep`/`WorkstepV2` has already reached its final stable
    address (e.g. `AutomaticSowingData::registerDailyFunction`, item 3, is fine — it's called externally,
    well after the object is already owned via `WSPtr`, not from inside `merge`/the factory).
13. [x] `SaveMonicaStateData` — leaf. `apply`'s real capnp serialization I/O (`kj::newDiskFilesystem`,
    `capnp::MallocMessageBuilder`, `monicamodel::serialize`) ported unchanged; added the needed capnp/kj
    includes (`<capnp/compat/json.h>`, `<capnp/message.h>`, `<capnp/serialize.h>`, `<kj/filesystem.h>`,
    `<kj/string.h>`, `"model/monica/monica_state.capnp.h"`) to `workstep.cpp`, matching
    `cultivation-method.cpp`'s own include list. `isAbsolutePath` confirmed already available
    transitively (`tools/helper.h`, via `json11/json11-helper.h`, already in `workstep.h`).
    **`merge` needed `WorkstepV2*` for an unusual reason** (different shape from items 9/12's): the
    original constructor sets `_runAtStartOfDay = false` (this subtype defaults to running at the *end*
    of the day, unlike every other subtype's `true` default), and `merge` **re-parses**
    `"runAtStartOfDay"` with an explicit `false` default (`set_bool_valueD(...,false)`), overriding
    whatever the generic `mergeCommon` already left on the common field. So
    `workstep::merge(SaveMonicaStateData*, WorkstepV2*, json11::Json)` writes `ws->runAtStartOfDay`
    directly. `to_json` also needs `ws` (emits the common `runAtStartOfDay`) but — like `Transplant`
    (item 4) and this subtype's own constructor-set field pattern — **never emits `"date"`**, another
    subtype confirmed to skip it. The field-based factory (`makeSaveMonicaStateWorkstep(const Date&,
    string, bool, int)`) sets `ws.runAtStartOfDay = false` directly, mirroring the constructor.
14. [x] `IrrigationData` — leaf. Simple, no quirks: `merge`/`to_json` follow the standard
    ws-for-to_json-only shape (items 8/10/etc.), `IrrigationParameters` already a converted plain struct
    (`monicamodel::applyIrrigation(MonicaModel*, double, double, ...)` takes plain values). Inlined the
    trivial `nitrateConcentration()` accessor at its one call site (`i->params.nitrateConcentration`),
    per the usual rule.
15. [x] `AutomaticIrrigationData` — leaf. `apply` confirmed to always `return false` unless `done` was
    *already* true at entry (never "finishes" via its own return value — relies on
    `condition`/`reinit`'s `done` flag instead) — preserved exactly, and also confirmed `apply` never
    calls `applyCommon`/touches `ws` at all (unlike every other subtype), so its new signature is
    `apply(AutomaticIrrigationData*, MonicaModel*)`, no `WorkstepV2*`. Same story for `to_json`,
    `condition`, and `merge` — none of the three touch any common field, so all are ws-less too
    (`to_json` skips `"date"` again, joining `Transplant`/`SaveMonicaState`). Only `reinit` needs `ws`
    (chains to `reinitCommon`/`setDate`) — so this subtype is the first with only *one* of its five
    functions needing `ws`, everything else ws-less. **Another preserved-not-fixed discrepancy**:
    `merge`'s `startStage`/`endStage` 1-based-to-0-based conversion uses
    `std::max(0, startStage--)`/`std::min(7, endStage--)` — the post-decrement's side effect (setting
    `startStage` to `startStage-1`) is immediately **clobbered** by the enclosing assignment
    (`startStage = std::max(0, <the pre-decrement value>)`), so the `--` has **no net effect** — net
    result is exactly `startStage = std::max(0, startStage)`, decrement-free. Verified by careful
    sequencing analysis (C++17 guarantees the RHS, including the `--`'s side effect, fully evaluates
    before the assignment). Almost certainly an original bug (probably meant `--startStage` or
    `startStage - 1`), preserved literally with an explanatory comment, not fixed. Only one field-less
    constructor exists for this subtype (default + JSON, no field-based overload), so only
    `makeAutomaticIrrigationWorkstep(json11::Json)` was added (default aggregate-init already covers
    the no-args case, no separate factory needed, matching precedent from earlier plans).

**Phase 1 complete**: all 14 subtypes now have full `workstep::` free functions in `workstep.cpp`/
`workstep.h`, still entirely self-contained and unreachable from any live code path. Next is phase 2
(central dispatch, step 16).

### Phase 2 — central dispatch (single step, after all 14 payload structs above exist)

16. [x] Wrote the `workstep::...` dispatcher free functions in `workstep.cpp` (declared in `workstep.h`).
    Diverged from the original sketch above in a few ways, each for a concrete reason found while
    actually writing it:
    - **Fixed a real gap discovered from phase 1**: none of the 14 `make*Workstep(json11::Json)`
      factories ever applied the `Tools::defaultMerge` DEFAULT/"=" JSON-unwrap that the original
      `Workstep::merge` did via `Json11Serializable::merge(j)` — every phase-1 factory called
      `mergeCommon` (no wrap) then the per-payload `merge` directly, skipping it entirely. Fixed by
      adding the `defaultMerge` wrap to `workstep::mergeCommon` itself (retroactively editing the
      function built in step 2) rather than adding it to the new top-level `workstep::merge(WorkstepV2*,
      json11::Json)` dispatcher as decision #11 originally sketched — `mergeCommon` is the actual
      "called exactly once, first, for every subtype" entry point in this design (every one of the 14
      factories already calls it first), exactly mirroring how the original's `Json11Serializable::merge`
      call happened exactly once per external invocation regardless of how many levels of subtype
      chaining sat above it in the original class hierarchy. This one-line fix in `mergeCommon`
      automatically fixes all 14 already-built factories with no changes needed to them.
    - **Dropped `typeName()` entirely** (the string-returning form of `type()`) — the 2026-08-10 research
      pass confirmed zero external callers of `.type()` anywhere in the repo, and its only internal uses
      (`CultivationMethod::merge`'s `wsType` string comparisons, `allDynamicWorkstepsFinished`) become
      cleaner `WorkstepType` enum comparisons in phase 3 instead, so no string form is needed at all.
      Every per-subtype `to_json` already hardcodes its own literal type string directly (e.g.
      `{"type", "Sowing"}`), so the central `to_json` dispatcher doesn't need one either.
    - **Also skipped building free functions for `date()`/`noOfDaysAfterEvent()`/`afterEvent()`/
      `runAtStartOfDay()`/`errors()`** — all five are trivial, never-overridden field reads, so every
      call site (mostly phase 3's job) just becomes `ws->date`/`ws->applyNoOfDaysAfterEvent`/
      `ws->afterEvent`/`ws->runAtStartOfDay`/`ws->errors` directly, per the usual trivial-accessor rule.
      `absDate()` **does** get a function (`inline Tools::Date absDate(const WorkstepV2*)` in the header)
      since it has real logic (`date.isAbsoluteDate() ? date : absDate`) despite never being overridden.
    - **`makeWorkstepV2(json11::Json)` returns `WSPtrV2` (`shared_ptr<WorkstepV2>`), not a bare
      `WorkstepV2`** — a correction from the initial phrasing above. The original `monica::makeWorkstep`
      returns `WSPtr` and returns `{}` (null) for an unrecognized `"type"` string, which
      `CultivationMethod::merge` checks via `if (!ws) continue;`. A bare-`WorkstepV2`-returning factory
      can't represent "no workstep" the same way, so `makeWorkstepV2` wraps each phase-1
      `make<Type>Workstep(j)` result via `make_shared<WorkstepV2>(...)` and returns `{}` for unrecognized
      types, matching the original's nullable-return shape exactly (and conveniently already producing
      the `WSPtrV2` that `CultivationMethodV2::_allWorksteps` will hold in phase 3).
    - **Deliberately did *not* refactor the 14 phase-1 factories to route through the new
      `workstep::merge(WorkstepV2*, json11::Json)`** (which would collapse each factory's
      `mergeCommon(...); merge(&payload, ...);` pair into one call) — the `mergeCommon` fix above already
      makes them correct without touching them, and reworking 14 already-built/committed functions adds
      diff/risk to a step that's supposed to be about wiring, not refactoring. Left as a candidate for a
      later simplification pass, not done now.
    Every other function (`merge`, `to_json`, `isActive`, `apply`, `applyWithPossibleCondition`,
    `condition`, `reinit`, `registerDailyFunction`, `earliestDate`, `absEarliestDate`, `latestDate`,
    `absLatestDate`) matches the original sketch: a `switch (type(ws))` with explicit cases for every
    subtype that overrides that piece of behavior and a `default:` (or, for the 2 functions where every
    subtype except `AUTOMATIC_SOWING` shares the base behavior, an `if`) falling through to the relevant
    `*Common` helper or the trivial base expression. `makeWorkstepV2`'s if-chain mirrors the original
    `monica::makeWorkstep` exactly, including every deprecated type-string alias.
    Builds clean on the first attempt. **Build-only validation**, as planned — still nothing external
    calls into any of this.

### Phase 3 — CultivationMethod (single step, after phase 2)

17. [x] Wrote `struct CultivationMethodV2` + `cultivationmethod::...` free functions in `workstep.cpp`/
    `workstep.h` (kept in the same file pair rather than splitting — the split-at-cutover plan from
    decision #3 still stands, just didn't bother doing it early here since nothing external references
    either type yet). Ported `merge`/`to_json`/`apply`/`absApply`/`apply(model, bool)`/`nextDate`/
    `nextAbsDate`/`workstepsAt`/`absWorkstepsAt`/`areOnlyAbsoluteWorksteps`/`staticWorksteps`/
    `allDynamicWorksteps`/`allDynamicWorkstepsFinished`/`startDate`/`absStartDate`/
    `absLatestSowingDate`/`endDate`/`absEndDate`/`toString`/`reinit`, all 1:1, confirmed against a fresh
    full re-read of the original `.cpp` (not just recalled from the earlier planning pass). All the
    predicted translation points panned out exactly as sketched:
    - `CultivationMethod::merge`'s two `dynamic_cast`s became a `switch (workstep::type(ws.get()))` with
      `SOWING`/`AUTOMATIC_SOWING` cases setting `sowingWS` and `HARVEST`/`AUTOMATIC_HARVEST` cases
      setting `.sowing = sowingWS` on the (upcast-compatible, per decision #5) payload.
    - `absLatestSowingDate`'s `dynamic_cast<Sowing*>` (which in the original succeeds for *both*
      `Sowing` and `AutomaticSowing` objects, since the latter is-a the former, then calls the possibly-
      virtual `absLatestDate()`) becomes `type(ws) == SOWING || type(ws) == AUTOMATIC_SOWING` guarding a
      call to `workstep::absLatestDate(ws.get())` — which *already* does the right thing for both cases
      via its own switch (returns the `AutomaticSowingData`-specific field for `AUTOMATIC_SOWING`, falls
      through to `absDate(ws)` for plain `SOWING`), exactly replicating what virtual dispatch gave the
      original for free.
    - `p->toString()` inside `toString()` confirmed to be the `Json11Serializable` default
      (`to_json().dump()`, `Workstep` never overrode it) → `workstep::to_json(p.get()).dump()`.
    - `ws->type() == "NDemandFertilization"` in `allDynamicWorkstepsFinished` →
      `workstep::type(wsp.get()) == WorkstepType::N_DEMAND_FERTILIZATION`.
    **`addApplication<T>` was dropped entirely, not ported at all** — the 2026-08-10 research pass (and a
    follow-up grep confirmed at step 12's writeup) found the generic template itself, and both of its
    explicit specializations, have **zero callers anywhere in the whole repo**, not even inside
    `cultivation-method.cpp`. Porting a provably-dead template would just be carrying forward unused
    code; skipped, matching how other confirmed-dead constructs were handled earlier in this plan (e.g.
    the commented-out reader-based constructors in items 2/4).
    **Every trivial getter/setter became direct field access, no functions written**: `name()`,
    `unfinishedDynamicWorksteps()`, `getWorksteps()`, `clearWorksteps()`, `setCustomId(int)`,
    `customId()`, `canBeSkipped()`, `isCoverCrop()`, `repeat()` all just returned/set a single field with
    no other logic — future call sites (step 18) use `cm->name`, `cm->unfinishedDynamicWorksteps`,
    `cm->allWorksteps`, `cm->allWorksteps.clear()`, `cm->customId = ...`, `cm->customId`,
    `cm->canBeSkipped`, `cm->isCoverCrop`, `cm->repeat` directly.
    **`CultivationMethodV2` has no `errors` field** (unlike `WorkstepV2`) — confirmed the original
    `CultivationMethod(json11::Json)` constructor calls `merge(j)` and discards the returned `Errors`
    entirely (no `_errors.append(...)` the way every `Workstep` subtype's JSON constructor did) — so
    `makeCultivationMethodV2` does the same, calling `cultivationmethod::merge(&cm, j)` and dropping the
    result, matching the original exactly. Also confirmed (matching item 9's `MeasuredGroundwaterTableInformation`
    precedent) that `CultivationMethod::merge` never called `Json11Serializable::merge(j)` in the
    original either, so `cultivationmethod::merge` has no `defaultMerge` wrap.
    Builds clean on the first attempt. **Build-only validation** (still nothing external wired in) — this
    completes phase 3; only phase 4 (final cutover, step 18) remains.

### Phase 4 — final cutover (single, largest, highest-risk step — the only one with a real regression check)

18. [ ] **Before touching anything**, re-run a fresh repo-wide grep for every external touch point (the
    2026-08-10 research pass results are recorded below as a starting point, but re-verify — the
    codebase may have moved on by the time you get here). Then, in one coherent pass:
    - Fix every external call site (see "External usage notes" below for the as-of-2026-08-10 inventory)
      to use the new `Workstep`/`WorkstepType`/`workstep::...`/`CultivationMethodV2`/
      `cultivationmethod::...` API instead of the old classes/virtual calls/`dynamic_cast`.
    - Delete the old `class Workstep` + all 14 old subclasses + old `monica::makeWorkstep` + old
      `class CultivationMethod` + the two old `addApplication<>` explicit specializations from
      `cultivation-method.h`/`.cpp`.
    - Rename `CultivationMethodV2` -> `CultivationMethod`, `makeWorkstepV2` -> `makeWorkstep`,
      `addWorkstep`/whatever `addApplication` became -> finalize naming.
    - Move the `CultivationMethod` struct + `cultivationmethod::...` free functions out of
      `workstep.cpp`/`.h` into `cultivation-method.cpp`/`.h` (which `#include "workstep.h"`), so the file
      split matches the two concerns cleanly (Workstep union in `workstep.h`, CultivationMethod in
      `cultivation-method.h`) — this is the point where `cultivation-method.h`/`.cpp` stop containing any
      `Workstep`-related code at all.
    - Double-check the fresh grep still shows no `.clone()`/`addApplication<T>` callers before relying on
      `Workstep` being move-only (decision #8) — if something new has started calling either since
      2026-08-10, resolve then; unlikely but cheap to re-check.
    - Build (`cmake --build build --parallel`), then run the **"Run monica-run"** task and diff
      `sim-min-out_section_crop.csv` / `sim-min-out_section_daily.csv` against
      `sim-min-out_section_crop_3.6.60.csv` / `sim-min-out_section_daily_3.6.60.csv` — must be
      byte-identical (same validation baseline as `plan-monica-parameters.md`). This is the **first**
      point in this whole effort where a regression run is meaningful, since phases 0-3 are all inert/
      unreachable new code sitting next to the still-fully-live old implementation.
    - Commit + push.

## External usage notes (research pass 2026-08-10, spot-checked by hand — re-verify at step 18, the
codebase may have moved on by then, but this is a solid, checked starting map)

**Dropping the `Tools::Json11Serializable` inheritance is confirmed safe**: repo-wide grep found zero
`dynamic_cast`/polymorphic use of `Workstep`/`CultivationMethod` through a bare
`Tools::Json11Serializable*`/`&` anywhere, and the only 3 `dynamic_cast<Sowing*/Harvest*>(...)` sites in
the whole repo are inside `cultivation-method.cpp` itself (`CultivationMethod::merge`,
`CultivationMethod::absLatestSowingDate` — already accounted for in phase 3 above).

**Correction to an initial agent finding**: `src/run/serve-monica-zmq.cpp`'s block that constructs
`Sowing`/`Harvest`/`Cutting`/`MineralFertilization`/`OrganicFertilization`/`Tillage`/`Irrigation`
directly and calls `.apply(`/`seed.crop()`/`h.cropResult()` (roughly lines 298-330) is **entirely inside
a `/* ... */` comment block spanning lines 191-396** (verified directly, 2026-08-10) — it is dead,
non-compiled code, not a real external touch point. **No action needed in `serve-monica-zmq.cpp` for
this refactor.** (The file does still use `CultivationMethod`/`Workstep` transitively through
`monica-model.h`/`run-monica.h`, but has no direct call sites of its own.)

### Files that need real edits at step 18 (final cutover)

**`src/core/monica-model.h` / `src/core/monica-model.cpp`** — nested-type rename only (see design
decision #12): `harvestCurrentCrop(MonicaModel*, bool, const Harvest::Spec&, Harvest::OptCarbonManagementData
= Harvest::OptCarbonManagementData(), int)` -> same signature with `Harvest::` replaced by `HarvestData::`
throughout (declaration in `monica-model.h`, definition + body in `monica-model.cpp`, including the
`optCarbMgmtData.cropUsage == Harvest::greenManure` comparison around `monica-model.cpp:601`). Both files
already `#include` (transitively) `cultivation-method.h` -> `workstep.h`; no new include needed. Spot
verified 2026-08-10: `monica-model.h:37` already `#include "../run/cultivation-method.h"`.

**`src/core/crop-module.h` / `src/core/crop-module.cpp`** — same kind of rename:
`applyCutting(CropModule*, std::map<int, Cutting::Value>&, ...)` -> `std::map<int, CuttingData::Value>&`
(declaration `crop-module.h:503`, definition `crop-module.cpp:5510-5511`, body uses at `~5541`/`~5550`).
Spot verified 2026-08-10: `crop-module.h:39` already `#include "run/cultivation-method.h"`.

**`src/run/daily-monica-fbp-component-main.cpp`** (spot-checked live, not commented-out):
- `~L203-209` (`finalizeMonica`): direct `SaveMonicaState sms(currentDate, ...4 args...); sms.apply(monica.get());`
  -> becomes `Workstep ws = makeSaveMonicaStateWorkstep(currentDate, ...); apply(&ws, monica.get());`
  or equivalent using the new factory + dispatcher.
- `~L467-469`: `Harvest::Spec spec; monicamodel::harvestCurrentCrop(monica.get(), hp.getExported(), spec);`
  -> `HarvestData::Spec spec; ...` (once monica-model.h's signature is updated above).
- `~L541`: `std::map<int, Cutting::Value> organId2cuttingSpec;` -> `CuttingData::Value`.
- `~L566-587`: uses `Cutting::none`/`Cutting::cut`/`Cutting::left`/`Cutting::percentage`/`Cutting::biomass`/
  `Cutting::LAI` enumerators -> `CuttingData::none` etc. (or however the nested-enum names end up spelled
  once `CuttingData` exists — check at the time).

**`src/run/run-monica.h`** — `struct CropRotation { ...; std::vector<CultivationMethod> cropRotation; };`,
`makeCropRotation(...)`, and `struct Env`'s `cropRotation`/`cropRotation2` fields hold `CultivationMethod`
*by value* in a `std::vector` — plain field, unaffected by the struct conversion itself (no pointer-
stability concern the way `HarvestData::sowing` has, since nothing takes a raw pointer into a
`CultivationMethod`'s *own* internals across a `vector<CultivationMethod>` resize... except: `run-monica.cpp`
builds `vector<CultivationMethod*> cropRotation/cropRotation2` pointing at elements of `Env::cropRotation`/
`cropRotation2` — that's a pointer-into-a-vector-of-CultivationMethod pattern, same reallocation caveat as
usual `std::vector` pointer invalidation, but this is **pre-existing behavior unrelated to this
refactor** (already true today with the old class) — not something this conversion changes or needs to
fix, just preserve as-is.

**`src/run/run-monica.cpp`** — the heaviest external consumer, needs the most careful pass:
- `~L55-64`: a generic `template<typename Vector> Errors extractAndStore(const Json&, Vector&)` (distinct
  from — but same pattern as — the one already deleted from `output.cpp` during the `OId`/`Output`
  conversion) does `v.merge(cmj)` as a **member call**, instantiated with `Vector::value_type ==
  CultivationMethod` at `~L95` (`crop_rotation_merge`), `~L128`/`~L131` (`env_merge`). Once
  `CultivationMethod::merge` is gone, replace these 3 instantiation sites with manual loops calling
  `cultivationmethod::merge(&v, cmj)` (same fix pattern as the `output.cpp` `toVector<OId>`/`toJsonArray`
  leak-forward from the previous conversion) — or keep `extractAndStore` as a template but make it call a
  free `merge(Vector::value_type*, Json)` unqualified (ADL) instead of `.merge()` as a member, if that
  reads cleaner; your call at the time.
- Explicit `.to_json(` calls (not implicit-conversion, all explicit member calls) at `~L103`
  (`crop_rotation_to_json`), `~L162`/`~L165`/`~L171`/`~L181` (`env_to_json`, including nested loops over
  `cropRotations`/`cropRotations2`) -> `cultivationmethod::to_json(&c)`.
- `.toString(` at `~L221-222` -> `cultivationmethod::to_json(&cm).dump()` (CultivationMethod never
  overrode `toString()` either, same leak-forward pattern as `Workstep`'s `p->toString()` in
  `CultivationMethod::toString()` itself, see phase 3 above) — **or** check if `CultivationMethod::toString()`
  itself (once ported) is being called here instead of `.dump()`-of-`to_json` — re-check the exact
  original semantics (`Json11Serializable::toString()` default vs `CultivationMethod`'s own `toString()`
  which DOES exist and returns a human-readable "name/worksteps" dump, not JSON!) before assuming — this
  particular call site (`s << cm.toString() << endl;`) most likely wants `cultivationmethod::toString(&cm)`
  (the human-readable one, which *does* get ported per phase 3), not the JSON dump. Don't conflate the
  two `toString`s (`CultivationMethod::toString()` is real/non-trivial; `Workstep::toString()` was never
  overridden and fell back to the JSON-dump default) — re-verify which one each call site wants.
- Polymorphic `.registerDailyFunction(` calls at `~L741-744` and `~L758-761` (looping over
  `cm.getWorksteps()`) -> `workstep::registerDailyFunction(wsptr.get(), ...)` (phase 2's dispatcher,
  `default:` no-op matching the base's default empty-`std::function` return, explicit case only for
  `AUTOMATIC_SOWING`).
- Long list of plain `CultivationMethod` method calls needing mechanical `cultivationmethod::` rewiring
  (no dispatch/leak-forward subtlety, just member-call -> free-function-call): `.apply(` (`~L1007,1009,
  1114,1116`), `.absApply(` (`~L1015,1027`), `.nextAbsDate(` (`~L1018,1030`), `.reinit(` (`~L863,873`),
  `.isCoverCrop(` (`~L867,880`), `.absLatestSowingDate(` (`~L871,881`), `.canBeSkipped(` (`~L877,882`),
  `.absStartDate(` (`~L883,893`), `.staticWorksteps(` (`~L891`), `.areOnlyAbsoluteWorksteps(` (`~L845`),
  `.repeat(` (`~L845`), `.allDynamicWorkstepsFinished(` (`~L1129,1136`), `.getWorksteps(` (`~L740,757`).
- `vector<CultivationMethod*> cropRotation/cropRotation2` + `CultivationMethod *currentCM{nullptr}`/
  `currentCM2` raw-pointer aliasing (`~L786,804,834,845,858,891,919-920`) — stays fine as-is, plain
  pointer-to-struct, not polymorphic, no change needed beyond whatever the type becomes.

**`src/run/daily-monica-fbp-component-main.cpp`** (again, separately from the `SaveMonicaState`/`Harvest::Spec`
notes above) uses `Env`/`CultivationMethod` only via `run-monica.h`'s `Env` struct machinery — no direct
`CultivationMethod` method calls of its own found; covered by the `run-monica.*` fixes above.

### Direct-construction sites found (all except `serve-monica-zmq.cpp`'s dead comment block are real)

- `SaveMonicaState(Date, string, bool, int)` — `daily-monica-fbp-component-main.cpp:~203`,
  `run-monica.cpp:~1146` (a 3rd, `env2`-based call is commented out at `run-monica.cpp:~1153-1156`, dead,
  ignore).
- No other subtype (`Sowing`, `AutomaticSowing`, `Transplant`, `Harvest`, `AutomaticHarvest`, `Cutting`,
  `MineralFertilization`, `NDemandFertilization`, `OrganicFertilization`, `Tillage`, `Irrigation`,
  `AutomaticIrrigation`, `SetValue`) is constructed directly anywhere outside `cultivation-method.cpp`
  itself (the real, live JSON-driven path is exclusively through `CultivationMethod::merge` ->
  `makeWorkstep(json)`).
- `CultivationMethod` itself: only its own `CultivationMethod(json11::Json)` constructor
  (`cultivation-method.cpp:1554`); no direct external construction found either.

### Informational only (not C++ call sites, no action needed)

`src/python/monica-ini-to-json/monica-ini-to-json.py` builds workstep JSON dicts with `"type"` keys
(`"OrganicFertilization"`, `"MineralFertilization"`, `"Irrigation"`, `"Sowing"`, `"Harvest"`, `"Tillage"`)
— must keep matching whatever `workstep::typeName(...)` emits (decision #10 already guarantees this,
since the strings are kept byte-identical to today's).
