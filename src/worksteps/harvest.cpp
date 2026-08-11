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

#include "harvest.h"

#include "../core/monica-model.h"
#include "../run/workstep.h"
#include "json11/json11-helper.h"
#include "sowing.h"
#include "tools/debug.h"

using namespace std;
using namespace monica;
using namespace Tools;

Workstep monica::makeHarvestWorkstep(json11::Json j) {
  Workstep ws;
  ws.data = HarvestData{};
  Errors res = workstep::mergeCommon(&ws, j);
  res.append(workstep::merge(&std::get<HarvestData>(ws.data), j));
  ws.errors = res;
  return ws;
}

Errors workstep::merge(HarvestData *h, json11::Json j) {
  Errors res;

  set_int_value(h->incorporateIntoLayerNo, j, "incorporateIntoLayerNo");
  h->incorporateIntoLayerNo = max(1, h->incorporateIntoLayerNo);
  set_bool_value(h->exported, j, "exported");
  set_bool_value(h->optCarbMgmtData.optCarbonConservation, j,
                 "opt-carbon-conservation");
  set_double_value(h->optCarbMgmtData.cropImpactOnHumusBalance, j,
                   "crop-impact-on-humus-balance");
  auto cu = j["crop-usage"].string_value();
  if (cu == "green-manure")
    h->optCarbMgmtData.cropUsage = HarvestData::greenManure;
  else
    h->optCarbMgmtData.cropUsage = HarvestData::biomassProduction;
  set_double_value(h->optCarbMgmtData.residueHeq, j, "residue-heq");
  set_double_value(h->optCarbMgmtData.organicFertilizerHeq, j,
                   "organic-fertilizer-heq");
  set_double_value(h->optCarbMgmtData.maxResidueRecoverFraction, j,
                   "max-residue-recover-fraction");

  for (const string &organName :
       {"leaf", "shoot", "fruit", "struct", "sugar"}) {
    for (const auto &kv : j.object_items()) {
      if (toLower(kv.first) == organName && kv.second.is_object()) {
        HarvestData::Spec::Value sv;
        set_double_value(sv.exportPercentage, kv.second, "export");
        h->spec.organ2specVal[workstep::organIdFromName(kv.first, res)] = sv;
      }
    }
  }

  return res;
}

json11::Json workstep::to_json(const HarvestData *h, const Workstep *ws,
                               bool includeFullCropParameters) {
  auto jo = json11::Json::object{
      {"type", "Harvest"},
      {"date", ws->date.toIsoDateString()},
      {"incorporateIntoLayerNo", h->incorporateIntoLayerNo},
      {"exported", h->exported},
      {"opt-carbon-conservation", h->optCarbMgmtData.optCarbonConservation},
      {"crop-impact-on-humus-balance",
       h->optCarbMgmtData.cropImpactOnHumusBalance},
      {"crop-usage", h->optCarbMgmtData.cropUsage == HarvestData::greenManure
                         ? "green-manure"
                         : "biomass-production"},
      {"residue-heq", h->optCarbMgmtData.residueHeq},
      {"organic-fertilizer-heq", h->optCarbMgmtData.organicFertilizerHeq},
      {"max-residue-recover-fraction",
       h->optCarbMgmtData.maxResidueRecoverFraction}};

  for (const auto &p : h->spec.organ2specVal) {
    jo[workstep::organNameFromId(p.first)] =
        J11Object{{"export", J11Array{p.second.exportPercentage, "%"}},
                  {"incorporate", p.second.incorporate}};
  }

  return jo;
}

bool workstep::apply(HarvestData *h, Workstep *ws, MonicaModel *model) {
  workstep::applyCommon(ws, model);

  if (model->currentCropModule) {
    monicamodel::harvestCurrentCrop(model, h->exported, h->spec,
                                    h->optCarbMgmtData,
                                    h->incorporateIntoLayerNo - 1);
    if (h->sowing)
      debug() << "harvesting crop: "
              << cropparameters::cropName(&h->sowing->cropParams)
              << " at: " << ws->date.toString() << endl;
    model->currentEvents.insert("Harvest");
  }

  return true;
}
