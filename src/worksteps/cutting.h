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

#include <map>

#include "json11/json11.hpp"

#include "common/dll-exports.h"
#include "json11/json11-helper.h"

namespace monica {
class MonicaModel;
struct Workstep;

struct DLL_API CuttingData {
  enum CL { cut, left, none };

  enum Unit { percentage, biomass, LAI };

  struct Value {
    double value{0.0};
    Unit unit{percentage};
    CL cut_or_left{cut};
  };

  std::map<int, Value> organId2cuttingSpec;
  std::map<int, double> organId2biomAfterCutting;
  std::map<int, double> organId2exportFraction;
  double cutMaxAssimilationRateFraction{1.0};
};

namespace workstep {

DLL_API Tools::Errors merge(CuttingData *c, json11::Json j);
DLL_API json11::Json to_json(const CuttingData *c, const Workstep *ws);
DLL_API bool apply(CuttingData *c, Workstep *ws, MonicaModel *model);

} // namespace workstep

DLL_API Workstep makeCuttingWorkstep(json11::Json object);

} // namespace monica
