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
#if SQ_SOIL_TEMPERATURE
      KJ_ASSERT(_monica != nullptr);
  auto simPs = _monica->simulationParameters();
  auto sitePs = _monica->siteParameters();

  soilTempComp.seta(0.5);
  soilTempComp.setb(1.81);
  soilTempComp.setc(0.49);
  soilTempComp.setlambda_(2.454);
#endif
}

void MonicaInterface::run() {
#if SQ_SOIL_TEMPERATURE
      KJ_ASSERT(_monica != nullptr);
  auto climateData = _monica->currentStepClimateData();
  soilTempExo.setmaxTAir(climateData.at(Climate::tmax));
  soilTempExo.setdayLength(climateData[Climate::sunhours]);
  soilTempExo.setminTAir(climateData.at(Climate::tmin));
  soilTempExo.setmeanTAir(climateData.at(Climate::tavg));
  soilTempExo.setmeanAnnualAirTemp(_monica->simulationParameters().customData["TAV"].number_value());
  soilTempRate.setheatFlux(climateData[Climate::o3]); //o3 is used as heat flux
  if(_doInit){
    soilTempComp._CalculateSoilTemperature.Init(soilTempState, soilTempState1, soilTempRate, soilTempAux, soilTempExo);
    _doInit = false;
  }
  soilTempComp.Calculate_Model(soilTempState, soilTempState1, soilTempRate, soilTempAux, soilTempExo);
  //_monica->soilTemperatureNC().setSoilSurfaceTemperature(soilTempState.getSoilSurfaceTemperature());
#endif
}