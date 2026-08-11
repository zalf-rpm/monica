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
#include "tools/helper.h"

namespace monica {
class MonicaModel;
struct Workstep;
struct SowingData;

struct HarvestData {
  enum CropUsage { greenManure = 0, biomassProduction };

  struct OptCarbonManagementData {
    bool optCarbonConservation{false};
    double cropImpactOnHumusBalance{0};
    double maxResidueRecoverFraction{1};
    CropUsage cropUsage{biomassProduction};
    double residueHeq{0};
    double organicFertilizerHeq{0};
  };

  struct Spec {
    struct Value {
      double exportPercentage{100.0};
      bool incorporate{true};
    };

    std::map<int, Value> organ2specVal;
  };

  SowingData *sowing{
      nullptr}; // non-owning, points into another workstep's variant payload
  bool exported{true};
  Spec spec;
  OptCarbonManagementData optCarbMgmtData;
  int incorporateIntoLayerNo{1};
};

namespace workstep {

Tools::Errors merge(HarvestData *h, json11::Json j);
json11::Json to_json(const HarvestData *h, const Workstep *ws,
                     bool includeFullCropParameters = true);
bool apply(HarvestData *h, Workstep *ws, MonicaModel *model);

} // namespace workstep

Workstep makeHarvestWorkstep(json11::Json object);

} // namespace monica
