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

#include "mineral-fertilization.h"

#include <iostream>

#include "../core/monica-model.h"
#include "../run/workstep.h"
#include "json11/json11-helper.h"
#include "tools/debug.h"

using namespace std;
using namespace monica;
using namespace Tools;

Workstep monica::makeMineralFertilizationWorkstep(const Tools::Date &at,
                                                  MineralFertilizerParameters partition,
                                                  double amount) {
  Workstep ws;
  ws.date = at;
  MineralFertilizationData mf;
  mf.partition = partition;
  mf.amount = amount;
  ws.data = mf;
  return ws;
}

Workstep monica::makeMineralFertilizationWorkstep(json11::Json j) {
  Workstep ws;
  ws.data = MineralFertilizationData{};
  Errors res = workstep::mergeCommon(&ws, j);
  res.append(workstep::merge(&std::get<MineralFertilizationData>(ws.data), j));
  ws.errors = res;
  return ws;
}

Errors workstep::merge(MineralFertilizationData *mf, json11::Json j) {
  Errors res;
  {
    string err;
    if (j.has_shape({{"partition", json11::Json::OBJECT}}, err))
      mineralfertilizerparameters::merge(&mf->partition, j["partition"]);
    if (!err.empty())
      cerr << "Error @ MineralFertilization::merge: " << err << endl;
  }
  set_double_value(mf->amount, j, "amount");
  return res;
}

json11::Json workstep::to_json(const MineralFertilizationData *mf, const Workstep *ws) {
  return json11::Json::object{{"type", "MineralFertilization"},
                              {"date", ws->date.toIsoDateString()},
                              {"amount", mf->amount},
                              {"partition", mineralfertilizerparameters::to_json(&mf->partition)}};
}

bool workstep::apply(MineralFertilizationData *mf, Workstep *ws, MonicaModel *model) {
  workstep::applyCommon(ws, model);

  debug() << workstep::to_json(mf, ws).dump() << endl;
  monicamodel::applyMineralFertiliser(model, mf->partition, mf->amount);
  model->currentEvents.insert("MineralFertilization");

  return true;
}
