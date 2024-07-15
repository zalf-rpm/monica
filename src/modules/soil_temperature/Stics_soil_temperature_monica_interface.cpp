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
#if STICS_SOIL_TEMPERATURE
      KJ_ASSERT(_monica != nullptr);
  std::vector<int> layerThicknessCm;
#ifdef SKIP_BUILD_IN_MODULES
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
#endif
}

void MonicaInterface::run() {
#if STICS_SOIL_TEMPERATURE
      KJ_ASSERT(_monica != nullptr);
  auto climateData = _monica->currentStepClimateData();
  auto tmin = climateData.at(Climate::tmin);
  auto tmax = climateData.at(Climate::tmax);
  soilTempExo.setmin_temp(tmin);
  soilTempExo.setmax_temp(tmax);
  soilTempExo.setmin_canopy_temp(tmin);
  soilTempExo.setmax_canopy_temp(tmax);
  soilTempExo.setmin_air_temp(tmin);
  if(_doInit){
    soilTempComp.setair_temp_day1(climateData.at(Climate::tavg));
    soilTempComp._Temp_profile.Init(soilTempState, soilTempState1, soilTempRate, soilTempAux, soilTempExo);
    _doInit = false;
  }
  soilTempComp.Calculate_Model(soilTempState, soilTempState1, soilTempRate, soilTempAux, soilTempExo);
  _monica->soilTemperatureNC().setSoilSurfaceTemperature(soilTempState.getcanopy_temp_avg());
  const auto& soilTemp = soilTempState.gettemp_profile();
  auto& sc = _monica->soilColumnNC();
  double sumT = 0;
  int count = 0;
  int monicaLayer = 0;
  for (size_t i = 0; i < soilTemp.size(); ++i){
    if (i % 10 == 0 && i > 0 && monicaLayer < sc.size()) {
      sc.at(monicaLayer).set_Vs_SoilTemperature(sumT / 10);
      ++monicaLayer;
      count = 0;
      sumT = 0;
    }
    sumT += soilTemp.at(i);
    ++count;
  }
#endif
}