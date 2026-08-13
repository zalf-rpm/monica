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

#include "n-demand-fertilization.h"

#include <iostream>

#include "../core/monica-model.h"
#include "../run/workstep.h"
#include "json11/json11-helper.h"
#include "tools/debug.h"

using namespace std;
using namespace monica;
using namespace Tools;

Workstep monica::makeNDemandFertilizationWorkstep(int stage, double depth,
                                                  MineralFertilizerParameters partition,
                                                  double Ndemand) {
  Workstep ws;
  NDemandFertilizationData nd;
  nd.partition = partition;
  nd.Ndemand = Ndemand;
  nd.depth = depth;
  nd.stage = stage;
  ws.data = nd;
  return ws;
}

Workstep monica::makeNDemandFertilizationWorkstep(Tools::Date date, double depth,
                                                  MineralFertilizerParameters partition,
                                                  double Ndemand) {
  Workstep ws;
  ws.date = date;
  NDemandFertilizationData nd;
  nd.initialDate = date;
  nd.partition = partition;
  nd.Ndemand = Ndemand;
  nd.depth = depth;
  ws.data = nd;
  return ws;
}

Workstep monica::makeNDemandFertilizationWorkstep(json11::Json j) {
  Workstep ws;
  ws.data = NDemandFertilizationData{};
  Errors res = workstep::mergeCommon(&ws, j);
  res.append(workstep::merge(&std::get<NDemandFertilizationData>(ws.data), &ws, j));
  ws.errors = res;
  return ws;
}

Errors workstep::merge(NDemandFertilizationData *nd, Workstep *ws, json11::Json j) {
  Errors res;
  nd->initialDate = ws->date;
  set_double_value(nd->Ndemand, j, "N-demand");
  {
    string err;
    if (j.has_shape({{"partition", json11::Json::OBJECT}}, err))
      mineralfertilizerparameters::merge(&nd->partition, j["partition"]);
    if (!err.empty())
      cerr << "Error @ NDemandFertilization::merge: " << err << endl;
  }
  set_double_value(nd->depth, j, "depth");
  set_int_value(nd->stage, j, "stage");

  return res;
}

json11::Json workstep::to_json(const NDemandFertilizationData *nd) {
  auto o = J11Object{{"type", "NDemandFertilization"},
                     {"N-demand", nd->Ndemand},
                     {"partition", mineralfertilizerparameters::to_json(&nd->partition)},
                     {"depth", J11Array{nd->depth, "m", "depth of Nmin measurement"}}};
  if (nd->initialDate.isValid())
    o["date"] = nd->initialDate.toIsoDateString();
  else
    o["stage"] = J11Array{nd->stage, "",
                          "if this development stage is entered, the fertilizer will be applied"};

  return o;
}

bool workstep::apply(NDemandFertilizationData *nd, Workstep *ws, MonicaModel *model) {
  workstep::applyCommon(ws, model);

  double rd = model->currentCropModule->vc_RootingDepth_m;
  debug() << workstep::to_json(nd).dump() << endl;
  double appliedAmount = soilcolumn::applyMineralFertiliserViaNDemand(
      model->soilColumn.get(), nd->partition, rd < nd->depth ? rd : nd->depth, nd->Ndemand);
  model->dailySumFertiliser += appliedAmount;
  nd->appliedFertilizer = true;
  // record date of application until next reinit
  ws->date = model->currentStepDate;
  model->currentEvents.insert("NDemandFertilization");

  return true;
}

bool workstep::condition(NDemandFertilizationData *nd, Workstep *ws, MonicaModel *model) {
  bool conditionMet = false;

  auto *cg = model->currentCropModule.get();
  if (cg && !nd->appliedFertilizer) {
    auto currStage = cg->vc_DevelopmentalStage + 1;
    conditionMet = ws->date.isValid()         // is timed application
                   || currStage == nd->stage; // reached the requested stage
  }

  return conditionMet;
}

bool workstep::reinit(NDemandFertilizationData *nd, Workstep *ws, Tools::Date date, bool addYear,
                      bool forceInitYear) {
  ws->date = nd->initialDate;
  bool addedYear = workstep::reinitCommon(ws, date, addYear, forceInitYear);
  nd->appliedFertilizer = false;

  return false; // NOTE: original NDemandFertilization::reinit computes
                // addedYear but always returns false unconditionally -
                // preserved exactly, not a mistake on my part.
}
