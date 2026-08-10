/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
Authors:
Claas Nendel <claas.nendel@zalf.de>
Xenia Specka <xenia.specka@zalf.de>
Michael Berg <michael.berg@zalf.de>

Maintainers:
Currently maintained by the authors.

This file is part of the MONICA model.
Copyright (C) Leibniz Centre for Agricultural Landscape Research (ZALF)
*/

#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "json11/json11.hpp"

#include "../core/crop.h"
#include "../core/monica-parameters.h"
#include "../io/output.h"
#include "common/dll-exports.h"
#include "json11/json11-helper.h"
#include "tools/date.h"

namespace monica {
class MonicaModel;

enum class WorkstepType {
  SOWING,
  AUTOMATIC_SOWING,
  TRANSPLANT,
  HARVEST,
  AUTOMATIC_HARVEST,
  CUTTING,
  MINERAL_FERTILIZATION,
  N_DEMAND_FERTILIZATION,
  ORGANIC_FERTILIZATION,
  TILLAGE,
  SET_VALUE,
  SAVE_MONICA_STATE,
  IRRIGATION,
  AUTOMATIC_IRRIGATION
};

struct DLL_API SowingData {
  bool isValid{false};
  Tools::Date sowingDate;
  Tools::Date harvestDate;
  Tools::Maybe<bool> isPerennialCrop;
  CropParameters cropParams;
  kj::Own<CropParameters> separatePerennialCropParams;
  CropResidueParameters residueParams;
  int plantDensity{-1}; //[plants m-2]
  double initialKcb{0.15}; //!< FAO-56 Dual Kc: initial Kcb at planting (default = 0.15)
};

struct DLL_API AutomaticSowingData : SowingData {
  Tools::Date absEarliestDate;
  Tools::Date earliestDate;
  Tools::Date latestDate;
  Tools::Date absLatestDate;
  double minTempThreshold{0};
  int daysInTempWindow{0};
  double minPercentASW{0};
  double maxPercentASW{100};
  double max3dayPrecipSum{0};
  double maxCurrentDayPrecipSum{0};
  double tempSumAboveBaseTemp{0};
  double baseTemp{0};

  bool checkForSoilTemperature{false};
  double soilDepthForAveraging{0.30}; //= 30 cm
  int daysInSoilTempWindow{0};
  double sowingIfAboveAvgSoilTemp{0};
  std::function<std::vector<double> &()> getAvgSoilTemps;

  bool inSowingRange{false};
  bool cropSeeded{false};
};

struct DLL_API TransplantData : SowingData {
  kj::Own<Crop> cropToPlant; // Manages the genetic characteristics of the crop to plant

  // Seedling initial parameters forced at transplanting
  size_t initialStage{2};
  double initialGDD{0.0};
  double initRootMass{0.0};
  double initLeafMass{0.0};
  double initShootMass{0.0};
  double initLAI{0.0};
  int postTransplantDelay{0};
  double initialKcb{0.15}; //!< FAO-56 Dual Kc: initial Kcb at transplanting (default = 0.15)
};

struct DLL_API HarvestData {
  enum CropUsage { greenManure = 0, biomassProduction };

  struct OptCarbonManagementData {
    bool optCarbonConservation{false};
    double cropImpactOnHumusBalance{0};
    double maxResidueRecoverFraction{1};
    CropUsage cropUsage{biomassProduction};
    double residueHeq{0};
    double organicFertilizerHeq{0};
  };

  struct Spec {
    struct Value {
      double exportPercentage{100.0};
      bool incorporate{true};
    };

    std::map<int, Value> organ2specVal;
  };

  SowingData *sowing{nullptr}; // non-owning, points into another workstep's variant payload
  bool exported{true};
  Spec spec;
  OptCarbonManagementData optCarbMgmtData;
  int incorporateIntoLayerNo{1};
};

struct DLL_API AutomaticHarvestData : HarvestData {
  std::string harvestTime; //!< Harvest time parameter
  Tools::Date latestDate;
  Tools::Date absLatestDate;
  double minPercentASW{0};
  double maxPercentASW{999};
  double max3dayPrecipSum{9999};
  double maxCurrentDayPrecipSum{9999};
  bool cropHarvested{false};
};

struct DLL_API CuttingData {
  enum CL { cut, left, none };

  enum Unit { percentage, biomass, LAI };

  struct Value {
    double value{0.0};
    Unit unit{percentage};
    CL cut_or_left{cut};
  };

  std::map<int, Value> organId2cuttingSpec;
  std::map<int, double> organId2biomAfterCutting;
  std::map<int, double> organId2exportFraction;
  double cutMaxAssimilationRateFraction{1.0};
};

struct DLL_API MineralFertilizationData {
  MineralFertilizerParameters partition;
  double amount{0.0};
};

struct DLL_API NDemandFertilizationData {
  Tools::Date initialDate;
  MineralFertilizerParameters partition;
  double Ndemand{0};
  double depth{0.0};
  int stage{1};
  bool appliedFertilizer{false};
};

struct DLL_API OrganicFertilizationData {
  OrganicMatterParameters params;
  double amount{0.0};
  bool incorporation{false};
  int incorporateIntoLayerNo{1};
};

struct DLL_API TillageData {
  double depth{0.3};
};

struct DLL_API SetValueData {
  OId oid;
  json11::Json value;
  std::function<json11::Json(const monica::MonicaModel *)> getValue;
};

struct DLL_API SaveMonicaStateData {
  std::string pathToFile;
  bool toJson{false};
  int noOfPreviousDaysSerializedClimateData{-1};
};

struct DLL_API IrrigationData {
  double amount{0};
  IrrigationParameters params;
};

struct DLL_API AutomaticIrrigationData {
  Tools::Date absStartDate;
  Tools::Date absEndDate;
  bool irrigateCrop{false};
  int startStage{-1};
  int endStage{-1};
  AutomaticIrrigationParameters params;
  bool done{false};
  bool cropPlanted{false};
};

// Alternative order must exactly match WorkstepType's declaration order (workstep::type(...) below
// derives the tag via static_cast<WorkstepType>(data.index()), no separately stored tag field).
using WorkstepData = std::variant<
    SowingData, AutomaticSowingData, TransplantData, HarvestData, AutomaticHarvestData, CuttingData,
    MineralFertilizationData, NDemandFertilizationData, OrganicFertilizationData, TillageData,
    SetValueData, SaveMonicaStateData, IrrigationData, AutomaticIrrigationData>;

// NOTE: temporarily named WorkstepV2 (not Workstep) because the old, still-live OOP `class Workstep`
// in cultivation-method.h is transitively pulled in by monica-model.h (needed here for MonicaModel's
// full definition) - both can't be named `monica::Workstep` in the same translation unit. Renamed to
// `Workstep` (and WSPtrV2 -> WSPtr) at final cutover once the old class is deleted, per
// plan-cultivation-method.md.
struct DLL_API WorkstepV2 {
  Tools::Date date;
  Tools::Date absDate;
  int applyNoOfDaysAfterEvent{0};
  std::string afterEvent;
  int daysAfterEventCount{0};
  bool daysAfterEventCountActivated{false};
  bool isActive{true};
  bool runAtStartOfDay{true};
  Tools::Errors errors;

  WorkstepData data;
};

typedef std::shared_ptr<WorkstepV2> WSPtrV2;

namespace workstep {

inline WorkstepType type(const WorkstepV2 *ws) {
  return static_cast<WorkstepType>(ws->data.index());
}

inline bool isDynamicWorkstep(const WorkstepV2 *ws) { return !ws->date.isValid(); }

// Common (former base-class, non-overridden-by-default) Workstep behavior. Used both by the phase-1
// per-subtype make*Workstep(...) factories below and (later, once built) by the central dispatchers'
// default/fallback cases for the many subtypes that don't override a given piece of behavior.
DLL_API Tools::Errors mergeCommon(WorkstepV2 *ws, json11::Json j);
DLL_API bool applyCommon(WorkstepV2 *ws, MonicaModel *model);
DLL_API bool conditionCommon(WorkstepV2 *ws, MonicaModel *model);
DLL_API bool reinitCommon(WorkstepV2 *ws, Tools::Date date, bool addYear = false,
                          bool forceInitYear = false);
// setDate is inherently per-subtype dispatching (3 of the 14 subtypes override it), so unlike
// merge/apply/condition/reinit there's no single "common" body to factor out - this is already the
// full (if still incrementally-populated - see the per-step notes in plan-cultivation-method.md)
// dispatcher, not a "Common" helper.
DLL_API void setDate(WorkstepV2 *ws, Tools::Date date);

// SowingData
DLL_API Tools::Errors merge(SowingData *s, json11::Json j);
DLL_API json11::Json to_json(const SowingData *s, const WorkstepV2 *ws,
                             bool includeFullCropParameters = true);
DLL_API bool apply(SowingData *s, WorkstepV2 *ws, MonicaModel *model);

// AutomaticSowingData
DLL_API Tools::Errors merge(AutomaticSowingData *as, json11::Json j);
DLL_API json11::Json to_json(const AutomaticSowingData *as, const WorkstepV2 *ws,
                             bool includeFullCropParameters = true);
DLL_API bool apply(AutomaticSowingData *as, WorkstepV2 *ws, MonicaModel *model);
DLL_API bool condition(AutomaticSowingData *as, MonicaModel *model);
DLL_API bool reinit(AutomaticSowingData *as, WorkstepV2 *ws, Tools::Date date, bool addYear = false,
                    bool forceInitYear = false);
DLL_API std::function<double(MonicaModel *)>
registerDailyFunction(AutomaticSowingData *as, std::function<std::vector<double> &()> getDailyValues);

// TransplantData
DLL_API Tools::Errors merge(TransplantData *t, json11::Json j);
// note: unlike Sowing/AutomaticSowing, the original Transplant::to_json never embedded "date" - no
// WorkstepV2* parameter needed here, preserved as-is (straight translation).
DLL_API json11::Json to_json(const TransplantData *t, bool includeFullCropParameters = true);
DLL_API bool apply(TransplantData *t, WorkstepV2 *ws, MonicaModel *model);

} // namespace workstep

DLL_API WorkstepV2 makeSowingWorkstep(json11::Json object);
DLL_API WorkstepV2 makeAutomaticSowingWorkstep(json11::Json object);
DLL_API WorkstepV2 makeTransplantWorkstep(json11::Json object);

} // namespace monica
