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

#include "monica-parameters.h"

#include <map>
#include <sstream>
#include <iostream>
#include <fstream>
#include <cmath>
#include <utility>
#include <mutex>
#include <string>

//#include "db/abstract-db-connections.h"
#include "climate/climate-common.h"
#include "tools/helper.h"
#include "tools/algorithms.h"
#include "tools/debug.h"
#include "soil/conversion.h"
#include "soil/soil.h"

#include "climate.capnp.h"

//using namespace Db;
using namespace std;
using namespace monica;
using namespace Soil;
using namespace Tools;
using namespace Climate;
using namespace json11;


/**
 * @brief Constructor
 * @param oid organ ID
 * @param yp Yield percentage
 */
YieldComponent::YieldComponent(int oid, double yp, double ydm)
    : organId(oid),
      yieldPercentage(yp),
      yieldDryMatter(ydm) {}

// YieldComponent::YieldComponent(json11::Json j) {
//   merge(j);
// }

void YieldComponent::deserialize(mas::schema::model::monica::YieldComponent::Reader reader) {
  organId = (int) reader.getOrganId();
  yieldPercentage = reader.getYieldPercentage();
  yieldDryMatter = reader.getYieldDryMatter();
}

void YieldComponent::serialize(mas::schema::model::monica::YieldComponent::Builder builder) const {
  builder.setOrganId(organId);
  builder.setYieldPercentage(yieldPercentage);
  builder.setYieldDryMatter(yieldDryMatter);
}

Errors YieldComponent::merge(json11::Json j) {
  set_int_value(organId, j, "organId");
  set_double_value(yieldPercentage, j, "yieldPercentage");
  set_double_value(yieldDryMatter, j, "yieldDryMatter");

  return {};
}

json11::Json YieldComponent::to_json() const {
  return json11::Json::object
      {{"type",            "YieldComponent"},
       {"organId",         organId},
       {"yieldPercentage", yieldPercentage},
       {"yieldDryMatter",  yieldDryMatter}
      };
}



// SpeciesParameters::SpeciesParameters(json11::Json j) {
//   merge(j);
// }

void SpeciesParameters::deserialize(mas::schema::model::monica::SpeciesParameters::Reader reader) {
  pc_SpeciesId = reader.getSpeciesId();
  pc_CarboxylationPathway = reader.getCarboxylationPathway();
  pc_DefaultRadiationUseEfficiency = reader.getDefaultRadiationUseEfficiency();
  pc_PartBiologicalNFixation = reader.getPartBiologicalNFixation();
  pc_InitialKcFactor = reader.getInitialKcFactor();
  pc_LuxuryNCoeff = reader.getLuxuryNCoeff();
  pc_MaxCropDiameter = reader.getMaxCropDiameter();
  pc_StageAtMaxHeight = reader.getStageAtMaxHeight();
  pc_StageAtMaxDiameter = reader.getStageAtMaxDiameter();
  pc_MinimumNConcentration = reader.getMinimumNConcentration();
  pc_MinimumTemperatureForAssimilation = reader.getMinimumTemperatureForAssimilation();
  pc_OptimumTemperatureForAssimilation = reader.getOptimumTemperatureForAssimilation();
  pc_MaximumTemperatureForAssimilation = reader.getMaximumTemperatureForAssimilation();
  pc_NConcentrationAbovegroundBiomass = reader.getNConcentrationAbovegroundBiomass();
  pc_NConcentrationB0 = reader.getNConcentrationB0();
  pc_NConcentrationPN = reader.getNConcentrationPN();
  pc_NConcentrationRoot = reader.getNConcentrationRoot();
  pc_DevelopmentAccelerationByNitrogenStress = reader.getDevelopmentAccelerationByNitrogenStress();
  pc_FieldConditionModifier = reader.getFieldConditionModifier();
  pc_AssimilateReallocation = reader.getAssimilateReallocation();
  setFromCapnpList(pc_BaseTemperature, reader.getBaseTemperature());
  setFromCapnpList(pc_OrganMaintenanceRespiration, reader.getOrganMaintenanceRespiration());
  setFromCapnpList(pc_OrganGrowthRespiration, reader.getOrganGrowthRespiration());
  setFromCapnpList(pc_StageMaxRootNConcentration, reader.getStageMaxRootNConcentration());
  setFromCapnpList(pc_InitialOrganBiomass, reader.getInitialOrganBiomass());
  setFromCapnpList(pc_CriticalOxygenContent, reader.getCriticalOxygenContent());
  setFromCapnpList(pc_StageMobilFromStorageCoeff, reader.getStageMobilFromStorageCoeff());
  setFromCapnpList(pc_AbovegroundOrgan, reader.getAbovegroundOrgan());
  setFromCapnpList(pc_StorageOrgan, reader.getStorageOrgan());
  pc_SamplingDepth = reader.getSamplingDepth();
  pc_TargetNSamplingDepth = reader.getTargetNSamplingDepth();
  pc_TargetN30 = reader.getTargetN30();
  pc_MaxNUptakeParam = reader.getMaxNUptakeParam();
  pc_RootDistributionParam = reader.getRootDistributionParam();
  pc_PlantDensity = reader.getPlantDensity();
  pc_RootGrowthLag = reader.getRootGrowthLag();
  pc_MinimumTemperatureRootGrowth = reader.getMinimumTemperatureRootGrowth();
  pc_InitialRootingDepth = reader.getInitialRootingDepth();
  pc_RootPenetrationRate = reader.getRootPenetrationRate();
  pc_RootFormFactor = reader.getRootFormFactor();
  pc_SpecificRootLength = reader.getSpecificRootLength();
  pc_StageAfterCut = reader.getStageAfterCut();
  pc_LimitingTemperatureHeatStress = reader.getLimitingTemperatureHeatStress();
  pc_CuttingDelayDays = reader.getCuttingDelayDays();
  pc_DroughtImpactOnFertilityFactor = reader.getDroughtImpactOnFertilityFactor();
  EF_MONO = reader.getEfMono();
  EF_MONOS = reader.getEfMonos();
  EF_ISO = reader.getEfIso();
  VCMAX25 = reader.getVcMax25();
  AEKC = reader.getAekc();
  AEKO = reader.getAeko();
  AEVC = reader.getAevc();
  KC25 = reader.getKc25();
  KO25 = reader.getKo25();
  pc_TransitionStageLeafExp = reader.getTransitionStageLeafExp();
}

void SpeciesParameters::serialize(mas::schema::model::monica::SpeciesParameters::Builder builder) const {
  builder.setSpeciesId(pc_SpeciesId);
  builder.setCarboxylationPathway(pc_CarboxylationPathway);
  builder.setDefaultRadiationUseEfficiency(pc_DefaultRadiationUseEfficiency);
  builder.setPartBiologicalNFixation(pc_PartBiologicalNFixation);
  builder.setInitialKcFactor(pc_InitialKcFactor);
  builder.setLuxuryNCoeff(pc_LuxuryNCoeff);
  builder.setMaxCropDiameter(pc_MaxCropDiameter);
  builder.setStageAtMaxHeight(pc_StageAtMaxHeight);
  builder.setStageAtMaxDiameter(pc_StageAtMaxDiameter);
  builder.setMinimumNConcentration(pc_MinimumNConcentration);
  builder.setMinimumTemperatureForAssimilation(pc_MinimumTemperatureForAssimilation);
  builder.setOptimumTemperatureForAssimilation(pc_OptimumTemperatureForAssimilation);
  builder.setMaximumTemperatureForAssimilation(pc_MaximumTemperatureForAssimilation);
  builder.setNConcentrationAbovegroundBiomass(pc_NConcentrationAbovegroundBiomass);
  builder.setNConcentrationB0(pc_NConcentrationB0);
  builder.setNConcentrationPN(pc_NConcentrationPN);
  builder.setNConcentrationRoot(pc_NConcentrationRoot);
  builder.setDevelopmentAccelerationByNitrogenStress(pc_DevelopmentAccelerationByNitrogenStress);
  builder.setFieldConditionModifier(pc_FieldConditionModifier);
  builder.setAssimilateReallocation(pc_AssimilateReallocation);
  setCapnpList(pc_BaseTemperature, builder.initBaseTemperature((capnp::uint) pc_BaseTemperature.size()));
  setCapnpList(pc_OrganMaintenanceRespiration,
               builder.initOrganMaintenanceRespiration((capnp::uint) pc_OrganMaintenanceRespiration.size()));
  setCapnpList(pc_OrganGrowthRespiration,
               builder.initOrganGrowthRespiration((capnp::uint) pc_OrganGrowthRespiration.size()));
  setCapnpList(pc_StageMaxRootNConcentration,
               builder.initStageMaxRootNConcentration((capnp::uint) pc_StageMaxRootNConcentration.size()));
  setCapnpList(pc_InitialOrganBiomass, builder.initInitialOrganBiomass((capnp::uint) pc_InitialOrganBiomass.size()));
  setCapnpList(pc_CriticalOxygenContent,
               builder.initCriticalOxygenContent((capnp::uint) pc_CriticalOxygenContent.size()));
  setCapnpList(pc_StageMobilFromStorageCoeff,
               builder.initStageMobilFromStorageCoeff((capnp::uint) pc_StageMobilFromStorageCoeff.size()));
  setCapnpList(pc_AbovegroundOrgan, builder.initAbovegroundOrgan((capnp::uint) pc_AbovegroundOrgan.size()));
  setCapnpList(pc_StorageOrgan, builder.initStorageOrgan((capnp::uint) pc_StorageOrgan.size()));
  builder.setSamplingDepth(pc_SamplingDepth);
  builder.setTargetNSamplingDepth(pc_TargetNSamplingDepth);
  builder.setTargetN30(pc_TargetN30);
  builder.setMaxNUptakeParam(pc_MaxNUptakeParam);
  builder.setRootDistributionParam(pc_RootDistributionParam);
  builder.setPlantDensity(pc_PlantDensity);
  builder.setRootGrowthLag(pc_RootGrowthLag);
  builder.setMinimumTemperatureRootGrowth(pc_MinimumTemperatureRootGrowth);
  builder.setInitialRootingDepth(pc_InitialRootingDepth);
  builder.setRootPenetrationRate(pc_RootPenetrationRate);
  builder.setRootFormFactor(pc_RootFormFactor);
  builder.setSpecificRootLength(pc_SpecificRootLength);
  builder.setStageAfterCut(pc_StageAfterCut);
  builder.setLimitingTemperatureHeatStress(pc_LimitingTemperatureHeatStress);
  builder.setCuttingDelayDays(pc_CuttingDelayDays);
  builder.setDroughtImpactOnFertilityFactor(pc_DroughtImpactOnFertilityFactor);
  builder.setEfMono(EF_MONO);
  builder.setEfMonos(EF_MONOS);
  builder.setEfIso(EF_ISO);
  builder.setVcMax25(VCMAX25);
  builder.setAekc(AEKC);
  builder.setAeko(AEKO);
  builder.setAevc(AEVC);
  builder.setKc25(KC25);
  builder.setKo25(KO25);
  builder.setTransitionStageLeafExp(pc_TransitionStageLeafExp);
}

Errors SpeciesParameters::merge(json11::Json j) {
  Errors res = Json11Serializable::merge(j);

  set_string_value(pc_SpeciesId, j, "SpeciesName");
  set_int_value(pc_CarboxylationPathway, j, "CarboxylationPathway");
  set_double_value(pc_DefaultRadiationUseEfficiency, j, "DefaultRadiationUseEfficiency");
  set_double_value(pc_PartBiologicalNFixation, j, "PartBiologicalNFixation");
  set_double_value(pc_InitialKcFactor, j, "InitialKcFactor");
  set_double_value(pc_LuxuryNCoeff, j, "LuxuryNCoeff");
  set_double_value(pc_MaxCropDiameter, j, "MaxCropDiameter");
  set_double_value(pc_StageAtMaxHeight, j, "StageAtMaxHeight");
  set_double_value(pc_StageAtMaxDiameter, j, "StageAtMaxDiameter");
  set_double_value(pc_MinimumNConcentration, j, "MinimumNConcentration");
  set_double_value(pc_MinimumTemperatureForAssimilation, j, "MinimumTemperatureForAssimilation");
  set_double_value(pc_OptimumTemperatureForAssimilation, j, "OptimumTemperatureForAssimilation");
  set_double_value(pc_MaximumTemperatureForAssimilation, j, "MaximumTemperatureForAssimilation");
  set_double_value(pc_NConcentrationAbovegroundBiomass, j, "NConcentrationAbovegroundBiomass");
  set_double_value(pc_NConcentrationB0, j, "NConcentrationB0");
  set_double_value(pc_NConcentrationPN, j, "NConcentrationPN");
  set_double_value(pc_NConcentrationRoot, j, "NConcentrationRoot");
  set_int_value(pc_DevelopmentAccelerationByNitrogenStress, j, "DevelopmentAccelerationByNitrogenStress");
  set_double_value(pc_FieldConditionModifier, j, "FieldConditionModifier");
  set_double_value(pc_AssimilateReallocation, j, "AssimilateReallocation");
  set_double_vector(pc_BaseTemperature, j, "BaseTemperature");
  set_double_vector(pc_OrganMaintenanceRespiration, j, "OrganMaintenanceRespiration");
  set_double_vector(pc_OrganGrowthRespiration, j, "OrganGrowthRespiration");
  set_double_vector(pc_StageMaxRootNConcentration, j, "StageMaxRootNConcentration");
  set_double_vector(pc_InitialOrganBiomass, j, "InitialOrganBiomass");
  set_double_vector(pc_CriticalOxygenContent, j, "CriticalOxygenContent");

  set_double_vector(pc_StageMobilFromStorageCoeff, j, "StageMobilFromStorageCoeff");
  if (pc_StageMobilFromStorageCoeff.empty())
    pc_StageMobilFromStorageCoeff = vector<double>(pc_CriticalOxygenContent.size(), 0);

  set_bool_vector(pc_AbovegroundOrgan, j, "AbovegroundOrgan");
  set_bool_vector(pc_StorageOrgan, j, "StorageOrgan");
  set_double_value(pc_SamplingDepth, j, "SamplingDepth");
  set_double_value(pc_TargetNSamplingDepth, j, "TargetNSamplingDepth");
  set_double_value(pc_TargetN30, j, "TargetN30");
  set_double_value(pc_MaxNUptakeParam, j, "MaxNUptakeParam");
  set_double_value(pc_RootDistributionParam, j, "RootDistributionParam");
  set_int_value(pc_PlantDensity, j, "PlantDensity");
  set_double_value(pc_RootGrowthLag, j, "RootGrowthLag");
  set_double_value(pc_MinimumTemperatureRootGrowth, j, "MinimumTemperatureRootGrowth");
  set_double_value(pc_InitialRootingDepth, j, "InitialRootingDepth");
  set_double_value(pc_RootPenetrationRate, j, "RootPenetrationRate");
  set_double_value(pc_RootFormFactor, j, "RootFormFactor");
  set_double_value(pc_SpecificRootLength, j, "SpecificRootLength");
  set_int_value(pc_StageAfterCut, j, "StageAfterCut");
  set_double_value(pc_LimitingTemperatureHeatStress, j, "LimitingTemperatureHeatStress");
  set_int_value(pc_CuttingDelayDays, j, "CuttingDelayDays");
  set_double_value(pc_DroughtImpactOnFertilityFactor, j, "DroughtImpactOnFertilityFactor");

  set_double_value(EF_MONO, j, "EF_MONO");
  set_double_value(EF_MONOS, j, "EF_MONOS");
  set_double_value(EF_ISO, j, "EF_ISO");
  set_double_value(VCMAX25, j, "VCMAX25");
  set_double_value(AEKC, j, "AEKC");
  set_double_value(AEVC, j, "AEVC");
  set_double_value(AEKO, j, "AEKO");
  set_double_value(KC25, j, "KC25");
  set_double_value(KO25, j, "KO25");

  set_int_value(pc_TransitionStageLeafExp, j, "TransitionStageLeafExp");

  return res;
}

json11::Json SpeciesParameters::to_json() const {
  auto species = J11Object
      {{"type",                                    "SpeciesParameters"},
       {"SpeciesName",                             pc_SpeciesId},
       {"CarboxylationPathway",                    pc_CarboxylationPathway},
       {"DefaultRadiationUseEfficiency",           pc_DefaultRadiationUseEfficiency},
       {"PartBiologicalNFixation",                 pc_PartBiologicalNFixation},
       {"InitialKcFactor",                         pc_InitialKcFactor},
       {"LuxuryNCoeff",                            pc_LuxuryNCoeff},
       {"MaxCropDiameter",                         pc_MaxCropDiameter},
       {"StageAtMaxHeight",                        pc_StageAtMaxHeight},
       {"StageAtMaxDiameter",                      pc_StageAtMaxDiameter},
       {"MinimumNConcentration",                   pc_MinimumNConcentration},
       {"MinimumTemperatureForAssimilation",       pc_MinimumTemperatureForAssimilation},
       {"OptimumTemperatureForAssimilation",       pc_OptimumTemperatureForAssimilation},
       {"MaximumTemperatureForAssimilation",       pc_MaximumTemperatureForAssimilation},
       {"NConcentrationAbovegroundBiomass",        pc_NConcentrationAbovegroundBiomass},
       {"NConcentrationB0",                        pc_NConcentrationB0},
       {"NConcentrationPN",                        pc_NConcentrationPN},
       {"NConcentrationRoot",                      pc_NConcentrationRoot},
       {"DevelopmentAccelerationByNitrogenStress", pc_DevelopmentAccelerationByNitrogenStress},
       {"FieldConditionModifier",                  pc_FieldConditionModifier},
       {"AssimilateReallocation",                  pc_AssimilateReallocation},
       {"BaseTemperature",                         toPrimJsonArray(pc_BaseTemperature)},
       {"OrganMaintenanceRespiration",             toPrimJsonArray(pc_OrganMaintenanceRespiration)},
       {"OrganGrowthRespiration",                  toPrimJsonArray(pc_OrganGrowthRespiration)},
       {"StageMaxRootNConcentration",              toPrimJsonArray(pc_StageMaxRootNConcentration)},
       {"InitialOrganBiomass",                     toPrimJsonArray(pc_InitialOrganBiomass)},
       {"CriticalOxygenContent",                   toPrimJsonArray(pc_CriticalOxygenContent)},
       {"StageMobilFromStorageCoeff",              toPrimJsonArray(pc_StageMobilFromStorageCoeff)},
       {"AbovegroundOrgan",                        toPrimJsonArray(pc_AbovegroundOrgan)},
       {"StorageOrgan",                            toPrimJsonArray(pc_StorageOrgan)},
       {"SamplingDepth",                           pc_SamplingDepth},
       {"TargetNSamplingDepth",                    pc_TargetNSamplingDepth},
       {"TargetN30",                               pc_TargetN30},
       {"MaxNUptakeParam",                         pc_MaxNUptakeParam},
       {"RootDistributionParam",                   pc_RootDistributionParam},
       {"PlantDensity",                            J11Array{pc_PlantDensity, "plants m-2"}},
       {"RootGrowthLag",                           pc_RootGrowthLag},
       {"MinimumTemperatureRootGrowth",            pc_MinimumTemperatureRootGrowth},
       {"InitialRootingDepth",                     pc_InitialRootingDepth},
       {"RootPenetrationRate",                     pc_RootPenetrationRate},
       {"RootFormFactor",                          pc_RootFormFactor},
       {"SpecificRootLength",                      pc_SpecificRootLength},
       {"StageAfterCut",                           pc_StageAfterCut},
       {"LimitingTemperatureHeatStress",           pc_LimitingTemperatureHeatStress},
       {"CuttingDelayDays",                        pc_CuttingDelayDays},
       {"DroughtImpactOnFertilityFactor",          pc_DroughtImpactOnFertilityFactor},
       {"EF_MONO",                                 J11Array{EF_MONO, "ug gDW-1 h-1"}},
       {"EF_MONOS",                                J11Array{EF_MONOS, "ug gDW-1 h-1"}},
       {"EF_ISO",                                  J11Array{EF_ISO, "ug gDW-1 h-1"}},
       {"VCMAX25",                                 J11Array{VCMAX25, "umol m-2 s-1"}},
       {"AEKC",                                    J11Array{AEKC, "J mol-1"}},
       {"AEKO",                                    J11Array{AEKO, "J mol-1"}},
       {"AEVC",                                    J11Array{AEVC, "J mol-1"}},
       {"KC25",                                    J11Array{KC25, "umol mol-1 ubar-1"}},
       {"KO25",                                    J11Array{KO25, "mmol mol-1 mbar-1"}},
       {"TransitionStageLeafExp",                  J11Array{pc_TransitionStageLeafExp, "1-7"}}
      };

  return species;
}

size_t SpeciesParameters::pc_NumberOfDevelopmentalStages() const {
  return pc_BaseTemperature.size();
}
size_t SpeciesParameters::pc_NumberOfOrgans() const {
  return pc_OrganGrowthRespiration.size();
}

// CultivarParameters::CultivarParameters(json11::Json j) {
//   merge(j);
// }

void CultivarParameters::deserialize(mas::schema::model::monica::CultivarParameters::Reader reader) {
  pc_CultivarId = reader.getCultivarId();
  pc_Description = reader.getDescription();
  pc_Perennial = reader.getPerennial();
  pc_MaxAssimilationRate = reader.getMaxAssimilationRate();
  pc_MaxCropHeight = reader.getMaxCropHeight();
  pc_ResidueNRatio = reader.getResidueNRatio();
  pc_LT50cultivar = reader.getLt50cultivar();
  pc_CropHeightP1 = reader.getCropHeightP1();
  pc_CropHeightP2 = reader.getCropHeightP2();
  pc_CropSpecificMaxRootingDepth = reader.getCropSpecificMaxRootingDepth();

  {
    auto listReader = reader.getAssimilatePartitioningCoeff();
    capnp::uint i = 0;
    for (auto &v: pc_AssimilatePartitioningCoeff) setFromCapnpList(v, listReader[i++]);
  }

  {
    auto listReader = reader.getOrganSenescenceRate();
    capnp::uint i = 0;
    for (auto &v: pc_OrganSenescenceRate) setFromCapnpList(v, listReader[i++]);
  }

  setFromCapnpList(pc_BaseDaylength, reader.getBaseDaylength());
  setFromCapnpList(pc_OptimumTemperature, reader.getOptimumTemperature());
  setFromCapnpList(pc_DaylengthRequirement, reader.getDaylengthRequirement());
  setFromCapnpList(pc_DroughtStressThreshold, reader.getDroughtStressThreshold());
  setFromCapnpList(pc_SpecificLeafArea, reader.getSpecificLeafArea());
  setFromCapnpList(pc_StageKcFactor, reader.getStageKcFactor());
  setFromCapnpList(pc_StageTemperatureSum, reader.getStageTemperatureSum());
  setFromCapnpList(pc_VernalisationRequirement, reader.getVernalisationRequirement());
  pc_HeatSumIrrigationStart = reader.getHeatSumIrrigationStart();
  pc_HeatSumIrrigationEnd = reader.getHeatSumIrrigationEnd();
  pc_CriticalTemperatureHeatStress = reader.getCriticalTemperatureHeatStress();
  pc_BeginSensitivePhaseHeatStress = reader.getBeginSensitivePhaseHeatStress();
  pc_EndSensitivePhaseHeatStress = reader.getEndSensitivePhaseHeatStress();
  pc_FrostHardening = reader.getFrostHardening();
  pc_FrostDehardening = reader.getFrostDehardening();
  pc_LowTemperatureExposure = reader.getLowTemperatureExposure();
  pc_RespiratoryStress = reader.getRespiratoryStress();
  pc_LatestHarvestDoy = reader.getLatestHarvestDoy();
  setFromComplexCapnpList(pc_OrganIdsForPrimaryYield, reader.getOrganIdsForPrimaryYield());
  setFromComplexCapnpList(pc_OrganIdsForSecondaryYield, reader.getOrganIdsForSecondaryYield());
  setFromComplexCapnpList(pc_OrganIdsForCutting, reader.getOrganIdsForCutting());
  pc_EarlyRefLeafExp = reader.getEarlyRefLeafExp();
  pc_RefLeafExp = reader.getRefLeafExp();
  pc_MinTempDev_WE = reader.getMinTempDevWE();
  pc_OptTempDev_WE = reader.getOptTempDevWE();
  pc_MaxTempDev_WE = reader.getMaxTempDevWE();
  winterCrop = reader.getWinterCrop();
}

void CultivarParameters::serialize(mas::schema::model::monica::CultivarParameters::Builder builder) const {
  builder.setCultivarId(pc_CultivarId);
  builder.setDescription(pc_Description);
  builder.setPerennial(pc_Perennial);
  builder.setMaxAssimilationRate(pc_MaxAssimilationRate);
  builder.setMaxCropHeight(pc_MaxCropHeight);
  builder.setResidueNRatio(pc_ResidueNRatio);
  builder.setLt50cultivar(pc_LT50cultivar);
  builder.setCropHeightP1(pc_CropHeightP1);
  builder.setCropHeightP2(pc_CropHeightP2);
  builder.setCropSpecificMaxRootingDepth(pc_CropSpecificMaxRootingDepth);

  {
    auto listBuilder = builder.initAssimilatePartitioningCoeff((capnp::uint) pc_AssimilatePartitioningCoeff.size());
    capnp::uint i = 0;
    for (const auto &v: pc_AssimilatePartitioningCoeff) setCapnpList(v, listBuilder.init(i++, (capnp::uint) v.size()));
  }

  {
    auto listBuilder = builder.initOrganSenescenceRate((capnp::uint) pc_OrganSenescenceRate.size());
    capnp::uint i = 0;
    for (const auto &v: pc_OrganSenescenceRate) setCapnpList(v, listBuilder.init(i++, (capnp::uint) v.size()));
  }

  setCapnpList(pc_BaseDaylength, builder.initBaseDaylength((capnp::uint) pc_BaseDaylength.size()));
  setCapnpList(pc_OptimumTemperature, builder.initOptimumTemperature((capnp::uint) pc_OptimumTemperature.size()));
  setCapnpList(pc_DaylengthRequirement, builder.initDaylengthRequirement((capnp::uint) pc_DaylengthRequirement.size()));
  setCapnpList(pc_DroughtStressThreshold,
               builder.initDroughtStressThreshold((capnp::uint) pc_DroughtStressThreshold.size()));
  setCapnpList(pc_SpecificLeafArea, builder.initSpecificLeafArea((capnp::uint) pc_SpecificLeafArea.size()));
  setCapnpList(pc_StageKcFactor, builder.initStageKcFactor((capnp::uint) pc_StageKcFactor.size()));
  setCapnpList(pc_StageTemperatureSum, builder.initStageTemperatureSum((capnp::uint) pc_StageTemperatureSum.size()));
  setCapnpList(pc_VernalisationRequirement,
               builder.initVernalisationRequirement((capnp::uint) pc_VernalisationRequirement.size()));
  builder.setHeatSumIrrigationStart(pc_HeatSumIrrigationStart);
  builder.setHeatSumIrrigationEnd(pc_HeatSumIrrigationEnd);
  builder.setCriticalTemperatureHeatStress(pc_CriticalTemperatureHeatStress);
  builder.setBeginSensitivePhaseHeatStress(pc_BeginSensitivePhaseHeatStress);
  builder.setEndSensitivePhaseHeatStress(pc_EndSensitivePhaseHeatStress);
  builder.setFrostHardening(pc_FrostHardening);
  builder.setFrostDehardening(pc_FrostDehardening);
  builder.setLowTemperatureExposure(pc_LowTemperatureExposure);
  builder.setRespiratoryStress(pc_RespiratoryStress);
  builder.setLatestHarvestDoy(pc_LatestHarvestDoy);
  setComplexCapnpList(pc_OrganIdsForPrimaryYield,
                      builder.initOrganIdsForPrimaryYield((capnp::uint) pc_OrganIdsForPrimaryYield.size()));
  setComplexCapnpList(pc_OrganIdsForSecondaryYield,
                      builder.initOrganIdsForSecondaryYield((capnp::uint) pc_OrganIdsForSecondaryYield.size()));
  setComplexCapnpList(pc_OrganIdsForCutting,
                      builder.initOrganIdsForCutting((capnp::uint) pc_OrganIdsForCutting.size()));
  builder.setEarlyRefLeafExp(pc_EarlyRefLeafExp);
  builder.setRefLeafExp(pc_RefLeafExp);
  builder.setMinTempDevWE(pc_MinTempDev_WE);
  builder.setOptTempDevWE(pc_OptTempDev_WE);
  builder.setMaxTempDevWE(pc_MaxTempDev_WE);
  builder.setWinterCrop(winterCrop);
}

Errors CultivarParameters::merge(json11::Json j) {
  Errors res = Json11Serializable::merge(j);

  string err;
  if (j.has_shape({{"OrganIdsForPrimaryYield", json11::Json::ARRAY}}, err))
    pc_OrganIdsForPrimaryYield = toVector<YieldComponent>(j["OrganIdsForPrimaryYield"]);
  else
    res.errors.push_back(string("Couldn't read 'OrganIdsForPrimaryYield' key from JSON object:\n") + j.dump());

  if (j.has_shape({{"OrganIdsForSecondaryYield", json11::Json::ARRAY}}, err))
    pc_OrganIdsForSecondaryYield = toVector<YieldComponent>(j["OrganIdsForSecondaryYield"]);
  else
    res.errors.push_back(string("Couldn't read 'OrganIdsForSecondaryYield' key from JSON object:\n") + j.dump());

  if (j.has_shape({{"OrganIdsForCutting", json11::Json::ARRAY}}, err))
    pc_OrganIdsForCutting = toVector<YieldComponent>(j["OrganIdsForCutting"]);
  else
    res.warnings.push_back(string("Couldn't read 'OrganIdsForCutting' key from JSON object:\n") + j.dump());

  set_string_value(pc_CultivarId, j, "CultivarName");
  set_string_value(pc_Description, j, "Description");
  set_bool_value(pc_Perennial, j, "Perennial");
  set_double_value(pc_MaxAssimilationRate, j, "MaxAssimilationRate");
  set_double_value(pc_MaxCropHeight, j, "MaxCropHeight");
  set_double_value(pc_ResidueNRatio, j, "ResidueNRatio");
  set_double_value(pc_LT50cultivar, j, "LT50cultivar");
  set_double_value(pc_CropHeightP1, j, "CropHeightP1");
  set_double_value(pc_CropHeightP2, j, "CropHeightP2");
  set_double_value(pc_CropSpecificMaxRootingDepth, j, "CropSpecificMaxRootingDepth");
  set_double_vector(pc_BaseDaylength, j, "BaseDaylength");
  set_double_vector(pc_OptimumTemperature, j, "OptimumTemperature");
  set_double_vector(pc_DaylengthRequirement, j, "DaylengthRequirement");
  set_double_vector(pc_DroughtStressThreshold, j, "DroughtStressThreshold");
  set_double_vector(pc_SpecificLeafArea, j, "SpecificLeafArea");
  set_double_vector(pc_StageKcFactor, j, "StageKcFactor");
  set_double_vector(pc_StageTemperatureSum, j, "StageTemperatureSum");
  set_double_vector(pc_VernalisationRequirement, j, "VernalisationRequirement");
  set_double_value(pc_HeatSumIrrigationStart, j, "HeatSumIrrigationStart");
  set_double_value(pc_HeatSumIrrigationEnd, j, "HeatSumIrrigationEnd");
  set_double_value(pc_CriticalTemperatureHeatStress, j, "CriticalTemperatureHeatStress");
  set_double_value(pc_BeginSensitivePhaseHeatStress, j, "BeginSensitivePhaseHeatStress");
  set_double_value(pc_EndSensitivePhaseHeatStress, j, "EndSensitivePhaseHeatStress");
  set_double_value(pc_FrostHardening, j, "FrostHardening");
  set_double_value(pc_FrostDehardening, j, "FrostDehardening");
  set_double_value(pc_LowTemperatureExposure, j, "LowTemperatureExposure");
  set_double_value(pc_RespiratoryStress, j, "RespiratoryStress");
  set_int_value(pc_LatestHarvestDoy, j, "LatestHarvestDoy");
  set_bool_value(winterCrop, j, "WinterCrop");

  if (j["AssimilatePartitioningCoeff"].is_array()) {
    auto apcs = j["AssimilatePartitioningCoeff"].array_items();
    int i = 0;
    pc_AssimilatePartitioningCoeff.resize(apcs.size());
    for (auto js: apcs)
      pc_AssimilatePartitioningCoeff[i++] = double_vector(js);
  }
  if (j["OrganSenescenceRate"].is_array()) {
    auto osrs = j["OrganSenescenceRate"].array_items();
    int i = 0;
    pc_OrganSenescenceRate.resize(osrs.size());
    for (auto js: osrs)
      pc_OrganSenescenceRate[i++] = double_vector(js);
  }

  set_double_value(pc_EarlyRefLeafExp, j, "EarlyRefLeafExp");
  set_double_value(pc_RefLeafExp, j, "RefLeafExp");

  set_double_value(pc_MinTempDev_WE, j, "MinTempDev_WE");
  set_double_value(pc_OptTempDev_WE, j, "OptTempDev_WE");
  set_double_value(pc_MaxTempDev_WE, j, "MaxTempDev_WE");


  return res;
}

json11::Json CultivarParameters::to_json() const {
  J11Array apcs;
  for (auto v: pc_AssimilatePartitioningCoeff)
    apcs.push_back(toPrimJsonArray(v));

  J11Array osrs;
  for (auto v: pc_OrganSenescenceRate)
    osrs.push_back(toPrimJsonArray(v));

  auto cultivar = J11Object
      {{"type",                          "CultivarParameters"},
       {"CultivarName",                  pc_CultivarId},
       {"Description",                   pc_Description},
       {"Perennial",                     pc_Perennial},
       {"MaxAssimilationRate",           pc_MaxAssimilationRate},
       {"MaxCropHeight",                 J11Array{pc_MaxCropHeight, "m"}},
       {"ResidueNRatio",                 pc_ResidueNRatio},
       {"LT50cultivar",                  pc_LT50cultivar},
       {"CropHeightP1",                  pc_CropHeightP1},
       {"CropHeightP2",                  pc_CropHeightP2},
       {"CropSpecificMaxRootingDepth",   pc_CropSpecificMaxRootingDepth},
       {"AssimilatePartitioningCoeff",   apcs},
       {"OrganSenescenceRate",           osrs},
       {"BaseDaylength",                 J11Array{toPrimJsonArray(pc_BaseDaylength), "h"}},
       {"OptimumTemperature",            J11Array{toPrimJsonArray(pc_OptimumTemperature), "°C"}},
       {"DaylengthRequirement",          J11Array{toPrimJsonArray(pc_DaylengthRequirement), "h"}},
       {"DroughtStressThreshold",        toPrimJsonArray(pc_DroughtStressThreshold)},
       {"SpecificLeafArea",              J11Array{toPrimJsonArray(pc_SpecificLeafArea), "ha kg-1"}},
       {"StageKcFactor",                 J11Array{toPrimJsonArray(pc_StageKcFactor), "1;0"}},
       {"StageTemperatureSum",           J11Array{toPrimJsonArray(pc_StageTemperatureSum), "°C d"}},
       {"VernalisationRequirement",      toPrimJsonArray(pc_VernalisationRequirement)},
       {"HeatSumIrrigationStart",        pc_HeatSumIrrigationStart},
       {"HeatSumIrrigationEnd",          pc_HeatSumIrrigationEnd},
       {"CriticalTemperatureHeatStress", J11Array{pc_CriticalTemperatureHeatStress, "°C"}},
       {"BeginSensitivePhaseHeatStress", J11Array{pc_BeginSensitivePhaseHeatStress, "°C d"}},
       {"EndSensitivePhaseHeatStress",   J11Array{pc_EndSensitivePhaseHeatStress, "°C d"}},
       {"FrostHardening",                pc_FrostHardening},
       {"FrostDehardening",              pc_FrostDehardening},
       {"LowTemperatureExposure",        pc_LowTemperatureExposure},
       {"RespiratoryStress",             pc_RespiratoryStress},
       {"LatestHarvestDoy",              pc_LatestHarvestDoy},
       {"OrganIdsForPrimaryYield",       toJsonArray(pc_OrganIdsForPrimaryYield)},
       {"OrganIdsForSecondaryYield",     toJsonArray(pc_OrganIdsForSecondaryYield)},
       {"OrganIdsForCutting",            toJsonArray(pc_OrganIdsForCutting)},
       {"EarlyRefLeafExp",               pc_EarlyRefLeafExp},
       {"RefLeafExp",                    pc_RefLeafExp},
       {"MinTempDev_WE",                 pc_MinTempDev_WE},
       {"OptTempDev_WE",                 pc_OptTempDev_WE},
       {"MaxTempDev_WE",                 pc_MaxTempDev_WE},
       {"WinterCrop",                    winterCrop}
      };

  return cultivar;
}



// CropParameters::CropParameters(json11::Json j) {
//   merge(j["species"], j["cultivar"]);
// }

// CropParameters::CropParameters(json11::Json sj, json11::Json cj) {
//   merge(sj, cj);
// }

void CropParameters::deserialize(mas::schema::model::monica::CropParameters::Reader reader) {
  speciesParams.deserialize(reader.getSpeciesParams());
  cultivarParams.deserialize(reader.getCultivarParams());
}

void CropParameters::serialize(mas::schema::model::monica::CropParameters::Builder builder) const {
  speciesParams.serialize(builder.initSpeciesParams());
  cultivarParams.serialize(builder.initCultivarParams());
}

Errors CropParameters::merge(json11::Json j) {
  auto evff = j["__enable_vernalisation_factor_fix__"];
  if (!evff.is_null() && evff.is_bool()) __enable_vernalisation_factor_fix__ = evff.bool_value();
  return merge(j["species"], j["cultivar"]);
}

Errors CropParameters::merge(json11::Json sj, json11::Json cj) {
  Errors res;
  res.append(speciesParams.merge(sj));
  res.append(cultivarParams.merge(cj));
  return res;
}

json11::Json CropParameters::to_json() const {
  return J11Object
      {{"type",     "CropParameters"},
       {"species",  speciesParams.to_json()},
       {"cultivar", cultivarParams.to_json()}
      };
}




MineralFertilizerParameters::MineralFertilizerParameters(const string &id,
                                                         const std::string &name,
                                                         double carbamid,
                                                         double no3,
                                                         double nh4)
    : id(id),
      name(name),
      vo_Carbamid(carbamid),
      vo_NH4(nh4),
      vo_NO3(no3) {}

// MineralFertilizerParameters::MineralFertilizerParameters(json11::Json j) {
//   merge(j);
// }

void MineralFertilizerParameters::deserialize(
    mas::schema::model::monica::Params::MineralFertilization::Parameters::Reader reader) {
  id = reader.getId();
  name = reader.getName();
  vo_Carbamid = reader.getCarbamid();
  vo_NH4 = reader.getNh4();
  vo_NO3 = reader.getNo3();
}

void MineralFertilizerParameters::serialize(
    mas::schema::model::monica::Params::MineralFertilization::Parameters::Builder builder) const {
  builder.setId(id);
  builder.setName(name);
  builder.setCarbamid(vo_Carbamid);
  builder.setNh4(vo_NH4);
  builder.setNo3(vo_NO3);
}

Errors MineralFertilizerParameters::merge(json11::Json j) {
  Errors res = Json11Serializable::merge(j);

  set_string_value(id, j, "id");
  set_string_value(name, j, "name");
  set_double_value(vo_Carbamid, j, "Carbamid");
  set_double_value(vo_NH4, j, "NH4");
  set_double_value(vo_NO3, j, "NO3");

  return res;
}

json11::Json MineralFertilizerParameters::to_json() const {
  return J11Object
      {{"type",     "MineralFertilizerParameters"},
       {"id",       id},
       {"name",     name},
       {"Carbamid", vo_Carbamid},
       {"NH4",      vo_NH4},
       {"NO3",      vo_NO3}
      };
}

NMinApplicationParameters::NMinApplicationParameters(double min,
                                                     double max,
                                                     int delayInDays)
    : min(min), max(max), delayInDays(delayInDays) {}

// NMinApplicationParameters::NMinApplicationParameters(json11::Json j) {
//   merge(j);
// }

void NMinApplicationParameters::deserialize(mas::schema::model::monica::NMinApplicationParameters::Reader reader) {
  min = reader.getMin();
  max = reader.getMax();
  delayInDays = reader.getDelayInDays();
}

void
NMinApplicationParameters::serialize(mas::schema::model::monica::NMinApplicationParameters::Builder builder) const {
  builder.setMin(min);
  builder.setMax(max);
  builder.setDelayInDays(delayInDays);
}

Errors NMinApplicationParameters::merge(json11::Json j) {
  Errors res = Json11Serializable::merge(j);

  set_double_value(min, j, "min");
  set_double_value(max, j, "max");
  set_int_value(delayInDays, j, "delayInDays");

  return res;
}

json11::Json NMinApplicationParameters::to_json() const {
  return json11::Json::object
      {{"type",        "NMinApplicationParameters"},
       {"min",         min},
       {"max",         max},
       {"delayInDays", delayInDays}
      };
}



IrrigationParameters::IrrigationParameters(double nitrateConcentration,
                                           double sulfateConcentration)
    : nitrateConcentration(nitrateConcentration),
      sulfateConcentration(sulfateConcentration) {}

// IrrigationParameters::IrrigationParameters(json11::Json j) {
//   merge(j);
// }

void IrrigationParameters::deserialize(mas::schema::model::monica::Params::Irrigation::Parameters::Reader reader) {
  nitrateConcentration = reader.getNitrateConcentration();
  sulfateConcentration = reader.getSulfateConcentration();
}

void
IrrigationParameters::serialize(mas::schema::model::monica::Params::Irrigation::Parameters::Builder builder) const {
  builder.setNitrateConcentration(nitrateConcentration);
  builder.setSulfateConcentration(sulfateConcentration);
}

Errors IrrigationParameters::merge(json11::Json j) {
  Errors res = Json11Serializable::merge(j);

  set_double_value(nitrateConcentration, j, "nitrateConcentration");
  set_double_value(sulfateConcentration, j, "sulfateConcentration");

  return res;
}

json11::Json IrrigationParameters::to_json() const {
  return json11::Json::object
      {{"type",                 "IrrigationParameters"},
       {"nitrateConcentration", J11Array{nitrateConcentration, "mg dm-3"}},
       {"sulfateConcentration", J11Array{sulfateConcentration, "mg dm-3"}}
      };
}



AutomaticIrrigationParameters::AutomaticIrrigationParameters(double a,
                                                             double t,
                                                             double nc,
                                                             double sc)
    : IrrigationParameters(nc, sc), amount(a), threshold(t) {}

// AutomaticIrrigationParameters::AutomaticIrrigationParameters(json11::Json j) {
//   merge(j);
// }

void
AutomaticIrrigationParameters::deserialize(mas::schema::model::monica::AutomaticIrrigationParameters::Reader reader) {
  IrrigationParameters::deserialize(reader.getParams());
  amount = reader.getAmount();
  threshold = reader.getThreshold();
  //percentNFC = reader.getPercentNfc();
}

void AutomaticIrrigationParameters::serialize(
    mas::schema::model::monica::AutomaticIrrigationParameters::Builder builder) const {
  IrrigationParameters::serialize(builder.initParams());
  builder.setAmount(amount);
  builder.setThreshold(threshold);
  //builder.setPercentNfc(percentNFC);
}

Errors AutomaticIrrigationParameters::merge(json11::Json j) {
  Errors res = Json11Serializable::merge(j);

  res.append(IrrigationParameters::merge(j["irrigationParameters"]));
  set_iso_date_value(startDate, j, "startDate");
  set_double_value(amount, j, "amount");
  set_double_value(percentNFC, j, "set_to_%nFC");
  set_double_value(threshold, j, "threshold", transformIfPercent(j, "threshold"));
  set_double_value(threshold, j, "trigger_if_nFC_below_%", [](double v) { return v / 100.0; });
  set_double_value(criticalMoistureDepthM, j, "calc_nFC_until_depth_m",
                   transformIfNotMeters(j, "calc_nFC_until_depth_m"));
  set_int_value(minDaysBetweenIrrigationEvents, j, "minDaysBetweenIrrigationEvents");

  return res;
}

json11::Json AutomaticIrrigationParameters::to_json() const {
  auto o = json11::Json::object
      {{"type",                 "AutomaticIrrigationParameters"},
       {"startDate",            startDate.toIsoDateString()},
       {"irrigationParameters", IrrigationParameters::to_json()},
       {"trigger_if_nFC_below_%",            J11Array{threshold*100.0, "%"}},
       {"calc_nFC_until_depth_m",J11Array{criticalMoistureDepthM, "m"}},
       {"minDaysBetweenIrrigationEvents", J11Array{minDaysBetweenIrrigationEvents, "d"}}
      };
  if (amount > 0) o["amount"] = J11Array{amount, "mm"};
  else o["set_to_%nFC"] = J11Array{percentNFC, "%"};
  return o;
}

void MeasuredGroundwaterTableInformation::deserialize(
    mas::schema::model::monica::MeasuredGroundwaterTableInformation::Reader reader) {
  groundwaterInformationAvailable = reader.getGroundwaterInformationAvailable();
  groundwaterInfo.clear();
  for (auto gi: reader.getGroundwaterInfo()) groundwaterInfo[Date(gi.getDate())] = gi.getValue();
}

void MeasuredGroundwaterTableInformation::serialize(
    mas::schema::model::monica::MeasuredGroundwaterTableInformation::Builder builder) const {
  builder.setGroundwaterInformationAvailable(groundwaterInformationAvailable);
  auto gis = builder.initGroundwaterInfo((capnp::uint) groundwaterInfo.size());
  capnp::uint i = 0;
  for (auto p: groundwaterInfo) {
    p.first.serialize(gis[i].initDate());
    gis[i].setValue(p.second);
  }
}

// MeasuredGroundwaterTableInformation::MeasuredGroundwaterTableInformation(json11::Json j) {
//   merge(j);
// }

Errors MeasuredGroundwaterTableInformation::merge(json11::Json j) {
  Errors res;

  set_bool_value(groundwaterInformationAvailable, j, "groundwaterInformationAvailable");

  string err = "";
  if (j.has_shape({{"groundwaterInfo", json11::Json::OBJECT}}, err))
    for (auto p: j["groundwaterInfo"].object_items())
      groundwaterInfo[Tools::Date::fromIsoDateString(p.first)] = p.second.number_value();
  else
    res.errors.push_back(string("Couldn't read 'groundwaterInfo' key from JSON object:\n") + j.dump());

  return res;
}

json11::Json MeasuredGroundwaterTableInformation::to_json() const {
  json11::Json::object gi;
  for (auto p: groundwaterInfo)
    gi[p.first.toIsoDateString()] = p.second;

  return json11::Json::object
      {{"type",                            "MeasuredGroundwaterTableInformation"},
       {"groundwaterInformationAvailable", groundwaterInformationAvailable},
       {"groundwaterInfo",                 gi}
      };
}

/*
void MeasuredGroundwaterTableInformation::readInGroundwaterInformation(std::string path) {
  ifstream ifs(path.c_str(), ios::in);
  if (!ifs.is_open()) {
    cout << "ERROR while opening file " << path.c_str() << endl;
    return;
  }

  groundwaterInformationAvailable = true;

  // read in information from groundwater table file
  string s;
  while (getline(ifs, s)) {
    // date, value
    std::string date_string;
    double gw_cm;

    istringstream ss(s);
    ss >> date_string >> gw_cm;

    Date gw_date = Tools::fromMysqlString(date_string.c_str());

    if (!gw_date.isValid()) {
      debug() << "ERROR - Invalid date in \"" << path.c_str() << "\"" << endl;
      debug() << "Line: " << s.c_str() << endl;
      continue;
    }
    cout << "Added gw value\t" << gw_date.toString().c_str() << "\t" << gw_cm << endl;
    groundwaterInfo[gw_date] = gw_cm;
  }
}
 */

std::pair<bool, double> MeasuredGroundwaterTableInformation::getGroundwaterInformation(Tools::Date gwDate) const
{
  if (groundwaterInformationAvailable && !groundwaterInfo.empty())
  {
    auto it = groundwaterInfo.find(gwDate);
    if (it != groundwaterInfo.end())
      return make_pair(true, it->second);
  }
  return make_pair(false, 0);
}

//std::map<std::string, std::function<Tools::Errors(Soil::SoilParameters*)>>
//SiteParameters::calculateAndSetPwpFcSatFunctions = std::map<std::string, std::function<Tools::Errors(Soil::SoilParameters*)>>();

void SiteParameters::deserialize(mas::schema::model::monica::SiteParameters::Reader reader) {
  vs_Latitude = reader.getLatitude();
  vs_Slope = reader.getSlope();
  vs_HeightNN = reader.getHeightNN();
  vs_GroundwaterDepth = reader.getGroundwaterDepth();
  vs_Soil_CN_Ratio = reader.getSoilCNRatio();
  vs_DrainageCoeff = reader.getDrainageCoeff();
  vq_NDeposition = reader.getVqNDeposition();
  vs_MaxEffectiveRootingDepth = reader.getMaxEffectiveRootingDepth();
  vs_ImpenetrableLayerDepth = reader.getImpenetrableLayerDepth();
  vs_SoilSpecificHumusBalanceCorrection = reader.getSoilSpecificHumusBalanceCorrection();
  setFromComplexCapnpList(vs_SoilParameters, reader.getSoilParameters());
}

void SiteParameters::serialize(mas::schema::model::monica::SiteParameters::Builder builder) const {
  builder.setLatitude(vs_Latitude);
  builder.setSlope(vs_Slope);
  builder.setHeightNN(vs_HeightNN);
  builder.setGroundwaterDepth(vs_GroundwaterDepth);
  builder.setSoilCNRatio(vs_Soil_CN_Ratio);
  builder.setDrainageCoeff(vs_DrainageCoeff);
  builder.setVqNDeposition(vq_NDeposition);
  builder.setMaxEffectiveRootingDepth(vs_MaxEffectiveRootingDepth);
  builder.setImpenetrableLayerDepth(vs_ImpenetrableLayerDepth);
  builder.setSoilSpecificHumusBalanceCorrection(vs_SoilSpecificHumusBalanceCorrection);
  setComplexCapnpList(vs_SoilParameters, builder.initSoilParameters((capnp::uint) vs_SoilParameters.size()));
}

// SiteParameters::SiteParameters(json11::Json j) {
//   merge(j);
// }

Errors SiteParameters::merge(json11::Json j) {
  Errors res = Json11Serializable::merge(j);

  string err;
  set_double_value(vs_Latitude, j, "Latitude");
  set_double_value(vs_Slope, j, "Slope");
  set_double_value(vs_HeightNN, j, "HeightNN");
  set_double_value(vs_GroundwaterDepth, j, "GroundwaterDepth");
  set_double_value(vs_Soil_CN_Ratio, j, "Soil_CN_Ratio");
  set_double_value(vs_DrainageCoeff, j, "DrainageCoeff");
  set_double_value(vq_NDeposition, j, "NDeposition");
  set_double_value(vs_MaxEffectiveRootingDepth, j, "MaxEffectiveRootingDepth");
  set_double_value(vs_ImpenetrableLayerDepth, j, "ImpenetrableLayerDepth");
  set_double_value(vs_SoilSpecificHumusBalanceCorrection, j, "SoilSpecificHumusBalanceCorrection");
  set_double_value(bareSoilKcFactor, j, "Bare_soil_KC_factor");
  set_string_value(pwpFcSatFunction, j, "pwpFcSatFunction");

  set_int_value(numberOfLayers, j, "NumberOfLayers");
  set_double_value(layerThickness, j, "LayerThickness");

  std::function selectedSetPwpFcSatFunction = noSetPwpFcSat;
  if (const auto it = calculateAndSetPwpFcSatFunctions.find(pwpFcSatFunction);
    it != calculateAndSetPwpFcSatFunctions.end()) {
    selectedSetPwpFcSatFunction = it->second;
  } else {
    res.warnings.push_back("Couldn't find pwpFcSatFunction: " + pwpFcSatFunction);
  }

  if (j.has_shape({{"SoilProfileParameters", json11::Json::ARRAY}}, err)) {
    initSoilProfileSpec = j["SoilProfileParameters"].array_items();
    auto r = createEqualSizedSoilPMs(selectedSetPwpFcSatFunction, initSoilProfileSpec, layerThickness, numberOfLayers);
    if (r.success()) {
      vs_SoilParameters = kj::mv(r.result);
      if (vs_SoilParameters.empty()) res.appendError("Soil profile is empty!");
    } else res.append(r.errors);
  } else if(j["SoilProfileParameters"].is_string() && j["SoilProfileParameters"].string_value().find("capnp") != 0) {
    res.errors.push_back(string("Couldn't read 'SoilProfileParameters' JSON array from JSON object:\n") + j.dump());
  }

  //if (!j["groundwaterInformation"].is_null()) {
  //  res.append(groundwaterInformation.merge(j["groundwaterInformation"]));
  //}

  return res;
}

json11::Json SiteParameters::to_json() const {
  auto sps = J11Object
      {{"type",                               "SiteParameters"},
       {"Latitude",                           J11Array{vs_Latitude, "", "latitude in decimal degrees"}},
       {"Slope",                              J11Array{vs_Slope, "m m-1"}},
       {"HeightNN",                           J11Array{vs_HeightNN, "m", "height above sea level"}},
       {"GroundwaterDepth",                   J11Array{vs_GroundwaterDepth, "m"}},
       {"Soil_CN_Ratio",                      vs_Soil_CN_Ratio},
       {"DrainageCoeff",                      vs_DrainageCoeff},
       {"NDeposition",                        J11Array{vq_NDeposition, "kg N ha-1 y-1"}},
       {"MaxEffectiveRootingDepth",           J11Array{vs_MaxEffectiveRootingDepth, "m"}},
       {"ImpenetrableLayerDepth",             J11Array{vs_ImpenetrableLayerDepth, "m"}},
       {"SoilSpecificHumusBalanceCorrection", J11Array{vs_SoilSpecificHumusBalanceCorrection, "humus equivalents"}},
       {"Bare_soil_KC_factor", bareSoilKcFactor}
      };

  sps["SoilProfileParameters"] = toJsonArray(vs_SoilParameters);

  return sps;
}


AutomaticHarvestParameters::AutomaticHarvestParameters(HarvestTime yt)
    : _harvestTime(yt) {}

void AutomaticHarvestParameters::deserialize(mas::schema::model::monica::AutomaticHarvestParameters::Reader reader) {
  typedef mas::schema::model::monica::AutomaticHarvestParameters::HarvestTime HT;
  _harvestTime = reader.getHarvestTime() == HT::MATURITY ? maturity : unknown;
  _latestHarvestDOY = reader.getLatestHarvestDOY();
}

void
AutomaticHarvestParameters::serialize(mas::schema::model::monica::AutomaticHarvestParameters::Builder builder) const {
  typedef mas::schema::model::monica::AutomaticHarvestParameters::HarvestTime HT;
  builder.setHarvestTime(_harvestTime == maturity ? HT::MATURITY : HT::UNKNOWN);
  builder.setLatestHarvestDOY(_latestHarvestDOY);
}

// AutomaticHarvestParameters::AutomaticHarvestParameters(json11::Json j) {
//   merge(j);
// }

Errors AutomaticHarvestParameters::merge(json11::Json j) {
  Errors res = Json11Serializable::merge(j);

  int ht = -1;
  set_int_value(ht, j, "harvestTime");
  if (ht > -1)
    _harvestTime = HarvestTime(ht);
  set_int_value(_latestHarvestDOY, j, "latestHarvestDOY");

  return res;
}

json11::Json AutomaticHarvestParameters::to_json() const {
  return J11Object
      {{"harvestTime",     int(_harvestTime)},
       {"latestHavestDOY", _latestHarvestDOY}
      };
}



NMinCropParameters::NMinCropParameters(double samplingDepth, double nTarget, double nTarget30)
    : samplingDepth(samplingDepth),
      nTarget(nTarget),
      nTarget30(nTarget30) {}

void NMinCropParameters::deserialize(mas::schema::model::monica::NMinCropParameters::Reader reader) {
  samplingDepth = reader.getSamplingDepth();
  nTarget = reader.getNTarget();
  nTarget30 = reader.getNTarget30();
}

void NMinCropParameters::serialize(mas::schema::model::monica::NMinCropParameters::Builder builder) const {
  builder.setSamplingDepth(samplingDepth);
  builder.setNTarget(nTarget);
  builder.setNTarget30(nTarget30);
}

// NMinCropParameters::NMinCropParameters(json11::Json j) {
//   merge(j);
// }

Errors NMinCropParameters::merge(json11::Json j) {
  Errors res = Json11Serializable::merge(j);

  set_double_value(samplingDepth, j, "samplingDepth");
  set_double_value(nTarget, j, "nTarget");
  set_double_value(nTarget30, j, "nTarget30");

  return res;
}

json11::Json NMinCropParameters::to_json() const {
  return json11::Json::object
      {{"type",          "NMinCropParameters"},
       {"samplingDepth", samplingDepth},
       {"nTarget",       nTarget},
       {"nTarget30",     nTarget30}
      };
}



void OrganicMatterParameters::deserialize(
    mas::schema::model::monica::Params::OrganicFertilization::OrganicMatterParameters::Reader reader) {
  vo_AOM_DryMatterContent = reader.getAomDryMatterContent();
  vo_AOM_NH4Content = reader.getAomNH4Content();
  vo_AOM_NO3Content = reader.getAomNO3Content();
  vo_AOM_CarbamidContent = reader.getAomCarbamidContent();
  vo_AOM_SlowDecCoeffStandard = reader.getAomSlowDecCoeffStandard();
  vo_AOM_FastDecCoeffStandard = reader.getAomFastDecCoeffStandard();
  vo_PartAOM_to_AOM_Slow = reader.getPartAOMToAOMSlow();
  vo_PartAOM_to_AOM_Fast = reader.getPartAOMToAOMFast();
  vo_CN_Ratio_AOM_Slow = reader.getCnRatioAOMSlow();
  vo_CN_Ratio_AOM_Fast = reader.getCnRatioAOMFast();
  vo_PartAOM_Slow_to_SMB_Slow = reader.getPartAOMSlowToSMBSlow();
  vo_PartAOM_Slow_to_SMB_Fast = reader.getPartAOMSlowToSMBFast();
  vo_NConcentration = reader.getNConcentration();
  //vo_CorgContent = reader.getCorgContent();
}

void OrganicMatterParameters::serialize(
    mas::schema::model::monica::Params::OrganicFertilization::OrganicMatterParameters::Builder builder) const {
  builder.setAomDryMatterContent(vo_AOM_DryMatterContent);
  builder.setAomNH4Content(vo_AOM_NH4Content);
  builder.setAomNO3Content(vo_AOM_NO3Content);
  builder.setAomCarbamidContent(vo_AOM_CarbamidContent);
  builder.setAomSlowDecCoeffStandard(vo_AOM_SlowDecCoeffStandard);
  builder.setAomFastDecCoeffStandard(vo_AOM_FastDecCoeffStandard);
  builder.setPartAOMToAOMSlow(vo_PartAOM_to_AOM_Slow);
  builder.setPartAOMToAOMFast(vo_PartAOM_to_AOM_Fast);
  builder.setCnRatioAOMSlow(vo_CN_Ratio_AOM_Slow);
  builder.setCnRatioAOMFast(vo_CN_Ratio_AOM_Fast);
  builder.setPartAOMSlowToSMBSlow(vo_PartAOM_Slow_to_SMB_Slow);
  builder.setPartAOMSlowToSMBFast(vo_PartAOM_Slow_to_SMB_Fast);
  builder.setNConcentration(vo_NConcentration);
  //builder.setCorgContent(vo_CorgContent);
}

// OrganicMatterParameters::OrganicMatterParameters(json11::Json j) {
//   merge(j);
// }

Errors OrganicMatterParameters::merge(json11::Json j) {
  Errors res = Json11Serializable::merge(j);

  set_double_value(vo_AOM_DryMatterContent, j, "AOM_DryMatterContent");
  set_double_value(vo_AOM_NH4Content, j, "AOM_NH4Content");
  set_double_value(vo_AOM_NO3Content, j, "AOM_NO3Content");
  set_double_value(vo_AOM_CarbamidContent, j, "AOM_CarbamidContent");
  set_double_value(vo_AOM_SlowDecCoeffStandard, j, "AOM_SlowDecCoeffStandard");
  set_double_value(vo_AOM_FastDecCoeffStandard, j, "AOM_FastDecCoeffStandard");
  set_double_value(vo_PartAOM_to_AOM_Slow, j, "PartAOM_to_AOM_Slow");
  set_double_value(vo_PartAOM_to_AOM_Fast, j, "PartAOM_to_AOM_Fast");
  set_double_value(vo_CN_Ratio_AOM_Slow, j, "CN_Ratio_AOM_Slow");
  set_double_value(vo_CN_Ratio_AOM_Fast, j, "CN_Ratio_AOM_Fast");
  set_double_value(vo_PartAOM_Slow_to_SMB_Slow, j, "PartAOM_Slow_to_SMB_Slow");
  set_double_value(vo_PartAOM_Slow_to_SMB_Fast, j, "PartAOM_Slow_to_SMB_Fast");
  set_double_value(vo_NConcentration, j, "NConcentration");
  set_double_value(vo_CorgContent, j, "CorgContent");

  return res;
}

json11::Json OrganicMatterParameters::to_json() const {
  return J11Object
      {{"type",                     "OrganicMatterParameters"},
       {"AOM_DryMatterContent",     J11Array{vo_AOM_DryMatterContent, "kg DM kg FM-1",
                                             "Dry matter content of added organic matter"}},
       {"AOM_NH4Content",           J11Array{vo_AOM_NH4Content, "kg N kg DM-1",
                                             "Ammonium content in added organic matter"}},
       {"AOM_NO3Content",           J11Array{vo_AOM_NO3Content, "kg N kg DM-1",
                                             "Nitrate content in added organic matter"}},
       {"AOM_NO3Content",           J11Array{vo_AOM_NO3Content, "kg N kg DM-1",
                                             "Carbamide content in added organic matter"}},
       {"AOM_SlowDecCoeffStandard", J11Array{vo_AOM_SlowDecCoeffStandard, "d-1",
                                             "Decomposition rate coefficient of slow AOM at standard conditions"}},
       {"AOM_FastDecCoeffStandard", J11Array{vo_AOM_FastDecCoeffStandard, "d-1",
                                             "Decomposition rate coefficient of fast AOM at standard conditions"}},
       {"PartAOM_to_AOM_Slow",      J11Array{vo_PartAOM_to_AOM_Slow, "kg kg-1",
                                             "Part of AOM that is assigned to the slowly decomposing pool"}},
       {"PartAOM_to_AOM_Fast",      J11Array{vo_PartAOM_to_AOM_Fast, "kg kg-1",
                                             "Part of AOM that is assigned to the rapidly decomposing pool"}},
       {"CN_Ratio_AOM_Slow",        J11Array{vo_CN_Ratio_AOM_Slow, "",
                                             "C to N ratio of the slowly decomposing AOM pool"}},
       {"CN_Ratio_AOM_Fast",        J11Array{vo_CN_Ratio_AOM_Fast, "",
                                             "C to N ratio of the rapidly decomposing AOM pool"}},
       {"PartAOM_Slow_to_SMB_Slow", J11Array{vo_PartAOM_Slow_to_SMB_Slow, "kg kg-1",
                                             "Part of AOM slow consumed by slow soil microbial biomass"}},
       {"PartAOM_Slow_to_SMB_Fast", J11Array{vo_PartAOM_Slow_to_SMB_Fast, "kg kg-1",
                                             "Part of AOM slow consumed by fast soil microbial biomass"}},
       {"NConcentration",           J11Array{vo_NConcentration, "kg N kg DM-1",
                                             "Nitrogen content in added organic matter"}},
       {"CorgContent",              J11Array{vo_CorgContent, "kg C kg DM-1", "Carbon content in added organic matter"}}
      };
}

void OrganicFertilizerParameters::deserialize(
    mas::schema::model::monica::Params::OrganicFertilization::Parameters::Reader reader) {
  OrganicMatterParameters::deserialize(reader.getParams());
  id = reader.getId();
  name = reader.getName();
}

void OrganicFertilizerParameters::serialize(
    mas::schema::model::monica::Params::OrganicFertilization::Parameters::Builder builder) const {
  OrganicMatterParameters::serialize(builder.initParams());
  builder.setId(id);
  builder.setName(name);
}

// OrganicFertilizerParameters::OrganicFertilizerParameters(json11::Json j) {
//   merge(j);
// }

Errors OrganicFertilizerParameters::merge(json11::Json j) {
  Errors res = Json11Serializable::merge(j);

  res.append(OrganicMatterParameters::merge(j));

  set_string_value(id, j, "id");
  set_string_value(name, j, "name");

  return res;
}

json11::Json OrganicFertilizerParameters::to_json() const {
  auto omp = OrganicMatterParameters::to_json().object_items();
  omp["type"] = "OrganicFertilizerParameters";
  omp["id"] = id;
  omp["name"] = name;
  return omp;
}

void CropResidueParameters::deserialize(mas::schema::model::monica::CropResidueParameters::Reader reader) {
  OrganicMatterParameters::deserialize(reader.getParams());
  species = reader.getSpecies();
  residueType = reader.getResidueType();
}

void CropResidueParameters::serialize(mas::schema::model::monica::CropResidueParameters::Builder builder) const {
  OrganicMatterParameters::serialize(builder.initParams());
  builder.setSpecies(species);
  builder.setResidueType(residueType);
}

// CropResidueParameters::CropResidueParameters(json11::Json j) {
//   merge(j);
// }

Errors CropResidueParameters::merge(json11::Json j) {
  Errors res = Json11Serializable::merge(j);

  res.append(OrganicMatterParameters::merge(j));
  set_string_value(species, j, "species");
  set_string_value(residueType, j, "residueType");

  return res;
}

json11::Json CropResidueParameters::to_json() const {
  auto omp = OrganicMatterParameters::to_json().object_items();
  omp["type"] = "CropResidueParameters";
  omp["species"] = species;
  omp["residueType"] = residueType;
  return omp;
}

void SimulationParameters::deserialize(mas::schema::model::monica::SimulationParameters::Reader reader) {
  startDate.deserialize(reader.getStartDate());
  endDate.deserialize(reader.getEndDate());

  pc_NitrogenResponseOn = reader.getNitrogenResponseOn();
  pc_WaterDeficitResponseOn = reader.getWaterDeficitResponseOn();
  pc_EmergenceFloodingControlOn = reader.getEmergenceFloodingControlOn();
  pc_EmergenceMoistureControlOn = reader.getEmergenceMoistureControlOn();
  pc_FrostKillOn = reader.getFrostKillOn();

  p_UseAutomaticIrrigation = reader.getUseAutomaticIrrigation();
  p_AutoIrrigationParams.deserialize(reader.getAutoIrrigationParams());

  p_UseNMinMineralFertilisingMethod = reader.getUseNMinMineralFertilisingMethod();
  p_NMinFertiliserPartition.deserialize(reader.getNMinFertiliserPartition());
  p_NMinUserParams.deserialize(reader.getNMinApplicationParams());

  p_UseSecondaryYields = reader.getUseSecondaryYields();
  p_UseAutomaticHarvestTrigger = reader.getUseAutomaticHarvestTrigger();

  p_NumberOfLayers = reader.getNumberOfLayers();
  p_LayerThickness = reader.getLayerThickness();

  p_StartPVIndex = reader.getStartPVIndex();
  p_JulianDayAutomaticFertilising = reader.getJulianDayAutomaticFertilising();
}

void SimulationParameters::serialize(mas::schema::model::monica::SimulationParameters::Builder builder) const {
  startDate.serialize(builder.initStartDate());
  endDate.serialize(builder.initEndDate());

  builder.setNitrogenResponseOn(pc_NitrogenResponseOn);
  builder.setWaterDeficitResponseOn(pc_WaterDeficitResponseOn);
  builder.setEmergenceFloodingControlOn(pc_EmergenceFloodingControlOn);
  builder.setEmergenceMoistureControlOn(pc_EmergenceMoistureControlOn);
  builder.setFrostKillOn(pc_FrostKillOn);

  builder.setUseAutomaticIrrigation(p_UseAutomaticIrrigation);
  p_AutoIrrigationParams.serialize(builder.initAutoIrrigationParams());

  builder.setUseNMinMineralFertilisingMethod(p_UseNMinMineralFertilisingMethod);
  p_NMinFertiliserPartition.serialize(builder.initNMinFertiliserPartition());
  p_NMinUserParams.serialize(builder.initNMinApplicationParams());

  builder.setUseSecondaryYields(p_UseSecondaryYields);
  builder.setUseAutomaticHarvestTrigger(p_UseAutomaticHarvestTrigger);

  builder.setNumberOfLayers(p_NumberOfLayers);
  builder.setLayerThickness(p_LayerThickness);

  builder.setStartPVIndex(p_StartPVIndex);
  builder.setJulianDayAutomaticFertilising(p_JulianDayAutomaticFertilising);
}

// SimulationParameters::SimulationParameters(json11::Json j) {
//   merge(j);
// }

Errors SimulationParameters::merge(json11::Json j) {
  Errors res = Json11Serializable::merge(j);

  set_iso_date_value(startDate, j, "startDate");
  set_iso_date_value(endDate, j, "endDate");

  set_bool_value(pc_NitrogenResponseOn, j, "NitrogenResponseOn");
  set_bool_value(pc_WaterDeficitResponseOn, j, "WaterDeficitResponseOn");
  set_bool_value(pc_EmergenceFloodingControlOn, j, "EmergenceFloodingControlOn");
  set_bool_value(pc_EmergenceMoistureControlOn, j, "EmergenceMoistureControlOn");
  set_bool_value(pc_FrostKillOn, j, "FrostKillOn");

  set_bool_value(p_UseAutomaticIrrigation, j, "UseAutomaticIrrigation");
  p_AutoIrrigationParams.merge(j["AutoIrrigationParams"]);

  set_bool_value(p_UseNMinMineralFertilisingMethod, j, "UseNMinMineralFertilisingMethod");
  p_NMinFertiliserPartition.merge(j["NMinFertiliserPartition"]);
  p_NMinUserParams.merge(j["NMinUserParams"]);
  set_int_value(p_JulianDayAutomaticFertilising, j, "JulianDayAutomaticFertilising");

  set_bool_value(p_UseSecondaryYields, j, "UseSecondaryYields");
  set_bool_value(p_UseAutomaticHarvestTrigger, j, "UseAutomaticHarvestTrigger");
  set_int_value(p_NumberOfLayers, j, "NumberOfLayers");
  set_double_value(p_LayerThickness, j, "LayerThickness");

  set_int_value(p_StartPVIndex, j, "StartPVIndex");

  if (auto serState = j["serializedMonicaState"].object_items(); !serState.empty()) {
    if (const auto loadState = serState["load"]; loadState.is_object()) {
      set_bool_value(loadSerializedMonicaStateAtStart, loadState, "atStart");
      set_bool_value(deserializedMonicaStateFromJson, loadState, "fromJson");
      set_string_value(pathToLoadSerializationFile, loadState, "path");
    }
    if (const auto saveState = serState["save"]; saveState.is_object()) {
      set_bool_value(serializeMonicaStateAtEnd, saveState, "atEnd");
      set_bool_value(serializeMonicaStateAtEndToJson, saveState, "toJson");
      set_string_value(pathToSerializationAtEndFile, saveState, "path");
      noOfPreviousDaysSerializedClimateData = max(0, int_value(saveState, "noOfPreviousDaysSerializedClimateData"));
    }
  }

  return res;
}

json11::Json SimulationParameters::to_json() const {
  return json11::Json::object
      {{"type",                                  "SimulationParameters"},
       {"startDate",                             startDate.toIsoDateString()},
       {"endDate",                               endDate.toIsoDateString()},
       {"NitrogenResponseOn",                    pc_NitrogenResponseOn},
       {"WaterDeficitResponseOn",                pc_WaterDeficitResponseOn},
       {"EmergenceFloodingControlOn",            pc_EmergenceFloodingControlOn},
       {"EmergenceMoistureControlOn",            pc_EmergenceMoistureControlOn},
       {"FrostKillOn",                           pc_FrostKillOn},
       {"UseAutomaticIrrigation",                p_UseAutomaticIrrigation},
       {"AutoIrrigationParams",                  p_AutoIrrigationParams},
       {"UseNMinMineralFertilisingMethod",       p_UseNMinMineralFertilisingMethod},
       {"NMinFertiliserPartition",               p_NMinFertiliserPartition},
       {"NMinUserParams",                        p_NMinUserParams},
       {"JulianDayAutomaticFertilising",         p_JulianDayAutomaticFertilising},
       {"UseSecondaryYields",                    p_UseSecondaryYields},
       {"UseAutomaticHarvestTrigger",            p_UseAutomaticHarvestTrigger},
       {"NumberOfLayers",                        p_NumberOfLayers},
       {"LayerThickness",                        p_LayerThickness},
       {"StartPVIndex",                          p_StartPVIndex},
       {"serializeMonicaStateAtEnd",             serializeMonicaStateAtEnd},
       {"serializedMonicaState",                  Json::object {
           {
             "load", Json::object {
             { "atStart", loadSerializedMonicaStateAtStart },
             { "fromJson", deserializedMonicaStateFromJson },
             { "path", pathToLoadSerializationFile } }
           },
           {
             "save", Json::object {
             { "atEnd", serializeMonicaStateAtEnd },
             { "toJson", serializeMonicaStateAtEndToJson },
             { "path", pathToSerializationAtEndFile },
             { "noOfPreviousDaysSerializedClimateData", int(noOfPreviousDaysSerializedClimateData) } }
           }}
      }};
}

void CropModuleParameters::deserialize(mas::schema::model::monica::CropModuleParameters::Reader reader) {
  pc_CanopyReflectionCoefficient = reader.getCanopyReflectionCoefficient();
  pc_ReferenceMaxAssimilationRate = reader.getReferenceMaxAssimilationRate();
  pc_ReferenceLeafAreaIndex = reader.getReferenceLeafAreaIndex();
  pc_MaintenanceRespirationParameter1 = reader.getMaintenanceRespirationParameter1();
  pc_MaintenanceRespirationParameter2 = reader.getMaintenanceRespirationParameter2();
  pc_MinimumNConcentrationRoot = reader.getMinimumNConcentrationRoot();
  pc_MinimumAvailableN = reader.getMinimumAvailableN();
  pc_ReferenceAlbedo = reader.getReferenceAlbedo();
  pc_StomataConductanceAlpha = reader.getStomataConductanceAlpha();
  pc_SaturationBeta = reader.getSaturationBeta();
  pc_GrowthRespirationRedux = reader.getGrowthRespirationRedux();
  pc_MaxCropNDemand = reader.getMaxCropNDemand();
  pc_GrowthRespirationParameter1 = reader.getGrowthRespirationParameter1();
  pc_GrowthRespirationParameter2 = reader.getGrowthRespirationParameter2();
  pc_Tortuosity = reader.getTortuosity();
  pc_AdjustRootDepthForSoilProps = reader.getAdjustRootDepthForSoilProps();

  __enable_Phenology_WangEngelTemperatureResponse__ = reader.getExperimentalEnablePhenologyWangEngelTemperatureResponse();
  __enable_Photosynthesis_WangEngelTemperatureResponse__ = reader.getExperimentalEnablePhotosynthesisWangEngelTemperatureResponse();
  __enable_hourly_FvCB_photosynthesis__ = reader.getExperimentalEnableHourlyFvCBPhotosynthesis();
  __enable_T_response_leaf_expansion__ = reader.getExperimentalEnableTResponseLeafExpansion();
  __disable_daily_root_biomass_to_soil__ = reader.getExperimentalDisableDailyRootBiomassToSoil();
  __enable_vernalisation_factor_fix__ = reader.getEnableVernalisationFactorFix();
}

void CropModuleParameters::serialize(mas::schema::model::monica::CropModuleParameters::Builder builder) const {
  builder.setCanopyReflectionCoefficient(pc_CanopyReflectionCoefficient);
  builder.setReferenceMaxAssimilationRate(pc_ReferenceMaxAssimilationRate);
  builder.setReferenceLeafAreaIndex(pc_ReferenceLeafAreaIndex);
  builder.setMaintenanceRespirationParameter1(pc_MaintenanceRespirationParameter1);
  builder.setMaintenanceRespirationParameter2(pc_MaintenanceRespirationParameter2);
  builder.setMinimumNConcentrationRoot(pc_MinimumNConcentrationRoot);
  builder.setMinimumAvailableN(pc_MinimumAvailableN);
  builder.setReferenceAlbedo(pc_ReferenceAlbedo);
  builder.setStomataConductanceAlpha(pc_StomataConductanceAlpha);
  builder.setSaturationBeta(pc_SaturationBeta);
  builder.setGrowthRespirationRedux(pc_GrowthRespirationRedux);
  builder.setMaxCropNDemand(pc_MaxCropNDemand);
  builder.setGrowthRespirationParameter1(pc_GrowthRespirationParameter1);
  builder.setGrowthRespirationParameter2(pc_GrowthRespirationParameter2);
  builder.setTortuosity(pc_Tortuosity);
  builder.setAdjustRootDepthForSoilProps(pc_AdjustRootDepthForSoilProps);

  builder.setExperimentalEnablePhenologyWangEngelTemperatureResponse(__enable_Phenology_WangEngelTemperatureResponse__);
  builder.setExperimentalEnablePhotosynthesisWangEngelTemperatureResponse(
      __enable_Photosynthesis_WangEngelTemperatureResponse__);
  builder.setExperimentalEnableHourlyFvCBPhotosynthesis(__enable_hourly_FvCB_photosynthesis__);
  builder.setExperimentalEnableTResponseLeafExpansion(__enable_T_response_leaf_expansion__);
  builder.setExperimentalDisableDailyRootBiomassToSoil(__disable_daily_root_biomass_to_soil__);
  builder.setEnableVernalisationFactorFix(__enable_vernalisation_factor_fix__);
}

//CropModuleParameters::CropModuleParameters(json11::Json j) {
//  merge(j);
//}

Errors CropModuleParameters::merge(json11::Json j) {
  Errors res = Json11Serializable::merge(j);

  set_double_value(pc_CanopyReflectionCoefficient, j, "CanopyReflectionCoefficient");
  set_double_value(pc_ReferenceMaxAssimilationRate, j, "ReferenceMaxAssimilationRate");
  set_double_value(pc_ReferenceLeafAreaIndex, j, "ReferenceLeafAreaIndex");
  set_double_value(pc_MaintenanceRespirationParameter1, j, "MaintenanceRespirationParameter1");
  set_double_value(pc_MaintenanceRespirationParameter2, j, "MaintenanceRespirationParameter2");
  set_double_value(pc_MinimumNConcentrationRoot, j, "MinimumNConcentrationRoot");
  set_double_value(pc_MinimumAvailableN, j, "MinimumAvailableN");
  set_double_value(pc_ReferenceAlbedo, j, "ReferenceAlbedo");
  set_double_value(pc_StomataConductanceAlpha, j, "StomataConductanceAlpha");
  set_double_value(pc_SaturationBeta, j, "SaturationBeta");
  set_double_value(pc_GrowthRespirationRedux, j, "GrowthRespirationRedux");
  set_double_value(pc_MaxCropNDemand, j, "MaxCropNDemand");
  set_double_value(pc_GrowthRespirationParameter1, j, "GrowthRespirationParameter1");
  set_double_value(pc_GrowthRespirationParameter2, j, "GrowthRespirationParameter2");
  set_double_value(pc_Tortuosity, j, "Tortuosity");
  set_bool_value(pc_AdjustRootDepthForSoilProps, j, "AdjustRootDepthForSoilProps");
  if (j["TimeUnderAnoxiaThreshold"].is_number()) {
    std::fill(pc_TimeUnderAnoxiaThreshold.begin(), pc_TimeUnderAnoxiaThreshold.end(),
              int(j["TimeUnderAnoxiaThreshold"].number_value()));
  } else if(j["TimeUnderAnoxiaThreshold"].is_array()) {
    set_int_vector(pc_TimeUnderAnoxiaThreshold, j, "TimeUnderAnoxiaThreshold");
  }

  set_bool_value(__enable_Photosynthesis_WangEngelTemperatureResponse__, j,
                 "__enable_Photosynthesis_WangEngelTemperatureResponse__");
  set_bool_value(__enable_Phenology_WangEngelTemperatureResponse__, j,
                 "__enable_Phenology_WangEngelTemperatureResponse__");
  set_bool_value(__enable_hourly_FvCB_photosynthesis__, j, "__enable_hourly_FvCB_photosynthesis__");
  set_bool_value(__enable_T_response_leaf_expansion__, j, "__enable_T_response_leaf_expansion__");
  set_bool_value(__disable_daily_root_biomass_to_soil__, j, "__disable_daily_root_biomass_to_soil__");
  set_bool_value(__enable_vernalisation_factor_fix__, j, "__enable_vernalisation_factor_fix__");

  set_bool_value(isIntercropping, j["intercropping"], "is_intercropping");
  set_bool_value(sequentialWaterUse, j["intercropping"], "sequential_water_use");
  set_bool_value(twoWaySync, j["intercropping"], "two_way_sync");
  set_double_value(pc_intercropping_k_s, j["intercropping"], "k_s");
  set_double_value(pc_intercropping_k_t, j["intercropping"], "k_t");
  set_double_value(pc_intercropping_phRedux, j["intercropping"], "PHredux");
  set_double_value(pc_intercropping_dvs_phr, j["intercropping"], "DVS_PHr");
  set_bool_value(pc_intercropping_autoPhRedux, j["intercropping"], "auto_PHredux");
  set_string_value(pc_intercropping_reader_sr, j["intercropping"], "reader_sr");
  set_string_value(pc_intercropping_writer_sr, j["intercropping"], "writer_sr");
  return res;
}

json11::Json CropModuleParameters::to_json() const {
  return json11::Json::object
      {{"type",                                                   "CropModuleParameters"},
       {"CanopyReflectionCoefficient",                            pc_CanopyReflectionCoefficient},
       {"ReferenceMaxAssimilationRate",                           pc_ReferenceMaxAssimilationRate},
       {"ReferenceLeafAreaIndex",                                 pc_ReferenceLeafAreaIndex},
       {"MaintenanceRespirationParameter1",                       pc_MaintenanceRespirationParameter1},
       {"MaintenanceRespirationParameter2",                       pc_MaintenanceRespirationParameter2},
       {"MinimumNConcentrationRoot",                              pc_MinimumNConcentrationRoot},
       {"MinimumAvailableN",                                      pc_MinimumAvailableN},
       {"ReferenceAlbedo",                                        pc_ReferenceAlbedo},
       {"StomataConductanceAlpha",                                pc_StomataConductanceAlpha},
       {"SaturationBeta",                                         pc_SaturationBeta},
       {"GrowthRespirationRedux",                                 pc_GrowthRespirationRedux},
       {"MaxCropNDemand",                                         pc_MaxCropNDemand},
       {"GrowthRespirationParameter1",                            pc_GrowthRespirationParameter1},
       {"GrowthRespirationParameter2",                            pc_GrowthRespirationParameter2},
       {"Tortuosity",                                             pc_Tortuosity},
       {"AdjustRootDepthForSoilProps",                            pc_AdjustRootDepthForSoilProps},
       {"TimeUnderAnoxiaThreshold", pc_TimeUnderAnoxiaThreshold},
       {"__enable_Phenology_WangEngelTemperatureResponse__",      __enable_Phenology_WangEngelTemperatureResponse__},
       {"__enable_Photosynthesis_WangEngelTemperatureResponse__", __enable_Photosynthesis_WangEngelTemperatureResponse__},
       {"__enable_hourly_FvCB_photosynthesis__",                  __enable_hourly_FvCB_photosynthesis__},
       {"__enable_T_response_leaf_expansion__",                   __enable_T_response_leaf_expansion__},
       {"__disable_daily_root_biomass_to_soil__",                 __disable_daily_root_biomass_to_soil__},
       {"__enable_vernalisation_factor_fix__",                    __enable_vernalisation_factor_fix__}
      };
}

void EnvironmentParameters::deserialize(mas::schema::model::monica::EnvironmentParameters::Reader reader) {
  p_Albedo = reader.getAlbedo();
  p_AtmosphericCO2 = reader.getAtmosphericCO2();

  p_AtmosphericCO2s.clear();
  for (auto co2: reader.getAtmosphericCO2s()) p_AtmosphericCO2s[co2.getYear()] = co2.getValue();

  p_AtmosphericO3s.clear();
  for (auto o3: reader.getAtmosphericO3s()) p_AtmosphericO3s[o3.getYear()] = o3.getValue();

  p_WindSpeedHeight = reader.getWindSpeedHeight();
  p_LeachingDepth = reader.getLeachingDepth();
  p_timeStep = reader.getTimeStep();

  p_MaxGroundwaterDepth = reader.getMaxGroundwaterDepth();
  p_MinGroundwaterDepth = reader.getMinGroundwaterDepth();
  p_MinGroundwaterDepthMonth = reader.getMinGroundwaterDepthMonth();

  rcp = reader.getRcp();
}

void EnvironmentParameters::serialize(mas::schema::model::monica::EnvironmentParameters::Builder builder) const {
  builder.setAlbedo(p_Albedo);
  builder.setAtmosphericCO2(p_AtmosphericCO2);

  {
    auto co2s = builder.initAtmosphericCO2s((capnp::uint) p_AtmosphericCO2s.size());
    capnp::uint i = 0;
    for (auto p: p_AtmosphericCO2s) {
      co2s[i].setYear(p.first);
      co2s[i].setValue(p.second);
    }
  }
  builder.setAtmosphericO3(p_AtmosphericO3);
  {
    auto o3s = builder.initAtmosphericO3s((capnp::uint) p_AtmosphericO3s.size());
    capnp::uint i = 0;
    for (auto p: p_AtmosphericO3s) {
      o3s[i].setYear(p.first);
      o3s[i].setValue(p.second);
    }
  }
  builder.setWindSpeedHeight(p_WindSpeedHeight);
  builder.setLeachingDepth(p_LeachingDepth);
  builder.setTimeStep(p_timeStep);

  builder.setMaxGroundwaterDepth(p_MaxGroundwaterDepth);
  builder.setMinGroundwaterDepth(p_MinGroundwaterDepth);
  builder.setMinGroundwaterDepthMonth(p_MinGroundwaterDepthMonth);

  builder.setRcp(rcp);
}

//EnvironmentParameters::EnvironmentParameters(json11::Json j) {
//  merge(j);
//}

Errors EnvironmentParameters::merge(json11::Json j) {
  Errors res = Json11Serializable::merge(j);

  using namespace mas::schema::climate;
  auto str2rcp = [](string str) {
    switch (stoi(str.substr(3, 2))) {
      case 19:
        return RCP::RCP19;
        break;
      case 26:
        return RCP::RCP26;
        break;
      case 34:
        return RCP::RCP34;
        break;
      case 45:
        return RCP::RCP45;
        break;
      case 60:
        return RCP::RCP60;
        break;
      case 70:
        return RCP::RCP70;
        break;
      case 85:
        return RCP::RCP85;
        break;
    }
    return RCP::RCP85;
  };

  set_double_value(p_Albedo, j, "Albedo");

  if (j["rcp"].is_string()) rcp = str2rcp(j["rcp"].string_value());

  set_double_value(p_AtmosphericCO2, j, "AtmosphericCO2");
  if (j["AtmosphericCO2s"].is_object()) {
    p_AtmosphericCO2s.clear();
    for (auto p: j["AtmosphericCO2s"].object_items())
      p_AtmosphericCO2s[stoi(p.first)] = p.second.number_value();
  }
  set_double_value(p_AtmosphericO3, j, "AtmosphericO3");
  if (j["AtmosphericO3s"].is_object()) {
    p_AtmosphericO3s.clear();
    for (auto p: j["AtmosphericO3s"].object_items())
      p_AtmosphericO3s[stoi(p.first)] = p.second.number_value();
  }
  set_double_value(p_WindSpeedHeight, j, "WindSpeedHeight");
  set_double_value(p_LeachingDepth, j, "LeachingDepth");
  set_double_value(p_timeStep, j, "timeStep");
  set_double_value(p_MaxGroundwaterDepth, j, "MaxGroundwaterDepth");
  set_double_value(p_MinGroundwaterDepth, j, "MinGroundwaterDepth");
  set_int_value(p_MinGroundwaterDepthMonth, j, "MinGroundwaterDepthMonth");

  return res;
}

json11::Json EnvironmentParameters::to_json() const {
  json11::Json::object co2s;
  for (auto p: p_AtmosphericCO2s)
    co2s[to_string(p.first)] = p.second;

  json11::Json::object o3s;
  for (auto p: p_AtmosphericO3s)
    o3s[to_string(p.first)] = p.second;

  auto rcp2str = [](auto rcp) {
    using namespace mas::schema::climate;
    switch (rcp) {
      case RCP::RCP19:
        return "rcp19";
        break;
      case RCP::RCP26:
        return "rcp26";
        break;
      case RCP::RCP34:
        return "rcp34";
        break;
      case RCP::RCP45:
        return "rcp45";
        break;
      case RCP::RCP60:
        return "rcp60";
        break;
      case RCP::RCP70:
        return "rcp70";
        break;
      case RCP::RCP85:
        return "rcp85";
        break;
    }
    return "rcp85";
  };

  return json11::Json::object
      {{"type",                     "EnvironmentParameters"},
       {"Albedo",                   p_Albedo},
       {"rcp",                      rcp2str(rcp)},
       {"AtmosphericCO2",           p_AtmosphericCO2},
       {"AtmosphericCO2s",          co2s},
       {"AtmosphericO3",            p_AtmosphericO3},
       {"AtmosphericO3s",           o3s},
       {"WindSpeedHeight",          p_WindSpeedHeight},
       {"LeachingDepth",            p_LeachingDepth},
       {"timeStep",                 p_timeStep},
       {"MaxGroundwaterDepth",      p_MaxGroundwaterDepth},
       {"MinGroundwaterDepth",      p_MinGroundwaterDepth},
       {"MinGroundwaterDepthMonth", p_MinGroundwaterDepthMonth}
      };
}

SoilMoistureModuleParameters::SoilMoistureModuleParameters() {
  getCapillaryRiseRate = [](string soilTexture, size_t distance) { return 0.0; };
}

void
SoilMoistureModuleParameters::deserialize(mas::schema::model::monica::SoilMoistureModuleParameters::Reader reader) {
  //pm_CriticalMoistureDepth = reader.getCriticalMoistureDepth();
  pm_SaturatedHydraulicConductivity = reader.getSaturatedHydraulicConductivity();
  pm_SurfaceRoughness = reader.getSurfaceRoughness();
  pm_GroundwaterDischarge = reader.getGroundwaterDischarge();
  pm_HydraulicConductivityRedux = reader.getHydraulicConductivityRedux();
  pm_SnowAccumulationTresholdTemperature = reader.getSnowAccumulationTresholdTemperature();
  pm_KcFactor = reader.getKcFactor();
  pm_TemperatureLimitForLiquidWater = reader.getTemperatureLimitForLiquidWater();
  pm_CorrectionSnow = reader.getCorrectionSnow();
  pm_CorrectionRain = reader.getCorrectionRain();
  pm_SnowMaxAdditionalDensity = reader.getSnowMaxAdditionalDensity();
  pm_NewSnowDensityMin = reader.getNewSnowDensityMin();
  pm_SnowRetentionCapacityMin = reader.getSnowRetentionCapacityMin();
  pm_RefreezeParameter1 = reader.getRefreezeParameter1();
  pm_RefreezeParameter2 = reader.getRefreezeParameter2();
  pm_RefreezeTemperature = reader.getRefreezeTemperature();
  pm_SnowMeltTemperature = reader.getSnowMeltTemperature();
  pm_SnowPacking = reader.getSnowPacking();
  pm_SnowRetentionCapacityMax = reader.getSnowRetentionCapacityMax();
  pm_EvaporationZeta = reader.getEvaporationZeta();
  pm_XSACriticalSoilMoisture = reader.getXsaCriticalSoilMoisture();
  pm_MaximumEvaporationImpactDepth = reader.getMaximumEvaporationImpactDepth();
  pm_MaxPercolationRate = reader.getMaxPercolationRate();
  pm_MoistureInitValue = reader.getMoistureInitValue();
}

void SoilMoistureModuleParameters::serialize(
    mas::schema::model::monica::SoilMoistureModuleParameters::Builder builder) const {
  //builder.setCriticalMoistureDepth(pm_CriticalMoistureDepth);
  builder.setSaturatedHydraulicConductivity(pm_SaturatedHydraulicConductivity);
  builder.setSurfaceRoughness(pm_SurfaceRoughness);
  builder.setGroundwaterDischarge(pm_GroundwaterDischarge);
  builder.setHydraulicConductivityRedux(pm_HydraulicConductivityRedux);
  builder.setSnowAccumulationTresholdTemperature(pm_SnowAccumulationTresholdTemperature);
  builder.setKcFactor(pm_KcFactor);
  builder.setTemperatureLimitForLiquidWater(pm_TemperatureLimitForLiquidWater);
  builder.setCorrectionSnow(pm_CorrectionSnow);
  builder.setCorrectionRain(pm_CorrectionRain);
  builder.setSnowMaxAdditionalDensity(pm_SnowMaxAdditionalDensity);
  builder.setNewSnowDensityMin(pm_NewSnowDensityMin);
  builder.setSnowRetentionCapacityMin(pm_SnowRetentionCapacityMin);
  builder.setRefreezeParameter1(pm_RefreezeParameter1);
  builder.setRefreezeParameter2(pm_RefreezeParameter2);
  builder.setRefreezeTemperature(pm_RefreezeTemperature);
  builder.setSnowMeltTemperature(pm_SnowMeltTemperature);
  builder.setSnowPacking(pm_SnowPacking);
  builder.setSnowRetentionCapacityMax(pm_SnowRetentionCapacityMax);
  builder.setEvaporationZeta(pm_EvaporationZeta);
  builder.setXsaCriticalSoilMoisture(pm_XSACriticalSoilMoisture);
  builder.setMaximumEvaporationImpactDepth(pm_MaximumEvaporationImpactDepth);
  builder.setMaxPercolationRate(pm_MaxPercolationRate);
  builder.setMoistureInitValue(pm_MoistureInitValue);
}

// SoilMoistureModuleParameters::SoilMoistureModuleParameters(json11::Json j)
//     : SoilMoistureModuleParameters() {
//   merge(j);
// }

Errors SoilMoistureModuleParameters::merge(json11::Json j) {
  Errors res = Json11Serializable::merge(j);

  //set_double_value(pm_CriticalMoistureDepth, j, "CriticalMoistureDepth");
  set_double_value(pm_SaturatedHydraulicConductivity, j, "SaturatedHydraulicConductivity");
  set_double_value(pm_SurfaceRoughness, j, "SurfaceRoughness");
  set_double_value(pm_GroundwaterDischarge, j, "GroundwaterDischarge");
  set_double_value(pm_HydraulicConductivityRedux, j, "HydraulicConductivityRedux");
  set_double_value(pm_SnowAccumulationTresholdTemperature, j, "SnowAccumulationTresholdTemperature");
  set_double_value(pm_KcFactor, j, "KcFactor");
  set_double_value(pm_TemperatureLimitForLiquidWater, j, "TemperatureLimitForLiquidWater");
  set_double_value(pm_CorrectionSnow, j, "CorrectionSnow");
  set_double_value(pm_CorrectionRain, j, "CorrectionRain");
  set_double_value(pm_SnowMaxAdditionalDensity, j, "SnowMaxAdditionalDensity");
  set_double_value(pm_NewSnowDensityMin, j, "NewSnowDensityMin");
  set_double_value(pm_SnowRetentionCapacityMin, j, "SnowRetentionCapacityMin");
  set_double_value(pm_RefreezeParameter1, j, "RefreezeParameter1");
  set_double_value(pm_RefreezeParameter2, j, "RefreezeParameter2");
  set_double_value(pm_RefreezeTemperature, j, "RefreezeTemperature");
  set_double_value(pm_SnowMeltTemperature, j, "SnowMeltTemperature");
  set_double_value(pm_SnowPacking, j, "SnowPacking");
  set_double_value(pm_SnowRetentionCapacityMax, j, "SnowRetentionCapacityMax");
  set_double_value(pm_EvaporationZeta, j, "EvaporationZeta");
  set_double_value(pm_XSACriticalSoilMoisture, j, "XSACriticalSoilMoisture");
  set_double_value(pm_MaximumEvaporationImpactDepth, j, "MaximumEvaporationImpactDepth");
  set_double_value(pm_MaxPercolationRate, j, "MaxPercolationRate");
  set_double_value(pm_MoistureInitValue, j, "MoistureInitValue");

  return res;
}

json11::Json SoilMoistureModuleParameters::to_json() const {
  return json11::Json::object
      {{"type",                                "SoilMoistureModuleParameters"},
       //{"CriticalMoistureDepth",               pm_CriticalMoistureDepth},
       {"SaturatedHydraulicConductivity",      pm_SaturatedHydraulicConductivity},
       {"SurfaceRoughness",                    pm_SurfaceRoughness},
       {"GroundwaterDischarge",                pm_GroundwaterDischarge},
       {"HydraulicConductivityRedux",          pm_HydraulicConductivityRedux},
       {"SnowAccumulationTresholdTemperature", pm_SnowAccumulationTresholdTemperature},
       {"KcFactor",                            pm_KcFactor},
       {"TemperatureLimitForLiquidWater",      pm_TemperatureLimitForLiquidWater},
       {"CorrectionSnow",                      pm_CorrectionSnow},
       {"CorrectionRain",                      pm_CorrectionRain},
       {"SnowMaxAdditionalDensity",            pm_SnowMaxAdditionalDensity},
       {"NewSnowDensityMin",                   pm_NewSnowDensityMin},
       {"SnowRetentionCapacityMin",            pm_SnowRetentionCapacityMin},
       {"RefreezeParameter1",                  pm_RefreezeParameter1},
       {"RefreezeParameter2",                  pm_RefreezeParameter2},
       {"RefreezeTemperature",                 pm_RefreezeTemperature},
       {"SnowMeltTemperature",                 pm_SnowMeltTemperature},
       {"SnowPacking",                         pm_SnowPacking},
       {"SnowRetentionCapacityMax",            pm_SnowRetentionCapacityMax},
       {"EvaporationZeta",                     pm_EvaporationZeta},
       {"XSACriticalSoilMoisture",             pm_XSACriticalSoilMoisture},
       {"MaximumEvaporationImpactDepth",       pm_MaximumEvaporationImpactDepth},
       {"MaxPercolationRate",                  pm_MaxPercolationRate},
       {"MoistureInitValue",                   pm_MoistureInitValue}
      };
}

void SoilTemperatureModuleParameters::deserialize(
    mas::schema::model::monica::SoilTemperatureModuleParameters::Reader reader) {
  pt_NTau = reader.getNTau();
  pt_InitialSurfaceTemperature = reader.getInitialSurfaceTemperature();
  pt_QuartzRawDensity = reader.getQuartzRawDensity();
  pt_DensityAir = reader.getDensityAir();
  pt_DensityWater = reader.getDensityWater();
  pt_DensityHumus = reader.getDensityHumus();
  pt_SpecificHeatCapacityAir = reader.getSpecificHeatCapacityAir();
  pt_SpecificHeatCapacityQuartz = reader.getSpecificHeatCapacityQuartz();
  pt_SpecificHeatCapacityWater = reader.getSpecificHeatCapacityWater();
  pt_SpecificHeatCapacityHumus = reader.getSpecificHeatCapacityHumus();
  pt_SoilAlbedo = reader.getSoilAlbedo();
  pt_SoilMoisture = reader.getSoilMoisture();
}

void SoilTemperatureModuleParameters::serialize(
    mas::schema::model::monica::SoilTemperatureModuleParameters::Builder builder) const {
  builder.setNTau(pt_NTau);
  builder.setInitialSurfaceTemperature(pt_InitialSurfaceTemperature);
  builder.setBaseTemperature(pt_BaseTemperature);
  builder.setQuartzRawDensity(pt_QuartzRawDensity);
  builder.setDensityAir(pt_DensityAir);
  builder.setDensityWater(pt_DensityWater);
  builder.setDensityHumus(pt_DensityHumus);
  builder.setSpecificHeatCapacityAir(pt_SpecificHeatCapacityAir);
  builder.setSpecificHeatCapacityQuartz(pt_SpecificHeatCapacityQuartz);
  builder.setSpecificHeatCapacityWater(pt_SpecificHeatCapacityWater);
  builder.setSpecificHeatCapacityHumus(pt_SpecificHeatCapacityHumus);
  builder.setSoilAlbedo(pt_SoilAlbedo);
  builder.setSoilMoisture(pt_SoilMoisture);
}

// SoilTemperatureModuleParameters::SoilTemperatureModuleParameters(json11::Json j) {
//   merge(j);
// }

Errors SoilTemperatureModuleParameters::merge(json11::Json j) {
  Errors res = Json11Serializable::merge(j);

  set_double_value(pt_NTau, j, "NTau");
  set_double_value(pt_InitialSurfaceTemperature, j, "InitialSurfaceTemperature");
  set_double_value(pt_BaseTemperature, j, "BaseTemperature");
  set_double_value(pt_QuartzRawDensity, j, "QuartzRawDensity");
  set_double_value(pt_DensityAir, j, "DensityAir");
  set_double_value(pt_DensityWater, j, "DensityWater");
  set_double_value(pt_DensityHumus, j, "DensityHumus");
  set_double_value(pt_SpecificHeatCapacityAir, j, "SpecificHeatCapacityAir");
  set_double_value(pt_SpecificHeatCapacityQuartz, j, "SpecificHeatCapacityQuartz");
  set_double_value(pt_SpecificHeatCapacityWater, j, "SpecificHeatCapacityWater");
  set_double_value(pt_SpecificHeatCapacityHumus, j, "SpecificHeatCapacityHumus");
  set_double_value(pt_SoilAlbedo, j, "SoilAlbedo");
  set_double_value(pt_SoilMoisture, j, "SoilMoisture");

  return res;
}

json11::Json SoilTemperatureModuleParameters::to_json() const {
  return json11::Json::object
      {{"type",                       "SoilTemperatureModuleParameters"},
       {"NTau",                       pt_NTau},
       {"InitialSurfaceTemperature",  pt_InitialSurfaceTemperature},
       {"BaseTemperature",            pt_BaseTemperature},
       {"QuartzRawDensity",           pt_QuartzRawDensity},
       {"DensityAir",                 pt_DensityAir},
       {"DensityWater",               pt_DensityWater},
       {"DensityHumus",               pt_DensityHumus},
       {"SpecificHeatCapacityAir",    pt_SpecificHeatCapacityAir},
       {"SpecificHeatCapacityQuartz", pt_SpecificHeatCapacityQuartz},
       {"SpecificHeatCapacityWater",  pt_SpecificHeatCapacityWater},
       {"SpecificHeatCapacityHumus",  pt_SpecificHeatCapacityHumus},
       {"SoilAlbedo",                 pt_SoilAlbedo},
       {"SoilMoisture",               pt_SoilMoisture}
      };
}

void
SoilTransportModuleParameters::deserialize(mas::schema::model::monica::SoilTransportModuleParameters::Reader reader) {
  pq_DispersionLength = reader.getDispersionLength();
  pq_AD = reader.getAd();
  pq_DiffusionCoefficientStandard = reader.getDiffusionCoefficientStandard();
  pq_NDeposition = reader.getNDeposition();
}

void SoilTransportModuleParameters::serialize(
    mas::schema::model::monica::SoilTransportModuleParameters::Builder builder) const {
  builder.setDispersionLength(pq_DispersionLength);
  builder.setAd(pq_AD);
  builder.setDiffusionCoefficientStandard(pq_DiffusionCoefficientStandard);
  builder.setNDeposition(pq_NDeposition);
}

// SoilTransportModuleParameters::SoilTransportModuleParameters(json11::Json j) {
//   merge(j);
// }

Errors SoilTransportModuleParameters::merge(json11::Json j) {
  Errors res = Json11Serializable::merge(j);

  set_double_value(pq_DispersionLength, j, "DispersionLength");
  set_double_value(pq_AD, j, "AD");
  set_double_value(pq_DiffusionCoefficientStandard, j, "DiffusionCoefficientStandard");
  set_double_value(pq_NDeposition, j, "NDeposition");

  return res;
}

json11::Json SoilTransportModuleParameters::to_json() const {
  return json11::Json::object
      {{"type",                         "SoilTransportModuleParameters"},
       {"DispersionLength",             pq_DispersionLength},
       {"AD",                           pq_AD},
       {"DiffusionCoefficientStandard", pq_DiffusionCoefficientStandard},
       {"NDeposition",                  pq_NDeposition}
      };
}

void SticsParameters::deserialize(mas::schema::model::monica::SticsParameters::Reader reader) {
  use_n2o = reader.getUseN2O();
  use_nit = reader.getUseNit();
  use_denit = reader.getUseDenit();
  code_vnit = reader.getCodeVnit();
  code_tnit = reader.getCodeTnit();
  code_rationit = reader.getCodeRationit();
  code_hourly_wfps_nit = reader.getCodeHourlyWfpsNit();
  code_pdenit = reader.getCodePdenit();
  code_ratiodenit = reader.getCodeRatiodenit();
  code_hourly_wfps_denit = reader.getCodeHourlyWfpsDenit();
  hminn = reader.getHminn();
  hoptn = reader.getHoptn();
  pHminnit = reader.getPHminnit();
  pHmaxnit = reader.getPHmaxnit();
  nh4_min = reader.getNh4Min();
  pHminden = reader.getPHminden();
  pHmaxden = reader.getPHmaxden();
  wfpsc = reader.getWfpsc();
  tdenitopt_gauss = reader.getTdenitoptGauss();
  scale_tdenitopt = reader.getScaleTdenitopt();
  Kd = reader.getKd();
  k_desat = reader.getKDesat();
  fnx = reader.getFnx();
  vnitmax = reader.getVnitmax();
  Kamm = reader.getKamm();
  tnitmin = reader.getTnitmin();
  tnitopt = reader.getTnitopt();
  tnitop2 = reader.getTnitop2();
  tnitmax = reader.getTnitmax();
  tnitopt_gauss = reader.getTnitoptGauss();
  scale_tnitopt = reader.getScaleTnitopt();
  rationit = reader.getRationit();
  cmin_pdenit = reader.getCminPdenit();
  cmax_pdenit = reader.getCmaxPdenit();
  min_pdenit = reader.getMinPdenit();
  max_pdenit = reader.getMaxPdenit();
  ratiodenit = reader.getRatiodenit();
  profdenit = reader.getProfdenit();
  vpotdenit = reader.getVpotdenit();
}

void SticsParameters::serialize(mas::schema::model::monica::SticsParameters::Builder builder) const {
  builder.setUseN2O(use_n2o);
  builder.setUseNit(use_nit);
  builder.setUseDenit(use_denit);
  builder.setCodeVnit(code_vnit);
  builder.setCodeTnit(code_tnit);
  builder.setCodeRationit(code_rationit);
  builder.setCodeHourlyWfpsNit(code_hourly_wfps_nit);
  builder.setCodePdenit(code_pdenit);
  builder.setCodeRatiodenit(code_ratiodenit);
  builder.setCodeHourlyWfpsDenit(code_hourly_wfps_denit);
  builder.setHminn(hminn);
  builder.setHoptn(hoptn);
  builder.setPHminnit(pHminnit);
  builder.setPHmaxnit(pHmaxnit);
  builder.setNh4Min(nh4_min);
  builder.setPHminden(pHminden);
  builder.setPHmaxden(pHmaxden);
  builder.setWfpsc(wfpsc);
  builder.setTdenitoptGauss(tdenitopt_gauss);
  builder.setScaleTdenitopt(scale_tdenitopt);
  builder.setKd(Kd);
  builder.setKDesat(k_desat);
  builder.setFnx(fnx);
  builder.setVnitmax(vnitmax);
  builder.setKamm(Kamm);
  builder.setTnitmin(tnitmin);
  builder.setTnitopt(tnitopt);
  builder.setTnitop2(tnitop2);
  builder.setTnitmax(tnitmax);
  builder.setTnitoptGauss(tnitopt_gauss);
  builder.setScaleTnitopt(scale_tnitopt);
  builder.setRationit(rationit);
  builder.setCminPdenit(cmin_pdenit);
  builder.setCmaxPdenit(cmax_pdenit);
  builder.setMinPdenit(min_pdenit);
  builder.setMaxPdenit(max_pdenit);
  builder.setRatiodenit(ratiodenit);
  builder.setProfdenit(profdenit);
  builder.setVpotdenit(vpotdenit);
}

// SticsParameters::SticsParameters(json11::Json j) {
//   merge(j);
// }

Errors SticsParameters::merge(json11::Json j) {
  Errors res = Json11Serializable::merge(j);

  set_bool_value(use_n2o, j, "use_n2o");
  set_bool_value(use_nit, j, "use_nit");
  set_bool_value(use_denit, j, "use_denit");
  set_int_value(code_vnit, j, "code_vnit");
  set_int_value(code_tnit, j, "code_tnit");
  set_int_value(code_rationit, j, "code_rationit");
  set_int_value(code_hourly_wfps_nit, j, "code_hourly_wfps_nit");
  set_int_value(code_pdenit, j, "code_pdenit");
  set_int_value(code_ratiodenit, j, "code_ratiodenit");
  set_int_value(code_hourly_wfps_denit, j, "code_hourly_wfps_denit");
  set_double_value(hminn, j, "hminn");
  set_double_value(hoptn, j, "hoptn");
  set_double_value(pHminnit, j, "pHminnit");
  set_double_value(pHmaxnit, j, "pHmaxnit");
  set_double_value(nh4_min, j, "nh4_min");
  set_double_value(pHminden, j, "pHminden");
  set_double_value(pHmaxden, j, "pHmaxden");
  set_double_value(wfpsc, j, "wfpsc");
  set_double_value(tdenitopt_gauss, j, "tdenitopt_gauss");
  set_double_value(scale_tdenitopt, j, "scale_tdenitopt");
  set_double_value(Kd, j, "Kd");
  set_double_value(k_desat, j, "k_desat");
  set_double_value(fnx, j, "fnx");
  set_double_value(vnitmax, j, "vnitmax");
  set_double_value(Kamm, j, "Kamm");
  set_double_value(tnitmin, j, "tnitmin");
  set_double_value(tnitopt, j, "tnitopt");
  set_double_value(tnitop2, j, "tnitop2");
  set_double_value(tnitmax, j, "tnitmax");
  set_double_value(tnitopt_gauss, j, "tnitopt_gauss");
  set_double_value(scale_tnitopt, j, "scale_tnitopt");
  set_double_value(rationit, j, "rationit");
  set_double_value(cmin_pdenit, j, "cmin_pdenit");
  set_double_value(cmax_pdenit, j, "cmax_pdenit");
  set_double_value(min_pdenit, j, "min_pdenit");
  set_double_value(max_pdenit, j, "max_pdenit");
  set_double_value(ratiodenit, j, "ratiodenit");
  set_double_value(profdenit, j, "profdenit");
  set_double_value(vpotdenit, j, "vpotdenit");

  return res;
}

json11::Json SticsParameters::to_json() const {
  return json11::Json::object
      {{"type",                   "SticsParameters"},
       {"use_n2o",                use_n2o},
       {"use_nit",                use_nit},
       {"use_denit",              use_denit},
       {"code_vnit",              J11Array{code_vnit, ""}},
       {"code_tnit",              J11Array{code_tnit, ""}},
       {"code_rationit",          J11Array{code_rationit, ""}},
       {"code_hourly_wfps_nit",   J11Array{code_hourly_wfps_nit, ""}},
       {"code_pdenit",            J11Array{code_pdenit, ""}},
       {"code_ratiodenit",        J11Array{code_ratiodenit, ""}},
       {"code_hourly_wfps_denit", J11Array{code_hourly_wfps_denit, ""}},
       {"hminn",                  J11Array{hminn, ""}},
       {"hoptn",                  J11Array{hoptn, ""}},
       {"pHminnit",               J11Array{pHminnit, ""}},
       {"pHmaxnit",               J11Array{pHmaxnit, ""}},
       {"nh4_min",                J11Array{nh4_min, ""}},
       {"pHminden",               J11Array{pHminden, ""}},
       {"pHmaxden",               J11Array{pHmaxden, ""}},
       {"wfpsc",                  J11Array{wfpsc, ""}},
       {"tdenitopt_gauss",        J11Array{tdenitopt_gauss, ""}},
       {"scale_tdenitopt",        J11Array{scale_tdenitopt, ""}},
       {"Kd",                     J11Array{Kd, ""}},
       {"k_desat",                J11Array{k_desat, ""}},
       {"fnx",                    J11Array{fnx, ""}},
       {"vnitmax",                J11Array{vnitmax, ""}},
       {"Kamm",                   J11Array{Kamm, ""}},
       {"tnitmin",                J11Array{tnitmin, ""}},
       {"tnitopt",                J11Array{tnitopt, ""}},
       {"tnitop2",                J11Array{tnitop2, ""}},
       {"tnitmax",                J11Array{tnitmax, ""}},
       {"tnitopt_gauss",          J11Array{tnitopt_gauss, ""}},
       {"scale_tnitopt",          J11Array{scale_tnitopt, ""}},
       {"rationit",               J11Array{rationit, ""}},
       {"cmin_pdenit",            J11Array{cmin_pdenit, ""}},
       {"cmax_pdenit",            J11Array{cmax_pdenit, ""}},
       {"min_pdenit",             J11Array{min_pdenit, ""}},
       {"max_pdenit",             J11Array{max_pdenit, ""}},
       {"ratiodenit",             J11Array{ratiodenit, ""}},
       {"profdenit",              J11Array{profdenit, ""}},
       {"vpotdenit",              vpotdenit}
      };
}

//-----------------------------------------------------------------------------

void SoilOrganicModuleParameters::deserialize(mas::schema::model::monica::SoilOrganicModuleParameters::Reader reader) {
  po_SOM_SlowDecCoeffStandard = reader.getSomSlowDecCoeffStandard();
  po_SOM_FastDecCoeffStandard = reader.getSomFastDecCoeffStandard();
  po_SMB_SlowMaintRateStandard = reader.getSmbSlowMaintRateStandard();
  po_SMB_FastMaintRateStandard = reader.getSmbFastMaintRateStandard();
  po_SMB_SlowDeathRateStandard = reader.getSmbSlowDeathRateStandard();
  po_SMB_FastDeathRateStandard = reader.getSmbFastDeathRateStandard();
  po_SMB_UtilizationEfficiency = reader.getSmbUtilizationEfficiency();
  po_SOM_SlowUtilizationEfficiency = reader.getSomSlowUtilizationEfficiency();
  po_SOM_FastUtilizationEfficiency = reader.getSomFastUtilizationEfficiency();
  po_AOM_SlowUtilizationEfficiency = reader.getAomSlowUtilizationEfficiency();
  po_AOM_FastUtilizationEfficiency = reader.getAomFastUtilizationEfficiency();
  po_AOM_FastMaxC_to_N = reader.getAomFastMaxCtoN();
  po_PartSOM_Fast_to_SOM_Slow = reader.getPartSOMFastToSOMSlow();
  po_PartSMB_Slow_to_SOM_Fast = reader.getPartSMBSlowToSOMFast();
  po_PartSMB_Fast_to_SOM_Fast = reader.getPartSMBFastToSOMFast();
  po_PartSOM_to_SMB_Slow = reader.getPartSOMToSMBSlow();
  po_PartSOM_to_SMB_Fast = reader.getPartSOMToSMBFast();
  po_CN_Ratio_SMB = reader.getCnRatioSMB();
  po_LimitClayEffect = reader.getLimitClayEffect();
  //po_QTenFactor = reader.getQTenFactor();
  //po_TempDecOptimal = reader.getTempDecOptimal();
  //po_MoistureDecOptimal = reader.getMoistureDecOptimal();
  po_AmmoniaOxidationRateCoeffStandard = reader.getAmmoniaOxidationRateCoeffStandard();
  po_NitriteOxidationRateCoeffStandard = reader.getNitriteOxidationRateCoeffStandard();
  po_TransportRateCoeff = reader.getTransportRateCoeff();
  po_SpecAnaerobDenitrification = reader.getSpecAnaerobDenitrification();
  po_ImmobilisationRateCoeffNO3 = reader.getImmobilisationRateCoeffNO3();
  po_ImmobilisationRateCoeffNH4 = reader.getImmobilisationRateCoeffNH4();
  po_Denit1 = reader.getDenit1();
  po_Denit2 = reader.getDenit2();
  po_Denit3 = reader.getDenit3();
  po_HydrolysisKM = reader.getHydrolysisKM();
  po_ActivationEnergy = reader.getActivationEnergy();
  po_HydrolysisP1 = reader.getHydrolysisP1();
  po_HydrolysisP2 = reader.getHydrolysisP2();
  po_AtmosphericResistance = reader.getAtmosphericResistance();
  po_N2OProductionRate = reader.getN2oProductionRate();
  po_Inhibitor_NH3 = reader.getInhibitorNH3();
  ps_MaxMineralisationDepth = reader.getPsMaxMineralisationDepth();
  sticsParams.deserialize(reader.getSticsParams());
}

void
SoilOrganicModuleParameters::serialize(mas::schema::model::monica::SoilOrganicModuleParameters::Builder builder) const {
  builder.setSomSlowDecCoeffStandard(po_SOM_SlowDecCoeffStandard);
  builder.setSomFastDecCoeffStandard(po_SOM_FastDecCoeffStandard);
  builder.setSmbSlowMaintRateStandard(po_SMB_SlowMaintRateStandard);
  builder.setSmbFastMaintRateStandard(po_SMB_FastMaintRateStandard);
  builder.setSmbSlowDeathRateStandard(po_SMB_SlowDeathRateStandard);
  builder.setSmbFastDeathRateStandard(po_SMB_FastDeathRateStandard);
  builder.setSmbUtilizationEfficiency(po_SMB_UtilizationEfficiency);
  builder.setSomSlowUtilizationEfficiency(po_SOM_SlowUtilizationEfficiency);
  builder.setSomFastUtilizationEfficiency(po_SOM_FastUtilizationEfficiency);
  builder.setAomSlowUtilizationEfficiency(po_AOM_SlowUtilizationEfficiency);
  builder.setAomFastUtilizationEfficiency(po_AOM_FastUtilizationEfficiency);
  builder.setAomFastMaxCtoN(po_AOM_FastMaxC_to_N);
  builder.setPartSOMFastToSOMSlow(po_PartSOM_Fast_to_SOM_Slow);
  builder.setPartSMBSlowToSOMFast(po_PartSMB_Slow_to_SOM_Fast);
  builder.setPartSMBFastToSOMFast(po_PartSMB_Fast_to_SOM_Fast);
  builder.setPartSOMToSMBSlow(po_PartSOM_to_SMB_Slow);
  builder.setPartSOMToSMBFast(po_PartSOM_to_SMB_Fast);
  builder.setCnRatioSMB(po_CN_Ratio_SMB);
  builder.setLimitClayEffect(po_LimitClayEffect);
  //builder.setQTenFactor(po_QTenFactor);
  //builder.setTempDecOptimal(po_TempDecOptimal);
  //builder.setMoistureDecOptimal(po_MoistureDecOptimal);
  builder.setAmmoniaOxidationRateCoeffStandard(po_AmmoniaOxidationRateCoeffStandard);
  builder.setNitriteOxidationRateCoeffStandard(po_NitriteOxidationRateCoeffStandard);
  builder.setTransportRateCoeff(po_TransportRateCoeff);
  builder.setSpecAnaerobDenitrification(po_SpecAnaerobDenitrification);
  builder.setImmobilisationRateCoeffNO3(po_ImmobilisationRateCoeffNO3);
  builder.setImmobilisationRateCoeffNH4(po_ImmobilisationRateCoeffNH4);
  builder.setDenit1(po_Denit1);
  builder.setDenit2(po_Denit2);
  builder.setDenit3(po_Denit3);
  builder.setHydrolysisKM(po_HydrolysisKM);
  builder.setActivationEnergy(po_ActivationEnergy);
  builder.setHydrolysisP1(po_HydrolysisP1);
  builder.setHydrolysisP2(po_HydrolysisP2);
  builder.setAtmosphericResistance(po_AtmosphericResistance);
  builder.setN2oProductionRate(po_N2OProductionRate);
  builder.setInhibitorNH3(po_Inhibitor_NH3);
  builder.setPsMaxMineralisationDepth(ps_MaxMineralisationDepth);
  sticsParams.serialize(builder.initSticsParams());
}

// SoilOrganicModuleParameters::SoilOrganicModuleParameters(json11::Json j) {
//   merge(j);
// }

Errors SoilOrganicModuleParameters::merge(json11::Json j) {
  Errors res = Json11Serializable::merge(j);

  set_double_value(po_SOM_SlowDecCoeffStandard, j, "SOM_SlowDecCoeffStandard");
  set_double_value(po_SOM_FastDecCoeffStandard, j, "SOM_FastDecCoeffStandard");
  set_double_value(po_SMB_SlowMaintRateStandard, j, "SMB_SlowMaintRateStandard");
  set_double_value(po_SMB_FastMaintRateStandard, j, "SMB_FastMaintRateStandard");
  set_double_value(po_SMB_SlowDeathRateStandard, j, "SMB_SlowDeathRateStandard");
  set_double_value(po_SMB_FastDeathRateStandard, j, "SMB_FastDeathRateStandard");
  set_double_value(po_SMB_UtilizationEfficiency, j, "SMB_UtilizationEfficiency");
  set_double_value(po_SOM_SlowUtilizationEfficiency, j, "SOM_SlowUtilizationEfficiency");
  set_double_value(po_SOM_FastUtilizationEfficiency, j, "SOM_FastUtilizationEfficiency");
  set_double_value(po_AOM_SlowUtilizationEfficiency, j, "AOM_SlowUtilizationEfficiency");
  set_double_value(po_AOM_FastUtilizationEfficiency, j, "AOM_FastUtilizationEfficiency");
  set_double_value(po_AOM_FastMaxC_to_N, j, "AOM_FastMaxC_to_N");
  set_double_value(po_PartSOM_Fast_to_SOM_Slow, j, "PartSOM_Fast_to_SOM_Slow");
  set_double_value(po_PartSMB_Slow_to_SOM_Fast, j, "PartSMB_Slow_to_SOM_Fast");
  set_double_value(po_PartSMB_Fast_to_SOM_Fast, j, "PartSMB_Fast_to_SOM_Fast");
  set_double_value(po_PartSOM_to_SMB_Slow, j, "PartSOM_to_SMB_Slow");
  set_double_value(po_PartSOM_to_SMB_Fast, j, "PartSOM_to_SMB_Fast");
  set_double_value(po_CN_Ratio_SMB, j, "CN_Ratio_SMB");
  set_double_value(po_LimitClayEffect, j, "LimitClayEffect");
  set_double_value(po_QTenFactor, j, "QTenFactor");
  set_double_value(po_TempDecOptimal, j, "TempDecOptimal");
  set_double_value(po_MoistureDecOptimal, j, "MoistureDecOptimal");
  set_double_value(po_AmmoniaOxidationRateCoeffStandard, j, "AmmoniaOxidationRateCoeffStandard");
  set_double_value(po_NitriteOxidationRateCoeffStandard, j, "NitriteOxidationRateCoeffStandard");
  set_double_value(po_TransportRateCoeff, j, "TransportRateCoeff");
  set_double_value(po_SpecAnaerobDenitrification, j, "SpecAnaerobDenitrification");
  set_double_value(po_ImmobilisationRateCoeffNO3, j, "ImmobilisationRateCoeffNO3");
  set_double_value(po_ImmobilisationRateCoeffNH4, j, "ImmobilisationRateCoeffNH4");
  set_double_value(po_Denit1, j, "Denit1");
  set_double_value(po_Denit2, j, "Denit2");
  set_double_value(po_Denit3, j, "Denit3");
  set_double_value(po_HydrolysisKM, j, "HydrolysisKM");
  set_double_value(po_ActivationEnergy, j, "ActivationEnergy");
  set_double_value(po_HydrolysisP1, j, "HydrolysisP1");
  set_double_value(po_HydrolysisP2, j, "HydrolysisP2");
  set_double_value(po_AtmosphericResistance, j, "AtmosphericResistance");
  set_double_value(po_N2OProductionRate, j, "N2OProductionRate");
  set_double_value(po_Inhibitor_NH3, j, "Inhibitor_NH3");
  set_double_value(ps_MaxMineralisationDepth, j, "MaxMineralisationDepth");

  set_bool_value(__enable_kaiteew_TempOnDecompostion__, j, "__enable_kaiteew_TempOnDecompostion__");
  set_bool_value(__enable_kaiteew_MoistOnDecompostion__, j, "__enable_kaiteew_MoistOnDecompostion__");
  set_bool_value(__enable_kaiteew_ClayOnDecompostion__, j, "__enable_kaiteew_ClayOnDecompostion__");

  if (j["stics"].is_object()) res.append(sticsParams.merge(j["stics"]));

  return res;
}

json11::Json SoilOrganicModuleParameters::to_json() const {
  return json11::Json::object
      {{"type",                              "SoilOrganicModuleParameters"},
       {"SOM_SlowDecCoeffStandard",          J11Array{po_SOM_SlowDecCoeffStandard, "d-1"}},
       {"SOM_FastDecCoeffStandard",          J11Array{po_SOM_FastDecCoeffStandard, "d-1"}},
       {"SMB_SlowMaintRateStandard",         J11Array{po_SMB_SlowMaintRateStandard, "d-1"}},
       {"SMB_FastMaintRateStandard",         J11Array{po_SMB_FastMaintRateStandard, "d-1"}},
       {"SMB_SlowDeathRateStandard",         J11Array{po_SMB_SlowDeathRateStandard, "d-1"}},
       {"SMB_FastDeathRateStandard",         J11Array{po_SMB_FastDeathRateStandard, "d-1"}},
       {"SMB_UtilizationEfficiency",         J11Array{po_SMB_UtilizationEfficiency, "d-1"}},
       {"SOM_SlowUtilizationEfficiency",     J11Array{po_SOM_SlowUtilizationEfficiency, ""}},
       {"SOM_FastUtilizationEfficiency",     J11Array{po_SOM_FastUtilizationEfficiency, ""}},
       {"AOM_SlowUtilizationEfficiency",     J11Array{po_AOM_SlowUtilizationEfficiency, ""}},
       {"AOM_FastUtilizationEfficiency",     J11Array{po_AOM_FastUtilizationEfficiency, ""}},
       {"AOM_FastMaxC_to_N",                 J11Array{po_AOM_FastMaxC_to_N, ""}},
       {"PartSOM_Fast_to_SOM_Slow",          J11Array{po_PartSOM_Fast_to_SOM_Slow, ""}},
       {"PartSMB_Slow_to_SOM_Fast",          J11Array{po_PartSMB_Slow_to_SOM_Fast, ""}},
       {"PartSMB_Fast_to_SOM_Fast",          J11Array{po_PartSMB_Fast_to_SOM_Fast, ""}},
       {"PartSOM_to_SMB_Slow",               J11Array{po_PartSOM_to_SMB_Slow, ""}},
       {"PartSOM_to_SMB_Fast",               J11Array{po_PartSOM_to_SMB_Fast, ""}},
       {"CN_Ratio_SMB",                      J11Array{po_CN_Ratio_SMB, ""}},
       {"LimitClayEffect",                   J11Array{po_LimitClayEffect, "kg kg-1"}},
       {"QTenFactor",                        J11Array{po_QTenFactor, ""}},
       {"TempDecOptimal",                    J11Array{po_TempDecOptimal, "°C"}},
       {"MoistureDecOptimal",                J11Array{po_MoistureDecOptimal, "%"}},
       {"AmmoniaOxidationRateCoeffStandard", J11Array{po_AmmoniaOxidationRateCoeffStandard, "d-1"}},
       {"NitriteOxidationRateCoeffStandard", J11Array{po_NitriteOxidationRateCoeffStandard, "d-1"}},
       {"TransportRateCoeff",                J11Array{po_TransportRateCoeff, "d-1"}},
       {"SpecAnaerobDenitrification",        J11Array{po_SpecAnaerobDenitrification, "g gas-N g CO2-C-1"}},
       {"ImmobilisationRateCoeffNO3",        J11Array{po_ImmobilisationRateCoeffNO3, "d-1"}},
       {"ImmobilisationRateCoeffNH4",        J11Array{po_ImmobilisationRateCoeffNH4, "d-1"}},
       {"Denit1",                            J11Array{po_Denit1, ""}},
       {"Denit2",                            J11Array{po_Denit2, ""}},
       {"Denit3",                            J11Array{po_Denit3, ""}},
       {"HydrolysisKM",                      J11Array{po_HydrolysisKM, ""}},
       {"ActivationEnergy",                  J11Array{po_ActivationEnergy, ""}},
       {"HydrolysisP1",                      J11Array{po_HydrolysisP1, ""}},
       {"HydrolysisP2",                      J11Array{po_HydrolysisP2, ""}},
       {"AtmosphericResistance",             J11Array{po_AtmosphericResistance, "s m-1"}},
       {"N2OProductionRate",                 J11Array{po_N2OProductionRate, "d-1"}},
       {"Inhibitor_NH3",                     J11Array{po_Inhibitor_NH3, "kg N m-3"}},
       {"MaxMineralisationDepth",            ps_MaxMineralisationDepth}
      };
}

CentralParameterProvider::CentralParameterProvider()
    : _pathToOutputDir("."), precipCorrectionValues(12, 1.0) {

}

// CentralParameterProvider::CentralParameterProvider(json11::Json j) {
//   merge(j);
// }

Errors CentralParameterProvider::merge(json11::Json j) {
  Errors res;

  res.append(userCropParameters.merge(j["userCropParameters"]));
  res.append(userEnvironmentParameters.merge(j["userEnvironmentParameters"]));
  res.append(userSoilMoistureParameters.merge(j["userSoilMoistureParameters"]));
  res.append(userSoilTemperatureParameters.merge(j["userSoilTemperatureParameters"]));
  res.append(userSoilTransportParameters.merge(j["userSoilTransportParameters"]));
  res.append(userSoilOrganicParameters.merge(j["userSoilOrganicParameters"]));
  res.append(simulationParameters.merge(j["simulationParameters"]));
  res.append(siteParameters.merge(j["siteParameters"]));
  if (!j["groundwaterInformation"].is_null()) {
    res.append(groundwaterInformation.merge(j["groundwaterInformation"]));
  }

  //set_bool_value(_writeOutputFiles, j, "writeOutputFiles");

  return res;
}

json11::Json CentralParameterProvider::to_json() const {
  return json11::Json::object
      {{"type",                          "CentralParameterProvider"},
       {"userCropParameters",            userCropParameters.to_json()},
       {"userEnvironmentParameters",     userEnvironmentParameters.to_json()},
       {"userSoilMoistureParameters",    userSoilMoistureParameters.to_json()},
       {"userSoilTemperatureParameters", userSoilTemperatureParameters.to_json()},
       {"userSoilTransportParameters",   userSoilTransportParameters.to_json()},
       {"userSoilOrganicParameters",     userSoilOrganicParameters.to_json()},
       {"simulationParameters",          simulationParameters.to_json()},
       {"siteParameters",                siteParameters.to_json()}
          //, {"groundwaterInformation", groundwaterInformation.to_json()}
          //, {"writeOutputFiles", writeOutputFiles()}
      };
}

/**
 * @brief Returns a precipitation correction value for a specific month.
 * @param month Month
 * @return Correction value that should be applied to precipitation value read from database.
 */
double CentralParameterProvider::getPrecipCorrectionValue(int month) const {
  assert(month < 12);
  assert(month >= 0);

  return precipCorrectionValues.at(month);
  //cerr << "Requested correction value for precipitation for an invalid month.\nMust be in range of 0<=value<12." << endl;
  //return 1.0;
}

/**
 * Sets a correction value for a specific month.
 * @param month Month the value should be used for.
 * @param value Correction value that should be added.
 */
void CentralParameterProvider::setPrecipCorrectionValue(int month, double value) {
  assert(month < 12);
  assert(month >= 0);
  precipCorrectionValues[month] = value;

  // debug
  //  cout << "Added precip correction value for month " << month << ":\t " << value << endl;
}

// --------------------------------------------------------------------
