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
#include <kj/map.h>

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
#include "soiltransport.h"
#include "soilorganic.h"
#include "soiltemperature.h"
#include "soilmoisture.h"
#include "crop-module.h"
#include "soilcolumn.h"

namespace monica {
  
class Crop;

class MonicaModel {
public:
  explicit MonicaModel(const CentralParameterProvider& cpp);

  explicit MonicaModel(mas::schema::model::monica::MonicaModelState::Reader reader) { deserialize(reader); }

  ~MonicaModel();

  void deserialize(mas::schema::model::monica::MonicaModelState::Reader reader);

  void serialize(mas::schema::model::monica::MonicaModelState::Builder builder);

  void step();

  void generalStep();

  void cropStep();

  double CO2ForDate(double year, double julianDay, bool isLeapYear, mas::schema::climate::RCP rcp = mas::schema::climate::RCP::RCP85);
  double CO2ForDate(Tools::Date, mas::schema::climate::RCP rcp = mas::schema::climate::RCP::RCP85);
  double GroundwaterDepthForDate(double maxGroundwaterDepth, double minGroundwaterDepth, int minGroundwaterDepthMonth,
    double julianday, bool leapYear);

  void seedCrop(mas::schema::model::monica::CropSpec::Reader reader, kj::StringPtr id);
  void seedCrop(Crop* crop, kj::StringPtr id);

  bool isCropPlanted() const { return soilColumn().id2cropModules.size() > 0; }

  void harvestCurrentCrop(bool exported, const Harvest::Spec& spec, kj::StringPtr id,
    Harvest::OptCarbonManagementData optCarbMgmtData = Harvest::OptCarbonManagementData());

  void incorporateCurrentCrop(const CropModule& cropModule);

  void applyMineralFertiliser(MineralFertilizerParameters partition, double amount);

  void applyOrganicFertiliser(const OrganicMatterParameters& omps, double amountFM, bool incorporation);

  bool useNMinMineralFertilisingMethod() const { return _simPs.p_UseNMinMineralFertilisingMethod; }

  double applyMineralFertiliserViaNMinMethod(MineralFertilizerParameters partition, NMinCropParameters cropParams);

  double dailySumFertiliser() const { return _dailySumFertiliser; }

  void addDailySumFertiliser(double amount) {
    _dailySumFertiliser += amount;
    _sumFertiliser += amount;
  }

  double dailySumOrgFertiliser() const { return _dailySumOrgFertiliser; }

  void addDailySumOrgFertiliser(double amountFM, const OrganicMatterParameters& params);

  double dailySumOrganicFertilizerDM() const { return _dailySumOrganicFertilizerDM; }
  double sumOrganicFertilizerDM() const { return _sumOrganicFertilizerDM; }

  void addDailySumOrganicFertilizerDM(double amountDM) {
    _dailySumOrganicFertilizerDM += amountDM;
    _sumOrganicFertilizerDM += amountDM;
  }

  double dailySumIrrigationWater() const { return _dailySumIrrigationWater; }

  void addDailySumIrrigationWater(double amount) {
    _dailySumIrrigationWater += amount;
  }

  double sumFertiliser() const { return _sumFertiliser; }
  double sumOrgFertiliser() const { return _sumOrgFertiliser; }

  void resetFertiliserCounter() {
    _sumFertiliser = 0;
    _sumOrgFertiliser = 0;
    _sumOrganicFertilizerDM = 0;
  }

  void dailyReset();

  void applyIrrigation(double amount, double nitrateConcentration = 0, double sulfateConcentration = 0);

  void applyTillage(double depth);

  double get_AtmosphericCO2Concentration() const { return vw_AtmosphericCO2Concentration; }
  double get_AtmosphericO3Concentration() const { return vw_AtmosphericO3Concentration; }

  double get_GroundwaterDepth() const { return vs_GroundwaterDepth; }

  const SoilTemperature& soilTemperature() const { return *_soilTemperature; }
  SoilTemperature& soilTemperatureNC() { return *_soilTemperature; }

  const SoilMoisture& soilMoisture() const { return *_soilMoisture; }
  SoilMoisture& soilMoistureNC() { return *_soilMoisture; }

  const SoilOrganic& soilOrganic() const { return *_soilOrganic; }
  SoilOrganic& soilOrganicNC() { return *_soilOrganic; }

  const SoilTransport& soilTransport() const { return *_soilTransport; }
  SoilTransport& soilTransportNC() { return *_soilTransport; }

  const SoilColumn& soilColumn() const { return *_soilColumn; }
  SoilColumn& soilColumnNC() { return *_soilColumn; }

  CropModule* cropModule(kj::StringPtr cropId = nullptr);
  const CropModule* cropModule(kj::StringPtr cropId = nullptr) const;

  double netRadiation(double globrad) { return globrad * (1 - _envPs.p_Albedo); }

  const SiteParameters& siteParameters() const { return _sitePs; }
  const EnvironmentParameters& environmentParameters() const { return _envPs; }
  const CropModuleParameters& cropParameters() const { return _cropPs; }
  CropModuleParameters& cropParametersNC() { return _cropPs; }
  const SimulationParameters& simulationParameters() const { return _simPs; }
  SimulationParameters& simulationParametersNC() { return _simPs; }

  Tools::Date currentStepDate() const { return _currentStepDate; }
  void setCurrentStepDate(Tools::Date d) { _currentStepDate = d; }

  const std::map<Climate::ACD, double>& currentStepClimateData() const { return _climateData.back(); }
  void setCurrentStepClimateData(const std::map<Climate::ACD, double>& cd) { _climateData.push_back(cd); }

  const std::vector<std::map<Climate::ACD, double>>& climateData() const { return _climateData; }

  void addEvent(std::string e) { _currentEvents.insert(e); }
  void clearEvents();
  const std::set<std::string>& currentEvents() const { return _currentEvents; }
  const std::set<std::string>& previousDaysEvents() const { return _previousDaysEvents; }

  int cultivationMethodCount() const { return _cultivationMethodCount; }

  double optCarbonExportedResidues() const { return _optCarbonExportedResidues; }
  double optCarbonReturnedResidues() const { return _optCarbonReturnedResidues; }
  double humusBalanceCarryOver() const { return _humusBalanceCarryOver; }

  Intercropping& intercropping() { return _intercropping; }
  void setIntercropping(Intercropping& ic) { _intercropping = ic; }

  void setOtherCropHeightAndLAIt(double cropHeight, double lait);

private:
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

  //! store applied fertiliser during one production process
  double _sumFertiliser{ 0.0 }; //mineral N
  double _sumOrgFertiliser{ 0.0 }; //organic N

  //! stores the daily sum of applied fertiliser
  double _dailySumFertiliser{ 0.0 }; //mineral N
  double _dailySumOrgFertiliser{ 0.0 }; //organic N

  double _dailySumOrganicFertilizerDM{ 0.0 };
  double _sumOrganicFertilizerDM{ 0.0 };

  double _humusBalanceCarryOver{ 0.0 };

  double _dailySumIrrigationWater{ 0.0 };

  double _optCarbonExportedResidues{ 0.0 };
  double _optCarbonReturnedResidues{ 0.0 };

  Tools::Date _currentStepDate;
  std::vector<std::map<Climate::ACD, double>> _climateData;
  std::set<std::string> _currentEvents;
  std::set<std::string> _previousDaysEvents;

  kj::Vector<kj::String> _clearCropWithIdUponNextDay;

  double vw_AtmosphericCO2Concentration{ 0.0 };
  double vw_AtmosphericO3Concentration{ 0.0 };
  double vs_GroundwaterDepth{ 0.0 };

  int _cultivationMethodCount{ 0 };

  bool initialRelativePresence{false};
  std::ofstream _suitabilitylogFile;
  Intercropping _intercropping;

  //public:
  //  uint critPos{ 0 };
  //  uint cmitPos{ 0 };
};

} // namespace monica
