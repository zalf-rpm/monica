/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
Authors:
Claas Nendel <claas.nendel@zalf.de>
Xenia Specka <xenia.specka@zalf.de>
Michael Berg <michael.berg@zalf.de>
Tommaso Stella <tommaso.stella@zalf.de>

Maintainers:
Currently maintained by the authors.

This file is part of the MONICA model.
Copyright (C) Leibniz Centre for Agricultural Landscape Research (ZALF)
*/

/** @todo Christian: Strahlungskonzept. Welche Information wird wo verwendet? */

#include "crop-module_simple.h"

#include <cmath>
#include <string>

#include <kj/exception.h>

#include "tools/debug.h"
#include "soilmoisture_simple.h"
#include "monica-parameters.h"
#include "tools/helper.h"
#include "tools/algorithms.h"
#include "voc-guenther.h"
#include "voc-jjv.h"
#include "voc-common.h"
#include "photosynthesis-FvCB.h"
#include "O3-impact.h"
#include "io/output.h"

const double PI = 3.14159265358979323;

using namespace std;
using namespace monica;
using namespace Tools;

/**
 * @brief Constructor
 * @param sc Soil column
 * @param gps General parameters
 * @param cps crop parameters
 * @param stps site parameters
 *
 * @author Claas Nendel
 */
void monica::cropModuleInitialize(CropModule* cm,
                                  SoilColumn* sc,
                                  const CropModuleParameters* cropPs,
                                  std::function<void(std::string)> fireEvent,
                                  std::function<void(std::map<size_t, double>, double)> addOrganicMatter,
                                  std::function<std::pair<double, double>(double)> getSnowDepthAndCalcTempUnderSnow,
                                  Intercropping* ic) {
  cm->_intercropping = ic;
  cm->soilColumn = sc;
  cm->cropPs = cropPs;
  cm->_fireEvent = kj::mv(fireEvent);
  cm->_addOrganicMatter = kj::mv(addOrganicMatter);
  cm->_getSnowDepthAndCalcTempUnderSnow = kj::mv(getSnowDepthAndCalcTempUnderSnow);
}

void monica::cropModuleInitializeFromCropParameters(CropModule* cm,
                                                    const CropParameters* cps,
                                                    CropResidueParameters rps,
                                                    bool isWinterCrop,
                                                    const SiteParameters* stps,
                                                    const SimulationParameters& simPs) {
  cm->_frostKillOn = simPs.pc_FrostKillOn;
  cm->speciesPs = cps->speciesParams;
  cm->cultivarPs = cps->cultivarParams;
  cm->residuePs = kj::mv(rps);
  cm->_isWinterCrop = isWinterCrop;
  cm->_bareSoilKcFactor = stps->bareSoilKcFactor;
  cm->vs_Latitude = stps->vs_Latitude;
  cm->pc_AbovegroundOrgan = cps->speciesParams.pc_AbovegroundOrgan;
  cm->pc_AssimilatePartitioningCoeff = cps->cultivarParams.pc_AssimilatePartitioningCoeff;
  cm->pc_AssimilateReallocation = cps->speciesParams.pc_AssimilateReallocation;
  cm->pc_BaseDaylength = cps->cultivarParams.pc_BaseDaylength;
  cm->pc_BaseTemperature = cps->speciesParams.pc_BaseTemperature;
  cm->pc_BeginSensitivePhaseHeatStress = cps->cultivarParams.pc_BeginSensitivePhaseHeatStress;
  cm->pc_CarboxylationPathway = cps->speciesParams.pc_CarboxylationPathway;
  cm->pc_CriticalOxygenContent = cps->speciesParams.pc_CriticalOxygenContent;
  cm->pc_CriticalTemperatureHeatStress = cps->cultivarParams.pc_CriticalTemperatureHeatStress;
  cm->pc_CropHeightP1 = cps->cultivarParams.pc_CropHeightP1;
  cm->pc_CropHeightP2 = cps->cultivarParams.pc_CropHeightP2;
  cm->pc_CropName = cps->pc_CropName();
  cm->pc_CropSpecificMaxRootingDepth = cps->cultivarParams.pc_CropSpecificMaxRootingDepth;
  cm->vc_CurrentTemperatureSum = std::vector<double>(cps->speciesParams.pc_NumberOfDevelopmentalStages(), 0.0);
  cm->pc_CuttingDelayDays = cps->speciesParams.pc_CuttingDelayDays;
  cm->pc_DaylengthRequirement = cps->cultivarParams.pc_DaylengthRequirement;
  cm->pc_DefaultRadiationUseEfficiency = cps->speciesParams.pc_DefaultRadiationUseEfficiency;
  cm->pc_DevelopmentAccelerationByNitrogenStress = cps->speciesParams.pc_DevelopmentAccelerationByNitrogenStress;
  cm->pc_DroughtStressThreshold = cps->cultivarParams.pc_DroughtStressThreshold;
  cm->pc_DroughtImpactOnFertilityFactor = cps->speciesParams.pc_DroughtImpactOnFertilityFactor;
  cm->pc_EmergenceFloodingControlOn = simPs.pc_EmergenceFloodingControlOn;
  cm->pc_EmergenceMoistureControlOn = simPs.pc_EmergenceMoistureControlOn;
  cm->pc_EndSensitivePhaseHeatStress = cps->cultivarParams.pc_EndSensitivePhaseHeatStress;
  cm->pc_FieldConditionModifier = cps->speciesParams.pc_FieldConditionModifier;
  cm->pc_FrostDehardening = cps->cultivarParams.pc_FrostDehardening;
  cm->pc_FrostHardening = cps->cultivarParams.pc_FrostHardening;
  cm->pc_HeatSumIrrigationStart = cps->cultivarParams.pc_HeatSumIrrigationStart;
  cm->pc_HeatSumIrrigationEnd = cps->cultivarParams.pc_HeatSumIrrigationEnd;
  cm->vs_HeightNN = stps->vs_HeightNN;
  cm->pc_InitialKcFactor = cps->speciesParams.pc_InitialKcFactor;
  cm->pc_InitialOrganBiomass = cps->speciesParams.pc_InitialOrganBiomass;
  cm->pc_InitialRootingDepth = cps->speciesParams.pc_InitialRootingDepth;
  cm->vc_sunlitLeafAreaIndex = std::vector<double>(24);
  cm->vc_shadedLeafAreaIndex = std::vector<double>(24);
  cm->pc_LowTemperatureExposure = cps->cultivarParams.pc_LowTemperatureExposure;
  cm->pc_LimitingTemperatureHeatStress = cps->speciesParams.pc_LimitingTemperatureHeatStress;
  cm->pc_LT50cultivar = cps->cultivarParams.pc_LT50cultivar;
  cm->pc_LuxuryNCoeff = cps->speciesParams.pc_LuxuryNCoeff;
  cm->pc_MaxAssimilationRate = cps->cultivarParams.pc_MaxAssimilationRate;
  cm->pc_MaxCropDiameter = cps->speciesParams.pc_MaxCropDiameter;
  cm->pc_MaxCropHeight = cps->cultivarParams.pc_MaxCropHeight;
  cm->pc_MaxNUptakeParam = cps->speciesParams.pc_MaxNUptakeParam;
  cm->pc_MinimumNConcentration = cps->speciesParams.pc_MinimumNConcentration;
  cm->pc_MinimumTemperatureForAssimilation = cps->speciesParams.pc_MinimumTemperatureForAssimilation;
  cm->pc_MaximumTemperatureForAssimilation = cps->speciesParams.pc_MaximumTemperatureForAssimilation;
  cm->pc_OptimumTemperatureForAssimilation = cps->speciesParams.pc_OptimumTemperatureForAssimilation;
  cm->pc_MinimumTemperatureRootGrowth = cps->speciesParams.pc_MinimumTemperatureRootGrowth;
  cm->pc_NConcentrationAbovegroundBiomass = cps->speciesParams.pc_NConcentrationAbovegroundBiomass;
  cm->pc_NConcentrationB0 = cps->speciesParams.pc_NConcentrationB0;
  cm->pc_NConcentrationPN = cps->speciesParams.pc_NConcentrationPN;
  cm->pc_NConcentrationRoot = cps->speciesParams.pc_NConcentrationRoot;
  cm->pc_NitrogenResponseOn = simPs.pc_NitrogenResponseOn;
  cm->pc_NumberOfDevelopmentalStages = cps->speciesParams.pc_NumberOfDevelopmentalStages();
  cm->pc_NumberOfOrgans = cps->speciesParams.pc_NumberOfOrgans();
  cm->vc_NUptakeFromLayer = std::vector<double>(cm->soilColumn->size(), 0.0);
  cm->pc_OptimumTemperature = cps->cultivarParams.pc_OptimumTemperature;
  cm->vc_OrganBiomass = std::vector<double>(cm->pc_NumberOfOrgans, 0.0);
  cm->vc_OrganDeadBiomass = std::vector<double>(cps->speciesParams.pc_NumberOfOrgans(), 0.0);
  cm->vc_OrganGreenBiomass = std::vector<double>(cps->speciesParams.pc_NumberOfOrgans(), 0.0);
  cm->vc_OrganGrowthIncrement = std::vector<double>(cm->pc_NumberOfOrgans, 0.0);
  cm->pc_OrganGrowthRespiration = cps->speciesParams.pc_OrganGrowthRespiration;
  cm->pc_OrganIdsForPrimaryYield = cps->cultivarParams.pc_OrganIdsForPrimaryYield;
  cm->pc_OrganIdsForSecondaryYield = cps->cultivarParams.pc_OrganIdsForSecondaryYield;
  cm->pc_OrganIdsForCutting = cps->cultivarParams.pc_OrganIdsForCutting;
  cm->pc_OrganMaintenanceRespiration = cps->speciesParams.pc_OrganMaintenanceRespiration;
  cm->vc_OrganSenescenceIncrement = std::vector<double>(cm->pc_NumberOfOrgans, 0.0);
  cm->pc_OrganSenescenceRate = cps->cultivarParams.pc_OrganSenescenceRate;
  cm->pc_PartBiologicalNFixation = cps->speciesParams.pc_PartBiologicalNFixation;
  cm->pc_Perennial = cps->cultivarParams.pc_Perennial;
  cm->pc_PlantDensity = cps->speciesParams.pc_PlantDensity;
  cm->pc_ResidueNRatio = cps->cultivarParams.pc_ResidueNRatio;
  cm->pc_RespiratoryStress = cps->cultivarParams.pc_RespiratoryStress;
  cm->vc_RootDensity = std::vector<double>(cm->soilColumn->size(), 0.0);
  cm->vc_RootDiameter = std::vector<double>(cm->soilColumn->size(), 0.0);
  cm->pc_RootDistributionParam = cps->speciesParams.pc_RootDistributionParam;
  cm->vc_RootEffectivity = std::vector<double>(cm->soilColumn->size(), 0.0);
  cm->pc_RootFormFactor = cps->speciesParams.pc_RootFormFactor;
  cm->pc_RootGrowthLag = cps->speciesParams.pc_RootGrowthLag;
  cm->pc_RootPenetrationRate = cps->speciesParams.pc_RootPenetrationRate;
  cm->vs_SoilMineralNContent = std::vector<double>(cm->soilColumn->size(), 0.0);
  cm->pc_SpecificLeafArea = cps->cultivarParams.pc_SpecificLeafArea;
  cm->pc_SpecificRootLength = cps->speciesParams.pc_SpecificRootLength;
  cm->pc_StageAfterCut = cps->speciesParams.pc_StageAfterCut - 1;
  cm->pc_StageAtMaxDiameter = cps->speciesParams.pc_StageAtMaxDiameter;
  cm->pc_StageAtMaxHeight = cps->speciesParams.pc_StageAtMaxHeight;
  cm->pc_StageMaxRootNConcentration = cps->speciesParams.pc_StageMaxRootNConcentration;
  cm->pc_StageKcFactor = cps->cultivarParams.pc_StageKcFactor;
  cm->pc_StageTemperatureSum = cps->cultivarParams.pc_StageTemperatureSum;
  cm->pc_StorageOrgan = cps->speciesParams.pc_StorageOrgan;
  cm->vc_TimeUnderAnoxiaThreshold = cm->cropPs->pc_TimeUnderAnoxiaThreshold;
  cm->vs_Tortuosity = cm->cropPs->pc_Tortuosity;
  cm->vc_Transpiration = std::vector<double>(cm->soilColumn->size(), 0.0);
  cm->vc_TranspirationRedux = std::vector<double>(cm->soilColumn->size(), 1.0);
  cm->pc_VernalisationRequirement = cps->cultivarParams.pc_VernalisationRequirement;
  cm->pc_WaterDeficitResponseOn = simPs.pc_WaterDeficitResponseOn;
  cm->vs_MaxEffectiveRootingDepth = stps->vs_MaxEffectiveRootingDepth;
  cm->vs_ImpenetrableLayerDepth = stps->vs_ImpenetrableLayerDepth;
  cm->_rad24 = std::vector<double>(cm->_stepSize24);
  cm->_rad240 = std::vector<double>(cm->_stepSize240);
  cm->_tfol24 = std::vector<double>(cm->_stepSize24);
  cm->_tfol240 = std::vector<double>(cm->_stepSize240);
  cm->__enable_vernalisation_factor_fix__ = cps->__enable_vernalisation_factor_fix__.
                                          orDefault(cm->cropPs->__enable_vernalisation_factor_fix__);

  auto& pc_StageTemperatureSum = cm->pc_StageTemperatureSum;
  auto& pc_NumberOfOrgans = cm->pc_NumberOfOrgans;
  auto& vc_OrganBiomass = cm->vc_OrganBiomass;
  auto& pc_AbovegroundOrgan = cm->pc_AbovegroundOrgan;
  auto& vc_AbovegroundBiomass = cm->vc_AbovegroundBiomass;
  auto& vc_RootBiomass = cm->vc_RootBiomass;
  auto& vc_LeafAreaIndex = cm->vc_LeafAreaIndex;
  auto& pc_SpecificLeafArea = cm->pc_SpecificLeafArea;
  auto& vc_DevelopmentalStage = cm->vc_DevelopmentalStage;
  auto& vc_TotalRootLength = cm->vc_TotalRootLength;
  auto& vc_TotalBiomassNContent = cm->vc_TotalBiomassNContent;
  auto& vc_NConcentrationAbovegroundBiomass = cm->vc_NConcentrationAbovegroundBiomass;
  auto& vc_NConcentrationRoot = cm->vc_NConcentrationRoot;
  auto& pc_NConcentrationAbovegroundBiomass = cm->pc_NConcentrationAbovegroundBiomass;
  auto& pc_NConcentrationRoot = cm->pc_NConcentrationRoot;
  auto& pc_CropSpecificMaxRootingDepth = cm->pc_CropSpecificMaxRootingDepth;
  auto& vc_MaxRootingDepth = cm->vc_MaxRootingDepth;
  auto& pc_StageKcFactor = cm->pc_StageKcFactor;
  auto& vc_Kcb_mid = cm->vc_Kcb_mid;
  auto& vc_Kcb_end = cm->vc_Kcb_end;
  auto& vc_KcbFactor = cm->vc_KcbFactor;
  auto& vc_Kcb_ini = cm->vc_Kcb_ini;

// Determining the total temperature sum of all developmental stages after
  // emergence (that's why i_Stage starts with 1) until before senescence
  for (int i_Stage = 1; i_Stage < cm->pc_NumberOfDevelopmentalStages - 1; i_Stage++) {
    cm->vc_TotalTemperatureSum += pc_StageTemperatureSum[i_Stage];
    if (i_Stage < cm->pc_NumberOfDevelopmentalStages - 3) cm->vc_TemperatureSumToFlowering += pc_StageTemperatureSum[i_Stage];
  }

  cm->vc_FinalDevelopmentalStage = cm->pc_NumberOfDevelopmentalStages - 1;

  // Determining the initial crop organ's biomass
  for (size_t i_Organ = 0; i_Organ < pc_NumberOfOrgans; i_Organ++) {
    vc_OrganBiomass[i_Organ] = cm->pc_InitialOrganBiomass[i_Organ]; // [kg ha-1]

    if (pc_AbovegroundOrgan[i_Organ]) vc_AbovegroundBiomass += cm->pc_InitialOrganBiomass[i_Organ]; // [kg ha-1]

    cm->vc_TotalBiomass += cm->pc_InitialOrganBiomass[i_Organ]; // [kg ha-1]

    // Define storage organ
    if (cm->pc_StorageOrgan[i_Organ]) cm->vc_StorageOrgan = i_Organ;
  }

  cm->vc_OrganGreenBiomass = vc_OrganBiomass;

  vc_RootBiomass = cm->pc_InitialOrganBiomass[0]; // [kg ha-1]

  // Initialisisng the leaf area index
  vc_LeafAreaIndex = vc_OrganBiomass[OId::LEAF] * pc_SpecificLeafArea[vc_DevelopmentalStage]; // [ha ha-1]

  if (vc_LeafAreaIndex <= 0.0) vc_LeafAreaIndex = 0.001;

  // Initialising the root
  vc_RootBiomass = vc_OrganBiomass[OId::ROOT];

  /** @todo Christian: Umrechnung korrekt wenn Biomasse in [kg m-2]? */
  vc_TotalRootLength = (vc_RootBiomass * 100000.0 * 100.0 / 7.0) / (0.015 * 0.015 * PI);

  vc_TotalBiomassNContent =
    (vc_AbovegroundBiomass * pc_NConcentrationAbovegroundBiomass) + (vc_RootBiomass * pc_NConcentrationRoot);
  vc_NConcentrationAbovegroundBiomass = pc_NConcentrationAbovegroundBiomass;
  vc_NConcentrationRoot = pc_NConcentrationRoot;

  // Initialising the initial maximum rooting depth
  if (cm->cropPs->pc_AdjustRootDepthForSoilProps) {
    //    double vc_SandContent = (*cm->soilColumn)[0]._sps.vs_SoilSandContent; // [kg kg-1]
    //    double vc_BulkDensity = (*cm->soilColumn)[0]._sps.vs_SoilBulkDensity(); // [kg m-3]
    //    if (vc_SandContent < 0.55) vc_SandContent = 0.55;
    //
    //    vc_SoilSpecificMaxRootingDepth = cm->vs_SoilSpecificMaxRootingDepth > 0.0
    //                                     ? cm->vs_SoilSpecificMaxRootingDepth
    //                                     : vc_SandContent * ((1.1 - vc_SandContent) / 0.275) *
    //                                       (1.4 / (vc_BulkDensity / 1000.0) +
    //                                        (vc_BulkDensity * vc_BulkDensity / 40000000.0)); // [m]
    //    vc_MaxRootingDepth = (vc_SoilSpecificMaxRootingDepth + (pc_CropSpecificMaxRootingDepth * 2.0)) / 3.0; //[m]
    //
    // std::cout << "old mrd: " << vc_MaxRootingDepth << " -> ";
    auto R_P_max = pc_CropSpecificMaxRootingDepth;
    double f_S = (*cm->soilColumn)[0]._sps.vs_SoilSandContent; // [kg kg-1]
    auto R_S = (f_S - 0.5) * -0.6;

    double rho_B = (*cm->soilColumn)[0]._sps.vs_SoilBulkDensity(); // [kg m-3]
    auto R_D = (rho_B / 1000.0 - 1) * -0.3;

    vc_MaxRootingDepth =
      R_P_max
      * ((R_P_max + (R_P_max * R_S)) / R_P_max)
      * ((R_P_max + (R_P_max * R_D)) / R_P_max);
    //std::cout << vc_MaxRootingDepth << " :new mrd" << endl;
  } else {
    vc_MaxRootingDepth = pc_CropSpecificMaxRootingDepth; //[m]
  }

  if (cm->vs_ImpenetrableLayerDepth > 0) vc_MaxRootingDepth = min(vc_MaxRootingDepth, cm->vs_ImpenetrableLayerDepth);

  // FAO-56 Dual Kc: Initialize GDD-based trapezoidal Kcb curve from pc_StageKcFactor.
  // vc_Kcb_ini defaults to 0.15 (FAO-56 Table 17 bare soil); setInitialKcb() may override it.
  if (!pc_StageKcFactor.empty()) {
    double max_Kc = *std::max_element(pc_StageKcFactor.begin(), pc_StageKcFactor.end());
    if (max_Kc >= 1.0) {
      vc_Kcb_mid = std::max(0.15, max_Kc - 0.05); // high-coverage crop (FAO-56 §7)
    } else {
      vc_Kcb_mid = std::max(0.15, max_Kc - 0.10); // low-coverage crop (FAO-56 §7)
    }
    vc_Kcb_end = std::max(0.0, pc_StageKcFactor.back() - 0.05);
  }
  vc_KcbFactor = vc_Kcb_ini; // start at initial value
}



double CropModule::sumStageTemperatureSums(int startAtStage, int endAtInclStage) const {
  double ts = 0;
  double endAtInclStage2 = endAtInclStage < 0 ? pc_NumberOfDevelopmentalStages + endAtInclStage + 1 : endAtInclStage;
  for (int s = startAtStage; s < endAtInclStage2; s++) ts += pc_StageTemperatureSum[s];
  return ts;
}

void monica::cropModuleDeserialize(CropModule* cm, mas::schema::model::monica::CropModuleState::Reader reader) {
  cm->_frostKillOn = reader.getFrostKillOn();
  cm->speciesPs.deserialize(reader.getSpeciesParams());
  cm->cultivarPs.deserialize(reader.getCultivarParams());
  cm->residuePs.deserialize(reader.getResidueParams());
  cm->_isWinterCrop = reader.getIsWinterCrop();
  cm->vs_Latitude = reader.getVsLatitude();
  cm->vc_AbovegroundBiomass = reader.getAbovegroundBiomass();
  cm->vc_AbovegroundBiomassOld = reader.getAbovegroundBiomassOld();
  setFromCapnpList(cm->pc_AbovegroundOrgan, reader.getPcAbovegroundOrgan());
  cm->vc_ActualTranspiration = reader.getActualTranspiration();

  {
    auto listReader = reader.getPcAssimilatePartitioningCoeff();
    cm->pc_AssimilatePartitioningCoeff.resize(listReader.size());
    capnp::uint i = 0;
    for (auto& v : cm->pc_AssimilatePartitioningCoeff) setFromCapnpList(v, listReader[i++]);
  }

  cm->pc_AssimilateReallocation = reader.getPcAssimilateReallocation();
  cm->vc_Assimilates = reader.getAssimilates();
  cm->vc_AssimilationRate = reader.getAssimilationRate();
  cm->vc_AstronomicDayLenght = reader.getAstronomicDayLenght();
  setFromCapnpList(cm->pc_BaseDaylength, reader.getPcBaseDaylength());
  setFromCapnpList(cm->pc_BaseTemperature, reader.getPcBaseTemperature());
  cm->pc_BeginSensitivePhaseHeatStress = reader.getPcBeginSensitivePhaseHeatStress();
  cm->vc_BelowgroundBiomass = reader.getBelowgroundBiomass();
  cm->vc_BelowgroundBiomassOld = reader.getBelowgroundBiomassOld();
  cm->pc_CarboxylationPathway = (int)reader.getPcCarboxylationPathway();
  cm->vc_ClearDayRadiation = reader.getClearDayRadiation();
  cm->pc_CO2Method = reader.getPcCo2Method();
  cm->vc_CriticalNConcentration = reader.getCriticalNConcentration();
  setFromCapnpList(cm->pc_CriticalOxygenContent, reader.getPcCriticalOxygenContent());
  cm->pc_CriticalTemperatureHeatStress = reader.getPcCriticalTemperatureHeatStress();
  cm->vc_CropDiameter = reader.getCropDiameter();
  cm->vc_CropFrostRedux = reader.getCropFrostRedux();
  cm->vc_CropHeatRedux = reader.getCropHeatRedux();
  cm->vc_CropHeight = reader.getCropHeight();
  cm->pc_CropHeightP1 = reader.getPcCropHeightP1();
  cm->pc_CropHeightP2 = reader.getPcCropHeightP2();
  cm->pc_CropName = reader.getPcCropName();
  cm->vc_CropNDemand = reader.getCropNDemand();
  cm->vc_CropNRedux = reader.getCropNRedux();
  cm->pc_CropSpecificMaxRootingDepth = reader.getPcCropSpecificMaxRootingDepth();
  setFromCapnpList(cm->vc_CropWaterUptake, reader.getCropWaterUptake());
  setFromCapnpList(cm->vc_CurrentTemperatureSum, reader.getCurrentTemperatureSum());
  cm->vc_CurrentTotalTemperatureSum = reader.getCurrentTotalTemperatureSum();
  cm->vc_CurrentTotalTemperatureSumRoot = reader.getCurrentTotalTemperatureSumRoot();
  cm->pc_CuttingDelayDays = reader.getPcCuttingDelayDays();
  cm->vc_DaylengthFactor = reader.getDaylengthFactor();
  setFromCapnpList(cm->pc_DaylengthRequirement, reader.getPcDaylengthRequirement());
  cm->vc_DaysAfterBeginFlowering = reader.getDaysAfterBeginFlowering();
  cm->vc_Declination = reader.getDeclination();
  cm->pc_DefaultRadiationUseEfficiency = reader.getPcDefaultRadiationUseEfficiency();
  cm->vm_DepthGroundwaterTable = reader.getVmDepthGroundwaterTable();
  cm->pc_DevelopmentAccelerationByNitrogenStress = (int)reader.getPcDevelopmentAccelerationByNitrogenStress();
  cm->vc_DevelopmentalStage = reader.getDevelopmentalStage();
  cm->_noOfCropSteps = reader.getNoOfCropSteps();
  cm->vc_DroughtImpactOnFertility = reader.getDroughtImpactOnFertility();
  cm->pc_DroughtImpactOnFertilityFactor = reader.getPcDroughtImpactOnFertilityFactor();
  setFromCapnpList(cm->pc_DroughtStressThreshold, reader.getPcDroughtStressThreshold());
  cm->pc_EmergenceFloodingControlOn = reader.getPcEmergenceFloodingControlOn();
  cm->pc_EmergenceMoistureControlOn = reader.getPcEmergenceMoistureControlOn();
  cm->pc_EndSensitivePhaseHeatStress = reader.getPcEndSensitivePhaseHeatStress();
  cm->vc_EffectiveDayLength = reader.getEffectiveDayLength();
  cm->vc_ErrorStatus = reader.getErrorStatus();
  cm->vc_ErrorMessage = reader.getErrorMessage();
  cm->vc_EvaporatedFromIntercept = reader.getEvaporatedFromIntercept();
  cm->vc_ExtraterrestrialRadiation = reader.getExtraterrestrialRadiation();
  cm->pc_FieldConditionModifier = reader.getPcFieldConditionModifier();
  cm->vc_FinalDevelopmentalStage = reader.getFinalDevelopmentalStage();
  cm->vc_FixedN = reader.getFixedN();
  // std::vector<double> vo_FreshSoilOrganicMatt
  cm->pc_FrostDehardening = reader.getPcFrostDehardening();
  cm->pc_FrostHardening = reader.getPcFrostHardening();
  cm->vc_GlobalRadiation = reader.getGlobalRadiation();
  cm->vc_GreenAreaIndex = reader.getGreenAreaIndex();
  cm->vc_GrossAssimilates = reader.getGrossAssimilates();
  cm->vc_GrossPhotosynthesis = reader.getGrossPhotosynthesis();
  cm->vc_GrossPhotosynthesis_mol = reader.getGrossPhotosynthesisMol();
  cm->vc_GrossPhotosynthesisReference_mol = reader.getGrossPhotosynthesisReferenceMol();
  cm->vc_GrossPrimaryProduction = reader.getGrossPrimaryProduction();
  cm->vc_GrowthCycleEnded = reader.getGrowthCycleEnded();
  cm->vc_GrowthRespirationAS = reader.getGrowthRespirationAS();
  cm->pc_HeatSumIrrigationStart = reader.getPcHeatSumIrrigationStart();
  cm->pc_HeatSumIrrigationEnd = reader.getPcHeatSumIrrigationEnd();
  cm->vs_HeightNN = reader.getVsHeightNN();
  cm->pc_InitialKcFactor = reader.getPcInitialKcFactor();
  setFromCapnpList(cm->pc_InitialOrganBiomass, reader.getPcInitialOrganBiomass());
  cm->pc_InitialRootingDepth = reader.getPcInitialRootingDepth();
  cm->vc_InterceptionStorage = reader.getInterceptionStorage();
  cm->vc_KcFactor = reader.getKcFactor();
  cm->vc_LeafAreaIndex = reader.getLeafAreaIndex();
  setFromCapnpList(cm->vc_sunlitLeafAreaIndex, reader.getSunlitLeafAreaIndex());
  setFromCapnpList(cm->vc_shadedLeafAreaIndex, reader.getShadedLeafAreaIndex());
  cm->pc_LowTemperatureExposure = reader.getPcLowTemperatureExposure();
  cm->pc_LimitingTemperatureHeatStress = reader.getPcLimitingTemperatureHeatStress();
  cm->vc_LT50 = reader.getLt50();
  cm->vc_LT50M = reader.getLt50m();
  cm->pc_LT50cultivar = reader.getPcLt50cultivar();
  cm->pc_LuxuryNCoeff = reader.getPcLuxuryNCoeff();
  cm->vc_MaintenanceRespirationAS = reader.getMaintenanceRespirationAS();
  cm->pc_MaxAssimilationRate = reader.getPcMaxAssimilationRate();
  cm->pc_MaxCropDiameter = reader.getPcMaxCropDiameter();
  cm->pc_MaxCropHeight = reader.getPcMaxCropHeight();
  cm->vc_MaxNUptake = reader.getMaxNUptake();
  cm->pc_MaxNUptakeParam = reader.getPcMaxNUptakeParam();
  cm->vc_MaxRootingDepth = reader.getPcMaxRootingDepth();
  cm->pc_MinimumNConcentration = reader.getPcMinimumNConcentration();
  cm->pc_MinimumTemperatureForAssimilation = reader.getPcMinimumTemperatureForAssimilation();
  cm->pc_OptimumTemperatureForAssimilation = reader.getPcOptimumTemperatureForAssimilation();
  cm->pc_MaximumTemperatureForAssimilation = reader.getPcMaximumTemperatureForAssimilation();
  cm->pc_MinimumTemperatureRootGrowth = reader.getPcMinimumTemperatureRootGrowth();
  cm->vc_NetMaintenanceRespiration = reader.getNetMaintenanceRespiration();
  cm->vc_NetPhotosynthesis = reader.getNetPhotosynthesis();
  cm->vc_NetPrecipitation = reader.getNetPrecipitation();
  cm->vc_NetPrimaryProduction = reader.getNetPrimaryProduction();
  cm->pc_NConcentrationAbovegroundBiomass = reader.getPcNConcentrationAbovegroundBiomass();
  cm->vc_NConcentrationAbovegroundBiomass = reader.getNConcentrationAbovegroundBiomass();
  cm->vc_NConcentrationAbovegroundBiomassOld = reader.getNConcentrationAbovegroundBiomassOld();
  cm->pc_NConcentrationB0 = reader.getPcNConcentrationB0();
  cm->vc_NContentDeficit = reader.getNContentDeficit();
  cm->pc_NConcentrationPN = reader.getPcNConcentrationPN();
  cm->pc_NConcentrationRoot = reader.getPcNConcentrationRoot();
  cm->vc_NConcentrationRoot = reader.getNConcentrationRoot();
  cm->vc_NConcentrationRootOld = reader.getNConcentrationRootOld();
  cm->pc_NitrogenResponseOn = reader.getPcNitrogenResponseOn();
  cm->pc_NumberOfDevelopmentalStages = (int)reader.getPcNumberOfDevelopmentalStages();
  cm->pc_NumberOfOrgans = (int)reader.getPcNumberOfOrgans();
  setFromCapnpList(cm->vc_NUptakeFromLayer, reader.getNUptakeFromLayer());
  setFromCapnpList(cm->pc_OptimumTemperature, reader.getPcOptimumTemperature());
  setFromCapnpList(cm->vc_OrganBiomass, reader.getOrganBiomass());
  setFromCapnpList(cm->vc_OrganDeadBiomass, reader.getOrganDeadBiomass());
  setFromCapnpList(cm->vc_OrganGreenBiomass, reader.getOrganGreenBiomass());
  setFromCapnpList(cm->vc_OrganGrowthIncrement, reader.getOrganGrowthIncrement());
  setFromCapnpList(cm->pc_OrganGrowthRespiration, reader.getPcOrganGrowthRespiration());
  setFromComplexCapnpList(cm->pc_OrganIdsForPrimaryYield, reader.getPcOrganIdsForPrimaryYield());
  setFromComplexCapnpList(cm->pc_OrganIdsForSecondaryYield, reader.getPcOrganIdsForSecondaryYield());
  setFromComplexCapnpList(cm->pc_OrganIdsForCutting, reader.getPcOrganIdsForCutting());
  setFromCapnpList(cm->pc_OrganMaintenanceRespiration, reader.getPcOrganMaintenanceRespiration());
  setFromCapnpList(cm->vc_OrganSenescenceIncrement, reader.getOrganSenescenceIncrement());

  {
    auto listReader = reader.getPcOrganSenescenceRate();
    cm->pc_OrganSenescenceRate.resize(listReader.size());
    capnp::uint i = 0;
    for (auto& v : cm->pc_OrganSenescenceRate) setFromCapnpList(v, listReader[i++]);
  }

  cm->vc_OvercastDayRadiation = reader.getOvercastDayRadiation();
  cm->vc_OxygenDeficit = reader.getOxygenDeficit();
  cm->pc_PartBiologicalNFixation = reader.getPcPartBiologicalNFixation();
  cm->pc_Perennial = reader.getPcPerennial();
  cm->vc_PhotoperiodicDaylength = reader.getPhotoperiodicDaylength();
  cm->vc_PhotActRadiationMean = reader.getPhotActRadiationMean();
  cm->pc_PlantDensity = reader.getPcPlantDensity();
  cm->vc_PotentialTranspiration = reader.getPotentialTranspiration();
  cm->vc_ReferenceEvapotranspiration = reader.getReferenceEvapotranspiration();
  cm->vc_RelativeTotalDevelopment = reader.getRelativeTotalDevelopment();
  cm->vc_RemainingEvapotranspiration = reader.getRemainingEvapotranspiration();
  cm->vc_ReserveAssimilatePool = reader.getReserveAssimilatePool();
  cm->pc_ResidueNRatio = reader.getPcResidueNRatio();
  cm->pc_RespiratoryStress = reader.getPcRespiratoryStress();
  cm->vc_RootBiomass = reader.getRootBiomass();
  cm->vc_RootBiomassOld = reader.getRootBiomassOld();
  setFromCapnpList(cm->vc_RootDensity, reader.getRootDensity());
  setFromCapnpList(cm->vc_RootDiameter, reader.getRootDiameter());
  cm->pc_RootDistributionParam = reader.getPcRootDistributionParam();
  setFromCapnpList(cm->vc_RootEffectivity, reader.getRootEffectivity());
  cm->pc_RootFormFactor = reader.getPcRootFormFactor();
  cm->pc_RootGrowthLag = reader.getPcRootGrowthLag();
  cm->vc_RootingDepth = reader.getRootingDepth();
  cm->vc_RootingDepth_m = reader.getRootingDepthM();
  cm->vc_RootingZone = reader.getRootingZone();
  cm->pc_RootPenetrationRate = reader.getPcRootPenetrationRate();
  cm->vm_SaturationDeficit = reader.getVmSaturationDeficit();
  cm->vc_SoilCoverage = reader.getSoilCoverage();
  setFromCapnpList(cm->vs_SoilMineralNContent, reader.getVsSoilMineralNContent());
  cm->vc_SoilSpecificMaxRootingDepth = reader.getSoilSpecificMaxRootingDepth();
  cm->vs_SoilSpecificMaxRootingDepth = reader.getVsSoilSpecificMaxRootingDepth();
  setFromCapnpList(cm->pc_SpecificLeafArea, reader.getPcSpecificLeafArea());
  cm->pc_SpecificRootLength = reader.getPcSpecificRootLength();
  cm->pc_StageAfterCut = reader.getPcStageAfterCut();
  cm->pc_StageAtMaxDiameter = reader.getPcStageAtMaxDiameter();
  cm->pc_StageAtMaxHeight = reader.getPcStageAtMaxHeight();
  setFromCapnpList(cm->pc_StageMaxRootNConcentration, reader.getPcStageMaxRootNConcentration());
  setFromCapnpList(cm->pc_StageKcFactor, reader.getPcStageKcFactor());
  setFromCapnpList(cm->pc_StageTemperatureSum, reader.getPcStageTemperatureSum());
  cm->vc_StomataResistance = reader.getStomataResistance();
  setFromCapnpList(cm->pc_StorageOrgan, reader.getPcStorageOrgan());
  cm->vc_StorageOrgan = reader.getStorageOrgan();
  cm->vc_TargetNConcentration = reader.getTargetNConcentration();
  cm->vc_TimeStep = reader.getTimeStep();
  cm->vc_TimeUnderAnoxia = (int)reader.getTimeUnderAnoxia();
  //cm->vc_TimeUnderAnoxiaThreshold = (int) reader.getTimeUnderAnoxiaThreshold();
  cm->vs_Tortuosity = reader.getVsTortuosity();
  cm->vc_TotalBiomass = reader.getTotalBiomass();
  cm->vc_TotalBiomassNContent = reader.getTotalBiomassNContent();
  cm->vc_TotalCropHeatImpact = reader.getTotalCropHeatImpact();
  cm->vc_TotalNInput = reader.getTotalNInput();
  cm->vc_TotalNUptake = reader.getTotalNUptake();
  cm->vc_TotalRespired = reader.getTotalRespired();
  cm->vc_Respiration = reader.getRespiration();
  cm->vc_SumTotalNUptake = reader.getSumTotalNUptake();
  cm->vc_TotalRootLength = reader.getTotalRootLength();
  cm->vc_TotalTemperatureSum = reader.getTotalTemperatureSum();
  cm->vc_TemperatureSumToFlowering = reader.getTemperatureSumToFlowering();
  setFromCapnpList(cm->vc_Transpiration, reader.getTranspiration());
  setFromCapnpList(cm->vc_TranspirationRedux, reader.getTranspirationRedux());
  cm->vc_TranspirationDeficit = reader.getTranspirationDeficit();
  cm->vc_VernalisationDays = reader.getVernalisationDays();
  cm->vc_VernalisationFactor = reader.getVernalisationFactor();
  setFromCapnpList(cm->pc_VernalisationRequirement, reader.getPcVernalisationRequirement());
  cm->pc_WaterDeficitResponseOn = reader.getPcWaterDeficitResponseOn();
  cm->dyingOut = reader.getDyingOut();
  cm->vc_AccumulatedETa = reader.getAccumulatedETa();
  cm->vc_AccumulatedTranspiration = reader.getAccumulatedTranspiration();
  cm->vc_sumExportedCutBiomass = reader.getSumExportedCutBiomass();
  cm->vc_exportedCutBiomass = reader.getExportedCutBiomass();
  cm->vc_sumResidueCutBiomass = reader.getSumResidueCutBiomass();
  cm->vc_residueCutBiomass = reader.getResidueCutBiomass();
  cm->vc_CuttingDelayDays = reader.getCuttingDelayDays();
  cm->vs_MaxEffectiveRootingDepth = reader.getVsMaxEffectiveRootingDepth();
  cm->vs_ImpenetrableLayerDepth = reader.getVsImpenetrableLayerDept();
  cm->vc_AnthesisDay = reader.getAnthesisDay();
  cm->vc_MaturityDay = reader.getMaturityDay();
  cm->vc_MaturityReached = reader.getMaturityReached();
  // VOC members
  cm->_stepSize24 = reader.getStepSize24();
  cm->_stepSize240 = reader.getStepSize240();
  setFromCapnpList(cm->_rad24, reader.getRad24());
  setFromCapnpList(cm->_rad240, reader.getRad240());
  setFromCapnpList(cm->_tfol24, reader.getTfol24());
  setFromCapnpList(cm->_tfol240, reader.getTfol240());
  cm->_index24 = reader.getIndex24();
  cm->_index240 = reader.getIndex240();
  cm->_full24 = reader.getFull24();
  cm->_full240 = reader.getFull240();
  cm->_guentherEmissions.deserialize(reader.getGuentherEmissions());
  cm->_jjvEmissions.deserialize(reader.getJjvEmissions());
  cm->_vocSpecies.deserialize(reader.getVocSpecies());
  cm->_cropPhotosynthesisResults.deserialize(reader.getCropPhotosynthesisResults());
  cm->vc_O3_shortTermDamage = reader.getO3ShortTermDamage();
  cm->vc_O3_longTermDamage = reader.getO3LongTermDamage();
  cm->vc_O3_senescence = reader.getO3Senescence();
  cm->vc_O3_sumUptake = reader.getO3SumUptake();
  cm->vc_O3_WStomatalClosure = reader.getO3WStomatalClosure();
  cm->_assimilatePartCoeffsReduced = reader.getAssimilatePartCoeffsReduced();
  cm->vc_KTkc = reader.getKtkc();
  cm->vc_KTko = reader.getKtko();
  cm->_stemElongationEventFired = reader.getStemElongationEventFired();
}

void monica::cropModuleSerialize(const CropModule* cm, mas::schema::model::monica::CropModuleState::Builder builder) {
  builder.setFrostKillOn(cm->_frostKillOn);
  cm->speciesPs.serialize(builder.initSpeciesParams());
  cm->cultivarPs.serialize(builder.initCultivarParams());
  cm->residuePs.serialize(builder.initResidueParams());
  builder.setIsWinterCrop(cm->_isWinterCrop);
  builder.setVsLatitude(cm->vs_Latitude);
  builder.setAbovegroundBiomass(cm->vc_AbovegroundBiomass);
  builder.setAbovegroundBiomassOld(cm->vc_AbovegroundBiomassOld);
  setCapnpList(cm->pc_AbovegroundOrgan, builder.initPcAbovegroundOrgan((capnp::uint)cm->pc_AbovegroundOrgan.size()));
  builder.setActualTranspiration(cm->vc_ActualTranspiration);

  {
    auto coeffs = builder.initPcAssimilatePartitioningCoeff((capnp::uint)cm->pc_AssimilatePartitioningCoeff.size());
    capnp::uint i = 0;
    for (const auto& v : cm->pc_AssimilatePartitioningCoeff) setCapnpList(v, coeffs.init(i++, (capnp::uint)v.size()));
  }

  builder.setPcAssimilateReallocation(cm->pc_AssimilateReallocation);
  builder.setAssimilates(cm->vc_Assimilates);
  builder.setAssimilationRate(cm->vc_AssimilationRate);
  builder.setAstronomicDayLenght(cm->vc_AstronomicDayLenght);
  setCapnpList(cm->pc_BaseDaylength, builder.initPcBaseDaylength((capnp::uint)cm->pc_BaseDaylength.size()));
  setCapnpList(cm->pc_BaseTemperature, builder.initPcBaseTemperature((capnp::uint)cm->pc_BaseTemperature.size()));
  builder.setPcBeginSensitivePhaseHeatStress(cm->pc_BeginSensitivePhaseHeatStress);
  builder.setBelowgroundBiomass(cm->vc_BelowgroundBiomass);
  builder.setBelowgroundBiomassOld(cm->vc_BelowgroundBiomassOld);
  builder.setPcCarboxylationPathway(cm->pc_CarboxylationPathway);
  builder.setClearDayRadiation(cm->vc_ClearDayRadiation);
  builder.setPcCo2Method(cm->pc_CO2Method);
  builder.setCriticalNConcentration(cm->vc_CriticalNConcentration);
  setCapnpList(cm->pc_CriticalOxygenContent,
               builder.initPcCriticalOxygenContent((capnp::uint)cm->pc_CriticalOxygenContent.size()));
  builder.setPcCriticalTemperatureHeatStress(cm->pc_CriticalTemperatureHeatStress);
  builder.setCropDiameter(cm->vc_CropDiameter);
  builder.setCropFrostRedux(cm->vc_CropFrostRedux);
  builder.setCropHeatRedux(cm->vc_CropHeatRedux);
  builder.setCropHeight(cm->vc_CropHeight);
  builder.setPcCropHeightP1(cm->pc_CropHeightP1);
  builder.setPcCropHeightP2(cm->pc_CropHeightP2);
  builder.setPcCropName(cm->pc_CropName);
  builder.setCropNDemand(cm->vc_CropNDemand);
  builder.setCropNRedux(cm->vc_CropNRedux);
  builder.setPcCropSpecificMaxRootingDepth(cm->pc_CropSpecificMaxRootingDepth);
  setCapnpList(cm->vc_CropWaterUptake, builder.initCropWaterUptake((capnp::uint)cm->vc_CropWaterUptake.size()));
  setCapnpList(cm->vc_CurrentTemperatureSum,
               builder.initCurrentTemperatureSum((capnp::uint)cm->vc_CurrentTemperatureSum.size()));
  builder.setCurrentTotalTemperatureSum(cm->vc_CurrentTotalTemperatureSum);
  builder.setCurrentTotalTemperatureSumRoot(cm->vc_CurrentTotalTemperatureSumRoot);
  builder.setPcCuttingDelayDays(cm->pc_CuttingDelayDays);
  builder.setDaylengthFactor(cm->vc_DaylengthFactor);
  setCapnpList(cm->pc_DaylengthRequirement,
               builder.initPcDaylengthRequirement((capnp::uint)cm->pc_DaylengthRequirement.size()));
  builder.setDaysAfterBeginFlowering(cm->vc_DaysAfterBeginFlowering);
  builder.setDeclination(cm->vc_Declination);
  builder.setPcDefaultRadiationUseEfficiency(cm->pc_DefaultRadiationUseEfficiency);
  builder.setVmDepthGroundwaterTable(cm->vm_DepthGroundwaterTable);
  builder.setPcDevelopmentAccelerationByNitrogenStress(cm->pc_DevelopmentAccelerationByNitrogenStress);
  builder.setDevelopmentalStage((uint16_t)cm->vc_DevelopmentalStage);
  builder.setNoOfCropSteps(cm->_noOfCropSteps);
  builder.setDroughtImpactOnFertility(cm->vc_DroughtImpactOnFertility);
  builder.setPcDroughtImpactOnFertilityFactor(cm->pc_DroughtImpactOnFertilityFactor);
  setCapnpList(cm->pc_DroughtStressThreshold,
               builder.initPcDroughtStressThreshold((capnp::uint)cm->pc_DroughtStressThreshold.size()));
  builder.setPcEmergenceFloodingControlOn(cm->pc_EmergenceFloodingControlOn);
  builder.setPcEmergenceMoistureControlOn(cm->pc_EmergenceMoistureControlOn);
  builder.setPcEndSensitivePhaseHeatStress(cm->pc_EndSensitivePhaseHeatStress);
  builder.setEffectiveDayLength(cm->vc_EffectiveDayLength);
  builder.setErrorStatus(cm->vc_ErrorStatus);
  builder.setErrorMessage(cm->vc_ErrorMessage);
  builder.setEvaporatedFromIntercept(cm->vc_EvaporatedFromIntercept);
  builder.setExtraterrestrialRadiation(cm->vc_ExtraterrestrialRadiation);
  builder.setPcFieldConditionModifier(cm->pc_FieldConditionModifier);
  builder.setFinalDevelopmentalStage((uint16_t)cm->vc_FinalDevelopmentalStage);
  builder.setFixedN(cm->vc_FixedN);
  // std::vector<double> vo_FreshSoilOrganicMatt
  builder.setPcFrostDehardening(cm->pc_FrostDehardening);
  builder.setPcFrostHardening(cm->pc_FrostHardening);
  builder.setGlobalRadiation(cm->vc_GlobalRadiation);
  builder.setGreenAreaIndex(cm->vc_GreenAreaIndex);
  builder.setGrossAssimilates(cm->vc_GrossAssimilates);
  builder.setGrossPhotosynthesis(cm->vc_GrossPhotosynthesis);
  builder.setGrossPhotosynthesisMol(cm->vc_GrossPhotosynthesis_mol);
  builder.setGrossPhotosynthesisReferenceMol(cm->vc_GrossPhotosynthesisReference_mol);
  builder.setGrossPrimaryProduction(cm->vc_GrossPrimaryProduction);
  builder.setGrowthCycleEnded(cm->vc_GrowthCycleEnded);
  builder.setGrowthRespirationAS(cm->vc_GrowthRespirationAS);
  builder.setPcHeatSumIrrigationStart(cm->pc_HeatSumIrrigationStart);
  builder.setPcHeatSumIrrigationEnd(cm->pc_HeatSumIrrigationEnd);
  builder.setVsHeightNN(cm->vs_HeightNN);
  builder.setPcInitialKcFactor(cm->pc_InitialKcFactor);
  setCapnpList(cm->pc_InitialOrganBiomass, builder.initPcInitialOrganBiomass((capnp::uint)cm->pc_InitialOrganBiomass.size()));
  builder.setPcInitialRootingDepth(cm->pc_InitialRootingDepth);
  builder.setInterceptionStorage(cm->vc_InterceptionStorage);
  builder.setKcFactor(cm->vc_KcFactor);
  builder.setLeafAreaIndex(cm->vc_LeafAreaIndex);
  setCapnpList(cm->vc_sunlitLeafAreaIndex, builder.initSunlitLeafAreaIndex((capnp::uint)cm->vc_sunlitLeafAreaIndex.size()));
  setCapnpList(cm->vc_shadedLeafAreaIndex, builder.initShadedLeafAreaIndex((capnp::uint)cm->vc_shadedLeafAreaIndex.size()));
  builder.setPcLowTemperatureExposure(cm->pc_LowTemperatureExposure);
  builder.setPcLimitingTemperatureHeatStress(cm->pc_LimitingTemperatureHeatStress);
  builder.setLt50(cm->vc_LT50);
  builder.setLt50m(cm->vc_LT50M);
  builder.setPcLt50cultivar(cm->pc_LT50cultivar);
  builder.setPcLuxuryNCoeff(cm->pc_LuxuryNCoeff);
  builder.setMaintenanceRespirationAS(cm->vc_MaintenanceRespirationAS);
  builder.setPcMaxAssimilationRate(cm->pc_MaxAssimilationRate);
  builder.setPcMaxCropDiameter(cm->pc_MaxCropDiameter);
  builder.setPcMaxCropHeight(cm->pc_MaxCropHeight);
  builder.setMaxNUptake(cm->vc_MaxNUptake);
  builder.setPcMaxNUptakeParam(cm->pc_MaxNUptakeParam);
  builder.setPcMaxRootingDepth(cm->vc_MaxRootingDepth);
  builder.setPcMinimumNConcentration(cm->pc_MinimumNConcentration);
  builder.setPcMinimumTemperatureForAssimilation(cm->pc_MinimumTemperatureForAssimilation);
  builder.setPcOptimumTemperatureForAssimilation(cm->pc_OptimumTemperatureForAssimilation);
  builder.setPcMaximumTemperatureForAssimilation(cm->pc_MaximumTemperatureForAssimilation);
  builder.setPcMinimumTemperatureRootGrowth(cm->pc_MinimumTemperatureRootGrowth);
  builder.setNetMaintenanceRespiration(cm->vc_NetMaintenanceRespiration);
  builder.setNetPhotosynthesis(cm->vc_NetPhotosynthesis);
  builder.setNetPrecipitation(cm->vc_NetPrecipitation);
  builder.setNetPrimaryProduction(cm->vc_NetPrimaryProduction);
  builder.setPcNConcentrationAbovegroundBiomass(cm->pc_NConcentrationAbovegroundBiomass);
  builder.setNConcentrationAbovegroundBiomass(cm->vc_NConcentrationAbovegroundBiomass);
  builder.setNConcentrationAbovegroundBiomassOld(cm->vc_NConcentrationAbovegroundBiomassOld);
  builder.setPcNConcentrationB0(cm->pc_NConcentrationB0);
  builder.setNContentDeficit(cm->vc_NContentDeficit);
  builder.setPcNConcentrationPN(cm->pc_NConcentrationPN);
  builder.setPcNConcentrationRoot(cm->pc_NConcentrationRoot);
  builder.setNConcentrationRoot(cm->vc_NConcentrationRoot);
  builder.setNConcentrationRootOld(cm->vc_NConcentrationRootOld);
  builder.setPcNitrogenResponseOn(cm->pc_NitrogenResponseOn);
  builder.setPcNumberOfDevelopmentalStages(cm->pc_NumberOfDevelopmentalStages);
  builder.setPcNumberOfOrgans(cm->pc_NumberOfOrgans);
  setCapnpList(cm->vc_NUptakeFromLayer, builder.initNUptakeFromLayer((capnp::uint)cm->vc_NUptakeFromLayer.size()));
  setCapnpList(cm->pc_OptimumTemperature, builder.initPcOptimumTemperature((capnp::uint)cm->pc_OptimumTemperature.size()));
  setCapnpList(cm->vc_OrganBiomass, builder.initOrganBiomass((capnp::uint)cm->vc_OrganBiomass.size()));
  setCapnpList(cm->vc_OrganDeadBiomass, builder.initOrganDeadBiomass((capnp::uint)cm->vc_OrganDeadBiomass.size()));
  setCapnpList(cm->vc_OrganGreenBiomass, builder.initOrganGreenBiomass((capnp::uint)cm->vc_OrganGreenBiomass.size()));
  setCapnpList(cm->vc_OrganGrowthIncrement, builder.initOrganGrowthIncrement((capnp::uint)cm->vc_OrganGrowthIncrement.size()));
  setCapnpList(cm->pc_OrganGrowthRespiration,
               builder.initPcOrganGrowthRespiration((capnp::uint)cm->pc_OrganGrowthRespiration.size()));
  setComplexCapnpList(cm->pc_OrganIdsForPrimaryYield,
                      builder.initPcOrganIdsForPrimaryYield((capnp::uint)cm->pc_OrganIdsForPrimaryYield.size()));
  setComplexCapnpList(cm->pc_OrganIdsForSecondaryYield,
                      builder.initPcOrganIdsForSecondaryYield((capnp::uint)cm->pc_OrganIdsForSecondaryYield.size()));
  setComplexCapnpList(cm->pc_OrganIdsForCutting,
                      builder.initPcOrganIdsForCutting((capnp::uint)cm->pc_OrganIdsForCutting.size()));
  setCapnpList(cm->pc_OrganMaintenanceRespiration,
               builder.initPcOrganMaintenanceRespiration((capnp::uint)cm->pc_OrganMaintenanceRespiration.size()));
  setCapnpList(cm->vc_OrganSenescenceIncrement,
               builder.initOrganSenescenceIncrement((capnp::uint)cm->vc_OrganSenescenceIncrement.size()));

  {
    auto listBuilder = builder.initPcOrganSenescenceRate((capnp::uint)cm->pc_OrganSenescenceRate.size());
    capnp::uint i = 0;
    for (const auto& v : cm->pc_OrganSenescenceRate) setCapnpList(v, listBuilder.init(i++, (capnp::uint)v.size()));
  }

  builder.setOvercastDayRadiation(cm->vc_OvercastDayRadiation);
  builder.setOxygenDeficit(cm->vc_OxygenDeficit);
  builder.setPcPartBiologicalNFixation(cm->pc_PartBiologicalNFixation);
  builder.setPcPerennial(cm->pc_Perennial);
  builder.setPhotoperiodicDaylength(cm->vc_PhotoperiodicDaylength);
  builder.setPhotActRadiationMean(cm->vc_PhotActRadiationMean);
  builder.setPcPlantDensity(cm->pc_PlantDensity);
  builder.setPotentialTranspiration(cm->vc_PotentialTranspiration);
  builder.setReferenceEvapotranspiration(cm->vc_ReferenceEvapotranspiration);
  builder.setRelativeTotalDevelopment(cm->vc_RelativeTotalDevelopment);
  builder.setRemainingEvapotranspiration(cm->vc_RemainingEvapotranspiration);
  builder.setReserveAssimilatePool(cm->vc_ReserveAssimilatePool);
  builder.setPcResidueNRatio(cm->pc_ResidueNRatio);
  builder.setPcRespiratoryStress(cm->pc_RespiratoryStress);
  builder.setRootBiomass(cm->vc_RootBiomass);
  builder.setRootBiomassOld(cm->vc_RootBiomassOld);
  setCapnpList(cm->vc_RootDensity, builder.initRootDensity((capnp::uint)cm->vc_RootDensity.size()));
  setCapnpList(cm->vc_RootDiameter, builder.initRootDiameter((capnp::uint)cm->vc_RootDiameter.size()));
  builder.setPcRootDistributionParam(cm->pc_RootDistributionParam);
  setCapnpList(cm->vc_RootEffectivity, builder.initRootEffectivity((capnp::uint)cm->vc_RootEffectivity.size()));
  builder.setPcRootFormFactor(cm->pc_RootFormFactor);
  builder.setPcRootGrowthLag(cm->pc_RootGrowthLag);
  builder.setRootingDepth((uint16_t)cm->vc_RootingDepth);
  builder.setRootingDepthM(cm->vc_RootingDepth_m);
  builder.setRootingZone((uint16_t)cm->vc_RootingZone);
  builder.setPcRootPenetrationRate(cm->pc_RootPenetrationRate);
  builder.setVmSaturationDeficit(cm->vm_SaturationDeficit);
  builder.setSoilCoverage(cm->vc_SoilCoverage);
  setCapnpList(cm->vs_SoilMineralNContent, builder.initVsSoilMineralNContent((capnp::uint)cm->vs_SoilMineralNContent.size()));
  builder.setSoilSpecificMaxRootingDepth(cm->vc_SoilSpecificMaxRootingDepth);
  builder.setVsSoilSpecificMaxRootingDepth(cm->vs_SoilSpecificMaxRootingDepth);
  setCapnpList(cm->pc_SpecificLeafArea, builder.initPcSpecificLeafArea((capnp::uint)cm->pc_SpecificLeafArea.size()));
  builder.setPcSpecificRootLength(cm->pc_SpecificRootLength);
  builder.setPcStageAfterCut(cm->pc_StageAfterCut);
  builder.setPcStageAtMaxDiameter(cm->pc_StageAtMaxDiameter);
  builder.setPcStageAtMaxHeight(cm->pc_StageAtMaxHeight);
  setCapnpList(cm->pc_StageMaxRootNConcentration,
               builder.initPcStageMaxRootNConcentration((capnp::uint)cm->pc_StageMaxRootNConcentration.size()));
  setCapnpList(cm->pc_StageKcFactor, builder.initPcStageKcFactor((capnp::uint)cm->pc_StageKcFactor.size()));
  setCapnpList(cm->pc_StageTemperatureSum, builder.initPcStageTemperatureSum((capnp::uint)cm->pc_StageTemperatureSum.size()));
  builder.setStomataResistance(cm->vc_StomataResistance);
  setCapnpList(cm->pc_StorageOrgan, builder.initPcStorageOrgan((capnp::uint)cm->pc_StorageOrgan.size()));
  builder.setStorageOrgan(cm->vc_StorageOrgan);
  builder.setTargetNConcentration(cm->vc_TargetNConcentration);
  builder.setTimeStep(cm->vc_TimeStep);
  builder.setTimeUnderAnoxia(cm->vc_TimeUnderAnoxia);
  //builder.setTimeUnderAnoxiaThreshold(cm->vc_TimeUnderAnoxiaTheshold);
  builder.setVsTortuosity(cm->vs_Tortuosity);
  builder.setTotalBiomass(cm->vc_TotalBiomass);
  builder.setTotalBiomassNContent(cm->vc_TotalBiomassNContent);
  builder.setTotalCropHeatImpact(cm->vc_TotalCropHeatImpact);
  builder.setTotalNInput(cm->vc_TotalNInput);
  builder.setTotalNUptake(cm->vc_TotalNUptake);
  builder.setTotalRespired(cm->vc_TotalRespired);
  builder.setRespiration(cm->vc_Respiration);
  builder.setSumTotalNUptake(cm->vc_SumTotalNUptake);
  builder.setTotalRootLength(cm->vc_TotalRootLength);
  builder.setTotalTemperatureSum(cm->vc_TotalTemperatureSum);
  builder.setTemperatureSumToFlowering(cm->vc_TemperatureSumToFlowering);
  setCapnpList(cm->vc_Transpiration, builder.initTranspiration((capnp::uint)cm->vc_Transpiration.size()));
  setCapnpList(cm->vc_TranspirationRedux, builder.initTranspirationRedux((capnp::uint)cm->vc_TranspirationRedux.size()));
  builder.setTranspirationDeficit(cm->vc_TranspirationDeficit);
  builder.setVernalisationDays(cm->vc_VernalisationDays);
  builder.setVernalisationFactor(cm->vc_VernalisationFactor);
  setCapnpList(cm->pc_VernalisationRequirement,
               builder.initPcVernalisationRequirement((capnp::uint)cm->pc_VernalisationRequirement.size()));
  builder.setPcWaterDeficitResponseOn(cm->pc_WaterDeficitResponseOn);
  builder.setDyingOut(cm->dyingOut);
  builder.setAccumulatedETa(cm->vc_AccumulatedETa);
  builder.setAccumulatedTranspiration(cm->vc_AccumulatedTranspiration);
  builder.setSumExportedCutBiomass(cm->vc_sumExportedCutBiomass);
  builder.setExportedCutBiomass(cm->vc_exportedCutBiomass);
  builder.setSumResidueCutBiomass(cm->vc_sumResidueCutBiomass);
  builder.setResidueCutBiomass(cm->vc_residueCutBiomass);
  builder.setCuttingDelayDays(cm->vc_CuttingDelayDays);
  builder.setVsMaxEffectiveRootingDepth(cm->vs_MaxEffectiveRootingDepth);
  builder.setVsImpenetrableLayerDept(cm->vs_ImpenetrableLayerDepth);
  builder.setAnthesisDay(cm->vc_AnthesisDay);
  builder.setMaturityDay(cm->vc_MaturityDay);
  builder.setMaturityReached(cm->vc_MaturityReached);
  builder.setStepSize24(cm->_stepSize24);
  builder.setStepSize240(cm->_stepSize240);
  setCapnpList(cm->_rad24, builder.initRad24((capnp::uint)cm->_rad24.size()));
  setCapnpList(cm->_rad240, builder.initRad240((capnp::uint)cm->_rad240.size()));
  setCapnpList(cm->_tfol24, builder.initTfol24((capnp::uint)cm->_tfol24.size()));
  setCapnpList(cm->_tfol240, builder.initTfol240((capnp::uint)cm->_tfol240.size()));
  builder.setIndex24(cm->_index24);
  builder.setIndex240(cm->_index240);
  builder.setFull24(cm->_full24);
  builder.setFull240(cm->_full240);
  cm->_guentherEmissions.serialize(builder.initGuentherEmissions());
  cm->_jjvEmissions.serialize(builder.initJjvEmissions());
  cm->_vocSpecies.serialize(builder.initVocSpecies());
  cm->_cropPhotosynthesisResults.serialize(builder.initCropPhotosynthesisResults());
  builder.setO3ShortTermDamage(cm->vc_O3_shortTermDamage);
  builder.setO3LongTermDamage(cm->vc_O3_longTermDamage);
  builder.setO3Senescence(cm->vc_O3_senescence);
  builder.setO3SumUptake(cm->vc_O3_sumUptake);
  builder.setO3WStomatalClosure(cm->vc_O3_WStomatalClosure);
  builder.setAssimilatePartCoeffsReduced(cm->_assimilatePartCoeffsReduced);
  builder.setKtkc(cm->vc_KTkc);
  builder.setKtko(cm->vc_KTko);
  builder.setStemElongationEventFired(cm->_stemElongationEventFired);
}

/**
 * @brief Calculates a single time step.
 *
 * @param vw_MeanAirTemperature Mean aire temperature according to weather data
 * @param vw_GlobalRadiation Global radiation
 * @param vw_SunshineHours Number of hours the sun has been shining the day (needed for photosynthesis)
 * @param vs_JulianDay Date in julian notation
 * @param vw_MaxAirTemperature Maximal air temperature for the calculated day
 * @param vw_MinAirTemperature, Minimal air temperature for the calculated day
 * @param vw_RelativeHumidity Relative humidity
 * @param vw_WindSpeed Spped of wind
 * @param vw_WindSpeedHeight Height in which the wind speed has been measured
 * @param vw_AtmosphericCO2Concentration CO2 concentration in the athmosphere (needed for photosynthesis)
 * @param vw_GrossPrecipitation Precipitation
 *
 * @author Claas Nendel
 */
void monica::cropModuleStep(CropModule* cm,
                             double meanAirTemperature,
                             double maxAirTemperature,
                             double minAirTemperature,
                             double globalRadiation,
                             double sunshineHours,
                             Tools::Date currentDate,
                             double relativeHumidity,
                             double windSpeed,
                             double windSpeedHeight,
                             double atmosphericCO2Concentration,
                             double atmosphericO3Concentration,
                             double grossPrecipitation,
                             double referenceEvapotranspiration) {
  auto& _bareSoilKcFactor = cm->_bareSoilKcFactor;
  auto& _fireEvent = cm->_fireEvent;
  auto& _frostKillOn = cm->_frostKillOn;
  auto& _intercropping = cm->_intercropping;
  auto& _intercroppingOtherCropHeight = cm->_intercroppingOtherCropHeight;
  auto& _noOfCropSteps = cm->_noOfCropSteps;
  auto& _perennialCropDormancyPeriodEndDate = cm->_perennialCropDormancyPeriodEndDate;
  auto& _stemElongationEventFired = cm->_stemElongationEventFired;
  const auto* cropPs = cm->cropPs;
  auto& pc_BaseDaylength = cm->pc_BaseDaylength;
  auto& pc_CriticalOxygenContent = cm->pc_CriticalOxygenContent;
  auto& pc_DaylengthRequirement = cm->pc_DaylengthRequirement;
  auto& pc_MaxCropHeight = cm->pc_MaxCropHeight;
  auto& pc_Perennial = cm->pc_Perennial;
  auto& pc_SpecificLeafArea = cm->pc_SpecificLeafArea;
  auto& pc_StageKcFactor = cm->pc_StageKcFactor;
  auto& pc_StageTemperatureSum = cm->pc_StageTemperatureSum;
  auto& pc_VernalisationRequirement = cm->pc_VernalisationRequirement;
  auto* soilColumn = cm->soilColumn;
  auto& speciesPs = cm->speciesPs;
  auto& vc_AnthesisDay = cm->vc_AnthesisDay;
  auto& vc_CropHeight = cm->vc_CropHeight;
  auto& vc_CurrentTemperatureSum = cm->vc_CurrentTemperatureSum;
  auto& vc_CurrentTotalTemperatureSum = cm->vc_CurrentTotalTemperatureSum;
  auto& vc_CuttingDelayDays = cm->vc_CuttingDelayDays;
  auto& vc_DaylengthFactor = cm->vc_DaylengthFactor;
  auto& vc_DaysSinceTransplant = cm->vc_DaysSinceTransplant;
  auto& vc_DevelopmentalStage = cm->vc_DevelopmentalStage;
  auto& vc_EffectiveDayLength = cm->vc_EffectiveDayLength;
  auto& vc_GrossPrimaryProduction = cm->vc_GrossPrimaryProduction;
  auto& vc_KcFactor = cm->vc_KcFactor;
  auto& vc_KcbFactor = cm->vc_KcbFactor;
  auto& vc_Kcb_end = cm->vc_Kcb_end;
  auto& vc_Kcb_ini = cm->vc_Kcb_ini;
  auto& vc_Kcb_mid = cm->vc_Kcb_mid;
  auto& vc_MaturityDay = cm->vc_MaturityDay;
  auto& vc_MaturityReached = cm->vc_MaturityReached;
  auto& vc_NetPrimaryProduction = cm->vc_NetPrimaryProduction;
  auto& vc_OrganGrowthIncrement = cm->vc_OrganGrowthIncrement;
  auto& vc_OrganSenescenceIncrement = cm->vc_OrganSenescenceIncrement;
  auto& vc_OxygenDeficit = cm->vc_OxygenDeficit;
  auto& vc_PhotoperiodicDaylength = cm->vc_PhotoperiodicDaylength;
  auto& vc_ReferenceEvapotranspiration = cm->vc_ReferenceEvapotranspiration;
  auto& vc_RelativeTotalDevelopment = cm->vc_RelativeTotalDevelopment;
  auto& vc_SoilCoverage = cm->vc_SoilCoverage;
  auto& vc_TotalRespired = cm->vc_TotalRespired;
  auto& vc_TotalTemperatureSum = cm->vc_TotalTemperatureSum;
  auto& vc_TransplantEfficiency = cm->vc_TransplantEfficiency;
  auto& vc_TransplantShockDuration = cm->vc_TransplantShockDuration;
  auto& vc_VernalisationDays = cm->vc_VernalisationDays;
  auto& vc_VernalisationFactor = cm->vc_VernalisationFactor;

  int vs_JulianDay = int(currentDate.julianDay());

  if (vc_CuttingDelayDays > 0) vc_CuttingDelayDays--;

  // [TRANSPLANT SHOCK] Daily stress recovery calculation.
  // The daily transplant efficiency factors in transplant shock, increasing linearly
  // from a baseline of 0.2 (80% initial stress) to 1.0 (no stress) over the post-transplant delay duration.
  vc_TransplantEfficiency = 1.0;
  if (vc_DaysSinceTransplant >= 0 && vc_DaysSinceTransplant < vc_TransplantShockDuration) {
    vc_TransplantEfficiency = 0.2 + 0.8 * (double(vc_DaysSinceTransplant) / vc_TransplantShockDuration);
    vc_DaysSinceTransplant++;
  } else if (vc_DaysSinceTransplant >= vc_TransplantShockDuration) {
    vc_DaysSinceTransplant = -1; // Recovery period has successfully concluded
  }

  //  cout << "Cropstep: " << minAirTemperature << "\t" << maxAirTemperature << "\t" << meanAirTemperature << endl;
  cropModuleFcRadiation(cm, vs_JulianDay, globalRadiation, sunshineHours);

  vc_OxygenDeficit = cropModuleFcOxygenDeficiency(cm, pc_CriticalOxygenContent[vc_DevelopmentalStage]);

  size_t old_DevelopmentalStage = vc_DevelopmentalStage;

  // start accumulating temperature sums only after dormancy
  if (!_perennialCropDormancyPeriodEndDate.isValid()) {
    _perennialCropDormancyPeriodEndDate =
      speciesPs.dormancyEndDoy == 0
        ? currentDate
        : Date(1, 1, currentDate.year()) + (speciesPs.dormancyEndDoy - 1);
  }
  if (!pc_Perennial || currentDate >= _perennialCropDormancyPeriodEndDate) {
    cropModuleFcCropDevelopmentalStage(cm,
                                       meanAirTemperature,
                                       (*soilColumn)[0].vs_SoilMoisture_m3,
                                       (*soilColumn)[0]._sps.vs_FieldCapacity,
                                       (*soilColumn)[0]._sps.vs_PermanentWiltingPoint,
                                       currentDate);
  }

  if (old_DevelopmentalStage == 0 && vc_DevelopmentalStage == 1) {
    if (_fireEvent) _fireEvent("emergence");
  } else if (cm->isAnthesisDay(old_DevelopmentalStage, vc_DevelopmentalStage)) {
    vc_AnthesisDay = vs_JulianDay;
    if (_fireEvent) _fireEvent("anthesis");
  } else if (cm->isMaturityDay(old_DevelopmentalStage, vc_DevelopmentalStage)) {
    vc_MaturityDay = vs_JulianDay;
    vc_MaturityReached = true;
    if (_fireEvent) _fireEvent("maturity");
  }

  if (!_stemElongationEventFired &&
      vc_CurrentTotalTemperatureSum >= pc_StageTemperatureSum[2] * 0.25 + pc_StageTemperatureSum[1]) {
    _fireEvent("cereal-stem-elongation");
    _stemElongationEventFired = true;
  }

  // fire stage event on stage change or right after sowing
  if (old_DevelopmentalStage != vc_DevelopmentalStage || _noOfCropSteps == 0) {
    if (_fireEvent) _fireEvent(string("Stage-") + to_string(vc_DevelopmentalStage + 1));
  }

  vc_DaylengthFactor =
    cropModuleFcDaylengthFactor(cm,
                                pc_DaylengthRequirement[vc_DevelopmentalStage],
                                vc_EffectiveDayLength,
                                vc_PhotoperiodicDaylength,
                                pc_BaseDaylength[vc_DevelopmentalStage]);

  tie(vc_VernalisationFactor, vc_VernalisationDays) =
    cropModuleFcVernalisationFactor(cm,
                                    meanAirTemperature,
                                    pc_VernalisationRequirement[vc_DevelopmentalStage],
                                    vc_VernalisationDays);

  if (vc_TotalTemperatureSum == 0.0) {
    vc_RelativeTotalDevelopment = 0.0;
  } else {
    vc_RelativeTotalDevelopment = vc_CurrentTotalTemperatureSum / vc_TotalTemperatureSum;
  }

  if (vc_DevelopmentalStage == 0) {
    vc_KcFactor = _bareSoilKcFactor; /** @todo Claas: muss hier etwas Genaueres hin, siehe FAO? */
  } else {
    vc_KcFactor = cropModuleFcKcFactor(cm,
                                       pc_StageTemperatureSum[vc_DevelopmentalStage],
                                       vc_CurrentTemperatureSum[vc_DevelopmentalStage],
                                       pc_StageKcFactor[vc_DevelopmentalStage],
                                       pc_StageKcFactor[vc_DevelopmentalStage - 1]);
  }

  // FAO-56 Dual Kc: GDD-based 4-phase trapezoidal Kcb curve (replaces static per-stage arrays).
  // Phase 1 (flat initial) | Phase 2 (linear ascent) | Phase 3 (mid-season) | Phase 4 (descent)
  {
    const auto nStages = pc_StageKcFactor.size();
    if (nStages > 0) {
      // Accumulate theoretical GDD (tracks total progress since crop start)
      // vc_TheoreticalGDDAccumulated += vc_CurrentTemperatureSum[vc_DevelopmentalStage] > 0.0
      //                                   ? 0.0 // already counted; use stage sums for phase detection
      //                                   : 0.0;
      // Compute total elapsed GDD as sum of all completed stages + current stage progress
      double elapsed_GDD = 0.0;
      for (auto s = 0; s < nStages; ++s) elapsed_GDD += vc_CurrentTemperatureSum[s];

      // Identify mid-season start: first stage where Kc equals the maximum
      double max_Kc = *std::max_element(pc_StageKcFactor.begin(), pc_StageKcFactor.end());
      auto mid_stage_start = nStages - 1;
      for (auto i = 1; i < nStages; ++i) {
        if (pc_StageKcFactor[i] >= max_Kc - 1e-6) {
          mid_stage_start = i;
          break;
        }
      }
      // Identify late-season start: first stage after plateau where Kc drops
      auto late_stage_start = nStages - 1;
      for (auto i = mid_stage_start + 1; i < nStages; ++i) {
        if (pc_StageKcFactor[i] < max_Kc - 1e-6) {
          late_stage_start = i;
          break;
        }
      }

      // GDD boundaries
      const double gdd_phase1_end = pc_StageTemperatureSum[0]; // end of germination/initial phase
      double gdd_to_mid = 0.0;
      for (auto i = 0; i < mid_stage_start; ++i) gdd_to_mid += pc_StageTemperatureSum[i];
      double gdd_late_start = 0.0;
      for (auto i = 0; i < late_stage_start; ++i) gdd_late_start += pc_StageTemperatureSum[i];
      double gdd_late_total = 0.0;
      for (auto i = late_stage_start; i < nStages; ++i) gdd_late_total += pc_StageTemperatureSum[i];

      // Phase 1: flat initial
      if (elapsed_GDD <= gdd_phase1_end) {
        vc_KcbFactor = vc_Kcb_ini;
      }
      // Phase 2: linear development ascent
      else if (vc_DevelopmentalStage < mid_stage_start) {
        const double denom = gdd_to_mid - gdd_phase1_end;
        const double frac = (denom > 0.0) ? std::min(1.0, (elapsed_GDD - gdd_phase1_end) / denom) : 1.0;
        vc_KcbFactor = vc_Kcb_ini + frac * (vc_Kcb_mid - vc_Kcb_ini);
      }
      // Phase 3: mid-season plateau
      else if (vc_DevelopmentalStage < late_stage_start) {
        vc_KcbFactor = vc_Kcb_mid;
      }
      // Phase 4: late-season linear descent
      else {
        const double gdd_since_late = elapsed_GDD - gdd_late_start;
        const double frac = (gdd_late_total > 0.0) ? std::min(1.0, gdd_since_late / gdd_late_total) : 1.0;
        vc_KcbFactor = vc_Kcb_mid + frac * (vc_Kcb_end - vc_Kcb_mid);
      }
      vc_KcbFactor = std::max(0.0, vc_KcbFactor);
    }
  }

  auto icSendRcv = [&](const string& outmsg) {
    if (cropPs->isIntercropping && _intercropping->isAsync()) {
      debug() << outmsg;
      // tell the other side our current crop height
      auto wreq = _intercropping->writer.writeRequest();
      auto wval = wreq.initValue();
      wval.setHeight(vc_CropHeight);
      auto prom = wreq.send();
      //.wait(_intercropping->ioContext->waitScope); //.eagerlyEvaluate(nullptr); //[](kj::Exception&& ex){ cout << "crop-module: CropModule::fc_CropPhotosynthesis: write height failed: " << ex.getDescription().cStr() << endl;});
      auto val = _intercropping->reader.readRequest().send().wait(_intercropping->ioContext->waitScope).getValue();
      debug() << "sent height: " << vc_CropHeight << " and received ";
      if (val.isHeight()) {
        _intercroppingOtherCropHeight = val.getHeight();
        debug() << "height: " << _intercroppingOtherCropHeight << endl;
      } else if (val.isNoCrop()) {
        _intercroppingOtherCropHeight = -1;
        debug() << " no-crop" << endl;
      } else if (val.isLait()) {
        debug() << " LAI_t -> Error shouldn't happen here." << endl;
        assert(false);
      }
    }
  };

  if (vc_DevelopmentalStage > 0) {
    auto maxCropHeight = cropPs->isIntercropping
                         && _intercroppingOtherCropHeight > vc_CropHeight
                           ? pc_MaxCropHeight * cropPs->pc_intercropping_phRedux
                           : pc_MaxCropHeight;
    debug() << "original maxCropHeight: " << pc_MaxCropHeight << " -> new maxCropHeight: " << maxCropHeight << endl;

    cropModuleFcCropSize(cm, maxCropHeight);

    icSendRcv("devstage > 0: ");

    cropModuleFcCropGreenArea(cm,
                              meanAirTemperature,
                              vc_OrganGrowthIncrement[OId::LEAF],
                              vc_OrganSenescenceIncrement[OId::LEAF],
                              pc_SpecificLeafArea[vc_DevelopmentalStage - 1],
                              pc_SpecificLeafArea[vc_DevelopmentalStage],
                              pc_SpecificLeafArea[1],
                              pc_StageTemperatureSum[vc_DevelopmentalStage],
                              vc_CurrentTemperatureSum[vc_DevelopmentalStage]);

    vc_SoilCoverage = cropModuleFcSoilCoverage(cm);

    cm->fc_CropPhotosynthesis(meanAirTemperature,
                          maxAirTemperature,
                          minAirTemperature,
                          atmosphericCO2Concentration,
                          atmosphericO3Concentration,
                          currentDate);

    cm->fc_HeatStressImpact(maxAirTemperature,
                        minAirTemperature);

    if (_frostKillOn) {
      cm->fc_FrostKill(maxAirTemperature,
                   minAirTemperature);
    }

    cm->fc_DroughtImpactOnFertility();

    cm->fc_CropNitrogen();

    cm->fc_CropDryMatter(meanAirTemperature);

    // calculate reference evapotranspiration if not provided directly via climate files
    if (referenceEvapotranspiration < 0) {
      vc_ReferenceEvapotranspiration = cm->fc_ReferenceEvapotranspiration(maxAirTemperature,
                                                                      minAirTemperature,
                                                                      relativeHumidity,
                                                                      meanAirTemperature,
                                                                      windSpeed,
                                                                      windSpeedHeight,
                                                                      atmosphericCO2Concentration);
    } else {
      // use reference evapotranspiration from climate file
      vc_ReferenceEvapotranspiration = referenceEvapotranspiration;
    }
    cm->fc_CropWaterUptake(soilColumn->vm_GroundwaterTableLayer,
                       grossPrecipitation,
                       vc_CurrentTotalTemperatureSum,
                       vc_TotalTemperatureSum);

    cm->fc_CropNUptake(soilColumn->vm_GroundwaterTableLayer,
                   vc_CurrentTotalTemperatureSum,
                   vc_TotalTemperatureSum);

    vc_GrossPrimaryProduction = cm->fc_GrossPrimaryProduction();

    vc_NetPrimaryProduction = cm->fc_NetPrimaryProduction(vc_TotalRespired);
  } else {
    icSendRcv("devstage 0: ");
  }

  _noOfCropSteps++;




}


/**
 * @brief Calculation of daylength and radiation
 *
 * Taken from the original HERMES model, Kersebaum, K.C. and Richter J.
 * (1991): Modelling nitrogen dynamics in a plant-soil system with a
 * simple model for advisory purposes. Fert. Res. 27 (2-3), 273 - 281.
 *
 * @param vs_JulianDay
 *
 * @author Claas Nendel
 */
void monica::cropModuleFcRadiation(CropModule* cm, double julianDay, double globalRadiation, double sunshineHours) {
  auto& vc_Declination = cm->vc_Declination;
  auto& vs_Latitude = cm->vs_Latitude;
  auto& vc_AstronomicDayLenght = cm->vc_AstronomicDayLenght;
  auto& vc_EffectiveDayLength = cm->vc_EffectiveDayLength;
  auto& vc_PhotoperiodicDaylength = cm->vc_PhotoperiodicDaylength;
  auto& vc_PhotActRadiationMean = cm->vc_PhotActRadiationMean;
  auto& vc_ClearDayRadiation = cm->vc_ClearDayRadiation;
  auto& vc_OvercastDayRadiation = cm->vc_OvercastDayRadiation;
  auto& vc_ExtraterrestrialRadiation = cm->vc_ExtraterrestrialRadiation;
  auto& vc_GlobalRadiation = cm->vc_GlobalRadiation;

  double vc_DeclinationSinus = 0.0; // old SINLD
  double vc_DeclinationCosinus = 0.0; // old COSLD

  // Calculation of declination - old DEC
  vc_Declination = -23.4 * cos(2.0 * PI * ((julianDay + 10.0) / 365.0));

  vc_DeclinationSinus = sin(vc_Declination * PI / 180.0) * sin(vs_Latitude * PI / 180.0);
  vc_DeclinationCosinus = cos(vc_Declination * PI / 180.0) * cos(vs_Latitude * PI / 180.0);

  // Calculation of the atmospheric day lenght - old DL
  double arg_AstroDayLength = vc_DeclinationSinus / vc_DeclinationCosinus;
  arg_AstroDayLength = bound(-1.0, arg_AstroDayLength, 1.0); // The argument of asin must be in the range of -1 to 1
  vc_AstronomicDayLenght = 12.0 * (PI + 2.0 * asin(arg_AstroDayLength)) / PI;

  // Calculation of the effective day length - old DLE

  double EDLHelper = (-sin(8.0 * PI / 180.0) + vc_DeclinationSinus) / vc_DeclinationCosinus;

  if ((EDLHelper < -1.0) || (EDLHelper > 1.0)) {
    vc_EffectiveDayLength = 0.01;
  } else {
    vc_EffectiveDayLength = 12.0 * (PI + 2.0 * asin(EDLHelper)) / PI;
  }
  /*EDLHelper = bound(-1.0, EDLHelper, 1.0);
  vc_EffectiveDayLength = 12.0 * (PI + 2.0 * asin(EDLHelper)) / PI;*/

  // old DLP
  double arg_PhotoDayLength = (-sin(-6.0 * PI / 180.0) + vc_DeclinationSinus) / vc_DeclinationCosinus;
  arg_PhotoDayLength = bound(-1.0, arg_PhotoDayLength, 1.0); // The argument of asin must be in the range of -1 to 1
  vc_PhotoperiodicDaylength = 12.0 * (PI + 2.0 * asin(arg_PhotoDayLength)) / PI;

  // Calculation of the mean photosynthetically active radiation [J m-2] - old RDN
  double arg_PhotAct = min(1.0, ((vc_DeclinationSinus / vc_DeclinationCosinus) *
                                 (vc_DeclinationSinus / vc_DeclinationCosinus))); // The argument of sqrt must be >= 0
  vc_PhotActRadiationMean = 3600.0 * (vc_DeclinationSinus * vc_AstronomicDayLenght +
                                      24.0 / PI * vc_DeclinationCosinus * sqrt(1.0 - arg_PhotAct));

  // Calculation of radiation on a clear day [J m-2] - old DRC
  if (vc_PhotActRadiationMean > 0 && vc_AstronomicDayLenght > 0) {
    vc_ClearDayRadiation = 0.5 * 1300.0 * vc_PhotActRadiationMean *
                           exp(-0.14 / (vc_PhotActRadiationMean / (vc_AstronomicDayLenght * 3600.0)));
  } else {
    vc_ClearDayRadiation = 0;
  }

  // Calculation of radiation on an overcast day [J m-2] - old DRO
  vc_OvercastDayRadiation = 0.2 * vc_ClearDayRadiation;

  // Calculation of extraterrestrial radiation - old EXT
  double pc_SolarConstant = 0.082;
  //[MJ m-2 d-1] Note: Here is the difference to HERMES, which calculates in [J cm-2 d-1]!
  double SC = 24.0 * 60.0 / PI * pc_SolarConstant * (1.0 + 0.033 * cos(2.0 * PI * julianDay / 365.0));

  double arg_SolarAngle = -tan(vs_Latitude * PI / 180.0) * tan(vc_Declination * PI / 180.0);
  arg_SolarAngle = bound(-1.0, arg_SolarAngle, 1.0);
  double vc_SunsetSolarAngle = acos(arg_SolarAngle);
  vc_ExtraterrestrialRadiation =
    SC * (vc_SunsetSolarAngle * vc_DeclinationSinus + vc_DeclinationCosinus * sin(vc_SunsetSolarAngle)); // [MJ m-2]

  if (globalRadiation > 0.0) {
    vc_GlobalRadiation = globalRadiation;
  } else if (vc_AstronomicDayLenght > 0) {
    vc_GlobalRadiation = vc_ExtraterrestrialRadiation *
                         (0.19 + 0.55 * sunshineHours / vc_AstronomicDayLenght);
  } else {
    vc_GlobalRadiation = 0;
  }
}

/**
 * @brief Calculation of day length factor
 * @param d_DaylengthRequirement
 * @param vc_PhotoperiodicDaylength
 * @param d_BaseDaylength
 *
 * @return Day length factor
 *
 * @author Claas Nendel
 */
double monica::cropModuleFcDaylengthFactor(CropModule* cm,
                                           double daylengthRequirement,
                                           double effectiveDayLength,
                                           double photoperiodicDayLength,
                                           double baseDaylength) {
  auto& vc_DaylengthFactor = cm->vc_DaylengthFactor;

  if (daylengthRequirement > 0.0) {
    // ************ Long-day plants **************
    // * Development acceleration by day length. *
    // *  (Day lenght requirement is positive.)  *
    // *******************************************

    vc_DaylengthFactor = (photoperiodicDayLength - baseDaylength) / (daylengthRequirement - baseDaylength);
  } else if (daylengthRequirement < 0.0) {
    // ************* Short-day plants **************
    // * Development acceleration by night lenght. *
    // *  (Day lenght requirement is negative and  *
    // *    represents critical day length.)   *
    // *********************************************

    double vc_CriticalDayLenght = -daylengthRequirement;
    double vc_MaximumDayLength = -baseDaylength;
    if (effectiveDayLength <= vc_CriticalDayLenght) {
      vc_DaylengthFactor = 1.0;
    } else {
      vc_DaylengthFactor = (effectiveDayLength - vc_MaximumDayLength) / (vc_CriticalDayLenght - vc_MaximumDayLength);
    }
  } else {
    vc_DaylengthFactor = 1.0;
  }

  vc_DaylengthFactor = max(0.0, min(vc_DaylengthFactor, 1.0));

  return vc_DaylengthFactor;
}

/**
 * @brief Calculation of vernalisation factor
 *
 * @param vw_MeanAirTemperature
 * @param d_VernalisationRequirement
 * @param d_VernalisationDays
 *
 * @return vernalisation factor
 *
 * @author Claas Nendel
 */
pair<double, double> monica::cropModuleFcVernalisationFactor(CropModule* cm,
                                                             double meanAirTemperature,
                                                             double vernalisationRequirement,
                                                             double vernalisationDays) {
  auto& __enable_vernalisation_factor_fix__ = cm->__enable_vernalisation_factor_fix__;
  auto& vc_TimeStep = cm->vc_TimeStep;
  auto& vc_VernalisationFactor = cm->vc_VernalisationFactor;
  double vc_EffectiveVernalisation;

  if (vernalisationRequirement == 0.0) {
    vc_VernalisationFactor = 1.0;
  } else {
    if ((meanAirTemperature > -4.0) && (meanAirTemperature <= 0.0)) {
      vc_EffectiveVernalisation = (meanAirTemperature + 4.0) / 4.0;
    } else if ((meanAirTemperature > 0.0) && (meanAirTemperature <= 3.0)) {
      vc_EffectiveVernalisation = 1.0;
    } else if ((meanAirTemperature > 3.0) && (meanAirTemperature <= 7.0)) {
      vc_EffectiveVernalisation = 1.0 - (0.2 * (meanAirTemperature - 3.0) / 4.0);
    } else if ((meanAirTemperature > 7.0) && (meanAirTemperature <= 9.0)) {
      vc_EffectiveVernalisation = 0.8 - (0.4 * (meanAirTemperature - 7.0) / 2.0);
    } else if ((meanAirTemperature > 9.0) && (meanAirTemperature <= 18.0)) {
      vc_EffectiveVernalisation = 0.4 - (0.4 * (meanAirTemperature - 9.0) / 9.0);
    } else if ((meanAirTemperature <= -4.0) || (meanAirTemperature > 18.0)) {
      vc_EffectiveVernalisation = 0.0;
    } else {
      vc_EffectiveVernalisation = 1.0;
    }

    // old VERNTAGE
    vernalisationDays += vc_EffectiveVernalisation * vc_TimeStep;

    // old VERSCHWELL
    double vc_VernalisationThreshold = min(vernalisationRequirement, 9.0) - 1.0;

    if (vc_VernalisationThreshold >= 1) {
      vc_VernalisationFactor = (vernalisationDays - vc_VernalisationThreshold) / (vernalisationRequirement
                                 - vc_VernalisationThreshold);

      if (__enable_vernalisation_factor_fix__) vc_VernalisationFactor = min(max(0.0, vc_VernalisationFactor), 1.0);
      if (vc_VernalisationFactor < 0) vc_VernalisationFactor = 0.0; //MP: Vernalisation kann nie negativ sein
    } else {
      vc_VernalisationFactor = 1.0;
      //MP: Vernalisation hat keinen Effekt, wenn kein (bzw. 0 als) Threshold definiert ist.
    }
  }

  return make_pair(vc_VernalisationFactor, vernalisationDays);
}

/*!
 * @brief Calculation of oxygen deficiency
 *
 * @returns Oxygen deficiency factor.
 *
 * @author Claas Nendel
 */
double monica::cropModuleFcOxygenDeficiency(CropModule* cm, double criticalOxygenContent) {
  auto& vc_DevelopmentalStage = cm->vc_DevelopmentalStage;
  auto& vc_TimeUnderAnoxiaThreshold = cm->vc_TimeUnderAnoxiaThreshold;
  auto& TimeUnderAnoxiaThresholdDefault = cm->TimeUnderAnoxiaThresholdDefault;
  auto& vc_RootingDepth = cm->vc_RootingDepth;
  auto* soilColumn = cm->soilColumn;
  auto& vc_TimeStep = cm->vc_TimeStep;
  auto& vc_TimeUnderAnoxia = cm->vc_TimeUnderAnoxia;
  auto& vc_OxygenDeficit = cm->vc_OxygenDeficit;
  int timeUnderAnoxiaThresholdAtStage = vc_DevelopmentalStage < vc_TimeUnderAnoxiaThreshold.size()
                                          ? vc_TimeUnderAnoxiaThreshold.at(vc_DevelopmentalStage)
                                          : TimeUnderAnoxiaThresholdDefault;
  // Reduktion bei Luftmangel Stauwasser berücksichtigen!!!!
  double sumSaturation = 0, sumSoilMoisture = 0;
  int sumLayers = 0;
  auto nols = std::min(std::max(size_t(3), vc_RootingDepth), soilColumn->size());
  //MP: changed to consider at least first 30 cm and then rooting depth
  for (size_t i = 0; i < nols; i++) {
    sumSaturation += (*soilColumn)[i]._sps.vs_Saturation;
    sumSoilMoisture += (*soilColumn)[i].vs_SoilMoisture_m3;
    sumLayers++;
  }
  double avgAirFilledPoreVolume = (sumSaturation - sumSoilMoisture) / sumLayers;
  if (avgAirFilledPoreVolume < criticalOxygenContent) { //MP: conditions changed for stage-dependent waterlogging
    avgAirFilledPoreVolume = std::max(0.0, avgAirFilledPoreVolume); //to quarantee for positive values
    vc_TimeUnderAnoxia = std::max(vc_TimeUnderAnoxia + int(vc_TimeStep), timeUnderAnoxiaThresholdAtStage);
    double vc_MaxOxygenDeficit = avgAirFilledPoreVolume / criticalOxygenContent;
    vc_OxygenDeficit =
      1.0 - double(vc_TimeUnderAnoxia / double(timeUnderAnoxiaThresholdAtStage)) * (1.0 - vc_MaxOxygenDeficit);
    vc_OxygenDeficit = std::max(0.0, vc_OxygenDeficit);
  } else {
    vc_TimeUnderAnoxia = 0;
    vc_OxygenDeficit = 1.0;
  }
  return vc_OxygenDeficit;
}

double WangEngelTemperatureResponse(double t, double tmin, double topt, double tmax, double betacoeff) {
  //MP: what is this beta coefficient?
  // prevent nan values with t < tmin
  if (t < tmin || t > tmax) return 0.0;

  double alfa = log(2) / log((tmax - tmin) / (topt - tmin));
  double numerator = 2 * pow(t - tmin, alfa) * pow(topt - tmin, alfa) - pow(t - tmin, 2 * alfa);
  double denominator = pow(topt - tmin, 2 * alfa);

  return pow(numerator / denominator, betacoeff); //MP: beta coefficient should be 2*alfa
};

/**
 * @brief Determining the crop's developmental stage
 *
 * In this module the developmental stage of the crop is calculated from
 * accumulated heat units (degree-days). It includes the germination of the
 * seed in dependence of the moisture status of the top soil and development
 * acceleration if stress occurs. Stress factors can be limited water or N
 * supply. The temperature sum is cumulated specifically for every
 * developmental stage. As soon as the current temperature sum exceeds the
 * defined crop-specific temperature sum for the respective developmental
 * stage the crop will enter the following stage, until the final stage's
 * temperature sum is reached. In order to follow the overage development
 * of the crop its total temperature sum is monitored.
 *
 * @param meanAirTemperature
 * @param soilMoisture_m3
 * @param fieldCapacity
 * @param permanentWiltingPoint
 *
 * @author Claas Nendel
 * @author Michael Berg-Mohnicke (refactoring)
 */
void monica::cropModuleFcCropDevelopmentalStage(CropModule* cm,
                                                double meanAirTemperature,
                                                double soilMoisture_m3,
                                                double fieldCapacity,
                                                double permanentWiltingPoint,
                                                Tools::Date currentDate) {
  auto& _perennialCropDormancyPeriodEndDate = cm->_perennialCropDormancyPeriodEndDate;
  const auto* cropPs = cm->cropPs;
  auto& cultivarPs = cm->cultivarPs;
  auto& speciesPs = cm->speciesPs;
  auto* soilColumn = cm->soilColumn;
  auto& pc_AssimilatePartitioningCoeff = cm->pc_AssimilatePartitioningCoeff;
  auto& pc_BaseTemperature = cm->pc_BaseTemperature;
  auto& pc_DevelopmentAccelerationByNitrogenStress = cm->pc_DevelopmentAccelerationByNitrogenStress;
  auto& pc_DroughtStressThreshold = cm->pc_DroughtStressThreshold;
  auto& pc_EmergenceFloodingControlOn = cm->pc_EmergenceFloodingControlOn;
  auto& pc_EmergenceMoistureControlOn = cm->pc_EmergenceMoistureControlOn;
  auto& pc_NumberOfDevelopmentalStages = cm->pc_NumberOfDevelopmentalStages;
  auto& pc_OptimumTemperature = cm->pc_OptimumTemperature;
  auto& pc_Perennial = cm->pc_Perennial;
  auto& pc_StageTemperatureSum = cm->pc_StageTemperatureSum;
  auto& vc_CropNRedux = cm->vc_CropNRedux;
  auto& vc_CurrentTemperatureSum = cm->vc_CurrentTemperatureSum;
  auto& vc_CurrentTotalTemperatureSum = cm->vc_CurrentTotalTemperatureSum;
  auto& vc_DaylengthFactor = cm->vc_DaylengthFactor;
  auto& vc_DevelopmentalStage = cm->vc_DevelopmentalStage;
  auto& vc_ErrorMessage = cm->vc_ErrorMessage;
  auto& vc_ErrorStatus = cm->vc_ErrorStatus;
  auto& vc_GrowthCycleEnded = cm->vc_GrowthCycleEnded;
  auto& vc_OxygenDeficit = cm->vc_OxygenDeficit;
  auto& vc_StorageOrgan = cm->vc_StorageOrgan;
  auto& vc_TimeStep = cm->vc_TimeStep;
  auto& vc_TranspirationDeficit = cm->vc_TranspirationDeficit;
  auto& vc_VernalisationFactor = cm->vc_VernalisationFactor;

  if (vc_DevelopmentalStage == 0) {
    if (pc_Perennial) { // pc_Perennial == true
      if (meanAirTemperature > pc_BaseTemperature[vc_DevelopmentalStage]) {
        double tempIncr = (min(meanAirTemperature, pc_OptimumTemperature[vc_DevelopmentalStage])
                           - pc_BaseTemperature[vc_DevelopmentalStage])
                          * vc_VernalisationFactor * vc_DaylengthFactor * vc_TimeStep;
        vc_CurrentTemperatureSum[vc_DevelopmentalStage] += tempIncr;
        vc_CurrentTotalTemperatureSum += tempIncr;
      }

      // @todo: shouldn't vc_CurrentTemperatureSum be reduces by the excess temperature like further below?
      if (vc_CurrentTemperatureSum[vc_DevelopmentalStage] >= pc_StageTemperatureSum[vc_DevelopmentalStage]) {
        if (vc_DevelopmentalStage < (pc_NumberOfDevelopmentalStages - 1)) vc_DevelopmentalStage++;
      }
    } else { // pc_Perennial == false
      double vc_SoilTemperature = (*soilColumn)[0].vs_SoilTemperature; //MP: Bodentemperatur der ersten 10cm
      if (vc_SoilTemperature > pc_BaseTemperature[vc_DevelopmentalStage]) {
        // @todo Claas: Schränkt trockener Boden das Aufsummieren der Wärmeeinheiten ein, oder
        // sollte nicht eher nur der Wechsel in das Stadium 1 davon abhängen? --> Christian //MP: das entspricht sich

        bool emergenceCondition = true;
        // Germination only if soil water content in top layer exceeds
        // 20% of capillary water, but is not beyond field capacity //MP: das kann an- oder ausgeschalten werden
        if (pc_EmergenceMoistureControlOn) {
          double vc_CapillaryWater = fieldCapacity - permanentWiltingPoint;
          emergenceCondition = emergenceCondition
                               && soilMoisture_m3 > ((0.2 * vc_CapillaryWater) + permanentWiltingPoint)
                               && soilMoisture_m3 <= fieldCapacity;
        }
        // Germination only if no water is stored on the soil surface.
        if (pc_EmergenceFloodingControlOn) {
          emergenceCondition = emergenceCondition && soilColumn->vs_SurfaceWaterStorage < 0.001;
        }

        if (emergenceCondition) {
          vc_CurrentTemperatureSum[vc_DevelopmentalStage] +=
            //MP: Wenn die Frucht noch nicht aufgegangen ist und die Auflaufbedingungen gegeben sind, wird begonnen die Bodentemperatur aufzusummieren.
            (vc_SoilTemperature - pc_BaseTemperature[vc_DevelopmentalStage]) * vc_TimeStep;

          if (vc_CurrentTemperatureSum[vc_DevelopmentalStage] >= pc_StageTemperatureSum[vc_DevelopmentalStage]) {
            //MP: wenn die Temperatursumme erreicht wird,
            double vc_StageExcessTemperatureSum =
              //MP: wird ein Temperatur"bonus" gespeichert, der dann auf die neue Wärmesumme angerechnet wird
              vc_CurrentTemperatureSum[vc_DevelopmentalStage] - pc_StageTemperatureSum[vc_DevelopmentalStage];
            if (vc_DevelopmentalStage < pc_NumberOfDevelopmentalStages - 1) {
              vc_DevelopmentalStage++;
              //MP: und erhöht sich die aktuelle Phase um eins, solange die Frucht nicht reif ist
              vc_CurrentTemperatureSum[vc_DevelopmentalStage] += vc_StageExcessTemperatureSum;
            }
          }
        }
      }
    }
  } else if (vc_DevelopmentalStage > 0) {
    //MP: wenn die Frucht aufgegangen ist, können N- und Wasser-Stress zum Tragen kommen (nur während der Kornfüllungsphase --> schnelleres Abreifen)
    auto apc = pc_AssimilatePartitioningCoeff[vc_DevelopmentalStage][vc_StorageOrgan];

    // Development acceleration by N deficit in crop tissue //MP: N-Stress als Beschläunigung nur während der Kornfüllungsphase
    double vc_DevelopmentAccelerationByNitrogenStress = 1; // old NPROG
    if (pc_DevelopmentAccelerationByNitrogenStress == 1 && apc > 0.9) {
      vc_DevelopmentAccelerationByNitrogenStress = 1.0 + ((1.0 - vc_CropNRedux) * (1.0 - vc_CropNRedux));
    }


    // Development acceleration by water deficit //MP: Access point for drought optimisation
    double vc_DevelopmentAccelerationByWaterStress = 1; // old WPROG
    if (vc_TranspirationDeficit < pc_DroughtStressThreshold[vc_DevelopmentalStage] && apc > 0.9) {
      //Das heißt, das betrifft nur die Kornfüllungsphase (acp>0.9).
      if (vc_OxygenDeficit >= 1.0) {
        vc_DevelopmentAccelerationByWaterStress =
          1.0 + ((1.0 - vc_TranspirationDeficit) * (1.0 - vc_TranspirationDeficit));
      }
    }

    //MP: added development slowdown by waterlogging
    //double vc_DevelopmentSlowdownByWaterlogging = 1;
    //if (vc_OxygenDeficit < 1.0) {
    //    vc_DevelopmentSlowdownByWaterlogging =
    //        1.0 - ((1.0 - vc_OxygenDeficit) * (1.0 - vc_OxygenDeficit));
    //}

    // old DEVPROG
    double vc_DevelopmentAccelerationByStress =
      max(vc_DevelopmentAccelerationByNitrogenStress, vc_DevelopmentAccelerationByWaterStress);
    // *vc_DevelopmentSlowdownByWaterlogging; //MP: this does not work?

    if (cropPs->__enable_Phenology_WangEngelTemperatureResponse__) {
      double devTresponse = max(0.0, WangEngelTemperatureResponse(meanAirTemperature,
                                                                  cultivarPs.pc_MinTempDev_WE,
                                                                  cultivarPs.pc_OptTempDev_WE,
                                                                  cultivarPs.pc_MaxTempDev_WE,
                                                                  1.0)); //MP: warum steht hier 1?
      double tempIncr = devTresponse * meanAirTemperature * vc_VernalisationFactor * vc_DaylengthFactor *
                        vc_DevelopmentAccelerationByStress * vc_TimeStep;
      vc_CurrentTemperatureSum[vc_DevelopmentalStage] += tempIncr;
      vc_CurrentTotalTemperatureSum += tempIncr;
    } else {
      if (meanAirTemperature > pc_BaseTemperature[vc_DevelopmentalStage]) {
        double tempIncr = (min(meanAirTemperature, pc_OptimumTemperature[vc_DevelopmentalStage])
                           - pc_BaseTemperature[vc_DevelopmentalStage])
                          * vc_VernalisationFactor * vc_DaylengthFactor * vc_DevelopmentAccelerationByStress *
                          vc_TimeStep;
        vc_CurrentTemperatureSum[vc_DevelopmentalStage] += tempIncr; //MP: effektive Temperatur wird aufsummiert
        vc_CurrentTotalTemperatureSum += tempIncr;
      }
    }

    bool doResetPerennialCrop =
      pc_Perennial
      && speciesPs.dormancyStartDoy > 0
      && currentDate.dayOfYear() >= speciesPs.dormancyStartDoy;
    if (vc_CurrentTemperatureSum[vc_DevelopmentalStage] >= pc_StageTemperatureSum[vc_DevelopmentalStage]) {
      if (vc_DevelopmentalStage < pc_NumberOfDevelopmentalStages - 1) {
        double stageExcessTemperatureSum =
          vc_CurrentTemperatureSum[vc_DevelopmentalStage] - pc_StageTemperatureSum[vc_DevelopmentalStage];
        vc_DevelopmentalStage++;
        vc_CurrentTemperatureSum[vc_DevelopmentalStage] += stageExcessTemperatureSum;
      } else if (vc_DevelopmentalStage == pc_NumberOfDevelopmentalStages - 1) { //MP: Frucht ist reif
        if (pc_Perennial && vc_GrowthCycleEnded) doResetPerennialCrop = true;
      }
    }
    if (doResetPerennialCrop) {
      vc_DevelopmentalStage = 0;
      cm->fc_UpdateCropParametersForPerennial();
      for (int stage = 0; stage < pc_NumberOfDevelopmentalStages; stage++) vc_CurrentTemperatureSum[stage] = 0.0;
      vc_CurrentTotalTemperatureSum = 0.0;
      vc_GrowthCycleEnded = false;
      _perennialCropDormancyPeriodEndDate =
        speciesPs.dormancyEndDoy == 0
          ? currentDate
          : Date(1, 1, currentDate.year() + (currentDate.dayOfYear() > speciesPs.dormancyEndDoy ? 1 : 0))
            + (speciesPs.dormancyEndDoy - 1);
    }
  } else {
    vc_ErrorStatus = true;
    vc_ErrorMessage = "irregular developmental stage";
  }

  debug() << "devstage: " << vc_DevelopmentalStage << endl;
}

/**
 * @brief Determining the crop's Kc factor
 *
 * @param vc_DevelopmentalStage
 * @param d_StageTemperatureSum
 * @param d_CurrentTemperatureSum
 * @param pc_InitialKcFactor
 * @param d_StageKcFactor
 * @param d_EarlierStageKcFactor
 *
 * @return
 *
 * @author Claas Nendel //MP: will this need refinement?
 */
double monica::cropModuleFcKcFactor(const CropModule* cm,
                                    double d_StageTemperatureSum,
                                    double d_CurrentTemperatureSum,
                                    double d_StageKcFactor, // DB
                                    double d_EarlierStageKcFactor) // DB
{
  auto& vc_DevelopmentalStage = cm->vc_DevelopmentalStage;
  auto& pc_InitialKcFactor = cm->pc_InitialKcFactor;
  double vc_RelativeDevelopment = 0.0;
  if (d_StageTemperatureSum > 0.0) {
    vc_RelativeDevelopment = min(d_CurrentTemperatureSum / d_StageTemperatureSum, 1.0); // old relint
  }

  if (vc_DevelopmentalStage == 0) {
    return pc_InitialKcFactor + (d_StageKcFactor - pc_InitialKcFactor) * vc_RelativeDevelopment;
  } else {
    // Interpolating the Kc Factors
    return d_EarlierStageKcFactor + ((d_StageKcFactor - d_EarlierStageKcFactor) * vc_RelativeDevelopment);
  }
}

/**
 * @brief Calculation of the crop's size
 *
 * This method uses the total relative development of the crop in terms of heat
 * units to determine the sigmoidal development of the crop's height.
 *
 * @Todo In a later stage this must be coupled to the stem's biomass in order to reflect water
 * and N stress to crop height.
 * Diameter is assumed to develop lineraly.
 *
 * @param maxCropHeight
 * @param pc_MaxCropDiameter
 * @param pc_StageAtMaxHeight
 * @param pc_StageAtMaxDiameter
 * @param vc_DevelopmentalStage
 * @param d_CurrentTemperatureSum
 * @param pc_StageTemperatureSum
 * @param vc_CurrentTotalTemperatureSum
 * @param pc_CropHeightP1
 * @param pc_CropHeightP2
 *
 * @author Claas Nendel
 */
void monica::cropModuleFcCropSize(CropModule* cm, double maxCropHeight) {
  auto& pc_StageAtMaxHeight = cm->pc_StageAtMaxHeight;
  auto& pc_StageTemperatureSum = cm->pc_StageTemperatureSum;
  auto& vc_CurrentTotalTemperatureSum = cm->vc_CurrentTotalTemperatureSum;
  auto& pc_CropHeightP1 = cm->pc_CropHeightP1;
  auto& pc_CropHeightP2 = cm->pc_CropHeightP2;
  auto& vc_CropHeight = cm->vc_CropHeight;
  auto& pc_StageAtMaxDiameter = cm->pc_StageAtMaxDiameter;
  auto& vc_CropDiameter = cm->vc_CropDiameter;
  auto& pc_MaxCropDiameter = cm->pc_MaxCropDiameter;
  double vc_TotalTemperatureSumForHeight = 0.0;
  for (int stage = 1; stage < pc_StageAtMaxHeight + 1; stage++) {
    vc_TotalTemperatureSumForHeight += pc_StageTemperatureSum[stage];
  }
  double vc_RelativeTotalDevelopmentForHeight =
    min(vc_CurrentTotalTemperatureSum / vc_TotalTemperatureSumForHeight, 1.0);
  if (vc_RelativeTotalDevelopmentForHeight > 0.0) {
    vc_CropHeight =
      maxCropHeight / (1.0 + exp(-pc_CropHeightP1 * (vc_RelativeTotalDevelopmentForHeight - pc_CropHeightP2)));
  } else {
    vc_CropHeight = 0.0;
  }

  double vc_TotalTemperatureSumForDiameter = 0.0;
  for (int stage = 1; stage < pc_StageAtMaxDiameter + 1; stage++) {
    vc_TotalTemperatureSumForDiameter += pc_StageTemperatureSum[stage];
  }
  double vc_RelativeTotalDevelopmentForDiameter =
    min(vc_CurrentTotalTemperatureSum / vc_TotalTemperatureSumForDiameter, 1.0);
  if (vc_RelativeTotalDevelopmentForDiameter > 0.0) {
    vc_CropDiameter = pc_MaxCropDiameter * vc_RelativeTotalDevelopmentForDiameter;
  } else {
    vc_CropDiameter = 0.0;
  }
}

/**
 * @brief Calculation of the crop's green area
 *
 * @param d_LeafBiomass
 * @param d_ShootBiomass
 * @param vc_CropHeight
 * @param vc_CropDiameter
 * @param d_SpecificLeafAreaStart
 * @param d_SpecificLeafAreaEnd
 * @param d_StageTemperatureSum
 * @param d_CurrentTemperatureSum
 * @param vc_TimeStep
 *
 * @author Claas Nendel
 */
void monica::cropModuleFcCropGreenArea(CropModule* cm,
                                       double vw_MeanAirTemperature,
                                       double d_LeafBiomassIncrement,
                                       double d_LeafBiomassDecrement,
                                       double d_SpecificLeafAreaStart,
                                       double d_SpecificLeafAreaEnd,
                                       double d_SpecificLeafAreaEarly,
                                       double d_StageTemperatureSum,
                                       double d_CurrentTemperatureSum) {
  const auto* cropPs = cm->cropPs;
  auto& vc_DevelopmentalStage = cm->vc_DevelopmentalStage;
  auto& speciesPs = cm->speciesPs;
  auto& cultivarPs = cm->cultivarPs;
  auto& vc_TimeStep = cm->vc_TimeStep;
  auto& vc_LeafAreaIndex = cm->vc_LeafAreaIndex;
  auto& vc_GreenAreaIndex = cm->vc_GreenAreaIndex;
  auto& vc_CropHeight = cm->vc_CropHeight;
  auto& vc_CropDiameter = cm->vc_CropDiameter;
  auto& pc_PlantDensity = cm->pc_PlantDensity;
  double TempResponseExpansion = 1.0;
  if (cropPs->__enable_T_response_leaf_expansion__) {
    // Stage switch T response leaf exp (wheat = 2, maize = -1 (deactivated))
    if (vc_DevelopmentalStage + 1 <= speciesPs.pc_TransitionStageLeafExp) {
      // Early stages leaf expansion T response
      //!!!! maybe referenceTempResponseExpansion calculation should be moved to the constructor because it has to be calculated just once per crop
      double referenceTempResponseExpansion = 223.9 * exp(-5.03 * exp(-0.0653 * cultivarPs.pc_EarlyRefLeafExp));
      TempResponseExpansion = std::min(
                                       223.9 * exp(-5.03 * exp(-0.0653 * vw_MeanAirTemperature)) /
                                       referenceTempResponseExpansion, 1.3);
    } else {
      // leaf expansion T response
      double referenceTempResponseExpansion = 37.7 * exp(-7.23 * exp(-0.1462 * cultivarPs.pc_RefLeafExp));
      TempResponseExpansion = std::min(
                                       37.7 * exp(-7.23 * exp(-0.1462 * vw_MeanAirTemperature)) /
                                       referenceTempResponseExpansion, 1.3);
    }
  }

  vc_LeafAreaIndex += (d_LeafBiomassIncrement * TempResponseExpansion * (d_SpecificLeafAreaStart +
                                                                         (d_CurrentTemperatureSum /
                                                                          d_StageTemperatureSum *
                                                                          (d_SpecificLeafAreaEnd -
                                                                           d_SpecificLeafAreaStart))) * vc_TimeStep) -
    (d_LeafBiomassDecrement * d_SpecificLeafAreaEarly * vc_TimeStep); // [ha ha-1]

  if (vc_LeafAreaIndex <= 0.0) vc_LeafAreaIndex = 0.001;
  vc_GreenAreaIndex = vc_LeafAreaIndex + (vc_CropHeight * PI * vc_CropDiameter * pc_PlantDensity); // [m2 m-2]
}

/**
 * @brief Calculation of soil area covered by the crop
 *
 * This approach to calculate soil coverage from crop leaf area index is a
 * work-around to at least have a feed-back relation of crop biomass to
 * transpiration. It emanates from The HERMES model. At a later stage LAI
 * should be used directly to calculate Transpiration.
 * Note: This appraoch does not consider spaces between row crops. If any
 * row crop is to be calculated, we need to think this over.
 *
 * @param vc_LeafAreaIndex
 *
 * @return part of soil coved by vegetation
 *
 * @author Claas Nendel
 */
double monica::cropModuleFcSoilCoverage(const CropModule* cm) {
  return 1.0 - (exp(-0.5 * cm->vc_LeafAreaIndex));
}

#ifdef TEST_HOURLY_OUTPUT
#include <fstream>
ostream& monica::tout(bool closeFile) {
  static ofstream out;
  static bool init = false;
  static bool failed = false;
  if (closeFile) {
    init = false;
    failed = false;
    out.close();
    return out;
  }

  if (!init) {
    out.open("hourly-data.csv");
    failed = out.fail();
    (failed ? cout : out) << "iso-date"
      ",hour"
      ",crop-name"
      ",in:global_rad"
      ",in:extra_terr_rad"
      ",in:solar_el"
      ",mcd:rad"
      ",in:LAI"
      ",in:mfol"
      ",in:sla"
      ",in:leaf_temp"
      ",in:VPD"
      ",in:Ca"
      ",in:fO3"
      ",in:fls"
      ",out:canopy_net_photos"
      ",out:canopy_res"
      ",out:canopy_gross_photos"
      ",out:jmax_c"
      //",out:guenther:iso"
      //",out:guenther:mono"
      ",out:sun:LAI"
      ",out:sun:mfol"
      ",out:sun:sla"
      ",out:sun:gs"
      ",out:sun:kc"
      ",out:sun:ko"
      ",out:sun:oi"
      ",out:sun:ci"
      ",out:sun:comp"
      ",out:sun:vcMax"
      ",out:sun:jMax"
      ",out:sun:rad"
      ",out:sun:jj"
      ",out:sun:jj1000"
      ",out:sun:jv"
      ",out:sun:guenther:iso"
      ",out:sun:guenther:mono"
      ",out:jjv:sun:iso"
      ",out:jjv:sun:mono"
      ",out:sh:LAI"
      ",out:sh:mfol"
      ",out:s:sla"
      ",out:sh:gs"
      ",out:sh:kc"
      ",out:sh:ko"
      ",out:sh:oi"
      ",out:sh:ci"
      ",out:sh:comp"
      ",out:sh:vcMax"
      ",out:sh:jMax"
      ",out:sh:rad"
      ",out:sh:jj"
      ",out:sh:jj1000"
      ",out:sh:jv"
      ",out:sh:guenther:iso"
      ",out:sh:guenther:mono"
      ",out:jjv:sh:iso"
      ",out:jjv:sh:mono"
      << endl;

    init = true;
  }

  return failed ? cout : out;
}
#endif

void CropModule::fc_MoveDeadRootBiomassToSoil(double deadRootBiomass,
                                              double vc_RootDensityFactorSum,
                                              const vector<double>& vc_RootDensityFactor) {
  auto nools = soilColumn->_vs_NumberOfOrganicLayers;

  map<size_t, double> layer2deadRootBiomassAtLayer;
  for (size_t i = 0; i < vc_RootingZone; i++) {
    double deadRootBiomassAtLayer = vc_RootDensityFactor.at(i) / vc_RootDensityFactorSum * deadRootBiomass;
    // just add organica matter if > 0.0001
    if (int(deadRootBiomassAtLayer * 10000) > 0) {
      layer2deadRootBiomassAtLayer[i < nools ? i : nools - 1] += deadRootBiomassAtLayer;
    }
  }

  if (!layer2deadRootBiomassAtLayer.empty()) _addOrganicMatter(layer2deadRootBiomassAtLayer, vc_NConcentrationRoot);
}

void CropModule::addAndDistributeRootBiomassInSoil(double rootBiomass) {
  auto p = calcRootDensityFactorAndSum();
  fc_MoveDeadRootBiomassToSoil(rootBiomass, p.second, p.first);
}

/**
 * @brief Calculation of photosynthesis
 *
 * In this function crop photosynthesis is calculated.
 *
 * @param vw_MeanAirTemperature
 * @param vw_MaxAirTemperature
 * @param vw_MinAirTemperature
 * @param vc_GlobalRadiation
 * @param vw_SunshineHours
 * @param vw_AtmosphericCO2Concentration
 * @param vs_JulianDay
 * @param vs_Latitude
 * @param vc_LeafAreaIndex
 * @param pc_DefaultRadiationUseEfficiency
 * @param pc_MaxAssimilationRate
 * @param pc_MinimumTemperatureForAssimilation
 * @param vc_PhotActRadiationMean
 * @param vc_AstronomicDayLenght
 * @param vc_Declination
 * @param vc_ClearDayRadiation
 * @param vc_EffectiveDayLength
 * @param vc_OvercastDayRadiation
 *
 * @author Claas Nendel
 */
void CropModule::fc_CropPhotosynthesis(double vw_MeanAirTemperature,
                                       double vw_MaxAirTemperature,
                                       double vw_MinAirTemperature,
                                       double vw_AtmosphericCO2Concentration,
                                       double vw_AtmosphericO3Concentration,
                                       Date currentDate) {
  using namespace Voc;

  double vc_AssimilationRateReference = 0.0;

  double pc_ReferenceLeafAreaIndex = cropPs->pc_ReferenceLeafAreaIndex;
  double pc_ReferenceMaxAssimilationRate = cropPs->pc_ReferenceMaxAssimilationRate;
  double pc_MaintenanceRespirationParameter_1 = cropPs->pc_MaintenanceRespirationParameter1;
  double pc_MaintenanceRespirationParameter_2 = cropPs->pc_MaintenanceRespirationParameter2;

  double pc_GrowthRespirationParameter_1 = cropPs->pc_GrowthRespirationParameter1;
  double pc_GrowthRespirationParameter_2 = cropPs->pc_GrowthRespirationParameter2;
  double pc_CanopyReflectionCoeff = cropPs->pc_CanopyReflectionCoefficient; // old REFLC;

  double vc_RadiationUseEfficiency = pc_DefaultRadiationUseEfficiency;
  double vc_RadiationUseEfficiencyReference = pc_DefaultRadiationUseEfficiency;
  if (pc_CarboxylationPathway == 1) {
    // Calculation of CO2 impact on crop growth
    if (pc_CO2Method == 3) {
      //////////////////////////////////////////////////////////////////////////
      // Method 3:
      // Long, S.P. 1991. Modification of the response of photosynthetic
      // productivity to rising temperature by atmospheric CO2
      // concentrations - Has its importance been underestimated. Plant
      // Cell Environ. 14(8): 729-739.
      // and
      // Mitchell, R.A.C., D.W. Lawlor, V.J. Mitchell, C.L. Gibbard, E.M.
      // White, and J.R. Porter. 1995. Effects of elevated CO2
      // concentration and increased temperature on winter-wheat - Test
      // of ARCWHEAT1 simulation model. Plant Cell Environ. 18(7):736-748.
      //////////////////////////////////////////////////////////////////////////

      double tempK = vw_MeanAirTemperature + D_IN_K;
      double term1 = (tempK - TK25) / (TK25 * tempK * RGAS);
      double term2 = sqrt(tempK / TK25);
      vc_KTkc = exp(speciesPs.AEKC * term1) * term2;
      vc_KTko = exp(speciesPs.AEKO * term1) * term2;
      double Mkc = speciesPs.KC25 * vc_KTkc; //[µmol mol-1]
      _cropPhotosynthesisResults.kc = Mkc;
      _cropPhotosynthesisResults.kc = Mkc;
      double Mko = speciesPs.KO25 * vc_KTko; //[mmol mol-1]
      _cropPhotosynthesisResults.ko = Mko * 1000.0; // mmol -> umol

      // OLD exponential response
      double KTvmax = cropPs->__enable_Photosynthesis_WangEngelTemperatureResponse__
                        ? max(0.00001, WangEngelTemperatureResponse(vw_MeanAirTemperature,
                                                                    pc_MinimumTemperatureForAssimilation,
                                                                    pc_OptimumTemperatureForAssimilation,
                                                                    pc_MaximumTemperatureForAssimilation,
                                                                    1.0))
                        : exp(speciesPs.AEVC * term1) * term2;

      // Berechnung des Transformationsfaktors für pflanzenspez. AMAX bei 25 grad
      // old fakamax
      double vc_AmaxFactor = pc_MaxAssimilationRate / 34.668;
      double vc_AmaxFactorReference = pc_ReferenceMaxAssimilationRate / 34.668;
      // old vcmax
      double vc_Vcmax = 98.0 * vc_AmaxFactor * KTvmax;
      _cropPhotosynthesisResults.vcMax = vc_Vcmax;
      double vc_VcmaxReference = 98.0 * vc_AmaxFactorReference * KTvmax;

      // Oi = 210.0 + (0.047
      double Oi = 210.0 * (0.047 - 0.0013087 * vw_MeanAirTemperature +
                           0.000025603 * (vw_MeanAirTemperature * vw_MeanAirTemperature) -
                           0.00000021441 * (vw_MeanAirTemperature * vw_MeanAirTemperature * vw_MeanAirTemperature)) /
                  0.026934; // [mmol mol-1]
      _cropPhotosynthesisResults.oi = Oi *
                                      1000.0; // mmol -> umol

      double Ci = vw_AtmosphericCO2Concentration * 0.7 * (1.674 - 0.061294 * vw_MeanAirTemperature +
                                                          0.0011688 * (vw_MeanAirTemperature * vw_MeanAirTemperature) -
                                                          0.0000088741 *
                                                          (vw_MeanAirTemperature * vw_MeanAirTemperature *
                                                           vw_MeanAirTemperature)) / 0.73547; // [µmol mol-1]
      _cropPhotosynthesisResults.ci = Ci;

      // similar to LDNDC::jarvis.cpp:217
      //  old COcomp
      double vc_CO2CompensationPoint =
        0.5 * 0.21 * vc_Vcmax * Mkc * Oi / (vc_Vcmax * Mko); // [µmol mol-1]
      double vc_CO2CompensationPointReference =
        0.5 * 0.21 * vc_VcmaxReference * Mkc * Oi / (vc_VcmaxReference * Mko); // [µmol mol-1]
      _cropPhotosynthesisResults.comp = vc_CO2CompensationPoint;

      // Mitchell et al. 1995:
      // old EFF
      vc_RadiationUseEfficiency = max(0.0, min(0.77 / 2.1 * (Ci - vc_CO2CompensationPoint) /
                                               (4.5 * Ci + 10.5 * vc_CO2CompensationPoint) * 8.3769, 0.5));
      vc_RadiationUseEfficiencyReference = max(0.0, min(0.77 / 2.1 * (Ci - vc_CO2CompensationPointReference) /
                                                        (4.5 * Ci + 10.5 * vc_CO2CompensationPointReference) * 8.3769,
                                                        0.5));

      vc_AssimilationRate = (Ci - vc_CO2CompensationPoint) * vc_Vcmax / (Ci + Mkc * (1.0 + Oi / Mko)) * 1.656;
      vc_AssimilationRateReference =
        (Ci - vc_CO2CompensationPointReference) * vc_VcmaxReference / (Ci + Mkc * (1.0 + Oi / Mko)) * 1.656;

      if (vw_MeanAirTemperature < pc_MinimumTemperatureForAssimilation) {
        vc_AssimilationRate = 0.0; //MP: warum gibt es für C3-Pflanzen keine maximale Temperatur
        vc_AssimilationRateReference = 0.0;
      }
    } else if (pc_CO2Method == 2) {
      //////////////////////////////////////////////////////////////////////////
      // Method 2:
      // Hoffmann, F. 1995. Fagus, a model for growth and development of
      // beech. Ecol. Mod. 83 (3):327-348.
      //////////////////////////////////////////////////////////////////////////
      double t_response = WangEngelTemperatureResponse(vw_MeanAirTemperature,
                                                       pc_MinimumTemperatureForAssimilation,
                                                       pc_OptimumTemperatureForAssimilation,
                                                       pc_MaximumTemperatureForAssimilation,
                                                       1.0);

      vc_AssimilationRate = pc_MaxAssimilationRate * t_response;
      vc_AssimilationRateReference = pc_ReferenceMaxAssimilationRate * t_response;

      // OLD hard-coded response
      //  if(vw_MeanAirTemperature < pc_MinimumTemperatureForAssimilation)
      //  {
      //  	vc_AssimilationRate = 0.0;
      //  	vc_AssimilationRateReference = 0.0;
      //  }
      //  else if(vw_MeanAirTemperature < 10.0)
      //  {
      //  	vc_AssimilationRate = pc_MaxAssimilationRate * vw_MeanAirTemperature / 10.0 * 0.4;
      //  	vc_AssimilationRateReference = pc_ReferenceMaxAssimilationRate * vw_MeanAirTemperature / 10.0 * 0.4;
      //  }
      //  else if(vw_MeanAirTemperature < 15.0)
      //  {
      //  	vc_AssimilationRate = pc_MaxAssimilationRate * (0.4 + (vw_MeanAirTemperature - 10.0) / 5.0 * 0.5);
      //  	vc_AssimilationRateReference = pc_ReferenceMaxAssimilationRate * (0.4 + (vw_MeanAirTemperature - 10.0) / 5.0
      //  																																		* 0.5);
      //  }
      //  else if(vw_MeanAirTemperature < 25.0)
      //  {
      //  	vc_AssimilationRate = pc_MaxAssimilationRate * (0.9 + (vw_MeanAirTemperature - 15.0) / 10.0 * 0.1);
      //  	vc_AssimilationRateReference = pc_ReferenceMaxAssimilationRate * (0.9 + (vw_MeanAirTemperature - 15.0) / 10.0
      //  																																		* 0.1);
      //  }
      //  else if(vw_MeanAirTemperature < 35.0)
      //  {
      //  	vc_AssimilationRate = pc_MaxAssimilationRate * (1.0 - (vw_MeanAirTemperature - 25.0) / 10.0);
      //  	vc_AssimilationRateReference = pc_ReferenceMaxAssimilationRate * (1.0 - (vw_MeanAirTemperature - 25.0) / 10.0);
      //  }
      //  else
      //  {
      //  	vc_AssimilationRate = 0.0;
      //  	vc_AssimilationRateReference = 0.0;
      //  }

      // @FOR_PARAM
      // old KCo1
      double vc_HoffmannK1 = 220.0 + 0.158 * (vc_GlobalRadiation * 86400.0 / 1000000.0);

      // PAR [MJ m-2], Hoffmann's model requires [W m-2] ->
      // conversion of [MJ m-2] to [W m-2]

      // old coco
      double vc_HoffmannC0 = 80.0 - 0.036 * (vc_GlobalRadiation * 86400.0 / 1000000.0);

      // old KCO2
      double vc_HoffmannKCO2 = ((vw_AtmosphericCO2Concentration - vc_HoffmannC0) /
                                (vc_HoffmannK1 + vw_AtmosphericCO2Concentration - vc_HoffmannC0)) /
                               ((350.0 - vc_HoffmannC0) / (vc_HoffmannK1 + 350.0 - vc_HoffmannC0));

      vc_AssimilationRate = vc_AssimilationRate * vc_HoffmannKCO2;
      vc_AssimilationRateReference = vc_AssimilationRateReference * vc_HoffmannKCO2;
    }
  } else { // if pc_CarboxylationPathway = 2

    double t_response = WangEngelTemperatureResponse(vw_MeanAirTemperature,
                                                     pc_MinimumTemperatureForAssimilation,
                                                     pc_OptimumTemperatureForAssimilation,
                                                     pc_MaximumTemperatureForAssimilation,
                                                     1.0);

    vc_AssimilationRate = pc_MaxAssimilationRate * t_response;
    vc_AssimilationRateReference = pc_ReferenceMaxAssimilationRate * t_response;

    // OLD hard-coded response
    // if(vw_MeanAirTemperature < pc_MinimumTemperatureForAssimilation)
    //{
    //	vc_AssimilationRate = 0;
    //	vc_AssimilationRateReference = 0.0;

    //	// Sage & Kubien (2007): The temperature response of C3 and C4 phtotsynthesis.
    //	// Plant, Cell and Environment 30, 1086 - 1106.

    //}
    // else if(vw_MeanAirTemperature < 9.0)
    //{
    //	vc_AssimilationRate = pc_MaxAssimilationRate * vw_MeanAirTemperature / 10.0 * 0.08;
    //	vc_AssimilationRateReference = pc_ReferenceMaxAssimilationRate * vw_MeanAirTemperature / 10.0 * 0.08;
    //}
    // else if(vw_MeanAirTemperature < 14.0)
    //{
    //	vc_AssimilationRate = pc_MaxAssimilationRate * (0.071 + (vw_MeanAirTemperature - 9.0) * 0.03);
    //	vc_AssimilationRateReference = pc_ReferenceMaxAssimilationRate * (0.071 + (vw_MeanAirTemperature - 9.0) * 0.03);
    //}
    // else if(vw_MeanAirTemperature < 20.0)
    //{
    //	vc_AssimilationRate = pc_MaxAssimilationRate * (0.221 + (vw_MeanAirTemperature - 14.0) * 0.09);
    //	vc_AssimilationRateReference = pc_ReferenceMaxAssimilationRate * (0.221 + (vw_MeanAirTemperature - 14.0) * 0.09);
    //}
    // else if(vw_MeanAirTemperature < 24.0)
    //{
    //	vc_AssimilationRate = pc_MaxAssimilationRate * (0.761 + (vw_MeanAirTemperature - 20.0) * 0.04);
    //	vc_AssimilationRateReference = pc_ReferenceMaxAssimilationRate * (0.761 + (vw_MeanAirTemperature - 20.0) * 0.04);
    //}
    // else if(vw_MeanAirTemperature < 32.0)
    //{
    //	vc_AssimilationRate = pc_MaxAssimilationRate * (0.921 + (vw_MeanAirTemperature - 24.0) * 0.01);
    //	vc_AssimilationRateReference = pc_ReferenceMaxAssimilationRate * (0.921 + (vw_MeanAirTemperature - 24.0) * 0.01);
    //}
    // else if(vw_MeanAirTemperature < 38.0)
    //{
    //	vc_AssimilationRate = pc_MaxAssimilationRate;
    //	vc_AssimilationRateReference = pc_ReferenceMaxAssimilationRate;
    //}
    // else if(vw_MeanAirTemperature < 42.0)
    //{
    //	vc_AssimilationRate = pc_MaxAssimilationRate * (1.0 - (vw_MeanAirTemperature - 38.0) * 0.01);
    //	vc_AssimilationRateReference = pc_ReferenceMaxAssimilationRate * (1.0 - (vw_MeanAirTemperature - 38.0) * 0.01);
    //}
    // else if(vw_MeanAirTemperature < 45.0)
    //{
    //	vc_AssimilationRate = pc_MaxAssimilationRate * (0.96 - (vw_MeanAirTemperature - 42.0) * 0.04);
    //	vc_AssimilationRateReference = pc_ReferenceMaxAssimilationRate * (0.96 - (vw_MeanAirTemperature - 42.0) * 0.04);
    //}
    // else if(vw_MeanAirTemperature < 54.0)
    //{
    //	vc_AssimilationRate = pc_MaxAssimilationRate * (0.84 - (vw_MeanAirTemperature - 45.0) * 0.09);
    //	vc_AssimilationRateReference = pc_ReferenceMaxAssimilationRate * (0.84 - (vw_MeanAirTemperature - 45.0) * 0.09);
    //}
    // else
    //{
    //	vc_AssimilationRate = 0;
    //	vc_AssimilationRateReference = 0;

    //	//    // HERMES
    //	//  } else if (vw_MeanAirTemperature < 9.0) {
    //	//    vc_AssimilationRate = pc_MaxAssimilationRate * vw_MeanAirTemperature / 10.0 * 0.0555;
    //	//    vc_AssimilationRateReference = pc_ReferenceMaxAssimilationRate * vw_MeanAirTemperature / 10.0 * 0.0555;
    //	//  } else if (vw_MeanAirTemperature < 16.0) {
    //	//    vc_AssimilationRate = pc_MaxAssimilationRate * (0.05 + (vw_MeanAirTemperature - 9.0) /7.0 * 0.75);
    //	//    vc_AssimilationRateReference = pc_ReferenceMaxAssimilationRate * (0.05 + (vw_MeanAirTemperature - 9.0) /7 * 0.075);
    //	//  } else if (vw_MeanAirTemperature < 18.0) {
    //	//    vc_AssimilationRate = pc_MaxAssimilationRate * (0.8 + (vw_MeanAirTemperature - 16.0) * 0.07);
    //	//    vc_AssimilationRateReference = pc_ReferenceMaxAssimilationRate * (0.8 + (vw_MeanAirTemperature - 16.0) * 0.07);
    //	//  } else if (vw_MeanAirTemperature < 20.0) {
    //	//    vc_AssimilationRate = pc_MaxAssimilationRate * (0.94 + (vw_MeanAirTemperature - 18.0) * 0.03);
    //	//    vc_AssimilationRateReference = pc_ReferenceMaxAssimilationRate * (0.94 + (vw_MeanAirTemperature - 18.0) * 0.03);
    //	//  } else if (vw_MeanAirTemperature < 30.0) {
    //	//    vc_AssimilationRate = pc_MaxAssimilationRate;
    //	//    vc_AssimilationRateReference = pc_ReferenceMaxAssimilationRate;
    //	//  } else if (vw_MeanAirTemperature < 36.0) {
    //	//    vc_AssimilationRate = pc_MaxAssimilationRate * (1.0 - (vw_MeanAirTemperature - 30.0) * 0.0083);
    //	//    vc_AssimilationRateReference = pc_ReferenceMaxAssimilationRate * (1.0 - (vw_MeanAirTemperature - 30.0) * 0.0083);
    //	//  } else if (vw_MeanAirTemperature < 42.0) {
    //	//    vc_AssimilationRate = pc_MaxAssimilationRate * (0.95 - (vw_MeanAirTemperature - 36.0) * 0.0065);
    //	//    vc_AssimilationRateReference = pc_ReferenceMaxAssimilationRate * (0.95 - (vw_MeanAirTemperature - 36.0) * 0.0065);
    //	//  } else if (vw_MeanAirTemperature < 55.0) {
    //	//    vc_AssimilationRate = pc_MaxAssimilationRate * (0.91 - (vw_MeanAirTemperature - 42.0) * 0.07);
    //	//    vc_AssimilationRateReference = pc_ReferenceMaxAssimilationRate * (0.91 - (vw_MeanAirTemperature - 42.0) * 0.07);
    //	//  } else {
    //	//    vc_AssimilationRate = 0;
    //	//    vc_AssimilationRateReference = 0;
    //}
  }

  if (vc_CuttingDelayDays > 0) {
    vc_AssimilationRate = 0.1;

    // 	if (!_assimilatePartCoeffsReduced)
    // 	{

    // 		pc_AssimilatePartitioningCoeff[0][0] = 0.8;
    // 		pc_AssimilatePartitioningCoeff[0][1] = 0.2;

    // 		pc_AssimilatePartitioningCoeff[1][0] = 0.35;
    // 		pc_AssimilatePartitioningCoeff[1][1] = 0.25;
    // 		pc_AssimilatePartitioningCoeff[1][2] = 0.40;

    // 		pc_AssimilatePartitioningCoeff[2][0] = 0.30;
    // 		pc_AssimilatePartitioningCoeff[2][1] = 0.25;
    // 		pc_AssimilatePartitioningCoeff[2][2] = 0.45;

    // 		pc_AssimilatePartitioningCoeff[3][0] = 0.25;
    // 		pc_AssimilatePartitioningCoeff[3][1] = 0.25;
    // 		pc_AssimilatePartitioningCoeff[3][2] = 0.50;

    // 		pc_AssimilatePartitioningCoeff[4][0] = 0.25;
    // 		pc_AssimilatePartitioningCoeff[4][1] = 0.25;
    // 		pc_AssimilatePartitioningCoeff[4][2] = 0.50;

    // 		 this->pc_MaxAssimilationRate = pc_MaxAssimilationRate = 10;
    // 		_assimilatePartCoeffsReduced = true;
    // 	}
    // }
    // else
    // {
    // 	if (_assimilatePartCoeffsReduced)
    // 	{
    // 		//vc_AssimilationRate = 0.1;
    // 		pc_AssimilatePartitioningCoeff[0][0] = 0.69999999999999996;
    // 		pc_AssimilatePartitioningCoeff[0][1] = 0.29999999999999999;

    // 		pc_AssimilatePartitioningCoeff[1][0] = 0.20;
    // 		pc_AssimilatePartitioningCoeff[1][1] = 0.6;
    // 		pc_AssimilatePartitioningCoeff[1][2] = 0.199999;

    // 		pc_AssimilatePartitioningCoeff[2][1] = 0.0;
    // 		pc_AssimilatePartitioningCoeff[2][2] = 0.50;
    // 		pc_AssimilatePartitioningCoeff[2][2] = 0.5;

    // 		pc_AssimilatePartitioningCoeff[3][0] = 0.0;
    // 		pc_AssimilatePartitioningCoeff[3][1] = 0.50;
    // 		pc_AssimilatePartitioningCoeff[3][2] = 0.50;

    // 		pc_AssimilatePartitioningCoeff[4][0] = 0.0;
    // 		pc_AssimilatePartitioningCoeff[4][1] = 0.50;
    // 		pc_AssimilatePartitioningCoeff[4][2] = 0.50;

    // 		//pc_AssimilatePartitioningCoeff[1][1] += 0.3;
    // 		//pc_AssimilatePartitioningCoeff[1][0] -= 0.15;
    // 		//pc_AssimilatePartitioningCoeff[1][2] -= 0.215;
    // 		this->pc_MaxAssimilationRate = pc_MaxAssimilationRate = 30;
    // 		_assimilatePartCoeffsReduced = false;
    // 	}
  }

  vc_AssimilationRate = max(0.1, vc_AssimilationRate);
  vc_AssimilationRateReference = max(0.1, vc_AssimilationRateReference);

  ///////////////////////////////////////////////////////////////////////////
  // Calculation of light interception in the crop
  //
  // Penning De Vries, F.W.T and H.H. van Laar (1982): Simulation of
  // plant growth and crop production. Pudoc, Wageningen, The
  // Netherlands, p. 87-97.
  ///////////////////////////////////////////////////////////////////////////

  // old EFFE
  double vc_NetRadiationUseEfficiency = (1.0 - pc_CanopyReflectionCoeff) * vc_RadiationUseEfficiency;
  double vc_NetRadiationUseEfficiencyReference = (1.0 - pc_CanopyReflectionCoeff) * vc_RadiationUseEfficiencyReference;

  double SSLAE = sin((90.0 + vc_Declination - vs_Latitude) * PI / 180.0); // = HERMES

  double X = log(1.0 + 0.45 * vc_ClearDayRadiation / (vc_EffectiveDayLength * 3600.0) * vc_NetRadiationUseEfficiency /
                 (SSLAE * vc_AssimilationRate)); // = HERMES
  double XReference = log(1.0 + 0.45 * vc_ClearDayRadiation / (vc_EffectiveDayLength * 3600.0) *
                          vc_NetRadiationUseEfficiencyReference / (SSLAE * vc_AssimilationRateReference));

  double PHCH1 = SSLAE * vc_AssimilationRate * vc_EffectiveDayLength * X / (1.0 + X); // = HERMES
  double PHCH1Reference =
    SSLAE * vc_AssimilationRateReference * vc_EffectiveDayLength * XReference / (1.0 + XReference);

  double Y = log(1.0 + 0.55 * vc_ClearDayRadiation / (vc_EffectiveDayLength * 3600.0) * vc_NetRadiationUseEfficiency /
                 ((5.0 - SSLAE) * vc_AssimilationRate)); // = HERMES
  double YReference = log(1.0 + 0.55 * vc_ClearDayRadiation / (vc_EffectiveDayLength * 3600.0) *
                          vc_NetRadiationUseEfficiency / ((5.0 - SSLAE) * vc_AssimilationRateReference));

  double PHCH2 = (5.0 - SSLAE) * vc_AssimilationRate * vc_EffectiveDayLength * Y / (1.0 + Y); // = HERMES
  double PHCH2Reference =
    (5.0 - SSLAE) * vc_AssimilationRateReference * vc_EffectiveDayLength * YReference / (1.0 + YReference);

  double PHCH = 0.95 * (PHCH1 + PHCH2) + 20.5; // = HERMES
  double PHCHReference = 0.95 * (PHCH1Reference + PHCH2Reference) + 20.5;

  // vc_OxygenDeficit separates drought stress (ETa/Etp) from saturation stress.
  // old VSWELL
  double vc_DroughtStressThreshold = vc_OxygenDeficit < 1.0
                                       ? 0.0
                                       : pc_DroughtStressThreshold[vc_DevelopmentalStage];

  // Calculation of time fraction for overcast sky situations by
  // comparing clear day radiation and measured PAR in [J m-2].
  // HERMES uses PAR as 50% of global radiation
  // old FOV
  double vc_OvercastSkyTimeFraction = 0;
  if (vc_ClearDayRadiation != 0) {
    vc_OvercastSkyTimeFraction =
      (vc_ClearDayRadiation - (1000000.0 * vc_GlobalRadiation * 0.50)) / (0.8 * vc_ClearDayRadiation);
  } // [J m-2]
  vc_OvercastSkyTimeFraction = max(0.0, min(vc_OvercastSkyTimeFraction, 1.0));

  auto code = [&](std::function<double(double)> calcFractionOfInterceptedRadiation, double LAI) {
    double fractionOfInterceptedRadiation = calcFractionOfInterceptedRadiation(LAI);

    double PHC3 = PHCH * fractionOfInterceptedRadiation;
    double PHC3Reference = PHCHReference * calcFractionOfInterceptedRadiation(pc_ReferenceLeafAreaIndex);

    double PHC4 = vc_AstronomicDayLenght * LAI * vc_AssimilationRate;
    double PHC4Reference = vc_AstronomicDayLenght * pc_ReferenceLeafAreaIndex * vc_AssimilationRateReference;

    double PHCL = PHC3 < PHC4
                    ? PHC3 * (1.0 - exp(-PHC4 / PHC3))
                    : PHC4 * (1.0 - exp(-PHC3 / PHC4));

    double PHCLReference = PHC3Reference < PHC4Reference
                             ? PHC3Reference * (1.0 - exp(-PHC4Reference / PHC3Reference))
                             : PHC4Reference * (1.0 - exp(-PHC3Reference / PHC4Reference));

    double Z = vc_OvercastDayRadiation / (vc_EffectiveDayLength * 3600.0) * vc_NetRadiationUseEfficiency /
               (5.0 * vc_AssimilationRate);

    double PHOH1 = 5.0 * vc_AssimilationRate * vc_EffectiveDayLength * Z / (1.0 + Z);
    double PHOH = 0.9935 * PHOH1 + 1.1;
    double PHO3 = PHOH * fractionOfInterceptedRadiation;
    double PHO3Reference = PHOH * calcFractionOfInterceptedRadiation(pc_ReferenceLeafAreaIndex);

    double PHOL = PHO3 < PHC4
                    ? PHO3 * (1.0 - exp(-PHC4 / PHO3))
                    : PHC4 * (1.0 - exp(-PHO3 / PHC4));

    double PHOLReference = PHO3Reference < PHC4Reference
                             ? PHO3Reference * (1.0 - exp(-PHC4Reference / PHO3Reference))
                             : PHC4Reference * (1.0 - exp(-PHO3Reference / PHC4Reference));

    double vc_ClearDayCO2Assimilation = LAI < 5.0 ? PHCL : PHCH; // [J m-2]
    double vc_OvercastDayCO2Assimilation = LAI < 5.0 ? PHOL : PHOH; // [J m-2]

    double vc_ClearDayCO2AssimilationReference = PHCLReference;
    double vc_OvercastDayCO2AssimilationReference = PHOLReference;

    // Calculation of gross CO2 assimilation in dependence of cloudiness
    // old DTGA
    double vc_GrossCO2Assimilation = vc_OvercastSkyTimeFraction * vc_OvercastDayCO2Assimilation +
                                     (1.0 - vc_OvercastSkyTimeFraction) * vc_ClearDayCO2Assimilation;

    // used for ET0 calculation
    double vc_GrossCO2AssimilationReference = vc_OvercastSkyTimeFraction * vc_OvercastDayCO2AssimilationReference +
                                              (1.0 - vc_OvercastSkyTimeFraction) * vc_ClearDayCO2AssimilationReference;

    // Gross CO2 assimilation is used for reference evapotranspiration calculation.
    // For this purpose it must not be affected by drought stress, as the grass
    // reference is defined as being always well supplied with water. Water stress
    // is acting at a later stage.
    if (vc_TranspirationDeficit < vc_DroughtStressThreshold) {
      //MP. what about conditions where vc_TranspirationDeficit > vc_DroughtStressThreshold
      vc_GrossCO2Assimilation = vc_GrossCO2Assimilation; // *  vc_TranspirationDeficit;
    }

#pragma region hourly FvCB code
    int vs_JulianDay = currentDate.julianDay();
    double dailyGP = 0;
    if (cropPs->__enable_hourly_FvCB_photosynthesis__ && pc_CarboxylationPathway == 1) {
      vector<double> hourlyGlobrads;
      vector<double> hourlyExtrarad;
      int sunriseH = 0;

      for (int h = 0; h < 24; h++) {
        double hgr = hourlyRad(vc_GlobalRadiation, vs_Latitude, vs_JulianDay, h);
        if (hgr > 0 && hourlyGlobrads.back() == 0.0) {
          sunriseH = h;
        }
        hourlyGlobrads.push_back(hgr);

        hourlyExtrarad.push_back(hourlyRad(vc_ExtraterrestrialRadiation, vs_Latitude, vs_JulianDay, h));
      }

      using namespace FvCB;

      _guentherEmissions = Voc::Emissions();
      _jjvEmissions = Voc::Emissions();

      for (int h = 0; h < 24; h++) {
#ifdef TEST_FVCB_HOURLY_OUTPUT
        FvCB::tout()
          << currentDate.toIsoDateString()
          << "," << h
          << "," << speciesPs.pc_SpeciesId << "/" << cultivarPs.pc_CultivarId
          << "," << vw_AtmosphericCO2Concentration;
#endif
        // hourly photosynthesis
        FvCB_canopy_hourly_in FvCB_in;

        double hourlyTemp = hourlyT(vw_MinAirTemperature, vw_MaxAirTemperature, h, sunriseH);
        FvCB_in.leaf_temp = hourlyTemp;
        FvCB_in.global_rad = hourlyGlobrads.at(h);
        FvCB_in.extra_terr_rad = hourlyExtrarad.at(h);
        FvCB_in.LAI = LAI;
        FvCB_in.solar_el = solarElevation(h, vs_Latitude, vs_JulianDay);
        FvCB_in.VPD = hourlyVaporPressureDeficit(hourlyTemp, vw_MinAirTemperature, vw_MeanAirTemperature,
                                                 vw_MaxAirTemperature);
        FvCB_in.Ca = vw_AtmosphericCO2Concentration;

        FvCB_canopy_hourly_params hps;
        hps.Vcmax_25 = speciesPs.VCMAX25 * vc_O3_shortTermDamage * vc_O3_senescence;

        auto FvCB_res = FvCB_canopy_hourly_C3(FvCB_in, hps);

        vc_sunlitLeafAreaIndex[h] = FvCB_res.sunlit.LAI;
        vc_shadedLeafAreaIndex[h] = FvCB_res.shaded.LAI;

        // [µmol CO2 m-2 (h-1)] -> [kg CO2 ha-1 (d-1)]
        dailyGP += FvCB_res.canopy_gross_photos * 44. / 100. / 1000.;

        // hourly O3 uptake and damage
        O3impact::O3_impact_in O3_in;
        O3impact::O3_impact_params O3_par;
        O3_par.gamma3 = 0.05; // TODO: calibrate and add to crop params
        O3_par.gamma1 = 0.025; // TODO: calibrate and add to crop params

        auto root_depth = get_RootingDepth();
        if (root_depth >= 1) // the crop has emerged
        {
#ifdef TEST_O3_HOURLY_OUTPUT
          O3impact::tout()
            << currentDate.toIsoDateString()
            << "," << h
            << "," << speciesPs.pc_SpeciesId << "/" << cultivarPs.pc_CultivarId
            << "," << vw_AtmosphericCO2Concentration
            << "," << vw_AtmosphericO3Concentration;
#endif
          double FC = 0, WP = 0, SWC = 0;
          for (int i = 0; i < root_depth; i++) {
            FC += (*soilColumn)[i]._sps.vs_FieldCapacity;
            WP += (*soilColumn)[i]._sps.vs_PermanentWiltingPoint;
            SWC += (*soilColumn)[i].vs_SoilMoisture_m3;
          }

          // weighted average gs and conversion from unit ground area to unit leaf area
          double lai_sun_weight = FvCB_res.sunlit.LAI / (FvCB_res.sunlit.LAI + FvCB_res.shaded.LAI);
          double lai_sh_weight = 1 - lai_sun_weight;
          double avg_leaf_gs = lai_sh_weight * FvCB_res.shaded.gs / FvCB_res.shaded.LAI;
          if (FvCB_res.sunlit.LAI > 0) {
            avg_leaf_gs += lai_sun_weight * FvCB_res.sunlit.gs / FvCB_res.sunlit.LAI;
          }

          O3_in.FC = FC / (root_depth + 1); // field capacity, m3 m-3, avg in the rooted zone
          O3_in.WP = WP / (root_depth + 1); // wilting point, m3 m-3
          O3_in.SWC = SWC / (root_depth + 1); // soil water content, m3 m-3
          O3_in.ET0 = get_ReferenceEvapotranspiration();
          O3_in.O3a = vw_AtmosphericO3Concentration; // ambient O3 partial pressure, nbar or nmol mol-1
          O3_in.gs = avg_leaf_gs; // stomatal conductance mol m-2 s-1 bar-1
          O3_in.h = h; // hour of the day (0-23)
          O3_in.reldev = vc_RelativeTotalDevelopment;
          O3_in.GDD_flo = vc_TemperatureSumToFlowering; // GDD from emergence to flowering
          O3_in.GDD_mat = vc_TotalTemperatureSum; // GDD from emergence to maturity
          O3_in.fO3s_d_prev = vc_O3_shortTermDamage;
          // short term ozone induced reduction of Ac of the previous time step
          O3_in.sum_O3_up = vc_O3_sumUptake; // cumulated O3 uptake, µmol m-2 (unit ground area)

          auto O3_res = O3impact::O3_impact_hourly(O3_in, O3_par, pc_WaterDeficitResponseOn);

          vc_O3_shortTermDamage = O3_res.fO3s_d;
          vc_O3_longTermDamage = O3_res.fO3l;
          vc_O3_senescence = O3_res.fLS;
          vc_O3_sumUptake += O3_res.hourly_O3_up;
          vc_O3_WStomatalClosure = O3_res.WS_st_clos;
        }

        // calculate VOC emissions
        double globradWm2 = FvCB_in.global_rad * 1000000.0 / 3600; // MJ m-2 h-1 -> W m-2
        if (_index240 < _stepSize240 - 1) {
          _index240++;
        } else {
          _index240 = 0;
          _full240 = true;
        }
        _rad240[_index240] = globradWm2;
        _tfol240[_index240] = FvCB_in.leaf_temp;

        if (_index24 < _stepSize24 - 1) {
          _index24++;
        } else {
          _index24 = 0;
          _full24 = true;
        }
        _rad24[_index24] = globradWm2;
        _tfol24[_index24] = FvCB_in.leaf_temp;

        Voc::MicroClimateData mcd;
        // hourly or time step average global radiation (in case of monica usually 24h)
        mcd.rad = globradWm2;
        mcd.rad24 = accumulate(_rad24.begin(), _rad24.end(), 0.0) / (_full24 ? _rad24.size() : _index24 + 1);
        mcd.rad240 = accumulate(_rad240.begin(), _rad240.end(), 0.0) / (_full240 ? _rad240.size() : _index240 + 1);
        mcd.tFol = FvCB_in.leaf_temp;
        mcd.tFol24 = accumulate(_tfol24.begin(), _tfol24.end(), 0.0) / (_full24 ? _tfol24.size() : _index24 + 1);
        mcd.tFol240 = accumulate(_tfol240.begin(), _tfol240.end(), 0.0) / (_full240 ? _tfol240.size() : _index240 + 1);
        mcd.co2concentration = vw_AtmosphericCO2Concentration;

        // auto sunShadeLaiAtZenith = laiSunShade(_sitePs.vs_Latitude, julday, 12, vc_LeafAreaIndex);
        // mcd.sunlitfoliagefraction = sunShadeLaiAtZenith.first / lai;
        // mcd.sunlitfoliagefraction24 = mcd.sunlitfoliagefraction;

        Voc::SpeciesData species;
        // species.id = 0; // right now we just have one crop at a time, so no need to distinguish multiple crops
        species.lai = LAI;
        species.mFol =
          get_OrganGreenBiomass(OId::LEAF) / (100. * 100.); // kg/ha -> kg/m2
        species.sla =
          species.mFol > 0
            ? species.lai / species.mFol
            : pc_SpecificLeafArea[vc_DevelopmentalStage] * 100. *
              100.; // ha/kg -> m2/kg

        species.EF_MONO = speciesPs.EF_MONO;
        species.EF_MONOS = speciesPs.EF_MONOS;
        species.EF_ISO = speciesPs.EF_ISO;
        species.VCMAX25 = speciesPs.VCMAX25;
        species.AEKC = speciesPs.AEKC;
        species.AEKO = speciesPs.AEKO;
        species.AEVC = speciesPs.AEVC;
        species.KC25 = speciesPs.KC25;

        auto ges = Voc::calculateGuentherVOCEmissions(species, mcd, 1. / 24.);
        // cout << "G: C: " << ges.monoterpene_emission << " em: " << ges.isoprene_emission << endl;
        _guentherEmissions += ges;
        // debug() << "guenther: isoprene: " << gems.isoprene_emission << " monoterpene: " << gems.monoterpene_emission << endl;

#ifdef TEST_HOURLY_OUTPUT
        tout()
          << currentDate.toIsoDateString()
          << "," << h
          << "," << speciesPs.pc_SpeciesId << "/" << cultivarPs.pc_CultivarId
          << "," << FvCB_in.global_rad
          << "," << FvCB_in.extra_terr_rad
          << "," << FvCB_in.solar_el
          << "," << mcd.rad
          << "," << FvCB_in.LAI
          << "," << species.mFol
          << "," << species.sla
          << "," << FvCB_in.leaf_temp
          << "," << FvCB_in.VPD
          << "," << FvCB_in.Ca
          << "," << FvCB_in.fO3
          << "," << FvCB_in.fls
          << "," << FvCB_res.canopy_net_photos
          << "," << FvCB_res.canopy_resp
          << "," << FvCB_res.canopy_gross_photos
          << "," << FvCB_res.jmax_c;
        //<< "," << ges.isoprene_emission
        //<< "," << ges.monoterpene_emission;
#endif
        double sun_LAI = FvCB_res.sunlit.LAI;
        double sh_LAI = FvCB_res.shaded.LAI;
        // JJV
        for (const auto& lf : {FvCB_res.sunlit, FvCB_res.shaded}) {
          species.lai = lf.LAI;
          species.mFol = get_OrganGreenBiomass(OId::LEAF) / (100. * 100.) * lf.LAI /
                         (sun_LAI + sh_LAI); // kg/ha -> kg/m2
          species.sla =
            species.mFol > 0
              ? species.lai / species.mFol
              : pc_SpecificLeafArea[vc_DevelopmentalStage] * 100. *
                100.; // ha/kg -> m2/kg

          mcd.rad = lf.rad; // lf.rad; //W m-2 global incident

          // auto ges = Voc::calculateGuentherVOCEmissions(species, mcd, 1. / 24.);
          // cout << "G: C: " << ges.monoterpene_emission << " em: " << ges.isoprene_emission << endl;
          //_guentherEmissions += ges;
          // debug() << "guenther: isoprene: " << gems.isoprene_emission << " monoterpene: " << gems.monoterpene_emission << endl;

          _cropPhotosynthesisResults.kc = lf.kc;
          _cropPhotosynthesisResults.ko = lf.ko * 1000;
          _cropPhotosynthesisResults.oi = lf.oi * 1000;
          _cropPhotosynthesisResults.ci = lf.ci;
          _cropPhotosynthesisResults.vcMax = FvCB::Vcmax_bernacchi_f(mcd.tFol, speciesPs.VCMAX25) * vc_CropNRedux *
                                             vc_TranspirationDeficit;
          // lf.vcMax; MP: do we have to include OxygenDeficit?
          _cropPhotosynthesisResults.jMax =
            FvCB::Jmax_bernacchi_f(mcd.tFol, 120) * vc_CropNRedux * vc_TranspirationDeficit;
          // lf.jMax; MP: do we have to include OxygenDeficit?
          _cropPhotosynthesisResults.jj = lf.jj;
          _cropPhotosynthesisResults.jj1000 = lf.jj1000;
          _cropPhotosynthesisResults.jv = lf.jv;

          auto jjves = Voc::calculateJJVVOCEmissions(species, mcd, _cropPhotosynthesisResults, 1. / 24., false);
          // cout << "J: C: " << jjves.monoterpene_emission << " em: " << jjves.isoprene_emission << endl;
          _jjvEmissions += jjves;
          // debug() << "jjv: isoprene: " << jjvems.isoprene_emission << " monoterpene: " << jjvems.monoterpene_emission << endl;

#ifdef TEST_HOURLY_OUTPUT
          tout()
            << "," << species.lai
            << "," << species.mFol
            << "," << species.sla
            << "," << lf.gs
            << "," << lf.kc
            << "," << lf.ko
            << "," << lf.oi
            << "," << lf.ci
            << "," << lf.comp
            << "," << lf.vcMax
            << "," << lf.jMax
            << "," << lf.rad
            << "," << lf.jj
            << "," << lf.jj1000
            << "," << lf.jv
            << "," << ges.isoprene_emission
            << "," << ges.monoterpene_emission
            << "," << jjves.isoprene_emission
            << "," << jjves.monoterpene_emission;
#endif
        }
#ifdef TEST_HOURLY_OUTPUT
        tout() << endl;
#endif
      }
    }
#pragma endregion hourly FvCB code

    vc_GrossCO2Assimilation = cropPs->__enable_hourly_FvCB_photosynthesis__ && pc_CarboxylationPathway == 1
                                ? dailyGP
                                : vc_GrossCO2Assimilation;

    return make_pair(vc_GrossCO2Assimilation, vc_GrossCO2AssimilationReference);
  };

  double vc_GrossCO2Assimilation = 0, vc_GrossCO2AssimilationReference = 0;
  double zeroHeightEps = 0.00001;
  if (_intercroppingOtherCropHeight <= zeroHeightEps || vc_CropHeight <= zeroHeightEps) {
    debug() << "no-other-crop: dev-stage: " << (vc_DevelopmentalStage + 1)
      << " other-crop-height: " << _intercroppingOtherCropHeight
      << " own-crop-height: " << vc_CropHeight << endl;
    debug() << "vc_OvercastSkyTimeFraction: " << vc_OvercastSkyTimeFraction << endl;
    auto F_t1 = [this](double LAI) {
      return 1.0 - exp(-cultivarPs.pc_LightExtinctionCoefficient * LAI);
    };
    tie(vc_GrossCO2Assimilation, vc_GrossCO2AssimilationReference) = code(F_t1, vc_LeafAreaIndex);
    fractionOfInterceptedRadiation1 = F_t1(vc_LeafAreaIndex);
    debug() << "assimilation calculations for only one crop: grossCO2Assim: " << vc_GrossCO2Assimilation
      << " ref: " << vc_GrossCO2AssimilationReference << endl;
  } else {
    double k_t = cropPs->pc_intercropping_k_t;
    double k_s = cropPs->pc_intercropping_k_s;
    double phRedux = cropPs->pc_intercropping_phRedux;
    double ph_s = min(_intercroppingOtherCropHeight, vc_CropHeight);
    double ph_t = max(_intercroppingOtherCropHeight, vc_CropHeight);
    double phr = vc_CropHeight <= zeroHeightEps ? 0.0 : ph_s * phRedux / ph_t;
    //std::cout << "phRedux: " << phRedux << " phr: " << phr << " CropHeight: " << vc_CropHeight << " otherCropHeight: " << _intercroppingOtherCropHeight << " /: " << phRedux / vc_CropHeight << std::endl;
    double LAI_t = _intercroppingOtherCropHeight < vc_CropHeight ? vc_LeafAreaIndex : _intercroppingOtherLAIt;
    double LAI_t1 = max(0.001, (1 - phr) * LAI_t);
    // fraction of radiation intercepted for upper plant part
    auto F_t1 = [k_t](double LAI_t1) {
      return 1.0 - exp(-k_t * LAI_t1);
    };
    double one_minus_F_t1_val = 1 - F_t1(LAI_t1);

    assert(_intercroppingOtherCropHeight > zeroHeightEps);
    if (vc_CropHeight < _intercroppingOtherCropHeight) {
      debug() << "smaller crop: dev-stage: " << (vc_DevelopmentalStage + 1)
        << " other-crop-height: " << _intercroppingOtherCropHeight
        << " own-crop-height: " << vc_CropHeight << endl;

      // send out LAI_s and wait for LAI_t2 from the larger plant
      double LAI_t2 = phr * _intercroppingOtherLAIt;
      if (_intercropping->isAsync()) {
        auto wreq = _intercropping->writer.writeRequest();
        auto wval = wreq.initValue();
        wval.setLait(vc_LeafAreaIndex);
        auto prom = wreq.send();
        //.wait(_intercropping->ioContext->waitScope); //.eagerlyEvaluate(nullptr);//[](kj::Exception&& ex){ cout << "crop-module: CropModule::fc_CropPhotosynthesis: write LAI failed: " << ex.getDescription().cStr() << endl;});
        auto val = _intercropping->reader.readRequest().send().wait(_intercropping->ioContext->waitScope).getValue();
        LAI_t2 = val.isLait()
                   ? val.getLait()
                   : -9999; // throw kj::Exception(kj::Exception::Type::FAILED, "crop-module.cpp", 2718);
        debug() << "sent LAI_s: " << vc_LeafAreaIndex << " received LAI_t2: " << LAI_t2 << endl;
      }
      // fraction of radiation intercepted for lower plant part

      auto F_s = [k_s, k_t, LAI_t2, one_minus_F_t1_val](double LAI_s) {
        return (k_s * LAI_s) / (k_t * LAI_t2 + k_s * LAI_s) * (1 - exp(-k_t * LAI_t2 - k_s * LAI_s)) *
               one_minus_F_t1_val;
      };

      tie(vc_GrossCO2Assimilation, vc_GrossCO2AssimilationReference) = code(F_s, vc_LeafAreaIndex);
      fractionOfInterceptedRadiation1 = F_s(vc_LeafAreaIndex) / one_minus_F_t1_val;
      debug() << "assimilation calculations for smaller crop: grossCO2Assim: " << vc_GrossCO2Assimilation
        << " ref: " << vc_GrossCO2AssimilationReference << endl;
    } else {
      debug() << "taller crop: dev-stage: " << (vc_DevelopmentalStage + 1)
        << " other-crop-height: " << _intercroppingOtherCropHeight
        << " own-crop-height: " << vc_CropHeight << endl;
      // this crop is larger than the other
      double LAI_t2 = max(0.001, phr * vc_LeafAreaIndex);

      // send out LAI_t2 and wait for LAI_s from the smaller plant
      double LAI_s = _intercroppingOtherLAIt;
      if (_intercropping->isAsync()) {
        auto wreq = _intercropping->writer.writeRequest();
        auto wval = wreq.initValue();
        wval.setLait(LAI_t2);
        auto prom = wreq.send();
        //.wait(_intercropping->ioContext->waitScope); //.eagerlyEvaluate(nullptr);//[](kj::Exception&& ex){ cout << "crop-module: CropModule::fc_CropPhotosynthesis: write LAI failed: " << ex.getDescription().cStr() << endl;});
        auto val = _intercropping->reader.readRequest().send().wait(_intercropping->ioContext->waitScope).getValue();
        LAI_s = val.isLait()
                  ? val.getLait()
                  : -9999; // throw kj::Exception(kj::Exception::Type::FAILED, "crop-module.cpp", 2724);
        debug() << "sent LAI_t2: " << LAI_t2 << " received LAI_s: " << LAI_s << endl;
      }
      // fraction of radiation intercepted for lower plant part
      auto F_t2 = [k_s, k_t, LAI_s, one_minus_F_t1_val](double LAI_t2) {
        return (k_t * LAI_t2) / (k_t * LAI_t2 + k_s * LAI_s) * (1 - exp(-k_t * LAI_t2 - k_s * LAI_s)) *
               one_minus_F_t1_val;
      };

      auto t1 = code(F_t1, LAI_t1);
      auto t2 = code(F_t2, LAI_t2);
      vc_GrossCO2Assimilation = t1.first + t2.first;
      fractionOfInterceptedRadiation1 = F_t1(LAI_t1);
      fractionOfInterceptedRadiation2 = F_t2(LAI_t2) / one_minus_F_t1_val;
      vc_GrossCO2AssimilationReference = t1.second + t2.second;
      debug() << "assimilation calculations for taller crop: grossCO2Assim: " << vc_GrossCO2Assimilation
        << " ref: " << vc_GrossCO2AssimilationReference << endl;
    }
  }

  // [TRANSPLANT SHOCK] Photosynthesis Limitation.
  // Reduces the daily gross CO2 assimilation rate according to the shock recovery efficiency factor.
  if (vc_TransplantEfficiency < 1.0) {
    vc_GrossCO2Assimilation *= vc_TransplantEfficiency;
  }

  // Calculation of photosynthesis rate from [kg CO2 ha-1 d-1] to [kg CH2O ha-1 d-1]
  vc_GrossPhotosynthesis = vc_GrossCO2Assimilation * 30.0 / 44.0;

  // Calculation of photosynthesis rate from [kg CO2 ha-1 d-1]  to [mol m-2 s-1] or [cm3 cm-2 s-1]
  vc_GrossPhotosynthesis_mol = vc_GrossCO2Assimilation * 22414.0 / (10.0 * 3600.0 * 24.0 * 44.0);
  vc_GrossPhotosynthesisReference_mol = vc_GrossCO2AssimilationReference * 22414.0 / (10.0 * 3600.0 * 24.0 * 44.0);

  // Converting photosynthesis rate from [kg CO2 ha leaf-1 d-1] to [kg CH2O ha-1  d-1]
  vc_Assimilates = vc_GrossCO2Assimilation * 30.0 / 44.0;

  // reduction value for assimilate amount to simulate field conditions;
  vc_Assimilates *= pc_FieldConditionModifier;

  // reduction value for assimilate amount to simulate frost damage;
  vc_Assimilates *= vc_CropFrostRedux;

  //MP: added reduction value for assimilate amount to simulate waterlogging;
  vc_Assimilates *= vc_OxygenDeficit;

  if (vc_TranspirationDeficit < vc_DroughtStressThreshold) { //MP: Access point for drought optimisation
    vc_Assimilates = vc_Assimilates * vc_TranspirationDeficit;
    // vc_DroughtStressThreshold; // MP:changed to exclude division
  }

  vc_GrossAssimilates = vc_Assimilates;

  // ########################################################################
  // #                AGROSIM                 #
  // ########################################################################

  // AGROSIM night and day temperatures
  double vc_PhotoTemperature = vw_MaxAirTemperature - ((vw_MaxAirTemperature - vw_MinAirTemperature) / 4.0);
  double vc_NightTemperature = vw_MinAirTemperature + ((vw_MaxAirTemperature - vw_MinAirTemperature) / 4.0);

  double vc_MaintenanceRespirationSum = 0.0;
  // AGOSIM night and day maintenance and growth respiration
  for (int i_Organ = 0; i_Organ < pc_NumberOfOrgans; i_Organ++) {
    vc_MaintenanceRespirationSum +=
      vc_OrganGreenBiomass[i_Organ] * pc_OrganMaintenanceRespiration[i_Organ]; // [kg CH2O ha-1]
    // * vc_ActiveFraction[i_Organ]; wenn nicht schon durch acc dead matter abgedeckt
  }

  double vc_NormalisedDayLength = 2.0 - (vc_PhotoperiodicDaylength / 12.0);

  double vc_PhotoMaintenanceRespiration = vc_MaintenanceRespirationSum * pow(2.0,
                                                                             (pc_MaintenanceRespirationParameter_1 *
                                                                              (vc_PhotoTemperature -
                                                                               pc_MaintenanceRespirationParameter_2))) *
                                          (2.0 - vc_NormalisedDayLength); // @todo: [g m-2] --> [kg ha-1]

  double vc_DarkMaintenanceRespiration = vc_MaintenanceRespirationSum * pow(2.0, (pc_MaintenanceRespirationParameter_1 *
                                                                              (vc_NightTemperature -
                                                                               pc_MaintenanceRespirationParameter_2))) *
                                         vc_NormalisedDayLength; // @todo: [g m-2] --> [kg ha-1]

  vc_MaintenanceRespirationAS = vc_PhotoMaintenanceRespiration + vc_DarkMaintenanceRespiration; // [kg CH2O ha-1]

  vc_Assimilates -= vc_PhotoMaintenanceRespiration + vc_DarkMaintenanceRespiration; // [kg CH2O ha-1]

  double vc_GrowthRespirationSum = 0.0;

  if (vc_Assimilates > 0) {
    for (int i_Organ = 0; i_Organ < pc_NumberOfOrgans; i_Organ++) {
      vc_GrowthRespirationSum += pc_AssimilatePartitioningCoeff[vc_DevelopmentalStage][i_Organ] * vc_Assimilates *
        pc_OrganGrowthRespiration[i_Organ];
    }
  }

  double vc_PhotoGrowthRespiration = 0.0;
  if (vc_Assimilates > 0.0) {
    vc_PhotoGrowthRespiration = vc_GrowthRespirationSum * pow(2.0, (pc_GrowthRespirationParameter_1 *
                                                                    (vc_PhotoTemperature -
                                                                     pc_GrowthRespirationParameter_2))) *
                                (2.0 - vc_NormalisedDayLength); // [kg CH2O ha-1]

    if (vc_Assimilates > vc_PhotoGrowthRespiration) {
      vc_Assimilates -= vc_PhotoGrowthRespiration;
    } else {
      vc_PhotoGrowthRespiration = vc_Assimilates; // in this case the plant will be restricted in growth!
      vc_Assimilates = 0.0;
    }
  }

  double vc_DarkGrowthRespiration = 0.0;
  if (vc_Assimilates > 0.0) {
    vc_DarkGrowthRespiration = vc_GrowthRespirationSum * pow(2.0, (pc_GrowthRespirationParameter_1 *
                                                                   (vc_PhotoTemperature -
                                                                    pc_GrowthRespirationParameter_2))) *
                               vc_NormalisedDayLength; // [kg CH2O ha-1]

    if (vc_Assimilates > vc_DarkGrowthRespiration) {
      vc_Assimilates -= vc_DarkGrowthRespiration;
    } else {
      vc_DarkGrowthRespiration = vc_Assimilates; // in this case the plant will be restricted in growth!
      vc_Assimilates = 0.0;
    }
  }
  vc_GrowthRespirationAS = vc_PhotoGrowthRespiration + vc_DarkGrowthRespiration; // [kg CH2O ha-1]
  vc_TotalRespired = vc_GrossAssimilates - vc_Assimilates; // [kg CH2O ha-1]

  // to reactivate HERMES algorithms, needs to be vc_NetPhotosynthesis
  // used instead of  vc_Assimilates in the subsequent methods
  // #########################################################################
  // HERMES calculation of maintenance respiration in dependence of temperature

  // old TEFF
  double vc_MaintenanceTemperatureDependency = pow(2.0, (0.1 * vw_MeanAirTemperature - 2.5));

  // old MAINTS
  double vc_MaintenanceRespiration = 0.0;
  for (int i_Organ = 0; i_Organ < pc_NumberOfOrgans; i_Organ++) {
    vc_MaintenanceRespiration += vc_OrganGreenBiomass[i_Organ] * pc_OrganMaintenanceRespiration[i_Organ];
  }

  if (vc_GrossPhotosynthesis < (vc_MaintenanceRespiration * vc_MaintenanceTemperatureDependency)) {
    vc_NetMaintenanceRespiration = vc_GrossPhotosynthesis;
  } else {
    vc_NetMaintenanceRespiration = vc_MaintenanceRespiration * vc_MaintenanceTemperatureDependency;
  }

  if (vw_MeanAirTemperature < pc_MinimumTemperatureForAssimilation) {
    vc_GrossPhotosynthesis = vc_NetMaintenanceRespiration;
  }
}

/**
 * @brief Heat stress impact //MP: currently, heat affects flowering and therefore fertility
 *
 * @param vw_MaxAirTemperature
 * @param vw_MinAirTemperature
 * @param vc_CurrentTotalTemperatureSum
 */
void CropModule::fc_HeatStressImpact(double vw_MaxAirTemperature,
                                     double vw_MinAirTemperature) {
  // AGROSIM night and day temperatures
  double vc_PhotoTemperature = vw_MaxAirTemperature - ((vw_MaxAirTemperature - vw_MinAirTemperature) / 4.0);
  double vc_FractionOpenFlowers = 0.0;
  double vc_YesterdaysFractionOpenFlowers = 0.0;

  if ((pc_BeginSensitivePhaseHeatStress == 0.0) && (pc_EndSensitivePhaseHeatStress == 0.0)) {
    vc_TotalCropHeatImpact = 1.0;
  }

  if ((vc_CurrentTotalTemperatureSum >= pc_BeginSensitivePhaseHeatStress) &&
      vc_CurrentTotalTemperatureSum < pc_EndSensitivePhaseHeatStress) {
    // Crop heat redux: Challinor et al. (2005): Simulation of the impact of high
    // temperature stress on annual crop yields. Agricultural and Forest
    // Meteorology 135, 180 - 189.

    double vc_CropHeatImpact = 1.0 - ((vc_PhotoTemperature - pc_CriticalTemperatureHeatStress) /
                                      (pc_LimitingTemperatureHeatStress - pc_CriticalTemperatureHeatStress));

    if (vc_CropHeatImpact > 1.0) {
      vc_CropHeatImpact = 1.0;
    }

    if (vc_CropHeatImpact < 0.0) {
      vc_CropHeatImpact = 0.0;
    }

    // Fraction open flowers from Moriondo et al. (2011): Climate change impact
    // assessment: the role of climate extremes in crop yield simulation. Climatic
    // Change 104 (3-4), 679-701.

    vc_FractionOpenFlowers = 1.0 / (1.0 + ((1.0 / 0.015) - 1.0) * exp(-1.4 * vc_DaysAfterBeginFlowering));
    if (vc_DaysAfterBeginFlowering > 0) {
      vc_YesterdaysFractionOpenFlowers =
        1.0 / (1.0 + ((1.0 / 0.015) - 1.0) * exp(-1.4 * (vc_DaysAfterBeginFlowering - 1)));
    } else {
      vc_YesterdaysFractionOpenFlowers = 0.0;
    }
    double vc_DailyFloweringRate = vc_FractionOpenFlowers - vc_YesterdaysFractionOpenFlowers;

    // Total effect: Challinor et al. (2005): Simulation of the impact of high
    // temperature stress on annual crop yields. Agricultural and Forest
    // Meteorology 135, 180 - 189.
    vc_TotalCropHeatImpact += vc_CropHeatImpact * vc_DailyFloweringRate;

    vc_DaysAfterBeginFlowering += 1;
  }

  if (vc_CurrentTotalTemperatureSum >= pc_EndSensitivePhaseHeatStress || vc_FractionOpenFlowers > 0.999999) {
    if (vc_TotalCropHeatImpact < vc_CropHeatRedux) {
      vc_CropHeatRedux = vc_TotalCropHeatImpact;
    }
  }
}

/**
 * @brief Frost kill
 *
 * @param vw_MaxAirTemperature
 * @param vw_MinAirTemperature
 */

void CropModule::fc_FrostKill(double vw_MaxAirTemperature, double vw_MinAirTemperature) {
  // ************************************************************
  // ** Fowler, D.B., B.M. Byrns, K.J. Greer. 2014. Overwinter **
  // **	Low-Temperature Responses of Cereals: Analyses and   **
  // **	Simulation. Crop Sci. 54:2395–2405.          **
  // ************************************************************

  double vc_LT50old = vc_LT50;
  vc_LT50M = min(vc_LT50, vc_LT50M);

  double vc_NightTemperature = vw_MinAirTemperature + ((vw_MaxAirTemperature - vw_MinAirTemperature) / 4.0);
  double vc_CrownTemperature = vc_NightTemperature * 0.8;
  auto snowDepthAndTempUnderSnow = _getSnowDepthAndCalcTempUnderSnow(vc_CrownTemperature);
  if (vc_DevelopmentalStage <= 1) {
    vc_CrownTemperature =
      (3.0 * soilColumn->vt_SoilSurfaceTemperature + 2.0 * (*soilColumn)[0].vs_SoilTemperature) / 5.0;
  } else if (snowDepthAndTempUnderSnow.first > 0.0) {
    vc_CrownTemperature = snowDepthAndTempUnderSnow.second;
  }

  double vc_FrostHardening = 0.0;
  double vc_ThresholdInductionTemperature = 3.72135 - 0.401124 * pc_LT50cultivar;
  if ((vc_VernalisationFactor < 1.0) && (vc_CrownTemperature < vc_ThresholdInductionTemperature)) {
    vc_FrostHardening =
      pc_FrostHardening * (vc_ThresholdInductionTemperature - vc_CrownTemperature) * (vc_LT50old - pc_LT50cultivar);
  }

  double vc_FrostDehardening = 0.0;
  double vc_DoubleRidgeCounter = vc_CurrentTemperatureSum[1] / pc_StageTemperatureSum[1];
  double vc_VRTFactor = 1 / (1 + (exp(80.0 * (vc_DoubleRidgeCounter - 0.9))));
  if ((vc_DoubleRidgeCounter < 1.0 && vc_CrownTemperature >= vc_ThresholdInductionTemperature) ||
      vc_DoubleRidgeCounter >= 1.0) {
    vc_FrostDehardening = pc_FrostDehardening / (1.0 + exp(4.35 - 0.28 * vc_CrownTemperature));
  } else if (vc_DoubleRidgeCounter < 1.0 && -4.0 <= vc_CrownTemperature &&
             vc_CrownTemperature < vc_ThresholdInductionTemperature) {
    vc_FrostDehardening = (1 - vc_VRTFactor) * pc_FrostDehardening / (1.0 + exp(4.35 - 0.28 * vc_CrownTemperature));
  }

  // double vc_LowTemperatureExposure = 0.0;
  // if (vc_CrownTemperature < -3.0 && (vc_LT50M - vc_CrownTemperature) > -12.0)
  //{
  //	vc_LowTemperatureExposure = -(vc_LT50M - vc_CrownTemperature) /
  //		exp(-pc_LowTemperatureExposure * (vc_LT50M - vc_CrownTemperature) - 3.74);
  // }

  double vc_SnowDepthFactor = 1.0;
  if (soilColumn->vm_SnowDepth <= 125.0) vc_SnowDepthFactor = soilColumn->vm_SnowDepth / 125.0;

  double vc_RespirationFactor = (exp(0.84 + 0.051 * vc_CrownTemperature) - 2.0) / 1.85;
  double vc_RespiratoryStress = pc_RespiratoryStress * vc_RespirationFactor * vc_SnowDepthFactor;

  // vc_LT50 = vc_LT50old - vc_FrostHardening + vc_FrostDehardening + vc_LowTemperatureExposure + vc_RespiratoryStress;
  vc_LT50 = vc_LT50old - vc_FrostHardening + vc_FrostDehardening + vc_RespiratoryStress;
  // cout << "CrownT: " << vc_CrownTemperature
  //	<< " LT50: " << vc_LT50 << " LT50old: " << vc_LT50old << " LT50M: " << vc_LT50M << " LT50c: " << pc_LT50cultivar
  //	<< " FH: " << vc_FrostHardening << " FDH: " << vc_FrostDehardening
  //	/*<< " LTE: " << vc_LowTemperatureExposure*/ << " RS: " << vc_RespiratoryStress << endl;

  if (vc_LT50 > -3.0) vc_LT50 = -3.0;
  if (vc_CrownTemperature < vc_LT50) vc_CropFrostRedux *= 0.5;
}

/**
 * @brief Drought impact on crop fertility
 */
void CropModule::fc_DroughtImpactOnFertility() {
  if (vc_TranspirationDeficit < 0.0) vc_TranspirationDeficit = 0.0;

  // Fertility of the crop is reduced in cases of severe drought during bloom
  if ((vc_TranspirationDeficit < (pc_DroughtImpactOnFertilityFactor *
                                  pc_DroughtStressThreshold[vc_DevelopmentalStage])) &&
      (pc_AssimilatePartitioningCoeff[vc_DevelopmentalStage][vc_StorageOrgan] > 0.0)) {
    double vc_TranspirationDeficitHelper = vc_TranspirationDeficit /
                                           (pc_DroughtImpactOnFertilityFactor *
                                            pc_DroughtStressThreshold[vc_DevelopmentalStage]);

    if (vc_OxygenDeficit < 1.0) {
      vc_DroughtImpactOnFertility = 1.0;
    } else {
      vc_DroughtImpactOnFertility =
        1.0 - ((1.0 - vc_TranspirationDeficitHelper) * (1.0 - vc_TranspirationDeficitHelper));
    }
  } else {
    vc_DroughtImpactOnFertility = 1.0;
  }
}

/**
 * @brief Crop Nitrogen
 */
void CropModule::fc_CropNitrogen() {
  vc_CriticalNConcentration = pc_NConcentrationPN *
                              (1.0 + (pc_NConcentrationB0 *
                                      exp(-0.26 * (vc_AbovegroundBiomass + vc_BelowgroundBiomass) / 1000.0))) /
                              100.0; // [kg ha-1 -> t ha-1]
  vc_TargetNConcentration = vc_CriticalNConcentration * pc_LuxuryNCoeff;
  vc_NConcentrationAbovegroundBiomassOld = vc_NConcentrationAbovegroundBiomass;
  vc_NConcentrationRootOld = vc_NConcentrationRoot;

  if (vc_NConcentrationRoot < 0.01) {
    if (vc_NConcentrationRoot <= 0.005) {
      rootNRedux = 0.0;
    } else {
      // old WUX
      auto rootNReduxHelper = (vc_NConcentrationRoot - 0.005) / 0.005;
      rootNRedux = 1.0 - sqrt(1.0 - rootNReduxHelper * rootNReduxHelper);
    }
  } else {
    rootNRedux = 1.0;
  }

  if (vc_NConcentrationAbovegroundBiomass < vc_CriticalNConcentration) {
    if (vc_NConcentrationAbovegroundBiomass <= pc_MinimumNConcentration) {
      vc_CropNRedux = 0.0;
    } else {
      double cropNReduxHelper = (vc_NConcentrationAbovegroundBiomass - pc_MinimumNConcentration) /
                                (vc_CriticalNConcentration - pc_MinimumNConcentration);

      // New Monica approach
      vc_CropNRedux = 1.0 - exp(pc_MinimumNConcentration - (5.0 * cropNReduxHelper));

      // Original HERMES approach
      //vc_CropNRedux = (1.0 - exp(1.0 + 1.0 / (vc_CropNReduxHelper - 1.0))) *
      //  (1.0 - exp(1.0 + 1.0 / (vc_CropNReduxHelper - 1.0)));
    }
  } else {
    vc_CropNRedux = 1.0;
  }

  if (pc_NitrogenResponseOn == false) vc_CropNRedux = 1.0;
}

/**
 * @brief  Dry matter allocation within the crop
 *
 *  In this function the result from crop photosynthesis is allocated to the
 *  different crop organs under consideration of any stress factors
 *  (Water, Nitrogen, Temperature)
 *
 * @param vs_NumberOfLayers
 * @param vs_LayerThickness
 * @param pc_CropName
 * @param vc_DevelopmentalStage
 * @param vc_Assimilates
 * @param vc_NetMaintenanceRespiration
 * @param pc_CropSpecificMaxRootingDepth
 * @param vc_CropNRedux
 *
 * @author Claas Nendel
 */
void CropModule::fc_CropDryMatter(double vw_MeanAirTemperature) {
  assert(soilColumn->size() >= 0);
  auto nols = soilColumn->size();
  double layerThickness = soilColumn->at(0).vs_LayerThickness;

  double vc_MaxRootNConcentration = 0.0; // old WGM
  double vc_NConcentrationOptimum = 0.0; // old DTOPTN
  double vc_RootNIncrement = 0.0; // old WUMM
  double vc_AssimilatePartitioningCoeffOld = 0.0;
  double vc_AssimilatePartitioningCoeff = 0.0;
  // double vc_RootDistributionFactor          = 0.0; // old QREZ
  // double vc_SoilDepth                 = 0.0; // old TIEFE
  // std::vector<double> vc_RootLengthToLayer(nols, 0.0);	// old WULAE
  // std::vector<double> vc_RootLengthInLayer(nols, 0.0);	// old WULAE2
  //   std::vector<double> vc_CapillaryWater(nols, 0.0);
  // std::vector<double> vc_RootSurface(nols, 0.0); // old FL

  const CropModuleParameters& user_crops = *cropPs;
  double pc_MaxCropNDemand = user_crops.pc_MaxCropNDemand;

  // double pc_GrowthRespirationRedux = user_crops->getPc_GrowthRespirationRedux();
  //  Assuming that growth respiration takes 30% of total assimilation --> 0.7 [kg ha-1]
  // vc_NetPhotosynthesis = (vc_GrossPhotosynthesis - vc_NetMaintenanceRespiration + vc_ReserveAssimilatePool) * pc_GrowthRespirationRedux; // from HERMES algorithms
  vc_NetPhotosynthesis = vc_Assimilates; // from AGROSIM algorithms
  // double stage_mobil_from_storage_coeff = 0.3;
  double TMP_Regulatory_factor = speciesPs.pc_StageMobilFromStorageCoeff[vc_DevelopmentalStage];

  if (vc_DevelopmentalStage == 1) {
    TMP_Regulatory_factor = speciesPs.pc_StageMobilFromStorageCoeff[vc_DevelopmentalStage] * vc_KTkc;
  }

  double mobilization_from_storage =
    vc_OrganBiomass[vc_StorageOrgan] * speciesPs.pc_StageMobilFromStorageCoeff[vc_DevelopmentalStage] * vc_KTkc;

  vc_ReserveAssimilatePool = 0.0;

  vc_AbovegroundBiomassOld = vc_AbovegroundBiomass;
  vc_AbovegroundBiomass = 0.0;
  vc_BelowgroundBiomassOld = vc_BelowgroundBiomass;
  vc_BelowgroundBiomass = 0.0;
  vc_TotalBiomass = 0.0;

  // old PESUM [kg m-2 --> kg ha-1]
  // vc_TotalBiomassNContent += (soilColumn->vq_CropNUptake * 10000.0) + vc_FixedN;

  // Dry matter production
  // old NRKOM
  // double assimilate_partition_shoot = 0.7;
  double assimilate_partition_leaf = 0.05;
  //vector<double> dailyDeadBiomassIncrement(pc_NumberOfOrgans, 0.0);
  double dailyDeadRootBiomassIncrement = 0.0;
  for (int i_Organ = 0; i_Organ < pc_NumberOfOrgans; i_Organ++) {
    // Prevent out-of-bounds array access on developmental stage indices during early phases.
    // If vc_DevelopmentalStage is 0, we fall back to index 0 as the previous stage index.
    size_t prevStage = (vc_DevelopmentalStage > 0) ? (vc_DevelopmentalStage - 1) : 0;
    vc_AssimilatePartitioningCoeffOld = pc_AssimilatePartitioningCoeff[prevStage][i_Organ];
    vc_AssimilatePartitioningCoeff = pc_AssimilatePartitioningCoeff[vc_DevelopmentalStage][i_Organ];

    // Identify storage organ and reduce assimilate flux in case of heat stress //MP: this might be extended for drougth stress (see suggestions for altered assimilate allocations)
    if (pc_StorageOrgan[i_Organ]) {
      vc_AssimilatePartitioningCoeffOld =
        vc_AssimilatePartitioningCoeffOld * vc_CropHeatRedux * vc_DroughtImpactOnFertility;
      vc_AssimilatePartitioningCoeff = vc_AssimilatePartitioningCoeff * vc_CropHeatRedux * vc_DroughtImpactOnFertility;
    }

    if ((vc_CurrentTemperatureSum[vc_DevelopmentalStage] / pc_StageTemperatureSum[vc_DevelopmentalStage]) > 1) {
      // Pflanze ist ausgewachsen
      vc_OrganGrowthIncrement[i_Organ] = 0.0;
      vc_OrganSenescenceIncrement[i_Organ] = 0.0;
      if (pc_Perennial) vc_GrowthCycleEnded = true;
    } else {
      // test if there is a positive balance of produced assimilates
      // if vc_NetPhotosynthesis is negative, the crop needs more for
      // maintenance than for building new biomass
      if (vc_NetPhotosynthesis < 0.0) {
        // reduce biomass from leaf and shoot because of negative assimilate //MP: what is the effect on roots here?
        //! TODO: hard coded organ ids; must be more generalized because in database organ_ids can be mixed
        // vc_OrganBiomass[i_Organ];

        if (i_Organ == OId::LEAF) { // leaf

          double incr = assimilate_partition_leaf * vc_NetPhotosynthesis;
          if (fabs(incr) <= vc_OrganBiomass[i_Organ]) {
            debug() << "LEAF - Reducing organ biomass - default case ("
              << vc_OrganBiomass[i_Organ] + vc_OrganGrowthIncrement[i_Organ] << ")" << endl;
            vc_OrganGrowthIncrement[i_Organ] = incr;
          } else {
            // temporary hack because complex algorithm produces questionable results
            debug() << "LEAF - Not enough biomass for reduction - Reducing only what is available " << endl;
            vc_OrganGrowthIncrement[i_Organ] = (-1) * vc_OrganBiomass[i_Organ];
            //            debug() << "LEAF - Not enough biomass for reduction; Need to calculate new partition coefficient" << endl;
            //            // calculate new partition coefficient to detect, how much of organ biomass
            //            // can be reduced
            //            assimilate_partition_leaf = fabs(vc_OrganBiomass[i_Organ] / vc_NetPhotosynthesis);
            //            assimilate_partition_shoot = 1.0 - assimilate_partition_leaf;
            //            debug() << "LEAF - New Partition: " << assimilate_partition_leaf << endl;
            //
            //            // reduce biomass for leaf
            //            incr = assimilate_partition_leaf * vc_NetPhotosynthesis; // should be negative, therefor the addition
            //            vc_OrganGrowthIncrement[i_Organ] = incr;
            //            debug() << "LEAF - Reducing organ by " << incr << " (" << vc_OrganBiomass[i_Organ] + vc_OrganGrowthIncrement[i_Organ] << ")"<< endl;
          }
        } else if (i_Organ == OId::SHOOT) { // shoot

          double incr = assimilate_partition_leaf * vc_NetPhotosynthesis; // should be negative

          if (fabs(incr) <= vc_OrganBiomass[i_Organ]) {
            vc_OrganGrowthIncrement[i_Organ] = incr;
            debug() << "SHOOT - Reducing organ biomass - default case ("
              << vc_OrganBiomass[i_Organ] + vc_OrganGrowthIncrement[i_Organ] << ")" << endl;
          } else {
            // temporary hack because complex algorithm produces questionable results
            debug() << "SHOOT - Not enough biomass for reduction - Reducing only what is available " << endl;
            vc_OrganGrowthIncrement[i_Organ] = (-1) * vc_OrganBiomass[i_Organ];
            //            debug() << "SHOOT - Not enough biomass for reduction; Need to calculate new partition coefficient" << endl;
            //
            //            assimilate_partition_shoot = fabs(vc_OrganBiomass[i_Organ] / vc_NetPhotosynthesis);
            //            assimilate_partition_leaf = 1.0 - assimilate_partition_shoot;
            //            debug() << "SHOOT - New Partition: " << assimilate_partition_shoot << endl;
            //
            //            incr = assimilate_partition_shoot * vc_NetPhotosynthesis;
            //            vc_OrganGrowthIncrement[i_Organ] = incr;
            //            debug() << "SHOOT - Reducing organ (" << vc_OrganBiomass[i_Organ] + vc_OrganGrowthIncrement[i_Organ] << ")"<< endl;
            //
            //            // test if there is the possibility to reduce biomass of leaf
            //            // for remaining assimilates
            //            incr = assimilate_partition_leaf * vc_NetPhotosynthesis;
            //            double available_leaf_biomass = vc_OrganBiomass[LEAF] + vc_OrganGrowthIncrement[LEAF];
            //            if (incr<available_leaf_biomass) {
            //              // leaf biomass is big enough, so reduce biomass furthermore
            //              vc_OrganGrowthIncrement[LEAF] += incr; // should be negative, therefor the addition
            //              debug() << "LEAF - Reducing leaf biomasse further (" << vc_OrganBiomass[LEAF] + vc_OrganGrowthIncrement[LEAF] << ")"<< endl;
            //            } else {
            //              // worst case - there is not enough biomass available to reduce
            //              // maintenaince respiration requires more assimilates that can be
            //              // provided by plant itselft
            //              // now the plant is dying - sorry
            //              dyingOut = true;
            //            }
          }
        } else {
          // root or storage organ - do nothing in case of negative photosynthesis //MP: for roots, this appears to be questionable...
          vc_OrganGrowthIncrement[i_Organ] = 0;
        }
      } else { // if (vc_NetPhotosynthesis >= 0.0) {
        vc_OrganGrowthIncrement[i_Organ] = vc_NetPhotosynthesis *
                                           (vc_AssimilatePartitioningCoeffOld +
                                            ((vc_AssimilatePartitioningCoeff - vc_AssimilatePartitioningCoeffOld) *
                                             (vc_CurrentTemperatureSum[vc_DevelopmentalStage] /
                                              pc_StageTemperatureSum[vc_DevelopmentalStage]))) *
                                           vc_CropNRedux; // [kg CH2O ha-1]
        bool _mobilization_from_storage = true;

        if (_mobilization_from_storage) {
          if (i_Organ != vc_StorageOrgan) {
            vc_OrganGrowthIncrement[i_Organ] += mobilization_from_storage *
            (vc_AssimilatePartitioningCoeffOld +
             ((vc_AssimilatePartitioningCoeff - vc_AssimilatePartitioningCoeffOld) *
              (vc_CurrentTemperatureSum[vc_DevelopmentalStage] /
               pc_StageTemperatureSum[vc_DevelopmentalStage]))) * vc_CropNRedux;
          } else {
            vc_OrganGrowthIncrement[i_Organ] -= mobilization_from_storage * vc_CropNRedux;
            vc_OrganGrowthIncrement[i_Organ] += mobilization_from_storage *
            (vc_AssimilatePartitioningCoeffOld +
             ((vc_AssimilatePartitioningCoeff - vc_AssimilatePartitioningCoeffOld) *
              (vc_CurrentTemperatureSum[vc_DevelopmentalStage] /
               pc_StageTemperatureSum[vc_DevelopmentalStage]))) * vc_CropNRedux;
          }
        }
      }
      vc_OrganSenescenceIncrement[i_Organ] =
        vc_OrganGreenBiomass[i_Organ] * (pc_OrganSenescenceRate[prevStage][i_Organ] +
                                         ((pc_OrganSenescenceRate[vc_DevelopmentalStage][i_Organ] -
                                           pc_OrganSenescenceRate[prevStage][i_Organ]) *
                                          (vc_CurrentTemperatureSum[vc_DevelopmentalStage] /
                                           pc_StageTemperatureSum[vc_DevelopmentalStage]))); // [kg CH2O ha-1]
    }

    vc_OrganBiomass[i_Organ] += vc_OrganGrowthIncrement[i_Organ] * vc_TimeStep; // [kg CH2O ha-1]
    if (i_Organ == vc_StorageOrgan) {
      vc_OrganDeadBiomass[i_Organ] += vc_OrganSenescenceIncrement[i_Organ] * vc_TimeStep; // [kg CH2O ha-1]
    } else {
      // root, shoot, leaf
      const double reallocationRate = pc_AssimilateReallocation * vc_OrganSenescenceIncrement[i_Organ] * vc_TimeStep;
      // [kg CH2O ha-1]
      vc_OrganBiomass[vc_StorageOrgan] += reallocationRate;
      double dailyDeadBiomassIncrement = vc_OrganSenescenceIncrement[i_Organ] - reallocationRate;
      //v me
      //vc_OrganBiomass[i_Organ] -= reallocationRate;
      //vc_OrganDeadBiomass[i_Organ] += dailyDeadBiomassIncrement; // [kg CH2O ha-1]
      //^ me
      if (i_Organ == OId::ROOT) {
        vc_OrganBiomass[OId::ROOT] -= vc_OrganSenescenceIncrement[OId::ROOT];
        //v me
        //dailyDeadBiomassIncrement = vc_OrganSenescenceIncrement[i_Organ];
        //vc_TotalBiomassNContent -= dailyDeadBiomassIncrement * vc_NConcentrationRoot;
        //vc_OrganDeadBiomass[OId::ROOT] += dailyDeadBiomassIncrement;
        //dailyDeadRootBiomassIncrement = dailyDeadBiomassIncrement;
        //^ me
        vc_TotalBiomassNContent -= dailyDeadBiomassIncrement * vc_NConcentrationRoot;
        dailyDeadRootBiomassIncrement = dailyDeadBiomassIncrement;
      } else {
        // shoot or leaf
        vc_OrganBiomass[i_Organ] -= reallocationRate;
        vc_OrganDeadBiomass[i_Organ] += dailyDeadBiomassIncrement; // [kg CH2O ha-1]
      }
    }

    vc_OrganGreenBiomass[i_Organ] = vc_OrganBiomass[i_Organ] - vc_OrganDeadBiomass[i_Organ]; // [kg CH2O ha-1]
    if (vc_OrganGreenBiomass[i_Organ] < 0.0) {
      vc_OrganDeadBiomass[i_Organ] = vc_OrganBiomass[i_Organ];
      vc_OrganGreenBiomass[i_Organ] = 0.0;
    }

    if (pc_AbovegroundOrgan[i_Organ]) {
      vc_AbovegroundBiomass += vc_OrganBiomass[i_Organ]; // [kg CH2O ha-1]
    } else if (i_Organ != OId::ROOT) {
      vc_BelowgroundBiomass += vc_OrganBiomass[i_Organ];
    } // [kg CH2O ha-1]

    vc_TotalBiomass += vc_OrganBiomass[i_Organ]; // [kg CH2O ha-1]
  }

  /** @todo N redux noch ausgeschaltet */
  vc_ReserveAssimilatePool = 0.0; //+= vc_NetPhotosynthesis * (1.0 - vc_CropNRedux);
  vc_RootBiomassOld = vc_RootBiomass;
  vc_RootBiomass = vc_OrganBiomass[OId::ROOT]; // [kg CH2O ha-1]

  if (vc_DevelopmentalStage > 0) {
    vc_MaxRootNConcentration = pc_StageMaxRootNConcentration[vc_DevelopmentalStage - 1] -
                               (pc_StageMaxRootNConcentration[vc_DevelopmentalStage - 1] -
                                pc_StageMaxRootNConcentration[vc_DevelopmentalStage]) *
                               vc_CurrentTemperatureSum[vc_DevelopmentalStage] /
                               pc_StageTemperatureSum[vc_DevelopmentalStage]; //[kg kg-1]
  } else {
    vc_MaxRootNConcentration = pc_StageMaxRootNConcentration[vc_DevelopmentalStage];
  }

  vc_CropNDemand =
    ((vc_TargetNConcentration * vc_AbovegroundBiomass) + (vc_RootBiomass * vc_MaxRootNConcentration) +
     (vc_TargetNConcentration * vc_BelowgroundBiomass / pc_ResidueNRatio) - vc_TotalBiomassNContent) *
    vc_TimeStep; // [kg ha-1]

  vc_NConcentrationOptimum =
  ((vc_TargetNConcentration - (vc_TargetNConcentration - vc_CriticalNConcentration) * 0.15) *
   vc_AbovegroundBiomass +
   (vc_TargetNConcentration - (vc_TargetNConcentration - vc_CriticalNConcentration) * 0.15) *
   vc_BelowgroundBiomass / pc_ResidueNRatio + (vc_RootBiomass * vc_MaxRootNConcentration) -
   vc_TotalBiomassNContent) * vc_TimeStep; // [kg ha-1]

  if (vc_CropNDemand > (pc_MaxCropNDemand * vc_TimeStep)) {
    // Not more than 6kg N per day to be taken up.
    vc_CropNDemand = pc_MaxCropNDemand * vc_TimeStep;
  }

  if (vc_CropNDemand < 0) vc_CropNDemand = 0.0;

  if (vc_RootBiomass < vc_RootBiomassOld) {
    /** @todo: Claas: Macht die Bedingung hier Sinn? Hat sich die Wurzel wirklich zurückgebildet? */
    vc_RootNIncrement = (vc_RootBiomassOld - vc_RootBiomass) * vc_NConcentrationRoot;
  } else {
    vc_RootNIncrement = 0;
  }

  auto layerIndexBelowRootingDepth = std::min(vc_RootingDepth, nols - 1);
  double vc_AvailableWaterPercentage = 0.0;
  if (cropPs->__enable_PASW_root_penetration__) {
    // In case of drought stress the root will grow deeper //MP: Access point for drought optimisation (this could be changed for waterlogging)
    double vc_AvailableWater =
      (*soilColumn)[layerIndexBelowRootingDepth]._sps.vs_FieldCapacity - (*soilColumn)[layerIndexBelowRootingDepth]._sps.vs_PermanentWiltingPoint;
    vc_AvailableWaterPercentage =
      ((*soilColumn)[layerIndexBelowRootingDepth].vs_SoilMoisture_m3 - (*soilColumn)[layerIndexBelowRootingDepth]._sps.vs_PermanentWiltingPoint) /
      vc_AvailableWater;
    if (vc_AvailableWaterPercentage < 0.0) {
      vc_AvailableWaterPercentage = 0.0;
    }
  } else {
    //MP:turn this off first
    if (vc_TranspirationDeficit < (0.95 * pc_DroughtStressThreshold[vc_DevelopmentalStage])
        //MP: vielleicht ist das Problem, dass hier der selbe Wert verwendet wird
        && pc_CropSpecificMaxRootingDepth >= 0.8 // only if the crop specific max rooting depth is deeper than 80 cm
        && vc_RootingDepth_m > 0.95 * vc_MaxRootingDepth
        && vc_DevelopmentalStage < (pc_NumberOfDevelopmentalStages - 1)) {
      vc_MaxRootingDepth += 0.005;
    }
  }

  if (vc_MaxRootingDepth > (double(nols - 1) * layerThickness)) {
    vc_MaxRootingDepth = double(nols - 1) * layerThickness;
  }

  // restrict rootgrowth to everything above impentrable layer
  if (vs_ImpenetrableLayerDepth > 0) {
    vc_MaxRootingDepth = min(vc_MaxRootingDepth, vs_ImpenetrableLayerDepth);
  }

  // ***************************************************************************
  // *** Taken from Pedersen et al. 2010: Modelling diverse root density   *** //MP: the paper is actually from 2010
  // *** dynamics and deep nitrogen uptake - a simple approach.        ***
  // *** Plant & Soil 326, 493 - 510                     ***
  // ***************************************************************************

  // Determining temperature sum for root growth //MP: there is at least some effect of temperature on root growth (but soil temperature does not seem to be included so far)
  double pc_MaximumTemperatureRootGrowth = pc_MinimumTemperatureRootGrowth + 20.0;
  double vc_DailyTemperatureRoot = 0.0;
  if (vw_MeanAirTemperature >= pc_MaximumTemperatureRootGrowth) {
    vc_DailyTemperatureRoot = pc_MaximumTemperatureRootGrowth - pc_MinimumTemperatureRootGrowth;
  } else {
    vc_DailyTemperatureRoot = vw_MeanAirTemperature - pc_MinimumTemperatureRootGrowth;
  }
  if (vc_DailyTemperatureRoot < 0.0) {
    vc_DailyTemperatureRoot = 0.0;
  }
  vc_CurrentTotalTemperatureSumRoot += vc_DailyTemperatureRoot;

  // Determining root penetration rate according to soil clay content [m °C-1 d-1] //MP: this might be extended for soil temperature, bulk density, and available water content
  // This is not described in Pedersen et al. ... (source ??)
  double vc_RootPenetrationRate = 0.0; // [m °C-1 d-1]

  if ((*soilColumn)[layerIndexBelowRootingDepth]._sps.vs_SoilClayContent <= 0.02) {
    vc_RootPenetrationRate = 0.5 * pc_RootPenetrationRate;
  } else if ((*soilColumn)[layerIndexBelowRootingDepth]._sps.vs_SoilClayContent <= 0.08) {
    vc_RootPenetrationRate =
      ((1.0 / 3.0) + (0.5 / 0.06 * (*soilColumn)[layerIndexBelowRootingDepth]._sps.vs_SoilClayContent)) *
      pc_RootPenetrationRate; // [m °C-1 d-1]
  } else {
    vc_RootPenetrationRate = pc_RootPenetrationRate; // [m °C-1 d-1]
  }
  if (cropPs->__enable_PASW_root_penetration__) {
    //MP: add constraint for reduced root penetration rate when soil is dry
    if (vc_AvailableWaterPercentage <= 0.25) {
      vc_RootPenetrationRate = std::min(1.0, (4 * vc_AvailableWaterPercentage)) * vc_RootPenetrationRate;
      //MP: APSIM method according to Jones et al (1991)
    } else {
      vc_RootPenetrationRate = pc_RootPenetrationRate; // [m °C-1 d-1]
    }
  }

  // Calculating rooting depth [m]
  if (vc_CurrentTotalTemperatureSumRoot <= pc_RootGrowthLag) {
    vc_RootingDepth_m = pc_InitialRootingDepth; // [m]
  } else {
    // corrected because oscillating rooting depth at layer boundaries with texture change
    /* vc_RootingDepth_m = pc_InitialRootingDepth
           + ((vc_CurrentTotalTemperatureSumRoot - pc_RootGrowthLag)
           * vc_RootPenetrationRate); // [m] */

    vc_RootingDepth_m += (vc_DailyTemperatureRoot * vc_RootPenetrationRate); // [m]
  }

  if (vc_RootingDepth_m <= pc_InitialRootingDepth) vc_RootingDepth_m = pc_InitialRootingDepth;
  if (vc_RootingDepth_m > vc_MaxRootingDepth) vc_RootingDepth_m = vc_MaxRootingDepth; // [m]
  if (vc_RootingDepth_m > vs_MaxEffectiveRootingDepth) vc_RootingDepth_m = vs_MaxEffectiveRootingDepth;

  vc_RootingDepth = std::min(int(std::round(vc_RootingDepth_m / layerThickness)), int(nols)); // layer no
  vc_RootingZone = std::min(int(std::round(1.3 * vc_RootingDepth_m / layerThickness)), int(nols)); // layer no

  vc_TotalRootLength = vc_RootBiomass * pc_SpecificRootLength; //[m m-2]

  // Calculating a root density distribution factor []
  std::vector<double> vc_RootDensityFactor;
  double vc_RootDensityFactorSum = 0.0;
  tie(vc_RootDensityFactor, vc_RootDensityFactorSum) = calcRootDensityFactorAndSum();

  // calculate the distribution of dead root biomass (for later addition into AOM pools (in soil-organic))
  if (!cropPs->__disable_daily_root_biomass_to_soil__) {
    fc_MoveDeadRootBiomassToSoil(dailyDeadRootBiomassIncrement, vc_RootDensityFactorSum, vc_RootDensityFactor);
  }

  // Calculating root density per layer from total root length and
  // a relative root density distribution factor
  for (size_t i_Layer = 0; i_Layer < vc_RootingZone; i_Layer++) {
    vc_RootDensity[i_Layer] = (vc_RootDensityFactor[i_Layer] / vc_RootDensityFactorSum) * vc_TotalRootLength;
    // [m m-3]//MP: hier könnte man auch an einer layer-spezifischen density arbeiten
  }

  for (size_t i_Layer = 0; i_Layer < vc_RootingZone; i_Layer++) {
    // Root diameter [m]
    if (pc_AbovegroundOrgan[3]) {
      vc_RootDiameter[i_Layer] = 0.0002 - ((i_Layer + 1) * 0.00001); // [m]
    } else {
      vc_RootDiameter[i_Layer] = 0.0001;
    } //[m]

    // Default root decay - 10 %
    // vo_FreshSoilOrganicMatter[i_Layer] += vc_RootNIncrement
    //	* vc_RootDensity[i_Layer]
    //	* 10.0
    //	/ vc_TotalRootLength;
  }
  // *****************************************************************************

  // *** Original HERMES approach: *** //MP: why is this not used?
  //  // Taken from Gerwitz & Page --> Parameter for e-function indicating the
  //  // depth above which 68% of the roots are present
  //  if (pc_AbovegroundOrgan[3] == 0) {
  //  vc_RootDistributionFactor = pow((0.081476 + exp(-pc_RootDistributionParam * vc_CurrentTotalTemperatureSum)), 1.8);
  //  } else {
  //  vc_RootDistributionFactor = pow((0.081476 + exp(-pc_RootDistributionParam * (vc_CurrentTotalTemperatureSum
  //								 + vc_CurrentTemperatureSum[1]))), 1.8);
  //  }
  //
  //  if (vc_RootDistributionFactor > 1.00) { // HERMES: 0.35
  //  vc_RootDistributionFactor = 1.00;
  //  }
  //
  //  if (vc_RootDistributionFactor < (0.45 / (vc_MaxRootingDepth * (vs_LayerThickness * 100.0)))) {
  //  vc_RootDistributionFactor = 0.45 / (vc_MaxRootingDepth * (vs_LayerThickness * 100.0));
  //  }
  //
  //  // original HERMES:
  //  // vc_RootingDepth = int(floor(0.5 + (4.5 / vc_RootDistributionFactor / (vs_LayerThickness * 100.0)))); //[layer]
  //
  //  // new:
  //  vc_RootingDepth = int(floor((1.1 - vc_RootDistributionFactor) * (1.0 / vs_LayerThickness) * vc_MaxRootingDepth)); //[layer]
  //
  //
  //
  //
  //  // Assuming that root diameter [cm] decreases with depth
  //  for (int i_Layer = 0; i_Layer < vc_RootingDepth; i_Layer++) {
  //
  //  if (pc_AbovegroundOrgan[3] == 0) {
  //    vc_RootDiameter[i_Layer] = 0.0001; //[m]
  //  } else {
  //    vc_RootDiameter[i_Layer] = 0.0002 - ((i_Layer + 1) * 0.00001); // [m]
  //  }
  //  }
  //
  //
  //
  //  for (int i_Layer = 0; i_Layer < vc_RootingDepth; i_Layer++) {
  //
  //  vc_SoilDepth = (i_Layer + 1) * vs_LayerThickness; //[m]
  //
  //  // Root length down to layer i [m]
  //  vc_RootLengthToLayer[i_Layer] = (vc_RootBiomass * (1.0 - exp(-vc_RootDistributionFactor * vc_SoilDepth * 100.0))
  //			     / 100000.0 * 100.0 / 7.0) / 100.0;
  //
  //  if (i_Layer > 1) {
  //    vc_RootLengthInLayer[i_Layer] = fabs(vc_RootLengthToLayer[i_Layer] - vc_RootLengthToLayer[i_Layer - 1])
  //			    / (vc_RootDiameter[i_Layer] * vc_RootDiameter[i_Layer] * PI) / vs_LayerThickness;
  //    // [m m-3]
  //  } else {
  //    vc_RootLengthInLayer[i_Layer] = fabs(vc_RootLengthToLayer[i_Layer]) / (vc_RootDiameter[i_Layer]
  //							    * vc_RootDiameter[i_Layer] * PI) / vs_LayerThickness;
  //    // [m m-3]
  //  }
  //
  //  // Root density per volume soil [m m-3]
  //  vc_RootDensity[i_Layer] = vc_RootLengthInLayer[i_Layer];
  //
  //  // Root surface [m m-2]
  //  vc_RootSurface[i_Layer] = vc_RootDensity[i_Layer] * vc_RootDiameter[i_Layer] * 2.0 * PI;
  //  }
  //
  //  vc_TotalRootLength = 0;
  //  for (int i_Layer = 0; i_Layer < vc_RootingDepth; i_Layer++) {
  //
  //  // Total root length in [m m-2]
  //  vc_TotalRootLength += vc_RootDensity[i_Layer] * vs_LayerThickness;
  //  }
  //
  //  for (int i_Layer = 0; i_Layer < vc_RootingDepth; i_Layer++) {
  //  // default root decay - 10 %
  //  vo_FreshSoilOrganicMatter[i_Layer] += vc_RootNIncrement * vc_RootDensity[i_Layer] * 10.0 / vc_TotalRootLength;
  //  }
  //

  // Limiting the maximum N-uptake to 26-13*10^-14 mol/cm W./sec
  vc_MaxNUptake = pc_MaxNUptakeParam - (vc_CurrentTotalTemperatureSum / vc_TotalTemperatureSum); // [kg m Wurzel-1]

  if ((vc_CropNDemand / 10000.0) > (vc_TotalRootLength * vc_MaxNUptake * vc_TimeStep)) {
    vc_CropNDemand = vc_TotalRootLength * vc_MaxNUptake * vc_TimeStep; //[kg m-2]
  } else {
    vc_CropNDemand = vc_CropNDemand / 10000.0; // [kg ha-1 --> kg m-2]
  }
}

pair<vector<double>, double> CropModule::calcRootDensityFactorAndSum() {
  auto nols = soilColumn->size();
  double layerThickness = soilColumn->at(0).vs_LayerThickness;

  // Calculating a root density distribution factor []
  std::vector<double> vc_RootDensityFactor(nols, 0.0);
  for (size_t i_Layer = 0; i_Layer < nols; i_Layer++) {
    if (i_Layer < vc_RootingDepth) {
      vc_RootDensityFactor[i_Layer] = exp(-pc_RootFormFactor * (i_Layer * layerThickness));
      // [] //MP: hier könnte man was ändern, wenn man layer-spezifische densities haben will.
    } else if (i_Layer < vc_RootingZone) {
      vc_RootDensityFactor[i_Layer] = exp(-pc_RootFormFactor * (i_Layer * layerThickness)) *
                                      (1.0 - ((i_Layer - vc_RootingDepth) / (vc_RootingZone - vc_RootingDepth))); // []
    } else {
      vc_RootDensityFactor[i_Layer] = 0.0;
    } // []
  }

  // Summing up all factors to scale to a relative factor between [0;1]
  double vc_RootDensityFactorSum = 0.0;
  for (size_t i_Layer = 0; i_Layer < vc_RootingZone; i_Layer
       ++)
    vc_RootDensityFactorSum += vc_RootDensityFactor[i_Layer]; // []

  return make_pair(vc_RootDensityFactor, vc_RootDensityFactorSum);
}

/**
 * @brief Reference evapotranspiration
 *
 * A method following Penman-Monteith as described by the FAO in Allen
 * RG, Pereira LS, Raes D, Smith M. (1998) Crop evapotranspiration.
 * Guidelines for computing crop water requirements. FAO Irrigation and
 * Drainage Paper 56, FAO, Roma
 *
 * @param vs_HeightNN Height above sea level
 * @param vw_MaxAirTemperature Maximal air temperature for the calculated day
 * @param vw_MinAirTemperature Minimal air temperature for the calculated day
 * @param vw_RelativeHumidity Relative humidity
 * @param vw_MeanAirTemperature Mean air temperature
 * @param vw_WindSpeed Spped of wind
 * @param vw_WindSpeedHeight Height in which the wind speed has been measured *
 * @param vc_GlobalRadiation Global radiation
 * @param vw_AtmosphericCO2Concentration CO2 concentration in the athmosphere (needed for photosynthesis)
 * @param vc_GrossPhotosynthesisReference_mol under well watered conditions
 * @return Reference evapotranspiration
 */
double CropModule::fc_ReferenceEvapotranspiration(double vw_MaxAirTemperature,
                                                  double vw_MinAirTemperature,
                                                  double vw_RelativeHumidity,
                                                  double vw_MeanAirTemperature,
                                                  double vw_WindSpeed,
                                                  double vw_WindSpeedHeight,
                                                  double vw_AtmosphericCO2Concentration) {
  double vc_AtmosphericPressure; //[kPA]
  double vc_PsycrometerConstant; //[kPA °C-1]
  double vc_SaturatedVapourPressureMax; //[kPA]
  double vc_SaturatedVapourPressureMin; //[kPA]
  double vc_SaturatedVapourPressure; //[kPA]
  double vc_VapourPressure; //[kPA]
  double vc_SaturationDeficit; //[kPA]
  double vc_SaturatedVapourPressureSlope; //[kPA °C-1]
  double vc_WindSpeed_2m; //[m s-1]
  double vc_AerodynamicResistance; //[s m-1]
  double vc_SurfaceResistance; //[s m-1]
  double vc_ReferenceEvapotranspiration; //[mm]
  double vw_NetRadiation; //[MJ m-2]

  const CropModuleParameters& user_crops = *cropPs;
  double pc_SaturationBeta = user_crops.pc_SaturationBeta; // Original: Yu et al. 2001; beta = 3.5
  double pc_StomataConductanceAlpha = user_crops.pc_StomataConductanceAlpha; // Original: Yu et al. 2001; alpha = 0.06
  double pc_ReferenceAlbedo = user_crops.pc_ReferenceAlbedo; // FAO Green gras reference albedo from Allen et al. (1998)

  // Calculation of atmospheric pressure
  vc_AtmosphericPressure = 101.3 * pow(((293.0 - (0.0065 * vs_HeightNN)) / 293.0), 5.26);

  // Calculation of psychrometer constant - Luchtfeuchtigkeit
  vc_PsycrometerConstant = 0.000665 * vc_AtmosphericPressure;

  // Calc. of saturated water vapour pressure at daily max temperature
  vc_SaturatedVapourPressureMax = 0.6108 * exp((17.27 * vw_MaxAirTemperature) / (237.3 + vw_MaxAirTemperature));

  // Calc. of saturated water vapour pressure at daily min temperature
  vc_SaturatedVapourPressureMin = 0.6108 * exp((17.27 * vw_MinAirTemperature) / (237.3 + vw_MinAirTemperature));

  // Calculation of the saturated water vapour pressure
  vc_SaturatedVapourPressure = (vc_SaturatedVapourPressureMax + vc_SaturatedVapourPressureMin) / 2.0;

  // Calculation of the water vapour pressure
  if (vw_RelativeHumidity <= 0.0) {
    // Assuming Tdew = Tmin as suggested in FAO56 Allen et al. 1998
    vc_VapourPressure = vc_SaturatedVapourPressureMin;
  } else {
    vc_VapourPressure = vw_RelativeHumidity * vc_SaturatedVapourPressure;
  }

  // Calculation of the air saturation deficit
  vc_SaturationDeficit = vc_SaturatedVapourPressure - vc_VapourPressure;

  // Slope of saturation water vapour pressure-to-temperature relation
  vc_SaturatedVapourPressureSlope =
    (4098.0 * (0.6108 * exp((17.27 * vw_MeanAirTemperature) / (vw_MeanAirTemperature + 237.3)))) /
    ((vw_MeanAirTemperature + 237.3) * (vw_MeanAirTemperature + 237.3));

  // Calculation of wind speed in 2m height
  vc_WindSpeed_2m = max(0.5, vw_WindSpeed * (4.87 / (log(67.8 * vw_WindSpeedHeight - 5.42))));
  // 0.5 minimum allowed windspeed for Penman-Monteith-Method FAO

  // Calculation of the aerodynamic resistance
  vc_AerodynamicResistance = 208.0 / vc_WindSpeed_2m;

  if (vc_GrossPhotosynthesisReference_mol <= 0.0) {
    vc_StomataResistance = 999999.9; // [s m-1]
  } else {
    if (pc_CarboxylationPathway == 1) {
      vc_StomataResistance = // [s m-1]
        (vw_AtmosphericCO2Concentration * (1.0 + vc_SaturationDeficit / pc_SaturationBeta)) /
        (pc_StomataConductanceAlpha * vc_GrossPhotosynthesisReference_mol);
    } else {
      vc_StomataResistance = // [s m-1]
        (vw_AtmosphericCO2Concentration * (1.0 + vc_SaturationDeficit / pc_SaturationBeta)) /
        (pc_StomataConductanceAlpha * vc_GrossPhotosynthesisReference_mol);
    }
  }

  vc_SurfaceResistance = vc_StomataResistance / 1.44;

  // vc_SurfaceResistance = vc_StomataResistance / (vc_CropHeight * vc_LeafAreaIndex);

  // vw_NetRadiation = vc_GlobalRadiation * (1.0 - pc_ReferenceAlbedo); // [MJ m-2]

  double vc_ClearSkyShortwaveRadiation = (0.75 + 0.00002 * vs_HeightNN) * vc_ExtraterrestrialRadiation;

  double vc_RelativeShortwaveRadiation = vc_ClearSkyShortwaveRadiation > 0
                                           ? vc_GlobalRadiation / vc_ClearSkyShortwaveRadiation
                                           : 0;

  double vc_NetShortwaveRadiation = (1.0 - pc_ReferenceAlbedo) * vc_GlobalRadiation;

  double pc_BolzmanConstant = 0.0000000049; // Bolzmann constant 4.903 * 10-9 MJ m-2 K-4 d-1
  vw_NetRadiation = vc_NetShortwaveRadiation - (pc_BolzmanConstant * (pow((vw_MinAirTemperature + 273.16), 4.0) +
                                                                      pow((vw_MaxAirTemperature + 273.16), 4.0)) / 2.0 *
                                                (1.35 * vc_RelativeShortwaveRadiation - 0.35) *
                                                (0.34 - 0.14 * sqrt(vc_VapourPressure)));

  // Calculation of reference evapotranspiration
  // Penman-Monteith-Method FAO
  vc_ReferenceEvapotranspiration = ((0.408 * vc_SaturatedVapourPressureSlope * vw_NetRadiation) +
                                    (vc_PsycrometerConstant * (900.0 / (vw_MeanAirTemperature + 273.0)) *
                                     vc_WindSpeed_2m * vc_SaturationDeficit)) / (vc_SaturatedVapourPressureSlope +
                                     vc_PsycrometerConstant * (1.0 +
                                                               (vc_SurfaceResistance /
                                                                vc_AerodynamicResistance)));

  if (vc_ReferenceEvapotranspiration < 0.0) {
    vc_ReferenceEvapotranspiration = 0.0;
  }

  return vc_ReferenceEvapotranspiration;
}

/**
 * @brief  Water uptake by the crop
 *
 *  In this function the potential transpiration calcuated from potential
 *  evapotranspiration by soil cover fraction is reduced by water availability
 *  in the soil accoridng to actaul water contents, root distribution and
 *  root effectivity.
 *
 * @param vs_NumberOfLayers
 * @param vs_LayerThickness
 * @param vc_GroundwaterTable
 * @param vw_GrossPrecipitation
 *
 * @author Claas Nendel
 */
void CropModule::fc_CropWaterUptake(size_t vc_GroundwaterTable,
                                    double vw_GrossPrecipitation,
                                    double /*vc_CurrentTotalTemperatureSum*/,
                                    double /*vc_TotalTemperatureSum*/) {
  size_t nols = soilColumn->size();
  double layerThickness = soilColumn->at(0).vs_LayerThickness;
  vc_PotentialTranspirationDeficit = 0.0; // [mm]
  vc_PotentialTranspiration = 0.0; // old TRAMAX [mm]
  double vc_PotentialEvapotranspiration = 0.0; // [mm]
  vc_TranspirationReduced = 0.0; // old TDRED [mm]
  vc_ActualTranspiration = 0.0; // [mm]
  double vc_RemainingTotalRootEffectivity = 0.0; // old WEFFREST [m]
  double vc_CropWaterUptakeFromGroundwater = 0.0; // old GAUF [mm]
  double vc_TotalRootEffectivity = 0.0; // old WEFF [m]
  vc_ActualTranspirationDeficit = 0.0; // old TREST [mm]
  double vc_Interception = 0.0;
  vc_RemainingEvapotranspiration = 0.0;

  for (size_t i_Layer = 0; i_Layer < nols; i_Layer++) {
    vc_Transpiration[i_Layer] = 0.0; // old TP [mm]
    vc_TranspirationRedux[i_Layer] = 0.0; // old TRRED []
    vc_RootEffectivity[i_Layer] = 0.0; // old WUEFF [?]
  }

  // ################
  // # Interception #
  // ################

  double vc_InterceptionStorageOld = vc_InterceptionStorage;

  // Interception in [mm d-1];
  vc_Interception = (2.5 * vc_CropHeight * vc_SoilCoverage) - vc_InterceptionStorage;

  if (vc_Interception < 0) {
    vc_Interception = 0.0;
  }

  // If no precipitation occurs, vm_Interception = 0
  if (vw_GrossPrecipitation <= 0) {
    vc_Interception = 0.0;
  }

  // Calculating net precipitation and adding to surface water
  if (vw_GrossPrecipitation <= vc_Interception) {
    vc_Interception = vw_GrossPrecipitation;
    vc_NetPrecipitation = 0.0;
  } else {
    vc_NetPrecipitation = vw_GrossPrecipitation - vc_Interception;
  }

  // add intercepted precipitation to the virtual interception water storage
  vc_InterceptionStorage = vc_InterceptionStorageOld + vc_Interception;

  // #################
  // # Transpiration #
  // #################

  vc_PotentialEvapotranspiration = vc_ReferenceEvapotranspiration * vc_KcFactor; // [mm]

  // from HERMES:
  if (vc_PotentialEvapotranspiration > 6.5) {
    vc_PotentialEvapotranspiration = 6.5;
  }

  vc_RemainingEvapotranspiration = vc_PotentialEvapotranspiration; // [mm]

  // If crop holds intercepted water, first evaporation from crop surface
  if (vc_InterceptionStorage > 0.0) {
    if (vc_RemainingEvapotranspiration >= vc_InterceptionStorage) {
      vc_RemainingEvapotranspiration -= vc_InterceptionStorage;
      vc_EvaporatedFromIntercept = vc_InterceptionStorage;
      vc_InterceptionStorage = 0.0;
    } else {
      vc_InterceptionStorage -= vc_RemainingEvapotranspiration;
      vc_EvaporatedFromIntercept = vc_RemainingEvapotranspiration;
      vc_RemainingEvapotranspiration = 0.0;
    }
  } else {
    vc_EvaporatedFromIntercept = 0.0;
  }

  // if the plant has matured, no transpiration occurs!
  if (vc_DevelopmentalStage < vc_FinalDevelopmentalStage) {
    // if ((vc_CurrentTotalTemperatureSum / vc_TotalTemperatureSum) < 1.0){

    vc_PotentialTranspiration = vc_RemainingEvapotranspiration * vc_SoilCoverage; // [mm]

    for (size_t i_Layer = 0; i_Layer < vc_RootingZone; i_Layer++) {
      double vc_AvailableWater =
        (*soilColumn)[i_Layer]._sps.vs_FieldCapacity - (*soilColumn)[i_Layer]._sps.vs_PermanentWiltingPoint;
      double vc_AvailableWaterPercentage =
        ((*soilColumn)[i_Layer].vs_SoilMoisture_m3 - (*soilColumn)[i_Layer]._sps.vs_PermanentWiltingPoint) /
        vc_AvailableWater;
      if (vc_AvailableWaterPercentage < 0.0) {
        vc_AvailableWaterPercentage = 0.0;
      }
      //MP: Where do all these numbers come from? Potential need for improvement of the numbers
      //This would be the access point for considering compensatory effects of increased/decreased water uptake from layers that hold enough water.
      //An alternative approach for considering compensatory effects is to go through a soil water-dependent root penetration rate.
      if (vc_AvailableWaterPercentage < 0.15) {
        //MP: Access point for drought optimisation (this could be extended for waterlogging), this is for very dry condtions
        vc_TranspirationRedux[i_Layer] = vc_AvailableWaterPercentage * 3.0; // []
        vc_RootEffectivity[i_Layer] = 0.15 + 0.45 * vc_AvailableWaterPercentage / 0.15; // [] MP: this is essentially *3
      } else if (vc_AvailableWaterPercentage < 0.3) {
        vc_TranspirationRedux[i_Layer] = 0.45 + (0.25 * (vc_AvailableWaterPercentage - 0.15) / 0.15);
        vc_RootEffectivity[i_Layer] = 0.6 + (0.2 * (vc_AvailableWaterPercentage - 0.15) / 0.15);
      } else if (vc_AvailableWaterPercentage < 0.5) { //MP: ab hier hat das fast keinen Effekt mehr
        vc_TranspirationRedux[i_Layer] = 0.7 + (0.275 * (vc_AvailableWaterPercentage - 0.3) / 0.2);
        vc_RootEffectivity[i_Layer] = 0.8 + (0.2 * (vc_AvailableWaterPercentage - 0.3) / 0.2);
      } else if (vc_AvailableWaterPercentage < 0.75) { //MP: ab hier ist nur mehr die Transpiration betroffen
        vc_TranspirationRedux[i_Layer] = 0.975 + (0.025 * (vc_AvailableWaterPercentage - 0.5) / 0.25);
        vc_RootEffectivity[i_Layer] = 1.0;
      } else {
        vc_TranspirationRedux[i_Layer] = 1.0;
        vc_RootEffectivity[i_Layer] = 1.0;
      }
      if (vc_TranspirationRedux[i_Layer] < 0) {
        vc_TranspirationRedux[i_Layer] = 0.0;
      }
      if (vc_RootEffectivity[i_Layer] < 0) {
        vc_RootEffectivity[i_Layer] = 0.0;
      }
      if (i_Layer == vc_GroundwaterTable) { // old GRW
        vc_RootEffectivity[i_Layer] = 0.5;
      }
      if (i_Layer > vc_GroundwaterTable) { // old GRW
        vc_RootEffectivity[i_Layer] = 0.0;
      }
      if (((i_Layer + 1) * layerThickness) >= vs_MaxEffectiveRootingDepth) {
        vc_RootEffectivity[i_Layer] = 0.0;
      }

      vc_TotalRootEffectivity += vc_RootEffectivity[i_Layer] * vc_RootDensity[i_Layer]; //[m m-3]
      vc_RemainingTotalRootEffectivity = vc_TotalRootEffectivity;
    }

    // [TRANSPLANT SHOCK] Water Uptake Limitation.
    // Limits the total active root water uptake effectivity proportional to the shock recovery efficiency factor.
    if (vc_TransplantEfficiency < 1.0) {
      vc_TotalRootEffectivity *= vc_TransplantEfficiency;
      vc_RemainingTotalRootEffectivity = vc_TotalRootEffectivity;
    }

    // std::cout << setprecision(11) << "vc_TotalRootEffectivity: " << vc_TotalRootEffectivity << std::endl;
    // std::cout << setprecision(11) << "vc_OxygenDeficit: " << vc_OxygenDeficit << std::endl;

    for (size_t i_Layer = 0; i_Layer < nols; i_Layer++) {
      if (i_Layer > min(vc_RootingZone, vc_GroundwaterTable + 1)) {
        vc_Transpiration[i_Layer] = 0.0; //[mm]
      } else {
        vc_Transpiration[i_Layer] = vc_TotalRootEffectivity != 0.0
                                      ? vc_PotentialTranspiration *
                                        ((vc_RootEffectivity[i_Layer] * vc_RootDensity[i_Layer]) /
                                         vc_TotalRootEffectivity) * vc_OxygenDeficit
                                      //MP: why is this not changing anything? (I think it would only change something for too dry conditions).
                                      : 0;

        // std::cout << setprecision(11) << "vc_Transpiration[i_Layer]: " << i_Layer << ", " << vc_Transpiration[i_Layer] << std::endl;
        // std::cout << setprecision(11) << "vc_RootEffectivity[i_Layer]: " << i_Layer << ", " << vc_RootEffectivity[i_Layer] << std::endl;
        // std::cout << setprecision(11) << "vc_RootDensity[i_Layer]: " << i_Layer << ", " << vc_RootDensity[i_Layer] << std::endl;

        // [mm]
      }
    }

    for (size_t i_Layer = 0; i_Layer < min(vc_RootingZone, vc_GroundwaterTable + 1); i_Layer++) {
      vc_RemainingTotalRootEffectivity -= vc_RootEffectivity[i_Layer] * vc_RootDensity[i_Layer]; // [m m-3]

      if (vc_RemainingTotalRootEffectivity <= 0.0) {
        vc_RemainingTotalRootEffectivity = 0.00001;
      }
      if (((vc_Transpiration[i_Layer] / 1000.0) / layerThickness) >
          (((*soilColumn)[i_Layer].vs_SoilMoisture_m3 - (*soilColumn)[i_Layer]._sps.vs_PermanentWiltingPoint))) {
        vc_PotentialTranspirationDeficit = (((vc_Transpiration[i_Layer] / 1000.0) / layerThickness) -
                                            ((*soilColumn)[i_Layer].vs_SoilMoisture_m3 -
                                             (*soilColumn)[i_Layer]._sps.vs_PermanentWiltingPoint)) * layerThickness *
                                           1000.0; // [mm]
        if (vc_PotentialTranspirationDeficit < 0.0) {
          vc_PotentialTranspirationDeficit = 0.0;
        }
        if (vc_PotentialTranspirationDeficit > vc_Transpiration[i_Layer]) {
          vc_PotentialTranspirationDeficit = vc_Transpiration[i_Layer]; //[mm]
        }
      } else {
        vc_PotentialTranspirationDeficit = 0.0;
      }
      vc_TranspirationReduced = vc_Transpiration[i_Layer] * (1.0 - vc_TranspirationRedux[i_Layer]);

      //! @todo Claas: How can we lower the groundwater table if crop water uptake is restricted in that layer?
      vc_ActualTranspirationDeficit = max(vc_TranspirationReduced, vc_PotentialTranspirationDeficit); //[mm]
      if (vc_ActualTranspirationDeficit > 0.0) {
        if (i_Layer < min(vc_RootingZone, vc_GroundwaterTable + 1)) {
          for (size_t i_Layer2 = i_Layer + 1; i_Layer2 < min(vc_RootingZone, vc_GroundwaterTable + 1); i_Layer2++) {
            vc_Transpiration[i_Layer2] += vc_ActualTranspirationDeficit *
            (vc_RootEffectivity[i_Layer2] * vc_RootDensity[i_Layer2] /
             vc_RemainingTotalRootEffectivity);
          }
        }
      }
      vc_Transpiration[i_Layer] = vc_Transpiration[i_Layer] - vc_ActualTranspirationDeficit;
      //MP: this is a key line for water stress response
      if (vc_Transpiration[i_Layer] < 0.0) {
        vc_Transpiration[i_Layer] = 0.0;
      }
      vc_ActualTranspiration += vc_Transpiration[i_Layer];
      if (i_Layer == vc_GroundwaterTable) {
        vc_CropWaterUptakeFromGroundwater = (vc_Transpiration[i_Layer] / 1000.0) / layerThickness; //[m3 m-3]
      }
    }
    if (vc_PotentialTranspiration >
        0) { vc_TranspirationDeficit = vc_ActualTranspiration / vc_PotentialTranspiration; } else {
      vc_TranspirationDeficit = 1.0;
    }

    int vm_GroundwaterDistance = (int)vc_GroundwaterTable - (int)vc_RootingDepth;
    // std::cout << "vm_GroundwaterDistance: " << vm_GroundwaterDistance << std::endl;
    if (vm_GroundwaterDistance <= 1) vc_TranspirationDeficit = 1.0;
    if (!pc_WaterDeficitResponseOn) vc_TranspirationDeficit = 1.0;
  }
}

/**
 * @brief Nitrogen uptake by the crop
 *
 * @param  vs_NumberOfLayers Number of soil layers
 * @param  vc_GroundwaterTable Depth of groundwater table
 *
 * @author Claas Nendel
 */
void CropModule::fc_CropNUptake(size_t vc_GroundwaterTable,
                                double /*vc_CurrentTotalTemperatureSum*/,
                                double /*vc_TotalTemperatureSum*/) {
  auto nols = soilColumn->size();
  double layerThickness = soilColumn->at(0).vs_LayerThickness;

  double vc_ConvectiveNUptake = 0.0; // old TRNSUM
  double vc_DiffusiveNUptake = 0.0; // old SUMDIFF
  std::vector<double> vc_ConvectiveNUptakeFromLayer(nols, 0.0); // old MASS

  std::vector<double> vc_DiffusionCoeff(nols, 0.0); // old D
  std::vector<double> vc_DiffusiveNUptakeFromLayer(nols, 0.0); // old DIFF
  double vc_ConvectiveNUptake_1 = 0.0; // old MASSUM
  double vc_DiffusiveNUptake_1 = 0.0; // old DIFFSUM
  double pc_MinimumAvailableN = cropPs->pc_MinimumAvailableN; // kg m-3
  double pc_MinimumNConcentrationRoot = cropPs->pc_MinimumNConcentrationRoot; // kg kg-1
  double pc_MaxCropNDemand = cropPs->pc_MaxCropNDemand;

  vc_TotalNUptake = 0.0;
  vc_TotalNInput = 0.0;
  vc_FixedN = 0.0;
  for (auto& v : vc_NUptakeFromLayer) v = 0.0;

  // if the plant has matured, no N uptake occurs!
  if (vc_DevelopmentalStage < vc_FinalDevelopmentalStage) {
    // if ((vc_CurrentTotalTemperatureSum / vc_TotalTemperatureSum) < 1.0){

    for (int i_Layer = 0; i_Layer < (min(vc_RootingZone, vc_GroundwaterTable)); i_Layer++) {
      vs_SoilMineralNContent[i_Layer] = (*soilColumn)[i_Layer].vs_SoilNO3; // [kg m-3]

      // Convective N uptake per layer
      vc_ConvectiveNUptakeFromLayer[i_Layer] = (vc_Transpiration[i_Layer] / 1000.0) * //[mm --> m]
                                               (vs_SoilMineralNContent[i_Layer] / // [kg m-3]
                                                ((*soilColumn)[i_Layer].vs_SoilMoisture_m3)) * // old WG [m3 m-3]
                                               vc_TimeStep; // -->[kg m-2]

      vc_ConvectiveNUptake += vc_ConvectiveNUptakeFromLayer[i_Layer]; // [kg m-2]

      /** @todo Claas: Woher kommt der Wert für vs_Tortuosity? */
      /** @todo Claas: Prüfen ob Umstellung auf [m] die folgenden Gleichungen beeinflusst */
      vc_DiffusionCoeff[i_Layer] = 0.000214 * (vs_Tortuosity * exp((*soilColumn)[i_Layer].vs_SoilMoisture_m3 * 10)) /
                                   (*soilColumn)[i_Layer].vs_SoilMoisture_m3; //[m2 d-1]

      vc_DiffusiveNUptakeFromLayer[i_Layer] = (vc_DiffusionCoeff[i_Layer] * // [m2 d-1]
                                               (*soilColumn)[i_Layer].vs_SoilMoisture_m3 * // [m3 m-3]
                                               2.0 * PI * vc_RootDiameter[i_Layer] * // [m]
                                               (vs_SoilMineralNContent[i_Layer] / 1000.0 / // [kg m-3]
                                                (*soilColumn)[i_Layer].vs_SoilMoisture_m3 -
                                                0.000014) * // [m3 m-3]
                                               sqrt(PI * vc_RootDensity[i_Layer])) * // [m m-3]
                                              vc_RootDensity[i_Layer] *
                                              1000.0 * vc_TimeStep; // -->[kg m-2]

      if (vc_DiffusiveNUptakeFromLayer[i_Layer] < 0.0) {
        vc_DiffusiveNUptakeFromLayer[i_Layer] = 0;
      }

      vc_DiffusiveNUptake += vc_DiffusiveNUptakeFromLayer[i_Layer]; // [kg m-2]
    }

    for (int i_Layer = 0; i_Layer < (min(vc_RootingZone, vc_GroundwaterTable)); i_Layer++) {
      if (vc_CropNDemand > 0.0) {
        if (vc_ConvectiveNUptake >= vc_CropNDemand) {
          // convective N uptake is sufficient
          vc_NUptakeFromLayer[i_Layer] = vc_CropNDemand * vc_ConvectiveNUptakeFromLayer[i_Layer] / vc_ConvectiveNUptake;
        } else {
          // N demand is not covered
          if ((vc_CropNDemand - vc_ConvectiveNUptake) < vc_DiffusiveNUptake) {
            vc_NUptakeFromLayer[i_Layer] = vc_ConvectiveNUptakeFromLayer[i_Layer] +
                                           ((vc_CropNDemand - vc_ConvectiveNUptake) *
                                            vc_DiffusiveNUptakeFromLayer[i_Layer] / vc_DiffusiveNUptake);
          } else {
            vc_NUptakeFromLayer[i_Layer] =
              vc_ConvectiveNUptakeFromLayer[i_Layer] + vc_DiffusiveNUptakeFromLayer[i_Layer];
          }
        }

        vc_ConvectiveNUptake_1 += vc_ConvectiveNUptakeFromLayer[i_Layer];
        vc_DiffusiveNUptake_1 += vc_DiffusiveNUptakeFromLayer[i_Layer];

        if (vc_NUptakeFromLayer[i_Layer] >
            ((vs_SoilMineralNContent[i_Layer] * layerThickness) - pc_MinimumAvailableN)) {
          vc_NUptakeFromLayer[i_Layer] = (vs_SoilMineralNContent[i_Layer] * layerThickness) - pc_MinimumAvailableN;
        }

        if (vc_NUptakeFromLayer[i_Layer] > (pc_MaxCropNDemand / 10000.0 * 0.75)) {
          vc_NUptakeFromLayer[i_Layer] = (pc_MaxCropNDemand / 10000.0 * 0.75);
        }

        if (vc_NUptakeFromLayer[i_Layer] < 0.0) {
          vc_NUptakeFromLayer[i_Layer] = 0.0;
        }
      } else {
        vc_NUptakeFromLayer[i_Layer] = 0.0;
      }

      vc_TotalNUptake += vc_NUptakeFromLayer[i_Layer] * 10000.0; //[kg m-2] --> [kg ha-1]
    } // for

    // Biological N Fixation
    vc_FixedN = pc_PartBiologicalNFixation * vc_CropNDemand * 10000.0; // [kg N ha-1]
    // Part of the deficit which can be covered by biological N fixation.

    if (((vc_CropNDemand * 10000.0) - vc_TotalNUptake) < vc_FixedN) {
      vc_TotalNInput = vc_CropNDemand * 10000.0;
      vc_FixedN = (vc_CropNDemand * 10000.0) - vc_TotalNUptake;
    } else {
      vc_TotalNInput = vc_TotalNUptake + vc_FixedN;
    }
  } // if

  vc_SumTotalNUptake += vc_TotalNUptake;
  vc_TotalBiomassNContent += vc_TotalNInput;

  if (vc_RootBiomass > vc_RootBiomassOld) {
    // root has been growing
    vc_NConcentrationRoot =
      ((vc_RootBiomassOld * vc_NConcentrationRoot)
       + ((vc_RootBiomass - vc_RootBiomassOld)
          / (vc_AbovegroundBiomass - vc_AbovegroundBiomassOld +
             vc_BelowgroundBiomass - vc_BelowgroundBiomassOld +
             vc_RootBiomass - vc_RootBiomassOld)
          * vc_TotalNInput))
      / vc_RootBiomass;

    vc_NConcentrationRoot = bound(pc_MinimumNConcentrationRoot,
                                  vc_NConcentrationRoot,
                                  pc_StageMaxRootNConcentration[vc_DevelopmentalStage]);
  }

  vc_NConcentrationAbovegroundBiomass =
    (vc_TotalBiomassNContent - (vc_RootBiomass * vc_NConcentrationRoot))
    / (vc_AbovegroundBiomass + (vc_BelowgroundBiomass / pc_ResidueNRatio));

  if ((vc_NConcentrationAbovegroundBiomass * vc_AbovegroundBiomass) <
      (vc_NConcentrationAbovegroundBiomassOld * vc_AbovegroundBiomassOld)) {
    double tempNConcentrationAbovegroundBiomass =
      vc_NConcentrationAbovegroundBiomassOld * vc_AbovegroundBiomassOld / vc_AbovegroundBiomass;

    double tempNConcentrationRoot =
      (vc_TotalBiomassNContent
       - (vc_NConcentrationAbovegroundBiomass * vc_AbovegroundBiomass)
       - (vc_NConcentrationAbovegroundBiomass * vc_BelowgroundBiomass / pc_ResidueNRatio))
      / vc_RootBiomass;

    if (tempNConcentrationRoot >= pc_MinimumNConcentrationRoot) {
      vc_NConcentrationAbovegroundBiomass = tempNConcentrationAbovegroundBiomass;
      vc_NConcentrationRoot = tempNConcentrationRoot;
    }
  }
}

/**
 * @brief Calculation of gross primary production [kg C ha-1 d-1]
 *
 * @param vc_LeafAreaIndex
 * @param vc_Assimilates
 * @return daily gross primary production per hectare
 *
 * @author Claas Nendel
 */
double CropModule::fc_GrossPrimaryProduction() {
  double vc_GPP = 0.0;
  // Converting photosynthesis rate from [kg CH2O ha-1 d-1] back to
  // [kg C ha-1 d-1]
  vc_GPP = vc_GrossAssimilates / 30.0 * 12.0;
  return vc_GPP;
}

/**
 * @brief Calculation of net primary production [kg C ha-1 d-1]
 *
 * @param vc_GrossPrimaryProduction
 * @param vc_TotalRespired
 * @param vc_LeafAreaIndex
 * @return daily net primary production per hectare
 *
 * @author Claas Nendel
 */
double CropModule::fc_NetPrimaryProduction(double vc_TotalRespired) {
  double vc_NPP = 0.0;
  // Convert [kg CH2O ha-1 d-1] to [kg C ha-1 d-1]
  vc_Respiration = vc_TotalRespired / 30.0 * 12.0;

  vc_NPP = vc_GrossPrimaryProduction - vc_Respiration;
  return vc_NPP;
}

/**
 * @brief Returns crop name [ ]
 * @return crop name
 */
std::string CropModule::get_CropName() const {
  return pc_CropName;
}

/**
 * @brief Returns gross photosynthesis rate [mol m-2 s-1]
 * @return photosynthesis rate
 */
double CropModule::get_GrossPhotosynthesisRate() const {
  return vc_GrossPhotosynthesis_mol;
}

/**
 * @brief Returns gross photosynthesis rate [kg ha-1]
 * @return photosynthesis rate
 */
double CropModule::get_GrossPhotosynthesisHaRate() const {
  return vc_GrossPhotosynthesis;
}

/**
 * @brief Returns assimilation rate [kg CO2 ha leaf-1]
 * @return Assimilation rate
 */
double CropModule::get_AssimilationRate() const {
  return vc_AssimilationRate;
}

/**
 * @brief Returns assimilates [kg CO2 ha-1]
 * @return Assimilates
 */
double CropModule::get_Assimilates() const {
  return vc_Assimilates;
}

/**
 * @brief Returns net maintenance respiration rate [kg CO2 ha-1]
 * @return Net maintenance respiration rate
 */
double CropModule::get_NetMaintenanceRespiration() const {
  return vc_NetMaintenanceRespiration;
}

/**
 * @brief Returns maintenance respiration rate from AGROSIM [kg CO2 ha-1]
 * @return Maintenance respiration rate
 */
double CropModule::get_MaintenanceRespirationAS() const {
  return vc_MaintenanceRespirationAS;
}

/**
 * @brief Returns growth respiration rate from AGROSIM [kg CO2 ha-1]
 * @return GRowth respiration rate
 */
double CropModule::get_GrowthRespirationAS() const {
  return vc_GrowthRespirationAS;
}

/**
 */
double CropModule::get_VernalisationFactor() const {
  return vc_VernalisationFactor;
}

/**
 */
double CropModule::get_DaylengthFactor() const {
  return vc_DaylengthFactor;
}

/**
 * @brief Returns growth increment of organ i [kg CH2O ha-1 d-1]
 * @return Organ growth increment
 */
double CropModule::get_OrganGrowthIncrement(int i_Organ) const {
  return vc_OrganGrowthIncrement[i_Organ];
}

/**
 * @brief Returns net photosynthesis [kg CH2O ha-1]
 * @return net photosynthesis
 */
double CropModule::get_NetPhotosynthesis() const {
  return vc_NetPhotosynthesis;
}

void CropModule::calculateVOCEmissions(const Voc::MicroClimateData& mcd) {
  Voc::SpeciesData species;
  // species.id = 0; // right now we just have one crop at a time, so no need to distinguish multiple crops
  species.lai = get_LeafAreaIndex();
  species.mFol = get_OrganBiomass(OId::LEAF) / (100. * 100.); // kg/ha -> kg/m2
  species.sla = pc_SpecificLeafArea[vc_DevelopmentalStage] * 100. * 100.; // ha/kg -> m2/kg

  species.EF_MONO = speciesPs.EF_MONO;
  species.EF_MONOS = speciesPs.EF_MONOS;
  species.EF_ISO = speciesPs.EF_ISO;
  species.VCMAX25 = speciesPs.VCMAX25;
  species.AEKC = speciesPs.AEKC;
  species.AEKO = speciesPs.AEKO;
  species.AEVC = speciesPs.AEVC;
  species.KC25 = speciesPs.KC25;

  _guentherEmissions = Voc::calculateGuentherVOCEmissions(species, mcd);
  // debug() << "guenther: isoprene: " << gems.isoprene_emission << " monoterpene: " << gems.monoterpene_emission << endl;

  _jjvEmissions = Voc::calculateJJVVOCEmissions(species, mcd, _cropPhotosynthesisResults);
  // debug() << "jjv: isoprene: " << jjvems.isoprene_emission << " monoterpene: " << jjvems.monoterpene_emission << endl;
}

/**
 * @brief Returns reference evapotranspiration [mm]
 * @return Reference evapotranspiration
 */
double CropModule::get_ReferenceEvapotranspiration() const {
  return vc_ReferenceEvapotranspiration;
}

/**
 * @brief Returns evapotranspiration remaining after evaporation of intercepted water [mm]
 * @return Remaning evapotranspirationn
 */
double CropModule::get_RemainingEvapotranspiration() const {
  return vc_RemainingEvapotranspiration;
}

/**
 * @brief Returns evaporation from intercepted water [mm]
 * @return evaporated from intercept
 */
double CropModule::get_EvaporatedFromIntercept() const {
  return vc_EvaporatedFromIntercept;
}

/**
 * @brief Returns precipitation after interception on crop surface [mm]
 * @return Remaning net precipitation
 */
double CropModule::get_NetPrecipitation() const {
  return vc_NetPrecipitation;
}

/**
 * @brief Returns leaf area index [m2 m-2]
 * @return Leaf area index
 */
double CropModule::get_LeafAreaIndex() const {
  return vc_LeafAreaIndex;
}

/**
 * @brief Returns crop height [m]
 * @return crop height
 */
double CropModule::get_CropHeight() const {
  return vc_CropHeight;
}

/**
 * @brief Returns rooting depth [layer]
 * @return rooting depth
 */
size_t CropModule::get_RootingDepth() const {
  return vc_RootingDepth;
}

/**
 * @brief Returns soil coverage [0;1]
 * @return soil coverage
 */
double CropModule::get_SoilCoverage() const {
  return vc_SoilCoverage;
}

/**
 * @brief Returns current Kc factor []
 * @return Kc factor
 */
double CropModule::get_KcFactor() const {
  return vc_KcFactor;
}

/**
 * @brief Returns the FAO-56 Dual Kc basal crop coefficient Kcb [-]
 * @return Kcb factor (interpolated per developmental stage, analogous to get_KcFactor)
 */
double CropModule::get_KcbFactor() const {
  return vc_KcbFactor;
}

/**
 * @brief Returns Stomata resistance [s m-1]
 * @return Stomata resistance
 */
double CropModule::get_StomataResistance() const {
  return vc_StomataResistance;
}

/**
 * @brief Returns transpiration per layer[mm]
 * @return transpiration per layer
 */
double CropModule::get_PotentialTranspiration() const {
  return vc_PotentialTranspiration;
}

/**
 * @brief Returns transpiration per layer[mm]
 * @return transpiration per layer
 */
double CropModule::get_ActualTranspiration() const {
  return vc_ActualTranspiration;
}

/**
 * @brief Returns transpiration per layer[mm]
 * @return transpiration per layer
 */
double CropModule::get_Transpiration(int i_Layer) const {
  return vc_Transpiration[i_Layer];
}

/**
 * @brief Returns transpiration deficit [0;1]
 * @return transpiration deficit
 */
double CropModule::get_TranspirationDeficit() const {
  return vc_TranspirationDeficit;
}

/**
 * @brief Returns oxygen deficit [0;1]
 * @return oxygen deficit
 */
double CropModule::get_OxygenDeficit() const {
  return vc_OxygenDeficit;
}

/**
 * @brief Returns Nitrogen deficit [0;1]
 * @return nitrogen deficit
 */
double CropModule::get_CropNRedux() const {
  return vc_CropNRedux;
}

/**
 * @brief Returns Heat stress reductor [0;1]
 * @return heat stress reductor
 */
double CropModule::get_HeatStressRedux() const {
  return vc_CropHeatRedux;
}

double CropModule::get_FrostStressRedux() const {
  return vc_CropFrostRedux;
}

/**
 * @brief Returns current total temperature sum [°Cd]
 * @return Current temperature sum
 */
double CropModule::get_CurrentTemperatureSum() const {
  return vc_CurrentTotalTemperatureSum;
}

/**
 * @brief Returns developmental stage[]
 * @return deveklopmental stage
 */
size_t CropModule::get_DevelopmentalStage() const {
  return vc_DevelopmentalStage;
}

/**
 * @brief Returns Relative total development []
 * @return Relative total development
 */
double CropModule::get_RelativeTotalDevelopment() const {
  return vc_RelativeTotalDevelopment;
}

/**
 * @brief Returns total number of organs[]
 * @return total number of organs
 */
int CropModule::get_NumberOfOrgans() const {
  return pc_NumberOfOrgans;
}

/**
 * @brief Returns current biomass of organ i [kg ha-1]
 * @return organ biomass
 */
double CropModule::get_OrganBiomass(int i_Organ) const {
  return vc_OrganBiomass[i_Organ];
}

/**
 * @brief Returns current green biomass of organ i [kg ha-1]
 * @return organ biomass
 */
double CropModule::get_OrganGreenBiomass(int i_Organ) const {
  return vc_OrganGreenBiomass[i_Organ];
}

/**
 * @brief Returns aboveground biomass [kg ha-1]
 * @return organ biomass
 */
double CropModule::get_AbovegroundBiomass() const {
  return vc_AbovegroundBiomass;
}

/**
 * @brief Returns crop's lethal temperature LT50 [°C]
 * @return LT50
 */
double CropModule::get_LT50() const {
  return vc_LT50;
}

/**
 * @brief Returns crop N uptake from layer i [kg N ha-1]
 * @return Crop N uptake
 */
double CropModule::get_NUptakeFromLayer(size_t i_Layer) const {
  return vc_NUptakeFromLayer[i_Layer];
}

/**
 * @brief Returns total crop biomass [kg ha-1]
 * @return Total crop biomass
 */
double CropModule::get_TotalBiomass() const {
  return vc_TotalBiomass;
}

/**
 * @brief Returns total crop N content [kg N ha-1]
 * @return Total crop N uptake
 */
double CropModule::get_TotalBiomassNContent() const {
  return vc_TotalBiomassNContent;
}

/**
 * @brief Returns aboveground biomass N content [kg N ha-1]
 * @return organ biomass
 */
double CropModule::get_AbovegroundBiomassNContent() const {
  return vc_AbovegroundBiomass * vc_NConcentrationAbovegroundBiomass;
}

/**
 * @brief Returns fruit biomass N concentration [kg N kg DM]
 * @return organ biomass
 */
double CropModule::get_FruitBiomassNConcentration() const {
  return (vc_TotalBiomassNContent - (get_OrganBiomass(0) * get_RootNConcentration()))
         / (get_OrganBiomass(3)
            + (pc_ResidueNRatio * (vc_TotalBiomass - get_OrganBiomass(0) - get_OrganBiomass(3))));
}

/**
 * @brief Returns fruit biomass N content [kg N ha-1]
 * @return organ biomass
 */
double CropModule::get_FruitBiomassNContent() const {
  return (get_OrganBiomass(3) * get_FruitBiomassNConcentration());
}

/**
 * @brief Returns root N content [kg N kg-1]
 * @return Root N content
 */
double CropModule::get_RootNConcentration() const {
  return vc_NConcentrationRoot;
}

/**
 * @brief Returns target N content [kg N kg-1]
 * @return Target N content
 */
double CropModule::get_TargetNConcentration() const {
  return vc_TargetNConcentration;
}

/**
 * @brief Returns critical N Content [kg N kg-1]
 * @return Critical N content
 */
double CropModule::get_CriticalNConcentration() const {
  return vc_CriticalNConcentration;
}

/**
 * @brief Returns above-ground biomass N concentration [kg N kg-1]
 * @return Above-ground biomass N concentration
 */
double CropModule::get_AbovegroundBiomassNConcentration() const {
  return vc_NConcentrationAbovegroundBiomass;
}

/**
 * @brief Returns heat sum for irrigation start [°C d]
 * @return heat sum for irrigation start
 */
double CropModule::get_HeatSumIrrigationStart() const {
  return pc_HeatSumIrrigationStart;
}

/**
 * @brief Returns heat sum for irrigation end [°C d]
 * @return heat sum for irrigation end
 */
double CropModule::get_HeatSumIrrigationEnd() const {
  return pc_HeatSumIrrigationEnd;
}

/**
 * @brief Returns number of above ground organs
 * @return number of above ground organs
 */
int CropModule::pc_NumberOfAbovegroundOrgans() const {
  int count = 0;
  for (size_t i = 0, size = pc_AbovegroundOrgan.size(); i < size; i++) {
    if (pc_AbovegroundOrgan[i]) {
      count++;
    }
  }
  return count;
}

namespace {
typedef vector<monica::YieldComponent> VYC;

/**
 * @brief Returns crop yield.
 *
 * @param v Vector yield component
 * @param bmv
 */
double calculateCropYield(const VYC& ycs, const vector<double>& bmv) {
  double yield = 0;
  for (const auto& yc : ycs) yield += bmv.at(yc.organId - 1) * (yc.yieldPercentage);
  return yield;
}

/**
 * @brief Returns crop yield.
 *
 * @param v Vector yield component
 * @param bmv
 */
double calculateCropFreshMatterYield(const VYC& ycs, const vector<double>& bmv) {
  double freshMatterYield = 0;
  for (auto yc : ycs) freshMatterYield += bmv.at(yc.organId - 1) * yc.yieldPercentage / yc.yieldDryMatter;
  return freshMatterYield;
}
} // namespace

/**
 * @brief Returns primary crop yield
 * @return primary yield
 */
double CropModule::get_PrimaryCropYield() const {
  return calculateCropYield(pc_OrganIdsForPrimaryYield, vc_OrganBiomass);
}

/**
 * @brief Returns secondary crop yield
 * @return crop yield
 */
double CropModule::get_SecondaryCropYield() const {
  return calculateCropYield(pc_OrganIdsForSecondaryYield, vc_OrganBiomass);
}

/**
 * @brief Returns crop yield after cutting
 * @return crop yield after cutting
 */
double CropModule::get_CropYieldAfterCutting() const {
  return calculateCropYield(pc_OrganIdsForCutting, vc_OrganBiomass);
}

/**
 * @brief Returns primary crop yield fresh matter
 * @return primary yield
 */
double CropModule::get_FreshPrimaryCropYield() const {
  return calculateCropFreshMatterYield(pc_OrganIdsForPrimaryYield, vc_OrganBiomass);
}

/**
 * @brief Returns secondary crop yield fresh matter
 * @return crop yield
 */
double CropModule::get_FreshSecondaryCropYield() const {
  return calculateCropFreshMatterYield(pc_OrganIdsForSecondaryYield, vc_OrganBiomass);
}

/**
 * @brief Returns fresh matter crop yield after cutting
 * @return fresh crop yield after cutting
 */
double CropModule::get_FreshCropYieldAfterCutting() const {
  return calculateCropFreshMatterYield(pc_OrganIdsForCutting, vc_OrganBiomass);
}

/**
 * @brief Returns residue biomass
 * @return residue biomass
 */
double CropModule::get_ResidueBiomass(bool useSecondaryCropYields, double alternativeCropYield) const {
  auto cropYield = alternativeCropYield >= 0
                     ? alternativeCropYield
                     : get_PrimaryCropYield() + (useSecondaryCropYields ? get_SecondaryCropYield() : 0);

  return vc_TotalBiomass - get_OrganBiomass(0) - cropYield;
}

/**
 * @brief Returns residue N concentration [kg kg-1]
 * @return residue N concentration
 */
double CropModule::get_ResiduesNConcentration(double alternativePrimaryCropYield) const {
  auto primaryCropYield = alternativePrimaryCropYield >= 0 ? alternativePrimaryCropYield : get_PrimaryCropYield();
  auto rootBiomass = get_OrganBiomass(0);

  return (vc_TotalBiomassNContent - (rootBiomass * get_RootNConcentration()))
         / ((primaryCropYield / pc_ResidueNRatio) + (vc_TotalBiomass - rootBiomass - primaryCropYield));
}

/**
 * @brief Returns primary yield N concentration [kg kg-1]
 * @return primary yield N concentration
 */
double CropModule::get_PrimaryYieldNConcentration(double alternativePrimaryCropYield) const {
  auto primaryCropYield = alternativePrimaryCropYield >= 0 ? alternativePrimaryCropYield : get_PrimaryCropYield();
  auto rootBiomass = get_OrganBiomass(0);

  return (vc_TotalBiomassNContent - (rootBiomass * get_RootNConcentration()))
         / (primaryCropYield + (pc_ResidueNRatio * (vc_TotalBiomass - rootBiomass - primaryCropYield)));
}

double CropModule::get_ResiduesNContent(bool useSecondaryCropYields, double alternativePrimaryCropYield,
                                        double alternativeCropYield) const {
  return get_ResidueBiomass(useSecondaryCropYields, alternativeCropYield) *
         get_ResiduesNConcentration(alternativePrimaryCropYield);
}

double CropModule::get_PrimaryYieldNContent(double alternativePrimaryCropYield) const {
  auto primaryCropYield = alternativePrimaryCropYield >= 0 ? alternativePrimaryCropYield : get_PrimaryCropYield();
  return primaryCropYield * get_PrimaryYieldNConcentration(alternativePrimaryCropYield);
}

double CropModule::get_RawProteinConcentration() const {
  // Assuming an average N concentration of raw protein of 16%
  return get_PrimaryYieldNConcentration() * 6.25;
}

double CropModule::get_SecondaryYieldNContent(double alternativePrimaryCropYield,
                                              double alternativeSecondaryCropYield) const {
  auto secondaryCropYield =
    alternativeSecondaryCropYield >= 0 ? alternativeSecondaryCropYield : get_SecondaryCropYield();
  return secondaryCropYield * get_ResiduesNConcentration(alternativePrimaryCropYield);
}

/**
 * @brief Returns the accumulated crop's actual N uptake [kg N ha-1]
 * @return Sum crop actual N uptake
 */
double CropModule::get_SumTotalNUptake() const {
  return vc_SumTotalNUptake;
}

/**
 * @brief Returns the crop's actual N uptake [kg N ha-1]
 * @return Actual N uptake
 */
double CropModule::get_ActNUptake() const {
  return vc_TotalNUptake;
}

/**
 * @brief Returns the crop's potential N uptake [kg N ha-1]
 * @return Potential N uptake
 */
double CropModule::get_PotNUptake() const {
  return vc_CropNDemand * 10000.0;
}

/**
 * @brief Returns the crop's N input via atmospheric fixation [kg N ha-1]
 * @return Biological N fixation
 */
double CropModule::get_BiologicalNFixation() const {
  return vc_FixedN;
}

/**
 * @brief Returns the gross primary production [kg C ha-1 d-1]
 * @return Gross primary production
 */
double CropModule::get_GrossPrimaryProduction() const {
  return vc_GrossPrimaryProduction;
}

/**
 * @brief Returns the net primary production [kg C ha-1 d-1]
 * @return Net primary production
 */
double CropModule::get_NetPrimaryProduction() const {
  return vc_NetPrimaryProduction;
}

/**
 * @brief Returns the respiration [kg C ha-1 d-1]
 * @return Net primary production
 */
double CropModule::get_AutotrophicRespiration() const {
  return vc_TotalRespired / 30.0 * 12.0;; // Convert [kg CH2O ha-1 d-1] to [kg C ha-1 d-1]
}

/**
 * Returns the individual respiration of the organs [kg C ha-1 d-1]
 * based on the current ratio of the crop's biomass.
 */
double CropModule::get_OrganSpecificTotalRespired(int organ) const {
  // get total amount of actual biomass
  double total_biomass = totalBiomass();

  // get biomass of specific organ and calculates ratio
  double organ_percentage = get_OrganBiomass(organ) / total_biomass;
  return (get_AutotrophicRespiration() * organ_percentage);
}

/**
 * @brief Returns the organ-specific net primary production [kg C ha-1 d-1]
 * @return Organ-specific net primary production
 */
double CropModule::get_OrganSpecificNPP(int organ) const {
  // get total amount of actual biomass
  double total_biomass = totalBiomass();

  // get biomass of specific organ and calculates ratio
  double organ_percentage = get_OrganBiomass(organ) / total_biomass;

  // cout << "get_OrganBiomass(organ) : " << organ << ", " << organ_percentage << std::endl; // JV!
  // cout << "total_biomass : " << total_biomass << std::endl; // JV!
  return (get_NetPrimaryProduction() * organ_percentage);
}

int CropModule::get_StageAfterCut() const {
  return pc_StageAfterCut;
}

void monica::cropModuleApplyCutting(CropModule* cm,
                                     std::map<int, Cutting::Value>& organs,
                                     std::map<int, double>& exports,
                                     double cutMaxAssimilationFraction) {
  auto& _addOrganicMatter = cm->_addOrganicMatter;
  auto& pc_CuttingDelayDays = cm->pc_CuttingDelayDays;
  auto& pc_MaxAssimilationRate = cm->pc_MaxAssimilationRate;
  auto& pc_OrganIdsForCutting = cm->pc_OrganIdsForCutting;
  auto& pc_StageAfterCut = cm->pc_StageAfterCut;
  auto& vc_AbovegroundBiomass = cm->vc_AbovegroundBiomass;
  auto& vc_CuttingDelayDays = cm->vc_CuttingDelayDays;
  auto& vc_LeafAreaIndex = cm->vc_LeafAreaIndex;
  auto& vc_OrganBiomass = cm->vc_OrganBiomass;
  auto& vc_OrganDeadBiomass = cm->vc_OrganDeadBiomass;
  auto& vc_OrganGreenBiomass = cm->vc_OrganGreenBiomass;
  auto& vc_TotalBiomassNContent = cm->vc_TotalBiomassNContent;
  auto& vc_exportedCutBiomass = cm->vc_exportedCutBiomass;
  auto& vc_residueCutBiomass = cm->vc_residueCutBiomass;
  auto& vc_sumExportedCutBiomass = cm->vc_sumExportedCutBiomass;
  auto& vc_sumResidueCutBiomass = cm->vc_sumResidueCutBiomass;

  double oldAbovegroundBiomass = vc_AbovegroundBiomass;
  double oldAgbNcontent = cm->get_AbovegroundBiomassNContent();
  double sumCutBiomass = 0.0;
  double currentSLA = cm->get_LeafAreaIndex() / vc_OrganGreenBiomass[OId::LEAF];

  Tools::debug() << "CropModule::applyCutting()" << endl;

  if (organs.empty()) {
    for (auto yc : pc_OrganIdsForCutting) {
      Cutting::Value v;
      v.value = yc.yieldPercentage;
      organs[yc.organId - 1] = v;
    }
  }

  double sumResidueBiomass = 0;
  for (auto p : organs) {
    int organId = p.first;
    Cutting::Value organSpec = p.second;

    double oldOrganBiomass = vc_OrganBiomass.at(organId);
    double oldOrganDeadBiomass = vc_OrganDeadBiomass.at(organId);
    double oldOrganGreenBiomass = oldOrganBiomass - oldOrganDeadBiomass;
    double newOrganBiomass = 0.0;
    double cutOrganBiomass = 0.0;

    if (organSpec.unit == Cutting::biomass) {
      if (organSpec.cut_or_left == Cutting::cut) {
        cutOrganBiomass = std::min(organSpec.value, oldOrganBiomass);
        newOrganBiomass = oldOrganBiomass - cutOrganBiomass;
      } else if (organSpec.cut_or_left == Cutting::left) {
        newOrganBiomass = std::min(organSpec.value, oldOrganBiomass);
        cutOrganBiomass = oldOrganBiomass - newOrganBiomass;
      }

      // update dead biomass
      if (oldOrganBiomass == 0) {
        vc_OrganDeadBiomass[organId] = 0;
      } else {
        vc_OrganDeadBiomass[organId] = newOrganBiomass * std::min(oldOrganDeadBiomass / oldOrganBiomass, 1.0);
      }
    } else if (organSpec.unit == Cutting::percentage) {
      if (organSpec.cut_or_left == Cutting::cut) {
        cutOrganBiomass = organSpec.value * oldOrganBiomass;
        newOrganBiomass = oldOrganBiomass - cutOrganBiomass;
      } else if (organSpec.cut_or_left == Cutting::left) {
        newOrganBiomass = organSpec.value * oldOrganBiomass;
        cutOrganBiomass = oldOrganBiomass - newOrganBiomass;
      }

      // update dead biomass
      if (oldOrganBiomass == 0) {
        vc_OrganDeadBiomass[organId] = 0;
      } else {
        vc_OrganDeadBiomass[organId] = newOrganBiomass * std::min(oldOrganDeadBiomass / oldOrganBiomass, 1.0);
      }
    } else if (organSpec.unit == Cutting::LAI) {
      // only "left" is supported for LAI
      double currentLAI = cm->get_LeafAreaIndex();
      if (organSpec.value > currentLAI) {
        newOrganBiomass = oldOrganGreenBiomass;
        cutOrganBiomass = oldOrganDeadBiomass;
        vc_OrganDeadBiomass[organId] = 0; // all the dead biomass is assumed to be cut
      } else {
        newOrganBiomass = std::min(organSpec.value / currentSLA, oldOrganGreenBiomass);
        cutOrganBiomass = oldOrganBiomass - newOrganBiomass;
        vc_OrganDeadBiomass[organId] = 0; // all the dead biomass is assumed to be cut
      }
    }

    double exportBiomass = cutOrganBiomass * exports[organId];

    debug() << "cutting organ with id: " << organId << " with old biomass: " << oldOrganBiomass
      << " exporting percentage: " << (exports[organId] * 100) << "% -> export biomass: " << exportBiomass
      << " -> residues biomass: " << (cutOrganBiomass - exportBiomass) << endl;
    vc_AbovegroundBiomass -= cutOrganBiomass;
    sumCutBiomass += cutOrganBiomass;
    sumResidueBiomass += (cutOrganBiomass - exportBiomass);
    vc_OrganBiomass[organId] = newOrganBiomass;
    vc_OrganGreenBiomass[organId] = vc_OrganBiomass[organId] - vc_OrganDeadBiomass[organId];

    //		debug() << "cutting organ with id: " << organId << " with old biomass: " << oldOrganBiomass
    //			<< " exporting percentage: " << (exportFraction * 100) << "% -> export biomass: " << (export100Biomass * (1 - exportFraction))
    //			<< " -> residues biomass: " << export100Biomass * (1 - exportFraction) << endl;
    //		vc_AbovegroundBiomass -= export100Biomass;
    //		sumCutBiomass += export100Biomass;
    //		sumResidueBiomass += export100Biomass * (1 - exportFraction);
    //		vc_OrganBiomass[organId] = newOrganBiomass;
  }

  vc_exportedCutBiomass = sumCutBiomass - sumResidueBiomass;
  vc_sumExportedCutBiomass += vc_exportedCutBiomass;
  vc_residueCutBiomass = sumResidueBiomass;
  vc_sumResidueCutBiomass += vc_residueCutBiomass;

  debug() << "total cut biomass: " << sumCutBiomass
    << " exported cut biomass: " << vc_exportedCutBiomass
    << " residue cut biomass: " << vc_residueCutBiomass << endl;

  if (sumResidueBiomass > 0) {
    // prepare to add crop residues to soilorganic (AOMs)
    double residueNConcentration = cm->get_AbovegroundBiomassNConcentration();
    debug() << "adding organic matter from cut residues to soilOrganic" << endl;
    debug() << "Residue biomass: " << sumResidueBiomass
      << " Residue N concentration: " << residueNConcentration << endl;
    _addOrganicMatter({{0, sumResidueBiomass}}, residueNConcentration);
  }

  // update LAI
  if (vc_OrganGreenBiomass[OId::LEAF] > 0) {
    vc_LeafAreaIndex = vc_OrganGreenBiomass[OId::LEAF] * currentSLA;
  }

  // reset stage and temperature some after cutting
  cm->setStage(pc_StageAfterCut);

  vc_CuttingDelayDays = pc_CuttingDelayDays;
  pc_MaxAssimilationRate = pc_MaxAssimilationRate * cutMaxAssimilationFraction;

  // double rootNcontent = vc_TotalBiomassNContent - cm->get_AbovegroundBiomassNContent();
  // vc_TotalBiomassNContent = (vc_AbovegroundBiomass / oldAbovegroundBiomass) * cm->get_AbovegroundBiomassNContent() + rootNcontent;
  if (oldAbovegroundBiomass > 0.0) {
    vc_TotalBiomassNContent -= (1 - vc_AbovegroundBiomass / oldAbovegroundBiomass) * oldAgbNcontent;
  }




}


/**
 * Returns the depth of the maximum active and effective root.
 * [m]
 */
double CropModule::getEffectiveRootingDepth() const {
  size_t nols = soilColumn->size();

  for (size_t i_Layer = 0; i_Layer < nols; i_Layer++)
    if (vc_RootEffectivity[i_Layer] == 0.0) {
      return (i_Layer + 1) / 10.0;
    }

  return (nols + 1) / 10.0;
}

/**
 * @brief Setter for crop parameters of perennial crops after the transplant season.
 * @sets crop parameters of perennial crops after the transplant season
 */
void CropModule::fc_UpdateCropParametersForPerennial() {
  if (!perennialCropParams) {
    return;
  }

  pc_AbovegroundOrgan = perennialCropParams->speciesParams.pc_AbovegroundOrgan;
  pc_AssimilatePartitioningCoeff = perennialCropParams->cultivarParams.pc_AssimilatePartitioningCoeff;
  pc_AssimilateReallocation = perennialCropParams->speciesParams.pc_AssimilateReallocation;
  pc_BaseDaylength = perennialCropParams->cultivarParams.pc_BaseDaylength;
  pc_BaseTemperature = perennialCropParams->speciesParams.pc_BaseTemperature;
  pc_BeginSensitivePhaseHeatStress = perennialCropParams->cultivarParams.pc_BeginSensitivePhaseHeatStress;
  pc_CarboxylationPathway = perennialCropParams->speciesParams.pc_CarboxylationPathway;
  pc_CriticalOxygenContent = perennialCropParams->speciesParams.pc_CriticalOxygenContent;
  pc_CriticalTemperatureHeatStress = perennialCropParams->cultivarParams.pc_CriticalTemperatureHeatStress;
  pc_CropHeightP1 = perennialCropParams->cultivarParams.pc_CropHeightP1;
  pc_CropHeightP2 = perennialCropParams->cultivarParams.pc_CropHeightP2;
  pc_CropName = perennialCropParams->pc_CropName();
  pc_CropSpecificMaxRootingDepth = perennialCropParams->cultivarParams.pc_CropSpecificMaxRootingDepth;
  pc_DaylengthRequirement = perennialCropParams->cultivarParams.pc_DaylengthRequirement;
  pc_DefaultRadiationUseEfficiency = perennialCropParams->speciesParams.pc_DefaultRadiationUseEfficiency;
  pc_DevelopmentAccelerationByNitrogenStress = perennialCropParams->speciesParams.
                                                                    pc_DevelopmentAccelerationByNitrogenStress;
  pc_DroughtStressThreshold = perennialCropParams->cultivarParams.pc_DroughtStressThreshold;
  pc_DroughtImpactOnFertilityFactor = perennialCropParams->speciesParams.pc_DroughtImpactOnFertilityFactor;
  pc_EndSensitivePhaseHeatStress = perennialCropParams->cultivarParams.pc_EndSensitivePhaseHeatStress;
  pc_PartBiologicalNFixation = perennialCropParams->speciesParams.pc_PartBiologicalNFixation;
  pc_InitialKcFactor = perennialCropParams->speciesParams.pc_InitialKcFactor;
  pc_InitialOrganBiomass = perennialCropParams->speciesParams.pc_InitialOrganBiomass;
  pc_InitialRootingDepth = perennialCropParams->speciesParams.pc_InitialRootingDepth;
  pc_LimitingTemperatureHeatStress = perennialCropParams->speciesParams.pc_LimitingTemperatureHeatStress;
  pc_LuxuryNCoeff = perennialCropParams->speciesParams.pc_LuxuryNCoeff;
  pc_MaxAssimilationRate = perennialCropParams->cultivarParams.pc_MaxAssimilationRate;
  pc_MaxCropDiameter = perennialCropParams->speciesParams.pc_MaxCropDiameter;
  pc_MaxCropHeight = perennialCropParams->cultivarParams.pc_MaxCropHeight;
  pc_MaxNUptakeParam = perennialCropParams->speciesParams.pc_MaxNUptakeParam;
  pc_MinimumNConcentration = perennialCropParams->speciesParams.pc_MinimumNConcentration;
  pc_MinimumTemperatureForAssimilation = perennialCropParams->speciesParams.pc_MinimumTemperatureForAssimilation;
  pc_MinimumTemperatureRootGrowth = perennialCropParams->speciesParams.pc_MinimumTemperatureRootGrowth;
  pc_NConcentrationAbovegroundBiomass = perennialCropParams->speciesParams.pc_NConcentrationAbovegroundBiomass;
  pc_NConcentrationB0 = perennialCropParams->speciesParams.pc_NConcentrationB0;
  pc_NConcentrationPN = perennialCropParams->speciesParams.pc_NConcentrationPN;
  pc_NConcentrationRoot = perennialCropParams->speciesParams.pc_NConcentrationRoot;
  pc_NumberOfDevelopmentalStages = perennialCropParams->speciesParams.pc_NumberOfDevelopmentalStages();
  pc_NumberOfOrgans = perennialCropParams->speciesParams.pc_NumberOfOrgans();
  pc_OptimumTemperature = perennialCropParams->cultivarParams.pc_OptimumTemperature;
  pc_OrganGrowthRespiration = perennialCropParams->speciesParams.pc_OrganGrowthRespiration;
  pc_OrganMaintenanceRespiration = perennialCropParams->speciesParams.pc_OrganMaintenanceRespiration;
  pc_OrganSenescenceRate = perennialCropParams->cultivarParams.pc_OrganSenescenceRate;
  pc_Perennial = perennialCropParams->cultivarParams.pc_Perennial;
  pc_PlantDensity = perennialCropParams->speciesParams.pc_PlantDensity;
  pc_ResidueNRatio = perennialCropParams->cultivarParams.pc_ResidueNRatio;
  pc_RootDistributionParam = perennialCropParams->speciesParams.pc_RootDistributionParam;
  pc_RootFormFactor = perennialCropParams->speciesParams.pc_RootFormFactor;
  pc_RootGrowthLag = perennialCropParams->speciesParams.pc_RootGrowthLag;
  pc_RootPenetrationRate = perennialCropParams->speciesParams.pc_RootPenetrationRate;
  pc_SpecificLeafArea = perennialCropParams->cultivarParams.pc_SpecificLeafArea;
  pc_SpecificRootLength = perennialCropParams->speciesParams.pc_SpecificRootLength;
  pc_StageAtMaxDiameter = perennialCropParams->speciesParams.pc_StageAtMaxDiameter;
  pc_StageAtMaxHeight = perennialCropParams->speciesParams.pc_StageAtMaxHeight;
  pc_StageMaxRootNConcentration = perennialCropParams->speciesParams.pc_StageMaxRootNConcentration;
  pc_StageKcFactor = perennialCropParams->cultivarParams.pc_StageKcFactor;
  pc_StageTemperatureSum = perennialCropParams->cultivarParams.pc_StageTemperatureSum;
  pc_StorageOrgan = perennialCropParams->speciesParams.pc_StorageOrgan;
  pc_VernalisationRequirement = perennialCropParams->cultivarParams.pc_VernalisationRequirement;
}

/**
 * @brief Test if anthesis state is reached
 * @return True, if anthesis is reached, false otherwise.
 *
 * Method is called after calculation of the developmental stage.
 */
bool CropModule::isAnthesisDay(size_t old_dev_stage, size_t new_dev_stage) {
  auto abs = anthesisBetweenStages();
  return kj::get<0>(abs) == old_dev_stage && kj::get<1>(abs) == new_dev_stage;
}

kj::Tuple<int, int> CropModule::anthesisBetweenStages() const {
  if (pc_NumberOfDevelopmentalStages == 6) { return kj::tuple(3, 4); } else if (
    pc_NumberOfDevelopmentalStages == 7)
    return kj::tuple(4, 5);
  return kj::tuple(-1, -1);
}

/**
 * @brief Test if maturity state is reached
 * @return True, if maturity is reached, false otherwise.
 *
 * Method is called after calculation of the developmental stage.
 */
bool CropModule::isMaturityDay(size_t old_dev_stage, size_t new_dev_stage) {
  // corn crops
  if (pc_NumberOfDevelopmentalStages == 6) {
    return (old_dev_stage == 4 && new_dev_stage == 5);
    // maize, sorghum and other crops with 7 developmental stages
  } else if (pc_NumberOfDevelopmentalStages == 7) {
    return (old_dev_stage == 5 && new_dev_stage == 6);
  }

  return false;
}

/**
 * @brief Getter for anthesis day.
 * @return Julian day of crop's anthesis
 */
int CropModule::getAnthesisDay() const {
  // cout << "Getter anthesis " << vc_AnthesisDay << endl;
  return vc_AnthesisDay;
}

/**
 * @brief Getter for maturity day.
 * @return Julian day of crop's maturity.
 */
int CropModule::getMaturityDay() const {
  // cout << "Getter maturity " << vc_MaturityDay << endl;
  return vc_MaturityDay;
}

bool CropModule::maturityReached() const {
  debug() << "vc_MaturityReached: " << vc_MaturityReached << endl;
  return vc_MaturityReached;
}

void CropModule::setStage(size_t newStage) {
  vc_CurrentTotalTemperatureSum = 0.0;
  for (size_t stage = 0; stage < pc_NumberOfDevelopmentalStages; stage++)
    if (stage < newStage) {
      vc_CurrentTotalTemperatureSum += vc_CurrentTemperatureSum[stage];
    } else {
      vc_CurrentTemperatureSum[stage] = 0.0;
    }

  vc_DevelopmentalStage = newStage;
}


// --- BEGIN TRANSPLANT MODIFICATION ---
/**
 * @brief Overrides normal seed-germination processes to force a custom crop state at transplanting.
 *
 * Biophysical & Design Rationale for Core MONICA Developers:
 *
 * 1. GDD Nursery Subtraction Arithmetic:
 *    A seedling is transplanted with an accumulated thermal sum (GDD) from the nursery. To integrate
 *    this into the stage-based MONICA architecture:
 *      a. All stages fully completed in the nursery (i < stage) are pre-filled to their target
 *         thresholds (`pc_StageTemperatureSum[i]`).
 *      b. The remainder is allocated to the active transplant stage's bucket (`vc_CurrentTemperatureSum[stage]`).
 *      c. Total running sum `vc_CurrentTotalTemperatureSum` is forced to `temperatureSum`.
 *      d. Crucially, `vc_TotalTemperatureSum` (representing the cumulative requirement for the whole cycle)
 *         is kept unmodified to preserve phenological targets.
 *
 * 2. Rooting Depth Initialization:
 *    At transplanting, the active roots are forced to `pc_InitialRootingDepth` parameter (retrieved from
 *    species parameters), and the active layers are synchronized immediately to avoid a 1-day step lag
 *    where soil interactions would be uninitialized.
 *
 * 3. Nitrogen Starvation bugfix:
 *    Seedlings forced directly into late stages start with zero internal Nitrogen pools, causing
 *    immediate lethal Nitrogen stress and negative photosynthesis. We explicitly initialize the
 *    biomass nitrogen concentration pools (`vc_NConcentrationAbovegroundBiomass`, `vc_NConcentrationRoot`,
 *    `vc_TotalBiomassNContent`) using standard optimal species concentration parameters.
 */
void CropModule::forceTransplantState(double temperatureSum, double lai, size_t stage,
                                      double rootMass, double leafMass, double shootMass, int postTransplantDelay) {
  // Initialize transplant shock duration parameters
  vc_TransplantShockDuration = postTransplantDelay;
  vc_DaysSinceTransplant = 0;

  // --- Step 1: Force developmental stage and LAI via native setters ---
  setStage(stage);
  setLeafAreaIndex(lai);

  // --- Step 2: Force cumulative and stage-specific GDD temperature sums ---
  vc_CurrentTotalTemperatureSum = temperatureSum;

  double remainingGDD = temperatureSum;
  for (size_t i = 0; i < vc_CurrentTemperatureSum.size(); i++) {
    if (i < stage) {
      double threshold = pc_StageTemperatureSum[i];
      vc_CurrentTemperatureSum[i] = threshold;
      remainingGDD -= threshold;
    } else if (i == stage) {
      vc_CurrentTemperatureSum[i] = std::max(0.0, remainingGDD);
    } else {
      vc_CurrentTemperatureSum[i] = 0.0;
    }
  }

  // --- Step 3: Initialize organ biomass pools ---
  if (vc_OrganBiomass.size() > 2) {
    vc_OrganBiomass[0] = rootMass;
    vc_OrganBiomass[1] = leafMass;
    vc_OrganBiomass[2] = shootMass;
    vc_OrganGreenBiomass[0] = rootMass;
    vc_OrganGreenBiomass[1] = leafMass;
    vc_OrganGreenBiomass[2] = shootMass;
  }

  // --- Step 4: Update carbon balance and rooting depth state variables ---
  vc_RootBiomass = rootMass;
  vc_AbovegroundBiomass = leafMass + shootMass;
  vc_TotalBiomass = rootMass + leafMass + shootMass;

  // Settle rooting zone and layers based on standard species parameters
  auto nols = soilColumn->size();
  double layerThickness = soilColumn->at(0).vs_LayerThickness;
  vc_RootingDepth_m = pc_InitialRootingDepth;
  vc_RootingDepth = std::min(int(std::round(vc_RootingDepth_m / layerThickness)), int(nols));
  vc_RootingZone = std::min(int(std::round(1.3 * vc_RootingDepth_m / layerThickness)), int(nols));

  // Force total root length based on physical constants
  vc_TotalRootLength = (vc_RootBiomass * 100000.0 * 100.0 / 7.0) / (0.015 * 0.015 * 3.14159265358979323);

  // Force nitrogen pools to prevent severe immediate starvation stress in new seedlings
  vc_NConcentrationAbovegroundBiomass = pc_NConcentrationAbovegroundBiomass;
  vc_NConcentrationRoot = pc_NConcentrationRoot;
  vc_TotalBiomassNContent = (vc_AbovegroundBiomass * pc_NConcentrationAbovegroundBiomass) +
                            (vc_RootBiomass * pc_NConcentrationRoot);
}

// --- END TRANSPLANT MODIFICATION ---

kj::Own<CropModule> monica::makeCropModule(SoilColumn* soilColumn,
                                           const CropParameters* cropParams,
                                           CropResidueParameters residueParams,
                                           bool isWinterCrop,
                                           const SiteParameters* siteParams,
                                           const CropModuleParameters* cropModuleParams,
                                           const SimulationParameters& simParams,
                                           std::function<void(std::string)> fireEvent,
                                           std::function<void(std::map<size_t, double>, double)> addOrganicMatter,
                                           std::function<std::pair<double, double>(double)> getSnowDepthAndCalcTempUnderSnow,
                                           Intercropping* intercropping) {
  auto cm = kj::heap<CropModule>();
  cropModuleInitialize(cm.get(), soilColumn, cropModuleParams, kj::mv(fireEvent), kj::mv(addOrganicMatter),
                       kj::mv(getSnowDepthAndCalcTempUnderSnow), intercropping);
  cropModuleInitializeFromCropParameters(cm.get(), cropParams, kj::mv(residueParams), isWinterCrop, siteParams, simParams);
  return cm;
}

kj::Own<CropModule> monica::makeCropModule(SoilColumn* soilColumn,
                                           const CropModuleParameters* cropModuleParams,
                                           std::function<void(std::string)> fireEvent,
                                           std::function<void(std::map<size_t, double>, double)> addOrganicMatter,
                                           std::function<std::pair<double, double>(double)> getSnowDepthAndCalcTempUnderSnow,
                                           mas::schema::model::monica::CropModuleState::Reader reader,
                                           Intercropping* intercropping) {
  auto cm = kj::heap<CropModule>();
  cropModuleInitialize(cm.get(), soilColumn, cropModuleParams, kj::mv(fireEvent), kj::mv(addOrganicMatter),
                       kj::mv(getSnowDepthAndCalcTempUnderSnow), intercropping);
  cropModuleDeserialize(cm.get(), reader);
  return cm;
}
