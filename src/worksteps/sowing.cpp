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

#include "sowing.h"

#include "../core/monica-model.h"
#include "../run/workstep.h"
#include "tools/debug.h"

using namespace std;
using namespace monica;
using namespace Tools;

Workstep monica::makeSowingWorkstep(json11::Json j) {
  Workstep ws;
  ws.data = SowingData{};
  Errors res = workstep::mergeCommon(&ws, j);
  res.append(workstep::merge(&std::get<SowingData>(ws.data), j));
  ws.errors = res;
  return ws;
}

Errors workstep::merge(SowingData *s, json11::Json j) {
  Errors res;

  set_iso_date_value(s->sowingDate, j, "seedDate");
  set_iso_date_value(s->harvestDate, j, "harvestDate");

  if (j["crop"].is_object()) {
    auto jc = j["crop"];

    if (jc["is-perennial-crop"].is_bool())
      s->isPerennialCrop.setValue(jc["is-perennial-crop"].bool_value());

    string err;
    if (jc.has_shape({{"cropParams", json11::Json::OBJECT}}, err)) {
      auto jcps = jc["cropParams"];
      if (jcps.has_shape({{"species", json11::Json::OBJECT}}, err) &&
          jcps.has_shape({{"cultivar", json11::Json::OBJECT}}, err))
        cropparameters::merge(&s->cropParams, jcps);
      else
        res.errors.push_back(string("Couldn't find 'species' or 'cultivar' key "
                                    "in JSON object 'cropParams':\n") +
                             jc.dump());

      if (s->isPerennialCrop.isValue())
        s->cropParams.cultivarParams.pc_Perennial = s->isPerennialCrop.value();
      else
        s->isPerennialCrop.setValue(s->cropParams.cultivarParams.pc_Perennial);

      s->isValid = true;
    } else {
      res.errors.push_back(
          string("Couldn't find 'cropParams' key in JSON object:\n") +
          jc.dump());
      s->isValid = false;
    }

    if (s->isPerennialCrop.isValue() && s->isPerennialCrop.value()) {
      err = "";
      if (jc.has_shape({{"perennialCropParams", json11::Json::OBJECT}}, err)) {
        auto jcps = jc["perennialCropParams"];
        if (jcps.has_shape({{"species", json11::Json::OBJECT}}, err) &&
            jcps.has_shape({{"cultivar", json11::Json::OBJECT}}, err)) {
          s->separatePerennialCropParams = nullptr;
          s->separatePerennialCropParams = kj::heap<CropParameters>();
          cropparameters::merge(s->separatePerennialCropParams.get(), jcps);
        }
      }
    }

    err = "";
    if (jc.has_shape({{"residueParams", json11::Json::OBJECT}}, err)) {
      cropresidueparameters::merge(&s->residueParams, jc["residueParams"]);
    } else {
      res.errors.push_back(
          string("Couldn't find 'residueParams' key in JSON object:\n") +
          jc.dump());
      s->isValid = false;
    }
  }

  set_int_value(s->plantDensity, j, "PlantDensity");
  if (s->plantDensity > 0) {
    s->cropParams.speciesParams.pc_PlantDensity = s->plantDensity;
  }
  // FAO-56 Dual Kc: optional initial Kcb at sowing (default 0.15 = bare soil)
  set_double_value(s->initialKcb, j, "initialKcb");
  return res;
}

json11::Json workstep::to_json(const SowingData *s, const Workstep *ws,
                               bool includeFullCropParameters) {
  auto co = json11::Json::object{
      {"cropParams", cropparameters::to_json(&s->cropParams)},
      {"residueParams", cropresidueparameters::to_json(&s->residueParams)}};
  if (s->separatePerennialCropParams)
    co["perennialCropParams"] =
        cropparameters::to_json(s->separatePerennialCropParams.get());

  auto o = json11::Json::object{
      {"type", "Sowing"},
      {"date", ws->date.toIsoDateString()},
      {"crop", co},
      {"initialKcb", s->initialKcb},
  };

  if (s->plantDensity > 0)
    o["PlantDensity"] = J11Array{s->plantDensity, "plants m-2"};

  return o;
}

bool workstep::apply(SowingData *s, Workstep *ws, MonicaModel *model) {
  workstep::applyCommon(ws, model);

  debug() << "sowing crop: " << cropparameters::cropName(&s->cropParams)
          << " at: " << s->sowingDate.toString() << endl;

  model->p_daysWithCrop = 0;
  model->p_accuNStress = 0.0;
  model->p_accuWaterStress = 0.0;
  model->p_accuHeatStress = 0.0;
  model->p_accuOxygenStress = 0.0;

  if (s->isValid) {
    model->cultivationMethodCount++;

    auto addOMFunc = [model](const std::map<size_t, double> &layer2amount,
                             double nconc) {
      soilorganic::addOrganicMatter(model->soilOrganic.get(),
                                    model->currentCropModule->residuePs,
                                    layer2amount, nconc);
    };
    model->currentCropModule = nullptr;
    model->currentCropModule = makeCropModule(
        model->soilColumn.get(), &s->cropParams, &s->residueParams,
        &model->sitePs, &model->cropPs, &model->simPs,
        [model](string event) {
          model->currentEvents.insert(std::move(event));
        },
        addOMFunc,
        [model](double avgAirTemp) {
          return soilmoisture::getSnowDepthAndCalcTemperatureUnderSnow(
              model->soilMoisture.get(), avgAirTemp);
        },
        &model->intercropping);

    if (s->separatePerennialCropParams)
      model->currentCropModule->perennialCropParams =
          kj::heap<CropParameters>(*s->separatePerennialCropParams.get());

    soiltransport::putCrop(model->soilTransport.get(),
                           model->currentCropModule.get());
    soilcolumn::putCrop(model->soilColumn.get(),
                        model->currentCropModule.get());
    model->soilMoisture->cropModule = model->currentCropModule.get();
    model->soilOrganic->cropModule = model->currentCropModule.get();

    if (model->simPs.p_UseNMinMineralFertilisingMethod &&
        !model->currentCropModule->isWinterCrop) {
      soilcolumn::clearTopDressingParams(model->soilColumn.get());
      debug() << "nMin fertilising summer crop" << endl;
      double fert_amount = monicamodel::applyMineralFertiliserViaNMinMethod(
          model, model->simPs.p_NMinFertiliserPartition,
          makeNMinCropParameters(
              s->cropParams.speciesParams.pc_SamplingDepth,
              s->cropParams.speciesParams.pc_TargetNSamplingDepth,
              s->cropParams.speciesParams.pc_TargetN30));
      monicamodel::addDailySumFertiliser(model, fert_amount);
    }
  }

  // FAO-56 Dual Kc: push initial Kcb into the freshly created crop module
  if (model->simPs.dualKcMethod && model->currentCropModule) {
    model->currentCropModule->vc_Kcb_ini = s->initialKcb;
  }
  model->currentEvents.insert("Sowing");

  return true;
}
