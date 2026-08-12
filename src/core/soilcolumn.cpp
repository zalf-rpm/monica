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

#include "soilcolumn.h"

#include <algorithm>
#include <cmath>

#include "crop-module.h"
#include "tools/debug.h"

using namespace monica;
using namespace std;
using namespace Soil;
using namespace Tools;

void AOM_Properties::deserialize(
    mas::schema::model::monica::AOMProperties::Reader reader) {
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

void AOM_Properties::serialize(
    mas::schema::model::monica::AOMProperties::Builder builder) const {
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
 * @param vs_LayerThickness Vertical expansion
 * @param sps Soil parameters
 */
SoilLayer monica::makeSoilLayer(double vs_LayerThickness,
                                const SoilParameters &sps) {
  SoilLayer sl;
  sl.vs_LayerThickness = vs_LayerThickness;
  sl.vs_SoilNH4 = sps.vs_SoilAmmonium;
  sl.vs_SoilNO3 = sps.vs_SoilNitrate;
  sl.sps = sps;
  sl.vs_SoilMoisture_m3 =
      sps.vs_FieldCapacity * sps.vs_SoilMoisturePercentFC / 100.0;
  return sl;
}

void soillayer::deserialize(
    SoilLayer *sl, mas::schema::model::monica::SoilLayerState::Reader reader) {
  sl->vs_LayerThickness = reader.getLayerThickness();
  sl->vs_SoilWaterFlux = reader.getSoilWaterFlux();
  setFromComplexCapnpList(sl->vo_AOM_Pool, reader.getVoAOMPool());
  sl->vs_SOM_Slow = reader.getSomSlow();
  sl->vs_SOM_Fast = reader.getSomFast();
  sl->vs_SMB_Slow = reader.getSmbSlow();
  sl->vs_SMB_Fast = reader.getSmbFast();
  sl->vs_SoilCarbamid = reader.getSoilCarbamid();
  sl->vs_SoilNH4 = reader.getSoilNH4();
  sl->vs_SoilNO2 = reader.getSoilNO2();
  sl->vs_SoilNO3 = reader.getSoilNO3();
  sl->vs_SoilFrozen = reader.getSoilFrozen();
  sl->sps.deserialize(reader.getSps());
  sl->vs_SoilMoisture_m3 = reader.getSoilMoistureM3();
  sl->vs_SoilTemperature = reader.getSoilTemperature();
}

void soillayer::serialize(
    const SoilLayer *sl,
    mas::schema::model::monica::SoilLayerState::Builder builder) {
  builder.setLayerThickness(sl->vs_LayerThickness);
  builder.setSoilWaterFlux(sl->vs_SoilWaterFlux);
  setComplexCapnpList(
      sl->vo_AOM_Pool,
      builder.initVoAOMPool((capnp::uint)sl->vo_AOM_Pool.size()));
  builder.setSomSlow(sl->vs_SOM_Slow);
  builder.setSomFast(sl->vs_SOM_Fast);
  builder.setSmbSlow(sl->vs_SMB_Slow);
  builder.setSmbFast(sl->vs_SMB_Fast);
  builder.setSoilCarbamid(sl->vs_SoilCarbamid);
  builder.setSoilNH4(sl->vs_SoilNH4);
  builder.setSoilNO2(sl->vs_SoilNO2);
  builder.setSoilNO3(sl->vs_SoilNO3);
  builder.setSoilFrozen(sl->vs_SoilFrozen);
  sl->sps.serialize(builder.initSps());
  builder.setSoilMoistureM3(sl->vs_SoilMoisture_m3);
  builder.setSoilTemperature(sl->vs_SoilTemperature);
}

/**
 * Soil layer's moisture content, expressed as logarithm of
 * pressure head in cm water column. Algorithm of Van Genuchten is used.
 * Conversion of water saturation into soil-moisture tension.
 *
 * @todo Einheiten prüfen
 */
double soillayer::soilMoisturePF(const SoilLayer *sl) {
  // Derivation of Van Genuchten parameters (Vereecken at al. 1989)

  auto ps = calcVanGenuchtenVereeckenParams(
      sl->sps.vs_PermanentWiltingPoint, sl->sps.vs_Saturation,
      sl->sps.vs_SoilSandContent, sl->sps.vs_SoilClayContent,
      sl->sps.vs_SoilBulkDensity(), sl->sps.vs_SoilOrganicCarbon());

  // Van Genuchten retention curve
  auto sm = sl->vs_SoilMoisture_m3;
  double matricHead =
      sm <= ps.thetaR
          ? 5.0E+7
          : (1.0 / ps.alpha) *
                (pow(pow((ps.thetaS - ps.thetaR) / (sm - ps.thetaR), 1 / ps.m) -
                         1,
                     1 / ps.n));
  double soilMoisture_pF = log10(matricHead);

  /* JV! set _vs_SoilMoisture_pF to "small" number in case of vs_Theta "close"
   * to vs_ThetaS (vs_Psi < 1 -> log(vs_Psi) < 0) */
  return soilMoisture_pF < 0.0 ? 5.0E-7 : soilMoisture_pF;
  //  debug() << "vs_SoilMoisture_pF: " << soilMoisture_pF << std::endl;
}

double soillayer::soilNmin(const SoilLayer *sl) {
  return sl->vs_SoilNO3 + sl->vs_SoilNO2 + sl->vs_SoilNH4;
}

//------------------------------------------------------------------------------

void SoilColumn::DelayedNMinApplicationParams::deserialize(
    mas::schema::model::monica::SoilColumnState::DelayedNMinApplicationParams::
        Reader reader) {
  mineralfertilizerparameters::deserialize(&fp, reader.getFp());
  vf_SamplingDepth = reader.getSamplingDepth();
  vf_CropNTarget = reader.getCropNTarget();
  vf_CropNTarget30 = reader.getCropNTarget30();
  vf_FertiliserMinApplication = reader.getFertiliserMinApplication();
  vf_FertiliserMaxApplication = reader.getFertiliserMaxApplication();
  vf_TopDressingDelay = (int)reader.getTopDressingDelay();
}

void SoilColumn::DelayedNMinApplicationParams::serialize(
    mas::schema::model::monica::SoilColumnState::DelayedNMinApplicationParams::
        Builder builder) const {
  mineralfertilizerparameters::serialize(&fp, builder.initFp());
  builder.setSamplingDepth(vf_SamplingDepth);
  builder.setCropNTarget(vf_CropNTarget);
  builder.setCropNTarget30(vf_CropNTarget30);
  builder.setFertiliserMinApplication(vf_FertiliserMinApplication);
  builder.setFertiliserMaxApplication(vf_FertiliserMaxApplication);
  builder.setTopDressingDelay(vf_TopDressingDelay);
}

/**
 * Constructs every layer in the vector with the layer-thickness and the
 * matching soil parameters for that layer.
 *
 * @param layerThickness Vertical expansion
 * @param maxMineralisationDepth
 * @param soilParams Soil Parameter
 */
kj::Own<SoilColumn> monica::makeSoilColumn(double layerThickness,
                                           double maxMineralisationDepth,
                                           const Soil::SoilPMs &soilParams) {
  auto sc = kj::heap<SoilColumn>();
  sc->ps_MaxMineralisationDepth = maxMineralisationDepth;
  debug() << "makeSoilColumn: " << soilParams.size() << endl;
  for (const auto &sp : soilParams) {
    sc->push_back(makeSoilLayer(layerThickness, sp));
  }
  sc->vs_NumberOfOrganicLayers =
      soilcolumn::calculateNumberOfOrganicLayers(sc.get());
  return sc;
}

kj::Own<SoilColumn> monica::makeSoilColumn(
    mas::schema::model::monica::SoilColumnState::Reader reader,
    CropModule *cropModule) {
  auto sc = kj::heap<SoilColumn>();
  sc->cropModule = cropModule;
  soilcolumn::deserialize(sc.get(), reader);
  return sc;
}

/**
 * @brief Calculates number of organic layers.
 *
 * Calculates number of organic layers in dependency on
 * the layer depth and the ps_MaxMineralisationDepth.
 */
int monica::soilcolumn::calculateNumberOfOrganicLayers(const SoilColumn *sc) {
  double lsum = 0;
  int count = 0;
  for (int i = 0; i < sc->size(); i++) {
    count++;
    lsum += sc->at(i).vs_LayerThickness;

    if (lsum >= sc->ps_MaxMineralisationDepth)
      break;
  }

  return count;
}

/**
 * @brief Returns index of layer that lays in the given depth.
 * @param depth Depth in meters
 * @return Index of layer
 */
size_t monica::soilcolumn::getLayerNumberForDepth(const SoilColumn *sc,
                                                  double depth) {
  size_t layer = 0;
  double accu_depth = 0;
  double layer_thickness = sc->at(0).vs_LayerThickness;

  // find number of layer that lay between the given depth
  for (size_t i = 0, _size = sc->size(); i < _size; i++) {
    accu_depth += layer_thickness;
    if (depth <= accu_depth)
      break;
    layer++;
  }

  return layer;
}

/**
 * Returns sum of soiltemperature for several soil layers.
 * @param layers Number of layers that are of interest
 * @return Temperature sum
 */
double monica::soilcolumn::sumSoilTemperature(const SoilColumn *sc,
                                              int layers) {
  double accu = 0.0;
  for (int i = 0; i < layers; i++)
    accu += sc->at(i).vs_SoilTemperature;
  return accu;
}

double monica::soilcolumn::applyMineralFertiliserViaNDemand(
    SoilColumn *sc, MineralFertilizerParameters fp, double demandDepth,
    double NdemandKgHa) {
  double sumSoilNkgHa = 0.0;
  int depthCm = 0;
  int i = 0;
  for (const auto &layer : *sc) {
    double layerSize = layer.vs_LayerThickness;
    depthCm += int(layerSize * 100.0);

    // convert [kg N m-3] to [kg N ha-1]
    sumSoilNkgHa +=
        (sc->at(i).vs_SoilNO3 + sc->at(i).vs_SoilNH4) * 10000.0 * layerSize;

    if (depthCm >= int(demandDepth * 100))
      break;

    i++;
  }

  double fertilizerRecommendation = max(0.0, NdemandKgHa - sumSoilNkgHa);
  if (fertilizerRecommendation > 0)
    applyMineralFertiliser(sc, fp, fertilizerRecommendation);

  return fertilizerRecommendation;
}

void monica::soilcolumn::deserialize(
    SoilColumn *sc,
    mas::schema::model::monica::SoilColumnState::Reader reader) {
  sc->vs_SurfaceWaterStorage = reader.getVsSurfaceWaterStorage();
  sc->vs_InterceptionStorage = reader.getVsInterceptionStorage();
  sc->vm_GroundwaterTableLayer = reader.getVmGroundwaterTable();
  sc->vs_FluxAtLowerBoundary = reader.getVsFluxAtLowerBoundary();
  sc->vq_CropNUptake = reader.getVqCropNUptake();
  sc->vt_SoilSurfaceTemperature = reader.getVtSoilSurfaceTemperature();
  sc->vm_SnowDepth = reader.getVmSnowDepth();
  sc->ps_MaxMineralisationDepth = reader.getPsMaxMineralisationDepth();
  sc->vs_NumberOfOrganicLayers = (int)reader.getVsNumberOfOrganicLayers();
  sc->vf_TopDressing = reader.getVfTopDressing();
  mineralfertilizerparameters::deserialize(&sc->vf_TopDressingPartition,
                                           reader.getVfTopDressingPartition());
  sc->vf_TopDressingDelay = reader.getVfTopDressingDelay();
  setFromComplexCapnpList(sc->_delayedNMinApplications,
                          reader.getDelayedNMinApplications());
  // pm_CriticalMoistureDepth = reader.getPmCriticalMoistureDepth();
  auto layers = reader.getLayers();
  sc->resize(layers.size());
  uint32_t i = 0;
  for (auto &layer : *sc)
    soillayer::deserialize(&layer, layers[i++]);
}

void monica::soilcolumn::serialize(
    const SoilColumn *sc,
    mas::schema::model::monica::SoilColumnState::Builder builder) {
  builder.setVsSurfaceWaterStorage(sc->vs_SurfaceWaterStorage);
  builder.setVsInterceptionStorage(sc->vs_InterceptionStorage);
  builder.setVmGroundwaterTable((uint16_t)sc->vm_GroundwaterTableLayer);
  builder.setVsFluxAtLowerBoundary(sc->vs_FluxAtLowerBoundary);
  builder.setVqCropNUptake(sc->vq_CropNUptake);
  builder.setVtSoilSurfaceTemperature(sc->vt_SoilSurfaceTemperature);
  builder.setVmSnowDepth(sc->vm_SnowDepth);
  builder.setPsMaxMineralisationDepth(sc->ps_MaxMineralisationDepth);
  builder.setVsNumberOfOrganicLayers(sc->vs_NumberOfOrganicLayers);
  builder.setVfTopDressing(sc->vf_TopDressing);
  mineralfertilizerparameters::serialize(&sc->vf_TopDressingPartition,
                                         builder.initVfTopDressingPartition());
  builder.setVfTopDressingDelay(sc->vf_TopDressingDelay);
  setComplexCapnpList(sc->_delayedNMinApplications,
                      builder.initDelayedNMinApplications(
                          (capnp::uint)sc->_delayedNMinApplications.size()));
  // builder.setPmCriticalMoistureDepth(pm_CriticalMoistureDepth);
  auto layersBuilder = builder.initLayers((capnp::uint)sc->size());
  uint32_t i = 0;
  for (const auto &layer : *sc)
    soillayer::serialize(&layer, layersBuilder[i++]);
}

void monica::soilcolumn::putCrop(SoilColumn *sc, CropModule *cm) {
  sc->cropModule = cm;
}
void monica::soilcolumn::removeCrop(SoilColumn *sc) {
  sc->cropModule = nullptr;
}
void monica::soilcolumn::clearTopDressingParams(SoilColumn *sc) {
  sc->vf_TopDressing = 0.0, sc->vf_TopDressingDelay = 0;
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
void monica::soilcolumn::deleteAOMPool(SoilColumn *sc) {
  for (unsigned int i_AOMPool = 0; i_AOMPool < sc->at(0).vo_AOM_Pool.size();) {
    double vo_SumAOM_Slow = 0.0;
    double vo_SumAOM_Fast = 0.0;

    for (int i_Layer = 0; i_Layer < sc->vs_NumberOfOrganicLayers; i_Layer++) {
      vo_SumAOM_Slow += sc->at(i_Layer).vo_AOM_Pool.at(i_AOMPool).vo_AOM_Slow;
      vo_SumAOM_Fast += sc->at(i_Layer).vo_AOM_Pool.at(i_AOMPool).vo_AOM_Fast;
    }

    // cout << "Pool " << i_AOMPool << " -> Slow: " << vo_SumAOM_Slow << ";
    // Fast: " << vo_SumAOM_Fast << endl;

    if ((vo_SumAOM_Slow + vo_SumAOM_Fast) < 0.00001) {
      for (int i_Layer = 0; i_Layer < sc->vs_NumberOfOrganicLayers; i_Layer++) {
        auto it_AOMPool = sc->at(i_Layer).vo_AOM_Pool.begin();
        it_AOMPool += i_AOMPool;
        sc->at(i_Layer).vo_AOM_Pool.erase(it_AOMPool);
      }
      // cout << "Pool " << i_AOMPool << " deleted" << endl;
    } else {
      i_AOMPool++;
    }
  }
}
/**
 * Calls function for applying delayed fertilizer and
 * then removes the first fertilizer item in list.
 */
double monica::soilcolumn::applyPossibleDelayedFerilizer(SoilColumn *sc) {
  auto delayedApps = sc->_delayedNMinApplications;
  double n_amount = 0.0;
  while (!delayedApps.empty()) {
    const auto &da = delayedApps.front();
    n_amount += applyMineralFertiliserViaNMinMethod(
        sc, da.fp, da.vf_SamplingDepth, da.vf_CropNTarget, da.vf_CropNTarget30,
        da.vf_FertiliserMinApplication, da.vf_FertiliserMaxApplication,
        da.vf_TopDressingDelay);
    delayedApps.pop_front();
    sc->_delayedNMinApplications.pop_front();
  }
  return n_amount;
}
/**
 * Tests for every calculation step if a delayed fertilising should be applied.
 * If not, the delay time will be decremented. Otherwise the surplus fertiliser
 * stored in _vf_TopDressing is applied.
 *
 * @see ApplyFertiliser
 */
double monica::soilcolumn::applyPossibleTopDressing(SoilColumn *sc) {
  double amount = 0;

  if (sc->vf_TopDressingDelay > 0) {
    sc->vf_TopDressingDelay--;
  } else if (sc->vf_TopDressingDelay == 0 && sc->vf_TopDressing > 0.0) {
    amount = sc->vf_TopDressing;
    applyMineralFertiliser(sc, sc->vf_TopDressingPartition, amount);
    sc->vf_TopDressing = 0;
  }
  return amount;
}
/**
 * @brief Applies mineral fertiliser
 *
 * @author: Claas Nendel
 */
void monica::soilcolumn::applyMineralFertiliser(SoilColumn *sc,
                                                MineralFertilizerParameters fp,
                                                double amount) {
  debug() << "SoilColumn::applyMineralFertilser: params: "
          << mineralfertilizerparameters::to_json(&fp).dump()
          << " amount: " << amount << endl;
  // [kg N ha-1 -> kg m-3]
  double kgHaTokgm3 = 10000.0 * sc->at(0).vs_LayerThickness;
  sc->at(0).vs_SoilNO3 += amount * fp.vo_NO3 / kgHaTokgm3;
  sc->at(0).vs_SoilNH4 += amount * fp.vo_NH4 / kgHaTokgm3;
  sc->at(0).vs_SoilCarbamid += amount * fp.vo_Carbamid / kgHaTokgm3;
}
/**
 * Method for calculating fertilizer demand from crop demand and soil mineral
 * status (Nmin method).
 *
 * @param fertiliserPartition
 * @param samplingDepth
 * @param cropNTargetValue N availability required by the crop down to rooting
 * depth
 * @param cropNTargetValue30 N availability required by the crop down to 30 cm
 * @param fertiliserMaxApplication Maximal value of N that can be applied until
 * the crop will be damaged
 * @param fertiliserMinApplication Threshold value for economically reasonable
 * fertilizer application
 * @param topDressingDelay Number of days for which the application of surplus
 * fertilizer is delayed
 */
double monica::soilcolumn::applyMineralFertiliserViaNMinMethod(
    SoilColumn *sc, MineralFertilizerParameters fertiliserPartition,
    double samplingDepth, double cropNTargetValue, double cropNTargetValue30,
    double fertiliserMaxApplication, double fertiliserMinApplication,
    int topDressingDelay) {
  if (sc->at(0).vs_SoilMoisture_m3 > sc->at(0).sps.vs_FieldCapacity) {
    sc->_delayedNMinApplications.push_back(
        {fertiliserPartition, samplingDepth, cropNTargetValue,
         cropNTargetValue30, fertiliserMaxApplication, fertiliserMinApplication,
         topDressingDelay});

    debug() << "Soil too wet for fertilisation. Fertiliser event adjourned to "
               "next day."
            << endl;
    return 0.0;
  }

  auto vf_Layer30cm = getLayerNumberForDepth(sc, 0.3);
  auto layerSamplingDepth = getLayerNumberForDepth(sc, samplingDepth);

  double vf_SoilNO3Sum = 0.0;
  double vf_SoilNH4Sum = 0.0;
  for (int i_Layer = 0;
       i_Layer < layerSamplingDepth /*(ceil(vf_SamplingDepth /
                                       at(i_Layer).vs_LayerThickness))*/
       ;
       i_Layer++) {
    // vf_TargetLayer is in cm. We want number of layers
    vf_SoilNO3Sum += sc->at(i_Layer).vs_SoilNO3; //! [kg N m-3]
    vf_SoilNH4Sum += sc->at(i_Layer).vs_SoilNH4; //! [kg N m-3]
  }

  double vf_SoilNO3Sum30 = 0.0;
  double vf_SoilNH4Sum30 = 0.0;
  // Same calculation for a depth of 30 cm
  /** @todo Must be adapted when using variable layer depth. */
  for (int i_Layer = 0; i_Layer < vf_Layer30cm; i_Layer++) {
    vf_SoilNO3Sum30 += sc->at(i_Layer).vs_SoilNO3; //! [kg N m-3]
    vf_SoilNH4Sum30 += sc->at(i_Layer).vs_SoilNH4; //! [kg N m-3]
  }

  // Converts [kg N ha-1] to [kg N m-3]
  double vf_CropNTargetValue =
      cropNTargetValue / 10000.0 / sc->at(0).vs_LayerThickness;
  double vf_CropNTargetValue30 =
      cropNTargetValue30 / 10000.0 / sc->at(0).vs_LayerThickness;

  double vf_FertiliserDemandVol =
      vf_CropNTargetValue - (vf_SoilNO3Sum + vf_SoilNH4Sum);
  double vf_FertiliserDemandVol30 =
      vf_CropNTargetValue30 - (vf_SoilNO3Sum30 + vf_SoilNH4Sum30);

  // Converts fertiliser demand back from [kg N m-3] to [kg N ha-1]
  double vf_FertiliserDemand =
      vf_FertiliserDemandVol * 10000.0 * sc->at(0).vs_LayerThickness;
  double vf_FertiliserDemand30 =
      vf_FertiliserDemandVol30 * 10000.0 * sc->at(0).vs_LayerThickness;

  double vf_FertiliserRecommendation =
      max(vf_FertiliserDemand, vf_FertiliserDemand30);

  if (vf_FertiliserRecommendation < fertiliserMaxApplication) {
    // If the N demand of the crop is smaller than the user defined
    // minimum fertilisation then no need to fertilise
    vf_FertiliserRecommendation = 0.0;
  }

  if (vf_FertiliserRecommendation > fertiliserMinApplication) {
    // If the N demand of the crop is greater than the user defined
    // maximum fertilisation then need to split so surplus fertilizer can
    // be applied after a delay time
    sc->vf_TopDressing = vf_FertiliserRecommendation - fertiliserMinApplication;
    sc->vf_TopDressingPartition = fertiliserPartition;
    sc->vf_TopDressingDelay = topDressingDelay;
    vf_FertiliserRecommendation = fertiliserMinApplication;
  }

  // Apply fertiliser
  applyMineralFertiliser(sc, fertiliserPartition, vf_FertiliserRecommendation);

  debug() << "SoilColumn::applyMineralFertiliserViaNMinMethod:\t"
          << vf_FertiliserRecommendation << endl;

  // apply the callback to all of the fertiliser, even though some if it
  //(the top-dressing) will only be applied later
  // we simply assume it really will be applied, in the worst case
  // the delay is so long, that the crop is already harvested until
  // the top-dressing will be applied
  return vf_FertiliserRecommendation;
}
/**
 * Method for calculating irrigation demand from soil moisture status.
 * The trigger will be activated and deactivated according to crop parameters
 * (temperature sum)
 *
 * @param automatic irrigation parameters
 * @return could irrigation be applied and how much has been applied
 */
std::pair<bool, double> monica::soilcolumn::applyIrrigationViaTrigger(
    SoilColumn *sc, const AutomaticIrrigationParameters &aips) {
  if (!sc->cropModule)
    return std::make_pair(false, 0);

  double s =
      sc->cropModule->cropParams.cultivarParams.pc_HeatSumIrrigationStart;
  double e = sc->cropModule->cropParams.cultivarParams.pc_HeatSumIrrigationEnd;
  double cts = sc->cropModule->vc_CurrentTotalTemperatureSum;
  if (cts < s || cts > e || aips.threshold < 0.0)
    return std::make_pair(false, 0);

  double actPAW = 0.0; // actualPlantAvailableWater
  double maxPAW = 0.0; // maxPlantAvailableWater
  double layerDepthM = 0;
  for (int i = 0; i < sc->size() && layerDepthM < aips.criticalMoistureDepthM;
       i++) {
    const auto &li = sc->at(i);
    auto smi = li.vs_SoilMoisture_m3;
    auto fci = li.sps.vs_FieldCapacity;
    auto pwpi = li.sps.vs_PermanentWiltingPoint;
    auto lti = li.vs_LayerThickness;

    actPAW += (smi - pwpi) * lti * 1000.0; // [mm]
    maxPAW += (fci - pwpi) * lti * 1000.0; // [mm]

    layerDepthM += lti;
  }
  if (Tools::flt_equal_zero(maxPAW))
    return std::make_pair(false, 0);
  const double fractPAW = actPAW / maxPAW;
  if (fractPAW <= aips.threshold) {
    double addedIrrigationWater = 0;
    if (aips.amount > 0.0) {
      applyIrrigation(sc, aips.amount, aips.nitrateConcentration);
      addedIrrigationWater = aips.amount;
    } else if (aips.percentNFC > 0.0) {
      layerDepthM = 0;
      for (int i = 0;
           i < sc->size() && layerDepthM < aips.criticalMoistureDepthM; i++) {
        auto &li = sc->at(i);
        auto smi = li.vs_SoilMoisture_m3;
        auto fci = li.sps.vs_FieldCapacity;
        auto pwpi = li.sps.vs_PermanentWiltingPoint;
        auto lti = li.vs_LayerThickness;

        double percentNFCi = (fci - pwpi) * aips.percentNFC / 100.0;
        double pawi = smi - pwpi;
        double addedIrrigationWaterAtLayer = std::max(0.0, percentNFCi - pawi);
        addedIrrigationWater += addedIrrigationWaterAtLayer;
        li.vs_SoilMoisture_m3 = percentNFCi + pwpi;
        double nitrateAddedViaIrrigation =    // -> //[kg m-3]
            aips.nitrateConcentration *       // [mg dm-3]
            addedIrrigationWaterAtLayer /     //[dm3 m-2]
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
void monica::soilcolumn::applyIrrigation(SoilColumn *sc, double amount,
                                         double nitrateConcentration) {
  // Adding irrigation water amount to surface water storage
  sc->vs_SurfaceWaterStorage += amount;        // [mm]
  double nitrateAddedViaIrrigation =           // -> //[kg m-3]
      nitrateConcentration *                   // [mg dm-3]
      amount /                                 //[dm3 m-2]
      sc->at(0).vs_LayerThickness / 1000000.0; // [m]

  // adding N from irrigation water to top soil nitrate pool
  sc->at(0).vs_SoilNO3 += nitrateAddedViaIrrigation;
}
/**
 * Applies tillage to effected layers. Parameters for effected soil layers
 * are averaged.
 * @param depth Depth of affected soil.
 */
void monica::soilcolumn::applyTillage(SoilColumn *sc, double depth) {
  auto layer_index = getLayerNumberForDepth(sc, depth) + 1;

  double soil_organic_carbon = 0.0;
  double soil_organic_matter = 0.0;
  double soil_temperature = 0.0;
  double soil_moisture = 0.0;
  // double soil_moistureOld = 0.0;
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
    soil_organic_carbon += sc->at(i).sps.vs_SoilOrganicCarbon();
    // soil_organic_matter += at(i).vs_SoilOrganicMatter();
    soil_temperature += sc->at(i).vs_SoilTemperature;
    soil_moisture += sc->at(i).vs_SoilMoisture_m3;
    // soil_moistureOld += at(i).vs_SoilMoistureOld_m3;
    som_slow += sc->at(i).vs_SOM_Slow;
    som_fast += sc->at(i).vs_SOM_Fast;
    smb_slow += sc->at(i).vs_SMB_Slow;
    smb_fast += sc->at(i).vs_SMB_Fast;
    carbamid += sc->at(i).vs_SoilCarbamid;
    nh4 += sc->at(i).vs_SoilNH4;
    no2 += sc->at(i).vs_SoilNO2;
    no3 += sc->at(i).vs_SoilNO3;
  }

  auto li = double(layer_index);

  // calculate mean value of accumulated soil parameters
  soil_organic_carbon /= li;
  // soil_organic_matter /= li;
  soil_temperature /= li;
  soil_moisture /= li;
  // soil_moistureOld /= li;
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
    // assert((soil_organic_carbon - (soil_organic_matter *
    // OrganicConstants::po_SOM_to_C)) < 0.00001);
    sc->at(i).sps.set_vs_SoilOrganicCarbon(soil_organic_carbon);
    // at(i).set_SoilOrganicMatter(soil_organic_matter);
    sc->at(i).vs_SoilTemperature = soil_temperature;
    sc->at(i).vs_SoilMoisture_m3 = soil_moisture;
    // at(i).vs_SoilMoistureOld_m3 = soil_moistureOld;
    sc->at(i).vs_SOM_Slow = som_slow;
    sc->at(i).vs_SOM_Fast = som_fast;
    sc->at(i).vs_SMB_Slow = smb_slow;
    sc->at(i).vs_SMB_Fast = smb_fast;
    sc->at(i).vs_SoilCarbamid = carbamid;
    sc->at(i).vs_SoilNH4 = nh4;
    sc->at(i).vs_SoilNO2 = no2;
    sc->at(i).vs_SoilNO3 = no3;
  }

  // merge aom pool
  auto aom_pool_count = sc->at(0).vo_AOM_Pool.size();

  if (aom_pool_count > 0) {
    vector<double> aom_slow(aom_pool_count);
    vector<double> aom_fast(aom_pool_count);

    // initialization of aom pool accumulator
    for (unsigned int pool_index = 0; pool_index < aom_pool_count;
         pool_index++) {
      aom_slow[pool_index] = 0.0;
      aom_fast[pool_index] = 0.0;
    }

    layer_index = min(layer_index, size_t(sc->vs_NumberOfOrganicLayers));

    // cout << "Soil parameters before applying tillage for the first "<<
    // layer_index+1 << " layers: " << endl;

    // add up pools for affected layer with same index
    for (size_t j = 0; j < layer_index; j++) {
      // cout << "Layer " << j << endl << endl;

      SoilLayer &layer = sc->at(j);
      size_t pool_index = 0;
      for (auto aomp : layer.vo_AOM_Pool) {
        aom_slow[pool_index] += aomp.vo_AOM_Slow;
        aom_fast[pool_index] += aomp.vo_AOM_Fast;

        // cout << "AOMPool " << pool_index << endl;
        // cout << "vo_AOM_Slow:\t"<< aomp.vo_AOM_Slow << endl;
        // cout << "vo_AOM_Fast:\t"<< aomp.vo_AOM_Fast << endl;

        pool_index++;
      }
    }

    //
    for (size_t pool_index = 0; pool_index < aom_pool_count; pool_index++) {
      aom_slow[pool_index] = aom_slow[pool_index] / li;
      aom_fast[pool_index] = aom_fast[pool_index] / li;
    }

    // cout << "Soil parameters after applying tillage for the first "<<
    // layer_index+1 << " layers: " << endl;

    // rewrite parameters of aom pool with mean values
    for (size_t j = 0; j < layer_index; j++) {
      SoilLayer &layer = sc->at(j);
      // cout << "Layer " << j << endl << endl;
      size_t pool_index = 0;
      for (auto aomp : layer.vo_AOM_Pool) {
        aomp.vo_AOM_Slow = aom_slow[pool_index];
        aomp.vo_AOM_Fast = aom_fast[pool_index];

        // cout << "AOMPool " << pool_index << endl;
        // cout << "vo_AOM_Slow:\t"<< aomp.vo_AOM_Slow << endl;
        // cout << "vo_AOM_Fast:\t"<< aomp.vo_AOM_Fast << endl;

        pool_index++;
      }
    }
  }

  // cout << "soil_organic_carbon: " << soil_organic_carbon << endl;
  // cout << "soil_organic_matter: " << soil_organic_matter << endl;
  // cout << "soil_temperature: " << soil_temperature << endl;
  // cout << "soil_moisture: " << soil_moisture << endl;
  // cout << "soil_moistureOld: " << soil_moistureOld << endl;
  // cout << "som_slow: " << som_slow << endl;
  // cout << "som_fast: " << som_fast << endl;
  // cout << "smb_slow: " << smb_slow << endl;
  // cout << "smb_fast: " << smb_fast << endl;
  // cout << "carbamid: " << carbamid << endl;
  // cout << "nh4: " << nh4 << endl;
  // cout << "no3: " << no3 << endl << endl;
}
