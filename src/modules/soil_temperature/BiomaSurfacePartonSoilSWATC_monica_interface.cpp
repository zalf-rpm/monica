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
#ifdef CPP2
  _soilTempExo.VolumetricWaterContent = sws;
#else
  _soilTempExo.setVolumetricWaterContent(sws);
#endif
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
  soilTempComp.setLayerThickness(layerThicknessM);
  soilTempComp.setBulkDensity(bds);
  soilTempComp.setSoilProfileDepth(profileDepth);
#endif
  _soilTempComp.setLagCoefficient(0.8);
#endif
}

void MonicaInterface::run() {
#if BIOMASURFACEPARTONSOILSWATC
      KJ_ASSERT(_monica != nullptr);
  auto climateData = _monica->currentStepClimateData();
#ifdef CPP2
  _soilTempExo.AirTemperatureMinimum = climateData.at(Climate::tmin);
  _soilTempExo.AirTemperatureMaximum = climateData.at(Climate::tmax);
  _soilTempExo.DayLength = climateData[Climate::sunhours];
  _soilTempExo.GlobalSolarRadiation = climateData.at(Climate::globrad);
#else
  _soilTempExo.setAirTemperatureMinimum(climateData.at(Climate::tmin));
  _soilTempExo.setAirTemperatureMaximum(climateData.at(Climate::tmax));
  _soilTempExo.setDayLength(climateData[Climate::sunhours]);
  _soilTempExo.setGlobalSolarRadiation(climateData.at(Climate::globrad));
#endif
#ifdef AMEI_SENSITIVITY_ANALYSIS
#ifdef CPP2
  _soilTempExo.AboveGroundBiomass = _monica->simulationParameters().customData["CWAD"].number_value();
#else
  _soilTempExo.setAboveGroundBiomass(_monica->simulationParameters().customData["CWAD"].number_value());
#endif
  _soilTempComp.setAirTemperatureAnnualAverage(_monica->simulationParameters().customData["TAV"].number_value());
#else
  auto tampNtav = _monica->dssatTAMPandTAV();
  soilTempComp.setAirTemperatureAnnualAverage(tampNtav.first);
  if (_monica->cropGrowth()) soilTempExo.setAboveGroundBiomass(_monica->cropGrowth()->get_AbovegroundBiomass());
  else soilTempExo.setAboveGroundBiomass(0);
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
  soilTempExo.setVolumetricWaterContent(sws);
#endif
  _soilTempComp.Calculate_Model(_soilTempState, _soilTempState1, _soilTempRate, _soilTempAux, _soilTempExo);
#ifndef AMEI_SENSITIVITY_ANALYSIS
  _monica->soilTemperatureNC().setSoilSurfaceTemperature(soilTempAux.getSurfaceSoilTemperature());
  int i = 0;
  KJ_ASSERT(_monica->soilColumnNC().size() == soilTempState.getSoilTemperatureByLayers().size());
  for (auto& sl : _monica->soilColumnNC()){
    sl.set_Vs_SoilTemperature(soilTempState.getSoilTemperatureByLayers().at(i++));
  }
#endif
#endif
}