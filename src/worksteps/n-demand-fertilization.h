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

struct DLL_API NDemandFertilizationData {
  Tools::Date initialDate;
  MineralFertilizerParameters partition;
  double Ndemand{0};
  double depth{0.0};
  int stage{1};
  bool appliedFertilizer{false};
};

namespace workstep {

// note: merge needs ws (it copies the just-parsed common date into
// initialDate); to_json doesn't (it only ever emits its own initialDate/stage
// fields, never the common ws->date).
DLL_API Tools::Errors merge(NDemandFertilizationData *nd, Workstep *ws,
                            json11::Json j);
DLL_API json11::Json to_json(const NDemandFertilizationData *nd);
DLL_API bool apply(NDemandFertilizationData *nd, Workstep *ws,
                   MonicaModel *model);
DLL_API bool condition(NDemandFertilizationData *nd, Workstep *ws,
                       MonicaModel *model);
DLL_API bool reinit(NDemandFertilizationData *nd, Workstep *ws,
                    Tools::Date date, bool addYear = false,
                    bool forceInitYear = false);

} // namespace workstep

DLL_API Workstep makeNDemandFertilizationWorkstep(json11::Json object);
DLL_API Workstep makeNDemandFertilizationWorkstep(
    int stage, double depth, MineralFertilizerParameters partition,
    double Ndemand);
DLL_API Workstep makeNDemandFertilizationWorkstep(
    Tools::Date date, double depth, MineralFertilizerParameters partition,
    double Ndemand);

} // namespace monica
