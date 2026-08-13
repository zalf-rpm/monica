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

#pragma once

#include <functional>
#include <vector>

#include "json11/json11.hpp"

#include "sowing.h"
#include "tools/date.h"

namespace monica {
class MonicaModel;
struct Workstep;

struct AutomaticSowingData : SowingData {
  Tools::Date absEarliestDate;
  Tools::Date earliestDate;
  Tools::Date latestDate;
  Tools::Date absLatestDate;
  double minTempThreshold{0};
  int daysInTempWindow{0};
  double minPercentASW{0};
  double maxPercentASW{100};
  double max3dayPrecipSum{0};
  double maxCurrentDayPrecipSum{0};
  double tempSumAboveBaseTemp{0};
  double baseTemp{0};

  bool checkForSoilTemperature{false};
  double soilDepthForAveraging{0.30}; //= 30 cm
  int daysInSoilTempWindow{0};
  double sowingIfAboveAvgSoilTemp{0};
  std::function<std::vector<double> &()> getAvgSoilTemps;

  bool inSowingRange{false};
  bool cropSeeded{false};
};

namespace workstep {

Tools::Errors merge(AutomaticSowingData *as, json11::Json j);
json11::Json to_json(const AutomaticSowingData *as, const Workstep *ws,
                     bool includeFullCropParameters = true);
bool apply(AutomaticSowingData *as, Workstep *ws, MonicaModel *model);
bool condition(AutomaticSowingData *as, MonicaModel *model);
bool reinit(AutomaticSowingData *as, Workstep *ws, Tools::Date date, bool addYear = false,
            bool forceInitYear = false);
std::function<double(MonicaModel *)>
registerDailyFunction(AutomaticSowingData *as,
                      std::function<std::vector<double> &()> getDailyValues);

} // namespace workstep

Workstep makeAutomaticSowingWorkstep(json11::Json object);

} // namespace monica
