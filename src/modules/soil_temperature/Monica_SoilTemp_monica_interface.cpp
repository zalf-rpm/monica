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

#include "Monica_SoilTemp_monica_interface.h"

#include <vector>
#include <kj/debug.h>

#include "core/monica-model.h"

using namespace Monica_SoilTemp;

MonicaInterface::MonicaInterface(monica::MonicaModel *monica) : _monica(monica) {}

void MonicaInterface::init(const monica::CentralParameterProvider &cpp) {
  KJ_ASSERT(_monica != nullptr);
  auto simPs = _monica->simulationParameters();
  auto sitePs = _monica->siteParameters();
#ifdef AMEI_SENSITIVITY_ANALYSIS
  auto awc = simPs.customData["AWC"].number_value();
#else
  double awc = 0.25;
#endif
  const auto &stParams = cpp.userSoilTemperatureParameters;
  soilTempComp.settimeStep(cpp.userEnvironmentParameters.p_timeStep);
  soilTempComp.setbaseTemp(stParams.pt_BaseTemperature);
  soilTempComp.setinitialSurfaceTemp(stParams.pt_InitialSurfaceTemperature);
  soilTempComp.setdensityAir(stParams.pt_DensityAir);
  soilTempComp.setspecificHeatCapacityAir(stParams.pt_SpecificHeatCapacityAir);
  soilTempComp.setdensityHumus(stParams.pt_DensityHumus);
  soilTempComp.setspecificHeatCapacityHumus(stParams.pt_SpecificHeatCapacityHumus);
  soilTempComp.setdensityWater(stParams.pt_DensityWater);
  soilTempComp.setspecificHeatCapacityWater(stParams.pt_SpecificHeatCapacityWater);
  soilTempComp.setquartzRawDensity(stParams.pt_QuartzRawDensity);
  soilTempComp.setspecificHeatCapacityQuartz(stParams.pt_SpecificHeatCapacityQuartz);
  soilTempComp.setnTau(stParams.pt_NTau);
  soilTempComp.setnoOfTempLayers(sitePs.numberOfLayers + 2);
  soilTempComp.setnoOfSoilLayers(sitePs.numberOfLayers);
  soilTempComp.setnoOfTempLayersPlus1(sitePs.numberOfLayers + 3);
  std::vector<double> lts;
  std::vector<double> bds;
  std::vector<double> sats;
  std::vector<double> oms;
  std::vector<double> smcs; //soil moisture const
  for (const auto &sps: sitePs.vs_SoilParameters) {
    //st.getlayerThickness().push_back(_sitePs.layerThickness);
    //st.getsoilBulkDensity().push_back(sps.vs_SoilBulkDensity());
    //st.getsaturation().push_back(sps.vs_Saturation);
    //st.getsoilOrganicMatter().push_back(sps.vs_SoilOrganicMatter());
    lts.push_back(sitePs.layerThickness);
    bds.push_back(sps.vs_SoilBulkDensity());
    sats.push_back(sps.vs_Saturation);
    oms.push_back(sps.vs_SoilOrganicMatter());
    smcs.push_back(sps.vs_PermanentWiltingPoint + awc*(sps.vs_FieldCapacity - sps.vs_PermanentWiltingPoint));
  }
  soilTempComp.setsoilMoistureConst(smcs);
  // add the two temperature layers
  //st.getlayerThickness().push_back(_sitePs.layerThickness);
  //st.getlayerThickness().push_back(_sitePs.layerThickness);
  lts.push_back(sitePs.layerThickness);
  lts.push_back(sitePs.layerThickness);
  soilTempComp.setlayerThickness(lts);
  soilTempComp.setsoilBulkDensity(bds);
  soilTempComp.setsaturation(sats);
  soilTempComp.setsoilOrganicMatter(oms);
  soilTempComp.setdampingFactor(stParams.dampingFactor);
}

void MonicaInterface::run() {
  KJ_ASSERT(_monica != nullptr);
  auto climateData = _monica->currentStepClimateData();
  soilTempExo.tmin = climateData.at(Climate::tmin);
  soilTempExo.tmax = climateData.at(Climate::tmax);
  soilTempExo.globrad = climateData.at(Climate::globrad);
  auto simPs = _monica->simulationParameters();
#ifdef AMEI_SENSITIVITY_ANALYSIS
  if (simPs.customData["LAI"].is_null()){
    soilTempExo.soilCoverage = 0;
  } else {
    auto lai = simPs.customData["LAI"].number_value();
    soilTempExo.soilCoverage = 1.0 - exp(-0.5 * lai);
  }
#else
  if (_monica->cropGrowth()) soilTempExo.soilCoverage = _monica->cropGrowth()->get_SoilCoverage();
#endif
  if (_monica->soilMoisturePtr() && _monica->soilMoisture().getSnowDepth() > 0.0) {
    soilTempExo.hasSnowCover = true;
    soilTempExo.soilSurfaceTemperatureBelowSnow = _monica->soilMoisture().getTemperatureUnderSnow();
  } else {
    soilTempExo.hasSnowCover = false;
  }
  if(_doInit){
    soilTempComp._SoilTemperature.Init(soilTempState, soilTempState1, soilTempRate, soilTempAux,         soilTempExo);
    _doInit = false;
  }
  soilTempComp.Calculate_Model(soilTempState, soilTempState1, soilTempRate, soilTempAux, soilTempExo);
#ifndef AMEI_SENSITIVITY_ANALYSIS
  _monica->soilTemperatureNC().setSoilSurfaceTemperature(soilTempState.soilSurfaceTemperature);
  auto& sc = _monica->soilColumnNC();
  for(size_t layer = 0; layer < sc.size(); layer++) {
    sc.at(layer).set_Vs_SoilTemperature(soilTempState.soilTemperature.at(layer));
  }
#endif
}