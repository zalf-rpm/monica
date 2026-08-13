/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
Authors:
Claas Nendel <claas.nendel@zalf.de>
Xenia Specka <xenia.specka@zalf.de>
Michael Berg <michael.berg@zalf.de>

Maintainers:
Currently maintained by the authors.

This file is part of the MONICA model.
Copyright (C) Leibniz Centre for Agricultural Landscape Research (ZALF)
*/

// General Workstep infrastructure: the WorkstepType tag, the WorkstepData
// tagged union, the Workstep struct itself, and the common (former
// base-class)/central-dispatch free functions that switch on the tag. Each
// concrete workstep's payload struct + its own free functions live in their own
// file pair under src/worksteps/ (e.g. src/worksteps/sowing.h/.cpp) - this file
// just #includes all of them to assemble the WorkstepData variant, and holds
// the code that's genuinely shared/dispatching across all of them.

#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "json11/json11.hpp"

#include "climate/climate-common.h"
#include "tools/date.h"

#include "../worksteps/automatic-harvest.h"
#include "../worksteps/automatic-irrigation.h"
#include "../worksteps/automatic-sowing.h"
#include "../worksteps/cutting.h"
#include "../worksteps/harvest.h"
#include "../worksteps/irrigation.h"
#include "../worksteps/mineral-fertilization.h"
#include "../worksteps/n-demand-fertilization.h"
#include "../worksteps/organic-fertilization.h"
#include "../worksteps/save-monica-state.h"
#include "../worksteps/set-value.h"
#include "../worksteps/sowing.h"
#include "../worksteps/tillage.h"
#include "../worksteps/transplant.h"

namespace monica {
class MonicaModel;

// Declaration order must exactly match WorkstepData's alternative order
// (workstep::type(...) below derives the tag via
// static_cast<WorkstepType>(data.index()), no separately stored tag field).
enum class WorkstepType {
  SOWING,
  AUTOMATIC_SOWING,
  TRANSPLANT,
  HARVEST,
  AUTOMATIC_HARVEST,
  CUTTING,
  MINERAL_FERTILIZATION,
  N_DEMAND_FERTILIZATION,
  ORGANIC_FERTILIZATION,
  TILLAGE,
  SET_VALUE,
  SAVE_MONICA_STATE,
  IRRIGATION,
  AUTOMATIC_IRRIGATION
};

using WorkstepData =
    std::variant<SowingData, AutomaticSowingData, TransplantData, HarvestData, AutomaticHarvestData,
                 CuttingData, MineralFertilizationData, NDemandFertilizationData,
                 OrganicFertilizationData, TillageData, SetValueData, SaveMonicaStateData,
                 IrrigationData, AutomaticIrrigationData>;

struct Workstep {
  Tools::Date date;
  Tools::Date absDate;
  int applyNoOfDaysAfterEvent{0};
  std::string afterEvent;
  int daysAfterEventCount{0};
  bool daysAfterEventCountActivated{false};
  bool isActive{true};
  bool runAtStartOfDay{true};
  Tools::Errors errors;

  WorkstepData data;
};

typedef std::shared_ptr<Workstep> WSPtr;

namespace workstep {

inline WorkstepType type(const Workstep *ws) { return static_cast<WorkstepType>(ws->data.index()); }

inline bool isDynamicWorkstep(const Workstep *ws) { return !ws->date.isValid(); }

// Shared helpers used by more than one concrete workstep's .cpp file (each was
// originally a private, anonymous-namespace-scoped helper local to
// workstep.cpp; promoted to declared functions here once splitting into
// src/worksteps/*.cpp meant more than one translation unit needed them).
std::pair<Tools::Date, bool> makeInitAbsDate(Tools::Date date, Tools::Date initDate, bool addYear,
                                             bool forceInitYear = false);
int organIdFromName(const std::string &organName, Tools::Errors &err);
std::string organNameFromId(int organId);
bool isSoilMoistureOk(MonicaModel *model, double minPercentASW, double maxPercentASW);
bool isPrecipitationOk(const std::vector<std::map<Climate::ACD, double>> &climateData,
                       double max3dayPrecipSum, double maxCurrentDayPrecipSum);

// Common (former base-class, non-overridden-by-default) Workstep behavior. Used
// both by the per-type make*Workstep(...) factories (in src/worksteps/*.cpp)
// and by the central dispatchers' default/ fallback cases below for the many
// subtypes that don't override a given piece of behavior.
Tools::Errors mergeCommon(Workstep *ws, json11::Json j);
bool applyCommon(Workstep *ws, MonicaModel *model);
bool conditionCommon(Workstep *ws, MonicaModel *model);
bool reinitCommon(Workstep *ws, Tools::Date date, bool addYear = false, bool forceInitYear = false);
// setDate is inherently per-subtype dispatching (3 of the 14 subtypes override
// it), so unlike merge/apply/condition/reinit there's no single "common" body
// to factor out - this is already the full central dispatcher, not a "Common"
// helper.
void setDate(Workstep *ws, Tools::Date date);

// Central dispatch - switches on type(ws) to reach the right per-payload
// function (declared in each concrete workstep's own header under
// src/worksteps/). Trivial, never-overridden pieces of the original Workstep
// interface (date(), noOfDaysAfterEvent(), afterEvent(), runAtStartOfDay(),
// errors()) are NOT ported as functions here at all - they're just plain field
// reads now (ws->date, ws->applyNoOfDaysAfterEvent, ws->afterEvent,
// ws->runAtStartOfDay, ws->errors), per the usual "trivial one-line accessors
// get inlined and removed" rule. `type()`'s string form (originally `virtual
// std::string type() const`) is also not ported as a general-purpose function -
// it had zero external callers, and its only internal use sites are cleaner as
// direct WorkstepType enum comparisons than as string comparisons.

inline Tools::Date absDate(const Workstep *ws) {
  return ws->date.isAbsoluteDate() ? ws->date : ws->absDate;
}

Tools::Date earliestDate(const Workstep *ws);
Tools::Date absEarliestDate(const Workstep *ws);
Tools::Date latestDate(const Workstep *ws);
Tools::Date absLatestDate(const Workstep *ws);

Tools::Errors merge(Workstep *ws, json11::Json j);
json11::Json to_json(const Workstep *ws, bool includeFullCropParameters = true);
bool isActive(const Workstep *ws);
bool apply(Workstep *ws, MonicaModel *model);
bool applyWithPossibleCondition(Workstep *ws, MonicaModel *model);
bool condition(Workstep *ws, MonicaModel *model);
bool reinit(Workstep *ws, Tools::Date date, bool addYear = false, bool forceInitYear = false);
std::function<double(MonicaModel *)>
registerDailyFunction(Workstep *ws, std::function<std::vector<double> &()> getDailyValues);

} // namespace workstep

WSPtr makeWorkstep(json11::Json object);

} // namespace monica
