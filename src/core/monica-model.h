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
#include "soiltransport.h"
#include "soilorganic.h"
#include "soiltemperature.h"
#include "soilmoisture.h"
#include "crop-module.h"
#include "soilcolumn.h"

#ifdef AMEI
#include "SoilTemperatureCompComponent.h"
#include "DSSAT_ST_standalone/STEMP_Component.h"
#include "DSSAT_EPICST_standalone/STEMP_EPIC_Component.h"
#include "Simplace_Soil_Temperature/SoilTemperatureComponent.h"
#endif


namespace monica {
  
class Crop;

class MonicaModel {
public:
  MonicaModel(const CentralParameterProvider& cpp);

  MonicaModel(mas::schema::model::monica::MonicaModelState::Reader reader) { deserialize(reader); }

  void deserialize(mas::schema::model::monica::MonicaModelState::Reader reader);

  void serialize(mas::schema::model::monica::MonicaModelState::Builder builder);

  void initComponents(const CentralParameterProvider &cpp);

  void step();

  void generalStep();

  void cropStep();

  double CO2ForDate(double year, double julianDay, bool isLeapYear, mas::schema::climate::RCP rcp = mas::schema::climate::RCP::RCP85);
  double CO2ForDate(Tools::Date, mas::schema::climate::RCP rcp = mas::schema::climate::RCP::RCP85);
  double GroundwaterDepthForDate(double maxGroundwaterDepth,
    double minGroundwaterDepth,
    int minGroundwaterDepthMonth,
    double julianday,
    bool leapYear);

  void seedCrop(Crop* crop);

  bool isCropPlanted() const { return _currentCropModule; }

  void harvestCurrentCrop(bool exported, Harvest::Spec spec, Harvest::OptCarbonManagementData optCarbMgmtData = Harvest::OptCarbonManagementData());

  void incorporateCurrentCrop();

  void applyMineralFertiliser(MineralFertilizerParameters partition,
    double amount);

  void applyOrganicFertiliser(const OrganicMatterParameters& omps,
    double amountFM,
    bool incorporation);

  bool useNMinMineralFertilisingMethod() const {
    return _simPs.p_UseNMinMineralFertilisingMethod;
  }

  double applyMineralFertiliserViaNMinMethod
  (MineralFertilizerParameters partition, NMinCropParameters cropParams);

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

  void applyIrrigation(double amount,
    double nitrateConcentration = 0,
    double sulfateConcentration = 0);

  void applyTillage(double depth);

  double get_AtmosphericCO2Concentration() const {
    return vw_AtmosphericCO2Concentration;
  }
  double get_AtmosphericO3Concentration() const {
    return vw_AtmosphericO3Concentration;
  }

  double get_GroundwaterDepth() const { return vs_GroundwaterDepth; }

  double avgCorg(double depth_m) const;
  double mean90cmWaterContent() const;
  double meanWaterContent(int layer, int number_of_layers) const;
  double sumNmin(double depth_m) const;
  double groundWaterRecharge() const;
  double nLeaching() const;
  double sumSoilTemperature(int layers) const;
  double sumNO3AtDay(double depth) const;
  double maxSnowDepth() const;
  double getAccumulatedSnowDepth() const;
  double getAccumulatedFrostDepth() const;
  double avg30cmSoilTemperature() const;
  double avgSoilMoisture(size_t startLayer, size_t endLayerInclusive) const;
  double avgCapillaryRise(int start_layer, int end_layer) const;
  double avgPercolationRate(int start_layer, int end_layer) const;
  double sumSurfaceRunOff() const;
  double surfaceRunoff() const;
  double getEvapotranspiration() const;
  double getTranspiration() const;
  double getEvaporation() const;
  double get_sum30cmSMB_CO2EvolutionRate() const;
  double getNH3Volatilised() const;
  double getSumNH3Volatilised() const;
  double getsum30cmActDenitrificationRate() const;
  double getETa() const;

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

  CropModule* cropGrowth() { return _currentCropModule.get(); }
  const CropModule* cropGrowth() const { return _currentCropModule.get(); }

  double netRadiation(double globrad) { return globrad * (1 - _envPs.p_Albedo); }

  int daysWithCrop() const { return p_daysWithCrop; }
  double getAccumulatedNStress() const { return p_accuNStress; }
  double getAccumulatedWaterStress() const { return p_accuWaterStress; }
  double getAccumulatedHeatStress() const { return p_accuHeatStress; }
  double getAccumulatedOxygenStress() const { return p_accuOxygenStress; }

  const SiteParameters& siteParameters() const { return _sitePs; }
  const EnvironmentParameters& environmentParameters() const { return _envPs; }
  const CropModuleParameters& cropParameters() const { return _cropPs; }
  CropModuleParameters& cropParametersNC() { return _cropPs; }
  const SimulationParameters& simulationParameters() const { return _simPs; }
  SimulationParameters& simulationParametersNC() { return _simPs; }

  Tools::Date currentStepDate() const { return _currentStepDate; }
  void setCurrentStepDate(Tools::Date d) { _currentStepDate = d; }

  const std::map<Climate::ACD, double>& currentStepClimateData() const {
    return _climateData.back();
  }
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

#ifdef AMEI
  struct Monica_SoilTemp_T {
      Monica_SoilTemp::SoilTemperatureCompComponent soilTempComp;
      Monica_SoilTemp::SoilTemperatureCompState soilTempState;
      Monica_SoilTemp::SoilTemperatureCompState soilTempState1;
      Monica_SoilTemp::SoilTemperatureCompExogenous soilTempExo;
      Monica_SoilTemp::SoilTemperatureCompRate soilTempRate;
      Monica_SoilTemp::SoilTemperatureCompAuxiliary soilTempAux;
      bool doInit{true};
  };
  kj::Own<Monica_SoilTemp_T> _instance_Monica_SoilTemp;

  struct DSSAT_ST_standalone_T {
      DSSAT_ST_standalone::STEMP_Component soilTempComp;
      DSSAT_ST_standalone::STEMP_State soilTempState;
      DSSAT_ST_standalone::STEMP_State soilTempState1;
      DSSAT_ST_standalone::STEMP_Exogenous soilTempExo;
      DSSAT_ST_standalone::STEMP_Rate soilTempRate;
      DSSAT_ST_standalone::STEMP_Auxiliary soilTempAux;
      bool doInit{true};
  };
  kj::Own<DSSAT_ST_standalone_T> _instance_DSSAT_ST_standalone;

  struct DSSAT_EPICST_standalone_T {
      DSSAT_EPICST_standalone::STEMP_EPIC_Component soilTempComp;
      DSSAT_EPICST_standalone::STEMP_EPIC_State soilTempState;
      DSSAT_EPICST_standalone::STEMP_EPIC_State soilTempState1;
      DSSAT_EPICST_standalone::STEMP_EPIC_Exogenous soilTempExo;
      DSSAT_EPICST_standalone::STEMP_EPIC_Rate soilTempRate;
      DSSAT_EPICST_standalone::STEMP_EPIC_Auxiliary soilTempAux;
      bool doInit{true};
  };
  kj::Own<DSSAT_EPICST_standalone_T> _instance_DSSAT_EPICST_standalone;

  struct Simplace_Soil_Temperature_T {
      Simplace_Soil_Temperature::SoilTemperatureComponent soilTempComp;
      Simplace_Soil_Temperature::SoilTemperatureState soilTempState;
      Simplace_Soil_Temperature::SoilTemperatureState soilTempState1;
      Simplace_Soil_Temperature::SoilTemperatureExogenous soilTempExo;
      Simplace_Soil_Temperature::SoilTemperatureRate soilTempRate;
      Simplace_Soil_Temperature::SoilTemperatureAuxiliary soilTempAux;
      bool doInit{true};
  };
  kj::Own<Simplace_Soil_Temperature_T> _instance_Simplace_Soil_Temperature;

  kj::Function<double(int)> _getSoilTemperatureAtDepthCm;
  kj::Function<double()> _getSoilSurfaceTemperature;
#endif

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
  kj::Own<CropModule> _currentCropModule; //!< crop code for possibly planted crop



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

  bool _clearCropUponNextDay{ false };

  int p_daysWithCrop{ 0 };
  double p_accuNStress{ 0.0 };
  double p_accuWaterStress{ 0.0 };
  double p_accuHeatStress{ 0.0 };
  double p_accuOxygenStress{ 0.0 };

  double vw_AtmosphericCO2Concentration{ 0.0 };
  double vw_AtmosphericO3Concentration{ 0.0 };
  double vs_GroundwaterDepth{ 0.0 };

  int _cultivationMethodCount{ 0 };

  Intercropping _intercropping;

  //public:
  //  uint critPos{ 0 };
  //  uint cmitPos{ 0 };
};

} // namespace monica
