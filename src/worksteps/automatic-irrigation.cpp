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

#include "automatic-irrigation.h"

#include <algorithm>

#include "../core/monica-model.h"
#include "../run/workstep.h"
#include "json11/json11-helper.h"

using namespace std;
using namespace monica;
using namespace Tools;

Workstep monica::makeAutomaticIrrigationWorkstep(json11::Json j) {
  Workstep ws;
  ws.data = AutomaticIrrigationData{};
  Errors res = workstep::mergeCommon(&ws, j);
  res.append(workstep::merge(&std::get<AutomaticIrrigationData>(ws.data), j));
  ws.errors = res;
  return ws;
}

Errors workstep::merge(AutomaticIrrigationData *ai, json11::Json j) {
  Errors res;

  set_int_value(ai->startStage, j, "startStage");
  // 1-based stages (user side) -> 0-based stages (model side)
  if (ai->startStage > -1)
    ai->startStage = std::max(0, ai->startStage - 1);

  set_int_value(ai->endStage, j, "endStage");
  // 1-based stages (user side) -> 0-based stages (model side)
  if (ai->endStage > -1)
    ai->endStage = std::min(7, ai->endStage - 1);

  set_bool_value(ai->irrigateWhenCrop, j, "irrigateWhenCrop");
  set_bool_value(ai->irrigateWhenCrop, j, "irrigateCrop"); // keep old name for compat reasons
  if (ai->startStage > -1 || ai->endStage > -1)
    ai->irrigateWhenCrop = true;

  if (j["parameters"].is_object()) {
    automaticirrigationparameters::merge(&ai->params, j["parameters"]);
  }

  if (!ai->irrigateWhenCrop && !ai->params.endDate.isValid()) {
    res.appendWarning(
        "Warning: AutomaticIrrigation has not set endDate and "
        "irrigateWhenCrop=false, so the automatic workstep won't be able to finish and only the "
        "first iteration of the cultivation method where this workstep appears will run!");
  }

  return res;
}

json11::Json workstep::to_json(const AutomaticIrrigationData *ai) {
  auto o =
      json11::Json::object{{"type", "AutomaticIrrigation"},
                           {"irrigateWhenCrop", ai->irrigateWhenCrop},
                           {"parameters", automaticirrigationparameters::to_json(&ai->params)}};
  if (ai->startStage > -1)
    o["startStage"] = ai->startStage + 1;
  if (ai->endStage > -1)
    o["endStage"] = ai->endStage + 1;
  return o;
}

bool workstep::apply(AutomaticIrrigationData *ai, MonicaModel *model) {
  if (ai->done) {
    return true;
  }

  auto irrigationTriggered = false;
  auto irrigationAmount = 0.0;
  tie(irrigationTriggered, irrigationAmount) =
      soilcolumn::applyIrrigationViaTrigger(model->soilColumn.get(), ai->params);
  if (irrigationTriggered) {
    model->currentEvents.insert("AutomaticIrrigation");
    model->soilOrganic->irrigationAmount += irrigationAmount;
    monicamodel::addDailySumIrrigationWater(model, irrigationAmount);
  }

  return false;
}

bool workstep::condition(AutomaticIrrigationData *ai, MonicaModel *model) {
  if (ai->done)
    return false;

  // meet the correct date range
  auto dateConditionMet = true;
  const auto date = model->currentStepDate;
  if (ai->absStartDate.isValid() && ai->absEndDate.isValid()) {
    dateConditionMet = date >= ai->absStartDate && date <= ai->absEndDate;
    if (date > ai->absEndDate) {
      ai->done = true;
    }
  } else if (ai->absStartDate.isValid()) {
    dateConditionMet = date >= ai->absStartDate;
  } else if (ai->absEndDate.isValid()) {
    dateConditionMet = date <= ai->absEndDate;
    if (date > ai->absEndDate)
      ai->done = true;
  }
  if (!dateConditionMet)
    return ai->done;
  if (!ai->irrigateWhenCrop)
    return true; // dates only; done is set via endDate above

  // meet the correct crop stage
  auto cropConditionMet = dateConditionMet;
  if (const auto *cg = model->currentCropModule.get(); cg && ai->irrigateWhenCrop) {
    ai->cropPlanted = true;
    const auto stage = cg->vc_DevelopmentalStage;
    if (ai->startStage > -1 && ai->endStage > -1) {
      cropConditionMet = stage >= ai->startStage && stage <= ai->endStage;
      if (stage > ai->endStage)
        ai->done = true;
    } else if (ai->startStage > -1) {
      cropConditionMet = stage >= ai->startStage;
    } else if (ai->endStage > -1) {
      cropConditionMet = stage <= ai->endStage;
      if (stage > ai->endStage)
        ai->done = true;
    }
  } else if (ai->cropPlanted) {
    ai->done = true;
    ai->cropPlanted = false;
    cropConditionMet = false;
  } else {
    cropConditionMet = false;
  }
  return ai->done || cropConditionMet;
}

bool workstep::reinit(AutomaticIrrigationData *ai, Workstep *ws, Tools::Date date, bool addYear,
                      bool forceInitYear) {
  workstep::reinitCommon(ws, date, addYear);
  ws->date = Tools::Date();

  bool startAddedYear, stopAddedYear;
  tie(ai->absStartDate, startAddedYear) =
      workstep::makeInitAbsDate(ai->params.startDate, date, addYear, forceInitYear);
  tie(ai->absEndDate, stopAddedYear) =
      workstep::makeInitAbsDate(ai->params.endDate, date, addYear, forceInitYear);
  ai->done = false;

  return startAddedYear;
}
