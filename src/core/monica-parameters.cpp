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
 * @param organId organ ID
 * @param yieldPercentage Yield percentage
 */
YieldComponent monica::makeYieldComponent(int organId, double yieldPercentage, double yieldDryMatter) {
  YieldComponent yc;
  yc.organId = organId;
  yc.yieldPercentage = yieldPercentage;
  yc.yieldDryMatter = yieldDryMatter;
  return yc;
}

YieldComponent monica::makeYieldComponent(mas::schema::model::monica::YieldComponent::Reader reader) {
  YieldComponent yc;
  yieldcomponent::deserialize(&yc, reader);
  return yc;
}

void yieldcomponent::deserialize(YieldComponent* yc, mas::schema::model::monica::YieldComponent::Reader reader) {
  yc->organId = (int)reader.getOrganId();
  yc->yieldPercentage = reader.getYieldPercentage();
  yc->yieldDryMatter = reader.getYieldDryMatter();
}

void yieldcomponent::serialize(const YieldComponent* yc, mas::schema::model::monica::YieldComponent::Builder builder) {
  builder.setOrganId(yc->organId);
  builder.setYieldPercentage(yc->yieldPercentage);
  builder.setYieldDryMatter(yc->yieldDryMatter);
}

Errors yieldcomponent::merge(YieldComponent* yc, json11::Json j) {
  set_int_value(yc->organId, j, "organId");
  set_double_value(yc->yieldPercentage, j, "yieldPercentage");
  set_double_value(yc->yieldDryMatter, j, "yieldDryMatter");

  return {};
}

json11::Json yieldcomponent::to_json(const YieldComponent* yc) {
  return json11::Json::object
  {
    {"type", "YieldComponent"},
    {"organId", yc->organId},
    {"yieldPercentage", yc->yieldPercentage},
    {"yieldDryMatter", yc->yieldDryMatter}
  };
}


SpeciesParameters monica::makeSpeciesParameters(mas::schema::model::monica::SpeciesParameters::Reader reader) {
  SpeciesParameters sp;
  speciesparameters::deserialize(&sp, reader);
  return sp;
}

void speciesparameters::deserialize(SpeciesParameters* sp, mas::schema::model::monica::SpeciesParameters::Reader reader) {
  sp->pc_SpeciesId = reader.getSpeciesId();
  sp->pc_CarboxylationPathway = reader.getCarboxylationPathway();
  sp->pc_DefaultRadiationUseEfficiency = reader.getDefaultRadiationUseEfficiency();
  sp->pc_PartBiologicalNFixation = reader.getPartBiologicalNFixation();
  sp->pc_InitialKcFactor = reader.getInitialKcFactor();
  sp->pc_LuxuryNCoeff = reader.getLuxuryNCoeff();
  sp->pc_MaxCropDiameter = reader.getMaxCropDiameter();
  sp->pc_StageAtMaxHeight = reader.getStageAtMaxHeight();
  sp->pc_StageAtMaxDiameter = reader.getStageAtMaxDiameter();
  sp->pc_MinimumNConcentration = reader.getMinimumNConcentration();
  sp->pc_MinimumTemperatureForAssimilation = reader.getMinimumTemperatureForAssimilation();
  sp->pc_OptimumTemperatureForAssimilation = reader.getOptimumTemperatureForAssimilation();
  sp->pc_MaximumTemperatureForAssimilation = reader.getMaximumTemperatureForAssimilation();
  sp->pc_NConcentrationAbovegroundBiomass = reader.getNConcentrationAbovegroundBiomass();
  sp->pc_NConcentrationB0 = reader.getNConcentrationB0();
  sp->pc_NConcentrationPN = reader.getNConcentrationPN();
  sp->pc_NConcentrationRoot = reader.getNConcentrationRoot();
  sp->pc_DevelopmentAccelerationByNitrogenStress = reader.getDevelopmentAccelerationByNitrogenStress();
  sp->pc_FieldConditionModifier = reader.getFieldConditionModifier();
  sp->pc_AssimilateReallocation = reader.getAssimilateReallocation();
  setFromCapnpList(sp->pc_BaseTemperature, reader.getBaseTemperature());
  setFromCapnpList(sp->pc_OrganMaintenanceRespiration, reader.getOrganMaintenanceRespiration());
  setFromCapnpList(sp->pc_OrganGrowthRespiration, reader.getOrganGrowthRespiration());
  setFromCapnpList(sp->pc_StageMaxRootNConcentration, reader.getStageMaxRootNConcentration());
  setFromCapnpList(sp->pc_InitialOrganBiomass, reader.getInitialOrganBiomass());
  setFromCapnpList(sp->pc_CriticalOxygenContent, reader.getCriticalOxygenContent());
  setFromCapnpList(sp->pc_StageMobilFromStorageCoeff, reader.getStageMobilFromStorageCoeff());
  if (sp->pc_StageMobilFromStorageCoeff.empty()) {
    sp->pc_StageMobilFromStorageCoeff = vector<double>(sp->pc_CriticalOxygenContent.size(), 0);
  }
  setFromCapnpList(sp->pc_AbovegroundOrgan, reader.getAbovegroundOrgan());
  setFromCapnpList(sp->pc_StorageOrgan, reader.getStorageOrgan());
  sp->pc_SamplingDepth = reader.getSamplingDepth();
  sp->pc_TargetNSamplingDepth = reader.getTargetNSamplingDepth();
  sp->pc_TargetN30 = reader.getTargetN30();
  sp->pc_MaxNUptakeParam = reader.getMaxNUptakeParam();
  sp->pc_RootDistributionParam = reader.getRootDistributionParam();
  sp->pc_PlantDensity = reader.getPlantDensity();
  sp->pc_RootGrowthLag = reader.getRootGrowthLag();
  sp->pc_MinimumTemperatureRootGrowth = reader.getMinimumTemperatureRootGrowth();
  sp->pc_InitialRootingDepth = reader.getInitialRootingDepth();
  sp->pc_RootPenetrationRate = reader.getRootPenetrationRate();
  sp->pc_RootFormFactor = reader.getRootFormFactor();
  sp->pc_SpecificRootLength = reader.getSpecificRootLength();
  sp->pc_StageAfterCut = reader.getStageAfterCut();
  sp->pc_LimitingTemperatureHeatStress = reader.getLimitingTemperatureHeatStress();
  sp->pc_CuttingDelayDays = reader.getCuttingDelayDays();
  sp->pc_DroughtImpactOnFertilityFactor = reader.getDroughtImpactOnFertilityFactor();
  sp->EF_MONO = reader.getEfMono();
  sp->EF_MONOS = reader.getEfMonos();
  sp->EF_ISO = reader.getEfIso();
  sp->VCMAX25 = reader.getVcMax25();
  sp->AEKC = reader.getAekc();
  sp->AEKO = reader.getAeko();
  sp->AEVC = reader.getAevc();
  sp->KC25 = reader.getKc25();
  sp->KO25 = reader.getKo25();
  sp->pc_TransitionStageLeafExp = reader.getTransitionStageLeafExp();
}

void speciesparameters::serialize(const SpeciesParameters* sp, mas::schema::model::monica::SpeciesParameters::Builder builder) {
  builder.setSpeciesId(sp->pc_SpeciesId);
  builder.setCarboxylationPathway(sp->pc_CarboxylationPathway);
  builder.setDefaultRadiationUseEfficiency(sp->pc_DefaultRadiationUseEfficiency);
  builder.setPartBiologicalNFixation(sp->pc_PartBiologicalNFixation);
  builder.setInitialKcFactor(sp->pc_InitialKcFactor);
  builder.setLuxuryNCoeff(sp->pc_LuxuryNCoeff);
  builder.setMaxCropDiameter(sp->pc_MaxCropDiameter);
  builder.setStageAtMaxHeight(sp->pc_StageAtMaxHeight);
  builder.setStageAtMaxDiameter(sp->pc_StageAtMaxDiameter);
  builder.setMinimumNConcentration(sp->pc_MinimumNConcentration);
  builder.setMinimumTemperatureForAssimilation(sp->pc_MinimumTemperatureForAssimilation);
  builder.setOptimumTemperatureForAssimilation(sp->pc_OptimumTemperatureForAssimilation);
  builder.setMaximumTemperatureForAssimilation(sp->pc_MaximumTemperatureForAssimilation);
  builder.setNConcentrationAbovegroundBiomass(sp->pc_NConcentrationAbovegroundBiomass);
  builder.setNConcentrationB0(sp->pc_NConcentrationB0);
  builder.setNConcentrationPN(sp->pc_NConcentrationPN);
  builder.setNConcentrationRoot(sp->pc_NConcentrationRoot);
  builder.setDevelopmentAccelerationByNitrogenStress(sp->pc_DevelopmentAccelerationByNitrogenStress);
  builder.setFieldConditionModifier(sp->pc_FieldConditionModifier);
  builder.setAssimilateReallocation(sp->pc_AssimilateReallocation);
  setCapnpList(sp->pc_BaseTemperature, builder.initBaseTemperature((capnp::uint)sp->pc_BaseTemperature.size()));
  setCapnpList(sp->pc_OrganMaintenanceRespiration,
               builder.initOrganMaintenanceRespiration((capnp::uint)sp->pc_OrganMaintenanceRespiration.size()));
  setCapnpList(sp->pc_OrganGrowthRespiration,
               builder.initOrganGrowthRespiration((capnp::uint)sp->pc_OrganGrowthRespiration.size()));
  setCapnpList(sp->pc_StageMaxRootNConcentration,
               builder.initStageMaxRootNConcentration((capnp::uint)sp->pc_StageMaxRootNConcentration.size()));
  setCapnpList(sp->pc_InitialOrganBiomass, builder.initInitialOrganBiomass((capnp::uint)sp->pc_InitialOrganBiomass.size()));
  setCapnpList(sp->pc_CriticalOxygenContent,
               builder.initCriticalOxygenContent((capnp::uint)sp->pc_CriticalOxygenContent.size()));
  setCapnpList(sp->pc_StageMobilFromStorageCoeff,
               builder.initStageMobilFromStorageCoeff((capnp::uint)sp->pc_StageMobilFromStorageCoeff.size()));
  setCapnpList(sp->pc_AbovegroundOrgan, builder.initAbovegroundOrgan((capnp::uint)sp->pc_AbovegroundOrgan.size()));
  setCapnpList(sp->pc_StorageOrgan, builder.initStorageOrgan((capnp::uint)sp->pc_StorageOrgan.size()));
  builder.setSamplingDepth(sp->pc_SamplingDepth);
  builder.setTargetNSamplingDepth(sp->pc_TargetNSamplingDepth);
  builder.setTargetN30(sp->pc_TargetN30);
  builder.setMaxNUptakeParam(sp->pc_MaxNUptakeParam);
  builder.setRootDistributionParam(sp->pc_RootDistributionParam);
  builder.setPlantDensity(sp->pc_PlantDensity);
  builder.setRootGrowthLag(sp->pc_RootGrowthLag);
  builder.setMinimumTemperatureRootGrowth(sp->pc_MinimumTemperatureRootGrowth);
  builder.setInitialRootingDepth(sp->pc_InitialRootingDepth);
  builder.setRootPenetrationRate(sp->pc_RootPenetrationRate);
  builder.setRootFormFactor(sp->pc_RootFormFactor);
  builder.setSpecificRootLength(sp->pc_SpecificRootLength);
  builder.setStageAfterCut(sp->pc_StageAfterCut);
  builder.setLimitingTemperatureHeatStress(sp->pc_LimitingTemperatureHeatStress);
  builder.setCuttingDelayDays(sp->pc_CuttingDelayDays);
  builder.setDroughtImpactOnFertilityFactor(sp->pc_DroughtImpactOnFertilityFactor);
  builder.setEfMono(sp->EF_MONO);
  builder.setEfMonos(sp->EF_MONOS);
  builder.setEfIso(sp->EF_ISO);
  builder.setVcMax25(sp->VCMAX25);
  builder.setAekc(sp->AEKC);
  builder.setAeko(sp->AEKO);
  builder.setAevc(sp->AEVC);
  builder.setKc25(sp->KC25);
  builder.setKo25(sp->KO25);
  builder.setTransitionStageLeafExp(sp->pc_TransitionStageLeafExp);
}

Errors speciesparameters::merge(SpeciesParameters* sp, json11::Json j) {
  Errors res = defaultMerge(j, [sp](json11::Json j2) { return merge(sp, j2); });

  set_string_value(sp->pc_SpeciesId, j, "SpeciesName");
  set_int_value(sp->pc_CarboxylationPathway, j, "CarboxylationPathway");
  set_double_value(sp->pc_DefaultRadiationUseEfficiency, j, "DefaultRadiationUseEfficiency");
  set_double_value(sp->pc_PartBiologicalNFixation, j, "PartBiologicalNFixation");
  set_double_value(sp->pc_InitialKcFactor, j, "InitialKcFactor");
  set_double_value(sp->pc_LuxuryNCoeff, j, "LuxuryNCoeff");
  set_double_value(sp->pc_MaxCropDiameter, j, "MaxCropDiameter");
  set_double_value(sp->pc_StageAtMaxHeight, j, "StageAtMaxHeight");
  set_double_value(sp->pc_StageAtMaxDiameter, j, "StageAtMaxDiameter");
  set_double_value(sp->pc_MinimumNConcentration, j, "MinimumNConcentration");
  set_double_value(sp->pc_MinimumTemperatureForAssimilation, j, "MinimumTemperatureForAssimilation");
  set_double_value(sp->pc_OptimumTemperatureForAssimilation, j, "OptimumTemperatureForAssimilation");
  set_double_value(sp->pc_MaximumTemperatureForAssimilation, j, "MaximumTemperatureForAssimilation");
  set_double_value(sp->pc_NConcentrationAbovegroundBiomass, j, "NConcentrationAbovegroundBiomass");
  set_double_value(sp->pc_NConcentrationB0, j, "NConcentrationB0");
  set_double_value(sp->pc_NConcentrationPN, j, "NConcentrationPN");
  set_double_value(sp->pc_NConcentrationRoot, j, "NConcentrationRoot");
  set_int_value(sp->pc_DevelopmentAccelerationByNitrogenStress, j, "DevelopmentAccelerationByNitrogenStress");
  set_double_value(sp->pc_FieldConditionModifier, j, "FieldConditionModifier");
  set_double_value(sp->pc_AssimilateReallocation, j, "AssimilateReallocation");
  set_double_vector(sp->pc_BaseTemperature, j, "BaseTemperature");
  set_double_vector(sp->pc_OrganMaintenanceRespiration, j, "OrganMaintenanceRespiration");
  set_double_vector(sp->pc_OrganGrowthRespiration, j, "OrganGrowthRespiration");
  set_double_vector(sp->pc_StageMaxRootNConcentration, j, "StageMaxRootNConcentration");
  set_double_vector(sp->pc_InitialOrganBiomass, j, "InitialOrganBiomass");
  set_double_vector(sp->pc_CriticalOxygenContent, j, "CriticalOxygenContent");

  set_double_vector(sp->pc_StageMobilFromStorageCoeff, j, "StageMobilFromStorageCoeff");
  if (sp->pc_StageMobilFromStorageCoeff.empty()) {
    sp->pc_StageMobilFromStorageCoeff = vector<double>(sp->pc_CriticalOxygenContent.size(), 0);
  }

  set_bool_vector(sp->pc_AbovegroundOrgan, j, "AbovegroundOrgan");
  set_bool_vector(sp->pc_StorageOrgan, j, "StorageOrgan");
  set_double_value(sp->pc_SamplingDepth, j, "SamplingDepth");
  set_double_value(sp->pc_TargetNSamplingDepth, j, "TargetNSamplingDepth");
  set_double_value(sp->pc_TargetN30, j, "TargetN30");
  set_double_value(sp->pc_MaxNUptakeParam, j, "MaxNUptakeParam");
  set_double_value(sp->pc_RootDistributionParam, j, "RootDistributionParam");
  set_int_value(sp->pc_PlantDensity, j, "PlantDensity");
  set_double_value(sp->pc_RootGrowthLag, j, "RootGrowthLag");
  set_double_value(sp->pc_MinimumTemperatureRootGrowth, j, "MinimumTemperatureRootGrowth");
  set_double_value(sp->pc_InitialRootingDepth, j, "InitialRootingDepth");
  set_double_value(sp->pc_RootPenetrationRate, j, "RootPenetrationRate");
  set_double_value(sp->pc_RootFormFactor, j, "RootFormFactor");
  set_double_value(sp->pc_SpecificRootLength, j, "SpecificRootLength");
  set_int_value(sp->pc_StageAfterCut, j, "StageAfterCut");
  set_double_value(sp->pc_LimitingTemperatureHeatStress, j, "LimitingTemperatureHeatStress");
  set_int_value(sp->pc_CuttingDelayDays, j, "CuttingDelayDays");
  set_double_value(sp->pc_DroughtImpactOnFertilityFactor, j, "DroughtImpactOnFertilityFactor");

  set_double_value(sp->EF_MONO, j, "EF_MONO");
  set_double_value(sp->EF_MONOS, j, "EF_MONOS");
  set_double_value(sp->EF_ISO, j, "EF_ISO");
  set_double_value(sp->VCMAX25, j, "VCMAX25");
  set_double_value(sp->AEKC, j, "AEKC");
  set_double_value(sp->AEVC, j, "AEVC");
  set_double_value(sp->AEKO, j, "AEKO");
  set_double_value(sp->KC25, j, "KC25");
  set_double_value(sp->KO25, j, "KO25");

  set_int_value(sp->pc_TransitionStageLeafExp, j, "TransitionStageLeafExp");
  set_int_value(sp->dormancyStartDoy, j, "DormancyStartDoy");
  set_int_value(sp->dormancyEndDoy, j, "DormancyEndDoy");

  return res;
}

json11::Json speciesparameters::to_json(const SpeciesParameters* sp) {
  auto species = J11Object
  {
    {"type", "SpeciesParameters"},
    {"SpeciesName", sp->pc_SpeciesId},
    {"CarboxylationPathway", sp->pc_CarboxylationPathway},
    {"DefaultRadiationUseEfficiency", sp->pc_DefaultRadiationUseEfficiency},
    {"PartBiologicalNFixation", sp->pc_PartBiologicalNFixation},
    {"InitialKcFactor", sp->pc_InitialKcFactor},
    {"LuxuryNCoeff", sp->pc_LuxuryNCoeff},
    {"MaxCropDiameter", sp->pc_MaxCropDiameter},
    {"StageAtMaxHeight", sp->pc_StageAtMaxHeight},
    {"StageAtMaxDiameter", sp->pc_StageAtMaxDiameter},
    {"MinimumNConcentration", sp->pc_MinimumNConcentration},
    {"MinimumTemperatureForAssimilation", sp->pc_MinimumTemperatureForAssimilation},
    {"OptimumTemperatureForAssimilation", sp->pc_OptimumTemperatureForAssimilation},
    {"MaximumTemperatureForAssimilation", sp->pc_MaximumTemperatureForAssimilation},
    {"NConcentrationAbovegroundBiomass", sp->pc_NConcentrationAbovegroundBiomass},
    {"NConcentrationB0", sp->pc_NConcentrationB0},
    {"NConcentrationPN", sp->pc_NConcentrationPN},
    {"NConcentrationRoot", sp->pc_NConcentrationRoot},
    {"DevelopmentAccelerationByNitrogenStress", sp->pc_DevelopmentAccelerationByNitrogenStress},
    {"FieldConditionModifier", sp->pc_FieldConditionModifier},
    {"AssimilateReallocation", sp->pc_AssimilateReallocation},
    {"BaseTemperature", toPrimJsonArray(sp->pc_BaseTemperature)},
    {"OrganMaintenanceRespiration", toPrimJsonArray(sp->pc_OrganMaintenanceRespiration)},
    {"OrganGrowthRespiration", toPrimJsonArray(sp->pc_OrganGrowthRespiration)},
    {"StageMaxRootNConcentration", toPrimJsonArray(sp->pc_StageMaxRootNConcentration)},
    {"InitialOrganBiomass", toPrimJsonArray(sp->pc_InitialOrganBiomass)},
    {"CriticalOxygenContent", toPrimJsonArray(sp->pc_CriticalOxygenContent)},
    {"StageMobilFromStorageCoeff", toPrimJsonArray(sp->pc_StageMobilFromStorageCoeff)},
    {"AbovegroundOrgan", toPrimJsonArray(sp->pc_AbovegroundOrgan)},
    {"StorageOrgan", toPrimJsonArray(sp->pc_StorageOrgan)},
    {"SamplingDepth", sp->pc_SamplingDepth},
    {"TargetNSamplingDepth", sp->pc_TargetNSamplingDepth},
    {"TargetN30", sp->pc_TargetN30},
    {"MaxNUptakeParam", sp->pc_MaxNUptakeParam},
    {"RootDistributionParam", sp->pc_RootDistributionParam},
    {"PlantDensity", J11Array{sp->pc_PlantDensity, "plants m-2"}},
    {"RootGrowthLag", sp->pc_RootGrowthLag},
    {"MinimumTemperatureRootGrowth", sp->pc_MinimumTemperatureRootGrowth},
    {"InitialRootingDepth", sp->pc_InitialRootingDepth},
    {"RootPenetrationRate", sp->pc_RootPenetrationRate},
    {"RootFormFactor", sp->pc_RootFormFactor},
    {"SpecificRootLength", sp->pc_SpecificRootLength},
    {"StageAfterCut", sp->pc_StageAfterCut},
    {"LimitingTemperatureHeatStress", sp->pc_LimitingTemperatureHeatStress},
    {"CuttingDelayDays", sp->pc_CuttingDelayDays},
    {"DroughtImpactOnFertilityFactor", sp->pc_DroughtImpactOnFertilityFactor},
    {"EF_MONO", J11Array{sp->EF_MONO, "ug gDW-1 h-1"}},
    {"EF_MONOS", J11Array{sp->EF_MONOS, "ug gDW-1 h-1"}},
    {"EF_ISO", J11Array{sp->EF_ISO, "ug gDW-1 h-1"}},
    {"VCMAX25", J11Array{sp->VCMAX25, "umol m-2 s-1"}},
    {"AEKC", J11Array{sp->AEKC, "J mol-1"}},
    {"AEKO", J11Array{sp->AEKO, "J mol-1"}},
    {"AEVC", J11Array{sp->AEVC, "J mol-1"}},
    {"KC25", J11Array{sp->KC25, "umol mol-1 ubar-1"}},
    {"KO25", J11Array{sp->KO25, "mmol mol-1 mbar-1"}},
    {"TransitionStageLeafExp", J11Array{sp->pc_TransitionStageLeafExp, "1-7"}},
    {"DormancyStartDoy", sp->dormancyStartDoy},
    {"DormancyEndDoy", sp->dormancyEndDoy}
  };

  return species;
}

size_t speciesparameters::numberOfDevelopmentalStages(const SpeciesParameters* sp) {
  return sp->pc_BaseTemperature.size();
}

size_t speciesparameters::numberOfOrgans(const SpeciesParameters* sp) {
  return sp->pc_OrganGrowthRespiration.size();
}

// CultivarParameters::CultivarParameters(json11::Json j) {
//   merge(j);
// }

CultivarParameters monica::makeCultivarParameters(mas::schema::model::monica::CultivarParameters::Reader reader) {
  CultivarParameters cp;
  cultivarparameters::deserialize(&cp, reader);
  return cp;
}

void cultivarparameters::deserialize(CultivarParameters* cp, mas::schema::model::monica::CultivarParameters::Reader reader) {
  cp->pc_CultivarId = reader.getCultivarId();
  cp->pc_Description = reader.getDescription();
  cp->pc_Perennial = reader.getPerennial();
  cp->pc_MaxAssimilationRate = reader.getMaxAssimilationRate();
  cp->pc_MaxCropHeight = reader.getMaxCropHeight();
  cp->pc_ResidueNRatio = reader.getResidueNRatio();
  cp->pc_LT50cultivar = reader.getLt50cultivar();
  cp->pc_CropHeightP1 = reader.getCropHeightP1();
  cp->pc_CropHeightP2 = reader.getCropHeightP2();
  cp->pc_CropSpecificMaxRootingDepth = reader.getCropSpecificMaxRootingDepth();

  {
    const auto listReader = reader.getAssimilatePartitioningCoeff();
    for (const auto lr : listReader) {
      vector<double> v;
      setFromCapnpList(v, lr);
      cp->pc_AssimilatePartitioningCoeff.emplace_back(v);
    }
  }

  {
    const auto listReader = reader.getOrganSenescenceRate();
    for (const auto lr : listReader) {
      vector<double> v;
      setFromCapnpList(v, lr);
      cp->pc_OrganSenescenceRate.emplace_back(v);
    }
  }

  setFromCapnpList(cp->pc_BaseDaylength, reader.getBaseDaylength());
  setFromCapnpList(cp->pc_OptimumTemperature, reader.getOptimumTemperature());
  setFromCapnpList(cp->pc_DaylengthRequirement, reader.getDaylengthRequirement());
  setFromCapnpList(cp->pc_DroughtStressThreshold, reader.getDroughtStressThreshold());
  setFromCapnpList(cp->pc_SpecificLeafArea, reader.getSpecificLeafArea());
  setFromCapnpList(cp->pc_StageKcFactor, reader.getStageKcFactor());
  setFromCapnpList(cp->pc_StageTemperatureSum, reader.getStageTemperatureSum());
  setFromCapnpList(cp->pc_VernalisationRequirement, reader.getVernalisationRequirement());
  cp->pc_HeatSumIrrigationStart = reader.getHeatSumIrrigationStart();
  cp->pc_HeatSumIrrigationEnd = reader.getHeatSumIrrigationEnd();
  cp->pc_CriticalTemperatureHeatStress = reader.getCriticalTemperatureHeatStress();
  cp->pc_BeginSensitivePhaseHeatStress = reader.getBeginSensitivePhaseHeatStress();
  cp->pc_EndSensitivePhaseHeatStress = reader.getEndSensitivePhaseHeatStress();
  cp->pc_FrostHardening = reader.getFrostHardening();
  cp->pc_FrostDehardening = reader.getFrostDehardening();
  cp->pc_LowTemperatureExposure = reader.getLowTemperatureExposure();
  cp->pc_RespiratoryStress = reader.getRespiratoryStress();
  cp->pc_LatestHarvestDoy = reader.getLatestHarvestDoy();
  auto deserializeYieldComponents = [](std::vector<YieldComponent>& ycs, auto listReader) {
    ycs.resize(listReader.size());
    uint32_t i = 0;
    for (auto& yc : ycs) yieldcomponent::deserialize(&yc, listReader[i++]);
  };
  deserializeYieldComponents(cp->pc_OrganIdsForPrimaryYield, reader.getOrganIdsForPrimaryYield());
  deserializeYieldComponents(cp->pc_OrganIdsForSecondaryYield, reader.getOrganIdsForSecondaryYield());
  deserializeYieldComponents(cp->pc_OrganIdsForCutting, reader.getOrganIdsForCutting());
  cp->pc_EarlyRefLeafExp = reader.getEarlyRefLeafExp();
  cp->pc_RefLeafExp = reader.getRefLeafExp();
  cp->pc_MinTempDev_WE = reader.getMinTempDevWE();
  cp->pc_OptTempDev_WE = reader.getOptTempDevWE();
  cp->pc_MaxTempDev_WE = reader.getMaxTempDevWE();
  cp->winterCrop = reader.getWinterCrop();
}

void cultivarparameters::serialize(const CultivarParameters* cp, mas::schema::model::monica::CultivarParameters::Builder builder) {
  builder.setCultivarId(cp->pc_CultivarId);
  builder.setDescription(cp->pc_Description);
  builder.setPerennial(cp->pc_Perennial);
  builder.setMaxAssimilationRate(cp->pc_MaxAssimilationRate);
  builder.setMaxCropHeight(cp->pc_MaxCropHeight);
  builder.setResidueNRatio(cp->pc_ResidueNRatio);
  builder.setLt50cultivar(cp->pc_LT50cultivar);
  builder.setCropHeightP1(cp->pc_CropHeightP1);
  builder.setCropHeightP2(cp->pc_CropHeightP2);
  builder.setCropSpecificMaxRootingDepth(cp->pc_CropSpecificMaxRootingDepth);

  {
    auto listBuilder = builder.initAssimilatePartitioningCoeff((capnp::uint)cp->pc_AssimilatePartitioningCoeff.size());
    capnp::uint i = 0;
    for (const auto& v : cp->pc_AssimilatePartitioningCoeff) setCapnpList(v, listBuilder.init(i++, (capnp::uint)v.size()));
  }

  {
    auto listBuilder = builder.initOrganSenescenceRate((capnp::uint)cp->pc_OrganSenescenceRate.size());
    capnp::uint i = 0;
    for (const auto& v : cp->pc_OrganSenescenceRate) setCapnpList(v, listBuilder.init(i++, (capnp::uint)v.size()));
  }

  setCapnpList(cp->pc_BaseDaylength, builder.initBaseDaylength((capnp::uint)cp->pc_BaseDaylength.size()));
  setCapnpList(cp->pc_OptimumTemperature, builder.initOptimumTemperature((capnp::uint)cp->pc_OptimumTemperature.size()));
  setCapnpList(cp->pc_DaylengthRequirement, builder.initDaylengthRequirement((capnp::uint)cp->pc_DaylengthRequirement.size()));
  setCapnpList(cp->pc_DroughtStressThreshold,
               builder.initDroughtStressThreshold((capnp::uint)cp->pc_DroughtStressThreshold.size()));
  setCapnpList(cp->pc_SpecificLeafArea, builder.initSpecificLeafArea((capnp::uint)cp->pc_SpecificLeafArea.size()));
  setCapnpList(cp->pc_StageKcFactor, builder.initStageKcFactor((capnp::uint)cp->pc_StageKcFactor.size()));
  setCapnpList(cp->pc_StageTemperatureSum, builder.initStageTemperatureSum((capnp::uint)cp->pc_StageTemperatureSum.size()));
  setCapnpList(cp->pc_VernalisationRequirement,
               builder.initVernalisationRequirement((capnp::uint)cp->pc_VernalisationRequirement.size()));
  builder.setHeatSumIrrigationStart(cp->pc_HeatSumIrrigationStart);
  builder.setHeatSumIrrigationEnd(cp->pc_HeatSumIrrigationEnd);
  builder.setCriticalTemperatureHeatStress(cp->pc_CriticalTemperatureHeatStress);
  builder.setBeginSensitivePhaseHeatStress(cp->pc_BeginSensitivePhaseHeatStress);
  builder.setEndSensitivePhaseHeatStress(cp->pc_EndSensitivePhaseHeatStress);
  builder.setFrostHardening(cp->pc_FrostHardening);
  builder.setFrostDehardening(cp->pc_FrostDehardening);
  builder.setLowTemperatureExposure(cp->pc_LowTemperatureExposure);
  builder.setRespiratoryStress(cp->pc_RespiratoryStress);
  builder.setLatestHarvestDoy(cp->pc_LatestHarvestDoy);
  auto serializeYieldComponents = [](const std::vector<YieldComponent>& ycs, auto listBuilder) {
    uint32_t i = 0;
    for (const auto& yc : ycs) yieldcomponent::serialize(&yc, listBuilder[i++]);
  };
  serializeYieldComponents(cp->pc_OrganIdsForPrimaryYield,
                           builder.initOrganIdsForPrimaryYield((capnp::uint)cp->pc_OrganIdsForPrimaryYield.size()));
  serializeYieldComponents(cp->pc_OrganIdsForSecondaryYield,
                           builder.initOrganIdsForSecondaryYield((capnp::uint)cp->pc_OrganIdsForSecondaryYield.size()));
  serializeYieldComponents(cp->pc_OrganIdsForCutting,
                           builder.initOrganIdsForCutting((capnp::uint)cp->pc_OrganIdsForCutting.size()));
  builder.setEarlyRefLeafExp(cp->pc_EarlyRefLeafExp);
  builder.setRefLeafExp(cp->pc_RefLeafExp);
  builder.setMinTempDevWE(cp->pc_MinTempDev_WE);
  builder.setOptTempDevWE(cp->pc_OptTempDev_WE);
  builder.setMaxTempDevWE(cp->pc_MaxTempDev_WE);
  builder.setWinterCrop(cp->winterCrop);
}

Errors cultivarparameters::merge(CultivarParameters* cp, json11::Json j) {
  Errors res = defaultMerge(j, [cp](json11::Json j2) { return merge(cp, j2); });

  auto mergeYieldComponents = [](json11::Json arr) {
    std::vector<YieldComponent> ycs;
    for (json11::Json jyc : arr.array_items()) {
      YieldComponent yc;
      yieldcomponent::merge(&yc, jyc);
      ycs.push_back(yc);
    }
    return ycs;
  };

  string err;
  if (j.has_shape({{"OrganIdsForPrimaryYield", json11::Json::ARRAY}}, err))
    cp->pc_OrganIdsForPrimaryYield = mergeYieldComponents(j["OrganIdsForPrimaryYield"]);
  else res.errors.push_back(string("Couldn't read 'OrganIdsForPrimaryYield' key from JSON object:\n") + j.dump());

  if (j.has_shape({{"OrganIdsForSecondaryYield", json11::Json::ARRAY}}, err))
    cp->pc_OrganIdsForSecondaryYield = mergeYieldComponents(j["OrganIdsForSecondaryYield"]);
  else res.errors.push_back(string("Couldn't read 'OrganIdsForSecondaryYield' key from JSON object:\n") + j.dump());

  if (j.has_shape({{"OrganIdsForCutting", json11::Json::ARRAY}}, err))
    cp->pc_OrganIdsForCutting = mergeYieldComponents(j["OrganIdsForCutting"]);
  else res.warnings.push_back(string("Couldn't read 'OrganIdsForCutting' key from JSON object:\n") + j.dump());

  set_string_value(cp->pc_CultivarId, j, "CultivarName");
  set_string_value(cp->pc_Description, j, "Description");
  set_bool_value(cp->pc_Perennial, j, "Perennial");
  set_double_value(cp->pc_MaxAssimilationRate, j, "MaxAssimilationRate");
  set_double_value(cp->pc_LightExtinctionCoefficient, j, "LightExtinctionCoefficient");
  set_double_value(cp->pc_MaxCropHeight, j, "MaxCropHeight");
  set_double_value(cp->pc_ResidueNRatio, j, "ResidueNRatio");
  set_double_value(cp->pc_LT50cultivar, j, "LT50cultivar");
  set_double_value(cp->pc_CropHeightP1, j, "CropHeightP1");
  set_double_value(cp->pc_CropHeightP2, j, "CropHeightP2");
  set_double_value(cp->pc_CropSpecificMaxRootingDepth, j, "CropSpecificMaxRootingDepth");
  set_double_vector(cp->pc_BaseDaylength, j, "BaseDaylength");
  set_double_vector(cp->pc_OptimumTemperature, j, "OptimumTemperature");
  set_double_vector(cp->pc_DaylengthRequirement, j, "DaylengthRequirement");
  set_double_vector(cp->pc_DroughtStressThreshold, j, "DroughtStressThreshold");
  set_double_vector(cp->pc_SpecificLeafArea, j, "SpecificLeafArea");
  set_double_vector(cp->pc_StageKcFactor, j, "StageKcFactor");
  set_double_vector(cp->pc_StageTemperatureSum, j, "StageTemperatureSum");
  set_double_vector(cp->pc_VernalisationRequirement, j, "VernalisationRequirement");
  set_double_value(cp->pc_HeatSumIrrigationStart, j, "HeatSumIrrigationStart");
  set_double_value(cp->pc_HeatSumIrrigationEnd, j, "HeatSumIrrigationEnd");
  set_double_value(cp->pc_CriticalTemperatureHeatStress, j, "CriticalTemperatureHeatStress");
  set_double_value(cp->pc_BeginSensitivePhaseHeatStress, j, "BeginSensitivePhaseHeatStress");
  set_double_value(cp->pc_EndSensitivePhaseHeatStress, j, "EndSensitivePhaseHeatStress");
  set_double_value(cp->pc_FrostHardening, j, "FrostHardening");
  set_double_value(cp->pc_FrostDehardening, j, "FrostDehardening");
  set_double_value(cp->pc_LowTemperatureExposure, j, "LowTemperatureExposure");
  set_double_value(cp->pc_RespiratoryStress, j, "RespiratoryStress");
  set_int_value(cp->pc_LatestHarvestDoy, j, "LatestHarvestDoy");
  set_bool_value(cp->winterCrop, j, "WinterCrop");

  if (j["AssimilatePartitioningCoeff"].is_array()) {
    auto apcs = j["AssimilatePartitioningCoeff"].array_items();
    int i = 0;
    cp->pc_AssimilatePartitioningCoeff.resize(apcs.size());
    for (auto js : apcs) cp->pc_AssimilatePartitioningCoeff[i++] = double_vector(js);
  }
  if (j["OrganSenescenceRate"].is_array()) {
    auto osrs = j["OrganSenescenceRate"].array_items();
    int i = 0;
    cp->pc_OrganSenescenceRate.resize(osrs.size());
    for (auto js : osrs) cp->pc_OrganSenescenceRate[i++] = double_vector(js);
  }

  set_double_value(cp->pc_EarlyRefLeafExp, j, "EarlyRefLeafExp");
  set_double_value(cp->pc_RefLeafExp, j, "RefLeafExp");

  set_double_value(cp->pc_MinTempDev_WE, j, "MinTempDev_WE");
  set_double_value(cp->pc_OptTempDev_WE, j, "OptTempDev_WE");
  set_double_value(cp->pc_MaxTempDev_WE, j, "MaxTempDev_WE");


  return res;
}

json11::Json cultivarparameters::to_json(const CultivarParameters* cp) {
  J11Array apcs;
  for (auto v : cp->pc_AssimilatePartitioningCoeff) apcs.push_back(toPrimJsonArray(v));

  J11Array osrs;
  for (auto v : cp->pc_OrganSenescenceRate) osrs.push_back(toPrimJsonArray(v));

  auto yieldComponentsToJson = [](const std::vector<YieldComponent>& ycs) {
    J11Array a;
    for (const auto& yc : ycs) a.push_back(yieldcomponent::to_json(&yc));
    return a;
  };

  auto cultivar = J11Object
  {
    {"type", "CultivarParameters"},
    {"CultivarName", cp->pc_CultivarId},
    {"Description", cp->pc_Description},
    {"Perennial", cp->pc_Perennial},
    {"MaxAssimilationRate", cp->pc_MaxAssimilationRate},
    {"LightExtinctionCoefficient", cp->pc_LightExtinctionCoefficient},
    {"MaxCropHeight", J11Array{cp->pc_MaxCropHeight, "m"}},
    {"ResidueNRatio", cp->pc_ResidueNRatio},
    {"LT50cultivar", cp->pc_LT50cultivar},
    {"CropHeightP1", cp->pc_CropHeightP1},
    {"CropHeightP2", cp->pc_CropHeightP2},
    {"CropSpecificMaxRootingDepth", cp->pc_CropSpecificMaxRootingDepth},
    {"AssimilatePartitioningCoeff", apcs},
    {"OrganSenescenceRate", osrs},
    {"BaseDaylength", J11Array{toPrimJsonArray(cp->pc_BaseDaylength), "h"}},
    {"OptimumTemperature", J11Array{toPrimJsonArray(cp->pc_OptimumTemperature), "°C"}},
    {"DaylengthRequirement", J11Array{toPrimJsonArray(cp->pc_DaylengthRequirement), "h"}},
    {"DroughtStressThreshold", toPrimJsonArray(cp->pc_DroughtStressThreshold)},
    {"SpecificLeafArea", J11Array{toPrimJsonArray(cp->pc_SpecificLeafArea), "ha kg-1"}},
    {"StageKcFactor", J11Array{toPrimJsonArray(cp->pc_StageKcFactor), "1;0"}},
    {"StageTemperatureSum", J11Array{toPrimJsonArray(cp->pc_StageTemperatureSum), "°C d"}},
    {"VernalisationRequirement", toPrimJsonArray(cp->pc_VernalisationRequirement)},
    {"HeatSumIrrigationStart", cp->pc_HeatSumIrrigationStart},
    {"HeatSumIrrigationEnd", cp->pc_HeatSumIrrigationEnd},
    {"CriticalTemperatureHeatStress", J11Array{cp->pc_CriticalTemperatureHeatStress, "°C"}},
    {"BeginSensitivePhaseHeatStress", J11Array{cp->pc_BeginSensitivePhaseHeatStress, "°C d"}},
    {"EndSensitivePhaseHeatStress", J11Array{cp->pc_EndSensitivePhaseHeatStress, "°C d"}},
    {"FrostHardening", cp->pc_FrostHardening},
    {"FrostDehardening", cp->pc_FrostDehardening},
    {"LowTemperatureExposure", cp->pc_LowTemperatureExposure},
    {"RespiratoryStress", cp->pc_RespiratoryStress},
    {"LatestHarvestDoy", cp->pc_LatestHarvestDoy},
    {"OrganIdsForPrimaryYield", yieldComponentsToJson(cp->pc_OrganIdsForPrimaryYield)},
    {"OrganIdsForSecondaryYield", yieldComponentsToJson(cp->pc_OrganIdsForSecondaryYield)},
    {"OrganIdsForCutting", yieldComponentsToJson(cp->pc_OrganIdsForCutting)},
    {"EarlyRefLeafExp", cp->pc_EarlyRefLeafExp},
    {"RefLeafExp", cp->pc_RefLeafExp},
    {"MinTempDev_WE", cp->pc_MinTempDev_WE},
    {"OptTempDev_WE", cp->pc_OptTempDev_WE},
    {"MaxTempDev_WE", cp->pc_MaxTempDev_WE},
    {"WinterCrop", cp->winterCrop}
  };

  return cultivar;
}


CropParameters monica::makeCropParameters(mas::schema::model::monica::CropParameters::Reader reader) {
  CropParameters cp;
  cropparameters::deserialize(&cp, reader);
  return cp;
}

void cropparameters::deserialize(CropParameters* cp, mas::schema::model::monica::CropParameters::Reader reader) {
  speciesparameters::deserialize(&cp->speciesParams, reader.getSpeciesParams());
  cultivarparameters::deserialize(&cp->cultivarParams, reader.getCultivarParams());
}

void cropparameters::serialize(const CropParameters* cp, mas::schema::model::monica::CropParameters::Builder builder) {
  speciesparameters::serialize(&cp->speciesParams, builder.initSpeciesParams());
  cultivarparameters::serialize(&cp->cultivarParams, builder.initCultivarParams());
}

Errors cropparameters::merge(CropParameters* cp, json11::Json j) {
  auto evff = j["__enable_vernalisation_factor_fix__"];
  if (!evff.is_null() && evff.is_bool()) cp->__enable_vernalisation_factor_fix__ = evff.bool_value();
  return merge(cp, j["species"], j["cultivar"]);
}

Errors cropparameters::merge(CropParameters* cp, json11::Json sj, json11::Json cj) {
  Errors res;
  res.append(speciesparameters::merge(&cp->speciesParams, sj));
  res.append(cultivarparameters::merge(&cp->cultivarParams, cj));
  return res;
}

json11::Json cropparameters::to_json(const CropParameters* cp) {
  return J11Object
  {
    {"type", "CropParameters"},
    {"species", speciesparameters::to_json(&cp->speciesParams)},
    {"cultivar", cultivarparameters::to_json(&cp->cultivarParams)}
  };
}


MineralFertilizerParameters monica::makeMineralFertilizerParameters(const string& id,
                                                                    const std::string& name,
                                                                    double carbamid,
                                                                    double no3,
                                                                    double nh4) {
  MineralFertilizerParameters fp;
  fp.id = id;
  fp.name = name;
  fp.vo_Carbamid = carbamid;
  fp.vo_NH4 = nh4;
  fp.vo_NO3 = no3;
  return fp;
}

MineralFertilizerParameters monica::makeMineralFertilizerParameters(
  mas::schema::model::monica::Params::MineralFertilization::Parameters::Reader reader) {
  MineralFertilizerParameters fp;
  mineralfertilizerparameters::deserialize(&fp, reader);
  return fp;
}

void mineralfertilizerparameters::deserialize(MineralFertilizerParameters* fp,
  mas::schema::model::monica::Params::MineralFertilization::Parameters::Reader reader) {
  fp->id = reader.getId();
  fp->name = reader.getName();
  fp->vo_Carbamid = reader.getCarbamid();
  fp->vo_NH4 = reader.getNh4();
  fp->vo_NO3 = reader.getNo3();
}

void mineralfertilizerparameters::serialize(const MineralFertilizerParameters* fp,
  mas::schema::model::monica::Params::MineralFertilization::Parameters::Builder builder) {
  builder.setId(fp->id);
  builder.setName(fp->name);
  builder.setCarbamid(fp->vo_Carbamid);
  builder.setNh4(fp->vo_NH4);
  builder.setNo3(fp->vo_NO3);
}

Errors mineralfertilizerparameters::merge(MineralFertilizerParameters* fp, json11::Json j) {
  Errors res = defaultMerge(j, [fp](json11::Json j2) { return merge(fp, j2); });

  set_string_value(fp->id, j, "id");
  set_string_value(fp->name, j, "name");
  set_double_value(fp->vo_Carbamid, j, "Carbamid");
  set_double_value(fp->vo_NH4, j, "NH4");
  set_double_value(fp->vo_NO3, j, "NO3");

  return res;
}

json11::Json mineralfertilizerparameters::to_json(const MineralFertilizerParameters* fp) {
  return J11Object
  {
    {"type", "MineralFertilizerParameters"},
    {"id", fp->id},
    {"name", fp->name},
    {"Carbamid", fp->vo_Carbamid},
    {"NH4", fp->vo_NH4},
    {"NO3", fp->vo_NO3}
  };
}

NMinApplicationParameters monica::makeNMinApplicationParameters(double min, double max, int delayInDays) {
  NMinApplicationParameters nap;
  nap.min = min;
  nap.max = max;
  nap.delayInDays = delayInDays;
  return nap;
}

NMinApplicationParameters monica::makeNMinApplicationParameters(
  mas::schema::model::monica::NMinApplicationParameters::Reader reader) {
  NMinApplicationParameters nap;
  nminapplicationparameters::deserialize(&nap, reader);
  return nap;
}

void nminapplicationparameters::deserialize(NMinApplicationParameters* nap,
  mas::schema::model::monica::NMinApplicationParameters::Reader reader) {
  nap->min = reader.getMin();
  nap->max = reader.getMax();
  nap->delayInDays = reader.getDelayInDays();
}

void nminapplicationparameters::serialize(const NMinApplicationParameters* nap,
  mas::schema::model::monica::NMinApplicationParameters::Builder builder) {
  builder.setMin(nap->min);
  builder.setMax(nap->max);
  builder.setDelayInDays(nap->delayInDays);
}

Errors nminapplicationparameters::merge(NMinApplicationParameters* nap, json11::Json j) {
  Errors res = defaultMerge(j, [nap](json11::Json j2) { return merge(nap, j2); });

  set_double_value(nap->min, j, "min");
  set_double_value(nap->max, j, "max");
  set_int_value(nap->delayInDays, j, "delayInDays");

  return res;
}

json11::Json nminapplicationparameters::to_json(const NMinApplicationParameters* nap) {
  return json11::Json::object
  {
    {"type", "NMinApplicationParameters"},
    {"min", nap->min},
    {"max", nap->max},
    {"delayInDays", nap->delayInDays}
  };
}


IrrigationParameters monica::makeIrrigationParameters(double nitrateConcentration, double sulfateConcentration) {
  IrrigationParameters ip;
  ip.nitrateConcentration = nitrateConcentration;
  ip.sulfateConcentration = sulfateConcentration;
  return ip;
}

IrrigationParameters monica::makeIrrigationParameters(
  mas::schema::model::monica::Params::Irrigation::Parameters::Reader reader) {
  IrrigationParameters ip;
  irrigationparameters::deserialize(&ip, reader);
  return ip;
}

void irrigationparameters::deserialize(IrrigationParameters* ip,
  mas::schema::model::monica::Params::Irrigation::Parameters::Reader reader) {
  ip->nitrateConcentration = reader.getNitrateConcentration();
  ip->sulfateConcentration = reader.getSulfateConcentration();
}

void irrigationparameters::serialize(const IrrigationParameters* ip,
  mas::schema::model::monica::Params::Irrigation::Parameters::Builder builder) {
  builder.setNitrateConcentration(ip->nitrateConcentration);
  builder.setSulfateConcentration(ip->sulfateConcentration);
}

Errors irrigationparameters::merge(IrrigationParameters* ip, json11::Json j) {
  Errors res = defaultMerge(j, [ip](json11::Json j2) { return merge(ip, j2); });

  set_double_value(ip->nitrateConcentration, j, "nitrateConcentration");
  set_double_value(ip->sulfateConcentration, j, "sulfateConcentration");
  set_bool_value(ip->isDripIrrigation, j, "isDripIrrigation");
  if (j["fw"].is_number()) ip->fw = std::max(0.0, std::min(1.0, j["fw"].number_value()));

  return res;
}

json11::Json irrigationparameters::to_json(const IrrigationParameters* ip) {
  return json11::Json::object
  {
    {"type", "IrrigationParameters"},
    {"nitrateConcentration", J11Array{ip->nitrateConcentration, "mg dm-3"}},
    {"sulfateConcentration", J11Array{ip->sulfateConcentration, "mg dm-3"}},
    {"isDripIrrigation", ip->isDripIrrigation},
    {"fw", ip->fw}
  };
}


AutomaticIrrigationParameters monica::makeAutomaticIrrigationParameters(double a, double t, double nc, double sc) {
  AutomaticIrrigationParameters aip;
  aip.nitrateConcentration = nc;
  aip.sulfateConcentration = sc;
  aip.amount = a;
  aip.threshold = t;
  return aip;
}

AutomaticIrrigationParameters monica::makeAutomaticIrrigationParameters(
  mas::schema::model::monica::AutomaticIrrigationParameters::Reader reader) {
  AutomaticIrrigationParameters aip;
  automaticirrigationparameters::deserialize(&aip, reader);
  return aip;
}

void automaticirrigationparameters::deserialize(AutomaticIrrigationParameters* aip,
  mas::schema::model::monica::AutomaticIrrigationParameters::Reader reader) {
  irrigationparameters::deserialize(aip, reader.getParams());
  aip->amount = reader.getAmount();
  aip->threshold = reader.getThreshold();
  //percentNFC = reader.getPercentNfc();
}

void automaticirrigationparameters::serialize(const AutomaticIrrigationParameters* aip,
  mas::schema::model::monica::AutomaticIrrigationParameters::Builder builder) {
  irrigationparameters::serialize(aip, builder.initParams());
  builder.setAmount(aip->amount);
  builder.setThreshold(aip->threshold);
  //builder.setPercentNfc(percentNFC);
}

Errors automaticirrigationparameters::merge(AutomaticIrrigationParameters* aip, json11::Json j) {
  Errors res = defaultMerge(j, [aip](json11::Json j2) { return merge(aip, j2); });

  res.append(irrigationparameters::merge(aip, j["irrigationParameters"]));
  set_iso_date_value(aip->startDate, j, "startDate");
  set_iso_date_value(aip->endDate, j, "stopDate");
  set_double_value(aip->amount, j, "amount");
  set_double_value(aip->percentNFC, j, "set_to_%nFC");
  set_double_value(aip->threshold, j, "threshold", transformIfPercent(j, "threshold"));
  set_double_value(aip->threshold, j, "trigger_if_nFC_below_%", [](double v) { return v / 100.0; });
  set_double_value(aip->criticalMoistureDepthM, j, "calc_nFC_until_depth_m",
                   transformIfNotMeters(j, "calc_nFC_until_depth_m"));
  set_int_value(aip->minDaysBetweenIrrigationEvents, j, "minDaysBetweenIrrigationEvents");

  return res;
}

json11::Json automaticirrigationparameters::to_json(const AutomaticIrrigationParameters* aip) {
  auto o = json11::Json::object
  {
    {"type", "AutomaticIrrigationParameters"},
    {"startDate", aip->startDate.toIsoDateString()},
    {"irrigationParameters", irrigationparameters::to_json(aip)},
    {"trigger_if_nFC_below_%", J11Array{aip->threshold * 100.0, "%"}},
    {"calc_nFC_until_depth_m", J11Array{aip->criticalMoistureDepthM, "m"}},
    {"minDaysBetweenIrrigationEvents", J11Array{aip->minDaysBetweenIrrigationEvents, "d"}}
  };
  if (aip->amount > 0) o["amount"] = J11Array{aip->amount, "mm"};
  else o["set_to_%nFC"] = J11Array{aip->percentNFC, "%"};
  return o;
}

MeasuredGroundwaterTableInformation monica::makeMeasuredGroundwaterTableInformation(
  mas::schema::model::monica::MeasuredGroundwaterTableInformation::Reader reader) {
  MeasuredGroundwaterTableInformation gwi;
  measuredgroundwatertableinformation::deserialize(&gwi, reader);
  return gwi;
}

void measuredgroundwatertableinformation::deserialize(MeasuredGroundwaterTableInformation* gwi,
  mas::schema::model::monica::MeasuredGroundwaterTableInformation::Reader reader) {
  gwi->groundwaterInformationAvailable = reader.getGroundwaterInformationAvailable();
  gwi->groundwaterInfo.clear();
  for (auto gi : reader.getGroundwaterInfo()) gwi->groundwaterInfo[Date(gi.getDate())] = gi.getValue();
}

void measuredgroundwatertableinformation::serialize(const MeasuredGroundwaterTableInformation* gwi,
  mas::schema::model::monica::MeasuredGroundwaterTableInformation::Builder builder) {
  builder.setGroundwaterInformationAvailable(gwi->groundwaterInformationAvailable);
  auto gis = builder.initGroundwaterInfo((capnp::uint)gwi->groundwaterInfo.size());
  capnp::uint i = 0;
  for (auto p : gwi->groundwaterInfo) {
    p.first.serialize(gis[i].initDate());
    gis[i].setValue(p.second);
  }
}

Errors measuredgroundwatertableinformation::merge(MeasuredGroundwaterTableInformation* gwi, json11::Json j) {
  Errors res;

  set_bool_value(gwi->groundwaterInformationAvailable, j, "groundwaterInformationAvailable");

  string err = "";
  if (j.has_shape({{"groundwaterInfo", json11::Json::OBJECT}}, err))
    for (auto p : j["groundwaterInfo"].
         object_items())
      gwi->groundwaterInfo[Tools::Date::fromIsoDateString(p.first)] = p.second.number_value();
  else res.errors.push_back(string("Couldn't read 'groundwaterInfo' key from JSON object:\n") + j.dump());

  return res;
}

json11::Json measuredgroundwatertableinformation::to_json(const MeasuredGroundwaterTableInformation* gwi) {
  json11::Json::object gi;
  for (auto p : gwi->groundwaterInfo) gi[p.first.toIsoDateString()] = p.second;

  return json11::Json::object
  {
    {"type", "MeasuredGroundwaterTableInformation"},
    {"groundwaterInformationAvailable", gwi->groundwaterInformationAvailable},
    {"groundwaterInfo", gi}
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

std::pair<bool, double> measuredgroundwatertableinformation::getGroundwaterInformation(
  const MeasuredGroundwaterTableInformation* gwi, Tools::Date gwDate) {
  if (gwi->groundwaterInformationAvailable && !gwi->groundwaterInfo.empty()) {
    auto it = gwi->groundwaterInfo.find(gwDate);
    if (it != gwi->groundwaterInfo.end()) return make_pair(true, it->second);
  }
  return make_pair(false, 0);
}

//std::map<std::string, std::function<Tools::Errors(Soil::SoilParameters*)>>
//SiteParameters::calculateAndSetPwpFcSatFunctions = std::map<std::string, std::function<Tools::Errors(Soil::SoilParameters*)>>();

SiteParameters monica::makeSiteParameters(mas::schema::model::monica::SiteParameters::Reader reader) {
  SiteParameters sp;
  siteparameters::deserialize(&sp, reader);
  return sp;
}

void siteparameters::deserialize(SiteParameters* sp, mas::schema::model::monica::SiteParameters::Reader reader) {
  sp->vs_Latitude = reader.getLatitude();
  sp->vs_Slope = reader.getSlope();
  sp->vs_HeightNN = reader.getHeightNN();
  sp->vs_GroundwaterDepth = reader.getGroundwaterDepth();
  sp->vs_Soil_CN_Ratio = reader.getSoilCNRatio();
  sp->vs_DrainageCoeff = reader.getDrainageCoeff();
  sp->vq_NDeposition = reader.getVqNDeposition();
  sp->vs_MaxEffectiveRootingDepth = reader.getMaxEffectiveRootingDepth();
  sp->vs_ImpenetrableLayerDepth = reader.getImpenetrableLayerDepth();
  sp->vs_SoilSpecificHumusBalanceCorrection = reader.getSoilSpecificHumusBalanceCorrection();
  setFromComplexCapnpList(sp->vs_SoilParameters, reader.getSoilParameters());
}

void siteparameters::serialize(const SiteParameters* sp, mas::schema::model::monica::SiteParameters::Builder builder) {
  builder.setLatitude(sp->vs_Latitude);
  builder.setSlope(sp->vs_Slope);
  builder.setHeightNN(sp->vs_HeightNN);
  builder.setGroundwaterDepth(sp->vs_GroundwaterDepth);
  builder.setSoilCNRatio(sp->vs_Soil_CN_Ratio);
  builder.setDrainageCoeff(sp->vs_DrainageCoeff);
  builder.setVqNDeposition(sp->vq_NDeposition);
  builder.setMaxEffectiveRootingDepth(sp->vs_MaxEffectiveRootingDepth);
  builder.setImpenetrableLayerDepth(sp->vs_ImpenetrableLayerDepth);
  builder.setSoilSpecificHumusBalanceCorrection(sp->vs_SoilSpecificHumusBalanceCorrection);
  setComplexCapnpList(sp->vs_SoilParameters, builder.initSoilParameters((capnp::uint)sp->vs_SoilParameters.size()));
}

Errors siteparameters::merge(SiteParameters* sp, json11::Json j) {
  Errors res = defaultMerge(j, [sp](json11::Json j2) { return merge(sp, j2); });

  string err;
  set_double_value(sp->vs_Latitude, j, "Latitude");
  set_double_value(sp->vs_Slope, j, "Slope");
  set_double_value(sp->vs_HeightNN, j, "HeightNN");
  set_double_value(sp->vs_GroundwaterDepth, j, "GroundwaterDepth");
  set_double_value(sp->vs_Soil_CN_Ratio, j, "Soil_CN_Ratio");
  set_double_value(sp->vs_DrainageCoeff, j, "DrainageCoeff");
  set_double_value(sp->vq_NDeposition, j, "NDeposition");
  set_double_value(sp->vs_MaxEffectiveRootingDepth, j, "MaxEffectiveRootingDepth");
  set_double_value(sp->vs_ImpenetrableLayerDepth, j, "ImpenetrableLayerDepth");
  set_double_value(sp->vs_SoilSpecificHumusBalanceCorrection, j, "SoilSpecificHumusBalanceCorrection");
  set_double_value(sp->bareSoilKcFactor, j, "Bare_soil_KC_factor");
  set_string_value(sp->pwpFcSatFunction, j, "pwpFcSatFunction");

  set_int_value(sp->numberOfLayers, j, "NumberOfLayers");
  set_double_value(sp->layerThickness, j, "LayerThickness");

  std::function selectedSetPwpFcSatFunction = noSetPwpFcSat;
  if (const auto it = sp->calculateAndSetPwpFcSatFunctions.find(sp->pwpFcSatFunction);
    it != sp->calculateAndSetPwpFcSatFunctions.end()) {
    selectedSetPwpFcSatFunction = it->second;
  } else {
    res.warnings.push_back("Couldn't find pwpFcSatFunction: " + sp->pwpFcSatFunction);
  }

  if (j.has_shape({{"SoilProfileParameters", json11::Json::ARRAY}}, err)) {
    sp->initSoilProfileSpec = j["SoilProfileParameters"].array_items();
    auto r = createEqualSizedSoilPMs(selectedSetPwpFcSatFunction, sp->initSoilProfileSpec, sp->layerThickness, sp->numberOfLayers);
    if (r.success()) {
      sp->vs_SoilParameters = kj::mv(r.result);
      if (sp->vs_SoilParameters.empty()) res.appendError("Soil profile is empty!");
    } else res.append(r.errors);
  } else if (j["SoilProfileParameters"].is_string() && j["SoilProfileParameters"].string_value().find("capnp") != 0) {
    res.errors.push_back(string("Couldn't read 'SoilProfileParameters' JSON array from JSON object:\n") + j.dump());
  }

  //if (!j["groundwaterInformation"].is_null()) {
  //  res.append(groundwaterInformation.merge(j["groundwaterInformation"]));
  //}

  return res;
}

json11::Json siteparameters::to_json(const SiteParameters* sp) {
  auto sps = J11Object
  {
    {"type", "SiteParameters"},
    {"Latitude", J11Array{sp->vs_Latitude, "", "latitude in decimal degrees"}},
    {"Slope", J11Array{sp->vs_Slope, "m m-1"}},
    {"HeightNN", J11Array{sp->vs_HeightNN, "m", "height above sea level"}},
    {"GroundwaterDepth", J11Array{sp->vs_GroundwaterDepth, "m"}},
    {"Soil_CN_Ratio", sp->vs_Soil_CN_Ratio},
    {"DrainageCoeff", sp->vs_DrainageCoeff},
    {"NDeposition", J11Array{sp->vq_NDeposition, "kg N ha-1 y-1"}},
    {"MaxEffectiveRootingDepth", J11Array{sp->vs_MaxEffectiveRootingDepth, "m"}},
    {"ImpenetrableLayerDepth", J11Array{sp->vs_ImpenetrableLayerDepth, "m"}},
    {"SoilSpecificHumusBalanceCorrection", J11Array{sp->vs_SoilSpecificHumusBalanceCorrection, "humus equivalents"}},
    {"Bare_soil_KC_factor", sp->bareSoilKcFactor}
  };

  sps["SoilProfileParameters"] = toJsonArray(sp->vs_SoilParameters);

  return sps;
}


AutomaticHarvestParameters monica::makeAutomaticHarvestParameters(AutomaticHarvestParameters::HarvestTime yt) {
  AutomaticHarvestParameters ahp;
  ahp._harvestTime = yt;
  return ahp;
}

AutomaticHarvestParameters monica::makeAutomaticHarvestParameters(
  mas::schema::model::monica::AutomaticHarvestParameters::Reader reader) {
  AutomaticHarvestParameters ahp;
  automaticharvestparameters::deserialize(&ahp, reader);
  return ahp;
}

void automaticharvestparameters::deserialize(AutomaticHarvestParameters* ahp,
  mas::schema::model::monica::AutomaticHarvestParameters::Reader reader) {
  typedef mas::schema::model::monica::AutomaticHarvestParameters::HarvestTime HT;
  ahp->_harvestTime = reader.getHarvestTime() == HT::MATURITY ? AutomaticHarvestParameters::maturity : AutomaticHarvestParameters::unknown;
  ahp->_latestHarvestDOY = reader.getLatestHarvestDOY();
}

void automaticharvestparameters::serialize(const AutomaticHarvestParameters* ahp,
  mas::schema::model::monica::AutomaticHarvestParameters::Builder builder) {
  typedef mas::schema::model::monica::AutomaticHarvestParameters::HarvestTime HT;
  builder.setHarvestTime(ahp->_harvestTime == AutomaticHarvestParameters::maturity ? HT::MATURITY : HT::UNKNOWN);
  builder.setLatestHarvestDOY(ahp->_latestHarvestDOY);
}

Errors automaticharvestparameters::merge(AutomaticHarvestParameters* ahp, json11::Json j) {
  Errors res = defaultMerge(j, [ahp](json11::Json j2) { return merge(ahp, j2); });

  int ht = -1;
  set_int_value(ht, j, "harvestTime");
  if (ht > -1) ahp->_harvestTime = AutomaticHarvestParameters::HarvestTime(ht);
  set_int_value(ahp->_latestHarvestDOY, j, "latestHarvestDOY");

  return res;
}

json11::Json automaticharvestparameters::to_json(const AutomaticHarvestParameters* ahp) {
  return J11Object
  {
    {"harvestTime", int(ahp->_harvestTime)},
    {"latestHavestDOY", ahp->_latestHarvestDOY}
  };
}


NMinCropParameters monica::makeNMinCropParameters(double samplingDepth, double nTarget, double nTarget30) {
  NMinCropParameters ncp;
  ncp.samplingDepth = samplingDepth;
  ncp.nTarget = nTarget;
  ncp.nTarget30 = nTarget30;
  return ncp;
}

NMinCropParameters monica::makeNMinCropParameters(mas::schema::model::monica::NMinCropParameters::Reader reader) {
  NMinCropParameters ncp;
  nmincropparameters::deserialize(&ncp, reader);
  return ncp;
}

void nmincropparameters::deserialize(NMinCropParameters* ncp, mas::schema::model::monica::NMinCropParameters::Reader reader) {
  ncp->samplingDepth = reader.getSamplingDepth();
  ncp->nTarget = reader.getNTarget();
  ncp->nTarget30 = reader.getNTarget30();
}

void nmincropparameters::serialize(const NMinCropParameters* ncp, mas::schema::model::monica::NMinCropParameters::Builder builder) {
  builder.setSamplingDepth(ncp->samplingDepth);
  builder.setNTarget(ncp->nTarget);
  builder.setNTarget30(ncp->nTarget30);
}

Errors nmincropparameters::merge(NMinCropParameters* ncp, json11::Json j) {
  Errors res = defaultMerge(j, [ncp](json11::Json j2) { return merge(ncp, j2); });

  set_double_value(ncp->samplingDepth, j, "samplingDepth");
  set_double_value(ncp->nTarget, j, "nTarget");
  set_double_value(ncp->nTarget30, j, "nTarget30");

  return res;
}

json11::Json nmincropparameters::to_json(const NMinCropParameters* ncp) {
  return json11::Json::object
  {
    {"type", "NMinCropParameters"},
    {"samplingDepth", ncp->samplingDepth},
    {"nTarget", ncp->nTarget},
    {"nTarget30", ncp->nTarget30}
  };
}


OrganicMatterParameters monica::makeOrganicMatterParameters(
  mas::schema::model::monica::Params::OrganicFertilization::OrganicMatterParameters::Reader reader) {
  OrganicMatterParameters omp;
  organicmatterparameters::deserialize(&omp, reader);
  return omp;
}

void organicmatterparameters::deserialize(OrganicMatterParameters* omp,
  mas::schema::model::monica::Params::OrganicFertilization::OrganicMatterParameters::Reader reader) {
  omp->vo_AOM_DryMatterContent = reader.getAomDryMatterContent();
  omp->vo_AOM_NH4Content = reader.getAomNH4Content();
  omp->vo_AOM_NO3Content = reader.getAomNO3Content();
  omp->vo_AOM_CarbamidContent = reader.getAomCarbamidContent();
  omp->vo_AOM_SlowDecCoeffStandard = reader.getAomSlowDecCoeffStandard();
  omp->vo_AOM_FastDecCoeffStandard = reader.getAomFastDecCoeffStandard();
  omp->vo_PartAOM_to_AOM_Slow = reader.getPartAOMToAOMSlow();
  omp->vo_PartAOM_to_AOM_Fast = reader.getPartAOMToAOMFast();
  omp->vo_CN_Ratio_AOM_Slow = reader.getCnRatioAOMSlow();
  omp->vo_CN_Ratio_AOM_Fast = reader.getCnRatioAOMFast();
  omp->vo_PartAOM_Slow_to_SMB_Slow = reader.getPartAOMSlowToSMBSlow();
  omp->vo_PartAOM_Slow_to_SMB_Fast = reader.getPartAOMSlowToSMBFast();
  omp->vo_NConcentration = reader.getNConcentration();
  //vo_CorgContent = reader.getCorgContent();
}

void organicmatterparameters::serialize(const OrganicMatterParameters* omp,
  mas::schema::model::monica::Params::OrganicFertilization::OrganicMatterParameters::Builder builder) {
  builder.setAomDryMatterContent(omp->vo_AOM_DryMatterContent);
  builder.setAomNH4Content(omp->vo_AOM_NH4Content);
  builder.setAomNO3Content(omp->vo_AOM_NO3Content);
  builder.setAomCarbamidContent(omp->vo_AOM_CarbamidContent);
  builder.setAomSlowDecCoeffStandard(omp->vo_AOM_SlowDecCoeffStandard);
  builder.setAomFastDecCoeffStandard(omp->vo_AOM_FastDecCoeffStandard);
  builder.setPartAOMToAOMSlow(omp->vo_PartAOM_to_AOM_Slow);
  builder.setPartAOMToAOMFast(omp->vo_PartAOM_to_AOM_Fast);
  builder.setCnRatioAOMSlow(omp->vo_CN_Ratio_AOM_Slow);
  builder.setCnRatioAOMFast(omp->vo_CN_Ratio_AOM_Fast);
  builder.setPartAOMSlowToSMBSlow(omp->vo_PartAOM_Slow_to_SMB_Slow);
  builder.setPartAOMSlowToSMBFast(omp->vo_PartAOM_Slow_to_SMB_Fast);
  builder.setNConcentration(omp->vo_NConcentration);
  //builder.setCorgContent(vo_CorgContent);
}

Errors organicmatterparameters::merge(OrganicMatterParameters* omp, json11::Json j) {
  Errors res = defaultMerge(j, [omp](json11::Json j2) { return merge(omp, j2); });

  set_double_value(omp->vo_AOM_DryMatterContent, j, "AOM_DryMatterContent");
  set_double_value(omp->vo_AOM_NH4Content, j, "AOM_NH4Content");
  set_double_value(omp->vo_AOM_NO3Content, j, "AOM_NO3Content");
  set_double_value(omp->vo_AOM_CarbamidContent, j, "AOM_CarbamidContent");
  set_double_value(omp->vo_AOM_SlowDecCoeffStandard, j, "AOM_SlowDecCoeffStandard");
  set_double_value(omp->vo_AOM_FastDecCoeffStandard, j, "AOM_FastDecCoeffStandard");
  set_double_value(omp->vo_PartAOM_to_AOM_Slow, j, "PartAOM_to_AOM_Slow");
  set_double_value(omp->vo_PartAOM_to_AOM_Fast, j, "PartAOM_to_AOM_Fast");
  set_double_value(omp->vo_CN_Ratio_AOM_Slow, j, "CN_Ratio_AOM_Slow");
  set_double_value(omp->vo_CN_Ratio_AOM_Fast, j, "CN_Ratio_AOM_Fast");
  set_double_value(omp->vo_PartAOM_Slow_to_SMB_Slow, j, "PartAOM_Slow_to_SMB_Slow");
  set_double_value(omp->vo_PartAOM_Slow_to_SMB_Fast, j, "PartAOM_Slow_to_SMB_Fast");
  set_double_value(omp->vo_NConcentration, j, "NConcentration");
  set_double_value(omp->vo_CorgContent, j, "CorgContent");

  return res;
}

json11::Json organicmatterparameters::to_json(const OrganicMatterParameters* omp) {
  return J11Object
  {
    {"type", "OrganicMatterParameters"},
    {
      "AOM_DryMatterContent",
      J11Array{
        omp->vo_AOM_DryMatterContent,
        "kg DM kg FM-1",
        "Dry matter content of added organic matter"
      }
    },
    {
      "AOM_NH4Content",
      J11Array{
        omp->vo_AOM_NH4Content,
        "kg N kg DM-1",
        "Ammonium content in added organic matter"
      }
    },
    {
      "AOM_NO3Content",
      J11Array{
        omp->vo_AOM_NO3Content,
        "kg N kg DM-1",
        "Nitrate content in added organic matter"
      }
    },
    {
      "AOM_NO3Content",
      J11Array{
        omp->vo_AOM_NO3Content,
        "kg N kg DM-1",
        "Carbamide content in added organic matter"
      }
    },
    {
      "AOM_SlowDecCoeffStandard",
      J11Array{
        omp->vo_AOM_SlowDecCoeffStandard,
        "d-1",
        "Decomposition rate coefficient of slow AOM at standard conditions"
      }
    },
    {
      "AOM_FastDecCoeffStandard",
      J11Array{
        omp->vo_AOM_FastDecCoeffStandard,
        "d-1",
        "Decomposition rate coefficient of fast AOM at standard conditions"
      }
    },
    {
      "PartAOM_to_AOM_Slow",
      J11Array{
        omp->vo_PartAOM_to_AOM_Slow,
        "kg kg-1",
        "Part of AOM that is assigned to the slowly decomposing pool"
      }
    },
    {
      "PartAOM_to_AOM_Fast",
      J11Array{
        omp->vo_PartAOM_to_AOM_Fast,
        "kg kg-1",
        "Part of AOM that is assigned to the rapidly decomposing pool"
      }
    },
    {
      "CN_Ratio_AOM_Slow",
      J11Array{
        omp->vo_CN_Ratio_AOM_Slow,
        "",
        "C to N ratio of the slowly decomposing AOM pool"
      }
    },
    {
      "CN_Ratio_AOM_Fast",
      J11Array{
        omp->vo_CN_Ratio_AOM_Fast,
        "",
        "C to N ratio of the rapidly decomposing AOM pool"
      }
    },
    {
      "PartAOM_Slow_to_SMB_Slow",
      J11Array{
        omp->vo_PartAOM_Slow_to_SMB_Slow,
        "kg kg-1",
        "Part of AOM slow consumed by slow soil microbial biomass"
      }
    },
    {
      "PartAOM_Slow_to_SMB_Fast",
      J11Array{
        omp->vo_PartAOM_Slow_to_SMB_Fast,
        "kg kg-1",
        "Part of AOM slow consumed by fast soil microbial biomass"
      }
    },
    {
      "NConcentration",
      J11Array{
        omp->vo_NConcentration,
        "kg N kg DM-1",
        "Nitrogen content in added organic matter"
      }
    },
    {"CorgContent", J11Array{omp->vo_CorgContent, "kg C kg DM-1", "Carbon content in added organic matter"}}
  };
}

OrganicFertilizerParameters monica::makeOrganicFertilizerParameters(
  mas::schema::model::monica::Params::OrganicFertilization::Parameters::Reader reader) {
  OrganicFertilizerParameters ofp;
  organicfertilizerparameters::deserialize(&ofp, reader);
  return ofp;
}

void organicfertilizerparameters::deserialize(OrganicFertilizerParameters* ofp,
  mas::schema::model::monica::Params::OrganicFertilization::Parameters::Reader reader) {
  organicmatterparameters::deserialize(ofp, reader.getParams());
  ofp->id = reader.getId();
  ofp->name = reader.getName();
}

void organicfertilizerparameters::serialize(const OrganicFertilizerParameters* ofp,
  mas::schema::model::monica::Params::OrganicFertilization::Parameters::Builder builder) {
  organicmatterparameters::serialize(ofp, builder.initParams());
  builder.setId(ofp->id);
  builder.setName(ofp->name);
}

Errors organicfertilizerparameters::merge(OrganicFertilizerParameters* ofp, json11::Json j) {
  Errors res = defaultMerge(j, [ofp](json11::Json j2) { return merge(ofp, j2); });

  res.append(organicmatterparameters::merge(ofp, j));

  set_string_value(ofp->id, j, "id");
  set_string_value(ofp->name, j, "name");

  return res;
}

json11::Json organicfertilizerparameters::to_json(const OrganicFertilizerParameters* ofp) {
  auto omp = organicmatterparameters::to_json(ofp).object_items();
  omp["type"] = "OrganicFertilizerParameters";
  omp["id"] = ofp->id;
  omp["name"] = ofp->name;
  return omp;
}

CropResidueParameters monica::makeCropResidueParameters(
  mas::schema::model::monica::CropResidueParameters::Reader reader) {
  CropResidueParameters crp;
  cropresidueparameters::deserialize(&crp, reader);
  return crp;
}

void cropresidueparameters::deserialize(CropResidueParameters* crp,
  mas::schema::model::monica::CropResidueParameters::Reader reader) {
  organicmatterparameters::deserialize(crp, reader.getParams());
  crp->species = reader.getSpecies();
  crp->residueType = reader.getResidueType();
}

void cropresidueparameters::serialize(const CropResidueParameters* crp,
  mas::schema::model::monica::CropResidueParameters::Builder builder) {
  organicmatterparameters::serialize(crp, builder.initParams());
  builder.setSpecies(crp->species);
  builder.setResidueType(crp->residueType);
}

Errors cropresidueparameters::merge(CropResidueParameters* crp, json11::Json j) {
  Errors res = defaultMerge(j, [crp](json11::Json j2) { return merge(crp, j2); });

  res.append(organicmatterparameters::merge(crp, j));
  set_string_value(crp->species, j, "species");
  set_string_value(crp->residueType, j, "residueType");

  return res;
}

json11::Json cropresidueparameters::to_json(const CropResidueParameters* crp) {
  auto omp = organicmatterparameters::to_json(crp).object_items();
  omp["type"] = "CropResidueParameters";
  omp["species"] = crp->species;
  omp["residueType"] = crp->residueType;
  return omp;
}

SimulationParameters monica::makeSimulationParameters(mas::schema::model::monica::SimulationParameters::Reader reader) {
  SimulationParameters sp;
  simulationparameters::deserialize(&sp, reader);
  return sp;
}

void simulationparameters::deserialize(SimulationParameters* sp, mas::schema::model::monica::SimulationParameters::Reader reader) {
  sp->startDate.deserialize(reader.getStartDate());
  sp->endDate.deserialize(reader.getEndDate());

  sp->pc_NitrogenResponseOn = reader.getNitrogenResponseOn();
  sp->pc_WaterDeficitResponseOn = reader.getWaterDeficitResponseOn();
  sp->pc_EmergenceFloodingControlOn = reader.getEmergenceFloodingControlOn();
  sp->pc_EmergenceMoistureControlOn = reader.getEmergenceMoistureControlOn();
  sp->pc_FrostKillOn = reader.getFrostKillOn();

  sp->p_UseAutomaticIrrigation = reader.getUseAutomaticIrrigation();
  automaticirrigationparameters::deserialize(&sp->p_AutoIrrigationParams, reader.getAutoIrrigationParams());

  sp->p_UseNMinMineralFertilisingMethod = reader.getUseNMinMineralFertilisingMethod();
  mineralfertilizerparameters::deserialize(&sp->p_NMinFertiliserPartition, reader.getNMinFertiliserPartition());
  nminapplicationparameters::deserialize(&sp->p_NMinUserParams, reader.getNMinApplicationParams());

  sp->p_UseSecondaryYields = reader.getUseSecondaryYields();
  sp->p_UseAutomaticHarvestTrigger = reader.getUseAutomaticHarvestTrigger();

  sp->p_NumberOfLayers = reader.getNumberOfLayers();
  sp->p_LayerThickness = reader.getLayerThickness();

  sp->p_StartPVIndex = reader.getStartPVIndex();
  sp->p_JulianDayAutomaticFertilising = reader.getJulianDayAutomaticFertilising();
}

void simulationparameters::serialize(const SimulationParameters* sp, mas::schema::model::monica::SimulationParameters::Builder builder) {
  sp->startDate.serialize(builder.initStartDate());
  sp->endDate.serialize(builder.initEndDate());

  builder.setNitrogenResponseOn(sp->pc_NitrogenResponseOn);
  builder.setWaterDeficitResponseOn(sp->pc_WaterDeficitResponseOn);
  builder.setEmergenceFloodingControlOn(sp->pc_EmergenceFloodingControlOn);
  builder.setEmergenceMoistureControlOn(sp->pc_EmergenceMoistureControlOn);
  builder.setFrostKillOn(sp->pc_FrostKillOn);

  builder.setUseAutomaticIrrigation(sp->p_UseAutomaticIrrigation);
  automaticirrigationparameters::serialize(&sp->p_AutoIrrigationParams, builder.initAutoIrrigationParams());

  builder.setUseNMinMineralFertilisingMethod(sp->p_UseNMinMineralFertilisingMethod);
  mineralfertilizerparameters::serialize(&sp->p_NMinFertiliserPartition, builder.initNMinFertiliserPartition());
  nminapplicationparameters::serialize(&sp->p_NMinUserParams, builder.initNMinApplicationParams());

  builder.setUseSecondaryYields(sp->p_UseSecondaryYields);
  builder.setUseAutomaticHarvestTrigger(sp->p_UseAutomaticHarvestTrigger);

  builder.setNumberOfLayers(sp->p_NumberOfLayers);
  builder.setLayerThickness(sp->p_LayerThickness);

  builder.setStartPVIndex(sp->p_StartPVIndex);
  builder.setJulianDayAutomaticFertilising(sp->p_JulianDayAutomaticFertilising);
}

// SimulationParameters::SimulationParameters(json11::Json j) {
//   merge(j);
// }

Errors simulationparameters::merge(SimulationParameters* sp, json11::Json j) {
  Errors res = defaultMerge(j, [sp](json11::Json j2) { return merge(sp, j2); });

  set_iso_date_value(sp->startDate, j, "startDate");
  set_iso_date_value(sp->endDate, j, "endDate");

  set_bool_value(sp->pc_NitrogenResponseOn, j, "NitrogenResponseOn");
  set_bool_value(sp->pc_WaterDeficitResponseOn, j, "WaterDeficitResponseOn");
  set_bool_value(sp->pc_EmergenceFloodingControlOn, j, "EmergenceFloodingControlOn");
  set_bool_value(sp->pc_EmergenceMoistureControlOn, j, "EmergenceMoistureControlOn");
  set_bool_value(sp->pc_FrostKillOn, j, "FrostKillOn");

  set_bool_value(sp->p_UseAutomaticIrrigation, j, "UseAutomaticIrrigation");
  automaticirrigationparameters::merge(&sp->p_AutoIrrigationParams, j["AutoIrrigationParams"]);

  set_bool_value(sp->p_UseNMinMineralFertilisingMethod, j, "UseNMinMineralFertilisingMethod");
  mineralfertilizerparameters::merge(&sp->p_NMinFertiliserPartition, j["NMinFertiliserPartition"]);
  nminapplicationparameters::merge(&sp->p_NMinUserParams, j["NMinUserParams"]);
  set_int_value(sp->p_JulianDayAutomaticFertilising, j, "JulianDayAutomaticFertilising");

  set_bool_value(sp->p_UseSecondaryYields, j, "UseSecondaryYields");
  set_bool_value(sp->p_UseAutomaticHarvestTrigger, j, "UseAutomaticHarvestTrigger");
  set_int_value(sp->p_NumberOfLayers, j, "NumberOfLayers");
  set_double_value(sp->p_LayerThickness, j, "LayerThickness");

  set_int_value(sp->p_StartPVIndex, j, "StartPVIndex");

  if (auto serState = j["serializedMonicaState"].object_items(); !serState.empty()) {
    if (const auto loadState = serState["load"]; loadState.is_object()) {
      set_bool_value(sp->loadSerializedMonicaStateAtStart, loadState, "atStart");
      set_bool_value(sp->deserializedMonicaStateFromJson, loadState, "fromJson");
      set_string_value(sp->pathToLoadSerializationFile, loadState, "path");
    }
    if (const auto saveState = serState["save"]; saveState.is_object()) {
      set_bool_value(sp->serializeMonicaStateAtEnd, saveState, "atEnd");
      set_bool_value(sp->serializeMonicaStateAtEndToJson, saveState, "toJson");
      set_string_value(sp->pathToSerializationAtEndFile, saveState, "path");
      sp->noOfPreviousDaysSerializedClimateData = max(0, int_value(saveState, "noOfPreviousDaysSerializedClimateData"));
    }
  }

  // FAO-56 Dual Kc: method switch.
  // "evapotranspiration-method": "FAO-56-Dual" activates the Dual Kc pathway.
  // All other values (or absent key) keep the native Single-Kc method.
  if (j["evapotranspiration-method"].string_value() == "FAO-56-Dual") sp->dualKcMethod = true;
  // Note: isDripIrrigation and fw are now parsed at the Irrigation workstep event level.

  return res;
}

json11::Json simulationparameters::to_json(const SimulationParameters* sp) {
  return json11::Json::object
  {
    {"type", "SimulationParameters"},
    {"startDate", sp->startDate.toIsoDateString()},
    {"endDate", sp->endDate.toIsoDateString()},
    {"NitrogenResponseOn", sp->pc_NitrogenResponseOn},
    {"WaterDeficitResponseOn", sp->pc_WaterDeficitResponseOn},
    {"EmergenceFloodingControlOn", sp->pc_EmergenceFloodingControlOn},
    {"EmergenceMoistureControlOn", sp->pc_EmergenceMoistureControlOn},
    {"FrostKillOn", sp->pc_FrostKillOn},
    {"UseAutomaticIrrigation", sp->p_UseAutomaticIrrigation},
    {"AutoIrrigationParams", automaticirrigationparameters::to_json(&sp->p_AutoIrrigationParams)},
    {"UseNMinMineralFertilisingMethod", sp->p_UseNMinMineralFertilisingMethod},
    {"NMinFertiliserPartition", mineralfertilizerparameters::to_json(&sp->p_NMinFertiliserPartition)},
    {"NMinUserParams", nminapplicationparameters::to_json(&sp->p_NMinUserParams)},
    {"JulianDayAutomaticFertilising", sp->p_JulianDayAutomaticFertilising},
    {"UseSecondaryYields", sp->p_UseSecondaryYields},
    {"UseAutomaticHarvestTrigger", sp->p_UseAutomaticHarvestTrigger},
    {"NumberOfLayers", sp->p_NumberOfLayers},
    {"LayerThickness", sp->p_LayerThickness},
    {"StartPVIndex", sp->p_StartPVIndex},
    {"serializeMonicaStateAtEnd", sp->serializeMonicaStateAtEnd},
    {
      "serializedMonicaState",
      Json::object{
        {
          "load",
          Json::object{
            {"atStart", sp->loadSerializedMonicaStateAtStart},
            {"fromJson", sp->deserializedMonicaStateFromJson},
            {"path", sp->pathToLoadSerializationFile}
          }
        },
        {
          "save",
          Json::object{
            {"atEnd", sp->serializeMonicaStateAtEnd},
            {"toJson", sp->serializeMonicaStateAtEndToJson},
            {"path", sp->pathToSerializationAtEndFile},
            {"noOfPreviousDaysSerializedClimateData", int(sp->noOfPreviousDaysSerializedClimateData)}
          }
        }
      }
    },
    // FAO-56 Dual Kc: method switch only; event-level fw/isDrip are not stored here
    {"evapotranspiration-method", sp->dualKcMethod ? std::string("FAO-56-Dual") : std::string("Penman-Monteith")},
  };
}

//CropModuleParameters::CropModuleParameters(json11::Json j) {
//  merge(j);
//}

CropModuleParameters monica::makeCropModuleParameters(
    mas::schema::model::monica::CropModuleParameters::Reader reader) {
  CropModuleParameters cmp;
  cropmoduleparameters::deserialize(&cmp, reader);
  return cmp;
}

void cropmoduleparameters::deserialize(CropModuleParameters* cmp,
                                       mas::schema::model::monica::CropModuleParameters::Reader reader) {
  cmp->pc_CanopyReflectionCoefficient = reader.getCanopyReflectionCoefficient();
  cmp->pc_ReferenceMaxAssimilationRate = reader.getReferenceMaxAssimilationRate();
  cmp->pc_ReferenceLeafAreaIndex = reader.getReferenceLeafAreaIndex();
  cmp->pc_MaintenanceRespirationParameter1 = reader.getMaintenanceRespirationParameter1();
  cmp->pc_MaintenanceRespirationParameter2 = reader.getMaintenanceRespirationParameter2();
  cmp->pc_MinimumNConcentrationRoot = reader.getMinimumNConcentrationRoot();
  cmp->pc_MinimumAvailableN = reader.getMinimumAvailableN();
  cmp->pc_ReferenceAlbedo = reader.getReferenceAlbedo();
  cmp->pc_StomataConductanceAlpha = reader.getStomataConductanceAlpha();
  cmp->pc_SaturationBeta = reader.getSaturationBeta();
  cmp->pc_GrowthRespirationRedux = reader.getGrowthRespirationRedux();
  cmp->pc_MaxCropNDemand = reader.getMaxCropNDemand();
  cmp->pc_GrowthRespirationParameter1 = reader.getGrowthRespirationParameter1();
  cmp->pc_GrowthRespirationParameter2 = reader.getGrowthRespirationParameter2();
  cmp->pc_Tortuosity = reader.getTortuosity();
  cmp->pc_AdjustRootDepthForSoilProps = reader.getAdjustRootDepthForSoilProps();

  cmp->__enable_Phenology_WangEngelTemperatureResponse__ = reader.
    getExperimentalEnablePhenologyWangEngelTemperatureResponse();
  cmp->__enable_Photosynthesis_WangEngelTemperatureResponse__ = reader.
    getExperimentalEnablePhotosynthesisWangEngelTemperatureResponse();
  cmp->__enable_hourly_FvCB_photosynthesis__ = reader.getExperimentalEnableHourlyFvCBPhotosynthesis();
  cmp->__enable_T_response_leaf_expansion__ = reader.getExperimentalEnableTResponseLeafExpansion();
  cmp->__disable_daily_root_biomass_to_soil__ = reader.getExperimentalDisableDailyRootBiomassToSoil();
  cmp->__enable_vernalisation_factor_fix__ = reader.getEnableVernalisationFactorFix();
}

void cropmoduleparameters::serialize(const CropModuleParameters* cmp,
                                     mas::schema::model::monica::CropModuleParameters::Builder builder) {
  builder.setCanopyReflectionCoefficient(cmp->pc_CanopyReflectionCoefficient);
  builder.setReferenceMaxAssimilationRate(cmp->pc_ReferenceMaxAssimilationRate);
  builder.setReferenceLeafAreaIndex(cmp->pc_ReferenceLeafAreaIndex);
  builder.setMaintenanceRespirationParameter1(cmp->pc_MaintenanceRespirationParameter1);
  builder.setMaintenanceRespirationParameter2(cmp->pc_MaintenanceRespirationParameter2);
  builder.setMinimumNConcentrationRoot(cmp->pc_MinimumNConcentrationRoot);
  builder.setMinimumAvailableN(cmp->pc_MinimumAvailableN);
  builder.setReferenceAlbedo(cmp->pc_ReferenceAlbedo);
  builder.setStomataConductanceAlpha(cmp->pc_StomataConductanceAlpha);
  builder.setSaturationBeta(cmp->pc_SaturationBeta);
  builder.setGrowthRespirationRedux(cmp->pc_GrowthRespirationRedux);
  builder.setMaxCropNDemand(cmp->pc_MaxCropNDemand);
  builder.setGrowthRespirationParameter1(cmp->pc_GrowthRespirationParameter1);
  builder.setGrowthRespirationParameter2(cmp->pc_GrowthRespirationParameter2);
  builder.setTortuosity(cmp->pc_Tortuosity);
  builder.setAdjustRootDepthForSoilProps(cmp->pc_AdjustRootDepthForSoilProps);

  builder.setExperimentalEnablePhenologyWangEngelTemperatureResponse(cmp->__enable_Phenology_WangEngelTemperatureResponse__);
  builder.setExperimentalEnablePhotosynthesisWangEngelTemperatureResponse(
                                                                          cmp->__enable_Photosynthesis_WangEngelTemperatureResponse__);
  builder.setExperimentalEnableHourlyFvCBPhotosynthesis(cmp->__enable_hourly_FvCB_photosynthesis__);
  builder.setExperimentalEnableTResponseLeafExpansion(cmp->__enable_T_response_leaf_expansion__);
  builder.setExperimentalDisableDailyRootBiomassToSoil(cmp->__disable_daily_root_biomass_to_soil__);
  builder.setEnableVernalisationFactorFix(cmp->__enable_vernalisation_factor_fix__);
}

Errors cropmoduleparameters::merge(CropModuleParameters* cmp, json11::Json j) {
  Errors res = defaultMerge(j, [cmp](json11::Json j2) { return merge(cmp, j2); });

  set_double_value(cmp->pc_CanopyReflectionCoefficient, j, "CanopyReflectionCoefficient");
  set_double_value(cmp->pc_ReferenceMaxAssimilationRate, j, "ReferenceMaxAssimilationRate");
  set_double_value(cmp->pc_ReferenceLeafAreaIndex, j, "ReferenceLeafAreaIndex");
  set_double_value(cmp->pc_MaintenanceRespirationParameter1, j, "MaintenanceRespirationParameter1");
  set_double_value(cmp->pc_MaintenanceRespirationParameter2, j, "MaintenanceRespirationParameter2");
  set_double_value(cmp->pc_MinimumNConcentrationRoot, j, "MinimumNConcentrationRoot");
  set_double_value(cmp->pc_MinimumAvailableN, j, "MinimumAvailableN");
  set_double_value(cmp->pc_ReferenceAlbedo, j, "ReferenceAlbedo");
  set_double_value(cmp->pc_StomataConductanceAlpha, j, "StomataConductanceAlpha");
  set_double_value(cmp->pc_SaturationBeta, j, "SaturationBeta");
  set_double_value(cmp->pc_GrowthRespirationRedux, j, "GrowthRespirationRedux");
  set_double_value(cmp->pc_MaxCropNDemand, j, "MaxCropNDemand");
  set_double_value(cmp->pc_GrowthRespirationParameter1, j, "GrowthRespirationParameter1");
  set_double_value(cmp->pc_GrowthRespirationParameter2, j, "GrowthRespirationParameter2");
  set_double_value(cmp->pc_Tortuosity, j, "Tortuosity");
  set_bool_value(cmp->pc_AdjustRootDepthForSoilProps, j, "AdjustRootDepthForSoilProps");
  if (j["TimeUnderAnoxiaThreshold"].is_number()) {
    std::fill(cmp->pc_TimeUnderAnoxiaThreshold.begin(), cmp->pc_TimeUnderAnoxiaThreshold.end(),
              int(j["TimeUnderAnoxiaThreshold"].number_value()));
  } else if (j["TimeUnderAnoxiaThreshold"].is_array()) {
    set_int_vector(cmp->pc_TimeUnderAnoxiaThreshold, j, "TimeUnderAnoxiaThreshold");
  }

  set_bool_value(cmp->__enable_Photosynthesis_WangEngelTemperatureResponse__, j,
                 "__enable_Photosynthesis_WangEngelTemperatureResponse__");
  set_bool_value(cmp->__enable_Phenology_WangEngelTemperatureResponse__, j,
                 "__enable_Phenology_WangEngelTemperatureResponse__");
  set_bool_value(cmp->__enable_hourly_FvCB_photosynthesis__, j, "__enable_hourly_FvCB_photosynthesis__");
  set_bool_value(cmp->__enable_T_response_leaf_expansion__, j, "__enable_T_response_leaf_expansion__");
  set_bool_value(cmp->__disable_daily_root_biomass_to_soil__, j, "__disable_daily_root_biomass_to_soil__");
  set_bool_value(cmp->__enable_vernalisation_factor_fix__, j, "__enable_vernalisation_factor_fix__");
  set_bool_value(cmp->__enable_PASW_root_penetration__, j, "__enable_PASW_root_penetration__");

  set_bool_value(cmp->isIntercropping, j["intercropping"], "is_intercropping");
  set_bool_value(cmp->sequentialWaterUse, j["intercropping"], "sequential_water_use");
  set_bool_value(cmp->twoWaySync, j["intercropping"], "two_way_sync");
  set_double_value(cmp->pc_intercropping_k_s, j["intercropping"], "k_s");
  set_double_value(cmp->pc_intercropping_k_t, j["intercropping"], "k_t");
  set_double_value(cmp->pc_intercropping_phRedux, j["intercropping"], "PHredux");
  set_double_value(cmp->pc_intercropping_dvs_phr, j["intercropping"], "DVS_PHr");
  set_bool_value(cmp->pc_intercropping_autoPhRedux, j["intercropping"], "auto_PHredux");
  set_string_value(cmp->pc_intercropping_reader_sr, j["intercropping"], "reader_sr");
  set_string_value(cmp->pc_intercropping_writer_sr, j["intercropping"], "writer_sr");
  return res;
}

json11::Json cropmoduleparameters::to_json(const CropModuleParameters* cmp) {
  return json11::Json::object
  {
    {"type", "CropModuleParameters"},
    {"CanopyReflectionCoefficient", cmp->pc_CanopyReflectionCoefficient},
    {"ReferenceMaxAssimilationRate", cmp->pc_ReferenceMaxAssimilationRate},
    {"ReferenceLeafAreaIndex", cmp->pc_ReferenceLeafAreaIndex},
    {"MaintenanceRespirationParameter1", cmp->pc_MaintenanceRespirationParameter1},
    {"MaintenanceRespirationParameter2", cmp->pc_MaintenanceRespirationParameter2},
    {"MinimumNConcentrationRoot", cmp->pc_MinimumNConcentrationRoot},
    {"MinimumAvailableN", cmp->pc_MinimumAvailableN},
    {"ReferenceAlbedo", cmp->pc_ReferenceAlbedo},
    {"StomataConductanceAlpha", cmp->pc_StomataConductanceAlpha},
    {"SaturationBeta", cmp->pc_SaturationBeta},
    {"GrowthRespirationRedux", cmp->pc_GrowthRespirationRedux},
    {"MaxCropNDemand", cmp->pc_MaxCropNDemand},
    {"GrowthRespirationParameter1", cmp->pc_GrowthRespirationParameter1},
    {"GrowthRespirationParameter2", cmp->pc_GrowthRespirationParameter2},
    {"Tortuosity", cmp->pc_Tortuosity},
    {"AdjustRootDepthForSoilProps", cmp->pc_AdjustRootDepthForSoilProps},
    {"TimeUnderAnoxiaThreshold", cmp->pc_TimeUnderAnoxiaThreshold},
    {"__enable_Phenology_WangEngelTemperatureResponse__", cmp->__enable_Phenology_WangEngelTemperatureResponse__},
    {"__enable_Photosynthesis_WangEngelTemperatureResponse__", cmp->__enable_Photosynthesis_WangEngelTemperatureResponse__},
    {"__enable_hourly_FvCB_photosynthesis__", cmp->__enable_hourly_FvCB_photosynthesis__},
    {"__enable_T_response_leaf_expansion__", cmp->__enable_T_response_leaf_expansion__},
    {"__disable_daily_root_biomass_to_soil__", cmp->__disable_daily_root_biomass_to_soil__},
    {"__enable_vernalisation_factor_fix__", cmp->__enable_vernalisation_factor_fix__}
  };
}

EnvironmentParameters monica::makeEnvironmentParameters(
    mas::schema::model::monica::EnvironmentParameters::Reader reader) {
  EnvironmentParameters ep;
  environmentparameters::deserialize(&ep, reader);
  return ep;
}

void environmentparameters::deserialize(EnvironmentParameters* ep,
                                        mas::schema::model::monica::EnvironmentParameters::Reader reader) {
  ep->p_Albedo = reader.getAlbedo();
  ep->p_AtmosphericCO2 = reader.getAtmosphericCO2();

  ep->p_AtmosphericCO2s.clear();
  for (auto co2 : reader.getAtmosphericCO2s()) ep->p_AtmosphericCO2s[co2.getYear()] = co2.getValue();

  ep->p_AtmosphericO3s.clear();
  for (auto o3 : reader.getAtmosphericO3s()) ep->p_AtmosphericO3s[o3.getYear()] = o3.getValue();

  ep->p_WindSpeedHeight = reader.getWindSpeedHeight();
  ep->p_LeachingDepth = reader.getLeachingDepth();
  ep->p_timeStep = reader.getTimeStep();

  ep->p_MaxGroundwaterDepth = reader.getMaxGroundwaterDepth();
  ep->p_MinGroundwaterDepth = reader.getMinGroundwaterDepth();
  ep->p_MinGroundwaterDepthMonth = reader.getMinGroundwaterDepthMonth();

  ep->rcp = reader.getRcp();
}

void environmentparameters::serialize(const EnvironmentParameters* ep,
                                      mas::schema::model::monica::EnvironmentParameters::Builder builder) {
  builder.setAlbedo(ep->p_Albedo);
  builder.setAtmosphericCO2(ep->p_AtmosphericCO2);

  {
    auto co2s = builder.initAtmosphericCO2s((capnp::uint)ep->p_AtmosphericCO2s.size());
    capnp::uint i = 0;
    for (auto p : ep->p_AtmosphericCO2s) {
      co2s[i].setYear(p.first);
      co2s[i].setValue(p.second);
    }
  }
  builder.setAtmosphericO3(ep->p_AtmosphericO3);
  {
    auto o3s = builder.initAtmosphericO3s((capnp::uint)ep->p_AtmosphericO3s.size());
    capnp::uint i = 0;
    for (auto p : ep->p_AtmosphericO3s) {
      o3s[i].setYear(p.first);
      o3s[i].setValue(p.second);
    }
  }
  builder.setWindSpeedHeight(ep->p_WindSpeedHeight);
  builder.setLeachingDepth(ep->p_LeachingDepth);
  builder.setTimeStep(ep->p_timeStep);

  builder.setMaxGroundwaterDepth(ep->p_MaxGroundwaterDepth);
  builder.setMinGroundwaterDepth(ep->p_MinGroundwaterDepth);
  builder.setMinGroundwaterDepthMonth(ep->p_MinGroundwaterDepthMonth);

  builder.setRcp(ep->rcp);
}

//EnvironmentParameters::EnvironmentParameters(json11::Json j) {
//  merge(j);
//}

Errors environmentparameters::merge(EnvironmentParameters* ep, json11::Json j) {
  Errors res = defaultMerge(j, [ep](json11::Json j2) { return merge(ep, j2); });

  using namespace mas::schema::climate;
  auto rcpNo2rcpEnum = [&](int rcpNo) {
    switch (rcpNo) {
    case 19: return RCP::RCP19;
      break;
    case 26: return RCP::RCP26;
      break;
    case 34: return RCP::RCP34;
      break;
    case 45: return RCP::RCP45;
      break;
    case 60: return RCP::RCP60;
      break;
    case 70: return RCP::RCP70;
      break;
    case 85: return RCP::RCP85;
      break;
    default: ;
    }
    res.appendWarning(kj::str("RCP", rcpNo, " unknown. Default RCP 8.5 used.").cStr());
    return RCP::RCP85;
  };

  set_double_value(ep->p_Albedo, j, "Albedo");

  if (j["rcp"].is_string()) {
    try {
      switch (const auto rcpStr = j["rcp"].string_value(); rcpStr.size()) {
      case 2: ep->rcp = rcpNo2rcpEnum(stoi(rcpStr));
        break;
      case 3: ep->rcp = rcpNo2rcpEnum(static_cast<int>(stod(rcpStr) * 10));
        break;
      case 5: ep->rcp = rcpNo2rcpEnum(stoi(rcpStr.substr(3)));
        break;
      case 6: ep->rcp = rcpNo2rcpEnum(static_cast<int>(stod(rcpStr.substr(3)) * 10));
        break;
      default: ep->rcp = RCP::RCP85;
      }
    } catch (std::exception&) {
      res.appendWarning(kj::str(j["rcp"].string_value(), " unknown. Default RCP 8.5 used.").cStr());
    }
  } else if (j["rcp"].is_number()) {
    if (const auto rcpNo = j["rcp"].number_value(); rcpNo < 10) {
      ep->rcp = rcpNo2rcpEnum(static_cast<int>(rcpNo * 10));
    } else ep->rcp = rcpNo2rcpEnum(static_cast<int>(rcpNo));
  }

  set_double_value(ep->p_AtmosphericCO2, j, "AtmosphericCO2");
  if (j["AtmosphericCO2s"].is_object()) {
    ep->p_AtmosphericCO2s.clear();
    for (auto p : j["AtmosphericCO2s"].object_items()) ep->p_AtmosphericCO2s[stoi(p.first)] = p.second.number_value();
  }
  set_double_value(ep->p_AtmosphericO3, j, "AtmosphericO3");
  if (j["AtmosphericO3s"].is_object()) {
    ep->p_AtmosphericO3s.clear();
    for (auto p : j["AtmosphericO3s"].object_items()) ep->p_AtmosphericO3s[stoi(p.first)] = p.second.number_value();
  }
  set_double_value(ep->p_WindSpeedHeight, j, "WindSpeedHeight");
  set_double_value(ep->p_LeachingDepth, j, "LeachingDepth");
  set_double_value(ep->p_timeStep, j, "timeStep");
  set_double_value(ep->p_MaxGroundwaterDepth, j, "MaxGroundwaterDepth");
  set_double_value(ep->p_MinGroundwaterDepth, j, "MinGroundwaterDepth");
  set_int_value(ep->p_MinGroundwaterDepthMonth, j, "MinGroundwaterDepthMonth");

  return res;
}

json11::Json environmentparameters::to_json(const EnvironmentParameters* ep) {
  json11::Json::object co2s;
  for (auto p : ep->p_AtmosphericCO2s) co2s[to_string(p.first)] = p.second;

  json11::Json::object o3s;
  for (auto p : ep->p_AtmosphericO3s) o3s[to_string(p.first)] = p.second;

  auto rcp2str = [](auto rcp) {
    using namespace mas::schema::climate;
    switch (rcp) {
    case RCP::RCP19: return "rcp19";
      break;
    case RCP::RCP26: return "rcp26";
      break;
    case RCP::RCP34: return "rcp34";
      break;
    case RCP::RCP45: return "rcp45";
      break;
    case RCP::RCP60: return "rcp60";
      break;
    case RCP::RCP70: return "rcp70";
      break;
    case RCP::RCP85: return "rcp85";
      break;
    }
    return "rcp85";
  };

  return json11::Json::object
  {
    {"type", "EnvironmentParameters"},
    {"Albedo", ep->p_Albedo},
    {"rcp", rcp2str(ep->rcp)},
    {"AtmosphericCO2", ep->p_AtmosphericCO2},
    {"AtmosphericCO2s", co2s},
    {"AtmosphericO3", ep->p_AtmosphericO3},
    {"AtmosphericO3s", o3s},
    {"WindSpeedHeight", ep->p_WindSpeedHeight},
    {"LeachingDepth", ep->p_LeachingDepth},
    {"timeStep", ep->p_timeStep},
    {"MaxGroundwaterDepth", ep->p_MaxGroundwaterDepth},
    {"MinGroundwaterDepth", ep->p_MinGroundwaterDepth},
    {"MinGroundwaterDepthMonth", ep->p_MinGroundwaterDepthMonth}
  };
}

SoilMoistureModuleParameters monica::makeSoilMoistureModuleParameters(
    mas::schema::model::monica::SoilMoistureModuleParameters::Reader reader) {
  SoilMoistureModuleParameters smp;
  soilmoisturemoduleparameters::deserialize(&smp, reader);
  return smp;
}

void soilmoisturemoduleparameters::deserialize(
  SoilMoistureModuleParameters* smp,
  mas::schema::model::monica::SoilMoistureModuleParameters::Reader reader) {
  //smp->pm_CriticalMoistureDepth = reader.getCriticalMoistureDepth();
  smp->pm_SaturatedHydraulicConductivity = reader.getSaturatedHydraulicConductivity();
  smp->pm_SurfaceRoughness = reader.getSurfaceRoughness();
  smp->pm_GroundwaterDischarge = reader.getGroundwaterDischarge();
  smp->pm_HydraulicConductivityRedux = reader.getHydraulicConductivityRedux();
  smp->pm_SnowAccumulationTresholdTemperature = reader.getSnowAccumulationTresholdTemperature();
  smp->pm_KcFactor = reader.getKcFactor();
  smp->pm_TemperatureLimitForLiquidWater = reader.getTemperatureLimitForLiquidWater();
  smp->pm_CorrectionSnow = reader.getCorrectionSnow();
  smp->pm_CorrectionRain = reader.getCorrectionRain();
  smp->pm_SnowMaxAdditionalDensity = reader.getSnowMaxAdditionalDensity();
  smp->pm_NewSnowDensityMin = reader.getNewSnowDensityMin();
  smp->pm_SnowRetentionCapacityMin = reader.getSnowRetentionCapacityMin();
  smp->pm_RefreezeParameter1 = reader.getRefreezeParameter1();
  smp->pm_RefreezeParameter2 = reader.getRefreezeParameter2();
  smp->pm_RefreezeTemperature = reader.getRefreezeTemperature();
  smp->pm_SnowMeltTemperature = reader.getSnowMeltTemperature();
  smp->pm_SnowPacking = reader.getSnowPacking();
  smp->pm_SnowRetentionCapacityMax = reader.getSnowRetentionCapacityMax();
  smp->pm_EvaporationZeta = reader.getEvaporationZeta();
  smp->pm_XSACriticalSoilMoisture = reader.getXsaCriticalSoilMoisture();
  smp->pm_MaximumEvaporationImpactDepth = reader.getMaximumEvaporationImpactDepth();
  smp->pm_MaxPercolationRate = reader.getMaxPercolationRate();
  smp->pm_MoistureInitValue = reader.getMoistureInitValue();
}

void soilmoisturemoduleparameters::serialize(
  const SoilMoistureModuleParameters* smp,
  mas::schema::model::monica::SoilMoistureModuleParameters::Builder builder) {
  //builder.setCriticalMoistureDepth(smp->pm_CriticalMoistureDepth);
  builder.setSaturatedHydraulicConductivity(smp->pm_SaturatedHydraulicConductivity);
  builder.setSurfaceRoughness(smp->pm_SurfaceRoughness);
  builder.setGroundwaterDischarge(smp->pm_GroundwaterDischarge);
  builder.setHydraulicConductivityRedux(smp->pm_HydraulicConductivityRedux);
  builder.setSnowAccumulationTresholdTemperature(smp->pm_SnowAccumulationTresholdTemperature);
  builder.setKcFactor(smp->pm_KcFactor);
  builder.setTemperatureLimitForLiquidWater(smp->pm_TemperatureLimitForLiquidWater);
  builder.setCorrectionSnow(smp->pm_CorrectionSnow);
  builder.setCorrectionRain(smp->pm_CorrectionRain);
  builder.setSnowMaxAdditionalDensity(smp->pm_SnowMaxAdditionalDensity);
  builder.setNewSnowDensityMin(smp->pm_NewSnowDensityMin);
  builder.setSnowRetentionCapacityMin(smp->pm_SnowRetentionCapacityMin);
  builder.setRefreezeParameter1(smp->pm_RefreezeParameter1);
  builder.setRefreezeParameter2(smp->pm_RefreezeParameter2);
  builder.setRefreezeTemperature(smp->pm_RefreezeTemperature);
  builder.setSnowMeltTemperature(smp->pm_SnowMeltTemperature);
  builder.setSnowPacking(smp->pm_SnowPacking);
  builder.setSnowRetentionCapacityMax(smp->pm_SnowRetentionCapacityMax);
  builder.setEvaporationZeta(smp->pm_EvaporationZeta);
  builder.setXsaCriticalSoilMoisture(smp->pm_XSACriticalSoilMoisture);
  builder.setMaximumEvaporationImpactDepth(smp->pm_MaximumEvaporationImpactDepth);
  builder.setMaxPercolationRate(smp->pm_MaxPercolationRate);
  builder.setMoistureInitValue(smp->pm_MoistureInitValue);
}

// SoilMoistureModuleParameters::SoilMoistureModuleParameters(json11::Json j)
//     : SoilMoistureModuleParameters() {
//   merge(j);
// }

Errors soilmoisturemoduleparameters::merge(SoilMoistureModuleParameters* smp, json11::Json j) {
  Errors res = defaultMerge(j, [smp](json11::Json j2) { return merge(smp, j2); });

  //set_double_value(smp->pm_CriticalMoistureDepth, j, "CriticalMoistureDepth");
  set_double_value(smp->pm_SaturatedHydraulicConductivity, j, "SaturatedHydraulicConductivity");
  set_double_value(smp->pm_SurfaceRoughness, j, "SurfaceRoughness");
  set_double_value(smp->pm_GroundwaterDischarge, j, "GroundwaterDischarge");
  set_double_value(smp->pm_HydraulicConductivityRedux, j, "HydraulicConductivityRedux");
  set_double_value(smp->pm_SnowAccumulationTresholdTemperature, j, "SnowAccumulationTresholdTemperature");
  set_double_value(smp->pm_KcFactor, j, "KcFactor");
  set_double_value(smp->pm_TemperatureLimitForLiquidWater, j, "TemperatureLimitForLiquidWater");
  set_double_value(smp->pm_CorrectionSnow, j, "CorrectionSnow");
  set_double_value(smp->pm_CorrectionRain, j, "CorrectionRain");
  set_double_value(smp->pm_SnowMaxAdditionalDensity, j, "SnowMaxAdditionalDensity");
  set_double_value(smp->pm_NewSnowDensityMin, j, "NewSnowDensityMin");
  set_double_value(smp->pm_SnowRetentionCapacityMin, j, "SnowRetentionCapacityMin");
  set_double_value(smp->pm_RefreezeParameter1, j, "RefreezeParameter1");
  set_double_value(smp->pm_RefreezeParameter2, j, "RefreezeParameter2");
  set_double_value(smp->pm_RefreezeTemperature, j, "RefreezeTemperature");
  set_double_value(smp->pm_SnowMeltTemperature, j, "SnowMeltTemperature");
  set_double_value(smp->pm_SnowPacking, j, "SnowPacking");
  set_double_value(smp->pm_SnowRetentionCapacityMax, j, "SnowRetentionCapacityMax");
  set_double_value(smp->pm_EvaporationZeta, j, "EvaporationZeta");
  set_double_value(smp->pm_XSACriticalSoilMoisture, j, "XSACriticalSoilMoisture");
  set_double_value(smp->pm_MaximumEvaporationImpactDepth, j, "MaximumEvaporationImpactDepth");
  set_double_value(smp->pm_MaxPercolationRate, j, "MaxPercolationRate");
  set_double_value(smp->pm_MoistureInitValue, j, "MoistureInitValue");

  return res;
}

json11::Json soilmoisturemoduleparameters::to_json(const SoilMoistureModuleParameters* smp) {
  return json11::Json::object
  {
    {"type", "SoilMoistureModuleParameters"},
    //{"CriticalMoistureDepth",               smp->pm_CriticalMoistureDepth},
    {"SaturatedHydraulicConductivity", smp->pm_SaturatedHydraulicConductivity},
    {"SurfaceRoughness", smp->pm_SurfaceRoughness},
    {"GroundwaterDischarge", smp->pm_GroundwaterDischarge},
    {"HydraulicConductivityRedux", smp->pm_HydraulicConductivityRedux},
    {"SnowAccumulationTresholdTemperature", smp->pm_SnowAccumulationTresholdTemperature},
    {"KcFactor", smp->pm_KcFactor},
    {"TemperatureLimitForLiquidWater", smp->pm_TemperatureLimitForLiquidWater},
    {"CorrectionSnow", smp->pm_CorrectionSnow},
    {"CorrectionRain", smp->pm_CorrectionRain},
    {"SnowMaxAdditionalDensity", smp->pm_SnowMaxAdditionalDensity},
    {"NewSnowDensityMin", smp->pm_NewSnowDensityMin},
    {"SnowRetentionCapacityMin", smp->pm_SnowRetentionCapacityMin},
    {"RefreezeParameter1", smp->pm_RefreezeParameter1},
    {"RefreezeParameter2", smp->pm_RefreezeParameter2},
    {"RefreezeTemperature", smp->pm_RefreezeTemperature},
    {"SnowMeltTemperature", smp->pm_SnowMeltTemperature},
    {"SnowPacking", smp->pm_SnowPacking},
    {"SnowRetentionCapacityMax", smp->pm_SnowRetentionCapacityMax},
    {"EvaporationZeta", smp->pm_EvaporationZeta},
    {"XSACriticalSoilMoisture", smp->pm_XSACriticalSoilMoisture},
    {"MaximumEvaporationImpactDepth", smp->pm_MaximumEvaporationImpactDepth},
    {"MaxPercolationRate", smp->pm_MaxPercolationRate},
    {"MoistureInitValue", smp->pm_MoistureInitValue}
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
  {
    {"type", "SoilTemperatureModuleParameters"},
    {"NTau", pt_NTau},
    {"InitialSurfaceTemperature", pt_InitialSurfaceTemperature},
    {"BaseTemperature", pt_BaseTemperature},
    {"QuartzRawDensity", pt_QuartzRawDensity},
    {"DensityAir", pt_DensityAir},
    {"DensityWater", pt_DensityWater},
    {"DensityHumus", pt_DensityHumus},
    {"SpecificHeatCapacityAir", pt_SpecificHeatCapacityAir},
    {"SpecificHeatCapacityQuartz", pt_SpecificHeatCapacityQuartz},
    {"SpecificHeatCapacityWater", pt_SpecificHeatCapacityWater},
    {"SpecificHeatCapacityHumus", pt_SpecificHeatCapacityHumus},
    {"SoilAlbedo", pt_SoilAlbedo},
    {"SoilMoisture", pt_SoilMoisture}
  };
}

void SoilTransportModuleParameters::deserialize(
  mas::schema::model::monica::SoilTransportModuleParameters::Reader reader) {
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
  {
    {"type", "SoilTransportModuleParameters"},
    {"DispersionLength", pq_DispersionLength},
    {"AD", pq_AD},
    {"DiffusionCoefficientStandard", pq_DiffusionCoefficientStandard},
    {"NDeposition", pq_NDeposition}
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
  {
    {"type", "SticsParameters"},
    {"use_n2o", use_n2o},
    {"use_nit", use_nit},
    {"use_denit", use_denit},
    {"code_vnit", J11Array{code_vnit, ""}},
    {"code_tnit", J11Array{code_tnit, ""}},
    {"code_rationit", J11Array{code_rationit, ""}},
    {"code_hourly_wfps_nit", J11Array{code_hourly_wfps_nit, ""}},
    {"code_pdenit", J11Array{code_pdenit, ""}},
    {"code_ratiodenit", J11Array{code_ratiodenit, ""}},
    {"code_hourly_wfps_denit", J11Array{code_hourly_wfps_denit, ""}},
    {"hminn", J11Array{hminn, ""}},
    {"hoptn", J11Array{hoptn, ""}},
    {"pHminnit", J11Array{pHminnit, ""}},
    {"pHmaxnit", J11Array{pHmaxnit, ""}},
    {"nh4_min", J11Array{nh4_min, ""}},
    {"pHminden", J11Array{pHminden, ""}},
    {"pHmaxden", J11Array{pHmaxden, ""}},
    {"wfpsc", J11Array{wfpsc, ""}},
    {"tdenitopt_gauss", J11Array{tdenitopt_gauss, ""}},
    {"scale_tdenitopt", J11Array{scale_tdenitopt, ""}},
    {"Kd", J11Array{Kd, ""}},
    {"k_desat", J11Array{k_desat, ""}},
    {"fnx", J11Array{fnx, ""}},
    {"vnitmax", J11Array{vnitmax, ""}},
    {"Kamm", J11Array{Kamm, ""}},
    {"tnitmin", J11Array{tnitmin, ""}},
    {"tnitopt", J11Array{tnitopt, ""}},
    {"tnitop2", J11Array{tnitop2, ""}},
    {"tnitmax", J11Array{tnitmax, ""}},
    {"tnitopt_gauss", J11Array{tnitopt_gauss, ""}},
    {"scale_tnitopt", J11Array{scale_tnitopt, ""}},
    {"rationit", J11Array{rationit, ""}},
    {"cmin_pdenit", J11Array{cmin_pdenit, ""}},
    {"cmax_pdenit", J11Array{cmax_pdenit, ""}},
    {"min_pdenit", J11Array{min_pdenit, ""}},
    {"max_pdenit", J11Array{max_pdenit, ""}},
    {"ratiodenit", J11Array{ratiodenit, ""}},
    {"profdenit", J11Array{profdenit, ""}},
    {"vpotdenit", vpotdenit}
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

void SoilOrganicModuleParameters::serialize(
  mas::schema::model::monica::SoilOrganicModuleParameters::Builder builder) const {
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
  {
    {"type", "SoilOrganicModuleParameters"},
    {"SOM_SlowDecCoeffStandard", J11Array{po_SOM_SlowDecCoeffStandard, "d-1"}},
    {"SOM_FastDecCoeffStandard", J11Array{po_SOM_FastDecCoeffStandard, "d-1"}},
    {"SMB_SlowMaintRateStandard", J11Array{po_SMB_SlowMaintRateStandard, "d-1"}},
    {"SMB_FastMaintRateStandard", J11Array{po_SMB_FastMaintRateStandard, "d-1"}},
    {"SMB_SlowDeathRateStandard", J11Array{po_SMB_SlowDeathRateStandard, "d-1"}},
    {"SMB_FastDeathRateStandard", J11Array{po_SMB_FastDeathRateStandard, "d-1"}},
    {"SMB_UtilizationEfficiency", J11Array{po_SMB_UtilizationEfficiency, "d-1"}},
    {"SOM_SlowUtilizationEfficiency", J11Array{po_SOM_SlowUtilizationEfficiency, ""}},
    {"SOM_FastUtilizationEfficiency", J11Array{po_SOM_FastUtilizationEfficiency, ""}},
    {"AOM_SlowUtilizationEfficiency", J11Array{po_AOM_SlowUtilizationEfficiency, ""}},
    {"AOM_FastUtilizationEfficiency", J11Array{po_AOM_FastUtilizationEfficiency, ""}},
    {"AOM_FastMaxC_to_N", J11Array{po_AOM_FastMaxC_to_N, ""}},
    {"PartSOM_Fast_to_SOM_Slow", J11Array{po_PartSOM_Fast_to_SOM_Slow, ""}},
    {"PartSMB_Slow_to_SOM_Fast", J11Array{po_PartSMB_Slow_to_SOM_Fast, ""}},
    {"PartSMB_Fast_to_SOM_Fast", J11Array{po_PartSMB_Fast_to_SOM_Fast, ""}},
    {"PartSOM_to_SMB_Slow", J11Array{po_PartSOM_to_SMB_Slow, ""}},
    {"PartSOM_to_SMB_Fast", J11Array{po_PartSOM_to_SMB_Fast, ""}},
    {"CN_Ratio_SMB", J11Array{po_CN_Ratio_SMB, ""}},
    {"LimitClayEffect", J11Array{po_LimitClayEffect, "kg kg-1"}},
    {"QTenFactor", J11Array{po_QTenFactor, ""}},
    {"TempDecOptimal", J11Array{po_TempDecOptimal, "°C"}},
    {"MoistureDecOptimal", J11Array{po_MoistureDecOptimal, "%"}},
    {"AmmoniaOxidationRateCoeffStandard", J11Array{po_AmmoniaOxidationRateCoeffStandard, "d-1"}},
    {"NitriteOxidationRateCoeffStandard", J11Array{po_NitriteOxidationRateCoeffStandard, "d-1"}},
    {"TransportRateCoeff", J11Array{po_TransportRateCoeff, "d-1"}},
    {"SpecAnaerobDenitrification", J11Array{po_SpecAnaerobDenitrification, "g gas-N g CO2-C-1"}},
    {"ImmobilisationRateCoeffNO3", J11Array{po_ImmobilisationRateCoeffNO3, "d-1"}},
    {"ImmobilisationRateCoeffNH4", J11Array{po_ImmobilisationRateCoeffNH4, "d-1"}},
    {"Denit1", J11Array{po_Denit1, ""}},
    {"Denit2", J11Array{po_Denit2, ""}},
    {"Denit3", J11Array{po_Denit3, ""}},
    {"HydrolysisKM", J11Array{po_HydrolysisKM, ""}},
    {"ActivationEnergy", J11Array{po_ActivationEnergy, ""}},
    {"HydrolysisP1", J11Array{po_HydrolysisP1, ""}},
    {"HydrolysisP2", J11Array{po_HydrolysisP2, ""}},
    {"AtmosphericResistance", J11Array{po_AtmosphericResistance, "s m-1"}},
    {"N2OProductionRate", J11Array{po_N2OProductionRate, "d-1"}},
    {"Inhibitor_NH3", J11Array{po_Inhibitor_NH3, "kg N m-3"}},
    {"MaxMineralisationDepth", ps_MaxMineralisationDepth}
  };
}

CentralParameterProvider::CentralParameterProvider()
: _pathToOutputDir(".")
, precipCorrectionValues(12, 1.0) {}

// CentralParameterProvider::CentralParameterProvider(json11::Json j) {
//   merge(j);
// }

Errors CentralParameterProvider::merge(json11::Json j) {
  Errors res;

  res.append(cropmoduleparameters::merge(&userCropParameters, j["userCropParameters"]));
  res.append(environmentparameters::merge(&userEnvironmentParameters, j["userEnvironmentParameters"]));
  res.append(soilmoisturemoduleparameters::merge(&userSoilMoistureParameters, j["userSoilMoistureParameters"]));
  res.append(userSoilTemperatureParameters.merge(j["userSoilTemperatureParameters"]));
  res.append(userSoilTransportParameters.merge(j["userSoilTransportParameters"]));
  res.append(userSoilOrganicParameters.merge(j["userSoilOrganicParameters"]));
  res.append(simulationparameters::merge(&simulationParameters, j["simulationParameters"]));
  res.append(siteparameters::merge(&siteParameters, j["siteParameters"]));
  if (!j["groundwaterInformation"].is_null()) {
    res.append(measuredgroundwatertableinformation::merge(&groundwaterInformation, j["groundwaterInformation"]));
  }

  //set_bool_value(_writeOutputFiles, j, "writeOutputFiles");

  return res;
}

json11::Json CentralParameterProvider::to_json() const {
  return json11::Json::object
  {
    {"type", "CentralParameterProvider"},
    {"userCropParameters", cropmoduleparameters::to_json(&userCropParameters)},
    {"userEnvironmentParameters", environmentparameters::to_json(&userEnvironmentParameters)},
    {"userSoilMoistureParameters", soilmoisturemoduleparameters::to_json(&userSoilMoistureParameters)},
    {"userSoilTemperatureParameters", userSoilTemperatureParameters.to_json()},
    {"userSoilTransportParameters", userSoilTransportParameters.to_json()},
    {"userSoilOrganicParameters", userSoilOrganicParameters.to_json()},
    {"simulationParameters", simulationparameters::to_json(&simulationParameters)},
    {"siteParameters", siteparameters::to_json(&siteParameters)}
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
