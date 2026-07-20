/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
Authors:
Claas Nendel <claas.nendel@zalf.de>
Xenia Specka <xenia.specka@zalf.de>
Michael Berg <michael.berg@zalf.de>

Maintainers:
Currently maintained by the authors.

This file is part of the MONICA model.
Copyright (C) Leibniz Centre for Agricultural Landscape Research (ZALF)
*/

#include "soilcolumn_simple.h"

#include <cmath>
#include <algorithm>

/**
 * @file soilcolumn_simple.cpp
 *
 * @brief This file contains the definition of classes AOM_Properties,  SoilLayer, SoilColumn
 *
 * @see Monica::AOM_Properties
 * @see Monica::SoilLayer
 * @see Monica::SoilColumn
 * @see Monica::FertilizerTriggerThunk
 */

#include "crop-module_simple.h"
#include "tools/debug.h"
#include "soil/constants.h"

using namespace monica;
using namespace std;
using namespace Soil;
using namespace Tools;

void AOM_Properties::deserialize(mas::schema::model::monica::AOMProperties::Reader reader) {
  vo_AOM_Slow = reader.getAomSlow();
  vo_AOM_Fast = reader.getAomFast();
  vo_AOM_SlowDecRate_to_SMB_Slow = reader.getAomSlowDecRatetoSMBSlow();
  vo_AOM_SlowDecRate_to_SMB_Fast = reader.getAomSlowDecRatetoSMBFast();
  vo_AOM_FastDecRate_to_SMB_Slow = reader.getAomFastDecRatetoSMBSlow();
  vo_AOM_FastDecRate_to_SMB_Fast = reader.getAomFastDecRatetoSMBFast();
  vo_AOM_SlowDecCoeff = reader.getAomSlowDecCoeff();
  vo_AOM_FastDecCoeff = reader.getAomFastDecCoeff();
  vo_AOM_SlowDecCoeffStandard = reader.getAomSlowDecCoeffStandard();
  vo_AOM_FastDecCoeffStandard = reader.getAomFastDecCoeffStandard();
  vo_PartAOM_Slow_to_SMB_Slow = reader.getPartAOMSlowtoSMBSlow();
  vo_PartAOM_Slow_to_SMB_Fast = reader.getPartAOMSlowtoSMBFast();
  vo_CN_Ratio_AOM_Slow = reader.getCnRatioAOMSlow();
  vo_CN_Ratio_AOM_Fast = reader.getCnRatioAOMFast();
  vo_DaysAfterApplication = reader.getDaysAfterApplication();
  vo_AOM_DryMatterContent = reader.getAomDryMatterContent();
  vo_AOM_NH4Content = reader.getAomNH4Content();
  vo_AOM_SlowDelta = reader.getAomSlowDelta();
  vo_AOM_FastDelta = reader.getAomFastDelta();
  incorporation = reader.getIncorporation();
  noVolatilization = reader.getNoVolatilization();
}

void AOM_Properties::serialize(mas::schema::model::monica::AOMProperties::Builder builder) const {
  builder.setAomSlow(vo_AOM_Slow);
  builder.setAomFast(vo_AOM_Fast);
  builder.setAomSlowDecRatetoSMBSlow(vo_AOM_SlowDecRate_to_SMB_Slow);
  builder.setAomSlowDecRatetoSMBFast(vo_AOM_SlowDecRate_to_SMB_Fast);
  builder.setAomFastDecRatetoSMBSlow(vo_AOM_FastDecRate_to_SMB_Slow);
  builder.setAomFastDecRatetoSMBFast(vo_AOM_FastDecRate_to_SMB_Fast);
  builder.setAomSlowDecCoeff(vo_AOM_SlowDecCoeff);
  builder.setAomFastDecCoeff(vo_AOM_FastDecCoeff);
  builder.setAomSlowDecCoeffStandard(vo_AOM_SlowDecCoeffStandard);
  builder.setAomFastDecCoeffStandard(vo_AOM_FastDecCoeffStandard);
  builder.setPartAOMSlowtoSMBSlow(vo_PartAOM_Slow_to_SMB_Slow);
  builder.setPartAOMSlowtoSMBFast(vo_PartAOM_Slow_to_SMB_Fast);
  builder.setCnRatioAOMSlow(vo_CN_Ratio_AOM_Slow);
  builder.setCnRatioAOMFast(vo_CN_Ratio_AOM_Fast);
  builder.setDaysAfterApplication(vo_DaysAfterApplication);
  builder.setAomDryMatterContent(vo_AOM_DryMatterContent);
  builder.setAomNH4Content(vo_AOM_NH4Content);
  builder.setAomSlowDelta(vo_AOM_SlowDelta);
  builder.setAomFastDelta(vo_AOM_FastDelta);
  builder.setIncorporation(incorporation);
  builder.setNoVolatilization(noVolatilization);
}


/**
 * Constructor
 * @param vs_LayerThickness Vertical expansion
 * @param sps Soil parameters
 */
SoilLayer::SoilLayer(double vs_LayerThickness,
                     const SoilParameters& sps)
: vs_LayerThickness(vs_LayerThickness)
, vs_SoilNH4(sps.vs_SoilAmmonium)
, vs_SoilNO3(sps.vs_SoilNitrate)
, _sps(sps)
, vs_SoilMoisture_m3(sps.vs_FieldCapacity * sps.vs_SoilMoisturePercentFC / 100.0)
//, vs_SoilMoistureOld_m3(sps.vs_FieldCapacity * sps.vs_SoilMoisturePercentFC / 100.0)
{}

void SoilLayer::deserialize(mas::schema::model::monica::SoilLayerState::Reader reader) {
  vs_LayerThickness = reader.getLayerThickness();
  vs_SoilWaterFlux = reader.getSoilWaterFlux();
  setFromComplexCapnpList(vo_AOM_Pool, reader.getVoAOMPool());
  vs_SOM_Slow = reader.getSomSlow();
  vs_SOM_Fast = reader.getSomFast();
  vs_SMB_Slow = reader.getSmbSlow();
  vs_SMB_Fast = reader.getSmbFast();
  vs_SoilCarbamid = reader.getSoilCarbamid();
  vs_SoilNH4 = reader.getSoilNH4();
  vs_SoilNO2 = reader.getSoilNO2();
  vs_SoilNO3 = reader.getSoilNO3();
  vs_SoilFrozen = reader.getSoilFrozen();
  _sps.deserialize(reader.getSps());
  vs_SoilMoisture_m3 = reader.getSoilMoistureM3();
  vs_SoilTemperature = reader.getSoilTemperature();
}

void SoilLayer::serialize(mas::schema::model::monica::SoilLayerState::Builder builder) const {
  builder.setLayerThickness(vs_LayerThickness);
  builder.setSoilWaterFlux(vs_SoilWaterFlux);
  setComplexCapnpList(vo_AOM_Pool, builder.initVoAOMPool((capnp::uint)vo_AOM_Pool.size()));
  builder.setSomSlow(vs_SOM_Slow);
  builder.setSomFast(vs_SOM_Fast);
  builder.setSmbSlow(vs_SMB_Slow);
  builder.setSmbFast(vs_SMB_Fast);
  builder.setSoilCarbamid(vs_SoilCarbamid);
  builder.setSoilNH4(vs_SoilNH4);
  builder.setSoilNO2(vs_SoilNO2);
  builder.setSoilNO3(vs_SoilNO3);
  builder.setSoilFrozen(vs_SoilFrozen);
  _sps.serialize(builder.initSps());
  builder.setSoilMoistureM3(vs_SoilMoisture_m3);
  builder.setSoilTemperature(vs_SoilTemperature);
}

/**
 * Soil layer's moisture content, expressed as logarithm of
 * pressure head in cm water column. Algorithm of Van Genuchten is used.
 * Conversion of water saturation into soil-moisture tension.
 *
 * @todo Einheiten prüfen
 */
double SoilLayer::vs_SoilMoisture_pF() {
  // Derivation of Van Genuchten parameters (Vereecken at al. 1989)

  auto ps = calcVanGenuchtenVereeckenParams(_sps.vs_PermanentWiltingPoint,
                                            _sps.vs_Saturation, _sps.vs_SoilSandContent, _sps.vs_SoilClayContent,
                                            _sps.vs_SoilBulkDensity(), _sps.vs_SoilOrganicCarbon());

  //Van Genuchten retention curve
  auto sm = vs_SoilMoisture_m3;
  double matricHead = sm <= ps.thetaR
                        ? 5.0E+7
                        : (1.0 / ps.alpha) * (pow(pow((ps.thetaS - ps.thetaR) / (sm - ps.thetaR),
                                                      1 / ps.m) - 1, 1 / ps.n));
  double soilMoisture_pF = log10(matricHead);

  /* JV! set _vs_SoilMoisture_pF to "small" number in case of vs_Theta "close" to vs_ThetaS (vs_Psi < 1 -> log(vs_Psi) < 0) */
  return soilMoisture_pF < 0.0 ? 5.0E-7 : soilMoisture_pF;
  //  debug() << "vs_SoilMoisture_pF: " << soilMoisture_pF << std::endl;
}

//------------------------------------------------------------------------------

/**
 * @brief Constructor
 *
 * Constructor with parameter initialization. Initializes every layer
 * in vector with the layer-thickness and special soil parameter in this layer.
 *
 * @param gps General Parameters
 * @param soilParams Soil Parameter
 */
SoilColumn::SoilColumn(double ps_LayerThickness,
                       double ps_MaxMineralisationDepth,
                       const SoilPMs& soilParams) //,
//double pm_CriticalMoistureDepth)
: ps_MaxMineralisationDepth(ps_MaxMineralisationDepth) {
  //, pm_CriticalMoistureDepth(pm_CriticalMoistureDepth) {
  debug() << "Constructor: SoilColumn " << soilParams.size() << endl;
  for (const auto& sp : soilParams) push_back(SoilLayer(ps_LayerThickness, sp));

  _vs_NumberOfOrganicLayers = calculateNumberOfOrganicLayers();
}

void SoilColumn::deserialize(mas::schema::model::monica::SoilColumnState::Reader reader) {
  vs_SurfaceWaterStorage = reader.getVsSurfaceWaterStorage();
  vs_InterceptionStorage = reader.getVsInterceptionStorage();
  vm_GroundwaterTableLayer = reader.getVmGroundwaterTable();
  vs_FluxAtLowerBoundary = reader.getVsFluxAtLowerBoundary();
  vq_CropNUptake = reader.getVqCropNUptake();
  vt_SoilSurfaceTemperature = reader.getVtSoilSurfaceTemperature();
  vm_SnowDepth = reader.getVmSnowDepth();
  ps_MaxMineralisationDepth = reader.getPsMaxMineralisationDepth();
  _vs_NumberOfOrganicLayers = (int)reader.getVsNumberOfOrganicLayers();
  _vf_TopDressing = reader.getVfTopDressing();
  _vf_TopDressingPartition.deserialize(reader.getVfTopDressingPartition());
  _vf_TopDressingDelay = reader.getVfTopDressingDelay();
  setFromComplexCapnpList(_delayedNMinApplications, reader.getDelayedNMinApplications());
  //pm_CriticalMoistureDepth = reader.getPmCriticalMoistureDepth();
  setFromComplexCapnpList(*this, reader.getLayers());
}

void SoilColumn::serialize(mas::schema::model::monica::SoilColumnState::Builder builder) const {
  builder.setVsSurfaceWaterStorage(vs_SurfaceWaterStorage);
  builder.setVsInterceptionStorage(vs_InterceptionStorage);
  builder.setVmGroundwaterTable((uint16_t)vm_GroundwaterTableLayer);
  builder.setVsFluxAtLowerBoundary(vs_FluxAtLowerBoundary);
  builder.setVqCropNUptake(vq_CropNUptake);
  builder.setVtSoilSurfaceTemperature(vt_SoilSurfaceTemperature);
  builder.setVmSnowDepth(vm_SnowDepth);
  builder.setPsMaxMineralisationDepth(ps_MaxMineralisationDepth);
  builder.setVsNumberOfOrganicLayers(_vs_NumberOfOrganicLayers);
  builder.setVfTopDressing(_vf_TopDressing);
  _vf_TopDressingPartition.serialize(builder.initVfTopDressingPartition());
  builder.setVfTopDressingDelay(_vf_TopDressingDelay);
  setComplexCapnpList(_delayedNMinApplications,
                      builder.initDelayedNMinApplications((capnp::uint)_delayedNMinApplications.size()));
  //builder.setPmCriticalMoistureDepth(pm_CriticalMoistureDepth);
  setComplexCapnpList(*this, builder.initLayers((capnp::uint)size()));
}


/**
 * @brief Calculates number of organic layers.
 *
 * Calculates number of organic layers in dependency on
 * the layer depth and the ps_MaxMineralisationDepth. Result is saved
 * in private member variable _vs_NumberOfOrganicLayers.
 */
int SoilColumn::calculateNumberOfOrganicLayers() {
  //std::cout << "--------- set_vs_NumberOfOrganicLayers -----------" << std::endl;
  double lsum = 0;
  int count = 0;
  for (int i = 0; i < size(); i++) {
    count++;
    lsum += at(i).vs_LayerThickness;

    if (lsum >= ps_MaxMineralisationDepth) break;
  }

  //std::cout << vs_NumberOfLayers() << std::endl;
  //std::cout << generalParams.ps_MaxMineralisationDepth << std::endl;
  //std::cout << _vs_NumberOfOrganicLayers << std::endl;

  return count;
}

double SoilColumn::applyMineralFertiliserViaNDemand(MineralFertilizerParameters fp,
                                                    double demandDepth,
                                                    double NdemandKgHa) {
  double sumSoilNkgHa = 0.0;
  int depthCm = 0;
  int i = 0;
  for (const auto& layer : *this) {
    double layerSize = layer.vs_LayerThickness;
    depthCm += int(layerSize * 100.0);

    //convert [kg N m-3] to [kg N ha-1]
    sumSoilNkgHa += (at(i).vs_SoilNO3 + at(i).vs_SoilNH4) * 10000.0 * layerSize;

    if (depthCm >= int(demandDepth * 100)) break;

    i++;
  }

  double fertilizerRecommendation = max(0.0, NdemandKgHa - sumSoilNkgHa);
  if (fertilizerRecommendation > 0) applyMineralFertiliser(fp, fertilizerRecommendation);

  return fertilizerRecommendation;
}

/**
 * Method for calculating fertilizer demand from crop demand and soil mineral
 * status (Nmin method).
 *
 * @param fp
 * @param vf_SamplingDepth
 * @param vf_CropNTarget N availability required by the crop down to rooting depth
 * @param vf_CropNTarget30 N availability required by the crop down to 30 cm
 * @param vf_FertiliserMaxApplication Maximal value of N that can be applied until the crop will be damaged
 * @param vf_FertiliserMinApplication Threshold value for economically reasonable fertilizer application
 * @param vf_TopDressingDelay Number of days for which the application of surplus fertilizer is delayed
 */
double SoilColumn::
applyMineralFertiliserViaNMinMethod(MineralFertilizerParameters fp,
                                    double vf_SamplingDepth,
                                    double vf_CropNTarget,
                                    double vf_CropNTarget30,
                                    double vf_FertiliserMinApplication,
                                    double vf_FertiliserMaxApplication,
                                    int vf_TopDressingDelay) {
  if (at(0).vs_SoilMoisture_m3 > at(0)._sps.vs_FieldCapacity) {
    _delayedNMinApplications.push_back({
                                         fp,
                                         vf_SamplingDepth,
                                         vf_CropNTarget,
                                         vf_CropNTarget30,
                                         vf_FertiliserMinApplication,
                                         vf_FertiliserMaxApplication,
                                         vf_TopDressingDelay
                                       }
                                      );

    debug() << "Soil too wet for fertilisation. Fertiliser event adjourned to next day." << endl;
    return 0.0;
  }

  auto vf_Layer30cm = getLayerNumberForDepth(0.3);
  auto layerSamplingDepth = getLayerNumberForDepth(vf_SamplingDepth);

  double vf_SoilNO3Sum = 0.0;
  double vf_SoilNH4Sum = 0.0;
  for (int i_Layer = 0;
       i_Layer < layerSamplingDepth /*(ceil(vf_SamplingDepth / at(i_Layer).vs_LayerThickness))*/; i_Layer++) {
    //vf_TargetLayer is in cm. We want number of layers
    vf_SoilNO3Sum += at(i_Layer).vs_SoilNO3; //! [kg N m-3]
    vf_SoilNH4Sum += at(i_Layer).vs_SoilNH4; //! [kg N m-3]
  }

  double vf_SoilNO3Sum30 = 0.0;
  double vf_SoilNH4Sum30 = 0.0;
  // Same calculation for a depth of 30 cm
  /** @todo Must be adapted when using variable layer depth. */
  for (int i_Layer = 0; i_Layer < vf_Layer30cm; i_Layer++) {
    vf_SoilNO3Sum30 += at(i_Layer).vs_SoilNO3; //! [kg N m-3]
    vf_SoilNH4Sum30 += at(i_Layer).vs_SoilNH4; //! [kg N m-3]
  }

  // Converts [kg N ha-1] to [kg N m-3]
  double vf_CropNTargetValue = vf_CropNTarget / 10000.0 / at(0).vs_LayerThickness;
  double vf_CropNTargetValue30 = vf_CropNTarget30 / 10000.0 / at(0).vs_LayerThickness;

  double vf_FertiliserDemandVol = vf_CropNTargetValue - (vf_SoilNO3Sum + vf_SoilNH4Sum);
  double vf_FertiliserDemandVol30 = vf_CropNTargetValue30 - (vf_SoilNO3Sum30 + vf_SoilNH4Sum30);

  // Converts fertiliser demand back from [kg N m-3] to [kg N ha-1]
  double vf_FertiliserDemand = vf_FertiliserDemandVol * 10000.0 * at(0).vs_LayerThickness;
  double vf_FertiliserDemand30 = vf_FertiliserDemandVol30 * 10000.0 * at(0).vs_LayerThickness;

  double vf_FertiliserRecommendation = max(vf_FertiliserDemand, vf_FertiliserDemand30);

  if (vf_FertiliserRecommendation < vf_FertiliserMinApplication) {
    // If the N demand of the crop is smaller than the user defined
    // minimum fertilisation then no need to fertilise
    vf_FertiliserRecommendation = 0.0;
  }

  if (vf_FertiliserRecommendation > vf_FertiliserMaxApplication) {
    // If the N demand of the crop is greater than the user defined
    // maximum fertilisation then need to split so surplus fertilizer can
    // be applied after a delay time
    _vf_TopDressing = vf_FertiliserRecommendation - vf_FertiliserMaxApplication;
    _vf_TopDressingPartition = fp;
    _vf_TopDressingDelay = vf_TopDressingDelay;
    vf_FertiliserRecommendation = vf_FertiliserMaxApplication;
  }

  //Apply fertiliser
  applyMineralFertiliser(fp, vf_FertiliserRecommendation);

  debug() << "SoilColumn::applyMineralFertiliserViaNMinMethod:\t" << vf_FertiliserRecommendation << endl;

  //apply the callback to all of the fertiliser, even though some if it
  //(the top-dressing) will only be applied later
  //we simply assume it really will be applied, in the worst case
  //the delay is so long, that the crop is already harvested until
  //the top-dressing will be applied
  return vf_FertiliserRecommendation;
}

// prüft ob top-dressing angewendet werden sollte, ansonsten wird
// zeitspanne nur reduziert

/**
 * Tests for every calculation step if a delayed fertilising should be applied.
 * If not, the delay time will be decremented. Otherwise the surplus fertiliser
 * stored in _vf_TopDressing is applied.
 *
 * @see ApplyFertiliser
 */
double SoilColumn::applyPossibleTopDressing() {
  double amount = 0;

  if (_vf_TopDressingDelay > 0) {
    _vf_TopDressingDelay--;
  } else if (_vf_TopDressingDelay == 0
             && _vf_TopDressing > 0.0) {
    amount = _vf_TopDressing;
    applyMineralFertiliser(_vf_TopDressingPartition, amount);
    _vf_TopDressing = 0;
  }
  return amount;
}

/**
 * Calls function for applying delayed fertilizer and
 * then removes the first fertilizer item in list.
 */
double SoilColumn::applyPossibleDelayedFerilizer() {
  auto delayedApps = _delayedNMinApplications;
  double n_amount = 0.0;
  while (!delayedApps.empty()) {
    const auto& da = delayedApps.front();
    n_amount += applyMineralFertiliserViaNMinMethod(
                                                    da.fp, da.vf_SamplingDepth, da.vf_CropNTarget, da.vf_CropNTarget30,
                                                    da.vf_FertiliserMinApplication, da.vf_FertiliserMaxApplication,
                                                    da.vf_TopDressingDelay);
    delayedApps.pop_front();
    _delayedNMinApplications.pop_front();
  }
  return n_amount;
}

/**
 * @brief Applies mineral fertiliser
 *
 * @author: Claas Nendel
 */
void SoilColumn::applyMineralFertiliser(MineralFertilizerParameters fp,
                                        double amount) {
  debug() << "SoilColumn::applyMineralFertilser: params: " << fp.toString()
    << " amount: " << amount << endl;
  // [kg N ha-1 -> kg m-3]
  double kgHaTokgm3 = 10000.0 * at(0).vs_LayerThickness;
  at(0).vs_SoilNO3 += amount * fp.getNO3() / kgHaTokgm3;
  at(0).vs_SoilNH4 += amount * fp.getNH4() / kgHaTokgm3;
  at(0).vs_SoilCarbamid += amount * fp.getCarbamid() / kgHaTokgm3;
}


/**
 * @brief Checks and deletes AOM pool
 *
 * This method checks the content of each AOM Pool. In case the sum over all
 * layers of a respective pool is very low the pool will be deleted from the
 * list.
 *
 * @author: Claas Nendel
 */
void SoilColumn::deleteAOMPool() {
  for (unsigned int i_AOMPool = 0; i_AOMPool < at(0).vo_AOM_Pool.size();) {
    double vo_SumAOM_Slow = 0.0;
    double vo_SumAOM_Fast = 0.0;

    for (int i_Layer = 0; i_Layer < _vs_NumberOfOrganicLayers; i_Layer++) {
      vo_SumAOM_Slow += at(i_Layer).vo_AOM_Pool.at(i_AOMPool).vo_AOM_Slow;
      vo_SumAOM_Fast += at(i_Layer).vo_AOM_Pool.at(i_AOMPool).vo_AOM_Fast;
    }

    //cout << "Pool " << i_AOMPool << " -> Slow: " << vo_SumAOM_Slow << "; Fast: " << vo_SumAOM_Fast << endl;

    if ((vo_SumAOM_Slow + vo_SumAOM_Fast) < 0.00001) {
      for (int i_Layer = 0; i_Layer < _vs_NumberOfOrganicLayers; i_Layer++) {
        auto it_AOMPool = at(i_Layer).vo_AOM_Pool.begin();
        it_AOMPool += i_AOMPool;
        at(i_Layer).vo_AOM_Pool.erase(it_AOMPool);
      }
      //cout << "Pool " << i_AOMPool << " deleted" << endl;
    } else {
      i_AOMPool++;
    }
  }
}

/**
 * Method for calculating irrigation demand from soil moisture status.
 * The trigger will be activated and deactivated according to crop parameters
 * (temperature sum)
 *
 * @param automatic irrigation parameters
 * @return could irrigation be applied and how much has been applied
 */
std::pair<bool, double> SoilColumn::applyIrrigationViaTrigger(const AutomaticIrrigationParameters& aips) {
  if (!cropModule) return std::make_pair(false, 0);

  double s = cropModule->pc_HeatSumIrrigationStart;
  double e = cropModule->pc_HeatSumIrrigationEnd;
  double cts = cropModule->vc_CurrentTotalTemperatureSum;
  if (cts < s || cts > e || aips.threshold < 0.0) return std::make_pair(false, 0);

  double actPAW = 0.0; // actualPlantAvailableWater
  double maxPAW = 0.0; // maxPlantAvailableWater
  double layerDepthM = 0;
  for (int i = 0; i < size() && layerDepthM < aips.criticalMoistureDepthM; i++) {
    const auto& li = at(i);
    auto smi = li.vs_SoilMoisture_m3;
    auto fci = li._sps.vs_FieldCapacity;
    auto pwpi = li._sps.vs_PermanentWiltingPoint;
    auto lti = li.vs_LayerThickness;

    actPAW += (smi - pwpi) * lti * 1000.0; // [mm]
    maxPAW += (fci - pwpi) * lti * 1000.0; // [mm]

    layerDepthM += lti;
  }
  if (Tools::flt_equal_zero(maxPAW)) return std::make_pair(false, 0);
  const double fractPAW = actPAW / maxPAW;
  if (fractPAW <= aips.threshold) {
    double addedIrrigationWater = 0;
    if (aips.amount > 0.0) {
      applyIrrigation(aips.amount, aips.nitrateConcentration);
      addedIrrigationWater = aips.amount;
    } else if (aips.percentNFC > 0.0) {
      layerDepthM = 0;
      for (int i = 0; i < size() && layerDepthM < aips.criticalMoistureDepthM; i++) {
        auto& li = at(i);
        auto smi = li.vs_SoilMoisture_m3;
        auto fci = li._sps.vs_FieldCapacity;
        auto pwpi = li._sps.vs_PermanentWiltingPoint;
        auto lti = li.vs_LayerThickness;

        double percentNFCi = (fci - pwpi) * aips.percentNFC / 100.0;
        double pawi = smi - pwpi;
        double addedIrrigationWaterAtLayer = std::max(0.0, percentNFCi - pawi);
        addedIrrigationWater += addedIrrigationWaterAtLayer;
        li.vs_SoilMoisture_m3 = percentNFCi + pwpi;
        double nitrateAddedViaIrrigation = // -> //[kg m-3]
          aips.nitrateConcentration * // [mg dm-3]
          addedIrrigationWaterAtLayer / //[dm3 m-2]
          li.vs_LayerThickness / 1000000.0; // [m]
        li.vs_SoilNO3 += nitrateAddedViaIrrigation;

        layerDepthM += lti;
      }
    } else {
      return make_pair(false, 0);
    }

    debug() << "applying automatic irrigation threshold: " << aips.threshold
      << " amount: " << aips.amount
      << " N concentration: " << aips.nitrateConcentration << endl;

    return make_pair(true, addedIrrigationWater);
  }

  return make_pair(false, 0);
}


/**
 * @brief Applies irrigation
 *
 * @author: Claas Nendel
 */
void SoilColumn::applyIrrigation(double amount, double nitrateConcentration) {
  // Adding irrigation water amount to surface water storage
  vs_SurfaceWaterStorage += amount; // [mm]
  double nitrateAddedViaIrrigation = // -> //[kg m-3]
    nitrateConcentration * // [mg dm-3]
    amount / //[dm3 m-2]
    at(0).vs_LayerThickness / 1000000.0; // [m]

  // adding N from irrigation water to top soil nitrate pool
  at(0).vs_SoilNO3 += nitrateAddedViaIrrigation;
}


/**
 * Applies tillage to effected layers. Parameters for effected soil layers
 * are averaged.
 * @param depth Depth of affected soil.
 */
void SoilColumn::applyTillage(double depth) {
  auto layer_index = getLayerNumberForDepth(depth) + 1;

  double soil_organic_carbon = 0.0;
  double soil_organic_matter = 0.0;
  double soil_temperature = 0.0;
  double soil_moisture = 0.0;
  //double soil_moistureOld = 0.0;
  double som_slow = 0.0;
  double som_fast = 0.0;
  double smb_slow = 0.0;
  double smb_fast = 0.0;
  double carbamid = 0.0;
  double nh4 = 0.0;
  double no2 = 0.0;
  double no3 = 0.0;

  // add up all parameters that are affected by tillage
  for (size_t i = 0; i < layer_index; i++) {
    soil_organic_carbon += at(i)._sps.vs_SoilOrganicCarbon();
    //soil_organic_matter += at(i).vs_SoilOrganicMatter();
    soil_temperature += at(i).vs_SoilTemperature;
    soil_moisture += at(i).vs_SoilMoisture_m3;
    //soil_moistureOld += at(i).vs_SoilMoistureOld_m3;
    som_slow += at(i).vs_SOM_Slow;
    som_fast += at(i).vs_SOM_Fast;
    smb_slow += at(i).vs_SMB_Slow;
    smb_fast += at(i).vs_SMB_Fast;
    carbamid += at(i).vs_SoilCarbamid;
    nh4 += at(i).vs_SoilNH4;
    no2 += at(i).vs_SoilNO2;
    no3 += at(i).vs_SoilNO3;
  }

  auto li = double(layer_index);

  // calculate mean value of accumulated soil parameters
  soil_organic_carbon /= li;
  //soil_organic_matter /= li;
  soil_temperature /= li;
  soil_moisture /= li;
  //soil_moistureOld /= li;
  som_slow /= li;
  som_fast /= li;
  smb_slow /= li;
  smb_fast /= li;
  carbamid /= li;
  nh4 /= li;
  no2 /= li;
  no3 /= li;

  // use calculated mean values for all affected layers
  for (size_t i = 0; i < layer_index; i++) {
    //assert((soil_organic_carbon - (soil_organic_matter * OrganicConstants::po_SOM_to_C)) < 0.00001);
    at(i)._sps.set_vs_SoilOrganicCarbon(soil_organic_carbon);
    //at(i).set_SoilOrganicMatter(soil_organic_matter);
    at(i).vs_SoilTemperature = soil_temperature;
    at(i).vs_SoilMoisture_m3 = soil_moisture;
    //at(i).vs_SoilMoistureOld_m3 = soil_moistureOld;
    at(i).vs_SOM_Slow = som_slow;
    at(i).vs_SOM_Fast = som_fast;
    at(i).vs_SMB_Slow = smb_slow;
    at(i).vs_SMB_Fast = smb_fast;
    at(i).vs_SoilCarbamid = carbamid;
    at(i).vs_SoilNH4 = nh4;
    at(i).vs_SoilNO2 = no2;
    at(i).vs_SoilNO3 = no3;
  }

  // merge aom pool
  auto aom_pool_count = at(0).vo_AOM_Pool.size();

  if (aom_pool_count > 0) {
    vector<double> aom_slow(aom_pool_count);
    vector<double> aom_fast(aom_pool_count);

    // initialization of aom pool accumulator
    for (unsigned int pool_index = 0; pool_index < aom_pool_count; pool_index++) {
      aom_slow[pool_index] = 0.0;
      aom_fast[pool_index] = 0.0;
    }

    layer_index = min(layer_index, size_t(_vs_NumberOfOrganicLayers));

    //cout << "Soil parameters before applying tillage for the first "<< layer_index+1 << " layers: " << endl;

    // add up pools for affected layer with same index
    for (size_t j = 0; j < layer_index; j++) {
      //cout << "Layer " << j << endl << endl;

      SoilLayer& layer = at(j);
      size_t pool_index = 0;
      for (auto aomp : layer.vo_AOM_Pool) {
        aom_slow[pool_index] += aomp.vo_AOM_Slow;
        aom_fast[pool_index] += aomp.vo_AOM_Fast;

        //cout << "AOMPool " << pool_index << endl;
        //cout << "vo_AOM_Slow:\t"<< aomp.vo_AOM_Slow << endl;
        //cout << "vo_AOM_Fast:\t"<< aomp.vo_AOM_Fast << endl;

        pool_index++;
      }
    }

    //
    for (size_t pool_index = 0; pool_index < aom_pool_count; pool_index++) {
      aom_slow[pool_index] = aom_slow[pool_index] / li;
      aom_fast[pool_index] = aom_fast[pool_index] / li;
    }

    //cout << "Soil parameters after applying tillage for the first "<< layer_index+1 << " layers: " << endl;

    // rewrite parameters of aom pool with mean values
    for (size_t j = 0; j < layer_index; j++) {
      SoilLayer& layer = at(j);
      //cout << "Layer " << j << endl << endl;
      size_t pool_index = 0;
      for (auto aomp : layer.vo_AOM_Pool) {
        aomp.vo_AOM_Slow = aom_slow[pool_index];
        aomp.vo_AOM_Fast = aom_fast[pool_index];

        //cout << "AOMPool " << pool_index << endl;
        //cout << "vo_AOM_Slow:\t"<< aomp.vo_AOM_Slow << endl;
        //cout << "vo_AOM_Fast:\t"<< aomp.vo_AOM_Fast << endl;

        pool_index++;
      }
    }
  }

  //cout << "soil_organic_carbon: " << soil_organic_carbon << endl;
  //cout << "soil_organic_matter: " << soil_organic_matter << endl;
  //cout << "soil_temperature: " << soil_temperature << endl;
  //cout << "soil_moisture: " << soil_moisture << endl;
  //cout << "soil_moistureOld: " << soil_moistureOld << endl;
  //cout << "som_slow: " << som_slow << endl;
  //cout << "som_fast: " << som_fast << endl;
  //cout << "smb_slow: " << smb_slow << endl;
  //cout << "smb_fast: " << smb_fast << endl;
  //cout << "carbamid: " << carbamid << endl;
  //cout << "nh4: " << nh4 << endl;
  //cout << "no3: " << no3 << endl << endl;
}

/**
 * @brief Returns index of layer that lays in the given depth.
 * @param depth Depth in meters
 * @return Index of layer
 */
size_t SoilColumn::getLayerNumberForDepth(double depth) const {
  size_t layer = 0;
  double accu_depth = 0;
  double layer_thickness = at(0).vs_LayerThickness;

  // find number of layer that lay between the given depth
  for (size_t i = 0, _size = size(); i < _size; i++) {
    accu_depth += layer_thickness;
    if (depth <= accu_depth) break;
    layer++;
  }

  return layer;
}

/**
 * Returns sum of soiltemperature for several soil layers.
 * @param layers Number of layers that are of interest
 * @return Temperature sum
 */
double SoilColumn::sumSoilTemperature(int layers) const {
  double accu = 0.0;
  for (int i = 0; i < layers; i++) accu += at(i).vs_SoilTemperature;
  return accu;
}

//------------------------------------------------------------------------------

void SoilColumn::DelayedNMinApplicationParams::deserialize(
  mas::schema::model::monica::SoilColumnState::DelayedNMinApplicationParams::Reader reader) {
  fp.deserialize(reader.getFp());
  vf_SamplingDepth = reader.getSamplingDepth();
  vf_CropNTarget = reader.getCropNTarget();
  vf_CropNTarget30 = reader.getCropNTarget30();
  vf_FertiliserMinApplication = reader.getFertiliserMinApplication();
  vf_FertiliserMaxApplication = reader.getFertiliserMaxApplication();
  vf_TopDressingDelay = (int)reader.getTopDressingDelay();
}

void SoilColumn::DelayedNMinApplicationParams::serialize(
  mas::schema::model::monica::SoilColumnState::DelayedNMinApplicationParams::Builder builder) const {
  fp.serialize(builder.initFp());
  builder.setSamplingDepth(vf_SamplingDepth);
  builder.setCropNTarget(vf_CropNTarget);
  builder.setCropNTarget30(vf_CropNTarget30);
  builder.setFertiliserMinApplication(vf_FertiliserMinApplication);
  builder.setFertiliserMaxApplication(vf_FertiliserMaxApplication);
  builder.setTopDressingDelay(vf_TopDressingDelay);
}

/**
 * @brief Constructor
 *
 * Parameter initialization.
 */
/*
FertilizerTriggerThunk::FertilizerTriggerThunk(
MineralFertiliserPartition fp,
SoilColumn* sc,
double vf_SamplingDepth,
double vf_CropNTargetValue,
double vf_CropNTargetValue30,
double vf_FertiliserMaxApplication,
double vf_FertiliserMinApplication,
int vf_TopDressingDelay)
: _fp(fp), _sc(sc),
_sd(vf_SamplingDepth),
_cntv(vf_CropNTargetValue),
_cntv30(vf_CropNTargetValue30),
_fmaxa(vf_FertiliserMaxApplication),
_fmina(vf_FertiliserMinApplication),
_tdd(vf_TopDressingDelay)
{

}
 */

/**
 * Starts applying of fertilizer in the soil column according
 * to specified type (nmin or manual).
 * @todo Frage an Micha: Warum wurd operator überladen anstatt eine methode zu implementieren?
 * @todo Micha: Das haut so nicht hin. Beide Fertiliser events haben unterschiedliche Parameter-Anforderungen!
 */
/*
void FertilizerTriggerThunk::operator()() const {
_sc->nMinFertiliserTrigger(_fp, _sd, _cntv, _cntv30, _fmaxa, _fmina, _tdd);
}
*/

kj::Own<SoilColumn> monica::makeSoilColumn(double layerThickness,
                                           double maxMineralisationDepth,
                                           const Soil::SoilPMs& soilParams) {
  return kj::heap<SoilColumn>(layerThickness, maxMineralisationDepth, soilParams);
}

kj::Own<SoilColumn> monica::makeSoilColumn(mas::schema::model::monica::SoilColumnState::Reader reader,
                                           CropModule* cropModule) {
  return kj::heap<SoilColumn>(reader, cropModule);
}

void monica::soilColumnDeserialize(SoilColumn* sc, mas::schema::model::monica::SoilColumnState::Reader reader) {
  sc->deserialize(reader);
}

void monica::soilColumnSerialize(const SoilColumn* sc, mas::schema::model::monica::SoilColumnState::Builder builder) {
  sc->serialize(builder);
}

void monica::soilColumnPutCrop(SoilColumn* sc, CropModule* cm) { sc->cropModule = cm; }
void monica::soilColumnRemoveCrop(SoilColumn* sc) { sc->cropModule = nullptr; }
void monica::soilColumnClearTopDressingParams(SoilColumn* sc) { sc->_vf_TopDressing = 0.0, sc->_vf_TopDressingDelay = 0; }
void monica::soilColumnDeleteAOMPool(SoilColumn* sc) { sc->deleteAOMPool(); }
double monica::soilColumnApplyPossibleDelayedFerilizer(SoilColumn* sc) { return sc->applyPossibleDelayedFerilizer(); }
double monica::soilColumnApplyPossibleTopDressing(SoilColumn* sc) { return sc->applyPossibleTopDressing(); }
void monica::soilColumnApplyMineralFertiliser(SoilColumn* sc, MineralFertilizerParameters fp, double amount) {
  sc->applyMineralFertiliser(fp, amount);
}
double monica::soilColumnApplyMineralFertiliserViaNMinMethod(SoilColumn* sc,
                                                              MineralFertilizerParameters fertiliserPartition,
                                                              double samplingDepth,
                                                              double cropNTargetValue,
                                                              double cropNTargetValue30,
                                                              double fertiliserMaxApplication,
                                                              double fertiliserMinApplication,
                                                              int topDressingDelay) {
  return sc->applyMineralFertiliserViaNMinMethod(fertiliserPartition,
                                                 samplingDepth,
                                                 cropNTargetValue,
                                                 cropNTargetValue30,
                                                 fertiliserMaxApplication,
                                                 fertiliserMinApplication,
                                                 topDressingDelay);
}
std::pair<bool, double> monica::soilColumnApplyIrrigationViaTrigger(SoilColumn* sc,
                                                                     const AutomaticIrrigationParameters& aips) {
  return sc->applyIrrigationViaTrigger(aips);
}
void monica::soilColumnApplyIrrigation(SoilColumn* sc, double amount, double nitrateConcentration) {
  sc->applyIrrigation(amount, nitrateConcentration);
}
void monica::soilColumnApplyTillage(SoilColumn* sc, double depth) { sc->applyTillage(depth); }
