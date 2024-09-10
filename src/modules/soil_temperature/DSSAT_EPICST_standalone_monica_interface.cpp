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

#include "DSSAT_EPICST_standalone_monica_interface.h"

#include <vector>
#include <kj/debug.h>

#include "core/monica-model.h"

using namespace DSSAT_EPICST_standalone;

MonicaInterface::MonicaInterface(monica::MonicaModel *monica) : _monica(monica) {}

void MonicaInterface::init(const monica::CentralParameterProvider &cpp) {
#if DSSAT_EPICST_STANDALONE
      KJ_ASSERT(_monica != nullptr);
  auto simPs = _monica->simulationParameters();
  auto sitePs = _monica->siteParameters();

#ifdef SKIP_BUILD_IN_MODULES
  auto awc = simPs.customData["AWC"].number_value();
#endif
  _soilTempComp.setISWWAT("Y");
  _soilTempComp.setNL(int(sitePs.initSoilProfileSpec.size()));
  _soilTempComp.setNLAYR(int(sitePs.initSoilProfileSpec.size()));
  int currentDepthCm = 0;
  std::vector<double> lls;
  std::vector<double> duls;
  std::vector<double> dss;
  std::vector<double> dlayrs;
  std::vector<double> bds;
#ifdef SKIP_BUILD_IN_MODULES
  _soilTempComp.setNL(int(sitePs.initSoilProfileSpec.size()));
  _soilTempComp.setNLAYR(int(sitePs.initSoilProfileSpec.size()));
  std::vector<double> sws;
  for (const auto& j : sitePs.initSoilProfileSpec){
    int layerSizeCm = int(Tools::double_value(j["Thickness"])*100);  // m -> cm
    currentDepthCm += layerSizeCm;
    Soil::SoilParameters sps;
    auto es = sps.merge(j);
    //st3.getLL().push_back(sps.vs_PermanentWiltingPoint);
    //st3.getDUL().push_back(sps.vs_FieldCapacity);
    //st3.getDS().push_back(currentDepthCm);
    //st3.getDLAYR().push_back(layerSizeCm);
    //st3.getBD().push_back(sps.vs_SoilBulkDensity());
    //st3.getSW().push_back(awc);
    lls.push_back(sps.vs_PermanentWiltingPoint);
    duls.push_back(sps.vs_FieldCapacity);
    dss.push_back(currentDepthCm);
    dlayrs.push_back(layerSizeCm);
    bds.push_back(sps.vs_SoilBulkDensity() / 1000.0);  // kg/m3 -> g/cm3
    sws.push_back(awc);
  }
  _soilTempComp.setSW(sws);
#else
  soilTempComp.setNL(int(_monica->soilColumn().size()));
  soilTempComp.setNLAYR(int(_monica->soilColumn().size()));
  for (const auto& sl : _monica->soilColumn()){
    int layerSizeCm = int(sl.vs_LayerThickness*100);  // m -> cm
    currentDepthCm += layerSizeCm;
    lls.push_back(sl.vs_PermanentWiltingPoint());
    duls.push_back(sl.vs_FieldCapacity());
    dss.push_back(currentDepthCm);
    dlayrs.push_back(layerSizeCm);
    bds.push_back(sl.vs_SoilBulkDensity() / 1000.0);  // kg/m3 -> g/cm3
  }
#endif
  _soilTempComp.setLL(lls);
  _soilTempComp.setDUL(duls);
  _soilTempComp.setDS(dss);
  _soilTempComp.setDLAYR(dlayrs);
  _soilTempComp.setBD(bds);
#endif
}

void MonicaInterface::run() {
#if DSSAT_EPICST_STANDALONE
      KJ_ASSERT(_monica != nullptr);
  auto climateData = _monica->currentStepClimateData();
  soilTempExo.setTMIN(climateData.at(Climate::tmin));
  soilTempExo.setTAVG(climateData.at(Climate::tavg));
  soilTempExo.setTMAX(climateData.at(Climate::tmax));
  soilTempExo.setRAIN(climateData.at(Climate::precip));
#if SKIP_BUILD_IN_MODULES
  soilTempExo.setSNOW(0);
  soilTempExo.setDEPIR(_monica->simulationParameters().customData["IRVAL"].number_value());
  soilTempExo.setMULCHMASS(_monica->simulationParameters().customData["MLTHK"].number_value());
  soilTempExo.setBIOMAS(_monica->simulationParameters().customData["CWAD"].number_value());
  soilTempExo.setTAV(_monica->simulationParameters().customData["TAV"].number_value());
  soilTempExo.setTAMP(_monica->simulationParameters().customData["TAMP"].number_value());
#else
  soilTempExo.setSNOW(_monica->soilMoisture().getSnowDepth());
  soilTempExo.setDEPIR(_monica->dailySumIrrigationWater());
  soilTempExo.setMULCHMASS(0);
  if (_monica->cropGrowth()) soilTempExo.setBIOMAS(_monica->cropGrowth()->get_AbovegroundBiomass());
  else soilTempExo.setBIOMAS(0);
  auto tampNtav = _monica->dssatTAMPandTAV();
  soilTempExo.setTAV(tampNtav.first);
  soilTempExo.setTAMP(tampNtav.second);
#endif
  if(_doInit){
    _soilTempComp._STEMP_EPIC.Init(soilTempState, soilTempState1, soilTempRate, soilTempAux, soilTempExo);
    _doInit = false;
  }
#ifndef SKIP_BUILD_IN_MODULES
  std::vector<double> sws;
  for (const auto& sl : _monica->soilColumn()){
    sws.push_back(sl.get_Vs_SoilMoisture_m3() - sl.vs_PermanentWiltingPoint());
  }
  soilTempComp.setSW(sws);
#endif
  _soilTempComp.Calculate_Model(soilTempState, soilTempState1, soilTempRate, soilTempAux, soilTempExo);
#ifndef SKIP_BUILD_IN_MODULES
  _monica->soilTemperatureNC().setSoilSurfaceTemperature(soilTempState.getSRFTEMP());
  int i = 0;
  KJ_ASSERT(_monica->soilColumnNC().size() == soilTempState.getST().size());
  for (auto& sl : _monica->soilColumnNC()){
    sl.set_Vs_SoilTemperature(soilTempState.getST().at(i++));
  }
#endif
#endif
}