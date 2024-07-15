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

#include "DSSAT_ST_standalone_monica_interface.h"

#include <vector>
#include <kj/debug.h>

#include "core/monica-model.h"

using namespace DSSAT_ST_standalone;

MonicaInterface::MonicaInterface(monica::MonicaModel *monica) : _monica(monica) {}

void MonicaInterface::init(const monica::CentralParameterProvider &cpp) {
#if DSSAT_ST_STANDALONE
  KJ_ASSERT(_monica != nullptr);
  auto simPs = _monica->simulationParameters();
  auto sitePs = _monica->siteParameters();
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
  auto awc = _monica->simulationParameters().customData["AWC"].number_value();
  std::vector<double> sws;
  for (const auto &j: sitePs.initSoilProfileSpec) {
    int layerSizeCm = int(Tools::double_value(j["Thickness"]) * 100);  // m -> cm
    currentDepthCm += layerSizeCm;
    Soil::SoilParameters sps;
    auto es = sps.merge(j);
    //soilTempComp.getLL().push_back(sps.vs_PermanentWiltingPoint);
    //soilTempComp.getDUL().push_back(sps.vs_FieldCapacity);
    //soilTempComp.getDS().push_back(currentDepthCm);
    //soilTempComp.getDLAYR().push_back(layerSizeCm);
    //soilTempComp.getBD().push_back(sps.vs_SoilBulkDensity());
    //soilTempComp.getSW().push_back(awc);
    lls.push_back(sps.vs_PermanentWiltingPoint);
    duls.push_back(sps.vs_FieldCapacity);
    dss.push_back(currentDepthCm);
    dlayrs.push_back(layerSizeCm);
    bds.push_back(sps.vs_SoilBulkDensity() / 1000.0);  // kg/m3 -> g/cm3
    sws.push_back(awc);
  }
  soilTempComp.setSW(sws);
  soilTempComp.setMSALB(_simPs.customData["SALB"].number_value());
#else
  _soilTempComp.setXLAT(sitePs.vs_Latitude);
  for (const auto& sl : _monica->soilColumn()){
    int layerSizeCm = int(sl.vs_LayerThickness*100);  // m -> cm
    currentDepthCm += layerSizeCm;
    lls.push_back(sl.vs_PermanentWiltingPoint());
    duls.push_back(sl.vs_FieldCapacity());
    dss.push_back(currentDepthCm);
    dlayrs.push_back(layerSizeCm);
    bds.push_back(sl.vs_SoilBulkDensity() / 1000.0);  // kg/m3 -> g/cm3
  }
  _soilTempComp.setMSALB(_monica->environmentParameters().p_Albedo);
#endif
  _soilTempComp.setLL(lls);
  _soilTempComp.setDUL(duls);
  _soilTempComp.setDS(dss);
  _soilTempComp.setDLAYR(dlayrs);
  _soilTempComp.setBD(bds);
#endif
}

void MonicaInterface::run() {
#if DSSAT_ST_STANDALONE
      KJ_ASSERT(_monica != nullptr);
  auto climateData = _monica->currentStepClimateData();
  _soilTempExo.setDOY(_monica->currentStepDate().dayOfYear());
  _soilTempExo.setSRAD(climateData.at(Climate::globrad));
  _soilTempExo.setTAVG(climateData.at(Climate::tavg));
  _soilTempExo.setTMAX(climateData.at(Climate::tmax));
  if (_doInit) {
#ifdef SKIP_BUILD_IN_MODULES
  soilTempExo.setTAV(simPs.customData["TAV"].number_value());
  soilTempExo.setTAMP(simPs.customData["TAMP"].number_value());
#else
    auto tampNtav = _monica->dssatTAMPandTAV();
    _soilTempExo.setTAV(tampNtav.first);
    _soilTempExo.setTAMP(tampNtav.second);
#endif
    _soilTempComp._STEMP.Init(_soilTempState, _soilTempState1, _soilTempRate, _soilTempAux, _soilTempExo);
    _doInit = false;
  }
#ifndef SKIP_BUILD_IN_MODULES
  std::vector<double> sws;
  for (const auto& sl : _monica->soilColumn()){
    sws.push_back(sl.get_Vs_SoilMoisture_m3() - sl.vs_PermanentWiltingPoint());
  }
  _soilTempComp.setSW(sws);
#endif
  _soilTempComp.Calculate_Model(_soilTempState, _soilTempState1, _soilTempRate, _soilTempAux, _soilTempExo);
  _monica->soilTemperatureNC().setSoilSurfaceTemperature(_soilTempState.getSRFTEMP());
  int i = 0;
      KJ_ASSERT(_monica->soilColumnNC().size() == _soilTempState.getST().size());
  for (auto& sl : _monica->soilColumnNC()){
    sl.set_Vs_SoilTemperature(_soilTempState.getST().at(i++));
  }
#endif
}