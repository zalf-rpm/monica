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

#include "common/dll-exports.h"
#include "sowing.h"
#include "tools/date.h"

namespace monica {
class MonicaModel;
struct Workstep;

struct DLL_API AutomaticSowingData : SowingData {
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

DLL_API Tools::Errors merge(AutomaticSowingData *as, json11::Json j);
DLL_API json11::Json to_json(const AutomaticSowingData *as, const Workstep *ws,
                             bool includeFullCropParameters = true);
DLL_API bool apply(AutomaticSowingData *as, Workstep *ws, MonicaModel *model);
DLL_API bool condition(AutomaticSowingData *as, MonicaModel *model);
DLL_API bool reinit(AutomaticSowingData *as, Workstep *ws, Tools::Date date,
                    bool addYear = false, bool forceInitYear = false);
DLL_API std::function<double(MonicaModel *)>
registerDailyFunction(AutomaticSowingData *as,
                      std::function<std::vector<double> &()> getDailyValues);

} // namespace workstep

DLL_API Workstep makeAutomaticSowingWorkstep(json11::Json object);

} // namespace monica
