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

#include "organic-fertilization.h"

#include "../core/monica-model.h"
#include "../run/workstep.h"
#include "json11/json11-helper.h"
#include "tools/debug.h"

using namespace std;
using namespace monica;
using namespace Tools;

Workstep
monica::makeOrganicFertilizationWorkstep(const Tools::Date &at,
                                         const OrganicMatterParameters &params,
                                         double amount, bool incorp) {
  Workstep ws;
  ws.date = at;
  OrganicFertilizationData of;
  of.params = params;
  of.amount = amount;
  of.incorporation = incorp;
  ws.data = of;
  return ws;
}

Workstep monica::makeOrganicFertilizationWorkstep(json11::Json j) {
  Workstep ws;
  ws.data = OrganicFertilizationData{};
  Errors res = workstep::mergeCommon(&ws, j);
  res.append(workstep::merge(&std::get<OrganicFertilizationData>(ws.data), j));
  ws.errors = res;
  return ws;
}

Errors workstep::merge(OrganicFertilizationData *of, json11::Json j) {
  Errors res;
  organicmatterparameters::merge(&of->params, j["parameters"]);
  set_double_value(of->amount, j, "amount");
  set_int_value(of->incorporateIntoLayerNo, j, "incorporateIntoLayerNo");
  of->incorporateIntoLayerNo = max(1, of->incorporateIntoLayerNo);
  set_bool_value(of->incorporation, j, "incorporation");
  return res;
}

json11::Json workstep::to_json(const OrganicFertilizationData *of,
                               const Workstep *ws) {
  return json11::Json::object{
      {"type", "OrganicFertilization"},
      {"date", ws->date.toIsoDateString()},
      {"amount", of->amount},
      {"parameters", organicmatterparameters::to_json(&of->params)},
      {"incorporateIntoLayerNo", of->incorporateIntoLayerNo},
      {"incorporation", of->incorporation}};
}

bool workstep::apply(OrganicFertilizationData *of, Workstep *ws,
                     MonicaModel *model) {
  workstep::applyCommon(ws, model);

  debug() << workstep::to_json(of, ws).dump() << endl;
  monicamodel::applyOrganicFertiliser(model, of->params, of->amount,
                                      of->incorporation,
                                      of->incorporateIntoLayerNo - 1);
  model->currentEvents.insert("OrganicFertilization");

  return true;
}
