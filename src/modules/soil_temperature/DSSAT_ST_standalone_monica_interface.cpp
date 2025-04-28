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
  KJ_ASSERT(_monica != nullptr);
  auto simPs = _monica->simulationParameters();
  auto sitePs = _monica->siteParameters();
  _soilTempComp.setISWWAT("Y");
  int currentDepthCm = 0;
  std::vector<double> lls;
  std::vector<double> duls;
  std::vector<double> dss;
  std::vector<double> dlayrs;
  std::vector<double> bds;
#ifdef AMEI_SENSITIVITY_ANALYSIS
  _soilTempComp.setNL(int(sitePs.initSoilProfileSpec.size()));
  _soilTempComp.setNLAYR(int(sitePs.initSoilProfileSpec.size()));
  _soilTempComp.setXLAT(_monica->simulationParameters().customData["XLAT"].number_value());
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
    sws.push_back(sps.vs_PermanentWiltingPoint + awc*(sps.vs_FieldCapacity - sps.vs_PermanentWiltingPoint));
  }
  _soilTempComp.setSW(sws);
  _soilTempComp.setMSALB(_monica->simulationParameters().customData["SALB"].number_value());
#else
  _soilTempComp.setNL(int(_monica->soilColumn().size()));
  _soilTempComp.setNLAYR(int(_monica->soilColumn().size()));
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
}

void MonicaInterface::run() {
  KJ_ASSERT(_monica != nullptr);
  auto climateData = _monica->currentStepClimateData();
  _soilTempExo.DOY = _monica->currentStepDate().dayOfYear();
  _soilTempExo.SRAD = climateData.at(Climate::globrad);
  _soilTempExo.TAVG = climateData.at(Climate::tavg);
  _soilTempExo.TMAX = climateData.at(Climate::tmax);
  if (_doInit) {
#ifdef AMEI_SENSITIVITY_ANALYSIS
  _soilTempExo.TAV = _monica->simulationParameters().customData["TAV"].number_value();
  _soilTempExo.TAMP = _monica->simulationParameters().customData["TAMP"].number_value();
#else
    auto tampNtav = _monica->dssatTAMPandTAV();
    _soilTempExo.TAV = tampNtav.second;
    _soilTempExo.TAMP = tampNtav.first;
#endif
    _soilTempComp._STEMP.Init(soilTempState, _soilTempState1, _soilTempRate, _soilTempAux, _soilTempExo);
    _doInit = false;
  }
#ifndef AMEI_SENSITIVITY_ANALYSIS
  std::vector<double> sws;
  for (const auto& sl : _monica->soilColumn()){
    sws.push_back(sl.get_Vs_SoilMoisture_m3());
  }
  _soilTempComp.setSW(sws);
#endif
  _soilTempComp.Calculate_Model(soilTempState, _soilTempState1, _soilTempRate, _soilTempAux, _soilTempExo);
#ifndef AMEI_SENSITIVITY_ANALYSIS
  _monica->soilTemperatureNC().setSoilSurfaceTemperature(soilTempState.SRFTEMP);
  int i = 0;
  KJ_ASSERT(_monica->soilColumnNC().size() == soilTempState.ST.size());
  for (auto& sl : _monica->soilColumnNC()){
    sl.set_Vs_SoilTemperature(soilTempState.ST.at(i++));
  }
#endif
}