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

#include "ApsimCampbell_monica_interface.h"

#include <vector>
#include <kj/debug.h>

#include "core/monica-model.h"

using namespace ApsimCampbell;

MonicaInterface::MonicaInterface(monica::MonicaModel *monica) : _monica(monica) {}

void MonicaInterface::init(const monica::CentralParameterProvider &cpp) {
      KJ_ASSERT(_monica != nullptr);
  auto simPs = _monica->simulationParameters();
  auto sitePs = _monica->siteParameters();
  _soilTempComp.setps(2.63);
  _soilTempComp.setpom(1.3);
  _soilTempComp.setsoilConstituentNames({"Rocks", "OrganicMatter", "Sand", "Silt", "Clay", "Water", "Ice", "Air"});
  std::vector<double> layerThicknessMM;
  std::vector<double> sands;
  std::vector<double> clays;
  std::vector<double> silts;
  std::vector<double> bds;
  std::vector<double> sws;
  std::vector<double> corgs;
  std::vector<double> rocks;
#ifdef AMEI_SENSITIVITY_ANALYSIS
  auto awc = simPs.customData["AWC"].number_value();
  for (const auto& j : sitePs.initSoilProfileSpec){
    layerThicknessMM.push_back(Tools::double_value(j["Thickness"])*1000.0);  // m -> mm
    Soil::SoilParameters sps;
    auto es = sps.merge(j);
    sands.push_back(sps.vs_SoilSandContent * 100.0); // %
    clays.push_back(sps.vs_SoilClayContent * 100.0); // %
    silts.push_back(sps.vs_SoilSiltContent() * 100.0); // %
    sws.push_back(sps.vs_PermanentWiltingPoint + awc*(sps.vs_FieldCapacity - sps.vs_PermanentWiltingPoint));
    bds.push_back(sps.vs_SoilBulkDensity() / 1000.0);  // kg/m3 -> g/cm3
    corgs.push_back(sps.vs_SoilOrganicCarbon() * 100.0); // %
    rocks.push_back(sps.vs_SoilStoneContent * 100.0); // %
  }
#else
  for (const auto& sl : _monica->soilColumn()){
    layerThicknessMM.push_back(sl.vs_LayerThickness*1000.0);  // m -> mm
    sands.push_back(sl.vs_SoilSandContent() * 100.0); // %
    clays.push_back(sl.vs_SoilClayContent() * 100.0); // %
    silts.push_back(sl.vs_SoilSiltContent() * 100.0); // %
    sws.push_back(sl.get_Vs_SoilMoisture_m3());
    bds.push_back(sl.vs_SoilBulkDensity() / 1000.0);  // kg/m3 -> g/cm3
    corgs.push_back(sl.vs_SoilOrganicCarbon() * 100.0); // %
    rocks.push_back(sl.vs_SoilStoneContent() * 100.0); // %
  }
#endif
  _soilTempExo.physical_ParticleSizeClay = clays;
  _soilTempExo.physical_ParticleSizeSand = sands;
  _soilTempExo.physical_ParticleSizeSilt = silts;
  _soilTempExo.physical_Rocks = rocks;
  _soilTempExo.organic_Carbon = corgs;
  _soilTempExo.waterBalance_SW = sws;
  _soilTempComp.setphysical_Thickness(layerThicknessMM);
  _soilTempComp.setphysical_BD(bds);
}

void MonicaInterface::run() {
      KJ_ASSERT(_monica != nullptr);
  auto climateData = _monica->currentStepClimateData();
  _soilTempExo.weather_MinT = climateData.at(Climate::tmin);
  _soilTempExo.weather_MaxT = climateData.at(Climate::tmax);
  _soilTempExo.weather_MeanT = climateData.at(Climate::tavg);
  _soilTempExo.weather_Radn = climateData.at(Climate::globrad);
  _soilTempExo.clock_Today_DayOfYear = _monica->currentStepDate().dayOfYear();
#ifdef AMEI_SENSITIVITY_ANALYSIS
  auto simPs = _monica->simulationParameters();
  _soilTempExo.weather_Wind = 3.0023;
  _soilTempExo.weather_AirPressure = 1013.25; // @(20°C 0m) 970.7716 @(20°C, 336m)
  _soilTempExo.waterBalance_Eo = climateData.at(Climate::x1);
  _soilTempExo.waterBalance_Eos = climateData.at(Climate::x3);
  _soilTempExo.waterBalance_Es = climateData.at(Climate::x2);
  _soilTempExo.microClimate_CanopyHeight = 0;
  _soilTempExo.waterBalance_Salb = simPs.customData["SALB"].number_value();
  _soilTempComp.setweather_Latitude(simPs.customData["XLAT"].number_value());
  _soilTempExo.weather_Tav = simPs.customData["TAV"].number_value();
  _soilTempExo.weather_Amp = simPs.customData["TAMP"].number_value();
#else
  _soilTempExo.weather_Wind = climateData.at(Climate::wind);
  _soilTempExo.weather_AirPressure = 1013.25;//climateData.at(Climate::airpress); 970.7716 (20°C, 336m)
  // !!! this seemingly should be just evaporation, or?
  _soilTempExo.waterBalance_Eo =  _monica->soilMoisture().get_PotentialEvapotranspiration(); // daily potential evaporation
  _soilTempExo.waterBalance_Eos = _monica->soilMoisture().get_PotentialEvapotranspiration(); //potential evaporation
  _soilTempExo.waterBalance_Es = _monica->soilMoisture().vm_ActualEvaporation; // actual evaporation
  auto tampNtav = _monica->dssatTAMPandTAV();
  _soilTempExo.weather_Tav = tampNtav.second;
  _soilTempExo.weather_Amp = tampNtav.first;
  if (_monica->cropGrowth()) _soilTempExo.microClimate_CanopyHeight = _monica->cropGrowth()->get_CropHeight();
#endif
  if(_doInit){
    _soilTempComp._SoilTemperature.Init(_soilTempState, _soilTempState1, _soilTempRate, _soilTempAux, _soilTempExo);
    _doInit = false;
  }
#ifndef AMEI_SENSITIVITY_ANALYSIS
  std::vector<double> sws;
  for (const auto& sl : _monica->soilColumn()){
    sws.push_back(sl.get_Vs_SoilMoisture_m3());
  }
  _soilTempExo.waterBalance_SW = sws;
#endif
  _soilTempComp.Calculate_Model(_soilTempState, _soilTempState1, _soilTempRate, _soilTempAux, _soilTempExo);
#ifndef AMEI_SENSITIVITY_ANALYSIS
  _monica->soilTemperatureNC().setSoilSurfaceTemperature(_soilTempState.aveSoilTemp.at(1));
  int i = 0;
  KJ_ASSERT(_monica->soilColumnNC().size() == _soilTempState.soilTemp.size());
  for (auto& sl : _monica->soilColumnNC()){
    sl.set_Vs_SoilTemperature(_soilTempState.aveSoilTemp.at(2+(i++)));
  }
#endif
}