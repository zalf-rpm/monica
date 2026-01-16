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

#include "Monica_Evapotranspiration_monica_interface.h"

#include <vector>
#include <kj/debug.h>

#include "core/monica-model.h"

using namespace Monica_Evapotranspiration;

MonicaInterface::MonicaInterface(monica::MonicaModel* monica) : _monica(monica) {}

void MonicaInterface::init(const monica::CentralParameterProvider& cpp) {
  KJ_ASSERT(_monica != nullptr);
  const auto sitePs = _monica->siteParameters();
  etComp.setevaporation_zeta(cpp.userSoilMoistureParameters.pm_EvaporationZeta);
  etComp.setmaximum_evaporation_impact_depth(cpp.userSoilMoistureParameters.pm_MaximumEvaporationImpactDepth);
  etComp.setreference_albedo(cpp.userCropParameters.pc_ReferenceAlbedo);
  //etComp.setstomata_resistance(100);
  //etComp.setevaporation_reduction_method(1);
  etComp.setxsa_critical_soil_moisture(cpp.userSoilMoistureParameters.pm_XSACriticalSoilMoisture);
  etComp.setlatitude(sitePs.vs_Latitude);
  etComp.setheight_nn(sitePs.vs_HeightNN);
  etComp.setno_of_soil_layers(sitePs.numberOfLayers);
  etComp.setno_of_soil_moisture_layers(sitePs.numberOfLayers + 1);
  // etComp.setlayer_thickness(_monica->soilMoisture().vm_LayerThickness);
  // etComp.setpermanent_wilting_point(_monica->soilMoisture().vm_PermanentWiltingPoint);
  // etComp.setfield_capacity(_monica->soilMoisture().vm_FieldCapacity);
  std::vector<double> lts;
  std::vector<double> pwps;
  std::vector<double> fcs;
  KJ_ASSERT(sitePs.vs_SoilParameters.size() == sitePs.numberOfLayers);
  for (const auto& sps : sitePs.vs_SoilParameters) {
    lts.push_back(sitePs.layerThickness);
    pwps.push_back(sps.vs_PermanentWiltingPoint);
    fcs.push_back(sps.vs_FieldCapacity);
  }
  lts.push_back(lts.back());
  pwps.push_back(pwps.back());
  fcs.push_back(fcs.back());
  etComp.setlayer_thickness(lts);
  etComp.setpermanent_wilting_point(pwps);
  etComp.setfield_capacity(fcs);
}

void MonicaInterface::run() {
  KJ_ASSERT(_monica != nullptr);
  auto climateData = _monica->currentStepClimateData();
  etExo.min_air_temperature = climateData.at(Climate::tmin);
  etExo.mean_air_temperature = climateData.at(Climate::tavg);
  etExo.max_air_temperature = climateData.at(Climate::tmax);
  etExo.global_radiation = climateData.at(Climate::globrad);
  if (climateData.find(Climate::et0) != climateData.end()) {
    etExo.external_reference_evapotranspiration = climateData.at(Climate::et0);
  }
  etExo.relative_humidity = climateData.at(Climate::relhumid) / 100.0;
  etExo.wind_speed = climateData.at(Climate::wind);
  etExo.wind_speed_height = _monica->environmentParameters().p_WindSpeedHeight;
  etExo.julian_day = _monica->currentStepDate().julianDay();

  if (_monica->cropGrowth()) {
    etExo.developmental_stage = static_cast<int>(_monica->cropGrowth()->get_DevelopmentalStage());
    if (etExo.external_reference_evapotranspiration < 0) {
      etExo.crop_reference_evapotranspiration = _monica->cropGrowth()->get_ReferenceEvapotranspiration();
    }
    etExo.crop_transpiration = _monica->cropGrowth()->get_Transpiration();
    etExo.crop_remaining_evapotranspiration = _monica->cropGrowth()->get_RemainingEvapotranspiration();
    etExo.crop_evaporated_from_intercepted = _monica->cropGrowth()->get_EvaporatedFromIntercept();
  } else {
    etExo.developmental_stage = 0;
    if (etExo.external_reference_evapotranspiration < 0) {
      etExo.crop_reference_evapotranspiration = 0;
    }
    etExo.crop_transpiration = std::vector<double>();
    etExo.crop_remaining_evapotranspiration = 0;
    etExo.crop_evaporated_from_intercepted = 0;
  }
  if (_doInit) {
    etComp._Evapotranspiration.Init(etState, etState1, etRate, etAux, etExo);
    _doInit = false;
  }
  if (_monica->soilMoisturePtr()) {
    etExo.percentage_soil_coverage = _monica->soilMoisture().vc_PercentageSoilCoverage;
    etExo.kc_factor = _monica->soilMoisture().vc_KcFactor;
    etExo.has_snow_cover = _monica->soilMoisture().getSnowDepth() > 0;
    etExo.vapor_pressure = _monica->soilMoisture()._vaporPressure;
    // soil_moisture and surface water storage, even though state
    // have to be synchronized with MONICA due to other uses and updates
    // maybe should be also defined as exogenous, even though are actually more state like?
    etState.soil_moisture = _monica->soilMoisture().vm_SoilMoisture;
    etState.surface_water_storage = _monica->soilMoisture().vm_SurfaceWaterStorage;
  }
  etComp.Calculate_Model(etState, etState1, etRate, etAux, etExo);
  if (_monica->soilMoisturePtr()) {
    _monica->soilMoistureNC().vm_ReferenceEvapotranspiration = etState.reference_evapotranspiration;
    _monica->soilMoistureNC().vm_EvaporatedFromSurface = etState.evaporated_from_surface;
    _monica->soilMoistureNC().vm_ActualEvapotranspiration = etState.actual_evapotranspiration;
    _monica->soilMoistureNC().vm_ActualEvaporation = etState.actual_evaporation;
    _monica->soilMoistureNC().vm_ActualTranspiration = etState.actual_transpiration;
    _monica->soilMoistureNC().vm_SurfaceWaterStorage = etState.surface_water_storage;
    _monica->soilMoistureNC().vm_SoilMoisture = etState.soil_moisture;
    _monica->soilMoistureNC().vw_NetRadiation = etState.net_radiation;
  }
}
