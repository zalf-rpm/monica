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
  auto simPs = _monica->simulationParameters();
  auto sitePs = _monica->siteParameters();
  double currentDepthM = 0;
  std::vector<double> slds;
#ifdef SKIP_BUILD_IN_MODULES
  auto awc4 = simPs.customData["AWC"].number_value();
  soilTempComp.setcAlbedo(simPs.customData["SALB"].number_value());
  soilTempComp.setcDampingDepth(6);
  soilTempComp.setcCarbonContent(0.5);
  soilTempComp.setcFirstDayMeanTemp(simPs.customData["TAV"].number_value());
  soilTempComp.setcAverageGroundTemperature(simPs.customData["TAV"].number_value());
  soilTempComp.setcAverageBulkDensity(simPs.customData["SABDM"].number_value());
  double initialWCSum = 0;
  for (const auto& j : sitePs.initSoilProfileSpec){
    double lt_m = Tools::double_value(j["Thickness"]);
    currentDepthM += lt_m;
    slds.push_back(currentDepthM);
    Soil::SoilParameters sps;
    auto es = sps.merge(j);
    double usableFC = sps.vs_FieldCapacity - sps.vs_PermanentWiltingPoint;
    double availableFC = usableFC * awc4;
    double initialWC = availableFC + sps.vs_PermanentWiltingPoint;
    double lt_dm = lt_m * 10;
    double initialWCInLayer = initialWC * 100 * lt_dm;
    initialWCSum += initialWCInLayer;
  }
  soilTempExo.setiSoilWaterContent(initialWCSum);
#else
  soilTempComp.setcAlbedo(_monica->environmentParameters().p_Albedo);
  soilTempComp.setcDampingDepth(6);
  soilTempComp.setcCarbonContent(0.5);
  for (const auto& sl : _monica->soilColumn()){
    currentDepthM += sl.vs_LayerThickness;
    slds.push_back(currentDepthM);
  }
#endif
  soilTempComp.setcSoilLayerDepth(slds);
#endif
}

void MonicaInterface::run() {
#if SIMPLACE_SOIL_TEMPERATURE
  KJ_ASSERT(_monica != nullptr);
  auto climateData = _monica->currentStepClimateData();
  soilTempExo.setiAirTemperatureMin(climateData.at(Climate::tmin));
  soilTempExo.setiAirTemperatureMax(climateData.at(Climate::tmax));
  soilTempExo.setiGlobalSolarRadiation(climateData.at(Climate::globrad));
#ifdef SKIP_BUILD_IN_MODULES
  soilTempExo.setiRAIN(0); // so that no snowcover will build up
  soilTempExo.setiLeafAreaIndex(_simPs.customData["LAI"].number_value());
  soilTempExo.setiPotentialSoilEvaporation(climateData[Climate::et0]); //use et0 as ETPot
#else
  soilTempExo.setiRAIN(climateData.at(Climate::precip)); // so that no snowcover will build up
  if (_monica->cropGrowth()) soilTempExo.setiLeafAreaIndex(_monica->cropGrowth()->getLeafAreaIndex());
  else soilTempExo.setiLeafAreaIndex(0);
  soilTempExo.setiPotentialSoilEvaporation(_monica->soilMoisture().get_PotentialEvapotranspiration());
#endif
  soilTempExo.setiCropResidues(0);
  if(_doInit){
    soilTempComp._STMPsimCalculator.Init(soilTempState, soilTempState1, soilTempRate, soilTempAux, soilTempExo);
    soilTempComp._SnowCoverCalculator.Init(soilTempState, soilTempState1, soilTempRate, soilTempAux, soilTempExo);
    _doInit = false;
  }
#ifndef SKIP_BUILD_IN_MODULES
  double wcSum = 0;
  for (const auto& sl : _monica->soilColumn()){
    wcSum += sl.get_Vs_SoilMoisture_m3();
  }
  soilTempExo.setiSoilWaterContent(wcSum);
#endif
  soilTempComp.Calculate_Model(soilTempState, soilTempState1, soilTempRate, soilTempAux, soilTempExo);
  _monica->soilTemperatureNC().setSoilSurfaceTemperature(soilTempState.getSoilSurfaceTemperature());
  int i = 0;
  KJ_ASSERT(_monica->soilColumnNC().size() == soilTempState.getSoilTempArray().size());
  for (auto& sl : _monica->soilColumnNC()){
    sl.set_Vs_SoilTemperature(soilTempState.getSoilTempArray().at(i++));
  }
#endif
}