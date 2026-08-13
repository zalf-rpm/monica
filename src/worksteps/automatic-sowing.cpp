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

#include "automatic-sowing.h"

#include <numeric>

#include "../core/monica-model.h"
#include "../run/workstep.h"

using namespace std;
using namespace monica;
using namespace Tools;
using namespace Climate;

namespace {
bool isSoilTemperatureOk(const std::vector<double> &soilTemps, int windowDays,
                         double targetAvgSoilTemp) {
  if (soilTemps.empty())
    return false;

  double sum = 0;
  auto size = soilTemps.size();
  for (int i = int(size - 1); i >= 0 && i >= size - windowDays; i--)
    sum += soilTemps[i];
  double avg = sum / (size > windowDays ? windowDays : size);
  return avg >= targetAvgSoilTemp;
}
} // namespace

Workstep monica::makeAutomaticSowingWorkstep(json11::Json j) {
  Workstep ws;
  ws.data = AutomaticSowingData{};
  Errors res = workstep::mergeCommon(&ws, j);
  res.append(workstep::merge(&std::get<AutomaticSowingData>(ws.data), j));
  ws.errors = res;
  return ws;
}

Errors workstep::merge(AutomaticSowingData *as, json11::Json j) {
  Errors res = workstep::merge(static_cast<SowingData *>(as), j);

  set_iso_date_value(as->earliestDate, j, "earliest-date");
  set_iso_date_value(as->latestDate, j, "latest-date");
  set_double_value(as->minTempThreshold, j, "min-temp");
  set_int_value(as->daysInTempWindow, j, "days-in-temp-window");
  set_double_value(as->minPercentASW, j, "min-%-asw");
  set_double_value(as->maxPercentASW, j, "max-%-asw");
  set_double_value(as->max3dayPrecipSum, j, "max-3d-precip");
  set_double_value(as->maxCurrentDayPrecipSum, j, "max-curr-day-precip");
  set_double_value(as->tempSumAboveBaseTemp, j, "temp-sum-above-base-temp");
  set_double_value(as->baseTemp, j, "base-temp");

  json11::Json avgSoilTemp = j["avg-soil-temp"];
  if (avgSoilTemp.is_object()) {
    set_double_value(as->soilDepthForAveraging, avgSoilTemp, "depth");
    set_int_value(as->daysInSoilTempWindow, avgSoilTemp, "days");
    set_double_value(as->sowingIfAboveAvgSoilTemp, avgSoilTemp, "Tavg");
    as->checkForSoilTemperature = as->soilDepthForAveraging > 0 && as->daysInSoilTempWindow > 0 &&
                                  as->sowingIfAboveAvgSoilTemp > 0;
  }

  return res;
}

json11::Json workstep::to_json(const AutomaticSowingData *as, const Workstep *ws,
                               bool includeFullCropParameters) {
  auto o = workstep::to_json(static_cast<const SowingData *>(as), ws).object_items();
  o["type"] = "AutomaticSowing";
  o["earliest-date"] = J11Array{as->earliestDate.toIsoDateString(), "", "earliest sowing date"};
  o["latest-date"] = J11Array{as->latestDate.toIsoDateString(), "", "latest sowing date"};
  o["min-temp"] = J11Array{as->minTempThreshold,
                           "\xEF\xBF\xBD"
                           "C",
                           "minimal air temperature for sowing (T >= thresh && "
                           "avg T in Twindow >= thresh)"};
  o["days-in-temp-window"] =
      J11Array{as->daysInTempWindow, "d", "days to be used for sliding window of min-temp"};
  o["min-%-asw"] =
      J11Array{as->minPercentASW, "%", "minimal soil-moisture in percent of available soil-water"};
  o["max-%-asw"] =
      J11Array{as->maxPercentASW, "%", "maximal soil-moisture in percent of available soil-water"};
  o["max-3d-precip-sum"] =
      J11Array{as->max3dayPrecipSum, "mm",
               "sum of precipitation in the last three days (including current day)"};
  o["max-curr-day-precip"] =
      J11Array{as->maxCurrentDayPrecipSum, "mm", "max precipitation allowed at current day"};
  o["temp-sum-above-base-temp"] = J11Array{as->tempSumAboveBaseTemp,
                                           "\xEF\xBF\xBD"
                                           "C",
                                           "temperature sum above T-base needed"};
  o["base-temp"] = J11Array{as->baseTemp,
                            "\xEF\xBF\xBD"
                            "C",
                            "base temperature above which temp-sum-above-base-temp is counted"};
  o["avg-soil-temp"] =
      J11Object{{"depth", J11Array{as->soilDepthForAveraging, "m",
                                   "soil depth until averaging will be done"}},
                {"days", J11Array{as->daysInSoilTempWindow, "d",
                                  "window/number of days for which the average "
                                  "temperature must be greater"}},
                {"Tavg", J11Array{as->sowingIfAboveAvgSoilTemp,
                                  "\xEF\xBF\xBD"
                                  "C",
                                  "temperature which has to be reached on average"}}};

  return o;
}

bool workstep::apply(AutomaticSowingData *as, Workstep *ws, MonicaModel *model) {
  auto currentDate = model->currentStepDate;

  as->sowingDate = currentDate;

  workstep::apply(static_cast<SowingData *>(as), ws, model);
  model->currentEvents.insert("AutomaticSowing");
  as->cropSeeded = true;
  as->inSowingRange = false;

  return true;
}

std::function<double(MonicaModel *)>
workstep::registerDailyFunction(AutomaticSowingData *as,
                                std::function<std::vector<double> &()> getDailyValues) {
  if (!as->checkForSoilTemperature)
    return std::function<double(MonicaModel *)>();

  as->getAvgSoilTemps = getDailyValues;
  return [as](MonicaModel *model) -> double {
    double avgSoilTemp = 0;
    size_t i = 0;
    for (auto size = soilcolumn::getLayerNumberForDepth(model->soilColumn.get(),
                                                        as->soilDepthForAveraging) +
                     1;
         i < size; i++) {
      avgSoilTemp += model->soilTemperature->soilColumn->layers.at(int(i)).vs_SoilTemperature;
    }
    return avgSoilTemp / double(i);
  };
}

bool workstep::condition(AutomaticSowingData *as, MonicaModel *model) {
  if (as->cropSeeded)
    return false;

  auto currentDate = model->currentStepDate;

  if (!as->inSowingRange && currentDate < as->absEarliestDate)
    return false;
  else
    as->inSowingRange = true;

  if (as->inSowingRange && currentDate >= as->absLatestDate)
    return true;

  // check soil temperature if requested
  if (as->checkForSoilTemperature) {
    if (!isSoilTemperatureOk(as->getAvgSoilTemps(), as->daysInSoilTempWindow,
                             as->sowingIfAboveAvgSoilTemp))
      return false;
  }

  const auto &cd = model->climateData;
  auto currentCd = cd.back();

  auto avg = [&](Climate::ACD acd) {
    return accumulate(cd.rbegin(), cd.rbegin() + std::min(int(cd.size()), as->daysInTempWindow),
                      0.0,
                      [acd](double acc, const map<ACD, double> &d) {
                        auto it = d.find(acd);
                        return acc + (it == d.end() ? 0 : it->second);
                      }) /
           min(int(cd.size()), as->daysInTempWindow);
  };

  // check temperature
  bool Tok = false;
  if (as->cropParams.cultivarParams.winterCrop) {
    double avgTavg = avg(Climate::tavg);
    Tok = avgTavg <= as->minTempThreshold;
  } else {
    double avgTmin = avg(Climate::tmin);
    bool avgTminOk = avgTmin >= as->minTempThreshold;
    bool TminOk = currentCd[Climate::tmin] >= as->minTempThreshold;
    Tok = avgTminOk && TminOk;
  }

  if (!Tok)
    return false;

  // check soil moisture
  if (!workstep::isSoilMoistureOk(model, as->minPercentASW, as->maxPercentASW))
    return false;

  // check precipitation
  if (!workstep::isPrecipitationOk(cd, as->max3dayPrecipSum, as->maxCurrentDayPrecipSum))
    return false;

  // check temperature sum
  double baseTemp = as->baseTemp;
  double tempSum =
      accumulate(cd.begin(), cd.end(), 0.0, [baseTemp](double acc, const map<ACD, double> &d) {
        auto it = d.find(Climate::tavg);
        return acc + (it == d.end() ? 0 : max(0.0, it->second - baseTemp));
      });
  if (tempSum < as->tempSumAboveBaseTemp)
    return false;

  return true;
}

bool workstep::reinit(AutomaticSowingData *as, Workstep *ws, Tools::Date date, bool addYear,
                      bool forceInitYear) {
  workstep::reinitCommon(ws, date, addYear);

  as->cropSeeded = as->inSowingRange = false;
  workstep::setDate(ws, Tools::Date());

  bool addedYear1, addedYear2;
  // init first the latest date, if the latest date stays in current year, so
  // has to stay the earliest date (thus force current year) if there is a
  // forced current (init) year, this will force both dates to this year
  tie(as->absLatestDate, addedYear1) =
      workstep::makeInitAbsDate(as->latestDate, date, addYear, forceInitYear);
  tie(as->absEarliestDate, addedYear2) =
      workstep::makeInitAbsDate(as->earliestDate, date, addYear, forceInitYear || !addedYear1);

  return addedYear1; // || addedYear2;
}
