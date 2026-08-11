/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
Authors:
Michael Berg <michael.berg@zalf.de>
Claas Nendel <claas.nendel@zalf.de>
Xenia Specka <xenia.specka@zalf.de>

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

json11::Json workstep::to_json(const TransplantData *t,
                               bool includeFullCropParameters) {
  return json11::Json::object{
      {"type", "Transplant"},
      {"crop", t->cropToPlant
                   ? t->cropToPlant->to_json(includeFullCropParameters)
                   : json11::Json::object{}},
      {"initialStage", static_cast<int>(t->initialStage)},
      {"initialTemperatureSum", t->initialGDD},
      {"initialRootBiomass", t->initRootMass},
      {"initialLeafBiomass", t->initLeafMass},
      {"initialShootBiomass", t->initShootMass},
      {"initialLAI", t->initLAI},
      {"postTransplantDelay", t->postTransplantDelay},
      {"initialKcb", t->initialKcb},
  };
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
