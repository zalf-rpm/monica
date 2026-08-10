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

#include "workstep.h"

#include <algorithm>
#include <numeric>
#include <utility>

#include "../core/monica-model.h"
#include "tools/algorithms.h"
#include "tools/debug.h"

using namespace std;
using namespace monica;
using namespace Soil;
using namespace Tools;
using namespace Climate;

namespace {
std::pair<Date, bool> makeInitAbsDate(Date date, Date initDate, bool addYear,
                                      bool forceInitYear = false) {
  bool addedYear = false;

  if (date.isAbsoluteDate())
    return make_pair(date, addedYear);

  Date absDate = date.toAbsoluteDate(initDate.year());
  if (!forceInitYear && (addYear || (absDate < initDate))) {
    addedYear = true;
    absDate.addYears(1);
  }

  return make_pair(absDate, addedYear);
}

int organIdFromName(const string &organName, Tools::Errors &err) {
  string os = toLower(organName);
  if (os == "root")
    return 0;
  else if (os == "leaf")
    return 1;
  else if (os == "shoot")
    return 2;
  else if (os == "fruit")
    return 3;
  else if (os == "struct")
    return 4;
  else if (os == "sugar")
    return 5;
  err.append(Errors(Errors::ERR, "organ id could not be resolved"));
  return -1; // default error
}

string organNameFromId(int organId) {
  string res = "unknown";
  switch (organId) {
  case 0:
    res = "Root";
    break;
  case 1:
    res = "Leaf";
    break;
  case 2:
    res = "Shoot";
    break;
  case 3:
    res = "Fruit";
    break;
  case 4:
    res = "Struct";
    break;
  case 5:
    res = "Sugar";
    break;
  default:
    "unknown";
  }
  return res;
}

// Harvest::Spec/OptCarbonManagementData are structurally identical to the new HarvestData::Spec/
// OptCarbonManagementData, but monicamodel::harvestCurrentCrop's signature still takes the old types
// (updating it to the new ones is a step-18/final-cutover change, since the old Harvest class it also
// serves is still live until then) - convert on the way in rather than touching monica-model.h early.
Harvest::Spec toOldHarvestSpec(const HarvestData::Spec &spec) {
  Harvest::Spec old;
  for (const auto &p : spec.organ2specVal) {
    Harvest::Spec::Value v;
    v.exportPercentage = p.second.exportPercentage;
    v.incorporate = p.second.incorporate;
    old.organ2specVal[p.first] = v;
  }
  return old;
}

Harvest::OptCarbonManagementData toOldOptCarbMgmtData(const HarvestData::OptCarbonManagementData &d) {
  Harvest::OptCarbonManagementData old;
  old.optCarbonConservation = d.optCarbonConservation;
  old.cropImpactOnHumusBalance = d.cropImpactOnHumusBalance;
  old.maxResidueRecoverFraction = d.maxResidueRecoverFraction;
  old.cropUsage = d.cropUsage == HarvestData::greenManure ? Harvest::greenManure : Harvest::biomassProduction;
  old.residueHeq = d.residueHeq;
  old.organicFertilizerHeq = d.organicFertilizerHeq;
  return old;
}
} // namespace

Errors workstep::mergeCommon(WorkstepV2 *ws, json11::Json j) {
  Errors res;

  set_iso_date_value(ws->date, j, "date");
  // at is a shortcut for after=event and days=1
  auto at = string_value(j, "at");
  if (!at.empty()) {
    ws->afterEvent = at;
    ws->applyNoOfDaysAfterEvent = 1;
  }
  set_int_value(ws->applyNoOfDaysAfterEvent, j, "days");
  set_string_value(ws->afterEvent, j, "after");
  set_bool_value(ws->runAtStartOfDay, j, "runAtStartOfDay");

  return res;
}

bool workstep::applyCommon(WorkstepV2 *ws, MonicaModel *model) {
  model->currentEvents.insert("Workstep");
  return true;
}

bool workstep::conditionCommon(WorkstepV2 *ws, MonicaModel *model) {
  if (ws->afterEvent.empty() || ws->applyNoOfDaysAfterEvent <= 0) {
    return false;
  }

  const auto &currEvents = model->currentEvents;
  const auto &prevEvents = model->previousDaysEvents;

  auto ceit = currEvents.find(ws->afterEvent);
  if (ws->daysAfterEventCountActivated) {
    ws->daysAfterEventCount++;
  } else if (ceit != currEvents.end() ||
             prevEvents.find(ws->afterEvent) != prevEvents.end()) {
    ws->daysAfterEventCountActivated = true;
  }

  return ws->daysAfterEventCount == ws->applyNoOfDaysAfterEvent;
}

bool workstep::reinitCommon(WorkstepV2 *ws, Tools::Date date, bool addYear, bool forceInitYear) {
  bool addedYear = false;

  if (ws->date.isValid()) {
    tie(ws->absDate, addedYear) = makeInitAbsDate(ws->date, date, addYear, forceInitYear);
  } else {
    ws->absDate = Date();
  }

  ws->isActive = true;
  ws->daysAfterEventCount = 0;
  ws->daysAfterEventCountActivated = false;

  return addedYear;
}

void workstep::setDate(WorkstepV2 *ws, Tools::Date date) {
  ws->date = date;
  switch (type(ws)) {
  case WorkstepType::SOWING:
    std::get<SowingData>(ws->data).sowingDate = date;
    break;
  case WorkstepType::AUTOMATIC_SOWING:
    std::get<AutomaticSowingData>(ws->data).sowingDate = date;
    break;
  case WorkstepType::TRANSPLANT: {
    auto &t = std::get<TransplantData>(ws->data);
    if (t.cropToPlant)
      t.cropToPlant->setSeedDate(date);
    break;
  }
  default:
    break;
  }
}

WorkstepV2 monica::makeSowingWorkstep(json11::Json j) {
  WorkstepV2 ws;
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
          string("Couldn't find 'cropParams' key in JSON object:\n") + jc.dump());
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
          string("Couldn't find 'residueParams' key in JSON object:\n") + jc.dump());
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

json11::Json workstep::to_json(const SowingData *s, const WorkstepV2 *ws, bool includeFullCropParameters) {
  auto co = json11::Json::object{
      {"cropParams", cropparameters::to_json(&s->cropParams)},
      {"residueParams", cropresidueparameters::to_json(&s->residueParams)}};
  if (s->separatePerennialCropParams)
    co["perennialCropParams"] = cropparameters::to_json(s->separatePerennialCropParams.get());

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

bool workstep::apply(SowingData *s, WorkstepV2 *ws, MonicaModel *model) {
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

    auto addOMFunc = [model](const std::map<size_t, double> &layer2amount, double nconc) {
      soilorganic::addOrganicMatter(model->soilOrganic.get(),
                                    model->currentCropModule->residuePs,
                                    layer2amount, nconc);
    };
    model->currentCropModule = nullptr;
    model->currentCropModule = makeCropModule(
        model->soilColumn.get(), &s->cropParams, &s->residueParams, &model->sitePs,
        &model->cropPs, &model->simPs,
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

    soiltransport::putCrop(model->soilTransport.get(), model->currentCropModule.get());
    soilcolumn::putCrop(model->soilColumn.get(), model->currentCropModule.get());
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

WorkstepV2 monica::makeAutomaticSowingWorkstep(json11::Json j) {
  WorkstepV2 ws;
  ws.data = AutomaticSowingData{};
  Errors res = workstep::mergeCommon(&ws, j);
  res.append(workstep::merge(&std::get<AutomaticSowingData>(ws.data), j));
  ws.errors = res;
  return ws;
}

Errors workstep::merge(AutomaticSowingData *as, json11::Json j) {
  Errors res = workstep::merge(static_cast<SowingData *>(as), j);

  set_iso_date_value(as->earliestDate, j, "earliest-date");
  set_iso_date_value(as->latestDate, j, "latest-date");
  set_double_value(as->minTempThreshold, j, "min-temp");
  set_int_value(as->daysInTempWindow, j, "days-in-temp-window");
  set_double_value(as->minPercentASW, j, "min-%-asw");
  set_double_value(as->maxPercentASW, j, "max-%-asw");
  set_double_value(as->max3dayPrecipSum, j, "max-3d-precip");
  set_double_value(as->maxCurrentDayPrecipSum, j, "max-curr-day-precip");
  set_double_value(as->tempSumAboveBaseTemp, j, "temp-sum-above-base-temp");
  set_double_value(as->baseTemp, j, "base-temp");

  json11::Json avgSoilTemp = j["avg-soil-temp"];
  if (avgSoilTemp.is_object()) {
    set_double_value(as->soilDepthForAveraging, avgSoilTemp, "depth");
    set_int_value(as->daysInSoilTempWindow, avgSoilTemp, "days");
    set_double_value(as->sowingIfAboveAvgSoilTemp, avgSoilTemp, "Tavg");
    as->checkForSoilTemperature = as->soilDepthForAveraging > 0 &&
                                  as->daysInSoilTempWindow > 0 &&
                                  as->sowingIfAboveAvgSoilTemp > 0;
  }

  return res;
}

json11::Json workstep::to_json(const AutomaticSowingData *as, const WorkstepV2 *ws,
                               bool includeFullCropParameters) {
  auto o = workstep::to_json(static_cast<const SowingData *>(as), ws).object_items();
  o["type"] = "AutomaticSowing";
  o["earliest-date"] =
      J11Array{as->earliestDate.toIsoDateString(), "", "earliest sowing date"};
  o["latest-date"] =
      J11Array{as->latestDate.toIsoDateString(), "", "latest sowing date"};
  o["min-temp"] = J11Array{as->minTempThreshold, "\xEF\xBF\xBD" "C",
                           "minimal air temperature for sowing (T >= thresh && "
                           "avg T in Twindow >= thresh)"};
  o["days-in-temp-window"] = J11Array{
      as->daysInTempWindow, "d", "days to be used for sliding window of min-temp"};
  o["min-%-asw"] =
      J11Array{as->minPercentASW, "%",
               "minimal soil-moisture in percent of available soil-water"};
  o["max-%-asw"] =
      J11Array{as->maxPercentASW, "%",
               "maximal soil-moisture in percent of available soil-water"};
  o["max-3d-precip-sum"] = J11Array{
      as->max3dayPrecipSum, "mm",
      "sum of precipitation in the last three days (including current day)"};
  o["max-curr-day-precip"] =
      J11Array{as->maxCurrentDayPrecipSum, "mm",
               "max precipitation allowed at current day"};
  o["temp-sum-above-base-temp"] = J11Array{
      as->tempSumAboveBaseTemp, "\xEF\xBF\xBD" "C", "temperature sum above T-base needed"};
  o["base-temp"] = J11Array{
      as->baseTemp, "\xEF\xBF\xBD" "C",
      "base temperature above which temp-sum-above-base-temp is counted"};
  o["avg-soil-temp"] = J11Object{
      {"depth", J11Array{as->soilDepthForAveraging, "m",
                        "soil depth until averaging will be done"}},
      {"days", J11Array{as->daysInSoilTempWindow, "d",
                       "window/number of days for which the average "
                       "temperature must be greater"}},
      {"Tavg", J11Array{as->sowingIfAboveAvgSoilTemp, "\xEF\xBF\xBD" "C",
                       "temperature which has to be reached on average"}}};

  return o;
}

namespace {
bool isSoilMoistureOk(MonicaModel *model, double minPercentASW, double maxPercentASW) {
  bool soilMoistureOk = false;
  double pwp = model->soilColumn->at(0)._sps.vs_PermanentWiltingPoint;
  double sm = max(0.0, model->soilColumn->at(0).vs_SoilMoisture_m3 - pwp);
  double asw = model->soilColumn->at(0)._sps.vs_FieldCapacity - pwp;
  double currentPercentASW = sm / asw * 100.0;
  soilMoistureOk =
      minPercentASW <= currentPercentASW && currentPercentASW <= maxPercentASW;

  return soilMoistureOk;
}

bool isPrecipitationOk(const std::vector<std::map<Climate::ACD, double>> &climateData,
                       double max3dayPrecipSum, double maxCurrentDayPrecipSum) {
  bool precipOk = false;
  double psum3d =
      std::accumulate(climateData.rbegin(), climateData.rbegin() + 3, 0.0,
                      [](double acc, const map<ACD, double> &d) {
                        auto it = d.find(Climate::precip);
                        return acc + (it == d.end() ? 0 : it->second);
                      });
  double currentp = climateData.back().at(Climate::precip);
  precipOk = psum3d <= max3dayPrecipSum && currentp <= maxCurrentDayPrecipSum;

  return precipOk;
}

bool isSoilTemperatureOk(const std::vector<double> &soilTemps, int windowDays,
                         double targetAvgSoilTemp) {
  if (soilTemps.empty())
    return false;

  double sum = 0;
  auto size = soilTemps.size();
  for (int i = int(size - 1); i >= 0 && i >= size - windowDays; i--)
    sum += soilTemps[i];
  double avg = sum / (size > windowDays ? windowDays : size);
  return avg >= targetAvgSoilTemp;
}
} // namespace

bool workstep::apply(AutomaticSowingData *as, WorkstepV2 *ws, MonicaModel *model) {
  auto currentDate = model->currentStepDate;

  as->sowingDate = currentDate;

  workstep::apply(static_cast<SowingData *>(as), ws, model);
  model->currentEvents.insert("AutomaticSowing");
  as->cropSeeded = true;
  as->inSowingRange = false;

  return true;
}

std::function<double(MonicaModel *)>
workstep::registerDailyFunction(AutomaticSowingData *as,
                                std::function<std::vector<double> &()> getDailyValues) {
  if (!as->checkForSoilTemperature)
    return std::function<double(MonicaModel *)>();

  as->getAvgSoilTemps = getDailyValues;
  return [as](MonicaModel *model) -> double {
    double avgSoilTemp = 0;
    size_t i = 0;
    for (auto size = soilcolumn::getLayerNumberForDepth(
                         model->soilColumn.get(), as->soilDepthForAveraging) +
                     1;
         i < size; i++) {
      avgSoilTemp += model->soilTemperature->soilColumn->at(int(i)).vs_SoilTemperature;
    }
    return avgSoilTemp / double(i);
  };
}

bool workstep::condition(AutomaticSowingData *as, MonicaModel *model) {
  if (as->cropSeeded)
    return false;

  auto currentDate = model->currentStepDate;

  if (!as->inSowingRange && currentDate < as->absEarliestDate)
    return false;
  else
    as->inSowingRange = true;

  if (as->inSowingRange && currentDate >= as->absLatestDate)
    return true;

  // check soil temperature if requested
  if (as->checkForSoilTemperature) {
    if (!isSoilTemperatureOk(as->getAvgSoilTemps(), as->daysInSoilTempWindow,
                             as->sowingIfAboveAvgSoilTemp))
      return false;
  }

  const auto &cd = model->climateData;
  auto currentCd = cd.back();

  auto avg = [&](Climate::ACD acd) {
    return accumulate(cd.rbegin(),
                      cd.rbegin() + std::min(int(cd.size()), as->daysInTempWindow), 0.0,
                      [acd](double acc, const map<ACD, double> &d) {
                        auto it = d.find(acd);
                        return acc + (it == d.end() ? 0 : it->second);
                      }) /
           min(int(cd.size()), as->daysInTempWindow);
  };

  // check temperature
  bool Tok = false;
  if (as->cropParams.cultivarParams.winterCrop) {
    double avgTavg = avg(Climate::tavg);
    Tok = avgTavg <= as->minTempThreshold;
  } else {
    double avgTmin = avg(Climate::tmin);
    bool avgTminOk = avgTmin >= as->minTempThreshold;
    bool TminOk = currentCd[Climate::tmin] >= as->minTempThreshold;
    Tok = avgTminOk && TminOk;
  }

  if (!Tok)
    return false;

  // check soil moisture
  if (!isSoilMoistureOk(model, as->minPercentASW, as->maxPercentASW))
    return false;

  // check precipitation
  if (!isPrecipitationOk(cd, as->max3dayPrecipSum, as->maxCurrentDayPrecipSum))
    return false;

  // check temperature sum
  double baseTemp = as->baseTemp;
  double tempSum = accumulate(
      cd.begin(), cd.end(), 0.0,
      [baseTemp](double acc, const map<ACD, double> &d) {
        auto it = d.find(Climate::tavg);
        return acc + (it == d.end() ? 0 : max(0.0, it->second - baseTemp));
      });
  if (tempSum < as->tempSumAboveBaseTemp)
    return false;

  return true;
}

bool workstep::reinit(AutomaticSowingData *as, WorkstepV2 *ws, Tools::Date date, bool addYear,
                      bool forceInitYear) {
  workstep::reinitCommon(ws, date, addYear);

  as->cropSeeded = as->inSowingRange = false;
  workstep::setDate(ws, Tools::Date());

  bool addedYear1, addedYear2;
  // init first the latest date, if the latest date stays in current year, so has to stay the earliest
  // date (thus force current year) if there is a forced current (init) year, this will force both
  // dates to this year
  tie(as->absLatestDate, addedYear1) = makeInitAbsDate(as->latestDate, date, addYear, forceInitYear);
  tie(as->absEarliestDate, addedYear2) =
      makeInitAbsDate(as->earliestDate, date, addYear, forceInitYear || !addedYear1);

  return addedYear1; // || addedYear2;
}

WorkstepV2 monica::makeTransplantWorkstep(json11::Json j) {
  WorkstepV2 ws;
  ws.data = TransplantData{};
  Errors res = workstep::mergeCommon(&ws, j);
  res.append(workstep::merge(&std::get<TransplantData>(ws.data), j));
  ws.errors = res;
  return ws;
}

Errors workstep::merge(TransplantData *t, json11::Json j) {
  // Mirrors Sowing's own merge (this is the only place that touches the common Workstep fields, via
  // mergeCommon, done once by the make*Workstep factory - not repeated here).
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
  // FAO-56 Dual Kc: optional initial Kcb at transplanting (default 0.15 = bare soil)
  set_double_value(t->initialKcb, j, "initialKcb");

  return res; // propagates ALL sub-errors (crop parse errors included)
}

json11::Json workstep::to_json(const TransplantData *t, bool includeFullCropParameters) {
  return json11::Json::object{
      {"type", "Transplant"},
      {"crop", t->cropToPlant ? t->cropToPlant->to_json(includeFullCropParameters)
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

bool workstep::apply(TransplantData *t, WorkstepV2 *ws, MonicaModel *model) {
  workstep::apply(static_cast<SowingData *>(t), ws, model);

  CropModule *cropModule = model->currentCropModule;
  if (!cropModule)
    return false;

  cropmodule::forceTransplantState(cropModule, t->initialGDD, t->initLAI, t->initialStage,
                                   t->initRootMass, t->initLeafMass, t->initShootMass,
                                   t->postTransplantDelay);

  if (model->simPs.dualKcMethod)
    cropModule->vc_Kcb_ini = t->initialKcb;

  model->currentEvents.insert("Transplant");

  return true;
}

WorkstepV2 monica::makeHarvestWorkstep(json11::Json j) {
  WorkstepV2 ws;
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
  set_bool_value(h->optCarbMgmtData.optCarbonConservation, j, "opt-carbon-conservation");
  set_double_value(h->optCarbMgmtData.cropImpactOnHumusBalance, j, "crop-impact-on-humus-balance");
  auto cu = j["crop-usage"].string_value();
  if (cu == "green-manure")
    h->optCarbMgmtData.cropUsage = HarvestData::greenManure;
  else
    h->optCarbMgmtData.cropUsage = HarvestData::biomassProduction;
  set_double_value(h->optCarbMgmtData.residueHeq, j, "residue-heq");
  set_double_value(h->optCarbMgmtData.organicFertilizerHeq, j, "organic-fertilizer-heq");
  set_double_value(h->optCarbMgmtData.maxResidueRecoverFraction, j, "max-residue-recover-fraction");

  for (const string &organName : {"leaf", "shoot", "fruit", "struct", "sugar"}) {
    for (const auto &kv : j.object_items()) {
      if (toLower(kv.first) == organName && kv.second.is_object()) {
        HarvestData::Spec::Value sv;
        set_double_value(sv.exportPercentage, kv.second, "export");
        h->spec.organ2specVal[organIdFromName(kv.first, res)] = sv;
      }
    }
  }

  return res;
}

json11::Json workstep::to_json(const HarvestData *h, const WorkstepV2 *ws,
                               bool includeFullCropParameters) {
  auto jo = json11::Json::object{
      {"type", "Harvest"},
      {"date", ws->date.toIsoDateString()},
      {"incorporateIntoLayerNo", h->incorporateIntoLayerNo},
      {"exported", h->exported},
      {"opt-carbon-conservation", h->optCarbMgmtData.optCarbonConservation},
      {"crop-impact-on-humus-balance", h->optCarbMgmtData.cropImpactOnHumusBalance},
      {"crop-usage", h->optCarbMgmtData.cropUsage == HarvestData::greenManure
                         ? "green-manure"
                         : "biomass-production"},
      {"residue-heq", h->optCarbMgmtData.residueHeq},
      {"organic-fertilizer-heq", h->optCarbMgmtData.organicFertilizerHeq},
      {"max-residue-recover-fraction", h->optCarbMgmtData.maxResidueRecoverFraction}};

  for (const auto &p : h->spec.organ2specVal) {
    jo[organNameFromId(p.first)] =
        J11Object{{"export", J11Array{p.second.exportPercentage, "%"}},
                  {"incorporate", p.second.incorporate}};
  }

  return jo;
}

bool workstep::apply(HarvestData *h, WorkstepV2 *ws, MonicaModel *model) {
  workstep::applyCommon(ws, model);

  if (model->currentCropModule) {
    monicamodel::harvestCurrentCrop(model, h->exported, toOldHarvestSpec(h->spec),
                                    toOldOptCarbMgmtData(h->optCarbMgmtData),
                                    h->incorporateIntoLayerNo - 1);
    if (h->sowing)
      debug() << "harvesting crop: " << cropparameters::cropName(&h->sowing->cropParams)
              << " at: " << ws->date.toString() << endl;
    model->currentEvents.insert("Harvest");
  }

  return true;
}
