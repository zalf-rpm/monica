/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
Authors:
Michael Berg <michael.berg@zalf.de>

Maintainers:
Currently maintained by the authors.

This file is part of the MONICA model.
Copyright (C) Leibniz Centre for Agricultural Landscape Research (ZALF)
*/

#include "Simplace_Soil_Temperature_monica_interface.h"

#include <vector>
#include <kj/debug.h>

#include "core/monica-model.h"

using namespace Simplace_Soil_Temperature;

MonicaInterface::MonicaInterface(monica::MonicaModel *monica) : _monica(monica) {}

void MonicaInterface::init(const monica::CentralParameterProvider &cpp) {
#if SIMPLACE_SOIL_TEMPERATURE
  KJ_ASSERT(_monica != nullptr);
  const auto simPs = _monica->simulationParameters();
  const auto sitePs = _monica->siteParameters();
  double currentDepthM = 0;
  std::vector<double> slds;
#ifdef AMEI_SENSITIVITY_ANALYSIS
  // these are the default values
  _soilTempComp._SnowCoverCalculator.setcSnowIsolationFactorA(2.3);
  _soilTempComp._SnowCoverCalculator.setcSnowIsolationFactorB(0.22);
  // from Andi?
  //_soilTempComp._SnowCoverCalculator.setcSnowIsolationFactorA(0.47);
  //_soilTempComp._SnowCoverCalculator.setcSnowIsolationFactorB(0.62);
  _soilTempComp.setcInitialAgeOfSnow(0);
  //AgeOfSnow will in snow calculator init function be set to cInitialAgeOfSnow
  _soilTempComp.setcInitialSnowWaterContent(0);
  //SnowWaterContent will in snow calculator init function be set to cInitialSnowWaterContent
  //rSoilTempArrayRate will be initialized in sim calculator (to 0s)
  //pSoilLayerDepth will be initialized in sim calculator

  const auto awc = simPs.customData["AWC"].number_value();
  _soilTempComp.setcAlbedo(simPs.customData["SALB"].number_value());
  //pInternalAlbedo gets set by init function of snow calculator
  //SoilSurfaceTemperature is a state and will also be initialized in snow calculator
  _soilTempComp.setcDampingDepth(6);
  _soilTempComp.setcCarbonContent(0.5);
  _soilTempComp.setcFirstDayMeanTemp(simPs.customData["TAV"].number_value());
  _soilTempComp.setcAverageGroundTemperature(simPs.customData["TAV"].number_value());
  _soilTempComp.setcAverageBulkDensity(simPs.customData["SABDM"].number_value());
  double mmInitialWCSum = 0;
  double initialWCSum2 = 0;
  //bool gotSLOCOfFirstLayer = false;
  for (const auto& j : sitePs.initSoilProfileSpec){
    const double lt_m = Tools::double_value(j["Thickness"]);
    currentDepthM += lt_m;
    slds.push_back(currentDepthM);
    Soil::SoilParameters sps;
    auto es = sps.merge(j);
    //if (!gotSLOCOfFirstLayer){
    //  _soilTempComp.setcCarbonContent(sps.vs_SoilOrganicCarbon()*100);
    //  gotSLOCOfFirstLayer = true;
    //}
    const double usableFC = sps.vs_FieldCapacity - sps.vs_PermanentWiltingPoint;
    const double availableFC = usableFC * awc;
    const double initialWC = availableFC + sps.vs_PermanentWiltingPoint;
    const double lt_dm = lt_m * 10;
    const double mmInitialWCInLayer = initialWC * 100 * lt_dm;
    mmInitialWCSum += mmInitialWCInLayer;
  }
  _soilTempExo.setiSoilWaterContent(mmInitialWCSum);
#else
  soilTempComp.setcAlbedo(_monica->environmentParameters().p_Albedo);
  soilTempComp.setcDampingDepth(6); // is also default
  double abd = 0;
  bool firstLayer = false;
  for (const auto& sl : _monica->soilColumn()){
    currentDepthM += sl.vs_LayerThickness;
    slds.push_back(currentDepthM);
    abd += sl.vs_SoilBulkDensity() / 1000.0; // kg/m3 -> t/m3
    if (!firstLayer){
      soilTempComp.setcCarbonContent(sl.vs_SoilOrganicCarbon()*100.0);
      firstLayer = true;
    }
  }
  soilTempComp.setcAverageBulkDensity(abd);
#endif
  _soilTempComp.setcSoilLayerDepth(slds);
#endif
}

void MonicaInterface::run() {
#if SIMPLACE_SOIL_TEMPERATURE
  KJ_ASSERT(_monica != nullptr);
  auto climateData = _monica->currentStepClimateData();
  _soilTempExo.setiAirTemperatureMin(climateData.at(Climate::tmin));
  _soilTempExo.setiAirTemperatureMax(climateData.at(Climate::tmax));
  _soilTempExo.setiGlobalSolarRadiation(climateData.at(Climate::globrad));
#ifdef AMEI_SENSITIVITY_ANALYSIS
  //_soilTempExo.setiRAIN(climateData.at(Climate::precip));
  _soilTempExo.setiRAIN(0);
  _soilTempComp.setcAverageGroundTemperature(_monica->simulationParameters().customData["TAV"].number_value());
  _soilTempComp.setcFirstDayMeanTemp(_monica->simulationParameters().customData["TAV"].number_value());
  _soilTempExo.setiLeafAreaIndex(_monica->simulationParameters().customData["LAI"].number_value());
  _soilTempExo.setiPotentialSoilEvaporation(climateData[Climate::et0]); //use et0 as ETPot
#else
  soilTempExo.setiRAIN(climateData.at(Climate::precip)); // so that no snowcover will build up
  if (_monica->cropGrowth()) soilTempExo.setiLeafAreaIndex(_monica->cropGrowth()->getLeafAreaIndex());
  else soilTempExo.setiLeafAreaIndex(0);
  soilTempExo.setiPotentialSoilEvaporation(_monica->soilMoisture().get_PotentialEvapotranspiration());
  double wcSum = 0;
  for (const auto& sl : _monica->soilColumn()){
    wcSum += sl.get_Vs_SoilMoisture_m3();
  }
  soilTempExo.setiSoilWaterContent(wcSum);
#endif
  _soilTempExo.setiCropResidues(0);
  _soilTempAux.setiSoilTempArray(_soilTempState.getSoilTempArray()); // set last days temperatures array
  if(_doInit){
    _soilTempExo.setiTempMin(climateData.at(Climate::tmin));
    _soilTempExo.setiTempMax(climateData.at(Climate::tmax));
    _soilTempExo.setiRadiation(climateData.at(Climate::globrad));
#ifndef AMEI_SENSITIVITY_ANALYSIS
    auto [fst, snd] = _monica->dssatTAMPandTAV();
    _soilTempComp.setcAverageGroundTemperature(fst);
    _soilTempComp.setcFirstDayMeanTemp(climateData.at(Climate::tavg));
#endif
    _soilTempComp._SnowCoverCalculator.Init(_soilTempState, _soilTempState1, _soilTempRate, _soilTempAux, _soilTempExo);
    _soilTempComp._STMPsimCalculator.Init(_soilTempState, _soilTempState1, _soilTempRate, _soilTempAux, _soilTempExo);
    _doInit = false;
  }
  _soilTempComp.Calculate_Model(_soilTempState, _soilTempState1, _soilTempRate, _soilTempAux, _soilTempExo);
#ifndef AMEI_SENSITIVITY_ANALYSIS
  _monica->soilTemperatureNC().setSoilSurfaceTemperature(soilTempState.getSoilSurfaceTemperature());
  int i = 0;
  KJ_ASSERT(soilTempState.getSoilTempArray().size() >= _monica->soilColumnNC().size());
  for (auto& sl : _monica->soilColumnNC()){
    sl.set_Vs_SoilTemperature(soilTempState.getSoilTempArray().at(i++));
  }
#endif
#endif
}