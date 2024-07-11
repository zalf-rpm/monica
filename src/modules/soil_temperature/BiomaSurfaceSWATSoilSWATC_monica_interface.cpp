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

#include "BiomaSurfaceSWATSoilSWATC_monica_interface.h"

#include <vector>
#include <kj/debug.h>

#include "core/monica-model.h"

using namespace BiomaSurfaceSWATSoilSWATC;

MonicaInterface::MonicaInterface(monica::MonicaModel *monica) : _monica(monica) {}

void MonicaInterface::init(const monica::CentralParameterProvider &cpp) {
#if BIOMASURFACESWATSOILSWATC
      KJ_ASSERT(_monica != nullptr);
  auto simPs = _monica->simulationParameters();
  auto sitePs = _monica->siteParameters();
  auto awc6 = simPs.customData["AWC"].number_value();
  // shouldn't be an exogenous variable
  soilTempAux.setAboveGroundBiomass(0);
  std::vector<double> layerThicknessM2;
  std::vector<double> bds5;
  std::vector<double> sws4;
  for (const auto& j : sitePs.initSoilProfileSpec){
    layerThicknessM2.push_back(Tools::double_value(j["Thickness"]));
    sws4.push_back(awc6);
    Soil::SoilParameters sps;
    auto es = sps.merge(j);
    bds5.push_back(sps.vs_SoilBulkDensity() / 1000.0);  // kg/m3 -> g/cm3
  }
  soilTempExo.setVolumetricWaterContent(sws4);
  soilTempExo.setAlbedo(simPs.customData["SALB"].number_value());
  soilTempComp.setLayerThickness(layerThicknessM2);
  soilTempComp.setBulkDensity(bds5);
  soilTempComp.setLagCoefficient(0.8);
  soilTempComp.setAirTemperatureAnnualAverage(simPs.customData["TAV"].number_value());
  soilTempComp.setSoilProfileDepth(simPs.customData["SLDP"].number_value() / 100.0);  // cm -> m
#endif
}

void MonicaInterface::run() {
#if BIOMASURFACESWATSOILSWATC
      KJ_ASSERT(_monica != nullptr);
  auto climateData = _monica->currentStepClimateData();
  soilTempExo.setAirTemperatureMinimum(climateData.at(Climate::tmin));
  soilTempExo.setAirTemperatureMaximum(climateData.at(Climate::tmax));
  soilTempExo.setGlobalSolarRadiation(climateData.at(Climate::globrad));
  soilTempExo.setWaterEquivalentOfSnowPack(climateData[Climate::precipOrig]);
  if(_doInit){
    soilTempComp._SoilTemperatureSWAT.Init(soilTempState, soilTempState1, soilTempRate, soilTempAux, soilTempExo);
    _doInit = false;
  }
  soilTempComp.Calculate_Model(soilTempState, soilTempState1, soilTempRate, soilTempAux, soilTempExo);
  _monica->soilTemperatureNC().setSoilSurfaceTemperature(soilTempAux.getSurfaceSoilTemperature());
#endif
}