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

#include "irrigation.h"

#include "../core/monica-model.h"
#include "../run/workstep.h"

using namespace std;
using namespace monica;
using namespace Tools;

Workstep monica::makeIrrigationWorkstep(const Tools::Date &at, double amount,
                                          IrrigationParameters params) {
  Workstep ws;
  ws.date = at;
  IrrigationData i;
  i.amount = amount;
  i.params = params;
  ws.data = i;
  return ws;
}

Workstep monica::makeIrrigationWorkstep(json11::Json j) {
  Workstep ws;
  ws.data = IrrigationData{};
  Errors res = workstep::mergeCommon(&ws, j);
  res.append(workstep::merge(&std::get<IrrigationData>(ws.data), j));
  ws.errors = res;
  return ws;
}

Errors workstep::merge(IrrigationData *i, json11::Json j) {
  Errors res;
  set_double_value(i->amount, j, "amount");
  if (j["parameters"].is_object()) {
    irrigationparameters::merge(&i->params, j["parameters"]);
  }
  return res;
}

json11::Json workstep::to_json(const IrrigationData *i, const Workstep *ws) {
  return json11::Json::object{{"type", "Irrigation"},
                              {"date", ws->date.toIsoDateString()},
                              {"amount", i->amount},
                              {"parameters", irrigationparameters::to_json(&i->params)}};
}

bool workstep::apply(IrrigationData *i, Workstep *ws, MonicaModel *model) {
  workstep::applyCommon(ws, model);

  // cout << toString() << endl;
  monicamodel::applyIrrigation(model, i->amount, i->params.nitrateConcentration);
  // FAO-56 Dual Kc: push event-level fw and isDrip into SoilMoisture for today's ET calculation
  // LIMITATION: Auto-irrigation uses sim.json params or defaults (fw=1.0, isDrip=false).
  if (model->simPs.dualKcMethod) {
    model->soilMoisture->vm_irrigFwEvent = i->params.fw;
    model->soilMoisture->vm_irrigIsDripEvent = i->params.isDripIrrigation;
  }
  model->currentEvents.insert("Irrigation");

  return true;
}
