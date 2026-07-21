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

#include <string>
#include <vector>
#include <list>
#include <utility>
#include <sstream>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <queue>
#include <set>

#include <kj/memory.h>
#include <kj/async-io.h>
#include "model/monica/monica_state.capnp.h"
#include "climate.capnp.h"

#include "climate/climate-common.h"
#include "tools/date.h"
#include "tools/datastructures.h"
#include "monica-parameters.h"
#include "tools/helper.h"
#include "soil/soil.h"
#include "soil/constants.h"
#include "../run/cultivation-method.h"
#include "soiltransport_simple.h"
#include "soilorganic_simple.h"
#include "soiltemperature_simple.h"
#include "soilmoisture_simple.h"
#include "crop-module_simple.h"
#include "soilcolumn_simple.h"

namespace monica {
class Crop;

struct MonicaModel {
public:
  SiteParameters _sitePs;
  EnvironmentParameters _envPs;
  CropModuleParameters _cropPs;
  SimulationParameters _simPs;
  MeasuredGroundwaterTableInformation _groundwaterInformation;

  kj::Own<SoilColumn> _soilColumn; //!< main soil data structure
  kj::Own<SoilTemperature> _soilTemperature; //!< temperature code
  kj::Own<SoilMoisture> _soilMoisture; //!< moisture code
  kj::Own<SoilOrganic> _soilOrganic; //!< organic code
  kj::Own<SoilTransport> _soilTransport; //!< transport code
  kj::Own<CropModule> _currentCropModule; //!< crop code for possibly planted crop

  //! store applied fertiliser during one production process
  double _sumFertiliser{0.0}; //mineral N
  double _sumOrgFertiliser{0.0}; //organic N

  //! stores the daily sum of applied fertiliser
  double _dailySumFertiliser{0.0}; //mineral N
  double _dailySumOrgFertiliser{0.0}; //organic N

  double _dailySumOrganicFertilizerDM{0.0};
  double _sumOrganicFertilizerDM{0.0};

  double _humusBalanceCarryOver{0.0};

  double _dailySumIrrigationWater{0.0};

  double _optCarbonExportedResidues{0.0};
  double _optCarbonReturnedResidues{0.0};

  Tools::Date _currentStepDate;
  std::vector<std::map<Climate::ACD, double>> _climateData;
  std::set<std::string> _currentEvents;
  std::set<std::string> _previousDaysEvents;

  bool _clearCropUponNextDay{false};

  int p_daysWithCrop{0};
  double p_accuNStress{0.0};
  double p_accuWaterStress{0.0};
  double p_accuHeatStress{0.0};
  double p_accuOxygenStress{0.0};

  double vw_AtmosphericCO2Concentration{0.0};
  double vw_AtmosphericO3Concentration{0.0};
  double vs_GroundwaterDepth{0.0};

  int _cultivationMethodCount{0};

  Intercropping _intercropping;

  //public:
  //  uint critPos{ 0 };
  //  uint cmitPos{ 0 };
};

kj::Own<MonicaModel> makeMonicaModel(const CentralParameterProvider& cpp);
kj::Own<MonicaModel> makeMonicaModel(mas::schema::model::monica::MonicaModelState::Reader reader);
namespace monicamodel {
void deserialize(MonicaModel* model, mas::schema::model::monica::MonicaModelState::Reader reader);
void serialize(MonicaModel* model, mas::schema::model::monica::MonicaModelState::Builder builder);
void step(MonicaModel* model);
void generalStep(MonicaModel* model);
void cropStep(MonicaModel* model);
double CO2ForDate(double year,
                  double julianDay,
                  bool isLeapYear,
                  mas::schema::climate::RCP rcp = mas::schema::climate::RCP::RCP85);
double CO2ForDate(const Tools::Date& d, mas::schema::climate::RCP rcp = mas::schema::climate::RCP::RCP85);
double groundwaterDepthForDate(double maxGroundwaterDepth,
                               double minGroundwaterDepth,
                               int minGroundwaterDepthMonth,
                               double julianDay,
                               bool isLeapYear);
void seedCrop(MonicaModel* model, mas::schema::model::monica::CropSpec::Reader reader);
void seedCrop(MonicaModel* model, Crop* crop);
void harvestCurrentCrop(MonicaModel* model,
                        bool exported,
                        const Harvest::Spec& spec,
                        Harvest::OptCarbonManagementData optCarbMgmtData = Harvest::OptCarbonManagementData(),
                        int incorporateIntoLayerIndex = 0);
void incorporateCurrentCrop(MonicaModel* model);
void applyMineralFertiliser(MonicaModel* model, MineralFertilizerParameters partition, double amount);
void applyOrganicFertiliser(MonicaModel* model,
                            const OrganicMatterParameters& omps,
                            double amountFM,
                            bool incorporation,
                            int incorporateIntoLayerIndex = 0);
double applyMineralFertiliserViaNMinMethod(MonicaModel* model,
                                           MineralFertilizerParameters partition,
                                           NMinCropParameters cropParams);
void addDailySumOrgFertiliser(MonicaModel* model, double amountFM, const OrganicMatterParameters& params);
void dailyReset(MonicaModel* model);
void applyIrrigation(MonicaModel* model,
                     double amount,
                     double nitrateConcentration = 0,
                     double sulfateConcentration = 0);
void applyTillage(MonicaModel* model, double depth);
void clearEvents(MonicaModel* model);
void setOtherCropHeightAndLAIt(MonicaModel* model, double cropHeight, double lait);
void addDailySumFertiliser(MonicaModel* model, double amount);
void addDailySumOrganicFertilizerDM(MonicaModel* model, double amountDM);
void addDailySumIrrigationWater(MonicaModel* model, double amount);
void resetFertiliserCounter(MonicaModel* model);
} // namespace monicamodel
} // namespace monica
