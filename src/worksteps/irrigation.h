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

struct IrrigationData {
  double amount{0};
  IrrigationParameters params;
};

namespace workstep {

Tools::Errors merge(IrrigationData *i, json11::Json j);
json11::Json to_json(const IrrigationData *i, const Workstep *ws);
bool apply(IrrigationData *i, Workstep *ws, MonicaModel *model);

} // namespace workstep

Workstep makeIrrigationWorkstep(json11::Json object);
Workstep makeIrrigationWorkstep(const Tools::Date &at, double amount,
                                IrrigationParameters params = IrrigationParameters());

} // namespace monica
