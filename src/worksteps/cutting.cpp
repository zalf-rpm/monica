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

#include "cutting.h"

#include <cassert>

#include "../core/monica-model.h"
#include "../run/workstep.h"
#include "json11/json11-helper.h"
#include "tools/debug.h"

using namespace std;
using namespace monica;
using namespace Tools;

Workstep monica::makeCuttingWorkstep(json11::Json j) {
  Workstep ws;
  ws.data = CuttingData{};
  Errors res = workstep::mergeCommon(&ws, j);
  res.append(workstep::merge(&std::get<CuttingData>(ws.data), j));
  ws.errors = res;
  return ws;
}

Errors workstep::merge(CuttingData *c, json11::Json j) {
  Errors errors;

  bool export_ = j["export"].is_bool() ? j["export"].bool_value() : true;

  for (auto p : j["organs"].object_items()) {
    int oid = workstep::organIdFromName(p.first, errors);
    if (oid == -1)
      continue;
    CuttingData::Value v;
    auto arr = p.second.array_items();
    if (arr.size() > 0)
      v.value = double_valueD(arr[0].number_value(), 0);
    if (arr.size() > 1) {
      v.unit = CuttingData::percentage;
      auto p2 = arr[1].string_value();
      if (p2 == "kg ha-1")
        v.unit = CuttingData::biomass;
      else if (p2 == "m2 m-2" && oid == 1)
        v.unit = CuttingData::LAI;
      else if (p2 == "%")
        v.value = v.value / 100.0;
      else {
        // treat no unit as percentage
        v.value = v.value / 100.0;
        errors.append(string("Unknown unit: ") + p2 +
                      " in Cutting workstep: " + j.dump());
      }
    }
    if (arr.size() > 2) {
      auto col = arr[2].string_value();
      if (col == "cut")
        v.cut_or_left = CuttingData::cut;
      else if (col == "left")
        v.cut_or_left = CuttingData::left;
      else
        v.cut_or_left = CuttingData::none;
    }

    c->organId2cuttingSpec[oid] = v;
    c->organId2exportFraction[oid] = export_ ? 1 : 0;
  }

  for (auto p : j["export"].object_items()) {
    int oid = workstep::organIdFromName(p.first, errors);
    if (oid == -1)
      continue;
    c->organId2exportFraction[oid] = int_valueD(p.second, 0) / 100.0;
  }

  set_double_value(c->cutMaxAssimilationRateFraction, j,
                   "cut-max-assimilation-rate",
                   [](double v) { return v / 100.0; });

  return errors;
}

json11::Json workstep::to_json(const CuttingData *c, const Workstep *ws) {
  J11Object organs;
  for (auto p : c->organId2cuttingSpec)
    organs[workstep::organNameFromId(p.first)] = J11Array{
        p.second.value *
            (p.second.unit == CuttingData::percentage ? 100.0 : 1.0),
        p.second.unit == CuttingData::percentage
            ? "%"
            : (p.second.unit == CuttingData::biomass ? "kg ha-1" : "m2 m-2"),
        p.second.cut_or_left == CuttingData::cut ? "cut" : "left"};

  // NOTE: computed but never actually included in the returned JSON below -
  // matches the original Cutting::to_json exactly (organsBiomAfterCutting is
  // built and then discarded there too).
  J11Object organsBiomAfterCutting;
  for (auto p : c->organId2biomAfterCutting)
    organsBiomAfterCutting[workstep::organNameFromId(p.first)] =
        J11Array{int(p.second), "kg ha-1"};

  J11Object exports;
  for (auto p : c->organId2exportFraction)
    exports[workstep::organNameFromId(p.first)] =
        J11Array{int(p.second * 100.0), "%"};

  return json11::Json::object{
      {"type", "Cutting"},
      {"date", ws->date.toIsoDateString()},
      {"organs", organs},
      {"exports", exports},
      {"cut-max-assimilation-rate",
       J11Array{int(c->cutMaxAssimilationRateFraction * 100.0), "%"}}};
}

bool workstep::apply(CuttingData *c, Workstep *ws, MonicaModel *model) {
  workstep::applyCommon(ws, model);

  assert(model->currentCropModule);
  debug() << "Cutting crop: "
          << cropparameters::cropName(&model->currentCropModule->cropParams)
          << " at: " << ws->date.toString() << endl;

  cropmodule::applyCutting(model->currentCropModule, c->organId2cuttingSpec,
                           c->organId2exportFraction,
                           c->cutMaxAssimilationRateFraction);
  model->currentEvents.insert("Cutting");

  return true;
}
