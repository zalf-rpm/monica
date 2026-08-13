/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
Authors:
Michael Berg <michael.berg@zalf.de>
Claas Nendel <claas.nendel@zalf.de>
Xenia Specka <xenia.specka@zalf.de>

Maintainers:
Currently maintained by the authors.

This file is part of the MONICA model.
Copyright (C) Leibniz Centre for Agricultural Landscape Research (ZALF)
*/

#include "workstep.h"

#include <numeric>
#include <utility>

#include "../core/monica-model.h"
#include "json11/json11-helper.h"

using namespace std;
using namespace monica;
using namespace Tools;
using namespace Climate;

std::pair<Date, bool> workstep::makeInitAbsDate(Date date, Date initDate, bool addYear,
                                                bool forceInitYear) {
  bool addedYear = false;

  if (date.isAbsoluteDate())
    return make_pair(date, addedYear);

  Date absDate = date.toAbsoluteDate(initDate.year());
  if (!forceInitYear && (addYear || (absDate < initDate))) {
    addedYear = true;
    absDate.addYears(1);
  }

  return make_pair(absDate, addedYear);
}

int workstep::organIdFromName(const string &organName, Tools::Errors &err) {
  string os = toLower(organName);
  if (os == "root")
    return 0;
  else if (os == "leaf")
    return 1;
  else if (os == "shoot")
    return 2;
  else if (os == "fruit")
    return 3;
  else if (os == "struct")
    return 4;
  else if (os == "sugar")
    return 5;
  err.append(Errors(Errors::ERR, "organ id could not be resolved"));
  return -1; // default error
}

string workstep::organNameFromId(int organId) {
  string res = "unknown";
  switch (organId) {
  case 0:
    res = "Root";
    break;
  case 1:
    res = "Leaf";
    break;
  case 2:
    res = "Shoot";
    break;
  case 3:
    res = "Fruit";
    break;
  case 4:
    res = "Struct";
    break;
  case 5:
    res = "Sugar";
    break;
  default:
    res = "unknown";
  }
  return res;
}

bool workstep::isSoilMoistureOk(MonicaModel *model, double minPercentASW, double maxPercentASW) {
  bool soilMoistureOk = false;
  double pwp = model->soilColumn->layers.at(0).vs_PermanentWiltingPoint;
  double sm = max(0.0, model->soilColumn->layers.at(0).vs_SoilMoisture_m3 - pwp);
  double asw = model->soilColumn->layers.at(0).vs_FieldCapacity - pwp;
  double currentPercentASW = sm / asw * 100.0;
  soilMoistureOk = minPercentASW <= currentPercentASW && currentPercentASW <= maxPercentASW;

  return soilMoistureOk;
}

bool workstep::isPrecipitationOk(const std::vector<std::map<Climate::ACD, double>> &climateData,
                                 double max3dayPrecipSum, double maxCurrentDayPrecipSum) {
  bool precipOk = false;
  double psum3d = std::accumulate(climateData.rbegin(), climateData.rbegin() + 3, 0.0,
                                  [](double acc, const map<ACD, double> &d) {
                                    auto it = d.find(Climate::precip);
                                    return acc + (it == d.end() ? 0 : it->second);
                                  });
  double currentp = climateData.back().at(Climate::precip);
  precipOk = psum3d <= max3dayPrecipSum && currentp <= maxCurrentDayPrecipSum;

  return precipOk;
}

Errors workstep::mergeCommon(Workstep *ws, json11::Json j) {
  // The DEFAULT/"=" JSON-unwrap wrap belongs here, not repeated in every
  // per-payload merge(XxxData*,
  // ...): mergeCommon is the "called exactly once, first, for every subtype"
  // entry point in this design, exactly mirroring how the original
  // Workstep::merge (which every subtype's merge() always chained to, directly
  // or transitively) was the sole place Json11Serializable::merge(j) got called
  // per external invocation, however many levels of subtype-chaining happened
  // above it.
  Errors res = defaultMerge(j, [ws](json11::Json j2) { return mergeCommon(ws, j2); });

  set_iso_date_value(ws->date, j, "date");
  // at is a shortcut for after=event and days=1
  auto at = string_value(j, "at");
  if (!at.empty()) {
    ws->afterEvent = at;
    ws->applyNoOfDaysAfterEvent = 1;
  }
  set_int_value(ws->applyNoOfDaysAfterEvent, j, "days");
  set_string_value(ws->afterEvent, j, "after");
  set_bool_value(ws->runAtStartOfDay, j, "runAtStartOfDay");

  return res;
}

bool workstep::applyCommon(Workstep *ws, MonicaModel *model) {
  model->currentEvents.insert("Workstep");
  return true;
}

bool workstep::conditionCommon(Workstep *ws, MonicaModel *model) {
  if (ws->afterEvent.empty() || ws->applyNoOfDaysAfterEvent <= 0) {
    return false;
  }

  const auto &currEvents = model->currentEvents;
  const auto &prevEvents = model->previousDaysEvents;

  auto ceit = currEvents.find(ws->afterEvent);
  if (ws->daysAfterEventCountActivated) {
    ws->daysAfterEventCount++;
  } else if (ceit != currEvents.end() || prevEvents.find(ws->afterEvent) != prevEvents.end()) {
    ws->daysAfterEventCountActivated = true;
  }

  return ws->daysAfterEventCount == ws->applyNoOfDaysAfterEvent;
}

bool workstep::reinitCommon(Workstep *ws, Tools::Date date, bool addYear, bool forceInitYear) {
  bool addedYear = false;

  if (ws->date.isValid()) {
    tie(ws->absDate, addedYear) = makeInitAbsDate(ws->date, date, addYear, forceInitYear);
  } else {
    ws->absDate = Date();
  }

  ws->isActive = true;
  ws->daysAfterEventCount = 0;
  ws->daysAfterEventCountActivated = false;

  return addedYear;
}

// Central dispatch - switches on type(ws) to reach the right per-payload
// function, declared in each concrete workstep's own header under
// src/worksteps/.

Tools::Date workstep::earliestDate(const Workstep *ws) {
  if (type(ws) == WorkstepType::AUTOMATIC_SOWING)
    return std::get<AutomaticSowingData>(ws->data).earliestDate;
  return ws->date;
}

Tools::Date workstep::absEarliestDate(const Workstep *ws) {
  if (type(ws) == WorkstepType::AUTOMATIC_SOWING)
    return std::get<AutomaticSowingData>(ws->data).absEarliestDate;
  return absDate(ws);
}

Tools::Date workstep::latestDate(const Workstep *ws) {
  switch (type(ws)) {
  case WorkstepType::AUTOMATIC_SOWING:
    return std::get<AutomaticSowingData>(ws->data).latestDate;
  case WorkstepType::AUTOMATIC_HARVEST:
    return std::get<AutomaticHarvestData>(ws->data).latestDate;
  default:
    return ws->date;
  }
}

Tools::Date workstep::absLatestDate(const Workstep *ws) {
  switch (type(ws)) {
  case WorkstepType::AUTOMATIC_SOWING:
    return std::get<AutomaticSowingData>(ws->data).absLatestDate;
  case WorkstepType::AUTOMATIC_HARVEST:
    return std::get<AutomaticHarvestData>(ws->data).absLatestDate;
  default:
    return absDate(ws);
  }
}

Errors workstep::merge(Workstep *ws, json11::Json j) {
  // mergeCommon already applies the DEFAULT/"=" unwrap (see its definition
  // above) - not repeated here.
  Errors res = mergeCommon(ws, j);

  switch (type(ws)) {
  case WorkstepType::SOWING:
    res.append(merge(&std::get<SowingData>(ws->data), j));
    break;
  case WorkstepType::AUTOMATIC_SOWING:
    res.append(merge(&std::get<AutomaticSowingData>(ws->data), j));
    break;
  case WorkstepType::TRANSPLANT:
    res.append(merge(&std::get<TransplantData>(ws->data), j));
    break;
  case WorkstepType::HARVEST:
    res.append(merge(&std::get<HarvestData>(ws->data), j));
    break;
  case WorkstepType::AUTOMATIC_HARVEST:
    res.append(merge(&std::get<AutomaticHarvestData>(ws->data), j));
    break;
  case WorkstepType::CUTTING:
    res.append(merge(&std::get<CuttingData>(ws->data), j));
    break;
  case WorkstepType::MINERAL_FERTILIZATION:
    res.append(merge(&std::get<MineralFertilizationData>(ws->data), j));
    break;
  case WorkstepType::N_DEMAND_FERTILIZATION:
    res.append(merge(&std::get<NDemandFertilizationData>(ws->data), ws, j));
    break;
  case WorkstepType::ORGANIC_FERTILIZATION:
    res.append(merge(&std::get<OrganicFertilizationData>(ws->data), j));
    break;
  case WorkstepType::TILLAGE:
    res.append(merge(&std::get<TillageData>(ws->data), j));
    break;
  case WorkstepType::SET_VALUE:
    res.append(merge(&std::get<SetValueData>(ws->data), j));
    break;
  case WorkstepType::SAVE_MONICA_STATE:
    res.append(merge(&std::get<SaveMonicaStateData>(ws->data), ws, j));
    break;
  case WorkstepType::IRRIGATION:
    res.append(merge(&std::get<IrrigationData>(ws->data), j));
    break;
  case WorkstepType::AUTOMATIC_IRRIGATION:
    res.append(merge(&std::get<AutomaticIrrigationData>(ws->data), j));
    break;
  }

  return res;
}

json11::Json workstep::to_json(const Workstep *ws, bool includeFullCropParameters) {
  switch (type(ws)) {
  case WorkstepType::SOWING:
    return to_json(&std::get<SowingData>(ws->data), ws, includeFullCropParameters);
  case WorkstepType::AUTOMATIC_SOWING:
    return to_json(&std::get<AutomaticSowingData>(ws->data), ws, includeFullCropParameters);
  case WorkstepType::TRANSPLANT:
    return to_json(&std::get<TransplantData>(ws->data), ws, includeFullCropParameters);
  case WorkstepType::HARVEST:
    return to_json(&std::get<HarvestData>(ws->data), ws, includeFullCropParameters);
  case WorkstepType::AUTOMATIC_HARVEST:
    return to_json(&std::get<AutomaticHarvestData>(ws->data), ws, includeFullCropParameters);
  case WorkstepType::CUTTING:
    return to_json(&std::get<CuttingData>(ws->data), ws);
  case WorkstepType::MINERAL_FERTILIZATION:
    return to_json(&std::get<MineralFertilizationData>(ws->data), ws);
  case WorkstepType::N_DEMAND_FERTILIZATION:
    return to_json(&std::get<NDemandFertilizationData>(ws->data));
  case WorkstepType::ORGANIC_FERTILIZATION:
    return to_json(&std::get<OrganicFertilizationData>(ws->data), ws);
  case WorkstepType::TILLAGE:
    return to_json(&std::get<TillageData>(ws->data), ws);
  case WorkstepType::SET_VALUE:
    return to_json(&std::get<SetValueData>(ws->data), ws);
  case WorkstepType::SAVE_MONICA_STATE:
    return to_json(&std::get<SaveMonicaStateData>(ws->data), ws);
  case WorkstepType::IRRIGATION:
    return to_json(&std::get<IrrigationData>(ws->data), ws);
  case WorkstepType::AUTOMATIC_IRRIGATION:
    return to_json(&std::get<AutomaticIrrigationData>(ws->data));
  }
  return json11::Json(); // unreachable, all WorkstepType values handled above
}

bool workstep::isActive(const Workstep *ws) {
  switch (type(ws)) {
  case WorkstepType::AUTOMATIC_SOWING:
    return !std::get<AutomaticSowingData>(ws->data).cropSeeded;
  case WorkstepType::AUTOMATIC_HARVEST:
    return !std::get<AutomaticHarvestData>(ws->data).cropHarvested;
  case WorkstepType::N_DEMAND_FERTILIZATION:
    return !std::get<NDemandFertilizationData>(ws->data).appliedFertilizer;
  default:
    return ws->isActive;
  }
}

bool workstep::apply(Workstep *ws, MonicaModel *model) {
  switch (type(ws)) {
  case WorkstepType::SOWING:
    return apply(&std::get<SowingData>(ws->data), ws, model);
  case WorkstepType::AUTOMATIC_SOWING:
    return apply(&std::get<AutomaticSowingData>(ws->data), ws, model);
  case WorkstepType::TRANSPLANT:
    return apply(&std::get<TransplantData>(ws->data), ws, model);
  case WorkstepType::HARVEST:
    return apply(&std::get<HarvestData>(ws->data), ws, model);
  case WorkstepType::AUTOMATIC_HARVEST:
    return apply(&std::get<AutomaticHarvestData>(ws->data), ws, model);
  case WorkstepType::CUTTING:
    return apply(&std::get<CuttingData>(ws->data), ws, model);
  case WorkstepType::MINERAL_FERTILIZATION:
    return apply(&std::get<MineralFertilizationData>(ws->data), ws, model);
  case WorkstepType::N_DEMAND_FERTILIZATION:
    return apply(&std::get<NDemandFertilizationData>(ws->data), ws, model);
  case WorkstepType::ORGANIC_FERTILIZATION:
    return apply(&std::get<OrganicFertilizationData>(ws->data), ws, model);
  case WorkstepType::TILLAGE:
    return apply(&std::get<TillageData>(ws->data), ws, model);
  case WorkstepType::SET_VALUE:
    return apply(&std::get<SetValueData>(ws->data), ws, model);
  case WorkstepType::SAVE_MONICA_STATE:
    return apply(&std::get<SaveMonicaStateData>(ws->data), ws, model);
  case WorkstepType::IRRIGATION:
    return apply(&std::get<IrrigationData>(ws->data), ws, model);
  case WorkstepType::AUTOMATIC_IRRIGATION:
    return apply(&std::get<AutomaticIrrigationData>(ws->data), model);
  }
  return false; // unreachable, all WorkstepType values handled above
}

bool workstep::applyWithPossibleCondition(Workstep *ws, MonicaModel *model) {
  bool workstepFinished = false;
  if (isActive(ws)) {
    if (isDynamicWorkstep(ws))
      workstepFinished = condition(ws, model) ? apply(ws, model) : false;
    else
      workstepFinished = apply(ws, model);
    ws->isActive = !workstepFinished;
  }
  return workstepFinished;
}

bool workstep::condition(Workstep *ws, MonicaModel *model) {
  switch (type(ws)) {
  case WorkstepType::AUTOMATIC_SOWING:
    return condition(&std::get<AutomaticSowingData>(ws->data), model);
  case WorkstepType::AUTOMATIC_HARVEST:
    return condition(&std::get<AutomaticHarvestData>(ws->data), model);
  case WorkstepType::N_DEMAND_FERTILIZATION:
    return condition(&std::get<NDemandFertilizationData>(ws->data), ws, model);
  case WorkstepType::AUTOMATIC_IRRIGATION:
    return condition(&std::get<AutomaticIrrigationData>(ws->data), model);
  default:
    return conditionCommon(ws, model);
  }
}

bool workstep::reinit(Workstep *ws, Tools::Date date, bool addYear, bool forceInitYear) {
  switch (type(ws)) {
  case WorkstepType::AUTOMATIC_SOWING:
    return reinit(&std::get<AutomaticSowingData>(ws->data), ws, date, addYear, forceInitYear);
  case WorkstepType::AUTOMATIC_HARVEST:
    return reinit(&std::get<AutomaticHarvestData>(ws->data), ws, date, addYear, forceInitYear);
  case WorkstepType::N_DEMAND_FERTILIZATION:
    return reinit(&std::get<NDemandFertilizationData>(ws->data), ws, date, addYear, forceInitYear);
  case WorkstepType::AUTOMATIC_IRRIGATION:
    return reinit(&std::get<AutomaticIrrigationData>(ws->data), ws, date, addYear, forceInitYear);
  default:
    return reinitCommon(ws, date, addYear, forceInitYear);
  }
}

std::function<double(MonicaModel *)>
workstep::registerDailyFunction(Workstep *ws,
                                std::function<std::vector<double> &()> getDailyValues) {
  if (type(ws) == WorkstepType::AUTOMATIC_SOWING)
    return registerDailyFunction(&std::get<AutomaticSowingData>(ws->data), getDailyValues);
  return std::function<double(MonicaModel *)>();
}

WSPtr monica::makeWorkstep(json11::Json j) {
  string type = string_value(j["type"]);

  if (type == "Sowing" || type == "Seed") {
    return make_shared<Workstep>(makeSowingWorkstep(j));
  }
  if (type == "Transplant") {
    return make_shared<Workstep>(makeTransplantWorkstep(j));
  }
  if (type == "AutomaticSowing") {
    return make_shared<Workstep>(makeAutomaticSowingWorkstep(j));
  }
  if (type == "Harvest") {
    return make_shared<Workstep>(makeHarvestWorkstep(j));
  }
  if (type == "AutomaticHarvest") {
    return make_shared<Workstep>(makeAutomaticHarvestWorkstep(j));
  }
  if (type == "Cutting") {
    return make_shared<Workstep>(makeCuttingWorkstep(j));
  }
  if (type == "MineralFertilization" || type == "MineralFertiliserApplication") { // deprecated name
    return make_shared<Workstep>(makeMineralFertilizationWorkstep(j));
  }
  if (type == "NDemandFertilization") {
    return make_shared<Workstep>(makeNDemandFertilizationWorkstep(j));
  }
  if (type == "OrganicFertilization" || type == "OrganicFertiliserApplication") { // deprecated name
    return make_shared<Workstep>(makeOrganicFertilizationWorkstep(j));
  }
  if (type == "Tillage" || type == "TillageApplication") { // deprecated name
    return make_shared<Workstep>(makeTillageWorkstep(j));
  }
  if (type == "Irrigation" || type == "IrrigationApplication") { // deprecated name
    return make_shared<Workstep>(makeIrrigationWorkstep(j));
  }
  if (type == "AutomaticIrrigation") {
    return make_shared<Workstep>(makeAutomaticIrrigationWorkstep(j));
  }
  if (type == "SetValue") {
    return make_shared<Workstep>(makeSetValueWorkstep(j));
  }
  if (type == "SaveMonicaState") {
    return make_shared<Workstep>(makeSaveMonicaStateWorkstep(j));
  }

  return {};
}
