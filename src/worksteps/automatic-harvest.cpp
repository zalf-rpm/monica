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

#include "automatic-harvest.h"

#include "../core/monica-model.h"
#include "../run/workstep.h"
#include "json11/json11-helper.h"

using namespace std;
using namespace monica;
using namespace Tools;

Workstep monica::makeAutomaticHarvestWorkstep(json11::Json j) {
  Workstep ws;
  ws.data = AutomaticHarvestData{};
  Errors res = workstep::mergeCommon(&ws, j);
  res.append(workstep::merge(&std::get<AutomaticHarvestData>(ws.data), j));
  ws.errors = res;
  return ws;
}

Errors workstep::merge(AutomaticHarvestData *ah, json11::Json j) {
  Errors res = workstep::merge(static_cast<HarvestData *>(ah), j);

  set_iso_date_value(ah->latestDate, j, "latest-date");
  set_double_value(ah->minPercentASW, j, "min-%-asw");
  set_double_value(ah->maxPercentASW, j, "max-%-asw");
  set_double_value(ah->max3dayPrecipSum, j, "max-3d-precip-sum");
  set_double_value(ah->maxCurrentDayPrecipSum, j, "max-curr-day-precip");
  set_string_value(ah->harvestTime, j, "harvest-time");

  return res;
}

json11::Json workstep::to_json(const AutomaticHarvestData *ah, const Workstep *ws,
                               bool includeFullCropParameters) {
  auto o = workstep::to_json(static_cast<const HarvestData *>(ah), ws, includeFullCropParameters)
               .object_items();
  o["type"] = "AutomaticHarvest";
  o["latest-date"] = J11Array{ah->latestDate.toIsoDateString(), "", "latest harvesting date"};
  o["min-%-asw"] =
      J11Array{ah->minPercentASW, "%", "minimal soil-moisture in percent of available soil-water"};
  o["max-%-asw"] =
      J11Array{ah->maxPercentASW, "%", "maximal soil-moisture in percent of available soil-water"};
  o["max-3d-precip-sum"] = J11Array{ah->max3dayPrecipSum, "mm",
                                    "sum of precipitation in the last three "
                                    "days (including current day)"};
  o["max-curr-day-precip"] =
      J11Array{ah->maxCurrentDayPrecipSum, "mm", "max precipitation allowed at current day"};
  o["harvest-time"] = ah->harvestTime;
  return o;
}

bool workstep::apply(AutomaticHarvestData *ah, Workstep *ws, MonicaModel *model) {
  workstep::apply(static_cast<HarvestData *>(ah), ws, model);

  model->currentEvents.insert("AutomaticHarvest");
  ah->cropHarvested = true;

  return true;
}

bool workstep::condition(AutomaticHarvestData *ah, MonicaModel *model) {
  bool conditionMet = false;

  auto *cg = model->currentCropModule.get();
  // got a crop and not yet harvested
  if (cg && !ah->cropHarvested)
    conditionMet =
        model->currentStepDate >= ah->absLatestDate // harvest after or at latest date
        || (ah->harvestTime == "maturity" &&
            cropmodule::maturityReached(model->currentCropModule) // has maturity been reached
            && workstep::isSoilMoistureOk(model, ah->minPercentASW,
                                          ah->maxPercentASW) // check soil moisture
            && workstep::isPrecipitationOk(model->climateData, ah->max3dayPrecipSum,
                                           ah->maxCurrentDayPrecipSum)); // check precipitation

  return conditionMet;
}

bool workstep::reinit(AutomaticHarvestData *ah, Workstep *ws, Tools::Date date, bool addYear,
                      bool forceInitYear) {
  workstep::reinitCommon(ws, date, addYear);

  ah->cropHarvested = false;
  workstep::setDate(ws, Tools::Date());

  bool addedYear;
  tie(ah->absLatestDate, addedYear) =
      workstep::makeInitAbsDate(ah->latestDate, date, addYear, forceInitYear);

  return addedYear;
}
