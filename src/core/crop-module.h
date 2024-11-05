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
#include <ostream>
#include <iostream>
#include <iomanip>
#include <vector>
#include <utility>

#include <kj/async-io.h>
#include <kj/tuple.h>

#include "model/monica/monica_state.capnp.h"

#include "monica-parameters.h"
#include "soilcolumn.h"
#include "voc-common.h"
#include "run/cultivation-method.h"

namespace monica {

/*
* @brief  Crop part of model
*
* The crop is divided into several organs e.g. root, shoot, leaf, storage organ
* and eventually a permanent structure.
* In the source code, the organs are mapped to numbers:<br>
* 0 - Root<br>
* 1 - Leaf<br>
* 2 - Shoot<br>
* 3 - Storage organ<br>
* 4 - Permanent structure <br>
*
*/
class CropModule {
public:
  CropModule(SoilColumn &soilColumn,
             const CropParameters &cropParams,
             CropResidueParameters rps,
             bool isWinterCrop,
             const SiteParameters &siteParams,
             const CropModuleParameters &cropPs,
             const SimulationParameters &simPs,
             std::function<void(std::string)> fireEvent,
             std::function<void(std::map<size_t, double>, double)> addOrganicMatter,
             std::function<std::pair<double, double>(double)> getSnowDepthAndCalcTempUnderSnow,
             Intercropping &ic);

  CropModule(SoilColumn &sc,
             const CropModuleParameters &cropPs,
             std::function<void(std::string)> fireEvent,
             std::function<void(std::map<size_t, double>, double)> addOrganicMatter,
             std::function<std::pair<double, double>(double)> getSnowDepthAndCalcTempUnderSnow,
             mas::schema::model::monica::CropModuleState::Reader reader,
             Intercropping &ic);

  void deserialize(mas::schema::model::monica::CropModuleState::Reader reader);

  void serialize(mas::schema::model::monica::CropModuleState::Builder builder) const;

  void applyCutting(std::map<int, Cutting::Value> &organs,
                    std::map<int, double> &exports,
                    double cutMaxAssimilateFraction);

  void step(double vw_MeanAirTemperature,
            double vw_MaxAirTemperature,
            double vw_MinAirTemperature,
            double vw_GlobalRadiation,
            double vw_SunshineHours,
            Tools::Date currentDate,
            double vw_RelativeHumidity,
            double vw_WindSpeed,
            double vw_WindSpeedHeight,
            double vw_AtmosphericCO2Concentration,
            double vw_AtmosphericO3Concentration,
            double vw_GrossPrecipitation,
            double vw_ReferenceEvapotranspiration);

  //void get_CropIdentity();
  //void get_CropParameters();

  void fc_Radiation(double vs_JulianDay,
                    double vw_GlobalRadiation,
                    double vw_SunshineHours);

  double fc_DaylengthFactor(double d_DaylengthRequirement,
                            double vc_EffectiveDayLength,
                            double vc_PhotoperiodicDaylength,
                            double d_BaseDaylength);

  std::pair<double, double> fc_VernalisationFactor(double vw_MeanAirTemperature,
                                                   double pc_VernalisationRequirement,
                                                   double vc_VernalisationDays);

  double fc_OxygenDeficiency(double pc_CriticalOxygenContent);

  void fc_CropDevelopmentalStage(double meanAirTemperature,
                                 double soilMoisture_m3,
                                 double fieldCapacity,
                                 double permanentWiltingPoint);

  double fc_KcFactor(double d_StageTemperatureSum,
                     double d_CurrentTemperatureSum,
                     double d_StageKcFactor,
                     double d_EarlierStageKcFactor) const;

  void fc_CropSize(double maxCropHeight);

  void fc_CropGreenArea(double vw_MeanAirTemperature,
                        double d_LeafBiomassIncrement,
                        double d_LeafBiomassDecrement,
                        double d_SpecificLeafAreaStart,
                        double pc_SpecificLeafAreaEnd,
                        double pc_SpecificLeafAreaEarly,
                        double d_StageTemperatureSum,
                        double d_CurrentTemperatureSum);

  double fc_SoilCoverage() const;

  void fc_MoveDeadRootBiomassToSoil(double deadRootBiomass,
                                    double vc_RootDensityFactorSum,
                                    const std::vector<double> &vc_RootDensityFactor);

  void addAndDistributeRootBiomassInSoil(double rootBiomass);

  void fc_CropPhotosynthesis(double vw_MeanAirTemperature,
                             double vw_MaxAirTemperature,
                             double vw_MinAirTemperature,
                             double vw_AtmosphericCO2Concentration,
                             double vw_AtmosphericO3Concentration,
                             Tools::Date currentDate);

  void fc_HeatStressImpact(double vw_MeanAirTemperature,
                           double vw_MaxAirTemperature);

  void fc_FrostKill(double vw_MeanAirTemperature,
                    double vw_MaxAirTemperature);

  void fc_DroughtImpactOnFertility();

  void fc_CropNitrogen();

  void fc_CropDryMatter(double vw_MeanAirTemperature);

  double fc_ReferenceEvapotranspiration(double vw_MaxAirTemperature,
                                        double vw_MinAirTemperature,
                                        double vw_RelativeHumidity,
                                        double vw_MeanAirTemperature,
                                        double vw_WindSpeed,
                                        double vw_WindSpeedHeight,
                                        double vw_AtmosphericCO2Concentration);

  void fc_CropWaterUptake(size_t vm_GroundwaterTable,
                          double vw_GrossPrecipitation,
                          double vc_CurrentTotalTemperatureSum,
                          double vc_TotalTemperatureSum);

  void fc_CropNUptake(size_t vm_GroundwaterTable,
                      double /*vc_CurrentTotalTemperatureSum*/,
                      double /*vc_TotalTemperatureSum*/);

  double fc_GrossPrimaryProduction();

  double fc_NetPrimaryProduction(double vc_TotalRespired);

  void calculateVOCEmissions(const Voc::MicroClimateData &mcd);

  Voc::Emissions guentherEmissions() const { return _guentherEmissions; }

  Voc::Emissions jjvEmissions() const { return _jjvEmissions; }

  double get_ReferenceEvapotranspiration() const;

  double get_RemainingEvapotranspiration() const;

  double get_EvaporatedFromIntercept() const;

  double get_NetPrecipitation() const;

  std::string get_CropName() const;

  double get_GrossPhotosynthesisRate() const;

  double get_GrossPhotosynthesisHaRate() const;

  double get_AssimilationRate() const;

  double get_Assimilates() const;

  double get_NetMaintenanceRespiration() const;

  double get_MaintenanceRespirationAS() const;

  double get_GrowthRespirationAS() const;

  double get_VernalisationFactor() const;

  double get_DaylengthFactor() const;

  double get_OrganGrowthIncrement(int i_Organ) const;

  double get_NetPhotosynthesis() const;

  double get_LeafAreaIndex() const;

  double get_CropHeight() const;

  size_t get_RootingDepth() const;

  double get_RootingDepth_m() const { return vc_RootingDepth_m; }

  double get_SoilCoverage() const;

  double get_KcFactor() const;

  double get_StomataResistance() const;

  double get_Transpiration(int i_Layer) const;

  double get_TranspirationDeficit() const;

  double get_CropNRedux() const;

  double get_FrostStressRedux() const;

  double get_HeatStressRedux() const;

  double get_PotentialTranspiration() const;

  double get_ActualTranspiration() const;

  double get_OxygenDeficit() const;

  double get_CurrentTemperatureSum() const;

  size_t get_DevelopmentalStage() const;

  double get_RelativeTotalDevelopment() const;

  double get_OrganBiomass(int i_Organ) const;

  double get_OrganGreenBiomass(int i_Organ) const;

  double get_AbovegroundBiomass() const;

  double get_LT50() const;

  double get_AbovegroundBiomassNContent() const;

  double get_FruitBiomassNConcentration() const;

  double get_FruitBiomassNContent() const;

  double get_HeatSumIrrigationStart() const;

  double get_HeatSumIrrigationEnd() const;

  double get_NUptakeFromLayer(size_t i_Layer) const;

  double get_TotalBiomass() const;

  double get_TotalBiomassNContent() const;

  double get_RootNConcentration() const;

  double get_TargetNConcentration() const;

  double get_CriticalNConcentration() const;

  double get_AbovegroundBiomassNConcentration() const;

  double get_PrimaryCropYield() const;

  double get_SecondaryCropYield() const;

  double get_CropYieldAfterCutting() const;

  double get_FreshPrimaryCropYield() const;

  double get_FreshSecondaryCropYield() const;

  double get_FreshCropYieldAfterCutting() const;

  double get_ResidueBiomass(bool useSecondaryCropYields = true, double alternativeCropYield = -1) const;

  double get_ResiduesNConcentration(double alternativePrimaryCropYield = -1) const;

  double get_PrimaryYieldNConcentration(double alternativePrimaryCropYield = -1) const;

  double get_ResiduesNContent(bool useSecondaryCropYields = true, double alternativePrimaryCropYield = -1,
                              double alternativeCropYield = -1) const;

  double get_PrimaryYieldNContent(double alternativePrimaryCropYield = -1) const;

  double get_RawProteinConcentration() const;

  double
  get_SecondaryYieldNContent(double alternativePrimaryCropYield = -1, double alternativeSecondaryCropYield = -1) const;

  double get_SumTotalNUptake() const;

  double get_ActNUptake() const;

  double get_PotNUptake() const;

  double get_BiologicalNFixation() const;

  double get_AccumulatedETa() const;

  double get_AccumulatedTranspiration() const;

  double get_AccumulatedPrimaryCropYield() const;

  double get_GrossPrimaryProduction() const;

  double get_NetPrimaryProduction() const;

  double get_AutotrophicRespiration() const;

  double get_OrganSpecificTotalRespired(int organ) const;

  double get_OrganSpecificNPP(int organ) const;

  double getEffectiveRootingDepth() const;

  int get_NumberOfOrgans() const;

  int get_StageAfterCut() const;

  int getAnthesisDay() const;

  int getMaturityDay() const;

  bool maturityReached() const;

  /**
  * @brief Returns short term O3 damage
  */
  double get_O3_shortTermDamage() const { return vc_O3_shortTermDamage; }

  /**
  * @brief Returns long term O3 damage
  */
  double get_O3_longTermDamage() const { return vc_O3_longTermDamage; }

  /**
  * @brief Returns reduction factor of O3 uptake due to stomatal closure
  */
  double get_O3_WStomatalClosure() const { return vc_O3_WStomatalClosure; }

  /**
  * @brief Returns O3 sum uptake
  */
  double get_O3_sumUptake() const { return vc_O3_sumUptake; }

  /*
 * @brief Getter for total biomass.
 * @return total biomass
 */
  inline double totalBiomass() const { return vc_TotalBiomass; }

  /*
   * Returns state of plant
   */
  inline bool isDying() const { return this->dyingOut; }

  void setPerennialCropParameters(const CropParameters &cps) { perennialCropParams = kj::heap<CropParameters>(cps); }

  void fc_UpdateCropParametersForPerennial();

  std::pair<const std::vector<double> &, const std::vector<double> &> sunlitAndShadedLAI() const {
    return make_pair(vc_sunlitLeafAreaIndex, vc_shadedLeafAreaIndex);
  }

  double getLeafAreaIndex() const { return vc_LeafAreaIndex; }

  void setLeafAreaIndex(double lai) { vc_LeafAreaIndex = lai; }

  double getSpecificLeafArea(int stage) const { return pc_SpecificLeafArea[stage]; }

  double sumExportedCutBiomass() const { return vc_sumExportedCutBiomass; }

  double exportedCutBiomass() const { return vc_exportedCutBiomass; }

  double sumResidueCutBiomass() const { return vc_sumResidueCutBiomass; }

  double residueCutBiomass() const { return vc_residueCutBiomass; }

  double rootNConcentration() const { return vc_NConcentrationRoot; }

  std::pair<std::vector<double>, double> calcRootDensityFactorAndSum();

  void setStage(size_t newStage);

  double getRootDensity(int layer) const { return vc_RootDensity.at(layer); }

  size_t rootingZone() const { return vc_RootingZone; };

  const SpeciesParameters &speciesParameters() const { return speciesPs; }

  const CultivarParameters &cultivarParameters() const { return cultivarPs; }

  const CropResidueParameters &residueParameters() const { return residuePs; }

  bool isWinterCrop() const { return _isWinterCrop; }

  std::set<int> organIdsForPrimaryYield() const {
    std::set<int> ids;
    for (const auto &yc: pc_OrganIdsForPrimaryYield) ids.insert(yc.organId);
    return ids;
  }

  void setOtherCropHeightAndLAIt(double cropHeight, double lait) {
    _intercroppingOtherCropHeight = cropHeight;
    _intercroppingOtherLAIt = lait;
  }

  double getFractionOfInterceptedRadiation1() const { return fractionOfInterceptedRadiation1; }

  double getFractionOfInterceptedRadiation2() const { return fractionOfInterceptedRadiation2; }

  [[nodiscard]] double getCurrentTotalTemperatureSum() const { return vc_CurrentTotalTemperatureSum; }

  [[nodiscard]] double getTotalTemperatureSum() const { return vc_TotalTemperatureSum; }

  [[nodiscard]] kj::Tuple<int, int> anthesisBetweenStages() const;

  // endAtInclStage < 0 -> count from end
  double sumStageTemperatureSums(int startAtStage, int endAtInclStage) const;

  double rootNRedux{0.0}; //! old REDWU
private:
  Intercropping &_intercropping;

  bool _frostKillOn{true};

  int pc_NumberOfAbovegroundOrgans() const;

  bool isAnthesisDay(size_t old_dev_stage, size_t new_dev_stage);

  bool isMaturityDay(size_t old_dev_stage, size_t new_dev_stage);

  // members
  SoilColumn &soilColumn;
  kj::Own<CropParameters> perennialCropParams;
  const CropModuleParameters &cropPs;
  SpeciesParameters speciesPs;
  CultivarParameters cultivarPs;
  CropResidueParameters residuePs;
  bool _isWinterCrop{false};
  double _bareSoilKcFactor{0.4};

  //! old N
  //  static const double vw_AtmosphericCO2Concentration;
  double vs_Latitude{};
  double vc_AbovegroundBiomass{0.0};//! old OBMAS
  double vc_AbovegroundBiomassOld{0.0}; //! old OBALT
  std::vector<bool> pc_AbovegroundOrgan;  //! old KOMP
  double vc_ActualTranspiration{0.0};
  std::vector<std::vector<double>> pc_AssimilatePartitioningCoeff; //! old PRO
  double pc_AssimilateReallocation{};
  double vc_Assimilates{0.0};
  double vc_AssimilationRate{0.0}; //! old AMAX
  double vc_AstronomicDayLenght{0.0};  //! old DL
  std::vector<double> pc_BaseDaylength;  //! old DLBAS
  std::vector<double> pc_BaseTemperature;  //! old BAS
  double pc_BeginSensitivePhaseHeatStress{};
  double vc_BelowgroundBiomass{0.0};
  double vc_BelowgroundBiomassOld{0.0};
  int pc_CarboxylationPathway{};    //! old TEMPTYP
  double vc_ClearDayRadiation{0.0};    //! old DRC
  int pc_CO2Method{3};
  double vc_CriticalNConcentration{0.0}; //! old GEHMIN
  std::vector<double> pc_CriticalOxygenContent; //! old LUKRIT
  double pc_CriticalTemperatureHeatStress{};
  double vc_CropDiameter{0.0};
  double vc_CropFrostRedux{1.0};
  double vc_CropHeatRedux{1.0};
  double vc_CropHeight{0.0};
  double pc_CropHeightP1{};
  double pc_CropHeightP2{};
  std::string pc_CropName;      //! old FRUCHT$(AKF)
  double vc_CropNDemand{0.0}; //! old DTGESN
  double vc_CropNRedux{1.0};              //! old REDUK
  double pc_CropSpecificMaxRootingDepth{};      //! old WUMAXPF [m]
  std::vector<double> vc_CropWaterUptake; //! old TP
  std::vector<double> vc_CurrentTemperatureSum;  //! old SUM
  double vc_CurrentTotalTemperatureSum{0.0};      //! old FP
  double vc_CurrentTotalTemperatureSumRoot{0.0};
  int pc_CuttingDelayDays{0};
  double vc_DaylengthFactor{0.0};            //! old DAYL
  std::vector<double> pc_DaylengthRequirement;    //! old DEC
  int vc_DaysAfterBeginFlowering{0};
  double vc_Declination{0.0};              //! old EFF0
  double pc_DefaultRadiationUseEfficiency{};
  int vm_DepthGroundwaterTable{0};    //! old GRW
  int pc_DevelopmentAccelerationByNitrogenStress{};
  size_t vc_DevelopmentalStage{0};      //! old INTWICK
  int _noOfCropSteps{0};
  double vc_DroughtImpactOnFertility{1.0};
  double pc_DroughtImpactOnFertilityFactor{};
  std::vector<double> pc_DroughtStressThreshold;  //! old DRYswell
  bool pc_EmergenceFloodingControlOn{};
  bool pc_EmergenceMoistureControlOn{};
  double pc_EndSensitivePhaseHeatStress{};
  double vc_EffectiveDayLength{0.0};    //! old DLE
  bool vc_ErrorStatus{false};
  std::string vc_ErrorMessage;
  double vc_EvaporatedFromIntercept{0.0};
  double vc_ExtraterrestrialRadiation{0.0};
  double pc_FieldConditionModifier{};
  size_t vc_FinalDevelopmentalStage{0};
  double vc_FixedN{0.0};
  //std::vector<double> vo_FreshSoilOrganicMatter;	//! old NFOS
  double pc_FrostDehardening{};
  double pc_FrostHardening{};
  double vc_GlobalRadiation{0.0};
  double vc_GreenAreaIndex{0.0};
  double vc_GrossAssimilates{0.0};
  double vc_GrossPhotosynthesis{0.0};          //! old GPHOT
  double vc_GrossPhotosynthesis_mol{0.0};
  double vc_GrossPhotosynthesisReference_mol{0.0};
  double vc_GrossPrimaryProduction{0.0};
  bool vc_GrowthCycleEnded{false};
  double vc_GrowthRespirationAS{0.0};
  double pc_HeatSumIrrigationStart{};
  double pc_HeatSumIrrigationEnd{};
  double vs_HeightNN{};
  double pc_InitialKcFactor{};            //! old Kcini
  std::vector<double> pc_InitialOrganBiomass;
  double pc_InitialRootingDepth{};
  double vc_InterceptionStorage{0.0};
  double vc_KcFactor{0.6};      //! old FKc
  double vc_LeafAreaIndex{0.0};  //! old LAI
  std::vector<double> vc_sunlitLeafAreaIndex;
  std::vector<double> vc_shadedLeafAreaIndex;
  double pc_LowTemperatureExposure{};
  double pc_LimitingTemperatureHeatStress{};
  double vc_LT50{-3.0};
  double vc_LT50M{-3.0};
  double pc_LT50cultivar{};
  double pc_LuxuryNCoeff{};
  double vc_MaintenanceRespirationAS{0.0};
  double pc_MaxAssimilationRate{};  //! old MAXAMAX
  double pc_MaxCropDiameter{};
  double pc_MaxCropHeight{};
  double vc_MaxNUptake{0.0}; //! old MAXUP
  double pc_MaxNUptakeParam{};
  double vc_MaxRootingDepth{0.0}; //! old WURM
  double pc_MinimumNConcentration{};
  double pc_MinimumTemperatureForAssimilation{};  //! old MINTMP
  double pc_OptimumTemperatureForAssimilation{};
  double pc_MaximumTemperatureForAssimilation{};
  double pc_MinimumTemperatureRootGrowth{};
  double vc_NetMaintenanceRespiration{0.0};      //! old MAINT
  double vc_NetPhotosynthesis{0.0}; //! old GTW
  double vc_NetPrecipitation{0.0};
  double vc_NetPrimaryProduction{0.0};
  double pc_NConcentrationAbovegroundBiomass{}; //! initial value of old GEHOB
  double vc_NConcentrationAbovegroundBiomass{0.0}; //! old GEHOB
  double vc_NConcentrationAbovegroundBiomassOld{0.0}; //! old GEHALT
  double pc_NConcentrationB0{};
  double vc_NContentDeficit{};
  double pc_NConcentrationPN{};
  double pc_NConcentrationRoot{};      //! initial value to WUGEH
  double vc_NConcentrationRoot{0.0};    //! old WUGEH
  double vc_NConcentrationRootOld{0.0};    //! old
  bool pc_NitrogenResponseOn{};
  size_t pc_NumberOfDevelopmentalStages{0};
  size_t pc_NumberOfOrgans{0};              //! old NRKOM
  std::vector<double> vc_NUptakeFromLayer; //! old PE
  std::vector<double> pc_OptimumTemperature;
  std::vector<double> vc_OrganBiomass;  //! old WORG
  std::vector<double> vc_OrganDeadBiomass;  //! old WDORG
  std::vector<double> vc_OrganGreenBiomass;
  std::vector<double> vc_OrganGrowthIncrement;      //! old GORG
  std::vector<double> pc_OrganGrowthRespiration;  //! old MAIRT
  std::vector<YieldComponent> pc_OrganIdsForPrimaryYield;
  std::vector<YieldComponent> pc_OrganIdsForSecondaryYield;
  std::vector<YieldComponent> pc_OrganIdsForCutting;
  std::vector<double> pc_OrganMaintenanceRespiration;  //! old MAIRT
  std::vector<double> vc_OrganSenescenceIncrement; //! old DGORG
  std::vector<std::vector<double>> pc_OrganSenescenceRate;  //! old DEAD
  double vc_OvercastDayRadiation{0.0};          //! old DRO
  double vc_OxygenDeficit{0.0};          //! old LURED
  double pc_PartBiologicalNFixation{};
  bool pc_Perennial{};
  double vc_PhotoperiodicDaylength{0.0};        //! old DLP
  double vc_PhotActRadiationMean{0.0};          //! old RDN
  double pc_PlantDensity{};
  double vc_PotentialTranspiration{0.0};
  double vc_ReferenceEvapotranspiration{0.0};
  double vc_RelativeTotalDevelopment{0.0};
  double vc_RemainingEvapotranspiration{0.0};
  double vc_ReserveAssimilatePool{0.0};        //! old ASPOO
  double pc_ResidueNRatio{};
  double pc_RespiratoryStress{};
  double vc_RootBiomass{0.0};              //! old WUMAS
  double vc_RootBiomassOld{0.0};            //! old WUMALT
  std::vector<double> vc_RootDensity;        //! old WUDICH
  std::vector<double> vc_RootDiameter;        //! old WRAD
  double pc_RootDistributionParam{};
  std::vector<double> vc_RootEffectivity; //! old WUEFF
  double pc_RootFormFactor{};
  double pc_RootGrowthLag{};
  size_t vc_RootingDepth{0};                      //! old WURZ
  double vc_RootingDepth_m{0.0};
  size_t vc_RootingZone{0};
  double pc_RootPenetrationRate{};
  double vm_SaturationDeficit{0.0};
  double vc_SoilCoverage{0.0};
  std::vector<double> vs_SoilMineralNContent;    //! old C1
  double vc_SoilSpecificMaxRootingDepth{0.0};        //! old WURZMAX [m]
  double vs_SoilSpecificMaxRootingDepth{0.0};
  std::vector<double> pc_SpecificLeafArea;    //! old LAIFKT [ha kg-1]
  double pc_SpecificRootLength{};
  int pc_StageAfterCut{0}; //0-indexed
  double pc_StageAtMaxDiameter{};
  double pc_StageAtMaxHeight{};
  std::vector<double> pc_StageMaxRootNConcentration;  //! old WGMAX
  std::vector<double> pc_StageKcFactor;    //! old Kc
  std::vector<double> pc_StageTemperatureSum;  //! old TSUM
  double vc_StomataResistance{0.0};          //! old RSTOM
  std::vector<bool> pc_StorageOrgan;
  int vc_StorageOrgan{4};
  double vc_TargetNConcentration{0.0}; //! old GEHMAX
  double vc_TimeStep{1.0}; //! old dt
  int vc_TimeUnderAnoxia{0};
  int TimeUnderAnoxiaThresholdDefault = 4;
  std::vector<int> vc_TimeUnderAnoxiaThreshold;
  double vs_Tortuosity{};              //! old AD
  double vc_TotalBiomass{0.0};
  double vc_TotalBiomassNContent{0.0}; //! old PESUM
  double vc_TotalCropHeatImpact{0.0};
  double vc_TotalNInput{0.0};
  double vc_TotalNUptake{0.0};      //! old SUMPE
  double vc_TotalRespired{0.0};
  double vc_Respiration{0.0};
  double vc_SumTotalNUptake{0.0};  //! summation of all calculated NUptake; needed for sensitivity analysis
  double vc_TotalRootLength{0.0};            //! old WULAEN
  double vc_TotalTemperatureSum{0.0};
  double vc_TemperatureSumToFlowering{0.0};
  std::vector<double> vc_Transpiration;      //! old TP
  std::vector<double> vc_TranspirationRedux;   //! old TRRED
  double vc_TranspirationDeficit{1.0};          //! old TRREL
  double vc_VernalisationDays{0.0}; //
  double vc_VernalisationFactor{0.0};          //! old FV
  std::vector<double> pc_VernalisationRequirement;  //! old VSCHWELL
  bool pc_WaterDeficitResponseOn{};

  bool dyingOut{false};
  double vc_AccumulatedETa{0.0};
  double vc_AccumulatedTranspiration{0.0};
  double vc_sumExportedCutBiomass{0.0};
  double vc_exportedCutBiomass{0.0};
  double vc_sumResidueCutBiomass{0.0};
  double vc_residueCutBiomass{0.0};

  int vc_CuttingDelayDays{0};
  double vs_MaxEffectiveRootingDepth{};
  double vs_ImpenetrableLayerDepth{};

  int vc_AnthesisDay{-1};
  int vc_MaturityDay{-1};

  bool vc_MaturityReached{false};

  //VOC members
  int _stepSize24{24}, _stepSize240{240};
  std::vector<double> _rad24, _rad240, _tfol24, _tfol240;
  int _index24{0}, _index240{0};
  bool _full24{false}, _full240{false};

  Voc::Emissions _guentherEmissions;
  Voc::Emissions _jjvEmissions;
  Voc::SpeciesData _vocSpecies;
  Voc::CPData _cropPhotosynthesisResults;

  std::function<void(std::string)> _fireEvent;
  std::function<void(std::map<size_t, double>, double)> _addOrganicMatter;
  std::function<std::pair<double, double>(double)> _getSnowDepthAndCalcTempUnderSnow;

  double vc_O3_shortTermDamage{1.0};
  double vc_O3_longTermDamage{1.0};
  double vc_O3_senescence{1.0};
  double vc_O3_sumUptake{0.0};
  double vc_O3_WStomatalClosure{1.0};

  bool _assimilatePartCoeffsReduced{false};
  double vc_KTkc{0}; // old KTkc
  double vc_KTko{0}; // old KTkc

  bool _stemElongationEventFired{false};

  //intercropping
  double _intercroppingOtherCropHeight{-1};
  double _intercroppingOtherLAIt{-1};

  double fractionOfInterceptedRadiation1{0.0};
  double fractionOfInterceptedRadiation2{0.0};

  bool __enable_vernalisation_factor_fix__{false};
};

//#define TEST_HOURLY_OUTPUT
#ifdef TEST_HOURLY_OUTPUT
std::ostream& tout(bool closeFile = false);
#endif

} // namespace monica



