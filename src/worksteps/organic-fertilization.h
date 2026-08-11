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
#include "tools/date.h"

namespace monica {
class MonicaModel;
struct Workstep;

struct DLL_API OrganicFertilizationData {
  OrganicMatterParameters params;
  double amount{0.0};
  bool incorporation{false};
  int incorporateIntoLayerNo{1};
};

namespace workstep {

DLL_API Tools::Errors merge(OrganicFertilizationData *of, json11::Json j);
DLL_API json11::Json to_json(const OrganicFertilizationData *of,
                             const Workstep *ws);
DLL_API bool apply(OrganicFertilizationData *of, Workstep *ws,
                   MonicaModel *model);

} // namespace workstep

DLL_API Workstep makeOrganicFertilizationWorkstep(json11::Json object);
DLL_API Workstep makeOrganicFertilizationWorkstep(
    const Tools::Date &at, const OrganicMatterParameters &params, double amount,
    bool incorp = true);

} // namespace monica
