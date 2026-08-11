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

#include "tillage.h"

#include "../core/monica-model.h"
#include "../run/workstep.h"
#include "json11/json11-helper.h"
#include "tools/debug.h"

using namespace std;
using namespace monica;
using namespace Tools;

Workstep monica::makeTillageWorkstep(const Tools::Date &at, double depth) {
  Workstep ws;
  ws.date = at;
  TillageData t;
  t.depth = depth;
  ws.data = t;
  return ws;
}

Workstep monica::makeTillageWorkstep(json11::Json j) {
  Workstep ws;
  ws.data = TillageData{};
  Errors res = workstep::mergeCommon(&ws, j);
  res.append(workstep::merge(&std::get<TillageData>(ws.data), j));
  ws.errors = res;
  return ws;
}

Errors workstep::merge(TillageData *t, json11::Json j) {
  Errors res;
  set_double_value(t->depth, j, "depth");
  return res;
}

json11::Json workstep::to_json(const TillageData *t, const Workstep *ws) {
  return json11::Json::object{{"type", "Tillage"},
                              {"date", ws->date.toIsoDateString()},
                              {"depth", t->depth}};
}

bool workstep::apply(TillageData *t, Workstep *ws, MonicaModel *model) {
  workstep::applyCommon(ws, model);

  debug() << workstep::to_json(t, ws).dump() << endl;
  monicamodel::applyTillage(model, t->depth);
  model->currentEvents.insert("Tillage");

  return true;
}
