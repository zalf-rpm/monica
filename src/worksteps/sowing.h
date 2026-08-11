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

struct DLL_API SowingData {
  bool isValid{false};
  Tools::Date sowingDate;
  Tools::Date harvestDate;
  Tools::Maybe<bool> isPerennialCrop;
  CropParameters cropParams;
  kj::Own<CropParameters> separatePerennialCropParams;
  CropResidueParameters residueParams;
  int plantDensity{-1}; //[plants m-2]
  double initialKcb{0.15}; //!< FAO-56 Dual Kc: initial Kcb at planting (default = 0.15)
};

namespace workstep {

DLL_API Tools::Errors merge(SowingData *s, json11::Json j);
DLL_API json11::Json to_json(const SowingData *s, const Workstep *ws,
                             bool includeFullCropParameters = true);
DLL_API bool apply(SowingData *s, Workstep *ws, MonicaModel *model);

} // namespace workstep

DLL_API Workstep makeSowingWorkstep(json11::Json object);

} // namespace monica
