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

#include "common/dll-exports.h"
#include "json11/json11-helper.h"
#include "tools/date.h"

namespace monica {
class MonicaModel;
struct Workstep;

struct DLL_API TillageData {
  double depth{0.3};
};

namespace workstep {

DLL_API Tools::Errors merge(TillageData *t, json11::Json j);
DLL_API json11::Json to_json(const TillageData *t, const Workstep *ws);
DLL_API bool apply(TillageData *t, Workstep *ws, MonicaModel *model);

} // namespace workstep

DLL_API Workstep makeTillageWorkstep(json11::Json object);
DLL_API Workstep makeTillageWorkstep(const Tools::Date &at, double depth);

} // namespace monica
