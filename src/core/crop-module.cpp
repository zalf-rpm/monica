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

#include "crop-module.h"

#include <cmath>
#include <string>

#include <kj/exception.h>

#include "tools/debug.h"
#include "soilmoisture.h"
#include "monica-parameters.h"
#include "tools/helper.h"
#include "tools/algorithms.h"
#include "voc-guenther.h"
#include "voc-jjv.h"
#include "voc-common.h"
#include "photosynthesis-FvCB.h"
#include "O3-impact.h"

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
CropModule::CropModule(SoilColumn &sc,
                       const CropParameters &cps,
                       CropResidueParameters rps,
                       bool isWinterCrop,
                       const SiteParameters &stps,
                       const CropModuleParameters &cropPs,
                       const SimulationParameters &simPs,
                       std::function<void(std::string)> fireEvent,
                       std::function<void(std::map<size_t, double>, double)> addOrganicMatter,
                       std::function<std::pair<double, double>(double)> getSnowDepthAndCalcTempUnderSnow,
                       Intercropping &ic)
    : _intercropping(ic)
      , _frostKillOn(simPs.pc_FrostKillOn)
      , soilColumn(sc)
      , cropPs(cropPs)
      , speciesPs(cps.speciesParams)
      , cultivarPs(cps.cultivarParams)
      , residuePs(kj::mv(rps))
      , _isWinterCrop(isWinterCrop)
      , vs_Latitude(stps.vs_Latitude)
      , pc_AbovegroundOrgan(cps.speciesParams.pc_AbovegroundOrgan)
      , pc_AssimilatePartitioningCoeff(cps.cultivarParams.pc_AssimilatePartitioningCoeff)
      , pc_AssimilateReallocation(cps.speciesParams.pc_AssimilateReallocation)
      , pc_BaseDaylength(cps.cultivarParams.pc_BaseDaylength)
      , pc_BaseTemperature(cps.speciesParams.pc_BaseTemperature)
      , pc_BeginSensitivePhaseHeatStress(cps.cultivarParams.pc_BeginSensitivePhaseHeatStress)
      , pc_CarboxylationPathway(cps.speciesParams.pc_CarboxylationPathway)
//  , pc_CO2Method(cps.pc_CO2Method)
      , pc_CriticalOxygenContent(cps.speciesParams.pc_CriticalOxygenContent)
      , pc_CriticalTemperatureHeatStress(cps.cultivarParams.pc_CriticalTemperatureHeatStress)
      , pc_CropHeightP1(cps.cultivarParams.pc_CropHeightP1)
      , pc_CropHeightP2(cps.cultivarParams.pc_CropHeightP2)
      , pc_CropName(cps.pc_CropName())
      , pc_CropSpecificMaxRootingDepth(cps.cultivarParams.pc_CropSpecificMaxRootingDepth)
      , vc_CurrentTemperatureSum(
        cps.speciesParams.pc_NumberOfDevelopmentalStages(), 0.0)
      , pc_CuttingDelayDays(cps.speciesParams.pc_CuttingDelayDays)
      , pc_DaylengthRequirement(cps.cultivarParams.pc_DaylengthRequirement)
      , pc_DefaultRadiationUseEfficiency(cps.speciesParams.pc_DefaultRadiationUseEfficiency)
      , pc_DevelopmentAccelerationByNitrogenStress(cps.speciesParams.pc_DevelopmentAccelerationByNitrogenStress)
      , pc_DroughtStressThreshold(cps.cultivarParams.pc_DroughtStressThreshold)
      , pc_DroughtImpactOnFertilityFactor(
        cps.speciesParams.pc_DroughtImpactOnFertilityFactor)
      , pc_EmergenceFloodingControlOn(
        simPs.pc_EmergenceFloodingControlOn)
      , pc_EmergenceMoistureControlOn(simPs.pc_EmergenceMoistureControlOn)
      , pc_EndSensitivePhaseHeatStress(cps.cultivarParams.pc_EndSensitivePhaseHeatStress), pc_FieldConditionModifier(
        cps.speciesParams.pc_FieldConditionModifier)
    //, vo_FreshSoilOrganicMatter(soilColumn.vs_NumberOfLayers(), 0.0)
      , pc_FrostDehardening(cps.cultivarParams.pc_FrostDehardening)
      , pc_FrostHardening(
        cps.cultivarParams.pc_FrostHardening)
      , pc_HeatSumIrrigationStart(cps.cultivarParams.pc_HeatSumIrrigationStart)
      , pc_HeatSumIrrigationEnd(cps.cultivarParams.pc_HeatSumIrrigationEnd)
      , vs_HeightNN(stps.vs_HeightNN)
      , pc_InitialKcFactor(cps.speciesParams.pc_InitialKcFactor)
      , pc_InitialOrganBiomass(cps.speciesParams.pc_InitialOrganBiomass)
      , pc_InitialRootingDepth(cps.speciesParams.pc_InitialRootingDepth)
      , vc_sunlitLeafAreaIndex(24)
      , vc_shadedLeafAreaIndex(24)
      , pc_LowTemperatureExposure(cps.cultivarParams.pc_LowTemperatureExposure)
      , pc_LimitingTemperatureHeatStress(cps.speciesParams.pc_LimitingTemperatureHeatStress)
      , pc_LT50cultivar(cps.cultivarParams.pc_LT50cultivar)
      , pc_LuxuryNCoeff(cps.speciesParams.pc_LuxuryNCoeff)
      , pc_MaxAssimilationRate(cps.cultivarParams.pc_MaxAssimilationRate)
      , pc_MaxCropDiameter(cps.speciesParams.pc_MaxCropDiameter)
      , pc_MaxCropHeight(cps.cultivarParams.pc_MaxCropHeight)
      , pc_MaxNUptakeParam(cps.speciesParams.pc_MaxNUptakeParam)
      , pc_MinimumNConcentration(cps.speciesParams.pc_MinimumNConcentration)
      , pc_MinimumTemperatureForAssimilation(
        cps.speciesParams.pc_MinimumTemperatureForAssimilation)
      , pc_MaximumTemperatureForAssimilation(
        cps.speciesParams.pc_MaximumTemperatureForAssimilation)
      , pc_OptimumTemperatureForAssimilation(
        cps.speciesParams.pc_OptimumTemperatureForAssimilation)
      , pc_MinimumTemperatureRootGrowth(
        cps.speciesParams.pc_MinimumTemperatureRootGrowth)
      , pc_NConcentrationAbovegroundBiomass(
        cps.speciesParams.pc_NConcentrationAbovegroundBiomass)
      , pc_NConcentrationB0(
        cps.speciesParams.pc_NConcentrationB0)
      , pc_NConcentrationPN(cps.speciesParams.pc_NConcentrationPN)
      , pc_NConcentrationRoot(cps.speciesParams.pc_NConcentrationRoot)
      , pc_NitrogenResponseOn(
        simPs.pc_NitrogenResponseOn)
      , pc_NumberOfDevelopmentalStages(cps.speciesParams.pc_NumberOfDevelopmentalStages())
      , pc_NumberOfOrgans(cps.speciesParams.pc_NumberOfOrgans())
      , vc_NUptakeFromLayer(soilColumn.vs_NumberOfLayers(), 0.0), pc_OptimumTemperature(
        cps.cultivarParams.pc_OptimumTemperature)
      , vc_OrganBiomass(pc_NumberOfOrgans, 0.0)
      , vc_OrganDeadBiomass(
        cps.speciesParams.pc_NumberOfOrgans(), 0.0)
      , vc_OrganGreenBiomass(cps.speciesParams.pc_NumberOfOrgans(), 0.0)
      , vc_OrganGrowthIncrement(pc_NumberOfOrgans, 0.0)
      , pc_OrganGrowthRespiration(
        cps.speciesParams.pc_OrganGrowthRespiration)
      , pc_OrganIdsForPrimaryYield(
        cps.cultivarParams.pc_OrganIdsForPrimaryYield)
      , pc_OrganIdsForSecondaryYield(
        cps.cultivarParams.pc_OrganIdsForSecondaryYield)
      , pc_OrganIdsForCutting(
        cps.cultivarParams.pc_OrganIdsForCutting)
      , pc_OrganMaintenanceRespiration(
        cps.speciesParams.pc_OrganMaintenanceRespiration), vc_OrganSenescenceIncrement(pc_NumberOfOrgans, 0.0)
      , pc_OrganSenescenceRate(cps.cultivarParams.pc_OrganSenescenceRate), pc_PartBiologicalNFixation(
        cps.speciesParams.pc_PartBiologicalNFixation), pc_Perennial(cps.cultivarParams.pc_Perennial), pc_PlantDensity(
        cps.speciesParams.pc_PlantDensity), pc_ResidueNRatio(cps.cultivarParams.pc_ResidueNRatio), pc_RespiratoryStress(
        cps.cultivarParams.pc_RespiratoryStress), vc_RootDensity(soilColumn.vs_NumberOfLayers(), 0.0), vc_RootDiameter(
        soilColumn.vs_NumberOfLayers(), 0.0), pc_RootDistributionParam(cps.speciesParams.pc_RootDistributionParam)
      , vc_RootEffectivity(soilColumn.vs_NumberOfLayers(), 0.0), pc_RootFormFactor(cps.speciesParams.pc_RootFormFactor)
      , pc_RootGrowthLag(cps.speciesParams.pc_RootGrowthLag), pc_RootPenetrationRate(
        cps.speciesParams.pc_RootPenetrationRate), vs_SoilMineralNContent(soilColumn.vs_NumberOfLayers(), 0.0)
      , pc_SpecificLeafArea(cps.cultivarParams.pc_SpecificLeafArea), pc_SpecificRootLength(
        cps.speciesParams.pc_SpecificRootLength), pc_StageAfterCut(cps.speciesParams.pc_StageAfterCut - 1)
      , pc_StageAtMaxDiameter(cps.speciesParams.pc_StageAtMaxDiameter), pc_StageAtMaxHeight(
        cps.speciesParams.pc_StageAtMaxHeight), pc_StageMaxRootNConcentration(
        cps.speciesParams.pc_StageMaxRootNConcentration), pc_StageKcFactor(cps.cultivarParams.pc_StageKcFactor)
      , pc_StageTemperatureSum(cps.cultivarParams.pc_StageTemperatureSum), pc_StorageOrgan(
        cps.speciesParams.pc_StorageOrgan), vs_Tortuosity(cropPs.pc_Tortuosity), vc_Transpiration(
        soilColumn.vs_NumberOfLayers(), 0.0), vc_TranspirationRedux(soilColumn.vs_NumberOfLayers(), 1.0)
      , pc_VernalisationRequirement(cps.cultivarParams.pc_VernalisationRequirement), pc_WaterDeficitResponseOn(
        simPs.pc_WaterDeficitResponseOn), vs_MaxEffectiveRootingDepth(stps.vs_MaxEffectiveRootingDepth)
      , vs_ImpenetrableLayerDepth(stps.vs_ImpenetrableLayerDepth), _rad24(_stepSize24), _rad240(_stepSize240), _tfol24(
        _stepSize24), _tfol240(_stepSize240), _fireEvent(kj::mv(fireEvent)), _addOrganicMatter(kj::mv(addOrganicMatter))
      , _getSnowDepthAndCalcTempUnderSnow(kj::mv(getSnowDepthAndCalcTempUnderSnow))
      , __enable_vernalisation_factor_fix__(
        cps.__enable_vernalisation_factor_fix__.orDefault(cropPs.__enable_vernalisation_factor_fix__)) {
  // Determining the total temperature sum of all developmental stages after
  // emergence (that's why i_Stage starts with 1) until before senescence
  for (int i_Stage = 1; i_Stage < pc_NumberOfDevelopmentalStages - 1; i_Stage++) {
    vc_TotalTemperatureSum += pc_StageTemperatureSum[i_Stage];
    if (i_Stage < pc_NumberOfDevelopmentalStages - 3) vc_TemperatureSumToFlowering += pc_StageTemperatureSum[i_Stage];
  }

  vc_FinalDevelopmentalStage = pc_NumberOfDevelopmentalStages - 1;

  // Determining the initial crop organ's biomass
  for (size_t i_Organ = 0; i_Organ < pc_NumberOfOrgans; i_Organ++) {
    vc_OrganBiomass[i_Organ] = pc_InitialOrganBiomass[i_Organ]; // [kg ha-1]

    if (pc_AbovegroundOrgan[i_Organ]) vc_AbovegroundBiomass += pc_InitialOrganBiomass[i_Organ]; // [kg ha-1]

    vc_TotalBiomass += pc_InitialOrganBiomass[i_Organ]; // [kg ha-1]

    // Define storage organ
    if (pc_StorageOrgan[i_Organ]) vc_StorageOrgan = i_Organ;
  }

  vc_OrganGreenBiomass = vc_OrganBiomass;

  vc_RootBiomass = pc_InitialOrganBiomass[0]; // [kg ha-1]

  // Initialisisng the leaf area index
  vc_LeafAreaIndex = vc_OrganBiomass[1] * pc_SpecificLeafArea[vc_DevelopmentalStage]; // [ha ha-1]

  if (vc_LeafAreaIndex <= 0.0) vc_LeafAreaIndex = 0.001;

  // Initialising the root
  vc_RootBiomass = vc_OrganBiomass[0];

  /** @todo Christian: Umrechnung korrekt wenn Biomasse in [kg m-2]? */
  vc_TotalRootLength = (vc_RootBiomass * 100000.0 * 100.0 / 7.0) / (0.015 * 0.015 * PI);

  vc_TotalBiomassNContent =
      (vc_AbovegroundBiomass * pc_NConcentrationAbovegroundBiomass) + (vc_RootBiomass * pc_NConcentrationRoot);
  vc_NConcentrationAbovegroundBiomass = pc_NConcentrationAbovegroundBiomass;
  vc_NConcentrationRoot = pc_NConcentrationRoot;

  // Initialising the initial maximum rooting depth
  if (cropPs.pc_AdjustRootDepthForSoilProps) {
//    double vc_SandContent = soilColumn[0].vs_SoilSandContent(); // [kg kg-1]
//    double vc_BulkDensity = soilColumn[0].vs_SoilBulkDensity(); // [kg m-3]
//    if (vc_SandContent < 0.55) vc_SandContent = 0.55;
//
//    vc_SoilSpecificMaxRootingDepth = vs_SoilSpecificMaxRootingDepth > 0.0
//                                     ? vs_SoilSpecificMaxRootingDepth
//                                     : vc_SandContent * ((1.1 - vc_SandContent) / 0.275) *
//                                       (1.4 / (vc_BulkDensity / 1000.0) +
//                                        (vc_BulkDensity * vc_BulkDensity / 40000000.0)); // [m]
//    vc_MaxRootingDepth = (vc_SoilSpecificMaxRootingDepth + (pc_CropSpecificMaxRootingDepth * 2.0)) / 3.0; //[m]
//
    // std::cout << "old mrd: " << vc_MaxRootingDepth << " -> ";
    auto R_P_max = pc_CropSpecificMaxRootingDepth;
    double f_S = soilColumn[0].vs_SoilSandContent(); // [kg kg-1]
    auto R_S = (f_S - 0.5) * -0.6;

    double rho_B = soilColumn[0].vs_SoilBulkDensity(); // [kg m-3]
    auto R_D = (rho_B / 1000.0 - 1) * -0.3;

    vc_MaxRootingDepth =
        R_P_max
        * ((R_P_max + (R_P_max * R_S)) / R_P_max)
        * ((R_P_max + (R_P_max * R_D)) / R_P_max);
    //std::cout << vc_MaxRootingDepth << " :new mrd" << endl;
  } else {
    vc_MaxRootingDepth = pc_CropSpecificMaxRootingDepth; //[m]
  }

  if (vs_ImpenetrableLayerDepth > 0) vc_MaxRootingDepth = min(vc_MaxRootingDepth, vs_ImpenetrableLayerDepth);
}

CropModule::CropModule(SoilColumn &sc,
                       const CropModuleParameters &cropPs,
                       std::function<void(std::string)> fireEvent,
                       std::function<void(std::map<size_t, double>, double)> addOrganicMatter,
                       std::function<std::pair<double, double>(double)> getSnowDepthAndCalcTempUnderSnow,
                       mas::schema::model::monica::CropModuleState::Reader reader,
                       Intercropping &ic)
    : _intercropping(ic), soilColumn(sc), cropPs(cropPs), _fireEvent(kj::mv(fireEvent)), _addOrganicMatter(
    kj::mv(addOrganicMatter)), _getSnowDepthAndCalcTempUnderSnow(kj::mv(getSnowDepthAndCalcTempUnderSnow)) {
  deserialize(reader);
}

double CropModule::sumStageTemperatureSums(int startAtStage, int endAtInclStage) const {
  double ts = 0;
  double endAtInclStage2 = endAtInclStage < 0 ? pc_NumberOfDevelopmentalStages + endAtInclStage + 1 : endAtInclStage;
  for (int s = startAtStage; s < endAtInclStage2; s++) ts += pc_StageTemperatureSum[s];
  return ts;
}

void CropModule::deserialize(mas::schema::model::monica::CropModuleState::Reader reader) {
  _frostKillOn = reader.getFrostKillOn();
  speciesPs.deserialize(reader.getSpeciesParams());
  cultivarPs.deserialize(reader.getCultivarParams());
  residuePs.deserialize(reader.getResidueParams());
  _isWinterCrop = reader.getIsWinterCrop();
  vs_Latitude = reader.getVsLatitude();
  vc_AbovegroundBiomass = reader.getAbovegroundBiomass();
  vc_AbovegroundBiomassOld = reader.getAbovegroundBiomassOld();
  setFromCapnpList(pc_AbovegroundOrgan, reader.getPcAbovegroundOrgan());
  vc_ActualTranspiration = reader.getActualTranspiration();

  {
    auto listReader = reader.getPcAssimilatePartitioningCoeff();
    pc_AssimilatePartitioningCoeff.resize(listReader.size());
    capnp::uint i = 0;
    for (auto &v: pc_AssimilatePartitioningCoeff)
      setFromCapnpList(v, listReader[i++]);
  }

  pc_AssimilateReallocation = reader.getPcAssimilateReallocation();
  vc_Assimilates = reader.getAssimilates();
  vc_AssimilationRate = reader.getAssimilationRate();
  vc_AstronomicDayLenght = reader.getAstronomicDayLenght();
  setFromCapnpList(pc_BaseDaylength, reader.getPcBaseDaylength());
  setFromCapnpList(pc_BaseTemperature, reader.getPcBaseTemperature());
  pc_BeginSensitivePhaseHeatStress = reader.getPcBeginSensitivePhaseHeatStress();
  vc_BelowgroundBiomass = reader.getBelowgroundBiomass();
  vc_BelowgroundBiomassOld = reader.getBelowgroundBiomassOld();
  pc_CarboxylationPathway = (int) reader.getPcCarboxylationPathway();
  vc_ClearDayRadiation = reader.getClearDayRadiation();
  pc_CO2Method = reader.getPcCo2Method();
  vc_CriticalNConcentration = reader.getCriticalNConcentration();
  setFromCapnpList(pc_CriticalOxygenContent, reader.getPcCriticalOxygenContent());
  pc_CriticalTemperatureHeatStress = reader.getPcCriticalTemperatureHeatStress();
  vc_CropDiameter = reader.getCropDiameter();
  vc_CropFrostRedux = reader.getCropFrostRedux();
  vc_CropHeatRedux = reader.getCropHeatRedux();
  vc_CropHeight = reader.getCropHeight();
  pc_CropHeightP1 = reader.getPcCropHeightP1();
  pc_CropHeightP2 = reader.getPcCropHeightP2();
  pc_CropName = reader.getPcCropName();
  vc_CropNDemand = reader.getCropNDemand();
  vc_CropNRedux = reader.getCropNRedux();
  pc_CropSpecificMaxRootingDepth = reader.getPcCropSpecificMaxRootingDepth();
  setFromCapnpList(vc_CropWaterUptake, reader.getCropWaterUptake());
  setFromCapnpList(vc_CurrentTemperatureSum, reader.getCurrentTemperatureSum());
  vc_CurrentTotalTemperatureSum = reader.getCurrentTotalTemperatureSum();
  vc_CurrentTotalTemperatureSumRoot = reader.getCurrentTotalTemperatureSumRoot();
  pc_CuttingDelayDays = reader.getPcCuttingDelayDays();
  vc_DaylengthFactor = reader.getDaylengthFactor();
  setFromCapnpList(pc_DaylengthRequirement, reader.getPcDaylengthRequirement());
  vc_DaysAfterBeginFlowering = reader.getDaysAfterBeginFlowering();
  vc_Declination = reader.getDeclination();
  pc_DefaultRadiationUseEfficiency = reader.getPcDefaultRadiationUseEfficiency();
  vm_DepthGroundwaterTable = reader.getVmDepthGroundwaterTable();
  pc_DevelopmentAccelerationByNitrogenStress = (int) reader.getPcDevelopmentAccelerationByNitrogenStress();
  vc_DevelopmentalStage = reader.getDevelopmentalStage();
  _noOfCropSteps = reader.getNoOfCropSteps();
  vc_DroughtImpactOnFertility = reader.getDroughtImpactOnFertility();
  pc_DroughtImpactOnFertilityFactor = reader.getPcDroughtImpactOnFertilityFactor();
  setFromCapnpList(pc_DroughtStressThreshold, reader.getPcDroughtStressThreshold());
  pc_EmergenceFloodingControlOn = reader.getPcEmergenceFloodingControlOn();
  pc_EmergenceMoistureControlOn = reader.getPcEmergenceMoistureControlOn();
  pc_EndSensitivePhaseHeatStress = reader.getPcEndSensitivePhaseHeatStress();
  vc_EffectiveDayLength = reader.getEffectiveDayLength();
  vc_ErrorStatus = reader.getErrorStatus();
  vc_ErrorMessage = reader.getErrorMessage();
  vc_EvaporatedFromIntercept = reader.getEvaporatedFromIntercept();
  vc_ExtraterrestrialRadiation = reader.getExtraterrestrialRadiation();
  pc_FieldConditionModifier = reader.getPcFieldConditionModifier();
  vc_FinalDevelopmentalStage = reader.getFinalDevelopmentalStage();
  vc_FixedN = reader.getFixedN();
  // std::vector<double> vo_FreshSoilOrganicMatt
  pc_FrostDehardening = reader.getPcFrostDehardening();
  pc_FrostHardening = reader.getPcFrostHardening();
  vc_GlobalRadiation = reader.getGlobalRadiation();
  vc_GreenAreaIndex = reader.getGreenAreaIndex();
  vc_GrossAssimilates = reader.getGrossAssimilates();
  vc_GrossPhotosynthesis = reader.getGrossPhotosynthesis();
  vc_GrossPhotosynthesis_mol = reader.getGrossPhotosynthesisMol();
  vc_GrossPhotosynthesisReference_mol = reader.getGrossPhotosynthesisReferenceMol();
  vc_GrossPrimaryProduction = reader.getGrossPrimaryProduction();
  vc_GrowthCycleEnded = reader.getGrowthCycleEnded();
  vc_GrowthRespirationAS = reader.getGrowthRespirationAS();
  pc_HeatSumIrrigationStart = reader.getPcHeatSumIrrigationStart();
  pc_HeatSumIrrigationEnd = reader.getPcHeatSumIrrigationEnd();
  vs_HeightNN = reader.getVsHeightNN();
  pc_InitialKcFactor = reader.getPcInitialKcFactor();
  setFromCapnpList(pc_InitialOrganBiomass, reader.getPcInitialOrganBiomass());
  pc_InitialRootingDepth = reader.getPcInitialRootingDepth();
  vc_InterceptionStorage = reader.getInterceptionStorage();
  vc_KcFactor = reader.getKcFactor();
  vc_LeafAreaIndex = reader.getLeafAreaIndex();
  setFromCapnpList(vc_sunlitLeafAreaIndex, reader.getSunlitLeafAreaIndex());
  setFromCapnpList(vc_shadedLeafAreaIndex, reader.getShadedLeafAreaIndex());
  pc_LowTemperatureExposure = reader.getPcLowTemperatureExposure();
  pc_LimitingTemperatureHeatStress = reader.getPcLimitingTemperatureHeatStress();
  vc_LT50 = reader.getLt50();
  vc_LT50M = reader.getLt50m();
  pc_LT50cultivar = reader.getPcLt50cultivar();
  pc_LuxuryNCoeff = reader.getPcLuxuryNCoeff();
  vc_MaintenanceRespirationAS = reader.getMaintenanceRespirationAS();
  pc_MaxAssimilationRate = reader.getPcMaxAssimilationRate();
  pc_MaxCropDiameter = reader.getPcMaxCropDiameter();
  pc_MaxCropHeight = reader.getPcMaxCropHeight();
  vc_MaxNUptake = reader.getMaxNUptake();
  pc_MaxNUptakeParam = reader.getPcMaxNUptakeParam();
  vc_MaxRootingDepth = reader.getPcMaxRootingDepth();
  pc_MinimumNConcentration = reader.getPcMinimumNConcentration();
  pc_MinimumTemperatureForAssimilation = reader.getPcMinimumTemperatureForAssimilation();
  pc_OptimumTemperatureForAssimilation = reader.getPcOptimumTemperatureForAssimilation();
  pc_MaximumTemperatureForAssimilation = reader.getPcMaximumTemperatureForAssimilation();
  pc_MinimumTemperatureRootGrowth = reader.getPcMinimumTemperatureRootGrowth();
  vc_NetMaintenanceRespiration = reader.getNetMaintenanceRespiration();
  vc_NetPhotosynthesis = reader.getNetPhotosynthesis();
  vc_NetPrecipitation = reader.getNetPrecipitation();
  vc_NetPrimaryProduction = reader.getNetPrimaryProduction();
  pc_NConcentrationAbovegroundBiomass = reader.getPcNConcentrationAbovegroundBiomass();
  vc_NConcentrationAbovegroundBiomass = reader.getNConcentrationAbovegroundBiomass();
  vc_NConcentrationAbovegroundBiomassOld = reader.getNConcentrationAbovegroundBiomassOld();
  pc_NConcentrationB0 = reader.getPcNConcentrationB0();
  vc_NContentDeficit = reader.getNContentDeficit();
  pc_NConcentrationPN = reader.getPcNConcentrationPN();
  pc_NConcentrationRoot = reader.getPcNConcentrationRoot();
  vc_NConcentrationRoot = reader.getNConcentrationRoot();
  vc_NConcentrationRootOld = reader.getNConcentrationRootOld();
  pc_NitrogenResponseOn = reader.getPcNitrogenResponseOn();
  pc_NumberOfDevelopmentalStages = (int) reader.getPcNumberOfDevelopmentalStages();
  pc_NumberOfOrgans = (int) reader.getPcNumberOfOrgans();
  setFromCapnpList(vc_NUptakeFromLayer, reader.getNUptakeFromLayer());
  setFromCapnpList(pc_OptimumTemperature, reader.getPcOptimumTemperature());
  setFromCapnpList(vc_OrganBiomass, reader.getOrganBiomass());
  setFromCapnpList(vc_OrganDeadBiomass, reader.getOrganDeadBiomass());
  setFromCapnpList(vc_OrganGreenBiomass, reader.getOrganGreenBiomass());
  setFromCapnpList(vc_OrganGrowthIncrement, reader.getOrganGrowthIncrement());
  setFromCapnpList(pc_OrganGrowthRespiration, reader.getPcOrganGrowthRespiration());
  setFromComplexCapnpList(pc_OrganIdsForPrimaryYield, reader.getPcOrganIdsForPrimaryYield());
  setFromComplexCapnpList(pc_OrganIdsForSecondaryYield, reader.getPcOrganIdsForSecondaryYield());
  setFromComplexCapnpList(pc_OrganIdsForCutting, reader.getPcOrganIdsForCutting());
  setFromCapnpList(pc_OrganMaintenanceRespiration, reader.getPcOrganMaintenanceRespiration());
  setFromCapnpList(vc_OrganSenescenceIncrement, reader.getOrganSenescenceIncrement());

  {
    auto listReader = reader.getPcOrganSenescenceRate();
    pc_OrganSenescenceRate.resize(listReader.size());
    capnp::uint i = 0;
    for (auto &v: pc_OrganSenescenceRate)
      setFromCapnpList(v, listReader[i++]);
  }

  vc_OvercastDayRadiation = reader.getOvercastDayRadiation();
  vc_OxygenDeficit = reader.getOxygenDeficit();
  pc_PartBiologicalNFixation = reader.getPcPartBiologicalNFixation();
  pc_Perennial = reader.getPcPerennial();
  vc_PhotoperiodicDaylength = reader.getPhotoperiodicDaylength();
  vc_PhotActRadiationMean = reader.getPhotActRadiationMean();
  pc_PlantDensity = reader.getPcPlantDensity();
  vc_PotentialTranspiration = reader.getPotentialTranspiration();
  vc_ReferenceEvapotranspiration = reader.getReferenceEvapotranspiration();
  vc_RelativeTotalDevelopment = reader.getRelativeTotalDevelopment();
  vc_RemainingEvapotranspiration = reader.getRemainingEvapotranspiration();
  vc_ReserveAssimilatePool = reader.getReserveAssimilatePool();
  pc_ResidueNRatio = reader.getPcResidueNRatio();
  pc_RespiratoryStress = reader.getPcRespiratoryStress();
  vc_RootBiomass = reader.getRootBiomass();
  vc_RootBiomassOld = reader.getRootBiomassOld();
  setFromCapnpList(vc_RootDensity, reader.getRootDensity());
  setFromCapnpList(vc_RootDiameter, reader.getRootDiameter());
  pc_RootDistributionParam = reader.getPcRootDistributionParam();
  setFromCapnpList(vc_RootEffectivity, reader.getRootEffectivity());
  pc_RootFormFactor = reader.getPcRootFormFactor();
  pc_RootGrowthLag = reader.getPcRootGrowthLag();
  vc_RootingDepth = reader.getRootingDepth();
  vc_RootingDepth_m = reader.getRootingDepthM();
  vc_RootingZone = reader.getRootingZone();
  pc_RootPenetrationRate = reader.getPcRootPenetrationRate();
  vm_SaturationDeficit = reader.getVmSaturationDeficit();
  vc_SoilCoverage = reader.getSoilCoverage();
  setFromCapnpList(vs_SoilMineralNContent, reader.getVsSoilMineralNContent());
  vc_SoilSpecificMaxRootingDepth = reader.getSoilSpecificMaxRootingDepth();
  vs_SoilSpecificMaxRootingDepth = reader.getVsSoilSpecificMaxRootingDepth();
  setFromCapnpList(pc_SpecificLeafArea, reader.getPcSpecificLeafArea());
  pc_SpecificRootLength = reader.getPcSpecificRootLength();
  pc_StageAfterCut = reader.getPcStageAfterCut();
  pc_StageAtMaxDiameter = reader.getPcStageAtMaxDiameter();
  pc_StageAtMaxHeight = reader.getPcStageAtMaxHeight();
  setFromCapnpList(pc_StageMaxRootNConcentration, reader.getPcStageMaxRootNConcentration());
  setFromCapnpList(pc_StageKcFactor, reader.getPcStageKcFactor());
  setFromCapnpList(pc_StageTemperatureSum, reader.getPcStageTemperatureSum());
  vc_StomataResistance = reader.getStomataResistance();
  setFromCapnpList(pc_StorageOrgan, reader.getPcStorageOrgan());
  vc_StorageOrgan = reader.getStorageOrgan();
  vc_TargetNConcentration = reader.getTargetNConcentration();
  vc_TimeStep = reader.getTimeStep();
  vc_TimeUnderAnoxia = (int) reader.getTimeUnderAnoxia();
  vs_Tortuosity = reader.getVsTortuosity();
  vc_TotalBiomass = reader.getTotalBiomass();
  vc_TotalBiomassNContent = reader.getTotalBiomassNContent();
  vc_TotalCropHeatImpact = reader.getTotalCropHeatImpact();
  vc_TotalNInput = reader.getTotalNInput();
  vc_TotalNUptake = reader.getTotalNUptake();
  vc_TotalRespired = reader.getTotalRespired();
  vc_Respiration = reader.getRespiration();
  vc_SumTotalNUptake = reader.getSumTotalNUptake();
  vc_TotalRootLength = reader.getTotalRootLength();
  vc_TotalTemperatureSum = reader.getTotalTemperatureSum();
  vc_TemperatureSumToFlowering = reader.getTemperatureSumToFlowering();
  setFromCapnpList(vc_Transpiration, reader.getTranspiration());
  setFromCapnpList(vc_TranspirationRedux, reader.getTranspirationRedux());
  vc_TranspirationDeficit = reader.getTranspirationDeficit();
  vc_VernalisationDays = reader.getVernalisationDays();
  vc_VernalisationFactor = reader.getVernalisationFactor();
  setFromCapnpList(pc_VernalisationRequirement, reader.getPcVernalisationRequirement());
  pc_WaterDeficitResponseOn = reader.getPcWaterDeficitResponseOn();
  dyingOut = reader.getDyingOut();
  vc_AccumulatedETa = reader.getAccumulatedETa();
  vc_AccumulatedTranspiration = reader.getAccumulatedTranspiration();
  vc_AccumulatedPrimaryCropYield = reader.getAccumulatedPrimaryCropYield();
  vc_sumExportedCutBiomass = reader.getSumExportedCutBiomass();
  vc_exportedCutBiomass = reader.getExportedCutBiomass();
  vc_sumResidueCutBiomass = reader.getSumResidueCutBiomass();
  vc_residueCutBiomass = reader.getResidueCutBiomass();
  vc_CuttingDelayDays = reader.getCuttingDelayDays();
  vs_MaxEffectiveRootingDepth = reader.getVsMaxEffectiveRootingDepth();
  vs_ImpenetrableLayerDepth = reader.getVsImpenetrableLayerDept();
  vc_AnthesisDay = reader.getAnthesisDay();
  vc_MaturityDay = reader.getMaturityDay();
  vc_MaturityReached = reader.getMaturityReached();
  // VOC members
  _stepSize24 = reader.getStepSize24();
  _stepSize240 = reader.getStepSize240();
  setFromCapnpList(_rad24, reader.getRad24());
  setFromCapnpList(_rad240, reader.getRad240());
  setFromCapnpList(_tfol24, reader.getTfol24());
  setFromCapnpList(_tfol240, reader.getTfol240());
  _index24 = reader.getIndex24();
  _index240 = reader.getIndex240();
  _full24 = reader.getFull24();
  _full240 = reader.getFull240();
  _guentherEmissions.deserialize(reader.getGuentherEmissions());
  _jjvEmissions.deserialize(reader.getJjvEmissions());
  _vocSpecies.deserialize(reader.getVocSpecies());
  _cropPhotosynthesisResults.deserialize(reader.getCropPhotosynthesisResults());
  vc_O3_shortTermDamage = reader.getO3ShortTermDamage();
  vc_O3_longTermDamage = reader.getO3LongTermDamage();
  vc_O3_senescence = reader.getO3Senescence();
  vc_O3_sumUptake = reader.getO3SumUptake();
  vc_O3_WStomatalClosure = reader.getO3WStomatalClosure();
  _assimilatePartCoeffsReduced = reader.getAssimilatePartCoeffsReduced();
  vc_KTkc = reader.getKtkc();
  vc_KTko = reader.getKtko();
  _stemElongationEventFired = reader.getStemElongationEventFired();
}

void CropModule::serialize(mas::schema::model::monica::CropModuleState::Builder builder) const {
  builder.setFrostKillOn(_frostKillOn);
  speciesPs.serialize(builder.initSpeciesParams());
  cultivarPs.serialize(builder.initCultivarParams());
  residuePs.serialize(builder.initResidueParams());
  builder.setIsWinterCrop(_isWinterCrop);
  builder.setVsLatitude(vs_Latitude);
  builder.setAbovegroundBiomass(vc_AbovegroundBiomass);
  builder.setAbovegroundBiomassOld(vc_AbovegroundBiomassOld);
  setCapnpList(pc_AbovegroundOrgan, builder.initPcAbovegroundOrgan((capnp::uint) pc_AbovegroundOrgan.size()));
  builder.setActualTranspiration(vc_ActualTranspiration);

  {
    auto coeffs = builder.initPcAssimilatePartitioningCoeff((capnp::uint) pc_AssimilatePartitioningCoeff.size());
    capnp::uint i = 0;
    for (const auto &v: pc_AssimilatePartitioningCoeff)
      setCapnpList(v, coeffs.init(i++, (capnp::uint) v.size()));
  }

  builder.setPcAssimilateReallocation(pc_AssimilateReallocation);
  builder.setAssimilates(vc_Assimilates);
  builder.setAssimilationRate(vc_AssimilationRate);
  builder.setAstronomicDayLenght(vc_AstronomicDayLenght);
  setCapnpList(pc_BaseDaylength, builder.initPcBaseDaylength((capnp::uint) pc_BaseDaylength.size()));
  setCapnpList(pc_BaseTemperature, builder.initPcBaseTemperature((capnp::uint) pc_BaseTemperature.size()));
  builder.setPcBeginSensitivePhaseHeatStress(pc_BeginSensitivePhaseHeatStress);
  builder.setBelowgroundBiomass(vc_BelowgroundBiomass);
  builder.setBelowgroundBiomassOld(vc_BelowgroundBiomassOld);
  builder.setPcCarboxylationPathway(pc_CarboxylationPathway);
  builder.setClearDayRadiation(vc_ClearDayRadiation);
  builder.setPcCo2Method(pc_CO2Method);
  builder.setCriticalNConcentration(vc_CriticalNConcentration);
  setCapnpList(pc_CriticalOxygenContent,
               builder.initPcCriticalOxygenContent((capnp::uint) pc_CriticalOxygenContent.size()));
  builder.setPcCriticalTemperatureHeatStress(pc_CriticalTemperatureHeatStress);
  builder.setCropDiameter(vc_CropDiameter);
  builder.setCropFrostRedux(vc_CropFrostRedux);
  builder.setCropHeatRedux(vc_CropHeatRedux);
  builder.setCropHeight(vc_CropHeight);
  builder.setPcCropHeightP1(pc_CropHeightP1);
  builder.setPcCropHeightP2(pc_CropHeightP2);
  builder.setPcCropName(pc_CropName);
  builder.setCropNDemand(vc_CropNDemand);
  builder.setCropNRedux(vc_CropNRedux);
  builder.setPcCropSpecificMaxRootingDepth(pc_CropSpecificMaxRootingDepth);
  setCapnpList(vc_CropWaterUptake, builder.initCropWaterUptake((capnp::uint) vc_CropWaterUptake.size()));
  setCapnpList(vc_CurrentTemperatureSum,
               builder.initCurrentTemperatureSum((capnp::uint) vc_CurrentTemperatureSum.size()));
  builder.setCurrentTotalTemperatureSum(vc_CurrentTotalTemperatureSum);
  builder.setCurrentTotalTemperatureSumRoot(vc_CurrentTotalTemperatureSumRoot);
  builder.setPcCuttingDelayDays(pc_CuttingDelayDays);
  builder.setDaylengthFactor(vc_DaylengthFactor);
  setCapnpList(pc_DaylengthRequirement,
               builder.initPcDaylengthRequirement((capnp::uint) pc_DaylengthRequirement.size()));
  builder.setDaysAfterBeginFlowering(vc_DaysAfterBeginFlowering);
  builder.setDeclination(vc_Declination);
  builder.setPcDefaultRadiationUseEfficiency(pc_DefaultRadiationUseEfficiency);
  builder.setVmDepthGroundwaterTable(vm_DepthGroundwaterTable);
  builder.setPcDevelopmentAccelerationByNitrogenStress(pc_DevelopmentAccelerationByNitrogenStress);
  builder.setDevelopmentalStage((uint16_t) vc_DevelopmentalStage);
  builder.setNoOfCropSteps(_noOfCropSteps);
  builder.setDroughtImpactOnFertility(vc_DroughtImpactOnFertility);
  builder.setPcDroughtImpactOnFertilityFactor(pc_DroughtImpactOnFertilityFactor);
  setCapnpList(pc_DroughtStressThreshold,
               builder.initPcDroughtStressThreshold((capnp::uint) pc_DroughtStressThreshold.size()));
  builder.setPcEmergenceFloodingControlOn(pc_EmergenceFloodingControlOn);
  builder.setPcEmergenceMoistureControlOn(pc_EmergenceMoistureControlOn);
  builder.setPcEndSensitivePhaseHeatStress(pc_EndSensitivePhaseHeatStress);
  builder.setEffectiveDayLength(vc_EffectiveDayLength);
  builder.setErrorStatus(vc_ErrorStatus);
  builder.setErrorMessage(vc_ErrorMessage);
  builder.setEvaporatedFromIntercept(vc_EvaporatedFromIntercept);
  builder.setExtraterrestrialRadiation(vc_ExtraterrestrialRadiation);
  builder.setPcFieldConditionModifier(pc_FieldConditionModifier);
  builder.setFinalDevelopmentalStage((uint16_t) vc_FinalDevelopmentalStage);
  builder.setFixedN(vc_FixedN);
  // std::vector<double> vo_FreshSoilOrganicMatt
  builder.setPcFrostDehardening(pc_FrostDehardening);
  builder.setPcFrostHardening(pc_FrostHardening);
  builder.setGlobalRadiation(vc_GlobalRadiation);
  builder.setGreenAreaIndex(vc_GreenAreaIndex);
  builder.setGrossAssimilates(vc_GrossAssimilates);
  builder.setGrossPhotosynthesis(vc_GrossPhotosynthesis);
  builder.setGrossPhotosynthesisMol(vc_GrossPhotosynthesis_mol);
  builder.setGrossPhotosynthesisReferenceMol(vc_GrossPhotosynthesisReference_mol);
  builder.setGrossPrimaryProduction(vc_GrossPrimaryProduction);
  builder.setGrowthCycleEnded(vc_GrowthCycleEnded);
  builder.setGrowthRespirationAS(vc_GrowthRespirationAS);
  builder.setPcHeatSumIrrigationStart(pc_HeatSumIrrigationStart);
  builder.setPcHeatSumIrrigationEnd(pc_HeatSumIrrigationEnd);
  builder.setVsHeightNN(vs_HeightNN);
  builder.setPcInitialKcFactor(pc_InitialKcFactor);
  setCapnpList(pc_InitialOrganBiomass, builder.initPcInitialOrganBiomass((capnp::uint) pc_InitialOrganBiomass.size()));
  builder.setPcInitialRootingDepth(pc_InitialRootingDepth);
  builder.setInterceptionStorage(vc_InterceptionStorage);
  builder.setKcFactor(vc_KcFactor);
  builder.setLeafAreaIndex(vc_LeafAreaIndex);
  setCapnpList(vc_sunlitLeafAreaIndex, builder.initSunlitLeafAreaIndex((capnp::uint) vc_sunlitLeafAreaIndex.size()));
  setCapnpList(vc_shadedLeafAreaIndex, builder.initShadedLeafAreaIndex((capnp::uint) vc_shadedLeafAreaIndex.size()));
  builder.setPcLowTemperatureExposure(pc_LowTemperatureExposure);
  builder.setPcLimitingTemperatureHeatStress(pc_LimitingTemperatureHeatStress);
  builder.setLt50(vc_LT50);
  builder.setLt50m(vc_LT50M);
  builder.setPcLt50cultivar(pc_LT50cultivar);
  builder.setPcLuxuryNCoeff(pc_LuxuryNCoeff);
  builder.setMaintenanceRespirationAS(vc_MaintenanceRespirationAS);
  builder.setPcMaxAssimilationRate(pc_MaxAssimilationRate);
  builder.setPcMaxCropDiameter(pc_MaxCropDiameter);
  builder.setPcMaxCropHeight(pc_MaxCropHeight);
  builder.setMaxNUptake(vc_MaxNUptake);
  builder.setPcMaxNUptakeParam(pc_MaxNUptakeParam);
  builder.setPcMaxRootingDepth(vc_MaxRootingDepth);
  builder.setPcMinimumNConcentration(pc_MinimumNConcentration);
  builder.setPcMinimumTemperatureForAssimilation(pc_MinimumTemperatureForAssimilation);
  builder.setPcOptimumTemperatureForAssimilation(pc_OptimumTemperatureForAssimilation);
  builder.setPcMaximumTemperatureForAssimilation(pc_MaximumTemperatureForAssimilation);
  builder.setPcMinimumTemperatureRootGrowth(pc_MinimumTemperatureRootGrowth);
  builder.setNetMaintenanceRespiration(vc_NetMaintenanceRespiration);
  builder.setNetPhotosynthesis(vc_NetPhotosynthesis);
  builder.setNetPrecipitation(vc_NetPrecipitation);
  builder.setNetPrimaryProduction(vc_NetPrimaryProduction);
  builder.setPcNConcentrationAbovegroundBiomass(pc_NConcentrationAbovegroundBiomass);
  builder.setNConcentrationAbovegroundBiomass(vc_NConcentrationAbovegroundBiomass);
  builder.setNConcentrationAbovegroundBiomassOld(vc_NConcentrationAbovegroundBiomassOld);
  builder.setPcNConcentrationB0(pc_NConcentrationB0);
  builder.setNContentDeficit(vc_NContentDeficit);
  builder.setPcNConcentrationPN(pc_NConcentrationPN);
  builder.setPcNConcentrationRoot(pc_NConcentrationRoot);
  builder.setNConcentrationRoot(vc_NConcentrationRoot);
  builder.setNConcentrationRootOld(vc_NConcentrationRootOld);
  builder.setPcNitrogenResponseOn(pc_NitrogenResponseOn);
  builder.setPcNumberOfDevelopmentalStages(pc_NumberOfDevelopmentalStages);
  builder.setPcNumberOfOrgans(pc_NumberOfOrgans);
  setCapnpList(vc_NUptakeFromLayer, builder.initNUptakeFromLayer((capnp::uint) vc_NUptakeFromLayer.size()));
  setCapnpList(pc_OptimumTemperature, builder.initPcOptimumTemperature((capnp::uint) pc_OptimumTemperature.size()));
  setCapnpList(vc_OrganBiomass, builder.initOrganBiomass((capnp::uint) vc_OrganBiomass.size()));
  setCapnpList(vc_OrganDeadBiomass, builder.initOrganDeadBiomass((capnp::uint) vc_OrganDeadBiomass.size()));
  setCapnpList(vc_OrganGreenBiomass, builder.initOrganGreenBiomass((capnp::uint) vc_OrganGreenBiomass.size()));
  setCapnpList(vc_OrganGrowthIncrement, builder.initOrganGrowthIncrement((capnp::uint) vc_OrganGrowthIncrement.size()));
  setCapnpList(pc_OrganGrowthRespiration,
               builder.initPcOrganGrowthRespiration((capnp::uint) pc_OrganGrowthRespiration.size()));
  setComplexCapnpList(pc_OrganIdsForPrimaryYield,
                      builder.initPcOrganIdsForPrimaryYield((capnp::uint) pc_OrganIdsForPrimaryYield.size()));
  setComplexCapnpList(pc_OrganIdsForSecondaryYield,
                      builder.initPcOrganIdsForSecondaryYield((capnp::uint) pc_OrganIdsForSecondaryYield.size()));
  setComplexCapnpList(pc_OrganIdsForCutting,
                      builder.initPcOrganIdsForCutting((capnp::uint) pc_OrganIdsForCutting.size()));
  setCapnpList(pc_OrganMaintenanceRespiration,
               builder.initPcOrganMaintenanceRespiration((capnp::uint) pc_OrganMaintenanceRespiration.size()));
  setCapnpList(vc_OrganSenescenceIncrement,
               builder.initOrganSenescenceIncrement((capnp::uint) vc_OrganSenescenceIncrement.size()));

  {
    auto listBuilder = builder.initPcOrganSenescenceRate((capnp::uint) pc_OrganSenescenceRate.size());
    capnp::uint i = 0;
    for (const auto &v: pc_OrganSenescenceRate)
      setCapnpList(v, listBuilder.init(i++, (capnp::uint) v.size()));
  }

  builder.setOvercastDayRadiation(vc_OvercastDayRadiation);
  builder.setOxygenDeficit(vc_OxygenDeficit);
  builder.setPcPartBiologicalNFixation(pc_PartBiologicalNFixation);
  builder.setPcPerennial(pc_Perennial);
  builder.setPhotoperiodicDaylength(vc_PhotoperiodicDaylength);
  builder.setPhotActRadiationMean(vc_PhotActRadiationMean);
  builder.setPcPlantDensity(pc_PlantDensity);
  builder.setPotentialTranspiration(vc_PotentialTranspiration);
  builder.setReferenceEvapotranspiration(vc_ReferenceEvapotranspiration);
  builder.setRelativeTotalDevelopment(vc_RelativeTotalDevelopment);
  builder.setRemainingEvapotranspiration(vc_RemainingEvapotranspiration);
  builder.setReserveAssimilatePool(vc_ReserveAssimilatePool);
  builder.setPcResidueNRatio(pc_ResidueNRatio);
  builder.setPcRespiratoryStress(pc_RespiratoryStress);
  builder.setRootBiomass(vc_RootBiomass);
  builder.setRootBiomassOld(vc_RootBiomassOld);
  setCapnpList(vc_RootDensity, builder.initRootDensity((capnp::uint) vc_RootDensity.size()));
  setCapnpList(vc_RootDiameter, builder.initRootDiameter((capnp::uint) vc_RootDiameter.size()));
  builder.setPcRootDistributionParam(pc_RootDistributionParam);
  setCapnpList(vc_RootEffectivity, builder.initRootEffectivity((capnp::uint) vc_RootEffectivity.size()));
  builder.setPcRootFormFactor(pc_RootFormFactor);
  builder.setPcRootGrowthLag(pc_RootGrowthLag);
  builder.setRootingDepth((uint16_t) vc_RootingDepth);
  builder.setRootingDepthM(vc_RootingDepth_m);
  builder.setRootingZone((uint16_t) vc_RootingZone);
  builder.setPcRootPenetrationRate(pc_RootPenetrationRate);
  builder.setVmSaturationDeficit(vm_SaturationDeficit);
  builder.setSoilCoverage(vc_SoilCoverage);
  setCapnpList(vs_SoilMineralNContent, builder.initVsSoilMineralNContent((capnp::uint) vs_SoilMineralNContent.size()));
  builder.setSoilSpecificMaxRootingDepth(vc_SoilSpecificMaxRootingDepth);
  builder.setVsSoilSpecificMaxRootingDepth(vs_SoilSpecificMaxRootingDepth);
  setCapnpList(pc_SpecificLeafArea, builder.initPcSpecificLeafArea((capnp::uint) pc_SpecificLeafArea.size()));
  builder.setPcSpecificRootLength(pc_SpecificRootLength);
  builder.setPcStageAfterCut(pc_StageAfterCut);
  builder.setPcStageAtMaxDiameter(pc_StageAtMaxDiameter);
  builder.setPcStageAtMaxHeight(pc_StageAtMaxHeight);
  setCapnpList(pc_StageMaxRootNConcentration,
               builder.initPcStageMaxRootNConcentration((capnp::uint) pc_StageMaxRootNConcentration.size()));
  setCapnpList(pc_StageKcFactor, builder.initPcStageKcFactor((capnp::uint) pc_StageKcFactor.size()));
  setCapnpList(pc_StageTemperatureSum, builder.initPcStageTemperatureSum((capnp::uint) pc_StageTemperatureSum.size()));
  builder.setStomataResistance(vc_StomataResistance);
  setCapnpList(pc_StorageOrgan, builder.initPcStorageOrgan((capnp::uint) pc_StorageOrgan.size()));
  builder.setStorageOrgan(vc_StorageOrgan);
  builder.setTargetNConcentration(vc_TargetNConcentration);
  builder.setTimeStep(vc_TimeStep);
  builder.setTimeUnderAnoxia(vc_TimeUnderAnoxia);
  builder.setVsTortuosity(vs_Tortuosity);
  builder.setTotalBiomass(vc_TotalBiomass);
  builder.setTotalBiomassNContent(vc_TotalBiomassNContent);
  builder.setTotalCropHeatImpact(vc_TotalCropHeatImpact);
  builder.setTotalNInput(vc_TotalNInput);
  builder.setTotalNUptake(vc_TotalNUptake);
  builder.setTotalRespired(vc_TotalRespired);
  builder.setRespiration(vc_Respiration);
  builder.setSumTotalNUptake(vc_SumTotalNUptake);
  builder.setTotalRootLength(vc_TotalRootLength);
  builder.setTotalTemperatureSum(vc_TotalTemperatureSum);
  builder.setTemperatureSumToFlowering(vc_TemperatureSumToFlowering);
  setCapnpList(vc_Transpiration, builder.initTranspiration((capnp::uint) vc_Transpiration.size()));
  setCapnpList(vc_TranspirationRedux, builder.initTranspirationRedux((capnp::uint) vc_TranspirationRedux.size()));
  builder.setTranspirationDeficit(vc_TranspirationDeficit);
  builder.setVernalisationDays(vc_VernalisationDays);
  builder.setVernalisationFactor(vc_VernalisationFactor);
  setCapnpList(pc_VernalisationRequirement,
               builder.initPcVernalisationRequirement((capnp::uint) pc_VernalisationRequirement.size()));
  builder.setPcWaterDeficitResponseOn(pc_WaterDeficitResponseOn);
  builder.setDyingOut(dyingOut);
  builder.setAccumulatedETa(vc_AccumulatedETa);
  builder.setAccumulatedTranspiration(vc_AccumulatedTranspiration);
  builder.setAccumulatedPrimaryCropYield(vc_AccumulatedPrimaryCropYield);
  builder.setSumExportedCutBiomass(vc_sumExportedCutBiomass);
  builder.setExportedCutBiomass(vc_exportedCutBiomass);
  builder.setSumResidueCutBiomass(vc_sumResidueCutBiomass);
  builder.setResidueCutBiomass(vc_residueCutBiomass);
  builder.setCuttingDelayDays(vc_CuttingDelayDays);
  builder.setVsMaxEffectiveRootingDepth(vs_MaxEffectiveRootingDepth);
  builder.setVsImpenetrableLayerDept(vs_ImpenetrableLayerDepth);
  builder.setAnthesisDay(vc_AnthesisDay);
  builder.setMaturityDay(vc_MaturityDay);
  builder.setMaturityReached(vc_MaturityReached);
  builder.setStepSize24(_stepSize24);
  builder.setStepSize240(_stepSize240);
  setCapnpList(_rad24, builder.initRad24((capnp::uint) _rad24.size()));
  setCapnpList(_rad240, builder.initRad240((capnp::uint) _rad240.size()));
  setCapnpList(_tfol24, builder.initTfol24((capnp::uint) _tfol24.size()));
  setCapnpList(_tfol240, builder.initTfol240((capnp::uint) _tfol240.size()));
  builder.setIndex24(_index24);
  builder.setIndex240(_index240);
  builder.setFull24(_full24);
  builder.setFull240(_full240);
  _guentherEmissions.serialize(builder.initGuentherEmissions());
  _jjvEmissions.serialize(builder.initJjvEmissions());
  _vocSpecies.serialize(builder.initVocSpecies());
  _cropPhotosynthesisResults.serialize(builder.initCropPhotosynthesisResults());
  builder.setO3ShortTermDamage(vc_O3_shortTermDamage);
  builder.setO3LongTermDamage(vc_O3_longTermDamage);
  builder.setO3Senescence(vc_O3_senescence);
  builder.setO3SumUptake(vc_O3_sumUptake);
  builder.setO3WStomatalClosure(vc_O3_WStomatalClosure);
  builder.setAssimilatePartCoeffsReduced(_assimilatePartCoeffsReduced);
  builder.setKtkc(vc_KTkc);
  builder.setKtko(vc_KTko);
  builder.setStemElongationEventFired(_stemElongationEventFired);
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
void CropModule::step(double vw_MeanAirTemperature,
                      double vw_MaxAirTemperature,
                      double vw_MinAirTemperature,
                      double vw_GlobalRadiation,
                      double vw_SunshineHours,
                      Date currentDate,
                      double vw_RelativeHumidity,
                      double vw_WindSpeed,
                      double vw_WindSpeedHeight,
                      double vw_AtmosphericCO2Concentration,
                      double vw_AtmosphericO3Concentration,
                      double vw_GrossPrecipitation,
                      double vw_ReferenceEvapotranspiration) {
  int vs_JulianDay = int(currentDate.julianDay());

  if (vc_CuttingDelayDays > 0) vc_CuttingDelayDays--;

  //  cout << "Cropstep: " << vw_MinAirTemperature << "\t" << vw_MaxAirTemperature << "\t" << vw_MeanAirTemperature << endl;
  fc_Radiation(vs_JulianDay, vw_GlobalRadiation, vw_SunshineHours);

  vc_OxygenDeficit = fc_OxygenDeficiency(pc_CriticalOxygenContent[vc_DevelopmentalStage]);

  size_t old_DevelopmentalStage = vc_DevelopmentalStage;

  fc_CropDevelopmentalStage(vw_MeanAirTemperature,
                            soilColumn[0].get_Vs_SoilMoisture_m3(),
                            soilColumn[0].vs_FieldCapacity(),
                            soilColumn[0].vs_PermanentWiltingPoint());

  if (old_DevelopmentalStage == 0 && vc_DevelopmentalStage == 1) {
    if (_fireEvent) _fireEvent("emergence");
  } else if (isAnthesisDay(old_DevelopmentalStage, vc_DevelopmentalStage)) {
    vc_AnthesisDay = vs_JulianDay;
    if (_fireEvent) _fireEvent("anthesis");
  } else if (isMaturityDay(old_DevelopmentalStage, vc_DevelopmentalStage)) {
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
      fc_DaylengthFactor(pc_DaylengthRequirement[vc_DevelopmentalStage],
                         vc_EffectiveDayLength,
                         vc_PhotoperiodicDaylength,
                         pc_BaseDaylength[vc_DevelopmentalStage]);

  tie(vc_VernalisationFactor, vc_VernalisationDays) =
      fc_VernalisationFactor(vw_MeanAirTemperature,
                             pc_VernalisationRequirement[vc_DevelopmentalStage],
                             vc_VernalisationDays);

  if (vc_TotalTemperatureSum == 0.0) {
    vc_RelativeTotalDevelopment = 0.0;
  } else {
    vc_RelativeTotalDevelopment = vc_CurrentTotalTemperatureSum / vc_TotalTemperatureSum;
  }

  if (vc_DevelopmentalStage == 0) {
    vc_KcFactor = 0.4; /** @todo Claas: muss hier etwas Genaueres hin, siehe FAO? */
  } else {
    vc_KcFactor = fc_KcFactor(pc_StageTemperatureSum[vc_DevelopmentalStage],
                              vc_CurrentTemperatureSum[vc_DevelopmentalStage],
                              pc_StageKcFactor[vc_DevelopmentalStage],
                              pc_StageKcFactor[vc_DevelopmentalStage - 1]);
  }

  auto icSendRcv = [&](const string &outmsg) {
    if (cropPs.isIntercropping && _intercropping.isAsync()) {
      debug() << outmsg;
      // tell the other side our current crop height
      auto wreq = _intercropping.writer.writeRequest();
      auto wval = wreq.initValue();
      wval.setHeight(vc_CropHeight);
      auto prom = wreq.send();//.wait(_intercropping.ioContext->waitScope); //.eagerlyEvaluate(nullptr); //[](kj::Exception&& ex){ cout << "crop-module: CropModule::fc_CropPhotosynthesis: write height failed: " << ex.getDescription().cStr() << endl;});
      auto val = _intercropping.reader.readRequest().send().wait(_intercropping.ioContext->waitScope).getValue();
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
    auto maxCropHeight = cropPs.isIntercropping
                         && _intercroppingOtherCropHeight > vc_CropHeight
                         ? pc_MaxCropHeight * cropPs.pc_intercropping_phRedux
                         : pc_MaxCropHeight;
    debug() << "original maxCropHeight: " << pc_MaxCropHeight << " -> new maxCropHeight: " << maxCropHeight << endl;

    fc_CropSize(maxCropHeight);

    icSendRcv("devstage > 0: ");

    fc_CropGreenArea(vw_MeanAirTemperature,
                     vc_OrganGrowthIncrement[1],
                     vc_OrganSenescenceIncrement[1],
                     pc_SpecificLeafArea[vc_DevelopmentalStage - 1],
                     pc_SpecificLeafArea[vc_DevelopmentalStage],
                     pc_SpecificLeafArea[1],
                     pc_StageTemperatureSum[vc_DevelopmentalStage],
                     vc_CurrentTemperatureSum[vc_DevelopmentalStage]);

    vc_SoilCoverage = fc_SoilCoverage();

    fc_CropPhotosynthesis(vw_MeanAirTemperature,
                          vw_MaxAirTemperature,
                          vw_MinAirTemperature,
                          vw_AtmosphericCO2Concentration,
                          vw_AtmosphericO3Concentration,
                          currentDate);

    fc_HeatStressImpact(vw_MaxAirTemperature,
                        vw_MinAirTemperature);

    if (_frostKillOn) {
      fc_FrostKill(vw_MaxAirTemperature,
                   vw_MinAirTemperature);
    }

    fc_DroughtImpactOnFertility();

    fc_CropNitrogen();

    fc_CropDryMatter(vw_MeanAirTemperature);

    // calculate reference evapotranspiration if not provided directly via climate files
    if (vw_ReferenceEvapotranspiration < 0) {
      vc_ReferenceEvapotranspiration = fc_ReferenceEvapotranspiration(vw_MaxAirTemperature,
                                                                      vw_MinAirTemperature,
                                                                      vw_RelativeHumidity,
                                                                      vw_MeanAirTemperature,
                                                                      vw_WindSpeed,
                                                                      vw_WindSpeedHeight,
                                                                      vw_AtmosphericCO2Concentration);
    } else {
      // use reference evapotranspiration from climate file
      vc_ReferenceEvapotranspiration = vw_ReferenceEvapotranspiration;
    }
    fc_CropWaterUptake(soilColumn.vm_GroundwaterTableLayer,
                       vw_GrossPrecipitation,
                       vc_CurrentTotalTemperatureSum,
                       vc_TotalTemperatureSum);

    fc_CropNUptake(soilColumn.vm_GroundwaterTableLayer,
                   vc_CurrentTotalTemperatureSum,
                   vc_TotalTemperatureSum);

    vc_GrossPrimaryProduction = fc_GrossPrimaryProduction();

    vc_NetPrimaryProduction = fc_NetPrimaryProduction(vc_TotalRespired);
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
void CropModule::fc_Radiation(double vs_JulianDay,
                              double vw_GlobalRadiation,
                              double vw_SunshineHours) {

  double vc_DeclinationSinus = 0.0;  // old SINLD
  double vc_DeclinationCosinus = 0.0; // old COSLD

  // Calculation of declination - old DEC
  vc_Declination = -23.4 * cos(2.0 * PI * ((vs_JulianDay + 10.0) / 365.0));

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
  double pc_SolarConstant = 0.082; //[MJ m-2 d-1] Note: Here is the difference to HERMES, which calculates in [J cm-2 d-1]!
  double SC = 24.0 * 60.0 / PI * pc_SolarConstant * (1.0 + 0.033 * cos(2.0 * PI * vs_JulianDay / 365.0));

  double arg_SolarAngle = -tan(vs_Latitude * PI / 180.0) * tan(vc_Declination * PI / 180.0);
  arg_SolarAngle = bound(-1.0, arg_SolarAngle, 1.0);
  double vc_SunsetSolarAngle = acos(arg_SolarAngle);
  vc_ExtraterrestrialRadiation =
      SC * (vc_SunsetSolarAngle * vc_DeclinationSinus + vc_DeclinationCosinus * sin(vc_SunsetSolarAngle)); // [MJ m-2]

  if (vw_GlobalRadiation > 0.0) {
    vc_GlobalRadiation = vw_GlobalRadiation;
  } else if (vc_AstronomicDayLenght > 0) {
    vc_GlobalRadiation = vc_ExtraterrestrialRadiation *
                         (0.19 + 0.55 * vw_SunshineHours / vc_AstronomicDayLenght);
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
double CropModule::fc_DaylengthFactor(double d_DaylengthRequirement, double vc_EffectiveDayLength,
                                      double vc_PhotoperiodicDayLength, double d_BaseDaylength) {
  if (d_DaylengthRequirement > 0.0) {

    // ************ Long-day plants **************
    // * Development acceleration by day length. *
    // *  (Day lenght requirement is positive.)  *
    // *******************************************

    vc_DaylengthFactor = (vc_PhotoperiodicDaylength - d_BaseDaylength) / (d_DaylengthRequirement - d_BaseDaylength);
  } else if (d_DaylengthRequirement < 0.0) {

    // ************* Short-day plants **************
    // * Development acceleration by night lenght. *
    // *  (Day lenght requirement is negative and  *
    // *    represents critical day length.)   *
    // *********************************************

    double vc_CriticalDayLenght = -d_DaylengthRequirement;
    double vc_MaximumDayLength = -d_BaseDaylength;
    if (vc_EffectiveDayLength <= vc_CriticalDayLenght) {
      vc_DaylengthFactor = 1.0;
    } else {
      vc_DaylengthFactor = (vc_EffectiveDayLength - vc_MaximumDayLength) / (vc_CriticalDayLenght - vc_MaximumDayLength);
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
pair<double, double> CropModule::fc_VernalisationFactor(double vw_MeanAirTemperature,
                                                        double d_VernalisationRequirement,
                                                        double d_VernalisationDays) {
  double vc_EffectiveVernalisation;

  if (d_VernalisationRequirement == 0.0) {
    vc_VernalisationFactor = 1.0;
  } else {
    if ((vw_MeanAirTemperature > -4.0) && (vw_MeanAirTemperature <= 0.0)) {
      vc_EffectiveVernalisation = (vw_MeanAirTemperature + 4.0) / 4.0;
    } else if ((vw_MeanAirTemperature > 0.0) && (vw_MeanAirTemperature <= 3.0)) {
      vc_EffectiveVernalisation = 1.0;
    } else if ((vw_MeanAirTemperature > 3.0) && (vw_MeanAirTemperature <= 7.0)) {
      vc_EffectiveVernalisation = 1.0 - (0.2 * (vw_MeanAirTemperature - 3.0) / 4.0);
    } else if ((vw_MeanAirTemperature > 7.0) && (vw_MeanAirTemperature <= 9.0)) {
      vc_EffectiveVernalisation = 0.8 - (0.4 * (vw_MeanAirTemperature - 7.0) / 2.0);
    } else if ((vw_MeanAirTemperature > 9.0) && (vw_MeanAirTemperature <= 18.0)) {
      vc_EffectiveVernalisation = 0.4 - (0.4 * (vw_MeanAirTemperature - 9.0) / 9.0);
    } else if ((vw_MeanAirTemperature <= -4.0) || (vw_MeanAirTemperature > 18.0)) {
      vc_EffectiveVernalisation = 0.0;
    } else {
      vc_EffectiveVernalisation = 1.0;
    }

    // old VERNTAGE
    d_VernalisationDays += vc_EffectiveVernalisation * vc_TimeStep;

    // old VERSCHWELL
    double vc_VernalisationThreshold = min(d_VernalisationRequirement, 9.0) - 1.0;

    if (vc_VernalisationThreshold >= 1) {
      vc_VernalisationFactor = (d_VernalisationDays - vc_VernalisationThreshold) / (d_VernalisationRequirement
                                                                                    - vc_VernalisationThreshold);

      if (__enable_vernalisation_factor_fix__) vc_VernalisationFactor = min(max(0.0, vc_VernalisationFactor), 1.0);
      if (vc_VernalisationFactor < 0) vc_VernalisationFactor = 0.0;
    } else {
      vc_VernalisationFactor = 1.0;
    }
  }

  return make_pair(vc_VernalisationFactor, d_VernalisationDays);
}

/*!
 * @brief Calculation of oxygen deficiency
 *
 * @returns Oxygen deficiency factor.
 *
 * @author Claas Nendel
 */
double CropModule::fc_OxygenDeficiency(double d_CriticalOxygenContent) {
  double vc_AirFilledPoreVolume = 0.0;
  double vc_MaxOxygenDeficit = 0.0;

  // Reduktion bei Luftmangel Stauwasser berücksichtigen!!!!
  vc_AirFilledPoreVolume =
      ((soilColumn[0].vs_Saturation() + soilColumn[1].vs_Saturation() + soilColumn[2].vs_Saturation()) -
       (soilColumn[0].get_Vs_SoilMoisture_m3() + soilColumn[1].get_Vs_SoilMoisture_m3() +
        soilColumn[2].get_Vs_SoilMoisture_m3())) / 3.0;
  if (vc_AirFilledPoreVolume < d_CriticalOxygenContent) {
    vc_TimeUnderAnoxia += int(vc_TimeStep);
    if (vc_TimeUnderAnoxia > 4) {
      vc_TimeUnderAnoxia = 4;
    }
    if (vc_AirFilledPoreVolume < 0.0) {
      vc_AirFilledPoreVolume = 0.0;
    }
    vc_MaxOxygenDeficit = vc_AirFilledPoreVolume / d_CriticalOxygenContent;
    vc_OxygenDeficit = 1.0 - double(vc_TimeUnderAnoxia / 4) * (1.0 - vc_MaxOxygenDeficit);
  } else {
    vc_TimeUnderAnoxia = 0;
    vc_OxygenDeficit = 1.0;
  }
  if (vc_OxygenDeficit > 1.0) {
    vc_OxygenDeficit = 1.0;
  }

  return vc_OxygenDeficit;
}

double WangEngelTemperatureResponse(double t, double tmin, double topt, double tmax, double betacoeff) {
  // prevent nan values with t < tmin
  if (t < tmin || t > tmax) return 0.0;

  double alfa = log(2) / log((tmax - tmin) / (topt - tmin));
  double numerator = 2 * pow(t - tmin, alfa) * pow(topt - tmin, alfa) - pow(t - tmin, 2 * alfa);
  double denominator = pow(topt - tmin, 2 * alfa);

  return pow(numerator / denominator, betacoeff);
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
void CropModule::fc_CropDevelopmentalStage(double meanAirTemperature,
                                           double soilMoisture_m3,
                                           double fieldCapacity,
                                           double permanentWiltingPoint) {
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
      double vc_SoilTemperature = soilColumn[0].get_Vs_SoilTemperature();
      if (vc_SoilTemperature > pc_BaseTemperature[vc_DevelopmentalStage]) {

        // @todo Claas: Schränkt trockener Boden das Aufsummieren der Wärmeeinheiten ein, oder
        // sollte nicht eher nur der Wechsel in das Stadium 1 davon abhängen? --> Christian

        bool emergenceCondition = true;
        // Germination only if soil water content in top layer exceeds
        // 20% of capillary water, but is not beyond field capacity
        if (pc_EmergenceMoistureControlOn) {
          double vc_CapillaryWater = fieldCapacity - permanentWiltingPoint;
          emergenceCondition = emergenceCondition
                               && soilMoisture_m3 > ((0.2 * vc_CapillaryWater) + permanentWiltingPoint)
                               && soilMoisture_m3 <= fieldCapacity;
        }
        // Germination only if no water is stored on the soil surface.
        if (pc_EmergenceFloodingControlOn) {
          emergenceCondition = emergenceCondition && soilColumn.vs_SurfaceWaterStorage < 0.001;
        }

        if (emergenceCondition) {
          vc_CurrentTemperatureSum[vc_DevelopmentalStage] +=
              (vc_SoilTemperature - pc_BaseTemperature[vc_DevelopmentalStage]) * vc_TimeStep;

          if (vc_CurrentTemperatureSum[vc_DevelopmentalStage] >= pc_StageTemperatureSum[vc_DevelopmentalStage]) {
            double vc_StageExcessTemperatureSum =
                vc_CurrentTemperatureSum[vc_DevelopmentalStage] - pc_StageTemperatureSum[vc_DevelopmentalStage];
            if (vc_DevelopmentalStage < pc_NumberOfDevelopmentalStages - 1) {
              vc_DevelopmentalStage++;
              vc_CurrentTemperatureSum[vc_DevelopmentalStage] += vc_StageExcessTemperatureSum;
            }
          }
        }
      }
    }
  } else if (vc_DevelopmentalStage > 0) {
    auto apc = pc_AssimilatePartitioningCoeff[vc_DevelopmentalStage][vc_StorageOrgan];

    // Development acceleration by N deficit in crop tissue
    double vc_DevelopmentAccelerationByNitrogenStress = 1; // old NPROG
    if (pc_DevelopmentAccelerationByNitrogenStress == 1 && apc > 0.9) {
      vc_DevelopmentAccelerationByNitrogenStress = 1.0 + ((1.0 - vc_CropNRedux) * (1.0 - vc_CropNRedux));
    }

    // Development acceleration by water deficit
    double vc_DevelopmentAccelerationByWaterStress = 1; // old WPROG
    if (vc_TranspirationDeficit < pc_DroughtStressThreshold[vc_DevelopmentalStage] && apc > 0.9) {
      if (vc_OxygenDeficit >= 1.0) {
        vc_DevelopmentAccelerationByWaterStress =
            1.0 + ((1.0 - vc_TranspirationDeficit) * (1.0 - vc_TranspirationDeficit));
      }
    }

    // old DEVPROG
    double vc_DevelopmentAccelerationByStress =
        max(vc_DevelopmentAccelerationByNitrogenStress, vc_DevelopmentAccelerationByWaterStress);

    if (cropPs.__enable_Phenology_WangEngelTemperatureResponse__) {
      double devTresponse = max(0.0, WangEngelTemperatureResponse(meanAirTemperature,
                                                                  cultivarPs.pc_MinTempDev_WE,
                                                                  cultivarPs.pc_OptTempDev_WE,
                                                                  cultivarPs.pc_MaxTempDev_WE,
                                                                  1.0));
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
        vc_CurrentTemperatureSum[vc_DevelopmentalStage] += tempIncr;
        vc_CurrentTotalTemperatureSum += tempIncr;
      }
    }

    if (vc_CurrentTemperatureSum[vc_DevelopmentalStage] >= pc_StageTemperatureSum[vc_DevelopmentalStage]) {
      double vc_StageExcessTemperatureSum =
          vc_CurrentTemperatureSum[vc_DevelopmentalStage] - pc_StageTemperatureSum[vc_DevelopmentalStage];

      if (vc_DevelopmentalStage < pc_NumberOfDevelopmentalStages - 1) {
        vc_DevelopmentalStage++;
        vc_CurrentTemperatureSum[vc_DevelopmentalStage] += vc_StageExcessTemperatureSum;
      } else if (vc_DevelopmentalStage == pc_NumberOfDevelopmentalStages - 1) {
        vc_StageExcessTemperatureSum = 0.0;
        if (pc_Perennial && vc_GrowthCycleEnded) {
          vc_DevelopmentalStage = 0;
          fc_UpdateCropParametersForPerennial();
          for (int stage = 0; stage < pc_NumberOfDevelopmentalStages; stage++) vc_CurrentTemperatureSum[stage] = 0.0;
          vc_CurrentTotalTemperatureSum = 0.0;
          vc_GrowthCycleEnded = false;
        }
      }
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
 * @author Claas Nendel
 */
double CropModule::fc_KcFactor(double d_StageTemperatureSum,
                               double d_CurrentTemperatureSum,
                               double d_StageKcFactor,      // DB
                               double d_EarlierStageKcFactor) const // DB
{
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
void CropModule::fc_CropSize(double maxCropHeight) {
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
void CropModule::fc_CropGreenArea(double vw_MeanAirTemperature,
                                  double d_LeafBiomassIncrement,
                                  double d_LeafBiomassDecrement,
                                  double d_SpecificLeafAreaStart,
                                  double d_SpecificLeafAreaEnd,
                                  double d_SpecificLeafAreaEarly,
                                  double d_StageTemperatureSum,
                                  double d_CurrentTemperatureSum) {
  double TempResponseExpansion = 1.0;
  if (cropPs.__enable_T_response_leaf_expansion__) {
    // Stage switch T response leaf exp (wheat = 2, maize = -1 (deactivated))
    if (vc_DevelopmentalStage + 1 <= speciesPs.pc_TransitionStageLeafExp) {
      // Early stages leaf expansion T response
      //!!!! maybe referenceTempResponseExpansion calculation should be moved to the constructor because it has to be calculated just once per crop
      double referenceTempResponseExpansion = 223.9 * exp(-5.03 * exp(-0.0653 * cultivarPs.pc_EarlyRefLeafExp));
      TempResponseExpansion = std::min(
          223.9 * exp(-5.03 * exp(-0.0653 * vw_MeanAirTemperature)) / referenceTempResponseExpansion, 1.3);
    } else {
      // leaf expansion T response
      double referenceTempResponseExpansion = 37.7 * exp(-7.23 * exp(-0.1462 * cultivarPs.pc_RefLeafExp));
      TempResponseExpansion = std::min(
          37.7 * exp(-7.23 * exp(-0.1462 * vw_MeanAirTemperature)) / referenceTempResponseExpansion, 1.3);
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
double CropModule::fc_SoilCoverage() const {
  return 1.0 - (exp(-0.5 * vc_LeafAreaIndex));
}

#ifdef TEST_HOURLY_OUTPUT
#include <fstream>
ostream &monica::tout(bool closeFile)
{
  static ofstream out;
  static bool init = false;
  static bool failed = false;
  if (closeFile)
  {
    init = false;
    failed = false;
    out.close();
    return out;
  }

  if (!init)
  {
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
                                              const vector<double> &vc_RootDensityFactor) {
  auto nools = soilColumn.vs_NumberOfOrganicLayers();

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

  double pc_ReferenceLeafAreaIndex = cropPs.pc_ReferenceLeafAreaIndex;
  double pc_ReferenceMaxAssimilationRate = cropPs.pc_ReferenceMaxAssimilationRate;
  double pc_MaintenanceRespirationParameter_1 = cropPs.pc_MaintenanceRespirationParameter1;
  double pc_MaintenanceRespirationParameter_2 = cropPs.pc_MaintenanceRespirationParameter2;

  double pc_GrowthRespirationParameter_1 = cropPs.pc_GrowthRespirationParameter1;
  double pc_GrowthRespirationParameter_2 = cropPs.pc_GrowthRespirationParameter2;
  double pc_CanopyReflectionCoeff = cropPs.pc_CanopyReflectionCoefficient; // old REFLC;

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
      double Mko = speciesPs.KO25 * vc_KTko;      //[mmol mol-1]
      _cropPhotosynthesisResults.ko = Mko * 1000.0; // mmol -> umol

      // OLD exponential response
      double KTvmax = cropPs.__enable_Photosynthesis_WangEngelTemperatureResponse__
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
                                      1000.0;                                                                                              // mmol -> umol

      double Ci = vw_AtmosphericCO2Concentration * 0.7 * (1.674 - 0.061294 * vw_MeanAirTemperature +
                                                          0.0011688 * (vw_MeanAirTemperature * vw_MeanAirTemperature) -
                                                          0.0000088741 *
                                                          (vw_MeanAirTemperature * vw_MeanAirTemperature *
                                                           vw_MeanAirTemperature)) / 0.73547; // [µmol mol-1]
      _cropPhotosynthesisResults.ci = Ci;

      // similar to LDNDC::jarvis.cpp:217
      //  old COcomp
      double vc_CO2CompensationPoint =
          0.5 * 0.21 * vc_Vcmax * Mkc * Oi / (vc_Vcmax * Mko);               // [µmol mol-1]
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
        vc_AssimilationRate = 0.0;
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

    double vc_ClearDayCO2Assimilation = LAI < 5.0 ? PHCL : PHCH;  // [J m-2]
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
      vc_GrossCO2Assimilation = vc_GrossCO2Assimilation; // *  vc_TranspirationDeficit;
    }

#pragma region hourly FvCB code
    int vs_JulianDay = currentDate.julianDay();
    double dailyGP = 0;
    if (cropPs.__enable_hourly_FvCB_photosynthesis__ && pc_CarboxylationPathway == 1) {
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
        O3_par.gamma3 = 0.05;  // TODO: calibrate and add to crop params
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
            FC += soilColumn[i].vs_FieldCapacity();
            WP += soilColumn[i].vs_PermanentWiltingPoint();
            SWC += soilColumn[i].get_Vs_SoilMoisture_m3();
          }

          // weighted average gs and conversion from unit ground area to unit leaf area
          double lai_sun_weight = FvCB_res.sunlit.LAI / (FvCB_res.sunlit.LAI + FvCB_res.shaded.LAI);
          double lai_sh_weight = 1 - lai_sun_weight;
          double avg_leaf_gs = lai_sh_weight * FvCB_res.shaded.gs / FvCB_res.shaded.LAI;
          if (FvCB_res.sunlit.LAI > 0) {
            avg_leaf_gs += lai_sun_weight * FvCB_res.sunlit.gs / FvCB_res.sunlit.LAI;
          }

          O3_in.FC = FC / (root_depth + 1);  // field capacity, m3 m-3, avg in the rooted zone
          O3_in.WP = WP / (root_depth + 1);  // wilting point, m3 m-3
          O3_in.SWC = SWC / (root_depth + 1); // soil water content, m3 m-3
          O3_in.ET0 = get_ReferenceEvapotranspiration();
          O3_in.O3a = vw_AtmosphericO3Concentration; // ambient O3 partial pressure, nbar or nmol mol-1
          O3_in.gs = avg_leaf_gs;             // stomatal conductance mol m-2 s-1 bar-1
          O3_in.h = h;                 // hour of the day (0-23)
          O3_in.reldev = vc_RelativeTotalDevelopment;
          O3_in.GDD_flo = vc_TemperatureSumToFlowering; // GDD from emergence to flowering
          O3_in.GDD_mat = vc_TotalTemperatureSum;      // GDD from emergence to maturity
          O3_in.fO3s_d_prev = vc_O3_shortTermDamage;    // short term ozone induced reduction of Ac of the previous time step
          O3_in.sum_O3_up = vc_O3_sumUptake;        // cumulated O3 uptake, µmol m-2 (unit ground area)

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
        species.mFol = get_OrganGreenBiomass(LEAF) / (100. * 100.);                                // kg/ha -> kg/m2
        species.sla =
            species.mFol > 0 ? species.lai / species.mFol : pc_SpecificLeafArea[vc_DevelopmentalStage] * 100. *
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
        for (const auto &lf: {FvCB_res.sunlit, FvCB_res.shaded}) {
          species.lai = lf.LAI;
          species.mFol = get_OrganGreenBiomass(LEAF) / (100. * 100.) * lf.LAI /
                         (sun_LAI + sh_LAI);                // kg/ha -> kg/m2
          species.sla =
              species.mFol > 0 ? species.lai / species.mFol : pc_SpecificLeafArea[vc_DevelopmentalStage] * 100. *
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
                                             vc_TranspirationDeficit; // lf.vcMax;
          _cropPhotosynthesisResults.jMax =
              FvCB::Jmax_bernacchi_f(mcd.tFol, 120) * vc_CropNRedux * vc_TranspirationDeficit;           // lf.jMax;
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

    vc_GrossCO2Assimilation = cropPs.__enable_hourly_FvCB_photosynthesis__ && pc_CarboxylationPathway == 1
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
    auto F_t1 = [](double LAI) {
      return 1.0 - exp(-0.8 * LAI);
    };
    tie(vc_GrossCO2Assimilation, vc_GrossCO2AssimilationReference) = code(F_t1, vc_LeafAreaIndex);
    fractionOfInterceptedRadiation1 = F_t1(vc_LeafAreaIndex);
    debug() << "assimilation calculations for only one crop: grossCO2Assim: " << vc_GrossCO2Assimilation
            << " ref: " << vc_GrossCO2AssimilationReference << endl;
  } else {
    double k_t = cropPs.pc_intercropping_k_t;
    double k_s = cropPs.pc_intercropping_k_s;
    double phRedux = cropPs.pc_intercropping_phRedux;
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
      if (_intercropping.isAsync()) {
        auto wreq = _intercropping.writer.writeRequest();
        auto wval = wreq.initValue();
        wval.setLait(vc_LeafAreaIndex);
        auto prom = wreq.send();//.wait(_intercropping.ioContext->waitScope); //.eagerlyEvaluate(nullptr);//[](kj::Exception&& ex){ cout << "crop-module: CropModule::fc_CropPhotosynthesis: write LAI failed: " << ex.getDescription().cStr() << endl;});
        auto val = _intercropping.reader.readRequest().send().wait(_intercropping.ioContext->waitScope).getValue();
        LAI_t2 = val.isLait() ? val.getLait()
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
      if (_intercropping.isAsync()) {
        auto wreq = _intercropping.writer.writeRequest();
        auto wval = wreq.initValue();
        wval.setLait(LAI_t2);
        auto prom = wreq.send();//.wait(_intercropping.ioContext->waitScope); //.eagerlyEvaluate(nullptr);//[](kj::Exception&& ex){ cout << "crop-module: CropModule::fc_CropPhotosynthesis: write LAI failed: " << ex.getDescription().cStr() << endl;});
        auto val = _intercropping.reader.readRequest().send().wait(_intercropping.ioContext->waitScope).getValue();
        LAI_s = val.isLait() ? val.getLait()
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

  if (vc_TranspirationDeficit < vc_DroughtStressThreshold) {
    // vc_Assimilates = vc_Assimilates * vc_TranspirationDeficit;
    vc_Assimilates = vc_Assimilates * vc_TranspirationDeficit / vc_DroughtStressThreshold;
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
  vc_TotalRespired = vc_GrossAssimilates - vc_Assimilates;             // [kg CH2O ha-1]

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
 * @brief Heat stress impact
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
        (3.0 * soilColumn.vt_SoilSurfaceTemperature + 2.0 * soilColumn[0].get_Vs_SoilTemperature()) / 5.0;
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
  if (soilColumn.vm_SnowDepth <= 125.0) vc_SnowDepthFactor = soilColumn.vm_SnowDepth / 125.0;

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
 *
 * @param vc_CurrentTotalTemperatureSum
 */
void CropModule::fc_CropNitrogen() {
  double vc_RootNRedux = 0.0;      // old REDWU
  double vc_RootNReduxHelper = 0.0; // old WUX
  double vc_CropNReduxHelper = 0.0; // old AUX

  vc_CriticalNConcentration = pc_NConcentrationPN *
                              (1.0 + (pc_NConcentrationB0 *
                                      exp(-0.26 * (vc_AbovegroundBiomass + vc_BelowgroundBiomass) / 1000.0))) /
                              100.0;
  // [kg ha-1 -> t ha-1]

  vc_TargetNConcentration = vc_CriticalNConcentration * pc_LuxuryNCoeff;

  vc_NConcentrationAbovegroundBiomassOld = vc_NConcentrationAbovegroundBiomass;
  vc_NConcentrationRootOld = vc_NConcentrationRoot;

  if (vc_NConcentrationRoot < 0.01) {

    if (vc_NConcentrationRoot <= 0.005) {
      vc_RootNRedux = 0.0;
    } else {

      vc_RootNReduxHelper = (vc_NConcentrationRoot - 0.005) / 0.005;
      vc_RootNRedux = 1.0 - sqrt(1.0 - vc_RootNReduxHelper * vc_RootNReduxHelper);
    }
  } else {
    vc_RootNRedux = 1.0;
  }

  if (vc_NConcentrationAbovegroundBiomass < vc_CriticalNConcentration) {

    if (vc_NConcentrationAbovegroundBiomass <= pc_MinimumNConcentration) {
      vc_CropNRedux = 0.0;
    } else {

      vc_CropNReduxHelper = (vc_NConcentrationAbovegroundBiomass - pc_MinimumNConcentration) /
                            (vc_CriticalNConcentration - pc_MinimumNConcentration);

      //     // New Monica appraoch
      vc_CropNRedux = 1.0 - exp(pc_MinimumNConcentration - (5.0 * vc_CropNReduxHelper));

      //    // Original HERMES approach
      //    vc_CropNRedux = (1.0 - exp(1.0 + 1.0 / (vc_CropNReduxHelper - 1.0))) *
      //          (1.0 - exp(1.0 + 1.0 / (vc_CropNReduxHelper - 1.0)));
    }
  } else {
    vc_CropNRedux = 1.0;
  }

  if (pc_NitrogenResponseOn == false) {
    vc_CropNRedux = 1.0;
  }
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
  assert(soilColumn.vs_NumberOfLayers() >= 0);
  auto nols = soilColumn.vs_NumberOfLayers();
  double layerThickness = soilColumn.vs_LayerThickness();

  double vc_MaxRootNConcentration = 0.0; // old WGM
  double vc_NConcentrationOptimum = 0.0; // old DTOPTN
  double vc_RootNIncrement = 0.0;       // old WUMM
  double vc_AssimilatePartitioningCoeffOld = 0.0;
  double vc_AssimilatePartitioningCoeff = 0.0;
  // double vc_RootDistributionFactor          = 0.0; // old QREZ
  // double vc_SoilDepth                 = 0.0; // old TIEFE
  // std::vector<double> vc_RootLengthToLayer(nols, 0.0);	// old WULAE
  // std::vector<double> vc_RootLengthInLayer(nols, 0.0);	// old WULAE2
  //   std::vector<double> vc_CapillaryWater(nols, 0.0);
  // std::vector<double> vc_RootSurface(nols, 0.0); // old FL

  const CropModuleParameters &user_crops = cropPs;
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
  // vc_TotalBiomassNContent += (soilColumn.vq_CropNUptake * 10000.0) + vc_FixedN;

  // Dry matter production
  // old NRKOM
  // double assimilate_partition_shoot = 0.7;
  double assimilate_partition_leaf = 0.05;
  vector<double> dailyDeadBiomassIncrement(pc_NumberOfOrgans, 0.0);
  for (int i_Organ = 0; i_Organ < pc_NumberOfOrgans; i_Organ++) {
    vc_AssimilatePartitioningCoeffOld = pc_AssimilatePartitioningCoeff[vc_DevelopmentalStage - 1][i_Organ];
    vc_AssimilatePartitioningCoeff = pc_AssimilatePartitioningCoeff[vc_DevelopmentalStage][i_Organ];

    // Identify storage organ and reduce assimilate flux in case of heat stress
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
        // reduce biomass from leaf and shoot because of negative assimilate
        //! TODO: hard coded organ ids; must be more generalized because in database organ_ids can be mixed
        // vc_OrganBiomass[i_Organ];

        if (i_Organ == LEAF) { // leaf

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
        } else if (i_Organ == SHOOT) { // shoot

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
          // root or storage organ - do nothing in case of negative photosynthesis
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
          vc_OrganGreenBiomass[i_Organ] * (pc_OrganSenescenceRate[vc_DevelopmentalStage - 1][i_Organ] +
                                           ((pc_OrganSenescenceRate[vc_DevelopmentalStage][i_Organ] -
                                             pc_OrganSenescenceRate[vc_DevelopmentalStage - 1][i_Organ]) *
                                            (vc_CurrentTemperatureSum[vc_DevelopmentalStage] /
                                             pc_StageTemperatureSum[vc_DevelopmentalStage]))); // [kg CH2O ha-1]
    }

    if (i_Organ != vc_StorageOrgan) {
      // Wurzel, Sprossachse, Blatt
      vc_OrganBiomass[i_Organ] += (vc_OrganGrowthIncrement[i_Organ] * vc_TimeStep);                // [kg CH2O ha-1]
      double reallocationRate =
          pc_AssimilateReallocation * vc_OrganSenescenceIncrement[i_Organ] * vc_TimeStep; // [kg CH2O ha-1]
      vc_OrganBiomass[i_Organ] -= reallocationRate;
      dailyDeadBiomassIncrement[i_Organ] = vc_OrganSenescenceIncrement[i_Organ] - reallocationRate;
      vc_OrganDeadBiomass[i_Organ] += dailyDeadBiomassIncrement[i_Organ]; // [kg CH2O ha-1]
      vc_OrganBiomass[vc_StorageOrgan] += reallocationRate;

      // update the root biomass and dead root biomass vars
      // root dead biomass will be transfered to proper AOM pools
      if (i_Organ == ROOT) {
        vc_OrganBiomass[ROOT] -= dailyDeadBiomassIncrement[ROOT];
        vc_OrganDeadBiomass[ROOT] -= dailyDeadBiomassIncrement[ROOT];
        vc_TotalBiomassNContent -= dailyDeadBiomassIncrement[ROOT] * vc_NConcentrationRoot;
      }
    } else {
      vc_OrganBiomass[i_Organ] += (vc_OrganGrowthIncrement[i_Organ] * vc_TimeStep);    // [kg CH2O ha-1]
      vc_OrganDeadBiomass[i_Organ] += vc_OrganSenescenceIncrement[i_Organ] * vc_TimeStep; // [kg CH2O ha-1]
    }

    vc_OrganGreenBiomass[i_Organ] = vc_OrganBiomass[i_Organ] - vc_OrganDeadBiomass[i_Organ]; // [kg CH2O ha-1]
    if ((vc_OrganGreenBiomass[i_Organ]) < 0.0) {
      vc_OrganDeadBiomass[i_Organ] = vc_OrganBiomass[i_Organ];
      vc_OrganGreenBiomass[i_Organ] = 0.0;
    }

    if (pc_AbovegroundOrgan[i_Organ]) {
      vc_AbovegroundBiomass += vc_OrganBiomass[i_Organ]; // [kg CH2O ha-1]
    } else if (!pc_AbovegroundOrgan[i_Organ] && i_Organ > 0) {
      vc_BelowgroundBiomass += vc_OrganBiomass[i_Organ];
    } // [kg CH2O ha-1]

    vc_TotalBiomass += vc_OrganBiomass[i_Organ]; // [kg CH2O ha-1]
  }

  /** @todo N redux noch ausgeschaltet */
  vc_ReserveAssimilatePool = 0.0; //+= vc_NetPhotosynthesis * (1.0 - vc_CropNRedux);
  vc_RootBiomassOld = vc_RootBiomass;
  vc_RootBiomass = vc_OrganBiomass[0];

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

  // In case of drought stress the root will grow deeper
  if (vc_TranspirationDeficit < (0.95 * pc_DroughtStressThreshold[vc_DevelopmentalStage])
      && pc_CropSpecificMaxRootingDepth >= 0.8 // only if the crop specific max rooting depth is deeper than 80 cm
      && vc_RootingDepth_m > 0.95 * vc_MaxRootingDepth
      && vc_DevelopmentalStage < (pc_NumberOfDevelopmentalStages - 1)) {
    vc_MaxRootingDepth += 0.005;
  }

  if (vc_MaxRootingDepth > (double(nols - 1) * layerThickness)) {
    vc_MaxRootingDepth = double(nols - 1) * layerThickness;
  }

  // restrict rootgrowth to everything above impentrable layer
  if (vs_ImpenetrableLayerDepth > 0) {
    vc_MaxRootingDepth = min(vc_MaxRootingDepth, vs_ImpenetrableLayerDepth);
  }

  // ***************************************************************************
  // *** Taken from Pedersen et al. 2010: Modelling diverse root density   ***
  // *** dynamics and deep nitrogen uptake - a simple approach.        ***
  // *** Plant & Soil 326, 493 - 510                     ***
  // ***************************************************************************

  // Determining temperature sum for root growth
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

  // Determining root penetration rate according to soil clay content [m °C-1 d-1]
  double vc_RootPenetrationRate = 0.0; // [m °C-1 d-1]
  if (soilColumn[vc_RootingDepth].vs_SoilClayContent() <= 0.02) {
    vc_RootPenetrationRate = 0.5 * pc_RootPenetrationRate;
  } else if (soilColumn[vc_RootingDepth].vs_SoilClayContent() <= 0.08) {
    vc_RootPenetrationRate = ((1.0 / 3.0) + (0.5 / 0.06 * soilColumn[vc_RootingDepth].vs_SoilClayContent())) *
                             pc_RootPenetrationRate; // [m °C-1 d-1]
  } else {
    vc_RootPenetrationRate = pc_RootPenetrationRate; // [m °C-1 d-1]
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

  if (vc_RootingDepth_m <= pc_InitialRootingDepth) {
    vc_RootingDepth_m = pc_InitialRootingDepth;
  }

  if (vc_RootingDepth_m > vc_MaxRootingDepth) {
    vc_RootingDepth_m = vc_MaxRootingDepth;
  } // [m]

  if (vc_RootingDepth_m > vs_MaxEffectiveRootingDepth) {
    vc_RootingDepth_m = vs_MaxEffectiveRootingDepth;
  }

  // Calculating rooting depth layer []
  vc_RootingDepth = int(std::floor(0.5 + (vc_RootingDepth_m / layerThickness))); // []
  if (vc_RootingDepth > nols) {
    vc_RootingDepth = nols;
  }

  vc_RootingZone = int(std::floor(0.5 + ((1.3 * vc_RootingDepth_m) / layerThickness))); // []
  if (vc_RootingZone > nols) {
    vc_RootingZone = nols;
  }

  vc_TotalRootLength = vc_RootBiomass * pc_SpecificRootLength; //[m m-2]

  // Calculating a root density distribution factor []
  std::vector<double> vc_RootDensityFactor;
  double vc_RootDensityFactorSum = 0.0;
  tie(vc_RootDensityFactor, vc_RootDensityFactorSum) = calcRootDensityFactorAndSum();

  // calculate the distribution of dead root biomass (for later addition into AOM pools (in soil-organic))
  if (!cropPs.__disable_daily_root_biomass_to_soil__) {
    fc_MoveDeadRootBiomassToSoil(dailyDeadBiomassIncrement[0], vc_RootDensityFactorSum, vc_RootDensityFactor);
  }

  // Calculating root density per layer from total root length and
  // a relative root density distribution factor
  for (size_t i_Layer = 0; i_Layer < vc_RootingZone; i_Layer++) {
    vc_RootDensity[i_Layer] = (vc_RootDensityFactor[i_Layer] / vc_RootDensityFactorSum) * vc_TotalRootLength; // [m m-3]
  }

  for (size_t i_Layer = 0; i_Layer < vc_RootingZone; i_Layer++) {
    // Root diameter [m]
    if (pc_AbovegroundOrgan[3]) {
      vc_RootDiameter[i_Layer] = 0.0002 - ((i_Layer + 1) * 0.00001); // [m]
    } else { vc_RootDiameter[i_Layer] = 0.0001; } //[m]

    // Default root decay - 10 %
    // vo_FreshSoilOrganicMatter[i_Layer] += vc_RootNIncrement
    //	* vc_RootDensity[i_Layer]
    //	* 10.0
    //	/ vc_TotalRootLength;
  }
  // *****************************************************************************

  // *** Original HERMES approach: ***
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
  auto nols = soilColumn.vs_NumberOfLayers();
  double layerThickness = soilColumn.vs_LayerThickness();

  // Calculating a root density distribution factor []
  std::vector<double> vc_RootDensityFactor(nols, 0.0);
  for (size_t i_Layer = 0; i_Layer < nols; i_Layer++) {
    if (i_Layer < vc_RootingDepth) {
      vc_RootDensityFactor[i_Layer] = exp(-pc_RootFormFactor * (i_Layer * layerThickness)); // []
    } else if (i_Layer < vc_RootingZone) {
      vc_RootDensityFactor[i_Layer] = exp(-pc_RootFormFactor * (i_Layer * layerThickness)) *
                                      (1.0 - ((i_Layer - vc_RootingDepth) / (vc_RootingZone - vc_RootingDepth))); // []
    } else {
      vc_RootDensityFactor[i_Layer] = 0.0;
    } // []
  }

  // Summing up all factors to scale to a relative factor between [0;1]
  double vc_RootDensityFactorSum = 0.0;
  for (size_t i_Layer = 0; i_Layer < vc_RootingZone; i_Layer++)
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
  double vc_AtmosphericPressure;      //[kPA]
  double vc_PsycrometerConstant;      //[kPA °C-1]
  double vc_SaturatedVapourPressureMax;  //[kPA]
  double vc_SaturatedVapourPressureMin;  //[kPA]
  double vc_SaturatedVapourPressure;    //[kPA]
  double vc_VapourPressure;        //[kPA]
  double vc_SaturationDeficit;      //[kPA]
  double vc_SaturatedVapourPressureSlope; //[kPA °C-1]
  double vc_WindSpeed_2m;          //[m s-1]
  double vc_AerodynamicResistance;    //[s m-1]
  double vc_SurfaceResistance;      //[s m-1]
  double vc_ReferenceEvapotranspiration;  //[mm]
  double vw_NetRadiation;          //[MJ m-2]

  const CropModuleParameters &user_crops = cropPs;
  double pc_SaturationBeta = user_crops.pc_SaturationBeta;           // Original: Yu et al. 2001; beta = 3.5
  double pc_StomataConductanceAlpha = user_crops.pc_StomataConductanceAlpha; // Original: Yu et al. 2001; alpha = 0.06
  double pc_ReferenceAlbedo = user_crops.pc_ReferenceAlbedo;           // FAO Green gras reference albedo from Allen et al. (1998)

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
  size_t nols = soilColumn.vs_NumberOfLayers();
  double layerThickness = soilColumn.vs_LayerThickness();
  double vc_PotentialTranspirationDeficit = 0.0;  // [mm]
  vc_PotentialTranspiration = 0.0;        // old TRAMAX [mm]
  double vc_PotentialEvapotranspiration = 0.0;  // [mm]
  double vc_TranspirationReduced = 0.0;      // old TDRED [mm]
  vc_ActualTranspiration = 0.0;          // [mm]
  double vc_RemainingTotalRootEffectivity = 0.0;  // old WEFFREST [m]
  double vc_CropWaterUptakeFromGroundwater = 0.0; // old GAUF [mm]
  double vc_TotalRootEffectivity = 0.0;      // old WEFF [m]
  double vc_ActualTranspirationDeficit = 0.0;    // old TREST [mm]
  double vc_Interception = 0.0;
  vc_RemainingEvapotranspiration = 0.0;

  for (size_t i_Layer = 0; i_Layer < nols; i_Layer++) {
    vc_Transpiration[i_Layer] = 0.0;    // old TP [mm]
    vc_TranspirationRedux[i_Layer] = 0.0; // old TRRED []
    vc_RootEffectivity[i_Layer] = 0.0;    // old WUEFF [?]
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
          soilColumn[i_Layer].vs_FieldCapacity() - soilColumn[i_Layer].vs_PermanentWiltingPoint();
      double vc_AvailableWaterPercentage =
          (soilColumn[i_Layer].get_Vs_SoilMoisture_m3() - soilColumn[i_Layer].vs_PermanentWiltingPoint()) /
          vc_AvailableWater;
      if (vc_AvailableWaterPercentage < 0.0) {
        vc_AvailableWaterPercentage = 0.0;
      }

      if (vc_AvailableWaterPercentage < 0.15) {
        vc_TranspirationRedux[i_Layer] = vc_AvailableWaterPercentage * 3.0;        // []
        vc_RootEffectivity[i_Layer] = 0.15 + 0.45 * vc_AvailableWaterPercentage / 0.15; // []
      } else if (vc_AvailableWaterPercentage < 0.3) {
        vc_TranspirationRedux[i_Layer] = 0.45 + (0.25 * (vc_AvailableWaterPercentage - 0.15) / 0.15);
        vc_RootEffectivity[i_Layer] = 0.6 + (0.2 * (vc_AvailableWaterPercentage - 0.15) / 0.15);
      } else if (vc_AvailableWaterPercentage < 0.5) {
        vc_TranspirationRedux[i_Layer] = 0.7 + (0.275 * (vc_AvailableWaterPercentage - 0.3) / 0.2);
        vc_RootEffectivity[i_Layer] = 0.8 + (0.2 * (vc_AvailableWaterPercentage - 0.3) / 0.2);
      } else if (vc_AvailableWaterPercentage < 0.75) {
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
          ((soilColumn[i_Layer].get_Vs_SoilMoisture_m3() - soilColumn[i_Layer].vs_PermanentWiltingPoint()))) {
        vc_PotentialTranspirationDeficit = (((vc_Transpiration[i_Layer] / 1000.0) / layerThickness) -
                                            (soilColumn[i_Layer].get_Vs_SoilMoisture_m3() -
                                             soilColumn[i_Layer].vs_PermanentWiltingPoint())) * layerThickness *
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
      if (vc_Transpiration[i_Layer] < 0.0) {
        vc_Transpiration[i_Layer] = 0.0;
      }
      vc_ActualTranspiration += vc_Transpiration[i_Layer];
      if (i_Layer == vc_GroundwaterTable) {
        vc_CropWaterUptakeFromGroundwater = (vc_Transpiration[i_Layer] / 1000.0) / layerThickness; //[m3 m-3]
      }
    }
    if (vc_PotentialTranspiration > 0) { vc_TranspirationDeficit = vc_ActualTranspiration / vc_PotentialTranspiration; }
    else { vc_TranspirationDeficit = 1.0; }

    int vm_GroundwaterDistance = (int) vc_GroundwaterTable - (int) vc_RootingDepth;
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
  auto nols = soilColumn.vs_NumberOfLayers();
  double layerThickness = soilColumn.vs_LayerThickness();

  double vc_ConvectiveNUptake = 0.0;                // old TRNSUM
  double vc_DiffusiveNUptake = 0.0;                // old SUMDIFF
  std::vector<double> vc_ConvectiveNUptakeFromLayer(nols, 0.0); // old MASS

  std::vector<double> vc_DiffusionCoeff(nols, 0.0);               // old D
  std::vector<double> vc_DiffusiveNUptakeFromLayer(nols, 0.0);         // old DIFF
  double vc_ConvectiveNUptake_1 = 0.0;                     // old MASSUM
  double vc_DiffusiveNUptake_1 = 0.0;                       // old DIFFSUM
  double pc_MinimumAvailableN = cropPs.pc_MinimumAvailableN;           // kg m-3
  double pc_MinimumNConcentrationRoot = cropPs.pc_MinimumNConcentrationRoot; // kg kg-1
  double pc_MaxCropNDemand = cropPs.pc_MaxCropNDemand;

  vc_TotalNUptake = 0.0;
  vc_TotalNInput = 0.0;
  vc_FixedN = 0.0;
  // for (size_t i_Layer = 0; i_Layer < nols; i_Layer++)
  //	vc_NUptakeFromLayer[i_Layer] = 0.0;
  for (auto &v: vc_NUptakeFromLayer)
    v = 0.0;

  // if the plant has matured, no N uptake occurs!
  if (vc_DevelopmentalStage < vc_FinalDevelopmentalStage) {
    // if ((vc_CurrentTotalTemperatureSum / vc_TotalTemperatureSum) < 1.0){

    for (int i_Layer = 0; i_Layer < (min(vc_RootingZone, vc_GroundwaterTable)); i_Layer++) {

      vs_SoilMineralNContent[i_Layer] = soilColumn[i_Layer].vs_SoilNO3; // [kg m-3]

      // Convective N uptake per layer
      vc_ConvectiveNUptakeFromLayer[i_Layer] = (vc_Transpiration[i_Layer] / 1000.0) *        //[mm --> m]
                                               (vs_SoilMineralNContent[i_Layer] /          // [kg m-3]
                                                (soilColumn[i_Layer].get_Vs_SoilMoisture_m3())) * // old WG [m3 m-3]
                                               vc_TimeStep;                    // -->[kg m-2]

      vc_ConvectiveNUptake += vc_ConvectiveNUptakeFromLayer[i_Layer]; // [kg m-2]

      /** @todo Claas: Woher kommt der Wert für vs_Tortuosity? */
      /** @todo Claas: Prüfen ob Umstellung auf [m] die folgenden Gleichungen beeinflusst */
      vc_DiffusionCoeff[i_Layer] = 0.000214 * (vs_Tortuosity * exp(soilColumn[i_Layer].get_Vs_SoilMoisture_m3() * 10)) /
                                   soilColumn[i_Layer].get_Vs_SoilMoisture_m3(); //[m2 d-1]

      vc_DiffusiveNUptakeFromLayer[i_Layer] = (vc_DiffusionCoeff[i_Layer] *          // [m2 d-1]
                                               soilColumn[i_Layer].get_Vs_SoilMoisture_m3() * // [m3 m-3]
                                               2.0 * PI * vc_RootDiameter[i_Layer] *      // [m]
                                               (vs_SoilMineralNContent[i_Layer] / 1000.0 /  // [kg m-3]
                                                soilColumn[i_Layer].get_Vs_SoilMoisture_m3() -
                                                0.000014) *               // [m3 m-3]
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

    // *** Biological N Fixation ***

    vc_FixedN = pc_PartBiologicalNFixation * vc_CropNDemand * 10000.0; // [kg N ha-1]
    // Part of the deficit which can be covered by biologocal N fixation.

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
        ((vc_RootBiomassOld * vc_NConcentrationRoot) + ((vc_RootBiomass - vc_RootBiomassOld) /
                                                        (vc_AbovegroundBiomass - vc_AbovegroundBiomassOld +
                                                         vc_BelowgroundBiomass - vc_BelowgroundBiomassOld +
                                                         vc_RootBiomass - vc_RootBiomassOld) * vc_TotalNInput)) /
        vc_RootBiomass;

    vc_NConcentrationRoot = bound(pc_MinimumNConcentrationRoot,
                                  vc_NConcentrationRoot,
                                  pc_StageMaxRootNConcentration[vc_DevelopmentalStage]);

    // vc_NConcentrationRoot = min(vc_NConcentrationRoot, pc_StageMaxRootNConcentration[vc_DevelopmentalStage]);

    // if(vc_NConcentrationRoot < pc_MinimumNConcentrationRoot)
    //{
    //	vc_NConcentrationRoot = pc_MinimumNConcentrationRoot;
    // }
  }

  vc_NConcentrationAbovegroundBiomass =
      (vc_TotalBiomassNContent - (vc_RootBiomass * vc_NConcentrationRoot)) /
      (vc_AbovegroundBiomass + (vc_BelowgroundBiomass / pc_ResidueNRatio));

  if ((vc_NConcentrationAbovegroundBiomass * vc_AbovegroundBiomass) <
      (vc_NConcentrationAbovegroundBiomassOld * vc_AbovegroundBiomassOld)) {
    double temp_vc_NConcentrationAbovegroundBiomass =
        vc_NConcentrationAbovegroundBiomassOld * vc_AbovegroundBiomassOld / vc_AbovegroundBiomass;

    double temp_vc_NConcentrationRoot =
        (vc_TotalBiomassNContent - (vc_NConcentrationAbovegroundBiomass * vc_AbovegroundBiomass) -
         (vc_NConcentrationAbovegroundBiomass / pc_ResidueNRatio * vc_BelowgroundBiomass)) / vc_RootBiomass;

    if (temp_vc_NConcentrationRoot >= pc_MinimumNConcentrationRoot) {
      vc_NConcentrationAbovegroundBiomass = temp_vc_NConcentrationAbovegroundBiomass;
      vc_NConcentrationRoot = temp_vc_NConcentrationRoot;
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

void CropModule::calculateVOCEmissions(const Voc::MicroClimateData &mcd) {
  Voc::SpeciesData species;
  // species.id = 0; // right now we just have one crop at a time, so no need to distinguish multiple crops
  species.lai = get_LeafAreaIndex();
  species.mFol = get_OrganBiomass(LEAF) / (100. * 100.);          // kg/ha -> kg/m2
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
  return (vc_TotalBiomassNContent -
          (get_OrganBiomass(0) * get_RootNConcentration())) /
         (get_OrganBiomass(3) + (pc_ResidueNRatio *
                                 (vc_TotalBiomass - get_OrganBiomass(0) - get_OrganBiomass(3))));
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
double calculateCropYield(const VYC &ycs, const vector<double> &bmv) {
  double yield = 0;
  for (const auto &yc: ycs)
    yield += bmv.at(yc.organId - 1) * (yc.yieldPercentage);
  return yield;
}

/**
 * @brief Returns crop yield.
 *
 * @param v Vector yield component
 * @param bmv
 */
double calculateCropFreshMatterYield(const VYC &ycs, const vector<double> &bmv) {
  double freshMatterYield = 0;
  for (auto yc: ycs)
    freshMatterYield += bmv.at(yc.organId - 1) * yc.yieldPercentage / yc.yieldDryMatter;
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

  return (vc_TotalBiomassNContent - (rootBiomass * get_RootNConcentration())) /
         ((primaryCropYield / pc_ResidueNRatio) + (vc_TotalBiomass - rootBiomass - primaryCropYield));
}

/**
 * @brief Returns primary yield N concentration [kg kg-1]
 * @return primary yield N concentration
 */
double CropModule::get_PrimaryYieldNConcentration(double alternativePrimaryCropYield) const {
  auto primaryCropYield = alternativePrimaryCropYield >= 0 ? alternativePrimaryCropYield : get_PrimaryCropYield();
  auto rootBiomass = get_OrganBiomass(0);

  return (vc_TotalBiomassNContent - (rootBiomass * get_RootNConcentration())) /
         (primaryCropYield + (pc_ResidueNRatio * (vc_TotalBiomass - rootBiomass - primaryCropYield)));
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

double
CropModule::get_SecondaryYieldNContent(double alternativePrimaryCropYield, double alternativeSecondaryCropYield) const {
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

void CropModule::applyCutting(std::map<int, Cutting::Value> &organs,
                              std::map<int, double> &exports,
                              double cutMaxAssimilationFraction) {
  double oldAbovegroundBiomass = vc_AbovegroundBiomass;
  double oldAgbNcontent = get_AbovegroundBiomassNContent();
  double sumCutBiomass = 0.0;
  double currentSLA = get_LeafAreaIndex() / vc_OrganGreenBiomass[1];

  Tools::debug() << "CropModule::applyCutting()" << endl;

  if (organs.empty()) {
    for (auto yc: pc_OrganIdsForCutting) {
      Cutting::Value v;
      v.value = yc.yieldPercentage;
      organs[yc.organId - 1] = v;
    }
  }

  double sumResidueBiomass = 0;
  for (auto p: organs) {
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
      double currentLAI = get_LeafAreaIndex();
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
    double residueNConcentration = get_AbovegroundBiomassNConcentration();
    debug() << "adding organic matter from cut residues to soilOrganic" << endl;
    debug() << "Residue biomass: " << sumResidueBiomass
            << " Residue N concentration: " << residueNConcentration << endl;
    _addOrganicMatter({{0, sumResidueBiomass}}, residueNConcentration);
  }

  // update LAI
  if (vc_OrganGreenBiomass[1] > 0) {
    vc_LeafAreaIndex = vc_OrganGreenBiomass[1] * currentSLA;
  }

  // reset stage and temperature some after cutting
  setStage(pc_StageAfterCut);

  // int stageAfterCutting = pc_StageAfterCut;
  // for(int stage = stageAfterCutting; stage < pc_NumberOfDevelopmentalStages; stage++)
  //	vc_CurrentTemperatureSum[stage] = 0.0;
  // vc_CurrentTotalTemperatureSum = 0.0;
  // vc_DevelopmentalStage = stageAfterCutting;

  vc_CuttingDelayDays = pc_CuttingDelayDays;
  pc_MaxAssimilationRate = pc_MaxAssimilationRate * cutMaxAssimilationFraction;

  // double rootNcontent = vc_TotalBiomassNContent - get_AbovegroundBiomassNContent();
  // vc_TotalBiomassNContent = (vc_AbovegroundBiomass / oldAbovegroundBiomass) * get_AbovegroundBiomassNContent() + rootNcontent;
  if (oldAbovegroundBiomass > 0.0) {
    vc_TotalBiomassNContent -= (1 - vc_AbovegroundBiomass / oldAbovegroundBiomass) * oldAgbNcontent;
  }
}

double CropModule::get_AccumulatedETa() const {
  return vc_AccumulatedETa;
}

double CropModule::get_AccumulatedTranspiration() const {
  return vc_AccumulatedTranspiration;
}

double CropModule::get_AccumulatedPrimaryCropYield() const {
  return vc_AccumulatedPrimaryCropYield;
}

/**
 * Returns the depth of the maximum active and effective root.
 * [m]
 */
double CropModule::getEffectiveRootingDepth() const {
  size_t nols = soilColumn.vs_NumberOfLayers();

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
  pc_DevelopmentAccelerationByNitrogenStress = perennialCropParams->speciesParams.pc_DevelopmentAccelerationByNitrogenStress;
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
  if (pc_NumberOfDevelopmentalStages == 6) { return kj::tuple(3, 4); }
  else if (pc_NumberOfDevelopmentalStages == 7) return kj::tuple(4, 5);
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
