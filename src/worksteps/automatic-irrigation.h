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

#include "json11/json11.hpp"

#include "../core/monica-parameters.h"
#include "common/dll-exports.h"
#include "json11/json11-helper.h"
#include "tools/date.h"

namespace monica {
class MonicaModel;
struct Workstep;

struct DLL_API AutomaticIrrigationData {
  Tools::Date absStartDate;
  Tools::Date absEndDate;
  bool irrigateCrop{false};
  int startStage{-1};
  int endStage{-1};
  AutomaticIrrigationParameters params;
  bool done{false};
  bool cropPlanted{false};
};

namespace workstep {

DLL_API Tools::Errors merge(AutomaticIrrigationData *ai, json11::Json j);
DLL_API json11::Json to_json(const AutomaticIrrigationData *ai);
DLL_API bool apply(AutomaticIrrigationData *ai, MonicaModel *model);
DLL_API bool condition(AutomaticIrrigationData *ai, MonicaModel *model);
DLL_API bool reinit(AutomaticIrrigationData *ai, Workstep *ws, Tools::Date date,
                    bool addYear = false, bool forceInitYear = false);

} // namespace workstep

DLL_API Workstep makeAutomaticIrrigationWorkstep(json11::Json object);

} // namespace monica
