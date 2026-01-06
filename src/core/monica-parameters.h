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

#include <map>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <cassert>

#include <kj/async-io.h>
#include <kj/common.h>

#include "json11/json11.hpp"

#include "fbp.capnp.h"
#include "model/monica/monica_params.capnp.h"
#include "model/monica/monica_state.capnp.h"

#include "common/dll-exports.h"
#include "climate/climate-common.h"
#include "tools/date.h"
#include "json11/json11-helper.h"
#include "soil/soil.h"
#include "soil/constants.h"

namespace monica {

class Run {
public:
  virtual void run() = 0;
};

class CentralParameterProvider;

enum Eva2_Nutzung {
  NUTZUNG_UNDEFINED = 0,
  NUTZUNG_GANZPFLANZE = 1,
  NUTZUNG_KORN = 2,
  NUTZUNG_GRUENDUENGUNG = 7,
  NUTZUNG_CCM = 8
};

/**
 * @brief
 */
struct DLL_API YieldComponent : public Tools::Json11Serializable {
  YieldComponent() {}

  YieldComponent(int organId, double yieldPercentage, double yieldDryMatter);

  YieldComponent(mas::schema::model::monica::YieldComponent::Reader reader) { deserialize(reader); }

  void deserialize(mas::schema::model::monica::YieldComponent::Reader reader);

  void serialize(mas::schema::model::monica::YieldComponent::Builder builder) const;

  // YieldComponent(json11::Json object);

  virtual Tools::Errors merge(json11::Json j);

  virtual json11::Json to_json() const;

  int organId{ -1 };
  double yieldPercentage{ 0.0 };
  double yieldDryMatter{ 0.0 };
};

struct DLL_API SpeciesParameters : public Tools::Json11Serializable {
  SpeciesParameters() {}

  SpeciesParameters(json11::Json j);

  SpeciesParameters(mas::schema::model::monica::SpeciesParameters::Reader reader) { deserialize(reader); }

  void deserialize(mas::schema::model::monica::SpeciesParameters::Reader reader);

  void serialize(mas::schema::model::monica::SpeciesParameters::Builder builder) const;

  virtual Tools::Errors merge(json11::Json j);

  virtual json11::Json to_json() const;

  size_t pc_NumberOfDevelopmentalStages() const;

  size_t pc_NumberOfOrgans() const;

  // members
  std::string pc_SpeciesId;
  int pc_CarboxylationPathway{ 0 };
  double pc_DefaultRadiationUseEfficiency{ 0.0 };
  double pc_PartBiologicalNFixation{ 0.0 };
  double pc_InitialKcFactor{ 0.0 };
  double pc_LuxuryNCoeff{ 0.0 };
  double pc_MaxCropDiameter{ 0.0 };
  double pc_StageAtMaxHeight{ 0.0 };
  double pc_StageAtMaxDiameter{ 0.0 };
  double pc_MinimumNConcentration{ 0.0 };
  double pc_MinimumTemperatureForAssimilation{ 0.0 };
  double pc_OptimumTemperatureForAssimilation{ 0.0 };
  double pc_MaximumTemperatureForAssimilation{ 0.0 };
  double pc_NConcentrationAbovegroundBiomass{ 0.0 };
  double pc_NConcentrationB0{ 0.0 };
  double pc_NConcentrationPN{ 0.0 };
  double pc_NConcentrationRoot{ 0.0 };
  int pc_DevelopmentAccelerationByNitrogenStress{ 0 };
  double pc_FieldConditionModifier{ 1.0 };
  double pc_AssimilateReallocation{ 0.0 };

  std::vector<double> pc_BaseTemperature;
  std::vector<double> pc_OrganMaintenanceRespiration;
  std::vector<double> pc_OrganGrowthRespiration;
  std::vector<double> pc_StageMaxRootNConcentration;
  std::vector<double> pc_InitialOrganBiomass;
  std::vector<double> pc_CriticalOxygenContent;
  std::vector<double> pc_StageMobilFromStorageCoeff;

  std::vector<bool> pc_AbovegroundOrgan;
  std::vector<bool> pc_StorageOrgan;

  double pc_SamplingDepth{ 0.0 };
  double pc_TargetNSamplingDepth{ 0.0 };
  double pc_TargetN30{ 0.0 };
  double pc_MaxNUptakeParam{ 0.0 };
  double pc_RootDistributionParam{ 0.0 };
  int pc_PlantDensity{ 0 }; //! [plants m-2]
  double pc_RootGrowthLag{ 0.0 };
  double pc_MinimumTemperatureRootGrowth{ 0.0 };
  double pc_InitialRootingDepth{ 0.0 };
  double pc_RootPenetrationRate{ 0.0 };
  double pc_RootFormFactor{ 0.0 };
  double pc_SpecificRootLength{ 0.0 };
  int pc_StageAfterCut{ 0 }; //!< stage number is zero-based
  double pc_LimitingTemperatureHeatStress{ 0.0 };
  int pc_CuttingDelayDays{ 0 };
  double pc_DroughtImpactOnFertilityFactor{ 0.0 };

  double EF_MONO{ 0.5 }; //!< = MTsynt [ug gDW-1 h-1] Monoterpenes, which will be emitted right after synthesis
  double EF_MONOS{ 0.5 }; //! = MTpool [ug gDW-1 h-1] Monoterpenes, which will be stored after synthesis in stores (mostly intra- oder intercellular space of leafs and then are being emitted; quasi evaporation)
  double EF_ISO{ 0 }; //! Isoprene emission factor
  double VCMAX25{ 0 }; //!< maximum RubP saturated rate of carboxylation at 25oC for sun leaves (umol m-2 s-1)
  double AEKC{ 65800.0 }; //!< activation energy for Michaelis-Menten constant for CO2 (J mol-1) | MONICA default=65800.0 | LDNDC default=59356.0
  double AEKO{ 1400.0 }; //!< activation energy for Michaelis-Menten constant for O2 (J mol-1) | MONICA default=65800.0 | LDNDC default=35948.0
  double AEVC{ 68800.0 }; //!< activation energy for photosynthesis (J mol-1) | MONICA default=68800.0 | LDNDC default=58520.0
  double KC25{ 460.0 }; //!< Michaelis-Menten constant for CO2 at 25oC (umol mol-1 ubar-1) | MONICA default=460.0 | LDNDC default=260.0
  double KO25{ 330.0 }; //!< Michaelis-Menten constant for O2 at 25oC (mmol mol-1 mbar-1) | MONICA default=330.0 | LDNDC default=179.0

  int pc_TransitionStageLeafExp{ -1 }; //!< [1-7]
  int dormancyStartDoy {0}; //!< start dormancy of perennial crops at that DOY (0 = unset)
  int dormancyEndDoy {0}; //!< end dormancy of perennial crops at that DOY, start accumulating temperature sums (0 = unset)

};

typedef std::shared_ptr<SpeciesParameters> SpeciesParametersPtr;


struct DLL_API CultivarParameters : public Tools::Json11Serializable {
  CultivarParameters() {}

  CultivarParameters(mas::schema::model::monica::CultivarParameters::Reader reader) { deserialize(reader); }

  void deserialize(mas::schema::model::monica::CultivarParameters::Reader reader);

  // CultivarParameters(json11::Json object);

  void serialize(mas::schema::model::monica::CultivarParameters::Builder builder) const;

  virtual Tools::Errors merge(json11::Json j);

  virtual json11::Json to_json() const;

  size_t pc_NumberOfDevelopmentalStages() const { return pc_BaseDaylength.size(); }

  std::string pc_CultivarId;
  std::string pc_Description;
  bool pc_Perennial{ false };
  //std::string pc_PermanentCultivarId;
  double pc_MaxAssimilationRate{ 0.0 };
  double pc_LightExtinctionCoefficient{ 0.8 };
  double pc_MaxCropHeight{ 0.0 };
  double pc_ResidueNRatio{ 0.0 };
  double pc_LT50cultivar{ 0.0 };

  double pc_CropHeightP1{ 0.0 };
  double pc_CropHeightP2{ 0.0 };
  double pc_CropSpecificMaxRootingDepth{ 0.0 };

  std::vector<std::vector<double>> pc_AssimilatePartitioningCoeff;
  std::vector<std::vector<double>> pc_OrganSenescenceRate;

  std::vector<double> pc_BaseDaylength;
  std::vector<double> pc_OptimumTemperature;
  std::vector<double> pc_DaylengthRequirement;
  std::vector<double> pc_DroughtStressThreshold;
  std::vector<double> pc_SpecificLeafArea;
  std::vector<double> pc_StageKcFactor;
  std::vector<double> pc_StageTemperatureSum;
  std::vector<double> pc_VernalisationRequirement;

  double pc_HeatSumIrrigationStart{ 0.0 };
  double pc_HeatSumIrrigationEnd{ 0.0 };

  double pc_CriticalTemperatureHeatStress{ 0.0 };
  double pc_BeginSensitivePhaseHeatStress{ 0.0 };
  double pc_EndSensitivePhaseHeatStress{ 0.0 };

  double pc_FrostHardening{ 0.0 };
  double pc_FrostDehardening{ 0.0 };
  double pc_LowTemperatureExposure{ 0.0 };
  double pc_RespiratoryStress{ 0.0 };
  int pc_LatestHarvestDoy{ -1 };

  std::vector<YieldComponent> pc_OrganIdsForPrimaryYield;
  std::vector<YieldComponent> pc_OrganIdsForSecondaryYield;
  std::vector<YieldComponent> pc_OrganIdsForCutting;

  double pc_EarlyRefLeafExp{ 12.0 }; //!< 12 = wheat (first guess)
  double pc_RefLeafExp{ 20.0 }; //!< 20 = wheat, 22 = maize (first guess)

  double pc_MinTempDev_WE{ 0.0 };
  double pc_OptTempDev_WE{ 0.0 };
  double pc_MaxTempDev_WE{ 0.0 };

  bool winterCrop{ false };
};

typedef std::shared_ptr<CultivarParameters> CultivarParametersPtr;


struct DLL_API CropParameters : public Tools::Json11Serializable {
  CropParameters() = default;

  explicit CropParameters(mas::schema::model::monica::CropParameters::Reader reader) { deserialize(reader); }

  void deserialize(mas::schema::model::monica::CropParameters::Reader reader);

  // CropParameters(json11::Json object);

  //CropParameters(json11::Json speciesObject, json11::Json cultivarObject);

  void serialize(mas::schema::model::monica::CropParameters::Builder builder) const;

  Tools::Errors merge(json11::Json j) override;

  Tools::Errors merge(json11::Json sj, json11::Json cj);

  json11::Json to_json() const override;

  std::string pc_CropName() const { return speciesParams.pc_SpeciesId + "/" + cultivarParams.pc_CultivarId; }

  SpeciesParameters speciesParams;
  CultivarParameters cultivarParams;
  kj::Maybe<bool> __enable_vernalisation_factor_fix__;
};

typedef std::shared_ptr<CropParameters> CropParametersPtr;


enum FertiliserType { mineral, organic, undefined };

/**
 * @brief Parameters for mineral fertiliser.
 * Simple data structure that holds information about mineral fertiliser.
 * @author Xenia Holtmann, Claas Nendel
 */
class DLL_API MineralFertilizerParameters : public Tools::Json11Serializable {
public:
  MineralFertilizerParameters() {}

  MineralFertilizerParameters(mas::schema::model::monica::Params::MineralFertilization::Parameters::Reader reader) { deserialize(reader); }

  void deserialize(mas::schema::model::monica::Params::MineralFertilization::Parameters::Reader reader);

  // MineralFertilizerParameters(json11::Json object);

  MineralFertilizerParameters(const std::string& id,
    const std::string& name,
    double carbamid,
    double no3,
    double nh4);

  void serialize(mas::schema::model::monica::Params::MineralFertilization::Parameters::Builder builder) const;

  virtual Tools::Errors merge(json11::Json j);

  virtual json11::Json to_json() const;

  std::string getId() const { return id; }
  void setId(const std::string& id) { this->id = id; }

  inline std::string getName() const { return name; }
  inline void setName(const std::string& name) { this->name = name; }

  //! @brief Returns carbamid part in percentage of fertiliser.
  //! @return Carbamid in percent
  inline double getCarbamid() const { return vo_Carbamid; }

  //! Sets carbamid part of fertilisers
  //! @param vo_Carbamid percent
  inline void setCarbamid(double carbamid) { vo_Carbamid = carbamid; }

  //! Returns ammonium part of fertiliser.
  //! @return Ammonium in percent
  inline double getNH4() const { return vo_NH4; }

  //! Sets nitrat part of fertiliser.
  //! @param vo_NH4
  inline void setNH4(double NH4) { vo_NH4 = NH4; }

  //! Returns nitrat part of fertiliser
  //! @return Nitrat in percent
  inline double getNO3() const { return vo_NO3; }

  //! Sets nitrat part of fertiliser.
  //! @param vo_NO3
  inline void setNO3(double NO3) { vo_NO3 = NO3; }

private:
  std::string id;
  std::string name;
  double vo_Carbamid{ 0.0 }; //!< [%]
  double vo_NH4{ 0.0 }; //!< [%]
  double vo_NO3{ 0.0 }; //!< [%]
};


struct DLL_API NMinApplicationParameters : public Tools::Json11Serializable {
  NMinApplicationParameters() {}

  NMinApplicationParameters(double min, double max, int delayInDays);

  NMinApplicationParameters(mas::schema::model::monica::NMinApplicationParameters::Reader reader) { deserialize(reader); }

  void deserialize(mas::schema::model::monica::NMinApplicationParameters::Reader reader);

  // NMinApplicationParameters(json11::Json object);

  void serialize(mas::schema::model::monica::NMinApplicationParameters::Builder builder) const;

  virtual Tools::Errors merge(json11::Json j);

  virtual json11::Json to_json() const;

  double min{ 0.0 };
  double max{ 0.0 };
  int delayInDays{ 0 };
};


struct DLL_API IrrigationParameters : public Tools::Json11Serializable {
  IrrigationParameters() {}

  IrrigationParameters(double nitrateConcentration, double sulfateConcentration);

  IrrigationParameters(mas::schema::model::monica::Params::Irrigation::Parameters::Reader reader) { deserialize(reader); }

  void deserialize(mas::schema::model::monica::Params::Irrigation::Parameters::Reader reader);

  // IrrigationParameters(json11::Json object);

  void serialize(mas::schema::model::monica::Params::Irrigation::Parameters::Builder builder) const;

  virtual Tools::Errors merge(json11::Json j);

  virtual json11::Json to_json() const;

  double nitrateConcentration{ 0.0 }; //!< nitrate concentration [mg dm-3]
  double sulfateConcentration{ 0.0 }; //!< sulfate concentration [mg dm-3]
};


struct DLL_API AutomaticIrrigationParameters : public IrrigationParameters {
  AutomaticIrrigationParameters() {}

  AutomaticIrrigationParameters(double a, double t, double nc, double sc);

  AutomaticIrrigationParameters(mas::schema::model::monica::AutomaticIrrigationParameters::Reader reader) { deserialize(reader); }

  void deserialize(mas::schema::model::monica::AutomaticIrrigationParameters::Reader reader);

  // AutomaticIrrigationParameters(json11::Json object);

  void serialize(mas::schema::model::monica::AutomaticIrrigationParameters::Builder builder) const;

  virtual Tools::Errors merge(json11::Json j);

  virtual json11::Json to_json() const;

  Tools::Date startDate;
  double amount{ -1.0 };
  double percentNFC{ -1.0 };
  double threshold{ -1.0 };
  double criticalMoistureDepthM{ 0.3 };
  int minDaysBetweenIrrigationEvents{ 0 };
};


class DLL_API MeasuredGroundwaterTableInformation : public Tools::Json11Serializable {
public:
  MeasuredGroundwaterTableInformation() {}

  MeasuredGroundwaterTableInformation(mas::schema::model::monica::MeasuredGroundwaterTableInformation::Reader reader) { deserialize(reader); }

  void deserialize(mas::schema::model::monica::MeasuredGroundwaterTableInformation::Reader reader);

  // MeasuredGroundwaterTableInformation(json11::Json object);

  void serialize(mas::schema::model::monica::MeasuredGroundwaterTableInformation::Builder builder) const;

  virtual Tools::Errors merge(json11::Json j);

  virtual json11::Json to_json() const;

  //void readInGroundwaterInformation(std::string path);

		std::pair<bool, double> getGroundwaterInformation(Tools::Date gwDate) const;

  bool isGroundwaterInformationAvailable() const { return groundwaterInformationAvailable; }

private:
  bool groundwaterInformationAvailable{ false };
  std::map<Tools::Date, double> groundwaterInfo;
};


struct DLL_API SiteParameters : public Tools::Json11Serializable {
  SiteParameters() {}

  SiteParameters(mas::schema::model::monica::SiteParameters::Reader reader) { deserialize(reader); }

  void deserialize(mas::schema::model::monica::SiteParameters::Reader reader);

  // SiteParameters(json11::Json object);

  void serialize(mas::schema::model::monica::SiteParameters::Builder builder) const;

  Tools::Errors merge(json11::Json j) override;

  json11::Json to_json() const override;

  double vs_Latitude{ 52.5 }; //ZALF latitude
  double vs_Slope{ 0.01 }; //!< [m m-1]
  double vs_HeightNN{ 50.0 }; //!< [m]
  double vs_GroundwaterDepth{ 70.0 }; //!< [m]
  double vs_Soil_CN_Ratio{ 10.0 };
  double vs_DrainageCoeff{ 1.0 };
  double vq_NDeposition{ 30.0 }; //!< [kg N ha-1 y-1]
  double vs_MaxEffectiveRootingDepth{ 2.0 }; //!< [m]
  double vs_ImpenetrableLayerDepth{ -1 }; //!< [m]
  double vs_SoilSpecificHumusBalanceCorrection{ 0.0 }; //humus equivalents
  double bareSoilKcFactor{ 0.4 };

  int numberOfLayers{ 20 };
  double layerThickness{ 0.1 };

  Soil::SoilPMs vs_SoilParameters;
  Tools::J11Array initSoilProfileSpec;
  std::string pwpFcSatFunction{ "Wessolek2009" };
  std::map<std::string, std::function<Tools::Errors(Soil::SoilParameters*)>> calculateAndSetPwpFcSatFunctions;
  //MeasuredGroundwaterTableInformation groundwaterInformation;
};


/**
* @brief Data structure that containts all relevant parameters for the automatic yield trigger.
*/
class DLL_API AutomaticHarvestParameters : public Tools::Json11Serializable {
public:
  //! Enumeration for defining automatic harvesting times

  //! Definition of different harvest time definition for the automatic
  //! yield trigger
  enum HarvestTime {
    maturity,	//!< crop is harvested when maturity is reached
    unknown		//!< default error value
  };
public:
  AutomaticHarvestParameters() {}

  AutomaticHarvestParameters(HarvestTime yt);

  AutomaticHarvestParameters(mas::schema::model::monica::AutomaticHarvestParameters::Reader reader) { deserialize(reader); }

  void deserialize(mas::schema::model::monica::AutomaticHarvestParameters::Reader reader);

  // AutomaticHarvestParameters(json11::Json object);

  void serialize(mas::schema::model::monica::AutomaticHarvestParameters::Builder builder) const;

  virtual Tools::Errors merge(json11::Json j);

  virtual json11::Json to_json() const;

  //! Setter for automatic harvest time
  void setHarvestTime(HarvestTime time) { _harvestTime = time; }

  //! Getter for automatic harvest time
  HarvestTime getHarvestTime() const { return _harvestTime; }

  //! Setter for fallback automatic harvest day
  void setLatestHarvestDOY(int doy) { _latestHarvestDOY = doy; }

  //! Getter for fallback automatic harvest day
  int getLatestHarvestDOY() const { return _latestHarvestDOY; }

private:
  HarvestTime _harvestTime{ unknown }; //!< Harvest time parameter
  int _latestHarvestDOY{ -1 }; //!< Fallback day for latest harvest of the crop
};


struct DLL_API NMinCropParameters : public Tools::Json11Serializable {
  NMinCropParameters() {}

  NMinCropParameters(double samplingDepth, double nTarget, double nTarget30);

  NMinCropParameters(mas::schema::model::monica::NMinCropParameters::Reader reader) { deserialize(reader); }

  void deserialize(mas::schema::model::monica::NMinCropParameters::Reader reader);

  // NMinCropParameters(json11::Json object);

  void serialize(mas::schema::model::monica::NMinCropParameters::Builder builder) const;

  virtual Tools::Errors merge(json11::Json j);

  virtual json11::Json to_json() const;

  double samplingDepth{ 0.0 };
  double nTarget{ 0.0 };
  double nTarget30{ 0.0 };
};

struct DLL_API OrganicMatterParameters : public Tools::Json11Serializable {
  OrganicMatterParameters() = default;

  explicit OrganicMatterParameters(mas::schema::model::monica::Params::OrganicFertilization::OrganicMatterParameters::Reader reader) { deserialize(reader); }

  void deserialize(mas::schema::model::monica::Params::OrganicFertilization::OrganicMatterParameters::Reader reader);

  // explicit OrganicMatterParameters(json11::Json object);

  void serialize(mas::schema::model::monica::Params::OrganicFertilization::OrganicMatterParameters::Builder builder) const;

  virtual Tools::Errors merge(json11::Json j);

  virtual json11::Json to_json() const;

  double vo_AOM_DryMatterContent{ 0.0 }; //!< Dry matter content of added organic matter [kg DM kg FM-1]
  double vo_AOM_NH4Content{ 0.0 }; //!< Ammonium content in added organic matter [kg N kg DM-1]
  double vo_AOM_NO3Content{ 0.0 }; //!< Nitrate content in added organic matter [kg N kg DM-1]
  double vo_AOM_CarbamidContent{ 0.0 }; //!< Carbamide content in added organic matter [kg N kg DM-1]
  double vo_CorgContent{ 0.0 }; //!< Carbon content in added organic matter [kg C kg DM-1]

  double vo_AOM_SlowDecCoeffStandard{ 0.0 }; //!< Decomposition rate coefficient of slow AOM at standard conditions [d-1]
  double vo_AOM_FastDecCoeffStandard{ 0.0 }; //!< Decomposition rate coefficient of fast AOM at standard conditions [d-1]

  double vo_PartAOM_to_AOM_Slow{ 0.0 }; //!< Part of AOM that is assigned to the slowly decomposing pool [kg kg-1
  double vo_PartAOM_to_AOM_Fast{ 0.0 }; //!< Part of AOM that is assigned to the rapidly decomposing pool [kg kg-1]

  double vo_CN_Ratio_AOM_Slow{ 0.0 }; //!< C to N ratio of the slowly decomposing AOM pool []
  double vo_CN_Ratio_AOM_Fast{ 0.0 }; //!< C to N ratio of the rapidly decomposing AOM pool []

  double vo_PartAOM_Slow_to_SMB_Slow{ 0.0 }; //!< Part of AOM slow consumed by slow soil microbial biomass [kg kg-1]
  double vo_PartAOM_Slow_to_SMB_Fast{ 0.0 }; //!< Part of AOM slow consumed by fast soil microbial biomass [kg kg-1]

  double vo_NConcentration{ 0.0 };
};

typedef std::shared_ptr<OrganicMatterParameters> OrganicMatterParametersPtr;

struct DLL_API OrganicFertilizerParameters : public OrganicMatterParameters {
  OrganicFertilizerParameters() {}

  OrganicFertilizerParameters(mas::schema::model::monica::Params::OrganicFertilization::Parameters::Reader reader) { deserialize(reader); }

  void deserialize(mas::schema::model::monica::Params::OrganicFertilization::Parameters::Reader reader);

  // OrganicFertilizerParameters(json11::Json object);

  void serialize(mas::schema::model::monica::Params::OrganicFertilization::Parameters::Builder builder) const;

  virtual Tools::Errors merge(json11::Json j);

  virtual json11::Json to_json() const;

  std::string id;
  std::string name;
};

typedef std::shared_ptr<OrganicFertilizerParameters> OrganicFertiliserParametersPtr;

struct DLL_API CropResidueParameters : public OrganicMatterParameters {
  CropResidueParameters() {}

  CropResidueParameters(mas::schema::model::monica::CropResidueParameters::Reader reader) { deserialize(reader); }

  void deserialize(mas::schema::model::monica::CropResidueParameters::Reader reader);

  void serialize(mas::schema::model::monica::CropResidueParameters::Builder builder) const;

  // CropResidueParameters(json11::Json object);

  virtual Tools::Errors merge(json11::Json j);

  virtual json11::Json to_json() const;

  std::string species;
  std::string residueType;
};

typedef std::shared_ptr<CropResidueParameters> CropResidueParametersPtr;


struct DLL_API SimulationParameters : public Tools::Json11Serializable {
  SimulationParameters() {}

  SimulationParameters(mas::schema::model::monica::SimulationParameters::Reader reader) { deserialize(reader); }

  void deserialize(mas::schema::model::monica::SimulationParameters::Reader reader);

  // SimulationParameters(json11::Json object);

  void serialize(mas::schema::model::monica::SimulationParameters::Builder builder) const;

  virtual Tools::Errors merge(json11::Json j);

  virtual json11::Json to_json() const;

  Tools::Date startDate;
  Tools::Date endDate;

  bool pc_NitrogenResponseOn{ true };
  bool pc_WaterDeficitResponseOn{ true };
  bool pc_EmergenceFloodingControlOn{ true };
  bool pc_EmergenceMoistureControlOn{ true };
  bool pc_FrostKillOn{ true };

  bool p_UseAutomaticIrrigation{ false };
  AutomaticIrrigationParameters p_AutoIrrigationParams;

  bool p_UseNMinMineralFertilisingMethod{ false };
  MineralFertilizerParameters p_NMinFertiliserPartition;
  NMinApplicationParameters p_NMinUserParams;

  bool p_UseSecondaryYields{ true };
  bool p_UseAutomaticHarvestTrigger{ false };

  //int p_NumberOfLayers{ 20 };
  //double p_LayerThickness{ 0.1 };

  int p_StartPVIndex{ 0 };
  int p_JulianDayAutomaticFertilising{ 0 };

  bool serializeMonicaStateAtEnd{ false };
  bool serializeMonicaStateAtEndToJson{ false };
  std::string pathToSerializationAtEndFile;
  bool loadSerializedMonicaStateAtStart{ false };
  bool deserializedMonicaStateFromJson{ false };
  std::string pathToLoadSerializationFile;
  uint64_t noOfPreviousDaysSerializedClimateData{ 0 };
  
  json11::Json customData;
  std::string soilTempModel{"internal"};
};


  /**
   * Class that holds information of crop defined by user.
   * @author Xenia Specka
   */
struct DLL_API CropModuleParameters : public Tools::Json11Serializable {
  CropModuleParameters() {}

  CropModuleParameters(mas::schema::model::monica::CropModuleParameters::Reader reader) { deserialize(reader); }

  void deserialize(mas::schema::model::monica::CropModuleParameters::Reader reader);

//  CropModuleParameters(json11::Json object);

  void serialize(mas::schema::model::monica::CropModuleParameters::Builder builder) const;

  virtual Tools::Errors merge(json11::Json j);

  virtual json11::Json to_json() const;

  double pc_CanopyReflectionCoefficient{ 0.0 };
  double pc_ReferenceMaxAssimilationRate{ 0.0 };
  double pc_ReferenceLeafAreaIndex{ 0.0 };
  double pc_MaintenanceRespirationParameter1{ 0.0 };
  double pc_MaintenanceRespirationParameter2{ 0.0 };
  double pc_MinimumNConcentrationRoot{ 0.0 };
  double pc_MinimumAvailableN{ 0.0 };
  double pc_ReferenceAlbedo{ 0.0 };
  double pc_StomataConductanceAlpha{ 0.0 };
  double pc_SaturationBeta{ 0.0 };
  double pc_GrowthRespirationRedux{ 0.0 };
  double pc_MaxCropNDemand{ 0.0 };
  double pc_GrowthRespirationParameter1{ 0.0 };
  double pc_GrowthRespirationParameter2{ 0.0 };
  double pc_Tortuosity{ 0.0 };
  bool pc_AdjustRootDepthForSoilProps{ true };
  std::vector<int> pc_TimeUnderAnoxiaThreshold{ 4, 4, 4, 4, 4, 4, 4 };

  bool __enable_Phenology_WangEngelTemperatureResponse__{ false };
  bool __enable_Photosynthesis_WangEngelTemperatureResponse__{ false };
  bool __enable_hourly_FvCB_photosynthesis__{ false };
  bool __enable_T_response_leaf_expansion__{ false };
  bool __disable_daily_root_biomass_to_soil__{ false };
  bool __enable_vernalisation_factor_fix__{ false };
  bool __enable_PASW_root_penetration__{false};

  bool isIntercropping { false };
  bool sequentialWaterUse { false };
  bool twoWaySync { true };
  double pc_intercropping_k_s {0.0};
  double pc_intercropping_k_t {0.0};
  double pc_intercropping_phRedux {0.5};
  double pc_intercropping_dvs_phr {5.791262};
  bool pc_intercropping_autoPhRedux { true };
  std::string pc_intercropping_reader_sr;
  std::string pc_intercropping_writer_sr;
};

/**
 * Class that holds information about user defined environment parameters.
 * @author Xenia Specka
 */
struct DLL_API EnvironmentParameters : public Tools::Json11Serializable {
  EnvironmentParameters() {}

  EnvironmentParameters(mas::schema::model::monica::EnvironmentParameters::Reader reader) { deserialize(reader); }

  void deserialize(mas::schema::model::monica::EnvironmentParameters::Reader reader);

//  EnvironmentParameters(json11::Json object);

  void serialize(mas::schema::model::monica::EnvironmentParameters::Builder builder) const;

  virtual Tools::Errors merge(json11::Json j);

  virtual json11::Json to_json() const;

  double p_Albedo{ 0.23 };
  mas::schema::climate::RCP rcp{ mas::schema::climate::RCP::RCP85 };
  double p_AtmosphericCO2{ 0.0 };
  std::map<int, double> p_AtmosphericCO2s;
  double p_AtmosphericO3{ 0.0 };
  std::map<int, double> p_AtmosphericO3s;
  double p_WindSpeedHeight{ 2.0 };
  double p_LeachingDepth{ 0.0 };
  double p_timeStep{ 0.0 };

  double p_MaxGroundwaterDepth{ 18.0 };
  double p_MinGroundwaterDepth{ 20.0 };
  int p_MinGroundwaterDepthMonth{ 3 };

};

  /**
   * Class that holds information about user defined soil moisture parameters.
   * @author Xenia Specka
   */
struct DLL_API SoilMoistureModuleParameters : public Tools::Json11Serializable {
  SoilMoistureModuleParameters();

  SoilMoistureModuleParameters(mas::schema::model::monica::SoilMoistureModuleParameters::Reader reader) { deserialize(reader); }

  void deserialize(mas::schema::model::monica::SoilMoistureModuleParameters::Reader reader);

//  SoilMoistureModuleParameters(json11::Json object);

  void serialize(mas::schema::model::monica::SoilMoistureModuleParameters::Builder builder) const;

  virtual Tools::Errors merge(json11::Json j);

  virtual json11::Json to_json() const;

  std::function<double(std::string, size_t)> getCapillaryRiseRate;

  //double pm_CriticalMoistureDepth{ 0.0 };
  double pm_SaturatedHydraulicConductivity{ 0.0 };
  double pm_SurfaceRoughness{ 0.0 };
  double pm_GroundwaterDischarge{ 0.0 };
  double pm_HydraulicConductivityRedux{ 0.0 };
  double pm_SnowAccumulationTresholdTemperature{ 0.0 };
  double pm_KcFactor{ 0.0 };
  double pm_TemperatureLimitForLiquidWater{ 0.0 };
  double pm_CorrectionSnow{ 0.0 };
  double pm_CorrectionRain{ 0.0 };
  double pm_SnowMaxAdditionalDensity{ 0.0 };
  double pm_NewSnowDensityMin{ 0.0 };
  double pm_SnowRetentionCapacityMin{ 0.0 };
  double pm_RefreezeParameter1{ 0.0 };
  double pm_RefreezeParameter2{ 0.0 };
  double pm_RefreezeTemperature{ 0.0 };
  double pm_SnowMeltTemperature{ 0.0 };
  double pm_SnowPacking{ 0.0 };
  double pm_SnowRetentionCapacityMax{ 0.0 };
  double pm_EvaporationZeta{ 0.0 };
  double pm_XSACriticalSoilMoisture{ 0.0 };
  double pm_MaximumEvaporationImpactDepth{ 0.0 };
  double pm_MaxPercolationRate{ 0.0 };
  double pm_MoistureInitValue{ 0.0 };
};


/**
 * Class that holds information about user defined soil temperature parameters.
 * @author Xenia Specka
 */
struct DLL_API SoilTemperatureModuleParameters : public Tools::Json11Serializable {
  SoilTemperatureModuleParameters() {}

  SoilTemperatureModuleParameters(mas::schema::model::monica::SoilTemperatureModuleParameters::Reader reader) { deserialize(reader); }

  void deserialize(mas::schema::model::monica::SoilTemperatureModuleParameters::Reader reader);

//  SoilTemperatureModuleParameters(json11::Json object);

  void serialize(mas::schema::model::monica::SoilTemperatureModuleParameters::Builder builder) const;

  virtual Tools::Errors merge(json11::Json j);

  virtual json11::Json to_json() const;

  double pt_NTau{ 0.0 };
  double pt_InitialSurfaceTemperature{ 0.0 };
  double pt_BaseTemperature{ 0.0 };
  double pt_QuartzRawDensity{ 0.0 };
  double pt_DensityAir{ 0.0 };
  double pt_DensityWater{ 0.0 };
  double pt_DensityHumus{ 0.0 };
  double pt_SpecificHeatCapacityAir{ 0.0 };
  double pt_SpecificHeatCapacityQuartz{ 0.0 };
  double pt_SpecificHeatCapacityWater{ 0.0 };
  double pt_SpecificHeatCapacityHumus{ 0.0 };
  double pt_SoilAlbedo{ 0.0 };
  double pt_SoilMoisture{ 0.25 }; // [m3 m-3]
  double pt_PlantAvailableWaterContentConst { 0.25 }; // [m3 m-3]
  double dampingFactor{ 0.8 };
};


/**
 * Class that holds information about user defined soil transport parameters.
 * @author Xenia Specka
 */
struct DLL_API SoilTransportModuleParameters : public Tools::Json11Serializable {
  SoilTransportModuleParameters() {}

  SoilTransportModuleParameters(mas::schema::model::monica::SoilTransportModuleParameters::Reader reader) { deserialize(reader); }

  void deserialize(mas::schema::model::monica::SoilTransportModuleParameters::Reader reader);

//  SoilTransportModuleParameters(json11::Json object);

  void serialize(mas::schema::model::monica::SoilTransportModuleParameters::Builder builder) const;

  virtual Tools::Errors merge(json11::Json j);

  virtual json11::Json to_json() const;

  double pq_DispersionLength{ 0.0 };
  double pq_AD{ 0.0 };
  double pq_DiffusionCoefficientStandard{ 0.0 };
  double pq_NDeposition{ 0.0 };
};


struct DLL_API SticsParameters : public Tools::Json11Serializable {
  SticsParameters() {}

  SticsParameters(mas::schema::model::monica::SticsParameters::Reader reader) { deserialize(reader); }

  void deserialize(mas::schema::model::monica::SticsParameters::Reader reader);

//  SticsParameters(json11::Json object);

  void serialize(mas::schema::model::monica::SticsParameters::Builder builder) const;

  virtual Tools::Errors merge(json11::Json j);

  virtual json11::Json to_json() const;

  bool use_n2o{ false };
  bool use_nit{ false };
  bool use_denit{ false };
  int code_vnit{ 1 };
  int code_tnit{ 2 };
  int code_rationit{ 2 };
  int code_hourly_wfps_nit{ 2 };
  int code_pdenit{ 1 };
  int code_ratiodenit{ 2 };
  int code_hourly_wfps_denit{ 2 };
  double hminn{ 0.3 };
  double hoptn{ 0.9 };
  double pHminnit{ 4.0 };
  double pHmaxnit{ 7.2 };
  double nh4_min{ 1.0 }; // [mg NH4-N/kg soil]
  double pHminden{ 7.2 };
  double pHmaxden{ 9.2 };
  double wfpsc{ 0.62 };
  double tdenitopt_gauss{ 47 }; // [°C]
  double scale_tdenitopt{ 25 }; // [°C]
  double Kd{ 148 }; // [mg NO3-N/L]
  double k_desat{ 3.0 }; // [1/day]
  double fnx{ 0.8 }; // [1/day]
  double vnitmax{ 27.3 }; // [mg NH4-N/kg soil/day]
  double Kamm{ 24 }; // [mg NH4-N/L]
  double tnitmin{ 5.0 }; // [°C]
  double tnitopt{ 30.0 }; // [°C]
  double tnitop2{ 35.0 }; // [°C]
  double tnitmax{ 58.0 }; // [°C]
  double tnitopt_gauss{ 32.5 }; // [°C]
  double scale_tnitopt{ 16.0 }; // [°C]
  double rationit{ 0.0016 };
  double cmin_pdenit{ 1.0 }; // [% [0-100]]
  double cmax_pdenit{ 6.0 }; // [% [0-100]]
  double min_pdenit{ 1.0 }; // [mg N/Kg soil/day]
  double max_pdenit{ 20.0 }; // [mg N/kg soil/day]
  double ratiodenit{ 0.2 };
  double profdenit{ 20 }; // [cm]
  double vpotdenit{ 2.0 }; // [kg N/ha/day]
};


/**
 * Class that holds information about user-defined soil organic parameters.
 * @author Claas Nendel
 */
struct DLL_API SoilOrganicModuleParameters : public Tools::Json11Serializable {
  SoilOrganicModuleParameters() {}

  SoilOrganicModuleParameters(mas::schema::model::monica::SoilOrganicModuleParameters::Reader reader) { deserialize(reader); }

  void deserialize(mas::schema::model::monica::SoilOrganicModuleParameters::Reader reader);

//  SoilOrganicModuleParameters(json11::Json object);

  void serialize(mas::schema::model::monica::SoilOrganicModuleParameters::Builder builder) const;

  virtual Tools::Errors merge(json11::Json j);

  virtual json11::Json to_json() const;

  double po_SOM_SlowDecCoeffStandard{ 4.30e-5 }; // 4.30e-5 [d-1], Bruun et al. 2003 4.3e-5
  double po_SOM_FastDecCoeffStandard{ 1.40e-4 }; // 1.40e-4 [d-1], from DAISY manual 1.4e-4
  double po_SMB_SlowMaintRateStandard{ 1.00e-3 }; // 1.00e-3 [d-1], from DAISY manual original 1.8e-3
  double po_SMB_FastMaintRateStandard{ 1.00e-2 }; // 1.00e-2 [d-1], from DAISY manual
  double po_SMB_SlowDeathRateStandard{ 1.00e-3 }; // 1.00e-3 [d-1], from DAISY manual
  double po_SMB_FastDeathRateStandard{ 1.00e-2 }; // 1.00e-2 [d-1], from DAISY manual
  double po_SMB_UtilizationEfficiency{ 0.60 }; // 0.60 [], from DAISY manual 0.6
  double po_SOM_SlowUtilizationEfficiency{ 0.40 }; // 0.40 [], from DAISY manual 0.4
  double po_SOM_FastUtilizationEfficiency{ 0.50 }; // 0.50 [], from DAISY manual 0.5
  double po_AOM_SlowUtilizationEfficiency{ 0.40 }; // 0.40 [], from DAISY manual original 0.13
  double po_AOM_FastUtilizationEfficiency{ 0.10 }; // 0.10 [], from DAISY manual original 0.69
  double po_AOM_FastMaxC_to_N{ 1000.0 }; // 1000.0
  double po_PartSOM_Fast_to_SOM_Slow{ 0.30 }; // 0.30 [], Bruun et al. 2003
  double po_PartSMB_Slow_to_SOM_Fast{ 0.60 }; // 0.60 [], from DAISY manual
  double po_PartSMB_Fast_to_SOM_Fast{ 0.60 }; // 0.60 [], from DAISY manual
  double po_PartSOM_to_SMB_Slow{ 0.0150 }; // 0.0150 [], optimised
  double po_PartSOM_to_SMB_Fast{ 0.0002 }; // 0.0002 [], optimised
  double po_CN_Ratio_SMB{ 6.70 }; // 6.70 [], from DAISY manual
  double po_LimitClayEffect{ 0.25 }; // 0.25 [kg kg-1], from DAISY manual
  double po_QTenFactor{ 2.9 }; // 2.4 [] default value, analysis literature
  double po_TempDecOptimal{ 38 }; // 38 [°C] default value, analysis literature
  double po_MoistureDecOptimal{ 0.45 }; // 0.45 [fraction] default value, analysis literature
  double po_AmmoniaOxidationRateCoeffStandard{ 1.0e-1 }; // 1.0e-1 [d-1], from DAISY manual
  double po_NitriteOxidationRateCoeffStandard{ 9.0e-1 }; // 9.0e-1 [d-1], fudged by Florian Stange
  double po_TransportRateCoeff{ 0.1 }; // 0.1 [d-1], from DAISY manual
  double po_SpecAnaerobDenitrification{ 0.1 }; // 0.1 [g gas-N g CO2-C-1]
  double po_ImmobilisationRateCoeffNO3{ 0.5 }; // 0.5 [d-1]
  double po_ImmobilisationRateCoeffNH4{ 0.5 }; // 0.5 [d-1]
  double po_Denit1{ 0.2 }; // 0.2 Denitrification parameter
  double po_Denit2{ 0.8 }; // 0.8 Denitrification parameter
  double po_Denit3{ 0.9 }; // 0.9 Denitrification parameter
  double po_HydrolysisKM{ 0.00334 }; // 0.00334 from Tabatabai 1973
  double po_ActivationEnergy{ 41000.0 }; // 41000.0 from Gould et al. 1973
  double po_HydrolysisP1{ 4.259e-12 }; // 4.259e-12 from Sadeghi et al. 1988
  double po_HydrolysisP2{ 1.408e-12 }; // 1.408e-12 from Sadeghi et al. 1988
  double po_AtmosphericResistance{ 0.0025 }; // 0.0025 [s m-1], from Sadeghi et al. 1988
  double po_N2OProductionRate{ 0.5 }; // 0.5 [d-1]
  double po_Inhibitor_NH3{ 1.0 }; // 1.0 [kg N m-3] NH3-induced inhibitor for nitrite oxidation
  double ps_MaxMineralisationDepth{ 0.4 };

  bool __enable_kaiteew_TempOnDecompostion__{ true };
  bool __enable_kaiteew_MoistOnDecompostion__{ true };
  bool __enable_kaiteew_ClayOnDecompostion__{ true };
  SticsParameters sticsParams;
};


/**
 * @brief Central data distribution class.
 *
 * Class that holds pointers and direct information of user defined parameters.
 *
 * @author Xenia Specka
 */
class DLL_API CentralParameterProvider : public Tools::Json11Serializable {
public:
  CentralParameterProvider();

//  CentralParameterProvider(json11::Json object);

  virtual Tools::Errors merge(json11::Json j);

  virtual json11::Json to_json() const;

  CropModuleParameters userCropParameters;
  EnvironmentParameters userEnvironmentParameters;
  SoilMoistureModuleParameters userSoilMoistureParameters;
  SoilTemperatureModuleParameters userSoilTemperatureParameters;
  SoilTransportModuleParameters userSoilTransportParameters;
  SoilOrganicModuleParameters userSoilOrganicParameters;
  SimulationParameters simulationParameters;

  SiteParameters siteParameters; //! site specific parameters
  Soil::OrganicConstants organic; //! constant organic parameters to the model

  MeasuredGroundwaterTableInformation groundwaterInformation;

  double getPrecipCorrectionValue(int month) const;
  void setPrecipCorrectionValue(int month, double value);

  //bool writeOutputFiles() const { return _writeOutputFiles; }
  //void setWriteOutputFiles(bool write) { _writeOutputFiles = write; }

  std::string pathToOutputDir() const {
    return _pathToOutputDir.empty() ? "./" : _pathToOutputDir;
  }
  void setPathToOutputDir(std::string path) { _pathToOutputDir = path; }

private:
  //bool _writeOutputFiles{false};
  std::string _pathToOutputDir;

  std::vector<double> precipCorrectionValues;
};


struct Intercropping {
  typedef mas::schema::model::monica::ICData ICD;
  typedef mas::schema::fbp::Channel<ICD>::ChanReader Reader;
  typedef mas::schema::fbp::Channel<ICD>::ChanWriter Writer;
  kj::AsyncIoContext* ioContext {nullptr};
  Reader::Client reader {nullptr};
  Writer::Client writer {nullptr};
  bool isAsync() const { return ioContext != nullptr; }
};

} // namespace monica

