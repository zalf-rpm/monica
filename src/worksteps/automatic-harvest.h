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

#include <string>

#include "json11/json11.hpp"

#include "common/dll-exports.h"
#include "harvest.h"
#include "tools/date.h"

namespace monica {
class MonicaModel;
struct Workstep;

struct DLL_API AutomaticHarvestData : HarvestData {
  std::string harvestTime{"maturity"}; //!< Harvest time parameter
  Tools::Date latestDate;
  Tools::Date absLatestDate;
  double minPercentASW{0};
  double maxPercentASW{999};
  double max3dayPrecipSum{9999};
  double maxCurrentDayPrecipSum{9999};
  bool cropHarvested{false};
};

namespace workstep {

DLL_API Tools::Errors merge(AutomaticHarvestData *ah, json11::Json j);
DLL_API json11::Json to_json(const AutomaticHarvestData *ah, const Workstep *ws,
                             bool includeFullCropParameters = true);
DLL_API bool apply(AutomaticHarvestData *ah, Workstep *ws, MonicaModel *model);
DLL_API bool condition(AutomaticHarvestData *ah, MonicaModel *model);
DLL_API bool reinit(AutomaticHarvestData *ah, Workstep *ws, Tools::Date date,
                    bool addYear = false, bool forceInitYear = false);

} // namespace workstep

DLL_API Workstep makeAutomaticHarvestWorkstep(json11::Json object);

} // namespace monica
