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

// #include "../core/crop.h"
#include "sowing.h"

namespace monica {
class MonicaModel;
struct Workstep;

struct TransplantData : SowingData {
  // kj::Own<Crop>
  // cropToPlant; // Manages the genetic characteristics of the crop to plant

  // Seedling initial parameters forced at transplanting
  size_t initialStage{2};
  double initialGDD{0.0};
  double initRootMass{0.0};
  double initLeafMass{0.0};
  double initShootMass{0.0};
  double initLAI{0.0};
  int postTransplantDelay{0};
  double initialKcb{
      0.15}; //!< FAO-56 Dual Kc: initial Kcb at transplanting (default = 0.15)
};

namespace workstep {

Tools::Errors merge(TransplantData *t, json11::Json j);
// note: unlike Sowing/AutomaticSowing, the original Transplant::to_json never
// embedded "date" - no Workstep* parameter needed here, preserved as-is
// (straight translation).
json11::Json to_json(const TransplantData *t, const Workstep *ws,
                     bool includeFullCropParameters = true);
bool apply(TransplantData *t, Workstep *ws, MonicaModel *model);

} // namespace workstep

Workstep makeTransplantWorkstep(json11::Json object);

} // namespace monica
