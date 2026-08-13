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

#pragma once

/**
 * @file soilcolumn.h
 *
 * @brief This file contains the declaration of classes AOM_Properties,
 * SoilLayer, SoilColumn and FertilizerTriggerThunk.
 *
 * @see Monica::AOM_Properties
 * @see Monica::SoilLayer
 * @see Monica::SoilColumn
 */

#include <assert.h>
#include <kj/memory.h>
#include <list>
#include <vector>

#include "model/monica/monica_state.capnp.h"

#include "monica-parameters.h"

namespace monica {
struct CropModule;

/**
 * @author Claas Nendel, Michael Berg
 *
 * @brief Storage class for the transformation of organic substance.
 *
 * Class stores data and parameters used in the AOM (Added Organic Matter)
 * circle, that is displayed in the picture below. The AOM-Circle is a
 * description for the transformation of organic substance.
 *
 * <img src="../images/aom-diagramm.png" width="600" height="420">
 */
struct AOM_Properties {
  double vo_AOM_Slow{0.0}; // C content in slowly decomposing added organic
                           // matter pool [kgC m-3]
  double vo_AOM_Fast{0.0}; // C content in rapidly decomposing added organic
                           // matter pool [kgC m-3]

  double vo_AOM_SlowDecRate_to_SMB_Slow{
      0.0}; // Rate for slow AOM consumed by SMB Slow is calculated.
  double vo_AOM_SlowDecRate_to_SMB_Fast{
      0.0}; // Rate for slow AOM consumed by SMB Fast is calculated.
  double vo_AOM_FastDecRate_to_SMB_Slow{
      0.0}; // Rate for fast AOM consumed by SMB Slow is calculated.
  double vo_AOM_FastDecRate_to_SMB_Fast{
      0.0}; // Rate for fast AOM consumed by SMB Fast is calculated.

  double vo_AOM_SlowDecCoeff{0.0}; // Is dependent on environment
  double vo_AOM_FastDecCoeff{0.0}; // Is dependent on environment

  double vo_AOM_SlowDecCoeffStandard{
      1.0}; // Decomposition rate coefficient for slow AOM pool at standard
            // conditions
  double vo_AOM_FastDecCoeffStandard{
      1.0}; // Decomposition rate coefficient for fast AOM pool at standard
            // conditions

  double vo_PartAOM_Slow_to_SMB_Slow{
      0.0}; // Partial transformation from AOM to SMB (soil microbiological
            // biomass) for slow AOMs.
  double vo_PartAOM_Slow_to_SMB_Fast{
      0.0}; // Partial transformation from AOM to SMB (soil microbiological
            // biomass) for fast AOMs.

  double vo_CN_Ratio_AOM_Slow{
      1.0}; // Used for calculation N-value if only C-value is known. Usually
            // a constant value.
  double vo_CN_Ratio_AOM_Fast{1.0}; // C-N-Ratio is dependent on the
                                    // nutritional condition of the plant.

  int vo_DaysAfterApplication{0};      // Fertilization parameter
  double vo_AOM_DryMatterContent{0.0}; // Fertilization parameter
  double vo_AOM_NH4Content{0.0};       // Fertilization parameter

  double vo_AOM_SlowDelta{0.0}; // Difference of AOM slow between to timesteps
  double vo_AOM_FastDelta{0.0}; // Difference of AOM fast between to timesteps

  bool incorporation{false};   // True if organic fertilizer is added with a
                               // subsequent incorporation.
  bool noVolatilization{true}; // true means it's a crop residue and won't
                               // participate in vo_volatilisation()
};

namespace aomproperties {

void deserialize(AOM_Properties *aomp,
                 mas::schema::model::monica::AOMProperties::Reader reader);
void serialize(const AOM_Properties *aomp,
               mas::schema::model::monica::AOMProperties::Builder builder);

} // namespace aomproperties

/**
 * @author Claas Nendel, Michael Berg
 *
 * @brief stores information and properties about a soil layer.
 *
 * Storage class for soil layer properties e.g. saturation, field capacity, etc.
 * Right now all layers are expected to be from the same size, but this code
 * allows different sizes for a layer, too.
 *
 */
struct SoilLayer {
  double vs_LayerThickness{0.1}; // Soil layer's vertical extension [m]
  // double vs_SoilMoistureOld_m3{0.25}; // Soil layer's moisture content of
  // previous day [m3 m-3]
  double vs_SoilWaterFlux{
      0.0}; // Water flux at the upper boundary of the soil layer [l m-2]

  std::vector<AOM_Properties> vo_AOM_Pool; // List of different added organic
                                           // matter pools in soil layer

  double vs_SOM_Slow{
      0.0}; // C content of soil organic matter slow pool [kg C m-3]
  double vs_SOM_Fast{
      0.0}; // C content of soil organic matter fast pool size [kg C m-3]
  double vs_SMB_Slow{
      0.0}; // C content of soil microbial biomass slow pool size [kg C m-3]
  double vs_SMB_Fast{
      0.0}; // C content of soil microbial biomass fast pool size [kg C m-3]

  // anorganische Stickstoff-Formen
  double vs_SoilCarbamid{
      0.0}; // Soil layer's carbamide-N content [kg Carbamide-N m-3]
  double vs_SoilNH4{0.0001}; // Soil layer's NH4-N content [kg NH4-N m-3]
  double vs_SoilNO2{0.001};  // Soil layer's NO2-N content [kg NO2-N m-3]
  double vs_SoilNO3{0.0001}; // Soil layer's NO3-N content [kg NO3-N m-3]
  bool vs_SoilFrozen{false};

  // formerly composed via `Soil::SoilParameters sps;` - flattened directly.
  double vs_SoilSandContent{
      -1.0}; //!< Soil layer's sand content [kg kg-1] //{0.4}
  double vs_SoilClayContent{
      -1.0};             //!< Soil layer's clay content [kg kg-1] (Ton) //{0.05}
  double vs_SoilpH{6.9}; //!< Soil pH value [] //{7.0}
  double vs_SoilStoneContent{
      0.0};               //!< Soil layer's stone content in soil [m3 m-3]
  double vs_Lambda{-1.0}; //!< Soil water conductivity coefficient [] //{0.5}
  double vs_FieldCapacity{-1.0};         //{0.21} //!< [m3 m-3]
  double vs_Saturation{-1.0};            //{0.43} //!< [m3 m-3]
  double vs_PermanentWiltingPoint{-1.0}; //{0.08} //!< [m3 m-3]
  std::string vs_SoilTexture;
  double vs_SoilAmmonium{0.0005}; //!< soil ammonium content [kg NH4-N m-3]
  double vs_SoilNitrate{0.005};   //!< soil nitrate content [kg NO3-N m-3]
  double vs_Soil_CN_Ratio{10.0};
  double vs_SoilMoisturePercentFC{100.0};
  // Raw/override values; -1 means "unset" and the resolved value has to be
  // computed via the corresponding soillayer::soilXyz() free function.
  double _vs_SoilRawDensity{-1.0};    //!< [kg m-3]
  double _vs_SoilBulkDensity{-1.0};   //!< [kg m-3]
  double _vs_SoilOrganicCarbon{-1.0}; //!< [kg kg-1]
  double _vs_SoilOrganicMatter{-1.0}; //!< [kg kg-1]

  double vs_SoilMoisture_m3{0.25}; // Soil layer's moisture content [m3 m-3]
  double vs_SoilTemperature{0.0};  // Soil layer's temperature [°C]
};

SoilLayer makeSoilLayer(double vs_LayerThickness,
                        const Soil::SoilParameters &soilParams);

namespace soillayer {

void deserialize(SoilLayer *sl,
                 mas::schema::model::monica::SoilLayerState::Reader reader);
void serialize(const SoilLayer *sl,
               mas::schema::model::monica::SoilLayerState::Builder builder);

//! Returns soil water pressure head as common logarithm pF.
double soilMoisturePF(const SoilLayer *sl);

//! soil mineral N content [kg m-3]
double soilNmin(const SoilLayer *sl);

//! Soil layer's silt content [kg kg-1] (Schluff)
double soilSiltContent(const SoilLayer *sl);

//! Resolved soil raw density (falls back to bulk density + clay content if unset)
double soilRawDensity(const SoilLayer *sl);

//! Resolved soil bulk density (falls back to raw density + clay content if unset)
double soilBulkDensity(const SoilLayer *sl);

//! Resolved soil organic carbon [kg C kg-1] (falls back to organic matter if unset)
double soilOrganicCarbon(const SoilLayer *sl);

//! Resolved soil organic matter [kg OM kg-1] (falls back to organic carbon if unset)
double soilOrganicMatter(const SoilLayer *sl);

} // namespace soillayer

/**
 * @author Claas Nendel, Michael Berg
 *
 * @brief Description of a soil column that consists of a list of soil layers.

  * All layers are stored in a list and can be access by different kind of
 operators.
  * This code is based on a Fortran-Program so some operators are overloaded
  * to make the access of an array similar to the Fortran's way.
  *
  * @see Monica::SoilLayer
  *
  */
struct SoilColumn {
  std::vector<SoilLayer> layers;

  double vs_SurfaceWaterStorage{
      0.0}; // Content of above-ground water storage [mm]
  double vs_InterceptionStorage{
      0.0}; // Amount of intercepted water on crop surface [mm]
  size_t vm_GroundwaterTableLayer{0}; // Layer of current groundwater table
  double vs_FluxAtLowerBoundary{0.0}; // Water flux out of bottom layer
  double vq_CropNUptake{0.0}; // Daily amount of N taken up by the crop [kg m-2]
  double vt_SoilSurfaceTemperature{0.0};
  double vm_SnowDepth{0.0};

  double ps_MaxMineralisationDepth{0.4};

  int vs_NumberOfOrganicLayers{0}; //!< Number of organic layers.
  double vf_TopDressing{0.0};
  MineralFertilizerParameters vf_TopDressingPartition;
  int vf_TopDressingDelay{0};

  CropModule *cropModule{nullptr};

  struct DelayedNMinApplicationParams {
    MineralFertilizerParameters fp;
    double vf_SamplingDepth;
    double vf_CropNTarget;
    double vf_CropNTarget30;
    double vf_FertiliserMinApplication;
    double vf_FertiliserMaxApplication;
    int vf_TopDressingDelay;
  };

  std::list<DelayedNMinApplicationParams> _delayedNMinApplications;

  // double pm_CriticalMoistureDepth{0};
};

kj::Own<SoilColumn> makeSoilColumn(double layerThickness,
                                   double maxMineralisationDepth,
                                   const Soil::SoilPMs &soilParams);
kj::Own<SoilColumn>
makeSoilColumn(mas::schema::model::monica::SoilColumnState::Reader reader,
               CropModule *cropModule = nullptr);

namespace soilcolumn {

void deserialize(SoilColumn *sc,
                 mas::schema::model::monica::SoilColumnState::Reader reader);
void serialize(const SoilColumn *sc,
               mas::schema::model::monica::SoilColumnState::Builder builder);
void deserializeDelayedNMinApplicationParams(
    SoilColumn::DelayedNMinApplicationParams *dnmap,
    mas::schema::model::monica::SoilColumnState::DelayedNMinApplicationParams::
        Reader reader);
void serializeDelayedNMinApplicationParams(
    const SoilColumn::DelayedNMinApplicationParams *dnmap,
    mas::schema::model::monica::SoilColumnState::DelayedNMinApplicationParams::
        Builder builder);
void putCrop(SoilColumn *sc, CropModule *cm);
void removeCrop(SoilColumn *sc);
void clearTopDressingParams(SoilColumn *sc);
void deleteAOMPool(SoilColumn *sc);
double applyPossibleDelayedFerilizer(SoilColumn *sc);
double applyPossibleTopDressing(SoilColumn *sc);
void applyMineralFertiliser(SoilColumn *sc, MineralFertilizerParameters fp,
                            double amount);
double applyMineralFertiliserViaNDemand(SoilColumn *sc,
                                        MineralFertilizerParameters fp,
                                        double demandDepth, double Ndemand);
//! Calculates number of organic layers, usually the number of layers in the
//! first 30 cm depth of soil.
int calculateNumberOfOrganicLayers(const SoilColumn *sc);
inline size_t numberOfLayers(const SoilColumn *sc) { return sc->layers.size(); }
inline size_t numberOfOrganicLayers(const SoilColumn *sc) {
  return sc->vs_NumberOfOrganicLayers;
}
//! Returns the thickness of a layer.
//! Right now by definition all layers have the same size,
//! therefor only the thickness of first layer is returned.
inline double layerThickness(const SoilColumn *sc) {
  return sc->layers.at(0).vs_LayerThickness;
}
//! Returns daily crop N uptake [kg N ha-1 d-1]
inline double dailyCropNUptake(const SoilColumn *sc) {
  return sc->vq_CropNUptake * 10000.0;
}
//! Returns index of layer that lays in the given depth.
size_t getLayerNumberForDepth(const SoilColumn *sc, double depth);
//! Returns sum of soiltemperature for several soil layers.
double sumSoilTemperature(const SoilColumn *sc, int layers);
double applyMineralFertiliserViaNMinMethod(
    SoilColumn *sc, MineralFertilizerParameters fertiliserPartition,
    double samplingDepth, double cropNTargetValue, double cropNTargetValue30,
    double fertiliserMaxApplication, double fertiliserMinApplication,
    int topDressingDelay);
std::pair<bool, double>
applyIrrigationViaTrigger(SoilColumn *sc,
                          const AutomaticIrrigationParameters &aips);
void applyIrrigation(SoilColumn *sc, double amount,
                     double nitrateConcentration);
void applyTillage(SoilColumn *sc, double depth);

} // namespace soilcolumn

} // namespace monica
