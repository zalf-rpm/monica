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
  auto sitePs = _monica->siteParameters();
  std::vector<int> layerThicknessCm;
  for (const auto& j : sitePs.initSoilProfileSpec){
    int layerSizeCm = int(Tools::double_value(j["Thickness"])*100);  // m -> cm
    layerThicknessCm.push_back(layerSizeCm);
  }
  soilTempComp.setlayer_thick(layerThicknessCm);
#endif
}

void MonicaInterface::run() {
#if STICS_SOIL_TEMPERATURE
      KJ_ASSERT(_monica != nullptr);
  auto climateData = _monica->currentStepClimateData();
  soilTempExo.setmin_temp(climateData.at(Climate::tmin));
  soilTempExo.setmax_temp(climateData.at(Climate::tmax));
  soilTempExo.setmin_canopy_temp(climateData.at(Climate::tmin));
  soilTempExo.setmax_canopy_temp(climateData.at(Climate::tmax));
  soilTempExo.setmin_air_temp(climateData.at(Climate::tmin));
  if(_doInit){
    soilTempComp.setair_temp_day1(climateData.at(Climate::tavg));
    soilTempComp._Temp_profile.Init(soilTempState, soilTempState1, soilTempRate, soilTempAux, soilTempExo);
    _doInit = false;
  }
  soilTempComp.Calculate_Model(soilTempState, soilTempState1, soilTempRate, soilTempAux, soilTempExo);
  _monica->soilTemperatureNC().setSoilSurfaceTemperature(soilTempState.getcanopy_temp_avg());
#endif
}