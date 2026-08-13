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
#include "tools/date.h"
#include "tools/helper.h"

namespace monica {
class MonicaModel;
struct Workstep;

struct AutomaticIrrigationData {
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

Tools::Errors merge(AutomaticIrrigationData *ai, json11::Json j);
json11::Json to_json(const AutomaticIrrigationData *ai);
bool apply(AutomaticIrrigationData *ai, MonicaModel *model);
bool condition(AutomaticIrrigationData *ai, MonicaModel *model);
bool reinit(AutomaticIrrigationData *ai, Workstep *ws, Tools::Date date, bool addYear = false,
            bool forceInitYear = false);

} // namespace workstep

Workstep makeAutomaticIrrigationWorkstep(json11::Json object);

} // namespace monica
