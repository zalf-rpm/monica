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

#include <cassert>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <kj/async-io.h>
#include <kj/common.h>

#include "json11/json11.hpp"

#include "fbp.capnp.h"
#include "model/monica/monica_params.capnp.h"
#include "model/monica/monica_state.capnp.h"

#include "json11/json11-helper.h"
#include "soil/constants.h"
#include "soil/soil.h"
#include "tools/date.h"

namespace monica {
class CentralParameterProvider;

struct YieldComponent {
  int organId{-1};
  double yieldPercentage{0.0};
  double yieldDryMatter{0.0};
};

YieldComponent makeYieldComponent(int organId, double yieldPercentage,
                                  double yieldDryMatter);
YieldComponent
makeYieldComponent(mas::schema::model::monica::YieldComponent::Reader reader);

namespace yieldcomponent {

void deserialize(YieldComponent *yc,
                 mas::schema::model::monica::YieldComponent::Reader reader);
void serialize(const YieldComponent *yc,
               mas::schema::model::monica::YieldComponent::Builder builder);
Tools::Errors merge(YieldComponent *yc, json11::Json j);
json11::Json to_json(const YieldComponent *yc);

} // namespace yieldcomponent

struct SpeciesParameters {
  std::string pc_SpeciesId;
  int pc_CarboxylationPathway{0}; // old TEMPTYP
  double pc_DefaultRadiationUseEfficiency{0.0};
  double pc_PartBiologicalNFixation{0.0};
  double pc_InitialKcFactor{0.0}; // old Kcini
  double pc_LuxuryNCoeff{0.0};
  double pc_MaxCropDiameter{0.0};
  double pc_StageAtMaxHeight{0.0};
  double pc_StageAtMaxDiameter{0.0};
  double pc_MinimumNConcentration{0.0};
  double pc_MinimumTemperatureForAssimilation{0.0}; // old MINTMP
  double pc_OptimumTemperatureForAssimilation{0.0};
  double pc_MaximumTemperatureForAssimilation{0.0};
  double pc_NConcentrationAbovegroundBiomass{0.0}; // initial value of old GEHOB
  double pc_NConcentrationB0{0.0};
  double pc_NConcentrationPN{0.0};
  double pc_NConcentrationRoot{0.0}; // initial value to WUGEH
  int pc_DevelopmentAccelerationByNitrogenStress{0};
  double pc_FieldConditionModifier{1.0};
  double pc_AssimilateReallocation{0.0};

  std::vector<double> pc_BaseTemperature;             // old BAS
  std::vector<double> pc_OrganMaintenanceRespiration; // old MAIRT
  std::vector<double> pc_OrganGrowthRespiration;      // old MAIRT
  std::vector<double> pc_StageMaxRootNConcentration;  // old WGMAX
  std::vector<double> pc_InitialOrganBiomass;
  std::vector<double> pc_CriticalOxygenContent; // old LUKRIT
  std::vector<double> pc_StageMobilFromStorageCoeff;

  std::vector<bool> pc_AbovegroundOrgan; // old KOMP
  std::vector<bool> pc_StorageOrgan;

  double pc_SamplingDepth{0.0};
  double pc_TargetNSamplingDepth{0.0};
  double pc_TargetN30{0.0};
  double pc_MaxNUptakeParam{0.0};
  double pc_RootDistributionParam{0.0};
  int pc_PlantDensity{0}; //! [plants m-2]
  double pc_RootGrowthLag{0.0};
  double pc_MinimumTemperatureRootGrowth{0.0};
  double pc_InitialRootingDepth{0.0};
  double pc_RootPenetrationRate{0.0};
  double pc_RootFormFactor{0.0};
  double pc_SpecificRootLength{0.0};
  int pc_StageAfterCut{0}; // stage number is zero-based
  double pc_LimitingTemperatureHeatStress{0.0};
  int pc_CuttingDelayDays{0};
  double pc_DroughtImpactOnFertilityFactor{0.0};

  double EF_MONO{0.5}; //!< = MTsynt [ug gDW-1 h-1] Monoterpenes, which will be
                       //!< emitted right after synthesis
  double EF_MONOS{0.5};
  //! = MTpool [ug gDW-1 h-1] Monoterpenes, which will be stored after synthesis
  //! in stores (mostly intra- oder intercellular space of leafs and then are
  //! being emitted; quasi evaporation)
  double EF_ISO{0};  //! Isoprene emission factor
  double VCMAX25{0}; //!< maximum RubP saturated rate of carboxylation at 25oC
                     //!< for sun leaves (umol m-2 s-1)
  double AEKC{65800.0};
  //!< activation energy for Michaelis-Menten constant for CO2 (J mol-1) |
  //!< MONICA default=65800.0 | LDNDC default=59356.0
  double AEKO{1400.0};
  //!< activation energy for Michaelis-Menten constant for O2 (J mol-1) | MONICA
  //!< default=65800.0 | LDNDC default=35948.0
  double AEVC{68800.0};
  //!< activation energy for photosynthesis (J mol-1) | MONICA default=68800.0 |
  //!< LDNDC default=58520.0
  double KC25{460.0};
  //!< Michaelis-Menten constant for CO2 at 25oC (umol mol-1 ubar-1) | MONICA
  //!< default=460.0 | LDNDC default=260.0
  double KO25{330.0};
  //!< Michaelis-Menten constant for O2 at 25oC (mmol mol-1 mbar-1) | MONICA
  //!< default=330.0 | LDNDC default=179.0

  int pc_TransitionStageLeafExp{-1}; //!< [1-7]
  int dormancyStartDoy{
      0}; //!< start dormancy of perennial crops at that DOY (0 = unset)
  int dormancyEndDoy{0};
  //!< end dormancy of perennial crops at that DOY, start accumulating
  //!< temperature sums (0 = unset)
};

SpeciesParameters makeSpeciesParameters(
    mas::schema::model::monica::SpeciesParameters::Reader reader);

namespace speciesparameters {

void deserialize(SpeciesParameters *sp,
                 mas::schema::model::monica::SpeciesParameters::Reader reader);
void serialize(const SpeciesParameters *sp,
               mas::schema::model::monica::SpeciesParameters::Builder builder);
Tools::Errors merge(SpeciesParameters *sp, json11::Json j);
json11::Json to_json(const SpeciesParameters *sp);
size_t numberOfDevelopmentalStages(const SpeciesParameters *sp);

// old NRKOM
size_t numberOfOrgans(const SpeciesParameters *sp);

} // namespace speciesparameters

typedef std::shared_ptr<SpeciesParameters> SpeciesParametersPtr;

struct CultivarParameters {
  std::string pc_CultivarId;
  std::string pc_Description;
  bool pc_Perennial{false};
  // std::string pc_PermanentCultivarId;
  double pc_MaxAssimilationRate{0.0}; // old MAXAMAX
  double pc_LightExtinctionCoefficient{0.8};
  double pc_MaxCropHeight{0.0};
  double pc_ResidueNRatio{0.0};
  double pc_LT50cultivar{0.0};

  double pc_CropHeightP1{0.0};
  double pc_CropHeightP2{0.0};
  double pc_CropSpecificMaxRootingDepth{0.0}; // old WUMAXPF [m]

  std::vector<std::vector<double>> pc_AssimilatePartitioningCoeff; // old PRO
  std::vector<std::vector<double>> pc_OrganSenescenceRate;         // old DEAD

  std::vector<double> pc_BaseDaylength; // old DLBAS
  std::vector<double> pc_OptimumTemperature;
  std::vector<double> pc_DaylengthRequirement;     // old DEC
  std::vector<double> pc_DroughtStressThreshold;   // old DRYswell
  std::vector<double> pc_SpecificLeafArea;         // old LAIFKT [ha kg-1]
  std::vector<double> pc_StageKcFactor;            // old Kc
  std::vector<double> pc_StageTemperatureSum;      // old TSUM
  std::vector<double> pc_VernalisationRequirement; // old VSCHWELL

  // FAO-56 Dual Kc: GDD-based trapezoidal curve is initialised in CropModule
  // constructor. Per-stage arrays removed; use vc_Kcb_ini/vc_Kcb_mid/vc_Kcb_end
  // in CropModule instead.

  double pc_HeatSumIrrigationStart{0.0};
  double pc_HeatSumIrrigationEnd{0.0};

  double pc_CriticalTemperatureHeatStress{0.0};
  double pc_BeginSensitivePhaseHeatStress{0.0};
  double pc_EndSensitivePhaseHeatStress{0.0};

  double pc_FrostHardening{0.0};
  double pc_FrostDehardening{0.0};
  double pc_LowTemperatureExposure{0.0};
  double pc_RespiratoryStress{0.0};
  int pc_LatestHarvestDoy{-1};

  std::vector<YieldComponent> pc_OrganIdsForPrimaryYield;
  std::vector<YieldComponent> pc_OrganIdsForSecondaryYield;
  std::vector<YieldComponent> pc_OrganIdsForCutting;

  double pc_EarlyRefLeafExp{12.0}; //!< 12 = wheat (first guess)
  double pc_RefLeafExp{20.0};      //!< 20 = wheat, 22 = maize (first guess)

  double pc_MinTempDev_WE{0.0};
  double pc_OptTempDev_WE{0.0};
  double pc_MaxTempDev_WE{0.0};

  bool winterCrop{false};
};

CultivarParameters makeCultivarParameters(
    mas::schema::model::monica::CultivarParameters::Reader reader);

namespace cultivarparameters {

void deserialize(CultivarParameters *cp,
                 mas::schema::model::monica::CultivarParameters::Reader reader);
void serialize(const CultivarParameters *cp,
               mas::schema::model::monica::CultivarParameters::Builder builder);
Tools::Errors merge(CultivarParameters *cp, json11::Json j);
json11::Json to_json(const CultivarParameters *cp);
inline size_t numberOfDevelopmentalStages(const CultivarParameters *cp) {
  return cp->pc_BaseDaylength.size();
}

} // namespace cultivarparameters

typedef std::shared_ptr<CultivarParameters> CultivarParametersPtr;

struct CropParameters {
  SpeciesParameters speciesParams;
  CultivarParameters cultivarParams;
  // Maybe, because unset should fall back to CropModuleParameters'
  // __enable_vernalisation_factor_fix__ default, not to false.
  kj::Maybe<bool> __enable_vernalisation_factor_fix__;
};

CropParameters
makeCropParameters(mas::schema::model::monica::CropParameters::Reader reader);

namespace cropparameters {

void deserialize(CropParameters *cp,
                 mas::schema::model::monica::CropParameters::Reader reader);
void serialize(const CropParameters *cp,
               mas::schema::model::monica::CropParameters::Builder builder);
Tools::Errors merge(CropParameters *cp, json11::Json j);
Tools::Errors merge(CropParameters *cp, json11::Json sj, json11::Json cj);
json11::Json to_json(const CropParameters *cp);

// old FRUCHT$(AKF)
inline std::string cropName(const CropParameters *cp) {
  return cp->speciesParams.pc_SpeciesId + "/" +
         cp->cultivarParams.pc_CultivarId;
}

} // namespace cropparameters

typedef std::shared_ptr<CropParameters> CropParametersPtr;

enum FertiliserType { mineral, organic, undefined };

/**
 * @brief Parameters for mineral fertiliser.
 * Simple data structure that holds information about mineral fertiliser.
 * @author Xenia Holtmann, Claas Nendel
 */
struct MineralFertilizerParameters {
  std::string id;
  std::string name;
  double vo_Carbamid{0.0}; //!< [%]
  double vo_NH4{0.0};      //!< [%]
  double vo_NO3{0.0};      //!< [%]
};

MineralFertilizerParameters makeMineralFertilizerParameters(
    mas::schema::model::monica::Params::MineralFertilization::Parameters::Reader
        reader);
MineralFertilizerParameters
makeMineralFertilizerParameters(const std::string &id, const std::string &name,
                                double carbamid, double no3, double nh4);

namespace mineralfertilizerparameters {

void deserialize(
    MineralFertilizerParameters *fp,
    mas::schema::model::monica::Params::MineralFertilization::Parameters::Reader
        reader);
void serialize(const MineralFertilizerParameters *fp,
               mas::schema::model::monica::Params::MineralFertilization::
                   Parameters::Builder builder);
Tools::Errors merge(MineralFertilizerParameters *fp, json11::Json j);
json11::Json to_json(const MineralFertilizerParameters *fp);

} // namespace mineralfertilizerparameters

struct NMinApplicationParameters {
  double min{0.0};
  double max{0.0};
  int delayInDays{0};
};

NMinApplicationParameters makeNMinApplicationParameters(double min, double max,
                                                        int delayInDays);
NMinApplicationParameters makeNMinApplicationParameters(
    mas::schema::model::monica::NMinApplicationParameters::Reader reader);

namespace nminapplicationparameters {

void deserialize(
    NMinApplicationParameters *nap,
    mas::schema::model::monica::NMinApplicationParameters::Reader reader);
void serialize(
    const NMinApplicationParameters *nap,
    mas::schema::model::monica::NMinApplicationParameters::Builder builder);
Tools::Errors merge(NMinApplicationParameters *nap, json11::Json j);
json11::Json to_json(const NMinApplicationParameters *nap);

} // namespace nminapplicationparameters

struct IrrigationParameters {
  double nitrateConcentration{0.0}; //!< nitrate concentration [mg dm-3]
  double sulfateConcentration{0.0}; //!< sulfate concentration [mg dm-3]

  // FAO-56 Dual Kc: event-level irrigation physical parameters
  bool isDripIrrigation{
      false};     //!< true = drip irrigation (shading adjustment applied)
  double fw{1.0}; //!< fraction of wetted soil surface [0-1]
};

IrrigationParameters makeIrrigationParameters(double nitrateConcentration,
                                              double sulfateConcentration);
IrrigationParameters makeIrrigationParameters(
    mas::schema::model::monica::Params::Irrigation::Parameters::Reader reader);

namespace irrigationparameters {

void deserialize(
    IrrigationParameters *ip,
    mas::schema::model::monica::Params::Irrigation::Parameters::Reader reader);
void serialize(
    const IrrigationParameters *ip,
    mas::schema::model::monica::Params::Irrigation::Parameters::Builder
        builder);
Tools::Errors merge(IrrigationParameters *ip, json11::Json j);
json11::Json to_json(const IrrigationParameters *ip);

} // namespace irrigationparameters

struct AutomaticIrrigationParameters : public IrrigationParameters {
  Tools::Date startDate;
  Tools::Date endDate;
  double amount{-1.0};
  double percentNFC{-1.0};
  double threshold{-1.0};
  double criticalMoistureDepthM{0.3};
  int minDaysBetweenIrrigationEvents{0};
};

AutomaticIrrigationParameters
makeAutomaticIrrigationParameters(double a, double t, double nc, double sc);
AutomaticIrrigationParameters makeAutomaticIrrigationParameters(
    mas::schema::model::monica::AutomaticIrrigationParameters::Reader reader);

namespace automaticirrigationparameters {

void deserialize(
    AutomaticIrrigationParameters *aip,
    mas::schema::model::monica::AutomaticIrrigationParameters::Reader reader);
void serialize(
    const AutomaticIrrigationParameters *aip,
    mas::schema::model::monica::AutomaticIrrigationParameters::Builder builder);
Tools::Errors merge(AutomaticIrrigationParameters *aip, json11::Json j);
json11::Json to_json(const AutomaticIrrigationParameters *aip);

} // namespace automaticirrigationparameters

struct MeasuredGroundwaterTableInformation {
  bool groundwaterInformationAvailable{false};
  std::map<Tools::Date, double> groundwaterInfo;
};

MeasuredGroundwaterTableInformation makeMeasuredGroundwaterTableInformation(
    mas::schema::model::monica::MeasuredGroundwaterTableInformation::Reader
        reader);

namespace measuredgroundwatertableinformation {

void deserialize(
    MeasuredGroundwaterTableInformation *gwi,
    mas::schema::model::monica::MeasuredGroundwaterTableInformation::Reader
        reader);
void serialize(
    const MeasuredGroundwaterTableInformation *gwi,
    mas::schema::model::monica::MeasuredGroundwaterTableInformation::Builder
        builder);
Tools::Errors merge(MeasuredGroundwaterTableInformation *gwi, json11::Json j);
json11::Json to_json(const MeasuredGroundwaterTableInformation *gwi);
std::pair<bool, double>
getGroundwaterInformation(const MeasuredGroundwaterTableInformation *gwi,
                          Tools::Date gwDate);

} // namespace measuredgroundwatertableinformation

struct SiteParameters {
  double vs_Latitude{52.5};         // ZALF latitude
  double vs_Slope{0.01};            //!< [m m-1]
  double vs_HeightNN{50.0};         //!< [m]
  double vs_GroundwaterDepth{70.0}; //!< [m]
  double vs_Soil_CN_Ratio{10.0};
  double vs_DrainageCoeff{1.0};
  double vq_NDeposition{30.0};                       // [kg N ha-1 y-1]
  double vs_MaxEffectiveRootingDepth{2.0};           // [m]
  double vs_ImpenetrableLayerDepth{-1};              // [m]
  double vs_SoilSpecificHumusBalanceCorrection{0.0}; // humus equivalents
  double bareSoilKcFactor{0.4};

  int numberOfLayers{20};
  double layerThickness{0.1};

  Soil::SoilPMs vs_SoilParameters;
  Tools::J11Array initSoilProfileSpec;
  std::string pwpFcSatFunction{"Wessolek2009"};
  std::map<std::string,
           std::function<Tools::Errors(Soil::SoilParameters *, int)>>
      calculateAndSetPwpFcSatFunctions;
  // MeasuredGroundwaterTableInformation groundwaterInformation;
};

SiteParameters
makeSiteParameters(mas::schema::model::monica::SiteParameters::Reader reader);

namespace siteparameters {

void deserialize(SiteParameters *sp,
                 mas::schema::model::monica::SiteParameters::Reader reader);
void serialize(const SiteParameters *sp,
               mas::schema::model::monica::SiteParameters::Builder builder);
Tools::Errors merge(SiteParameters *sp, json11::Json j);
json11::Json to_json(const SiteParameters *sp);

} // namespace siteparameters

/**
 * @brief Data structure that containts all relevant parameters for the
 * automatic yield trigger.
 */
struct AutomaticHarvestParameters {
  //! Enumeration for defining automatic harvesting times

  //! Definition of different harvest time definition for the automatic
  //! yield trigger
  enum HarvestTime {
    maturity, //!< crop is harvested when maturity is reached
    unknown   //!< default error value
  };

  HarvestTime _harvestTime{unknown}; //!< Harvest time parameter
  int _latestHarvestDOY{-1}; //!< Fallback day for latest harvest of the crop
};

AutomaticHarvestParameters
makeAutomaticHarvestParameters(AutomaticHarvestParameters::HarvestTime yt);
AutomaticHarvestParameters makeAutomaticHarvestParameters(
    mas::schema::model::monica::AutomaticHarvestParameters::Reader reader);

namespace automaticharvestparameters {

void deserialize(
    AutomaticHarvestParameters *ahp,
    mas::schema::model::monica::AutomaticHarvestParameters::Reader reader);
void serialize(
    const AutomaticHarvestParameters *ahp,
    mas::schema::model::monica::AutomaticHarvestParameters::Builder builder);
Tools::Errors merge(AutomaticHarvestParameters *ahp, json11::Json j);
json11::Json to_json(const AutomaticHarvestParameters *ahp);

} // namespace automaticharvestparameters

struct NMinCropParameters {
  double samplingDepth{0.0};
  double nTarget{0.0};
  double nTarget30{0.0};
};

NMinCropParameters makeNMinCropParameters(double samplingDepth, double nTarget,
                                          double nTarget30);
NMinCropParameters makeNMinCropParameters(
    mas::schema::model::monica::NMinCropParameters::Reader reader);

namespace nmincropparameters {

void deserialize(NMinCropParameters *ncp,
                 mas::schema::model::monica::NMinCropParameters::Reader reader);
void serialize(const NMinCropParameters *ncp,
               mas::schema::model::monica::NMinCropParameters::Builder builder);
Tools::Errors merge(NMinCropParameters *ncp, json11::Json j);
json11::Json to_json(const NMinCropParameters *ncp);

} // namespace nmincropparameters

struct OrganicMatterParameters {
  double vo_AOM_DryMatterContent{
      0.0}; //!< Dry matter content of added organic matter [kg DM kg FM-1]
  double vo_AOM_NH4Content{
      0.0}; //!< Ammonium content in added organic matter [kg N kg DM-1]
  double vo_AOM_NO3Content{
      0.0}; //!< Nitrate content in added organic matter [kg N kg DM-1]
  double vo_AOM_CarbamidContent{
      0.0}; //!< Carbamide content in added organic matter [kg N kg DM-1]
  double vo_CorgContent{
      0.0}; //!< Carbon content in added organic matter [kg C kg DM-1]

  double vo_AOM_SlowDecCoeffStandard{
      0.0}; //!< Decomposition rate coefficient of slow AOM at standard
            //!< conditions [d-1]
  double vo_AOM_FastDecCoeffStandard{
      0.0}; //!< Decomposition rate coefficient of fast AOM at standard
            //!< conditions [d-1]

  double vo_PartAOM_to_AOM_Slow{0.0}; //!< Part of AOM that is assigned to the
                                      //!< slowly decomposing pool [kg kg-1
  double vo_PartAOM_to_AOM_Fast{0.0}; //!< Part of AOM that is assigned to the
                                      //!< rapidly decomposing pool [kg kg-1]

  double vo_CN_Ratio_AOM_Slow{
      0.0}; //!< C to N ratio of the slowly decomposing AOM pool []
  double vo_CN_Ratio_AOM_Fast{
      0.0}; //!< C to N ratio of the rapidly decomposing AOM pool []

  double vo_PartAOM_Slow_to_SMB_Slow{
      0.0}; //!< Part of AOM slow consumed by slow soil microbial biomass [kg
            //!< kg-1]
  double vo_PartAOM_Slow_to_SMB_Fast{
      0.0}; //!< Part of AOM slow consumed by fast soil microbial biomass [kg
            //!< kg-1]

  double vo_NConcentration{0.0};
};

OrganicMatterParameters makeOrganicMatterParameters(
    mas::schema::model::monica::Params::OrganicFertilization::
        OrganicMatterParameters::Reader reader);

namespace organicmatterparameters {

void deserialize(OrganicMatterParameters *omp,
                 mas::schema::model::monica::Params::OrganicFertilization::
                     OrganicMatterParameters::Reader reader);
void serialize(const OrganicMatterParameters *omp,
               mas::schema::model::monica::Params::OrganicFertilization::
                   OrganicMatterParameters::Builder builder);
Tools::Errors merge(OrganicMatterParameters *omp, json11::Json j);
json11::Json to_json(const OrganicMatterParameters *omp);

} // namespace organicmatterparameters

typedef std::shared_ptr<OrganicMatterParameters> OrganicMatterParametersPtr;

struct OrganicFertilizerParameters : public OrganicMatterParameters {
  std::string id;
  std::string name;
};

OrganicFertilizerParameters makeOrganicFertilizerParameters(
    mas::schema::model::monica::Params::OrganicFertilization::Parameters::Reader
        reader);

namespace organicfertilizerparameters {

void deserialize(
    OrganicFertilizerParameters *ofp,
    mas::schema::model::monica::Params::OrganicFertilization::Parameters::Reader
        reader);
void serialize(const OrganicFertilizerParameters *ofp,
               mas::schema::model::monica::Params::OrganicFertilization::
                   Parameters::Builder builder);
Tools::Errors merge(OrganicFertilizerParameters *ofp, json11::Json j);
json11::Json to_json(const OrganicFertilizerParameters *ofp);

} // namespace organicfertilizerparameters

typedef std::shared_ptr<OrganicFertilizerParameters>
    OrganicFertiliserParametersPtr;

struct CropResidueParameters : public OrganicMatterParameters {
  std::string species;
  std::string residueType;
};

CropResidueParameters makeCropResidueParameters(
    mas::schema::model::monica::CropResidueParameters::Reader reader);

namespace cropresidueparameters {

void deserialize(
    CropResidueParameters *crp,
    mas::schema::model::monica::CropResidueParameters::Reader reader);
void serialize(
    const CropResidueParameters *crp,
    mas::schema::model::monica::CropResidueParameters::Builder builder);
Tools::Errors merge(CropResidueParameters *crp, json11::Json j);
json11::Json to_json(const CropResidueParameters *crp);

} // namespace cropresidueparameters

typedef std::shared_ptr<CropResidueParameters> CropResidueParametersPtr;

struct SimulationParameters {
  Tools::Date startDate;
  Tools::Date endDate;

  bool pc_NitrogenResponseOn{true};
  bool pc_WaterDeficitResponseOn{true};
  bool pc_EmergenceFloodingControlOn{true};
  bool pc_EmergenceMoistureControlOn{true};
  bool pc_FrostKillOn{true};

  bool p_UseAutomaticIrrigation{false};
  AutomaticIrrigationParameters p_AutoIrrigationParams;

  bool p_UseNMinMineralFertilisingMethod{false};
  MineralFertilizerParameters p_NMinFertiliserPartition;
  NMinApplicationParameters p_NMinUserParams;

  bool p_UseSecondaryYields{true};
  bool p_UseAutomaticHarvestTrigger{false};

  int p_NumberOfLayers{20};
  double p_LayerThickness{0.1};

  int p_StartPVIndex{0};
  int p_JulianDayAutomaticFertilising{0};

  bool serializeMonicaStateAtEnd{false};
  bool serializeMonicaStateAtEndToJson{false};
  std::string pathToSerializationAtEndFile;
  bool loadSerializedMonicaStateAtStart{false};
  bool deserializedMonicaStateFromJson{false};
  std::string pathToLoadSerializationFile;
  uint64_t noOfPreviousDaysSerializedClimateData{0};

  // FAO-56 Dual Kc: global method switch (read from sim.json
  // "evapotranspiration-method": "FAO-56-Dual") Irrigation physical params
  // (isDripIrrigation, fw) are now set at the Irrigation workstep event level.
  bool dualKcMethod{false}; //!< Use FAO-56 Dual Kc evaporation partitioning
};

SimulationParameters makeSimulationParameters(
    mas::schema::model::monica::SimulationParameters::Reader reader);

namespace simulationparameters {

void deserialize(
    SimulationParameters *sp,
    mas::schema::model::monica::SimulationParameters::Reader reader);
void serialize(
    const SimulationParameters *sp,
    mas::schema::model::monica::SimulationParameters::Builder builder);
Tools::Errors merge(SimulationParameters *sp, json11::Json j);
json11::Json to_json(const SimulationParameters *sp);

} // namespace simulationparameters

/**
 * Class that holds information of crop defined by user.
 * @author Xenia Specka
 */
struct CropModuleParameters {
  double pc_CanopyReflectionCoefficient{0.0};
  double pc_ReferenceMaxAssimilationRate{0.0};
  double pc_ReferenceLeafAreaIndex{0.0};
  double pc_MaintenanceRespirationParameter1{0.0};
  double pc_MaintenanceRespirationParameter2{0.0};
  double pc_MinimumNConcentrationRoot{0.0};
  double pc_MinimumAvailableN{0.0}; // [kg m-2]
  double pc_ReferenceAlbedo{0.0};
  double pc_StomataConductanceAlpha{0.0};
  double pc_SaturationBeta{0.0};
  double pc_GrowthRespirationRedux{0.0};
  double pc_MaxCropNDemand{0.0};
  double pc_GrowthRespirationParameter1{0.0};
  double pc_GrowthRespirationParameter2{0.0};
  double pc_Tortuosity{0.0}; // old AD
  bool pc_AdjustRootDepthForSoilProps{true};
  std::vector<int> pc_TimeUnderAnoxiaThreshold{4, 4, 4, 4, 4, 4, 4};

  bool __enable_Phenology_WangEngelTemperatureResponse__{false};
  bool __enable_Photosynthesis_WangEngelTemperatureResponse__{false};
  bool __enable_hourly_FvCB_photosynthesis__{false};
  bool __enable_T_response_leaf_expansion__{false};
  bool __disable_daily_root_biomass_to_soil__{false};
  bool __enable_vernalisation_factor_fix__{false};
  bool __enable_PASW_root_penetration__{false};

  bool isIntercropping{false};
  bool sequentialWaterUse{false};
  bool twoWaySync{true};
  double pc_intercropping_k_s{0.0};
  double pc_intercropping_k_t{0.0};
  double pc_intercropping_phRedux{0.5};
  double pc_intercropping_dvs_phr{5.791262};
  bool pc_intercropping_autoPhRedux{true};
  std::string pc_intercropping_reader_sr;
  std::string pc_intercropping_writer_sr;
};

CropModuleParameters makeCropModuleParameters(
    mas::schema::model::monica::CropModuleParameters::Reader reader);

namespace cropmoduleparameters {

void deserialize(
    CropModuleParameters *cmp,
    mas::schema::model::monica::CropModuleParameters::Reader reader);
void serialize(
    const CropModuleParameters *cmp,
    mas::schema::model::monica::CropModuleParameters::Builder builder);
Tools::Errors merge(CropModuleParameters *cmp, json11::Json j);
json11::Json to_json(const CropModuleParameters *cmp);

} // namespace cropmoduleparameters

/**
 * Class that holds information about user defined environment parameters.
 * @author Xenia Specka
 */
struct EnvironmentParameters {
  double p_Albedo{0.23};
  mas::schema::climate::RCP rcp{mas::schema::climate::RCP::RCP85};
  double p_AtmosphericCO2{0.0};
  std::map<int, double> p_AtmosphericCO2s;
  double p_AtmosphericO3{0.0};
  std::map<int, double> p_AtmosphericO3s;
  double p_WindSpeedHeight{2.0};
  double p_LeachingDepth{0.0};
  double p_timeStep{0.0};

  double p_MaxGroundwaterDepth{18.0};
  double p_MinGroundwaterDepth{20.0};
  int p_MinGroundwaterDepthMonth{3};
};

EnvironmentParameters makeEnvironmentParameters(
    mas::schema::model::monica::EnvironmentParameters::Reader reader);

namespace environmentparameters {

void deserialize(
    EnvironmentParameters *ep,
    mas::schema::model::monica::EnvironmentParameters::Reader reader);
void serialize(
    const EnvironmentParameters *ep,
    mas::schema::model::monica::EnvironmentParameters::Builder builder);
Tools::Errors merge(EnvironmentParameters *ep, json11::Json j);
json11::Json to_json(const EnvironmentParameters *ep);

} // namespace environmentparameters

/**
 * Class that holds information about user defined soil moisture parameters.
 * @author Xenia Specka
 */
struct SoilMoistureModuleParameters {
  std::function<double(std::string, size_t)> getCapillaryRiseRate{
      [](std::string soilTexture, size_t distance) { return 0.0; }};

  // double pm_CriticalMoistureDepth{ 0.0 };
  double pm_SaturatedHydraulicConductivity{0.0};
  double pm_SurfaceRoughness{0.0};
  double pm_GroundwaterDischarge{0.0};
  double pm_HydraulicConductivityRedux{0.0};
  double pm_SnowAccumulationTresholdTemperature{0.0};
  double pm_KcFactor{0.0};
  double pm_TemperatureLimitForLiquidWater{0.0};
  double pm_CorrectionSnow{0.0};
  double pm_CorrectionRain{0.0};
  double pm_SnowMaxAdditionalDensity{0.0};
  double pm_NewSnowDensityMin{0.0};
  double pm_SnowRetentionCapacityMin{0.0};
  double pm_RefreezeParameter1{0.0};
  double pm_RefreezeParameter2{0.0};
  double pm_RefreezeTemperature{0.0};
  double pm_SnowMeltTemperature{0.0};
  double pm_SnowPacking{0.0};
  double pm_SnowRetentionCapacityMax{0.0};
  double pm_EvaporationZeta{0.0};
  double pm_XSACriticalSoilMoisture{0.0};
  double pm_MaximumEvaporationImpactDepth{0.0};
  double pm_MaxPercolationRate{0.0};
  double pm_MoistureInitValue{0.0};
};

SoilMoistureModuleParameters makeSoilMoistureModuleParameters(
    mas::schema::model::monica::SoilMoistureModuleParameters::Reader reader);

namespace soilmoisturemoduleparameters {

void deserialize(
    SoilMoistureModuleParameters *smp,
    mas::schema::model::monica::SoilMoistureModuleParameters::Reader reader);
void serialize(
    const SoilMoistureModuleParameters *smp,
    mas::schema::model::monica::SoilMoistureModuleParameters::Builder builder);
Tools::Errors merge(SoilMoistureModuleParameters *smp, json11::Json j);
json11::Json to_json(const SoilMoistureModuleParameters *smp);

} // namespace soilmoisturemoduleparameters

/**
 * Class that holds information about user defined soil temperature parameters.
 * @author Xenia Specka
 */
struct SoilTemperatureModuleParameters {
  double pt_NTau{0.0};
  double pt_InitialSurfaceTemperature{0.0};
  double pt_BaseTemperature{0.0};
  double pt_QuartzRawDensity{0.0};
  double pt_DensityAir{0.0};
  double pt_DensityWater{0.0};
  double pt_DensityHumus{0.0};
  double pt_SpecificHeatCapacityAir{0.0};
  double pt_SpecificHeatCapacityQuartz{0.0};
  double pt_SpecificHeatCapacityWater{0.0};
  double pt_SpecificHeatCapacityHumus{0.0};
  double pt_SoilAlbedo{0.0};
  double pt_SoilMoisture{0.25};
};

SoilTemperatureModuleParameters makeSoilTemperatureModuleParameters(
    mas::schema::model::monica::SoilTemperatureModuleParameters::Reader reader);

namespace soiltemperaturemoduleparameters {

void deserialize(
    SoilTemperatureModuleParameters *stp,
    mas::schema::model::monica::SoilTemperatureModuleParameters::Reader reader);
void serialize(
    const SoilTemperatureModuleParameters *stp,
    mas::schema::model::monica::SoilTemperatureModuleParameters::Builder
        builder);
Tools::Errors merge(SoilTemperatureModuleParameters *stp, json11::Json j);
json11::Json to_json(const SoilTemperatureModuleParameters *stp);

} // namespace soiltemperaturemoduleparameters

/**
 * Class that holds information about user defined soil transport parameters.
 * @author Xenia Specka
 */
struct SoilTransportModuleParameters {
  double pq_DispersionLength{0.0};
  double pq_AD{0.0};
  double pq_DiffusionCoefficientStandard{0.0};
  double pq_NDeposition{0.0};
};

SoilTransportModuleParameters makeSoilTransportModuleParameters(
    mas::schema::model::monica::SoilTransportModuleParameters::Reader reader);

namespace soiltransportmoduleparameters {

void deserialize(
    SoilTransportModuleParameters *stp,
    mas::schema::model::monica::SoilTransportModuleParameters::Reader reader);
void serialize(
    const SoilTransportModuleParameters *stp,
    mas::schema::model::monica::SoilTransportModuleParameters::Builder builder);
Tools::Errors merge(SoilTransportModuleParameters *stp, json11::Json j);
json11::Json to_json(const SoilTransportModuleParameters *stp);

} // namespace soiltransportmoduleparameters

struct SticsParameters {
  bool use_n2o{false};
  bool use_nit{false};
  bool use_denit{false};
  int code_vnit{1};
  int code_tnit{2};
  int code_rationit{2};
  int code_hourly_wfps_nit{2};
  int code_pdenit{1};
  int code_ratiodenit{2};
  int code_hourly_wfps_denit{2};
  double hminn{0.3};
  double hoptn{0.9};
  double pHminnit{4.0};
  double pHmaxnit{7.2};
  double nh4_min{1.0}; // [mg NH4-N/kg soil]
  double pHminden{7.2};
  double pHmaxden{9.2};
  double wfpsc{0.62};
  double tdenitopt_gauss{47}; // [°C]
  double scale_tdenitopt{25}; // [°C]
  double Kd{148};             // [mg NO3-N/L]
  double k_desat{3.0};        // [1/day]
  double fnx{0.8};            // [1/day]
  double vnitmax{27.3};       // [mg NH4-N/kg soil/day]
  double Kamm{24};            // [mg NH4-N/L]
  double tnitmin{5.0};        // [°C]
  double tnitopt{30.0};       // [°C]
  double tnitop2{35.0};       // [°C]
  double tnitmax{58.0};       // [°C]
  double tnitopt_gauss{32.5}; // [°C]
  double scale_tnitopt{16.0}; // [°C]
  double rationit{0.0016};
  double cmin_pdenit{1.0}; // [% [0-100]]
  double cmax_pdenit{6.0}; // [% [0-100]]
  double min_pdenit{1.0};  // [mg N/Kg soil/day]
  double max_pdenit{20.0}; // [mg N/kg soil/day]
  double ratiodenit{0.2};
  double profdenit{20};  // [cm]
  double vpotdenit{2.0}; // [kg N/ha/day]
};

SticsParameters
makeSticsParameters(mas::schema::model::monica::SticsParameters::Reader reader);

namespace sticsparameters {

void deserialize(SticsParameters *sp,
                 mas::schema::model::monica::SticsParameters::Reader reader);
void serialize(const SticsParameters *sp,
               mas::schema::model::monica::SticsParameters::Builder builder);
Tools::Errors merge(SticsParameters *sp, json11::Json j);
json11::Json to_json(const SticsParameters *sp);

} // namespace sticsparameters

/**
 * Class that holds information about user-defined soil organic parameters.
 * @author Claas Nendel
 */
struct SoilOrganicModuleParameters {
  double po_SOM_SlowDecCoeffStandard{
      4.30e-5}; // 4.30e-5 [d-1], Bruun et al. 2003 4.3e-5
  double po_SOM_FastDecCoeffStandard{
      1.40e-4}; // 1.40e-4 [d-1], from DAISY manual 1.4e-4
  double po_SMB_SlowMaintRateStandard{
      1.00e-3}; // 1.00e-3 [d-1], from DAISY manual original 1.8e-3
  double po_SMB_FastMaintRateStandard{
      1.00e-2}; // 1.00e-2 [d-1], from DAISY manual
  double po_SMB_SlowDeathRateStandard{
      1.00e-3}; // 1.00e-3 [d-1], from DAISY manual
  double po_SMB_FastDeathRateStandard{
      1.00e-2};                              // 1.00e-2 [d-1], from DAISY manual
  double po_SMB_UtilizationEfficiency{0.60}; // 0.60 [], from DAISY manual 0.6
  double po_SOM_SlowUtilizationEfficiency{
      0.40}; // 0.40 [], from DAISY manual 0.4
  double po_SOM_FastUtilizationEfficiency{
      0.50}; // 0.50 [], from DAISY manual 0.5
  double po_AOM_SlowUtilizationEfficiency{
      0.40}; // 0.40 [], from DAISY manual original 0.13
  double po_AOM_FastUtilizationEfficiency{
      0.10}; // 0.10 [], from DAISY manual original 0.69
  double po_AOM_FastMaxC_to_N{1000.0};      // 1000.0
  double po_PartSOM_Fast_to_SOM_Slow{0.30}; // 0.30 [], Bruun et al. 2003
  double po_PartSMB_Slow_to_SOM_Fast{0.60}; // 0.60 [], from DAISY manual
  double po_PartSMB_Fast_to_SOM_Fast{0.60}; // 0.60 [], from DAISY manual
  double po_PartSOM_to_SMB_Slow{0.0150};    // 0.0150 [], optimised
  double po_PartSOM_to_SMB_Fast{0.0002};    // 0.0002 [], optimised
  double po_CN_Ratio_SMB{6.70};             // 6.70 [], from DAISY manual
  double po_LimitClayEffect{0.25};          // 0.25 [kg kg-1], from DAISY manual
  double po_QTenFactor{2.9};    // 2.4 [] default value, analysis literature
  double po_TempDecOptimal{38}; // 38 [°C] default value, analysis literature
  double po_MoistureDecOptimal{
      0.45}; // 0.45 [fraction] default value, analysis literature
  double po_AmmoniaOxidationRateCoeffStandard{
      1.0e-1}; // 1.0e-1 [d-1], from DAISY manual
  double po_NitriteOxidationRateCoeffStandard{
      9.0e-1};                       // 9.0e-1 [d-1], fudged by Florian Stange
  double po_TransportRateCoeff{0.1}; // 0.1 [d-1], from DAISY manual
  double po_SpecAnaerobDenitrification{0.1}; // 0.1 [g gas-N g CO2-C-1]
  double po_ImmobilisationRateCoeffNO3{0.5}; // 0.5 [d-1]
  double po_ImmobilisationRateCoeffNH4{0.5}; // 0.5 [d-1]
  double po_Denit1{0.2};                     // 0.2 Denitrification parameter
  double po_Denit2{0.8};                     // 0.8 Denitrification parameter
  double po_Denit3{0.9};                     // 0.9 Denitrification parameter
  double po_HydrolysisKM{0.00334};           // 0.00334 from Tabatabai 1973
  double po_ActivationEnergy{41000.0};       // 41000.0 from Gould et al. 1973
  double po_HydrolysisP1{4.259e-12}; // 4.259e-12 from Sadeghi et al. 1988
  double po_HydrolysisP2{1.408e-12}; // 1.408e-12 from Sadeghi et al. 1988
  double po_AtmosphericResistance{
      0.0025};                      // 0.0025 [s m-1], from Sadeghi et al. 1988
  double po_N2OProductionRate{0.5}; // 0.5 [d-1]
  double po_Inhibitor_NH3{
      1.0}; // 1.0 [kg N m-3] NH3-induced inhibitor for nitrite oxidation
  double ps_MaxMineralisationDepth{0.4};

  bool __enable_kaiteew_TempOnDecompostion__{true};
  bool __enable_kaiteew_MoistOnDecompostion__{true};
  bool __enable_kaiteew_ClayOnDecompostion__{true};
  SticsParameters sticsParams;
};

SoilOrganicModuleParameters makeSoilOrganicModuleParameters(
    mas::schema::model::monica::SoilOrganicModuleParameters::Reader reader);

namespace soilorganicmoduleparameters {

void deserialize(
    SoilOrganicModuleParameters *sop,
    mas::schema::model::monica::SoilOrganicModuleParameters::Reader reader);
void serialize(
    const SoilOrganicModuleParameters *sop,
    mas::schema::model::monica::SoilOrganicModuleParameters::Builder builder);
Tools::Errors merge(SoilOrganicModuleParameters *sop, json11::Json j);
json11::Json to_json(const SoilOrganicModuleParameters *sop);

} // namespace soilorganicmoduleparameters

/**
 * @brief Central data distribution class.
 *
 * Class that holds pointers and direct information of user defined parameters.
 *
 * @author Xenia Specka
 */
struct CentralParameterProvider {
  CropModuleParameters userCropParameters;
  EnvironmentParameters userEnvironmentParameters;
  SoilMoistureModuleParameters userSoilMoistureParameters;
  SoilTemperatureModuleParameters userSoilTemperatureParameters;
  SoilTransportModuleParameters userSoilTransportParameters;
  SoilOrganicModuleParameters userSoilOrganicParameters;
  SimulationParameters simulationParameters;

  SiteParameters siteParameters;  //! site specific parameters
  Soil::OrganicConstants organic; //! constant organic parameters to the model

  MeasuredGroundwaterTableInformation groundwaterInformation;

  // bool writeOutputFiles() const { return _writeOutputFiles; }
  // void setWriteOutputFiles(bool write) { _writeOutputFiles = write; }

  // bool _writeOutputFiles{false};
  std::string _pathToOutputDir{"."};

  std::vector<double> precipCorrectionValues{std::vector<double>(12, 1.0)};
};

namespace centralparameterprovider {

Tools::Errors merge(CentralParameterProvider *cpp, json11::Json j);
json11::Json to_json(const CentralParameterProvider *cpp);
double getPrecipCorrectionValue(const CentralParameterProvider *cpp, int month);
void setPrecipCorrectionValue(CentralParameterProvider *cpp, int month,
                              double value);

inline std::string pathToOutputDir(const CentralParameterProvider *cpp) {
  return cpp->_pathToOutputDir.empty() ? "./" : cpp->_pathToOutputDir;
}

} // namespace centralparameterprovider

struct Intercropping {
  typedef mas::schema::model::monica::ICData ICD;
  typedef mas::schema::fbp::Channel<ICD>::ChanReader Reader;
  typedef mas::schema::fbp::Channel<ICD>::ChanWriter Writer;
  kj::AsyncIoContext *ioContext{nullptr};
  Reader::Client reader{nullptr};
  Writer::Client writer{nullptr};
  bool isAsync() const { return ioContext != nullptr; }
};
} // namespace monica
