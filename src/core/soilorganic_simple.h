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

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <map>
#include <string>
#include <iostream>
#include <iomanip>
#include <vector>
#include <utility>
#include <list>
#include <kj/memory.h>

#include "model/monica/monica_state.capnp.h"
#include "monica-parameters.h"

namespace monica
{
struct SoilColumn;
struct CropModule;

struct SoilOrganic
{
public:
  //void fo_OM_Input(bool vo_AOM_Addition);
  
  // MONICA dentrification code

  typedef std::pair<double, double> NitDenitN2O;

  double fo_MoistOnDenitrification(double d_SoilMoisture_m3, double d_Saturation);
  double fo_NH3onNitriteOxidation (double d_SoilNH4, double d_SoilpH);
  //void fo_distributeDeadRootBiomass();

  SoilColumn& soilColumn;
  SoilOrganicModuleParameters _params;

  std::size_t vs_NumberOfLayers{0};
  std::size_t vs_NumberOfOrganicLayers{0};
  bool addedOrganicMatter{false};
  double irrigationAmount{0.0};
  std::vector<double> vo_ActAmmoniaOxidationRate; //!< [kg N m-3 d-1]
  std::vector<double> vo_ActNitrificationRate; //!< [kg N m-3 d-1]
  std::vector<double> vo_ActDenitrificationRate; //!< [kg N m-3 d-1]
  std::vector<double> vo_AOM_FastDeltaSum;
  std::vector<double> vo_AOM_FastInput; //!< AOMfast pool change by direct input [kg C m-3]
  std::vector<double> vo_AOM_FastSum;
  std::vector<double> vo_AOM_SlowDeltaSum;
  std::vector<double> vo_AOM_SlowInput; //!< AOMslow pool change by direct input [kg C m-3]
  std::vector<double> vo_AOM_SlowSum;
  std::vector<double> vo_CBalance;
  double vo_DecomposerRespiration{0.0};
  std::string vo_ErrorMessage;
  std::vector<double> vo_InertSoilOrganicC;
  std::vector<double> vo_InertSoilOrganicC_highCN;
  double vo_N2O_Produced{0.0}; // [kg-N2O-N/ha]
  double vo_N2O_Produced_Nit{ 0.0 }; // [kg-N2O-N/ha]
  double vo_N2O_Produced_Denit{ 0.0 }; // [kg-N2O-N/ha]
  double vo_NetEcosystemExchange{0.0};
  double vo_NetEcosystemProduction{0.0};
  double vo_NetNMineralisation{0.0};
  std::vector<double> vo_NetNMineralisationRate;
  double vo_Total_NH3_Volatilised{0.0};
  double vo_NH3_Volatilised{0.0};
  std::vector<double> vo_SMB_CO2EvolutionRate;
  std::vector<double> vo_SMB_FastDelta;
  std::vector<double> vo_SMB_SlowDelta;
  std::vector<double> vs_SoilMineralNContent;
  std::vector<double> vo_SoilOrganicC;
  std::vector<double> vo_SoilOrganicC_highCN;
  std::vector<double> vo_SOM_FastDelta;
  std::vector<double> vo_SOM_FastInput; //!< SOMfast pool change by direct input [kg C m-3]
  std::vector<double> vo_SOM_SlowDelta;
  double vo_SumDenitrification{0.0}; // kg-N/m2
  double vo_SumNetNMineralisation{0.0};
  double vo_SumN2O_Produced{0.0};
  double vo_SumNH3_Volatilised{0.0};
  double vo_TotalDenitrification{0.0};

  //! True, if organic fertilizer has been added with a following incorporation.
  //! Parameter is automatically set to false, if carbamid amount is falling below 0.001.
  bool incorporation{false};
  CropModule* cropModule{nullptr};
};

kj::Own<SoilOrganic> makeSoilOrganic(SoilColumn& soilColumn, SoilOrganicModuleParameters params);
kj::Own<SoilOrganic> makeSoilOrganic(SoilColumn& soilColumn,
                                     mas::schema::model::monica::SoilOrganicModuleState::Reader reader,
                                     CropModule* cropModule = nullptr);
void soilOrganicDeserialize(SoilOrganic* so, mas::schema::model::monica::SoilOrganicModuleState::Reader reader);
void soilOrganicSerialize(const SoilOrganic* so, mas::schema::model::monica::SoilOrganicModuleState::Builder builder);
void soilOrganicInitializeFromParams(SoilOrganic* so);
void soilOrganicFoUrea(SoilOrganic* so);
void soilOrganicFoMIT(SoilOrganic* so);
void soilOrganicFoVolatilisation(SoilOrganic* so, bool aomAddition, double meanAirTemperature, double windSpeed);
void soilOrganicFoNitrification(SoilOrganic* so);
void soilOrganicFoSticsNitrification(SoilOrganic* so);
void soilOrganicFoDenitrification(SoilOrganic* so);
void soilOrganicFoSticsDenitrification(SoilOrganic* so);
double soilOrganicFoN2OProduction(SoilOrganic* so);
SoilOrganic::NitDenitN2O soilOrganicFoSticsN2OProduction(SoilOrganic* so);
void soilOrganicFoPoolUpdate(SoilOrganic* so);
double soilOrganicFoNetEcosystemProduction(SoilOrganic* so, double netPrimaryProduction, double decomposerRespiration);
double soilOrganicFoNetEcosystemExchange(SoilOrganic* so, double netPrimaryProduction, double decomposerRespiration);
double soilOrganicFoClayOnDecompostionKaiteew(SoilOrganic* so, double soilClayContent, double limitClayEffect);
double soilOrganicFoTempOnDecompostionKaiteew(SoilOrganic* so, double soilTemperature, double qTenFactor, double tempDecOptimal);
double soilOrganicFoMoistOnDecompostionKaiteew(SoilOrganic* so, double soilMoisture_m3, double saturation, double moistureDecOptimal);
double soilOrganicFoClayOnDecompostion(SoilOrganic* so, double soilClayContent, double limitClayEffect);
double soilOrganicFoTempOnDecompostion(SoilOrganic* so, double soilTemperature);
double soilOrganicFoMoistOnDecompostion(SoilOrganic* so, double soilMoisture_pF);
double soilOrganicFoMoistOnHydrolysis(SoilOrganic* so, double soilMoisture_pF);
double soilOrganicFoTempOnNitrification(SoilOrganic* so, double soilTemperature);
double soilOrganicFoMoistOnNitrification(SoilOrganic* so, double soilMoisture_pF);
void soilOrganicStep(SoilOrganic* so, double meanAirTemperature, double precipitation, double windSpeed);
void soilOrganicPutCrop(SoilOrganic* so, CropModule* cm);
void soilOrganicRemoveCrop(SoilOrganic* so);
void soilOrganicSetIncorporation(SoilOrganic* so, bool incorp);
void soilOrganicAddOrganicMatter(SoilOrganic* so, const OrganicMatterParameters& params,
                                 const std::map<size_t, double>& layer2amount,
                                 double nConcentration = 0);
void soilOrganicAddOrganicMatter(SoilOrganic* so, const OrganicMatterParameters& params,
                                 double amount, double nConcentration = 0, size_t intoLayerIndex = 0);
void soilOrganicAddIrrigationWater(SoilOrganic* so, double amount);
double soilOrganicGetSoilOrganicC(const SoilOrganic* so, int iLayer);
double soilOrganicGetAOM_FastSum(const SoilOrganic* so, int iLayer);
double soilOrganicGetAOM_SlowSum(const SoilOrganic* so, int iLayer);
double soilOrganicGetSMB_Fast(const SoilOrganic* so, int iLayer);
double soilOrganicGetSMB_Slow(const SoilOrganic* so, int iLayer);
double soilOrganicGetSOM_Fast(const SoilOrganic* so, int iLayer);
double soilOrganicGetSOM_Slow(const SoilOrganic* so, int iLayer);
double soilOrganicGetCBalance(const SoilOrganic* so, int iLayer);
double soilOrganicGetSMB_CO2EvolutionRate(const SoilOrganic* so, int iLayer);
double soilOrganicGetActDenitrificationRate(const SoilOrganic* so, int iLayer);
double soilOrganicGetNetNMineralisationRate(const SoilOrganic* so, int iLayer);
double soilOrganicGetNH3_Volatilised(const SoilOrganic* so);
double soilOrganicGetSumNH3_Volatilised(const SoilOrganic* so);
double soilOrganicGetN2O_Produced(const SoilOrganic* so);
double soilOrganicGetN2O_ProducedNit(const SoilOrganic* so);
double soilOrganicGetN2O_ProducedDenit(const SoilOrganic* so);
double soilOrganicGetSumN2O_Produced(const SoilOrganic* so);
double soilOrganicGetNetNMineralisation(const SoilOrganic* so);
double soilOrganicGetSumNetNMineralisation(const SoilOrganic* so);
double soilOrganicGetSumDenitrification(const SoilOrganic* so);
double soilOrganicGetDenitrification(const SoilOrganic* so);
double soilOrganicGetDecomposerRespiration(const SoilOrganic* so);
double soilOrganicGetNetEcosystemProduction(const SoilOrganic* so);
double soilOrganicGetNetEcosystemExchange(const SoilOrganic* so);
double soilOrganicGetOrganicN(const SoilOrganic* so, int iLayer);
double soilOrganicGetActAmmoniaOxidationRate(const SoilOrganic* so, int i);
double soilOrganicGetActNitrificationRate(const SoilOrganic* so, int i);
double soilOrganicGetActDenitrificationRate(const SoilOrganic* so, int i);

} // namespace Monica
