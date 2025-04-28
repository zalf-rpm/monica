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
      KJ_ASSERT(_monica != nullptr);
  auto simPs = _monica->simulationParameters();
  auto sitePs = _monica->siteParameters();
#ifdef AMEI_SENSITIVITY_ANALYSIS
  auto awc = simPs.customData["AWC"].number_value();
  _soilTempComp.setSoilProfileDepth(simPs.customData["SLDP"].number_value() / 100.0);  // cm -> m
  std::vector<double> layerThicknessM;
  std::vector<double> bds;
  std::vector<double> sws;
  for (const auto& j : sitePs.initSoilProfileSpec){
    layerThicknessM.push_back(Tools::double_value(j["Thickness"]));
    Soil::SoilParameters sps;
    auto es = sps.merge(j);
    sws.push_back(sps.vs_PermanentWiltingPoint + awc*(sps.vs_FieldCapacity - sps.vs_PermanentWiltingPoint));
    bds.push_back(sps.vs_SoilBulkDensity() / 1000.0);  // kg/m3 -> t/m3
  }
  _soilTempExo.VolumetricWaterContent = sws;
  _soilTempComp.setLayerThickness(layerThicknessM);
  _soilTempComp.setBulkDensity(bds);
#else
  std::vector<double> layerThicknessM;
  std::vector<double> bds;
  double profileDepth = 0;
  for (const auto& sl : _monica->soilColumn()){
    layerThicknessM.push_back(sl.vs_LayerThickness);
    profileDepth += sl.vs_LayerThickness;
    bds.push_back(sl.vs_SoilBulkDensity() / 1000.0);  // kg/m3 -> g/cm3
  }
  _soilTempComp.setLayerThickness(layerThicknessM);
  _soilTempComp.setBulkDensity(bds);
  _soilTempComp.setSoilProfileDepth(profileDepth);
#endif
  _soilTempComp.setLagCoefficient(0.8);
}

void MonicaInterface::run() {
      KJ_ASSERT(_monica != nullptr);
  auto climateData = _monica->currentStepClimateData();
  _soilTempExo.AirTemperatureMinimum = climateData.at(Climate::tmin);
  _soilTempExo.AirTemperatureMaximum = climateData.at(Climate::tmax);
  _soilTempExo.DayLength = climateData[Climate::x4];
  _soilTempExo.GlobalSolarRadiation = climateData.at(Climate::globrad);
#ifdef AMEI_SENSITIVITY_ANALYSIS
  _soilTempExo.AboveGroundBiomass = _monica->simulationParameters().customData["CWAD"].number_value();
  _soilTempComp.setAirTemperatureAnnualAverage(_monica->simulationParameters().customData["TAV"].number_value());
#else
  auto tampNtav = _monica->dssatTAMPandTAV();
  _soilTempComp.setAirTemperatureAnnualAverage(tampNtav.second);
  if (_monica->cropGrowth()) _soilTempExo.AboveGroundBiomass = _monica->cropGrowth()->get_AbovegroundBiomass();
  else _soilTempExo.AboveGroundBiomass = 0;
#endif
  if(_doInit){
    _soilTempComp._SoilTemperatureSWAT.Init(_soilTempState, _soilTempState1, _soilTempRate, _soilTempAux, _soilTempExo);
    _doInit = false;
  }
#ifndef AMEI_SENSITIVITY_ANALYSIS
  std::vector<double> sws;
  for (const auto& sl : _monica->soilColumn()){
    sws.push_back(sl.get_Vs_SoilMoisture_m3() - sl.vs_PermanentWiltingPoint());
  }
  _soilTempExo.VolumetricWaterContent = sws;
#endif
  _soilTempComp.Calculate_Model(_soilTempState, _soilTempState1, _soilTempRate, _soilTempAux, _soilTempExo);
#ifndef AMEI_SENSITIVITY_ANALYSIS
  _monica->soilTemperatureNC().setSoilSurfaceTemperature(_soilTempAux.SurfaceSoilTemperature);
  int i = 0;
  KJ_ASSERT(_monica->soilColumnNC().size() == _soilTempState.SoilTemperatureByLayers.size());
  for (auto& sl : _monica->soilColumnNC()){
    sl.set_Vs_SoilTemperature(_soilTempState.SoilTemperatureByLayers.at(i++));
  }
#endif
}