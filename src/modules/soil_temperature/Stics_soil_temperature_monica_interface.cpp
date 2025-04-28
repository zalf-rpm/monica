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

#include "Stics_soil_temperature_monica_interface.h"

#include <vector>
#include <kj/debug.h>

#include "core/monica-model.h"

using namespace Stics_soil_temperature;

MonicaInterface::MonicaInterface(monica::MonicaModel *monica) : _monica(monica) {}

void MonicaInterface::init(const monica::CentralParameterProvider &cpp) {
      KJ_ASSERT(_monica != nullptr);
  std::vector<int> layerThicknessCm;
#ifdef AMEI_SENSITIVITY_ANALYSIS
  for (const auto& j : _monica->siteParameters().initSoilProfileSpec){
    int layerSizeCm = int(Tools::double_value(j["Thickness"])*100);  // m -> cm
    layerThicknessCm.push_back(layerSizeCm);
  }
#else
  for (const auto& sl : _monica->soilColumn()){
    int layerSizeCm = int(sl.vs_LayerThickness*100);  // m -> cm
    layerThicknessCm.push_back(layerSizeCm);
  }
#endif
  soilTempComp.setlayer_thick(layerThicknessCm);
}


void MonicaInterface::run() {
  KJ_ASSERT(_monica != nullptr);
  auto climateData = _monica->currentStepClimateData();
  auto tmin = climateData.at(Climate::tmin);
  auto tmax = climateData.at(Climate::tmax);
  soilTempExo.min_temp = tmin;
  soilTempExo.max_temp = tmax;
  soilTempExo.min_canopy_temp = tmin;
  soilTempExo.max_canopy_temp = tmax;
  soilTempExo.min_air_temp = tmin;
  if(_doInit){
    soilTempComp.setair_temp_day1(climateData.at(Climate::tavg));
    soilTempComp._Temp_profile.Init(_soilTempState, soilTempState1, soilTempRate, soilTempAux, soilTempExo);
    _doInit = false;
  }
  soilTempComp.Calculate_Model(_soilTempState, soilTempState1, soilTempRate, soilTempAux, soilTempExo);
#ifndef AMEI_SENSITIVITY_ANALYSIS
  _monica->soilTemperatureNC().setSoilSurfaceTemperature(_soilTempState.canopy_temp_avg);
  int i = 0;
  KJ_ASSERT(_monica->soilColumnNC().size() == _soilTempState.layer_temp.size());
  for (auto& sl : _monica->soilColumnNC()){
    sl.set_Vs_SoilTemperature(_soilTempState.layer_temp.at(i++));
  }
#endif
}