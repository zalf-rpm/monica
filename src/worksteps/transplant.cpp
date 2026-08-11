/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
Authors:
Michael Berg <michael.berg@zalf.de>

Maintainers:
Currently maintained by the authors.

This file is part of the MONICA model.
Copyright (C) Leibniz Centre for Agricultural Landscape Research (ZALF)
*/

#include "transplant.h"

#include "../core/monica-model.h"
#include "../run/workstep.h"
#include "json11/json11-helper.h"

using namespace std;
using namespace monica;
using namespace Tools;

Workstep monica::makeTransplantWorkstep(json11::Json j) {
  Workstep ws;
  ws.data = TransplantData{};
  Errors res = workstep::mergeCommon(&ws, j);
  res.append(workstep::merge(&std::get<TransplantData>(ws.data), j));
  ws.errors = res;
  return ws;
}

Errors workstep::merge(TransplantData *t, json11::Json j) {
  // Mirrors Sowing's own merge (this is the only place that touches the common
  // Workstep fields, via mergeCommon, done once by the make*Workstep factory -
  // not repeated here).
  Errors res = workstep::merge(static_cast<SowingData *>(t), j);

  if (!j["initialStage"].is_null()) {
    t->initialStage = static_cast<size_t>(j["initialStage"].int_value());
  }
  set_double_value(t->initialGDD, j, "initialTemperatureSum");
  set_double_value(t->initRootMass, j, "initialRootBiomass");
  set_double_value(t->initLeafMass, j, "initialLeafBiomass");
  set_double_value(t->initShootMass, j, "initialShootBiomass");
  set_double_value(t->initLAI, j, "initialLAI");
  set_int_value(t->postTransplantDelay, j, "postTransplantDelay");
  // FAO-56 Dual Kc: optional initial Kcb at transplanting (default 0.15 = bare
  // soil)
  set_double_value(t->initialKcb, j, "initialKcb");

  return res; // propagates ALL sub-errors (crop parse errors included)
}

json11::Json workstep::to_json(const TransplantData *t, const Workstep *ws,
                               bool includeFullCropParameters) {
  auto so =
      to_json(static_cast<const SowingData *>(t), ws, includeFullCropParameters)
          .object_items();
  so["type"] = "Transplant";
  so["initialStage"] = static_cast<int>(t->initialStage);
  so["initialTemperatureSum"] = t->initialGDD;
  so["initialRootBiomass"] = t->initRootMass;
  so["initialLeafBiomass"] = t->initLeafMass;
  so["initialShootBiomass"] = t->initShootMass;
  so["initialLAI"] = t->initLAI;
  so["postTransplantDelay"] = t->postTransplantDelay;
  so["initialKcb"] = t->initialKcb;
  return so;
}

bool workstep::apply(TransplantData *t, Workstep *ws, MonicaModel *model) {
  workstep::apply(static_cast<SowingData *>(t), ws, model);

  CropModule *cropModule = model->currentCropModule;
  if (!cropModule)
    return false;

  cropmodule::forceTransplantState(
      cropModule, t->initialGDD, t->initLAI, t->initialStage, t->initRootMass,
      t->initLeafMass, t->initShootMass, t->postTransplantDelay);

  if (model->simPs.dualKcMethod)
    cropModule->vc_Kcb_ini = t->initialKcb;

  model->currentEvents.insert("Transplant");

  return true;
}
