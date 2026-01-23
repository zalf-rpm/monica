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

#include "Monica_AMEI_potential_Evapotranspiration_monica_interface.h"

#include <vector>
#include <kj/debug.h>

#include "core/monica-model.h"

using namespace Monica_AMEI_potential_Evapotranspiration;

MonicaInterface::MonicaInterface(monica::MonicaModel* monica) : _monica(monica) {}

void MonicaInterface::init(const monica::CentralParameterProvider& cpp) {
  KJ_ASSERT(_monica != nullptr);
  const auto sitePs = _monica->siteParameters();
  etComp.setheight_nn(sitePs.vs_HeightNN);
}

void MonicaInterface::run() {
  KJ_ASSERT(_monica != nullptr);
  auto climateData = _monica->currentStepClimateData();
  etExo.mean_air_temperature = climateData.at(Climate::tavg);
  bool externalET0 = climateData.find(Climate::et0) != climateData.end();
  if (externalET0) {
    etAux.reference_evapotranspiration = climateData.at(Climate::et0);
  }
  etExo.wind_speed = climateData.at(Climate::wind);
  etExo.wind_speed_height = _monica->environmentParameters().p_WindSpeedHeight;

  if (_monica->cropGrowth()) {
    etExo.developmental_stage = static_cast<int>(_monica->cropGrowth()->get_DevelopmentalStage());
    if (externalET0) {
      etAux.reference_evapotranspiration = _monica->cropGrowth()->get_ReferenceEvapotranspiration();
    }
    etExo.crop_remaining_evapotranspiration = _monica->cropGrowth()->get_RemainingEvapotranspiration();
  } else {
    etExo.developmental_stage = 0;
    // if (etExo.external_reference_evapotranspiration < 0) {
    //   etExo.crop_reference_evapotranspiration = 0;
    // }
    etExo.crop_remaining_evapotranspiration = 0;
  }
  if (_doInit) {
    _doInit = false;
  }
  if (_monica->soilMoisturePtr()) {
    etExo.kc_factor = _monica->soilMoisture().vc_KcFactor;
    etAux.vapor_pressure = _monica->soilMoisture()._vaporPressure;
  }
  etComp.Calculate_Model(etState, etState1, etRate, etAux, etExo);
  if (_monica->soilMoisturePtr()) {
    _monica->soilMoistureNC().vm_ReferenceEvapotranspiration = etAux.reference_evapotranspiration;
    // _monica->soilMoistureNC().vm_EvaporatedFromSurface = etState.evaporated_from_surface;
    // _monica->soilMoistureNC().vm_ActualEvapotranspiration = etState.actual_evapotranspiration;
    // _monica->soilMoistureNC().vm_ActualEvaporation = etState.actual_evaporation;
    // _monica->soilMoistureNC().vm_ActualTranspiration = etState.actual_transpiration;
    // _monica->soilMoistureNC().vm_SurfaceWaterStorage = etState.surface_water_storage;
    // _monica->soilMoistureNC().vm_SoilMoisture = etState.soil_moisture;
    // _monica->soilMoistureNC().vw_NetRadiation = etState.net_radiation;
  }
}
