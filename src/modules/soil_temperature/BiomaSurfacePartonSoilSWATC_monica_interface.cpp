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

#include "BiomaSurfacePartonSoilSWATC_monica_interface.h"

#include <vector>
#include <kj/debug.h>

#include "core/monica-model.h"

using namespace BiomaSurfacePartonSoilSWATC;

MonicaInterface::MonicaInterface(monica::MonicaModel *monica) : _monica(monica) {}

void MonicaInterface::init(const monica::CentralParameterProvider &cpp) {
#if BIOMASURFACEPARTONSOILSWATC
      KJ_ASSERT(_monica != nullptr);
  auto simPs = _monica->simulationParameters();
  auto sitePs = _monica->siteParameters();
  auto awc5 = simPs.customData["AWC"].number_value();
  // shouldn't be an exogenous variable
  std::vector<double> layerThicknessM;
  std::vector<double> bds4;
  std::vector<double> sws3;
  for (const auto& j : sitePs.initSoilProfileSpec){
    layerThicknessM.push_back(Tools::double_value(j["Thickness"]));
    sws3.push_back(awc5);
    Soil::SoilParameters sps;
    auto es = sps.merge(j);
    bds4.push_back(sps.vs_SoilBulkDensity() / 1000.0);  // kg/m3 -> g/cm3
  }
  soilTempExo.setVolumetricWaterContent(sws3);
  soilTempComp.setLayerThickness(layerThicknessM);
  soilTempComp.setBulkDensity(bds4);
  soilTempComp.setLagCoefficient(0.8);
  soilTempComp.setAirTemperatureAnnualAverage(simPs.customData["TAV"].number_value());
  soilTempComp.setSoilProfileDepth(simPs.customData["SLDP"].number_value() / 100.0);  // cm -> m
#endif
}

void MonicaInterface::run() {
#if BIOMASURFACEPARTONSOILSWATC
      KJ_ASSERT(_monica != nullptr);
  auto climateData = _monica->currentStepClimateData();
  soilTempExo.setAboveGroundBiomass(0);
  soilTempExo.setAirTemperatureMinimum(climateData.at(Climate::tmin));
  soilTempExo.setAirTemperatureMaximum(climateData.at(Climate::tmax));
  soilTempExo.setDayLength(climateData[Climate::sunhours]);
  soilTempExo.setGlobalSolarRadiation(climateData.at(Climate::globrad));
  if(_doInit){
    soilTempComp._SoilTemperatureSWAT.Init(soilTempState, soilTempState1, soilTempRate, soilTempAux, soilTempExo);
    _doInit = false;
  }
  soilTempComp.Calculate_Model(soilTempState, soilTempState1, soilTempRate, soilTempAux, soilTempExo);
  _monica->soilTemperatureNC().setSoilSurfaceTemperature(soilTempAux.getSurfaceSoilTemperature());
#endif
}