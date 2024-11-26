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

#include "SQ_Soil_Temperature_monica_interface.h"

#include <vector>
#include <kj/debug.h>

#include "core/monica-model.h"

using namespace SQ_Soil_Temperature;

MonicaInterface::MonicaInterface(monica::MonicaModel *monica) : _monica(monica) {}

void MonicaInterface::init(const monica::CentralParameterProvider &cpp) {
  KJ_ASSERT(_monica != nullptr);
  _soilTempComp.seta(0.5);
  _soilTempComp.setb(1.81);
  _soilTempComp.setc(0.49);
  _soilTempComp.setlambda_(2.454);
}

void MonicaInterface::run() {
  KJ_ASSERT(_monica != nullptr);
  auto climateData = _monica->currentStepClimateData();
#ifdef CPP2
  _soilTempExo.maxTAir = climateData.at(Climate::tmax);
  if (climateData.find(Climate::sunhours) == climateData.end()) {
    const auto& dls = Tools::dayLengths(_monica->siteParameters().vs_Latitude,
      _monica->currentStepDate().julianDay());
    _soilTempExo.dayLength = dls.astronomicDayLenght;
  } else {
    _soilTempExo.dayLength = climateData.at(Climate::sunhours);
  }
  _soilTempExo.minTAir = climateData.at(Climate::tmin);
  _soilTempExo.meanTAir = climateData.at(Climate::tavg);
#else
  _soilTempExo.setmaxTAir(climateData.at(Climate::tmax));
  _soilTempExo.setdayLength(climateData[Climate::sunhours]);
  _soilTempExo.setminTAir(climateData.at(Climate::tmin));
  _soilTempExo.setmeanTAir(climateData.at(Climate::tavg));
#endif
#ifdef AMEI_SENSITIVITY_ANALYSIS
#ifdef CPP2
  _soilTempExo.meanAnnualAirTemp = _monica->simulationParameters().customData["TAV"].number_value();
  _soilTempRate.heatFlux = climateData[Climate::o3]; //o3 is used as heat flux
#else
  _soilTempExo.setmeanAnnualAirTemp(_monica->simulationParameters().customData["TAV"].number_value());
  _soilTempRate.setheatFlux(climateData[Climate::o3]); //o3 is used as heat flux
#endif
#else
  auto tampNtav = _monica->dssatTAMPandTAV();
  _soilTempExo.meanAnnualAirTemp = tampNtav.second;
  _soilTempRate.heatFlux = 0;
#endif
  if(_doInit){
    _soilTempComp._CalculateSoilTemperature.Init(_soilTempState, _soilTempState1, _soilTempRate, _soilTempAux, _soilTempExo);
    _doInit = false;
  }
  _soilTempComp.Calculate_Model(_soilTempState, _soilTempState1, _soilTempRate, _soilTempAux, _soilTempExo);
#ifndef AMEI_SENSITIVITY_ANALYSIS
  auto stMin = _soilTempState.minTSoil;
  auto stMax = _soilTempState.maxTSoil;
  _monica->soilTemperatureNC().setSoilSurfaceTemperature((stMin + stMax)/2.0);
  auto deep = _soilTempState.deepLayerT;
  for (auto& sl : _monica->soilColumnNC()) sl.set_Vs_SoilTemperature(deep);
#endif
}