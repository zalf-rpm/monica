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

#include "monica-model.h"

#include <cstdlib>
#include <iostream>
#include <algorithm>
#include <set>
#include <sstream>
#include <mutex>
#include <memory>
#include <chrono>
#include <thread>
#include <numeric>
#include <cmath>

#include "tools/debug.h"
#include "climate/climate-common.h"
//#include "db/abstract-db-connections.h"
#include "voc-common.h"
#include "tools/algorithms.h"
#include "crop.h"

using namespace monica;
using namespace std;
using namespace Climate;
using namespace Tools;
using namespace Soil;

namespace {
//! simple functor for use in fertiliser trigger
struct AddFertiliserAmountsCallback {
  double& _sf;
  double& _dsf;

  AddFertiliserAmountsCallback(double& sf, double& dsf) : _sf(sf)
                                                        , _dsf(dsf) {}

  void operator()(double v) {
    // cout << "!!!AddFertiliserAmountsCallback: daily_sum_fert += " << v << endl;
    _sf += v;
    _dsf += v;
  }
};
}


namespace {

void initializeMonicaModelFromParams(MonicaModel* model, const CentralParameterProvider& cpp) {
  model->sitePs = cpp.siteParameters;
  model->envPs = cpp.userEnvironmentParameters;
  model->cropPs = cpp.userCropParameters;
  model->simPs = cpp.simulationParameters;
  model->groundwaterInformation = cpp.groundwaterInformation;
  model->soilColumn = makeSoilColumn(model->simPs.p_LayerThickness,
                                      cpp.userSoilOrganicParameters.ps_MaxMineralisationDepth,
                                      model->sitePs.vs_SoilParameters);
  model->soilTemperature = soiltemperature::makeSoilTemperature(*model, cpp.userSoilTemperatureParameters);
  model->soilMoisture = soilmoisture::makeSoilMoisture(*model, cpp.userSoilMoistureParameters);
  model->soilOrganic = makeSoilOrganic(*model->soilColumn, cpp.userSoilOrganicParameters);
  model->soilTransport =
    soiltransport::makeSoilTransport(*model->soilColumn,
                                     model->sitePs,
                                     cpp.userSoilTransportParameters,
                                     model->envPs.p_LeachingDepth,
                                     model->envPs.p_timeStep,
                                     model->cropPs.pc_MinimumAvailableN);
}

} // namespace

kj::Own<MonicaModel> monica::makeMonicaModel(const CentralParameterProvider& cpp) {
  auto model = kj::heap<MonicaModel>();
  initializeMonicaModelFromParams(model.get(), cpp);
  return model;
}

kj::Own<MonicaModel> monica::makeMonicaModel(mas::schema::model::monica::MonicaModelState::Reader reader) {
  auto model = kj::heap<MonicaModel>();
  monicamodel::deserialize(model.get(), reader);
  return model;
}

void monica::monicamodel::deserialize(MonicaModel* model, mas::schema::model::monica::MonicaModelState::Reader reader) {
  auto& sitePs = model->sitePs;
  auto& envPs = model->envPs;
  auto& cropPs = model->cropPs;
  auto& simPs = model->simPs;
  auto& groundwaterInformation = model->groundwaterInformation;
  auto& soilColumn = model->soilColumn;
  auto& soilTemperature = model->soilTemperature;
  auto& soilMoisture = model->soilMoisture;
  auto& soilOrganic = model->soilOrganic;
  auto& soilTransport = model->soilTransport;
  auto& currentCropModule = model->currentCropModule;
  auto& intercropping = model->intercropping;
  auto& sumFertiliser = model->sumFertiliser;
  auto& sumOrgFertiliser = model->sumOrgFertiliser;
  auto& dailySumFertiliser = model->dailySumFertiliser;
  auto& dailySumOrgFertiliser = model->dailySumOrgFertiliser;
  auto& dailySumOrganicFertilizerDM = model->dailySumOrganicFertilizerDM;
  auto& sumOrganicFertilizerDM = model->sumOrganicFertilizerDM;
  auto& humusBalanceCarryOver = model->humusBalanceCarryOver;
  auto& dailySumIrrigationWater = model->dailySumIrrigationWater;
  auto& optCarbonExportedResidues = model->optCarbonExportedResidues;
  auto& optCarbonReturnedResidues = model->optCarbonReturnedResidues;
  auto& currentStepDate = model->currentStepDate;
  auto& climateData = model->climateData;
  auto& currentEvents = model->currentEvents;
  auto& previousDaysEvents = model->previousDaysEvents;
  auto& clearCropUponNextDay = model->clearCropUponNextDay;
  auto& p_daysWithCrop = model->p_daysWithCrop;
  auto& p_accuNStress = model->p_accuNStress;
  auto& p_accuWaterStress = model->p_accuWaterStress;
  auto& p_accuHeatStress = model->p_accuHeatStress;
  auto& p_accuOxygenStress = model->p_accuOxygenStress;
  auto& vw_AtmosphericCO2Concentration = model->vw_AtmosphericCO2Concentration;
  auto& vw_AtmosphericO3Concentration = model->vw_AtmosphericO3Concentration;
  auto& vs_GroundwaterDepth = model->vs_GroundwaterDepth;
  auto& cultivationMethodCount = model->cultivationMethodCount;

  sitePs.deserialize(reader.getSitePs());
  envPs.deserialize(reader.getEnvPs());
  cropPs.deserialize(reader.getCropPs());
  simPs.deserialize(reader.getSimPs());
  groundwaterInformation.deserialize(reader.getGroundwaterInformation());

  if (soilColumn) soilColumnDeserialize(soilColumn.get(), reader.getSoilColumn());
  else soilColumn = makeSoilColumn(reader.getSoilColumn());

  if (reader.hasCurrentCropModule()) {
    auto addOMFunc = [model](const std::map<size_t, double>& layer2amount, double nconc) {
      soilOrganicAddOrganicMatter(model->soilOrganic.get(),
                                  model->currentCropModule->residuePs,
                                  layer2amount,
                                  nconc);
    };
    currentCropModule = nullptr;
    currentCropModule = makeCropModule(soilColumn.get(), &cropPs,
                                        [model](string event) { model->currentEvents.insert(std::move(event)); }, addOMFunc,
                                        [model](double avgAirTemp) {
                                          return soilmoisture::getSnowDepthAndCalcTemperatureUnderSnow(
                                            model->soilMoisture.get(), avgAirTemp);
                                        },
                                        reader.getCurrentCropModule(),
                                        &intercropping);
  }

  soilColumnPutCrop(soilColumn.get(), currentCropModule.get());

  if (soilTemperature) soiltemperature::deserialize(soilTemperature.get(), reader.getSoilTemperature());
  else soilTemperature = soiltemperature::makeSoilTemperature(*model, reader.getSoilTemperature());

  if (soilMoisture) {
    soilmoisture::deserialize(soilMoisture.get(), reader.getSoilMoisture());
    soilMoisture->cropModule = currentCropModule.get();
  } else {
    soilMoisture = soilmoisture::makeSoilMoisture(*model, reader.getSoilMoisture(), currentCropModule.get());
  }

  if (soilOrganic) {
    soilOrganicDeserialize(soilOrganic.get(), reader.getSoilOrganic());
    soilOrganic->cropModule = currentCropModule.get();
  } else {
    soilOrganic = makeSoilOrganic(*soilColumn, reader.getSoilOrganic(), currentCropModule.get());
  }

  if (soilTransport) {
    soiltransport::deserialize(soilTransport.get(), reader.getSoilTransport());
    soiltransport::putCrop(soilTransport.get(), currentCropModule.get());
  } else {
    soilTransport = soiltransport::makeSoilTransport(*soilColumn, reader.getSoilTransport(), currentCropModule.get());
  }

  sumFertiliser = reader.getSumFertiliser();
  sumOrgFertiliser = reader.getSumOrgFertiliser();

  dailySumFertiliser = reader.getDailySumFertiliser();
  dailySumOrgFertiliser = reader.getDailySumOrgFertiliser();

  dailySumOrganicFertilizerDM = reader.getDailySumOrganicFertilizerDM();
  sumOrganicFertilizerDM = reader.getSumOrganicFertilizerDM();

  humusBalanceCarryOver = reader.getHumusBalanceCarryOver();

  dailySumIrrigationWater = reader.getDailySumIrrigationWater();

  optCarbonExportedResidues = reader.getOptCarbonExportedResidues();
  optCarbonReturnedResidues = reader.getOptCarbonReturnedResidues();

  currentStepDate.deserialize(reader.getCurrentStepDate());

  climateData.resize(reader.getClimateData().size());
  {
    capnp::uint i = 0;
    for (const auto& mapList : reader.getClimateData()) {
      auto& acd2val = climateData[i++];
      acd2val.clear();
      for (const auto& readAcd2Val : mapList) {
        acd2val[Climate::ACD(readAcd2Val.getAcd())] = readAcd2Val.getValue();
      }
    }
  }

  currentEvents.clear();
  for (auto s : reader.getCurrentEvents()) currentEvents.insert(s);

  previousDaysEvents.clear();
  for (auto s : reader.getPreviousDaysEvents()) previousDaysEvents.insert(s);

  clearCropUponNextDay = reader.getClearCropUponNextDay();

  p_daysWithCrop = reader.getDaysWithCrop();
  p_accuNStress = reader.getAccuNStress();
  p_accuWaterStress = reader.getAccuWaterStress();
  p_accuHeatStress = reader.getAccuHeatStress();
  p_accuOxygenStress = reader.getAccuOxygenStress();

  vw_AtmosphericCO2Concentration = reader.getVwAtmosphericCO2Concentration();
  vw_AtmosphericO3Concentration = reader.getVwAtmosphericO3Concentration();
  vs_GroundwaterDepth = reader.getVsGroundwaterDepth();

  cultivationMethodCount = reader.getCultivationMethodCount();
}

void monica::monicamodel::serialize(MonicaModel* model, mas::schema::model::monica::MonicaModelState::Builder builder) {
  auto& sitePs = model->sitePs;
  auto& envPs = model->envPs;
  auto& cropPs = model->cropPs;
  auto& simPs = model->simPs;
  auto& groundwaterInformation = model->groundwaterInformation;
  auto& soilColumn = model->soilColumn;
  auto& soilTemperature = model->soilTemperature;
  auto& soilMoisture = model->soilMoisture;
  auto& soilOrganic = model->soilOrganic;
  auto& soilTransport = model->soilTransport;
  auto& currentCropModule = model->currentCropModule;
  auto& sumFertiliser = model->sumFertiliser;
  auto& sumOrgFertiliser = model->sumOrgFertiliser;
  auto& dailySumFertiliser = model->dailySumFertiliser;
  auto& dailySumOrgFertiliser = model->dailySumOrgFertiliser;
  auto& dailySumOrganicFertilizerDM = model->dailySumOrganicFertilizerDM;
  auto& sumOrganicFertilizerDM = model->sumOrganicFertilizerDM;
  auto& humusBalanceCarryOver = model->humusBalanceCarryOver;
  auto& dailySumIrrigationWater = model->dailySumIrrigationWater;
  auto& optCarbonExportedResidues = model->optCarbonExportedResidues;
  auto& optCarbonReturnedResidues = model->optCarbonReturnedResidues;
  auto& currentStepDate = model->currentStepDate;
  auto& climateData = model->climateData;
  auto& currentEvents = model->currentEvents;
  auto& previousDaysEvents = model->previousDaysEvents;
  auto& clearCropUponNextDay = model->clearCropUponNextDay;
  auto& p_daysWithCrop = model->p_daysWithCrop;
  auto& p_accuNStress = model->p_accuNStress;
  auto& p_accuWaterStress = model->p_accuWaterStress;
  auto& p_accuHeatStress = model->p_accuHeatStress;
  auto& p_accuOxygenStress = model->p_accuOxygenStress;
  auto& vw_AtmosphericCO2Concentration = model->vw_AtmosphericCO2Concentration;
  auto& vw_AtmosphericO3Concentration = model->vw_AtmosphericO3Concentration;
  auto& vs_GroundwaterDepth = model->vs_GroundwaterDepth;
  auto& cultivationMethodCount = model->cultivationMethodCount;

  sitePs.serialize(builder.initSitePs());
  envPs.serialize(builder.initEnvPs());
  cropPs.serialize(builder.initCropPs());
  simPs.serialize(builder.initSimPs());
  groundwaterInformation.serialize(builder.initGroundwaterInformation());
  soilColumnSerialize(soilColumn.get(), builder.initSoilColumn());
  soiltemperature::serialize(soilTemperature.get(), builder.initSoilTemperature());
  soilmoisture::serialize(soilMoisture.get(), builder.initSoilMoisture());
  soilOrganicSerialize(soilOrganic.get(), builder.initSoilOrganic());
  soiltransport::serialize(soilTransport.get(), builder.initSoilTransport());

  if (currentCropModule) cropmodule::serialize(currentCropModule.get(), builder.initCurrentCropModule());

  builder.setSumFertiliser(sumFertiliser);
  builder.setSumOrgFertiliser(sumOrgFertiliser);

  builder.setDailySumFertiliser(dailySumFertiliser);
  builder.setDailySumOrgFertiliser(dailySumOrgFertiliser);

  builder.setDailySumOrganicFertilizerDM(dailySumOrganicFertilizerDM);
  builder.setSumOrganicFertilizerDM(sumOrganicFertilizerDM);

  builder.setHumusBalanceCarryOver(humusBalanceCarryOver);

  builder.setDailySumIrrigationWater(dailySumIrrigationWater);

  builder.setOptCarbonExportedResidues(optCarbonExportedResidues);
  builder.setOptCarbonReturnedResidues(optCarbonReturnedResidues);

  currentStepDate.serialize(builder.initCurrentStepDate());

  auto cdSize = climateData.size();
  auto serMaxDays = min(size_t(simPs.noOfPreviousDaysSerializedClimateData), cdSize);
  auto buildCdList = builder.initClimateData((capnp::uint)serMaxDays);
  {
    capnp::uint i = 0;
    for (size_t j = cdSize - serMaxDays; j < cdSize; j++) {
      auto& map = climateData[j];
      auto buildList = buildCdList.init(i++, (capnp::uint)map.size());
      capnp::uint k = 0;
      for (auto& p : map) {
        auto buildAcd2Val = buildList[k++];
        buildAcd2Val.setAcd(p.first);
        buildAcd2Val.setValue(p.second);
      }
    }
  }

  auto buildEvents = builder.initCurrentEvents((capnp::uint)currentEvents.size());
  {
    capnp::uint i = 0;
    for (auto& e : currentEvents) {
      buildEvents.set(i++, e);
    }
  }

  auto buildPrevEvents = builder.initPreviousDaysEvents((capnp::uint)previousDaysEvents.size());
  {
    capnp::uint i = 0;
    for (auto& e : previousDaysEvents) {
      buildPrevEvents.set(i++, e);
    }
  }

  builder.setClearCropUponNextDay(clearCropUponNextDay);

  builder.setDaysWithCrop(p_daysWithCrop);
  builder.setAccuNStress(p_accuNStress);
  builder.setAccuWaterStress(p_accuWaterStress);
  builder.setAccuHeatStress(p_accuHeatStress);
  builder.setAccuOxygenStress(p_accuOxygenStress);

  builder.setVwAtmosphericCO2Concentration(vw_AtmosphericCO2Concentration);
  builder.setVwAtmosphericO3Concentration(vw_AtmosphericO3Concentration);
  builder.setVsGroundwaterDepth(vs_GroundwaterDepth);

  builder.setCultivationMethodCount(cultivationMethodCount);
}

void monica::monicamodel::addDailySumFertiliser(MonicaModel* model, double amount) {
  model->dailySumFertiliser += amount;
  model->sumFertiliser += amount;
}

void monica::monicamodel::addDailySumOrganicFertilizerDM(MonicaModel* model, double amountDM) {
  model->dailySumOrganicFertilizerDM += amountDM;
  model->sumOrganicFertilizerDM += amountDM;
}

void monica::monicamodel::addDailySumIrrigationWater(MonicaModel* model, double amount) {
  model->dailySumIrrigationWater += amount;
}

void monica::monicamodel::resetFertiliserCounter(MonicaModel* model) {
  model->sumFertiliser = 0;
  model->sumOrgFertiliser = 0;
  model->sumOrganicFertilizerDM = 0;
}

void monica::monicamodel::seedCrop(MonicaModel* model, mas::schema::model::monica::CropSpec::Reader reader) {
  debug() << "seedCrop" << endl;

  model->p_daysWithCrop = 0;
  model->p_accuNStress = 0.0;
  model->p_accuWaterStress = 0.0;
  model->p_accuHeatStress = 0.0;
  model->p_accuOxygenStress = 0.0;

  if (reader.hasCropParams() && reader.hasResidueParams()) {
    auto cropParams = reader.getCropParams();
    if (!cropParams.hasSpeciesParams() || !cropParams.hasCultivarParams()) return;

    model->cultivationMethodCount++;

    auto addOMFunc = [model](const std::map<size_t, double>& layer2amount, double nConcentration) {
      soilOrganicAddOrganicMatter(model->soilOrganic.get(),
                                  model->currentCropModule->residuePs,
                                  layer2amount,
                                  nConcentration);
    };
    CropParameters cps(reader.getCropParams());
    model->currentCropModule = nullptr;
    model->currentCropModule = makeCropModule(model->soilColumn.get(), &cps, reader.getResidueParams(),
                                               cps.cultivarParams.winterCrop, &model->sitePs, &model->cropPs,
                                               model->simPs,
                                        [model](const string& event) { model->currentEvents.insert(event); },
                                        addOMFunc,
                                        [model](double avgAirTemp) {
                                          return soilmoisture::getSnowDepthAndCalcTemperatureUnderSnow(
                                            model->soilMoisture.get(), avgAirTemp);
                                        },
                                        &model->intercropping);

    //if (crop->separatePerennialCropParameters())
    //  currentCropModule->setPerennialCropParameters(crop->perennialCropParameters());

    soiltransport::putCrop(model->soilTransport.get(), model->currentCropModule.get());
    soilColumnPutCrop(model->soilColumn.get(), model->currentCropModule.get());
    model->soilMoisture->cropModule = model->currentCropModule.get();
    model->soilOrganic->cropModule = model->currentCropModule.get();

    if (model->simPs.p_UseNMinMineralFertilisingMethod
        && !model->currentCropModule->isWinterCrop) {
      soilColumnClearTopDressingParams(model->soilColumn.get());
      debug() << "nMin fertilising summer crop" << endl;
      double fertAmount = monicamodel::applyMineralFertiliserViaNMinMethod
        (model, model->simPs.p_NMinFertiliserPartition,
         NMinCropParameters(cps.speciesParams.pc_SamplingDepth,
                            cps.speciesParams.pc_TargetNSamplingDepth,
                            cps.speciesParams.pc_TargetN30));
      monicamodel::addDailySumFertiliser(model, fertAmount);
    }
  }
}

/**
 * @brief Simulation of crop seed.
 * @param crop to be planted
 */
void monica::monicamodel::seedCrop(MonicaModel* model, Crop* crop) {
  debug() << "seedCrop" << endl;

  model->p_daysWithCrop = 0;
  model->p_accuNStress = 0.0;
  model->p_accuWaterStress = 0.0;
  model->p_accuHeatStress = 0.0;
  model->p_accuOxygenStress = 0.0;

  if (crop->isValid()) {
    model->cultivationMethodCount++;

    auto addOMFunc = [model](const std::map<size_t, double>& layer2amount, double nconc) {
      soilOrganicAddOrganicMatter(model->soilOrganic.get(),
                                  model->currentCropModule->residuePs,
                                  layer2amount,
                                  nconc);
    };
    auto cps = crop->cropParameters();
    model->currentCropModule = nullptr;
    model->currentCropModule = makeCropModule(model->soilColumn.get(), &cps, crop->residueParameters(),
                                        crop->isWinterCrop(), &model->sitePs, &model->cropPs, model->simPs,
                                        [model](string event) { model->currentEvents.insert(std::move(event)); }, addOMFunc,
                                        [model](double avgAirTemp) {
                                          return soilmoisture::getSnowDepthAndCalcTemperatureUnderSnow(
                                            model->soilMoisture.get(), avgAirTemp);
                                        },
                                        &model->intercropping);

    if (crop->separatePerennialCropParameters())
      cropmodule::setPerennialCropParameters(model->currentCropModule.get(), crop->perennialCropParameters());

    soiltransport::putCrop(model->soilTransport.get(), model->currentCropModule.get());
    soilColumnPutCrop(model->soilColumn.get(), model->currentCropModule.get());
    model->soilMoisture->cropModule = model->currentCropModule.get();
    model->soilOrganic->cropModule = model->currentCropModule.get();

    //    debug() << "seedDate: "<< _currentCrop->seedDate().toString()
    //            << " harvestDate: " << _currentCrop->harvestDate().toString() << endl;

    if (model->simPs.p_UseNMinMineralFertilisingMethod
        && !model->currentCropModule->isWinterCrop) {
      soilColumnClearTopDressingParams(model->soilColumn.get());
      debug() << "nMin fertilising summer crop" << endl;
      double fert_amount = monicamodel::applyMineralFertiliserViaNMinMethod
        (model, model->simPs.p_NMinFertiliserPartition,
         NMinCropParameters(cps.speciesParams.pc_SamplingDepth,
                            cps.speciesParams.pc_TargetNSamplingDepth,
                            cps.speciesParams.pc_TargetN30));
      monicamodel::addDailySumFertiliser(model, fert_amount);
    }
  }
}

/**
 * @brief Simulating harvest of crop.
 *
 * Deletes the current crop.
 */
void monica::monicamodel::harvestCurrentCrop(MonicaModel* model,
                                           bool exported,
                                           const Harvest::Spec& spec,
                                           Harvest::OptCarbonManagementData optCarbMgmtData,
                                           int incorporateIntoLayerIndex) {
  auto& currentCropModule = model->currentCropModule;
  auto& soilOrganic = model->soilOrganic;
  auto& optCarbonExportedResidues = model->optCarbonExportedResidues;
  auto& optCarbonReturnedResidues = model->optCarbonReturnedResidues;
  auto& humusBalanceCarryOver = model->humusBalanceCarryOver;
  auto& simPs = model->simPs;
  auto& sitePs = model->sitePs;
  auto& clearCropUponNextDay = model->clearCropUponNextDay;

  if (currentCropModule) {
    // prepare to add root and crop residues to soilorganic (AOMs)
    // dead root biomass has already been added daily, so just living root biomass is left
    double rootBiomass = currentCropModule->vc_OrganGreenBiomass[0];
    double rootNConcentration = currentCropModule->vc_NConcentrationRoot;
    debug() << "adding organic matter from root to soilOrganic" << endl;
    debug() << "root biomass: " << rootBiomass
      << " Root N concentration: " << rootNConcentration << endl;
    cropmodule::addAndDistributeRootBiomassInSoil(currentCropModule.get(), rootBiomass);

    if (exported && spec.organ2specVal.empty()) {
      if (optCarbMgmtData.optCarbonConservation) {
        double residueBiomass = cropmodule::getResidueBiomass(currentCropModule.get(),
                                                                       false);
        //kg ha-1, secondary yield is ignored with this approach
        double cropContribToHumus = optCarbMgmtData.cropImpactOnHumusBalance;
        double appliedOrganicFertilizerDryMatter = model->sumOrganicFertilizerDM; //kg ha-1
        double intermediateHumusBalance =
          humusBalanceCarryOver + cropContribToHumus + appliedOrganicFertilizerDryMatter
          / 1000.0 * optCarbMgmtData.organicFertilizerHeq -
          sitePs.vs_SoilSpecificHumusBalanceCorrection;
        double potentialHumusFromResidues = residueBiomass / 1000.0 * optCarbMgmtData.residueHeq;

        double fractionToBeLeftOnField = 0.0;
        if (potentialHumusFromResidues > 0) {
          fractionToBeLeftOnField = -intermediateHumusBalance / potentialHumusFromResidues;
          //0 <= fractionToBeLeftOnField <= 1
          if (fractionToBeLeftOnField > 1) {
            fractionToBeLeftOnField = 1.0;
          } else if (fractionToBeLeftOnField < 0) {
            fractionToBeLeftOnField = 0.0;
          }
        }

        if (optCarbMgmtData.cropUsage == Harvest::greenManure)
          //if the crop is used as green manure, all the residues are incorporated regardless the humus balance
          fractionToBeLeftOnField = 1.0;

        //calculate theoretical residue removal
        optCarbonReturnedResidues = residueBiomass * fractionToBeLeftOnField;
        optCarbonExportedResidues = residueBiomass - optCarbonReturnedResidues;

        //adjust it if technically unfeasible
        double maxExportedResidues = residueBiomass * optCarbMgmtData.maxResidueRecoverFraction;
        if (optCarbonExportedResidues > maxExportedResidues) {
          optCarbonExportedResidues = maxExportedResidues;
          optCarbonReturnedResidues = residueBiomass - optCarbonExportedResidues;
        }

        soilOrganicAddOrganicMatter(soilOrganic.get(), currentCropModule->residuePs,
                                    optCarbonReturnedResidues,
                                    cropmodule::getResiduesNConcentration(currentCropModule.get()),
                                    incorporateIntoLayerIndex);

        humusBalanceCarryOver =
          intermediateHumusBalance + optCarbonReturnedResidues / 1000.0 * optCarbMgmtData.residueHeq;
      } else { // old default behavior
        double residueBiomass = cropmodule::getResidueBiomass(currentCropModule.get(), simPs.p_UseSecondaryYields);

        //!@todo Claas: das hier noch berechnen
        double residueNConcentration = cropmodule::getResiduesNConcentration(currentCropModule.get());
        debug() << "adding organic matter from residues to soilOrganic" << endl;
        debug() << "residue biomass: " << residueBiomass
          << " Residue N concentration: " << residueNConcentration << endl;
        debug() << "primary yield biomass: " << cropmodule::getPrimaryCropYield(currentCropModule.get())
          << " Primary yield N concentration: " << cropmodule::getPrimaryYieldNConcentration(currentCropModule.get()) << endl;
        debug() << "secondary yield biomass: " << cropmodule::getSecondaryCropYield(currentCropModule.get())
          << " Secondary yield N concentration: " << cropmodule::getPrimaryYieldNConcentration(currentCropModule.get()) << endl;
        debug() << "Residues N content: " << cropmodule::getResiduesNContent(currentCropModule.get())
          << " Primary yield N content: " << cropmodule::getPrimaryYieldNContent(currentCropModule.get())
          << " Secondary yield N content: " << cropmodule::getSecondaryYieldNContent(currentCropModule.get()) << endl;
        soilOrganicAddOrganicMatter(soilOrganic.get(), currentCropModule->residuePs,
                                    residueBiomass,
                                    residueNConcentration,
                                    incorporateIntoLayerIndex);
      }
    } else if (!spec.organ2specVal.empty()) { // harvest with a more detailed specification
      auto cropYield = 0.0;
      auto primaryCropYield = 0.0;
      auto sumOrganResidueBiomassAsOverlay = 0.0;
      auto sumOrganResidueBiomassToIncorporate = 0.0;
      auto organIdsForPrimaryYield = cropmodule::organIdsForPrimaryYield(currentCropModule.get());
      for (const auto& [organId, specVal] : spec.organ2specVal) {
        // ignore root, is probably an error, when the user specified the root organ (0) as something to harvest
        if (organId == 0) continue;
        auto organBiomass = currentCropModule->vc_OrganBiomass[organId];
        auto organYield = organBiomass * specVal.exportPercentage / 100.0;
        cropYield += organYield;
        if (organIdsForPrimaryYield.find(organId + 1) != organIdsForPrimaryYield.end()) {
          primaryCropYield += organYield;
        }
        if (specVal.incorporate) sumOrganResidueBiomassToIncorporate += organBiomass - organYield;
        else sumOrganResidueBiomassAsOverlay += organBiomass - organYield;
      }
      auto totalResidueBiomass = cropmodule::getResidueBiomass(currentCropModule.get(), false, cropYield);
      auto totalResidueBiomassToIncorporate = totalResidueBiomass - sumOrganResidueBiomassAsOverlay;
      auto residuesNConcentration = cropmodule::getResiduesNConcentration(currentCropModule.get(), primaryCropYield);
      soilOrganicAddOrganicMatter(soilOrganic.get(), currentCropModule->residuePs,
                                  totalResidueBiomassToIncorporate,
                                  residuesNConcentration,
                                  incorporateIntoLayerIndex);

      debug()
        << "adding organic matter from residues to soilOrganic"
        << endl
        << "total residue biomass: " << totalResidueBiomass
        << " residue biomass as overlay: " << sumOrganResidueBiomassAsOverlay
        << " residue N concentration: " << residuesNConcentration
        << endl
        << "primary yield biomass: " << primaryCropYield
        << " primary yield N concentration: " << cropmodule::getPrimaryYieldNConcentration(currentCropModule.get(), primaryCropYield)
        << endl
        << "secondary yield biomass: " << (cropYield - primaryCropYield)
        << " secondary yield N concentration: "
        << cropmodule::getPrimaryYieldNConcentration(currentCropModule.get(), primaryCropYield)
        << endl
        << "residues N content: " << cropmodule::getResiduesNContent(currentCropModule.get(), false, primaryCropYield, cropYield)
        << " primary yield N content: " << cropmodule::getPrimaryYieldNContent(currentCropModule.get(), primaryCropYield)
        << " secondary yield N content: "
        << cropmodule::getSecondaryYieldNContent(currentCropModule.get(), primaryCropYield, cropYield - primaryCropYield)
        << endl;
    } else {
      //prepare to add the total plant to soilorganic (AOMs)
      double abovegroundBiomass = currentCropModule->vc_AbovegroundBiomass;
      double abovegroundBiomassNConcentration =
        currentCropModule->vc_NConcentrationAbovegroundBiomass;
      debug() << "adding organic matter from aboveground biomass to soilOrganic" << endl;
      debug() << "aboveground biomass: " << abovegroundBiomass
        << " Aboveground biomass N concentration: " << abovegroundBiomassNConcentration << endl;
      soilOrganicAddOrganicMatter(soilOrganic.get(), currentCropModule->residuePs,
                                  abovegroundBiomass,
                                  abovegroundBiomassNConcentration,
                                  incorporateIntoLayerIndex);
    }
  }

  clearCropUponNextDay = true;
}

/**
 * @brief Simulating plowing or incorporating of total crop.
 *
 * Deletes the current crop.
 */
void monica::monicamodel::incorporateCurrentCrop(MonicaModel* model) {
  auto& currentCropModule = model->currentCropModule;
  auto& soilOrganic = model->soilOrganic;
  auto& clearCropUponNextDay = model->clearCropUponNextDay;

  if (currentCropModule) {
    //prepare to add root and crop residues to soilorganic (AOMs)
    double total_biomass = currentCropModule->vc_TotalBiomass;
    double totalNContent = cropmodule::getAbovegroundBiomassNContent(currentCropModule.get()) +
                           currentCropModule->vc_NConcentrationRoot * currentCropModule->vc_OrganBiomass[0];
    double totalNConcentration = totalNContent / total_biomass;

    debug() << "Adding organic matter from total biomass of crop to soilOrganic" << endl;
    debug() << "Total biomass: " << total_biomass << endl
      << " Total N concentration: " << totalNConcentration << endl;

    soilOrganicAddOrganicMatter(soilOrganic.get(), currentCropModule->residuePs,
                                total_biomass,
                                totalNConcentration);
  }

  clearCropUponNextDay = true;
}

/**
 * @brief Applying of fertilizer.
 *
 * @todo Nothing implemented yet.
 */
void monica::monicamodel::applyMineralFertiliser(MonicaModel* model,
                                               MineralFertilizerParameters partition,
                                               double amount) {
  auto& simPs = model->simPs;
  auto& soilColumn = model->soilColumn;
  if (!simPs.p_UseNMinMineralFertilisingMethod) {
    soilColumnApplyMineralFertiliser(soilColumn.get(), partition, amount);
    monicamodel::addDailySumFertiliser(model, amount);
  }
}

void monica::monicamodel::applyOrganicFertiliser(MonicaModel* model,
                                               const OrganicMatterParameters& params,
                                               double amountFM,
                                               bool incorporation,
                                               int incorporateIntoLayerIndex) {
  auto& soilOrganic = model->soilOrganic;
  debug() << "MONICA model: applyOrganicFertiliser:\t" << amountFM << "\t" << params.vo_NConcentration << endl;
  soilOrganic->incorporation = incorporation;
  soilOrganicAddOrganicMatter(soilOrganic.get(), params, amountFM, params.vo_NConcentration, incorporateIntoLayerIndex);
  monicamodel::addDailySumOrgFertiliser(model, amountFM, params);
  monicamodel::addDailySumOrganicFertilizerDM(model, amountFM * params.vo_AOM_DryMatterContent);
}

double monica::monicamodel::applyMineralFertiliserViaNMinMethod(MonicaModel* model,
                                                              MineralFertilizerParameters partition,
                                                              NMinCropParameters cps) {
  auto& simPs = model->simPs;
  auto& soilColumn = model->soilColumn;
  const NMinApplicationParameters& ups = simPs.p_NMinUserParams;
  return soilColumnApplyMineralFertiliserViaNMinMethod(soilColumn.get(),
                                                        partition,
                                                        cps.samplingDepth,
                                                        cps.nTarget,
                                                        cps.nTarget30,
                                                        ups.max,
                                                        ups.min,
                                                        ups.delayInDays);
}

void monica::monicamodel::addDailySumOrgFertiliser(MonicaModel* model,
                                                 double amountFM,
                                                 const OrganicMatterParameters& params) {
  auto& dailySumOrgFertiliser = model->dailySumOrgFertiliser;
  auto& sumOrgFertiliser = model->sumOrgFertiliser;
  auto& soilColumn = model->soilColumn;
  using namespace Soil;
  double AOM_fast_factor = OrganicConstants::po_AOM_to_C * params.vo_PartAOM_to_AOM_Fast / params.vo_CN_Ratio_AOM_Fast;
  double AOM_slow_factor = OrganicConstants::po_AOM_to_C * params.vo_PartAOM_to_AOM_Slow / params.vo_CN_Ratio_AOM_Slow;
  double SOM_Factor =
    (1 - (params.vo_PartAOM_to_AOM_Fast + params.vo_PartAOM_to_AOM_Slow)) * OrganicConstants::po_AOM_to_C /
    soilColumn->at(0)._sps.vs_Soil_CN_Ratio; // TODO ask CN for correctness

  double conversion =
    AOM_fast_factor + AOM_slow_factor + SOM_Factor + params.vo_AOM_NH4Content + params.vo_AOM_NO3Content;

  dailySumOrgFertiliser += amountFM * params.vo_AOM_DryMatterContent * conversion;
  sumOrgFertiliser += amountFM * params.vo_AOM_DryMatterContent * conversion;
}

void monica::monicamodel::dailyReset(MonicaModel* model) {
  auto& dailySumIrrigationWater = model->dailySumIrrigationWater;
  auto& dailySumFertiliser = model->dailySumFertiliser;
  auto& dailySumOrgFertiliser = model->dailySumOrgFertiliser;
  auto& dailySumOrganicFertilizerDM = model->dailySumOrganicFertilizerDM;
  auto& optCarbonExportedResidues = model->optCarbonExportedResidues;
  auto& optCarbonReturnedResidues = model->optCarbonReturnedResidues;
  auto& clearCropUponNextDay = model->clearCropUponNextDay;
  auto& soilTransport = model->soilTransport;
  auto& soilColumn = model->soilColumn;
  auto& soilMoisture = model->soilMoisture;
  auto& soilOrganic = model->soilOrganic;
  auto& currentCropModule = model->currentCropModule;

  dailySumIrrigationWater = 0.0;
  dailySumFertiliser = 0.0;
  dailySumOrgFertiliser = 0.0;
  dailySumOrganicFertilizerDM = 0.0;
  optCarbonExportedResidues = 0.0;
  optCarbonReturnedResidues = 0.0;
  monicamodel::clearEvents(model);

  if (clearCropUponNextDay) {
    soiltransport::removeCrop(soilTransport.get());
    soilColumnRemoveCrop(soilColumn.get());
    soilMoisture->cropModule = nullptr;
    soilOrganic->cropModule = nullptr;
    currentCropModule = kj::Own<CropModule>();

    clearCropUponNextDay = false;
  }
}

void monica::monicamodel::applyIrrigation(MonicaModel* model,
                                        double amount,
                                        double nitrateConcentration,
                                        double /*sulfateConcentration*/) {
  auto& simPs = model->simPs;
  auto& soilColumn = model->soilColumn;
  auto& soilOrganic = model->soilOrganic;
  //if the production process has still some defined manual irrigation dates
  if (!simPs.p_UseAutomaticIrrigation) {
    soilColumnApplyIrrigation(soilColumn.get(), amount, nitrateConcentration);
    soilOrganic->irrigationAmount += amount;
    monicamodel::addDailySumIrrigationWater(model, amount);
  }
}

/**
 * Applies tillage for a given soil depth. Tillage means in MONICA,
 * that for all effected soil layer the parameters are averaged.
 * @param depth Depth in meters
 */
void monica::monicamodel::applyTillage(MonicaModel* model, double depth) {
  soilColumnApplyTillage(model->soilColumn.get(), depth);
}

void monica::monicamodel::step(MonicaModel* model) {
  auto& clearCropUponNextDay = model->clearCropUponNextDay;
  auto& intercropping = model->intercropping;
  if (model->currentCropModule && !clearCropUponNextDay) {
    monicamodel::cropStep(model);
  } else if (intercropping.isAsync()) {
    // tell other side that there is currently no crop
    auto wreq = intercropping.writer.writeRequest();
    auto wval = wreq.initValue();
    wval.setNoCrop();
    debug() << "MonicaModel::step -> send no-crop" << std::endl;
    auto prom = wreq.send(); //.wait(intercropping.ioContext->waitScope);
    //prom.eagerlyEvaluate(nullptr); //[](kj::Exception&& ex){ cout << "MonicaModel::step write noCrop failed: " << ex.getDescription().cStr() << endl;});
    // wait for other sides crop height or no crop info
    auto val = intercropping.reader.readRequest().send().wait(intercropping.ioContext->waitScope).getValue();
    debug() << "MonicaModel::step -> sent no-crop, received ";
    if (val.isHeight()) cout << " height: " << val.getHeight() << endl;
    else if (val.isNoCrop()) cout << "no-crop" << endl;
    else if (val.isLait()) cout << " LAI_t: " << val.getLait() << " ---> Error: shouldn't happen." << endl;
  }

  monicamodel::generalStep(model);
}

/**
 * @brief Simulating the soil processes for one time step.
 * @param stepNo Number of current processed step
 */
void monica::monicamodel::generalStep(MonicaModel* model) {
  auto& currentStepDate = model->currentStepDate;
  auto& climateData = model->climateData;
  auto& groundwaterInformation = model->groundwaterInformation;
  auto& envPs = model->envPs;
  auto& simPs = model->simPs;
  auto& soilColumn = model->soilColumn;
  auto& soilTemperature = model->soilTemperature;
  auto& soilMoisture = model->soilMoisture;
  auto& soilOrganic = model->soilOrganic;
  auto& soilTransport = model->soilTransport;
  auto& currentCropModule = model->currentCropModule;
  auto& vs_GroundwaterDepth = model->vs_GroundwaterDepth;
  auto& vw_AtmosphericCO2Concentration = model->vw_AtmosphericCO2Concentration;

  auto date = currentStepDate;
  unsigned int julday = date.julianDay();
  bool leapYear = date.isLeapYear();

  const auto& dailyClimate = climateData.back();
  double tmin = dailyClimate.at(Climate::tmin);
  double tavg = dailyClimate.at(Climate::tavg);
  double tmax = dailyClimate.at(Climate::tmax);
  double precip = dailyClimate.at(Climate::precip);
  double wind = dailyClimate.at(Climate::wind);
  double globrad = dailyClimate.at(Climate::globrad);

  // test if data for relhumid are available; if not, value is set to -1.0
  double relhumid = dailyClimate.find(Climate::relhumid) == dailyClimate.end()
                      ? -1.0
                      : dailyClimate.at(Climate::relhumid);


  // test if simulated gw or measured values should be used
  auto gw_value_p = groundwaterInformation.getGroundwaterInformation(date);
  //  cout << "vs_GroundwaterDepth:\t" << envPs.p_MinGroundwaterDepth << "\t" << envPs.p_MaxGroundwaterDepth << endl;
  vs_GroundwaterDepth = gw_value_p.first
                          ? max(0.0, gw_value_p.second)
                          : monicamodel::groundwaterDepthForDate(envPs.p_MaxGroundwaterDepth,
                                                               envPs.p_MinGroundwaterDepth,
                                                               envPs.p_MinGroundwaterDepthMonth,
                                                               julday,
                                                               leapYear);

  // first try to get CO2 concentration from climate data
  auto co2it = dailyClimate.find(Climate::co2);
  if (co2it != dailyClimate.end()) {
    vw_AtmosphericCO2Concentration = co2it->second;
  } else {
    // try to get yearly values from UserEnvironmentParameters
    auto co2sit = envPs.p_AtmosphericCO2s.find(date.year());
    if (co2sit != envPs.p_AtmosphericCO2s.end()) {
      vw_AtmosphericCO2Concentration = co2sit->second;
      // potentially use MONICA algorithm to calculate CO2 concentration
    } else if (int(envPs.p_AtmosphericCO2) <= 0) {
      vw_AtmosphericCO2Concentration = monicamodel::CO2ForDate(date, envPs.rcp);
      // if everything fails value in UserEnvironmentParameters for the whole simulation
    } else {
      vw_AtmosphericCO2Concentration = envPs.p_AtmosphericCO2;
    }
  }

  //  debug << "step: " << stepNo << " p: " << precip << " gr: " << globrad << endl;

  soilColumnDeleteAOMPool(soilColumn.get());

  auto possibleDelayedFertilizerAmount = soilColumnApplyPossibleDelayedFerilizer(soilColumn.get());
  monicamodel::addDailySumFertiliser(model, possibleDelayedFertilizerAmount);
  double possibleTopDressingAmount = soilColumnApplyPossibleTopDressing(soilColumn.get());
  monicamodel::addDailySumFertiliser(model, possibleTopDressingAmount);

  if (currentCropModule
      && simPs.p_UseNMinMineralFertilisingMethod
      && currentCropModule->isWinterCrop
      && julday == simPs.p_JulianDayAutomaticFertilising) {
    soilColumnClearTopDressingParams(soilColumn.get());
    debug() << "nMin fertilising winter crop" << endl;
    auto sps = currentCropModule->speciesPs;
    double fertilizerAmount = monicamodel::applyMineralFertiliserViaNMinMethod
      (model, simPs.p_NMinFertiliserPartition,
       NMinCropParameters(sps.pc_SamplingDepth, sps.pc_TargetNSamplingDepth, sps.pc_TargetN30));
    monicamodel::addDailySumFertiliser(model, fertilizerAmount);
  }

  soiltemperature::step(soilTemperature.get(), tmin, tmax, globrad);

  // first try to get ReferenceEvapotranspiration from climate data
  auto et0_it = dailyClimate.find(Climate::et0);
  double et0 = et0_it == dailyClimate.end() ? -1.0 : et0_it->second;

  soilmoisture::step(soilMoisture.get(), vs_GroundwaterDepth, precip, tmax, tmin,
                   (relhumid / 100.0), tavg, wind, envPs.p_WindSpeedHeight, globrad,
                   julday, et0);

  soilOrganicStep(soilOrganic.get(), tavg, precip, wind);
  soiltransport::step(soilTransport.get());
}

pair<double, double> laiSunShade(double latitude, int doy, int hour, double lai) {
  //taken from Astronomy.cs Astronomy::SolarDeclination and Astronomy::SolarElevation
  double PI = 3.1415926536;
  double solarDeclination = -0.4093 * cos(2 * PI * (doy + 10) / 365);
  double dA = sin(solarDeclination) * sin(latitude);
  double dB = cos(solarDeclination) * cos(latitude);
  double dHa = PI * (hour - 12) / 12;
  double phi = asin(dA + dB * cos(dHa));

  //taken from Norman, J.M., 1982. Simulation of microclimates. In:
  //Hatfield, J.L., Thomason, I.J. (Eds.), Biometeorology in
  //Integrated Pest Management.Academic Press, New York,
  //pp. 65–99. Equation (14)
  double laiSun = 2 * cos(phi) * (1 - exp(-0.5 * lai / cos(phi)));

  return make_pair(laiSun, lai - laiSun);
}

void monica::monicamodel::cropStep(MonicaModel* model) {
  auto& currentStepDate = model->currentStepDate;
  auto& climateData = model->climateData;
  auto& currentCropModule = model->currentCropModule;
  auto& envPs = model->envPs;
  auto& simPs = model->simPs;
  auto& soilColumn = model->soilColumn;
  auto& soilOrganic = model->soilOrganic;
  auto& p_daysWithCrop = model->p_daysWithCrop;
  auto& p_accuNStress = model->p_accuNStress;
  auto& p_accuWaterStress = model->p_accuWaterStress;
  auto& p_accuHeatStress = model->p_accuHeatStress;
  auto& p_accuOxygenStress = model->p_accuOxygenStress;
  auto& vw_AtmosphericCO2Concentration = model->vw_AtmosphericCO2Concentration;
  auto& vw_AtmosphericO3Concentration = model->vw_AtmosphericO3Concentration;

  auto date = currentStepDate;
  const auto& dailyClimate = climateData.back();

  // do nothing if there is no crop
  if (!currentCropModule) return;

  p_daysWithCrop++;

  unsigned int julday = date.julianDay();

  double tavg = dailyClimate.at(Climate::tavg);
  double tmax = dailyClimate.at(Climate::tmax);
  double tmin = dailyClimate.at(Climate::tmin);
  double globrad = dailyClimate.at(Climate::globrad);

  // first try to get CO2 concentration from climate data
  auto o3it = dailyClimate.find(Climate::o3);
  if (o3it != dailyClimate.end()) {
    vw_AtmosphericO3Concentration = o3it->second;
  } else {
    // try to get yearly values from UserEnvironmentParameters
    auto o3sit = envPs.p_AtmosphericO3s.find(date.year());
    if (o3sit != envPs.p_AtmosphericO3s.end()) {
      vw_AtmosphericO3Concentration = o3sit->second;
      // if everything fails value in UserEnvironmentParameters for the whole simulation
    } else {
      vw_AtmosphericO3Concentration = envPs.p_AtmosphericO3;
    }
  }

  // test if data for sunhours are available; if not, value is set to -1.0
  auto sunhit = dailyClimate.find(Climate::sunhours);
  double sunhours = sunhit == dailyClimate.end() ? -1.0 : sunhit->second;

  // test if data for relhumid are available; if not, value is set to -1.0
  auto rhit = dailyClimate.find(Climate::relhumid);
  double relhumid = rhit == dailyClimate.end() ? -1.0 : rhit->second;

  auto wind_it = dailyClimate.find(Climate::wind);
  double wind = wind_it == dailyClimate.end() ? -1.0 : wind_it->second;

  double precip = dailyClimate.at(Climate::precip);

  // check if reference evapotranspiration was provided via climate files
  auto et0_it = dailyClimate.find(Climate::et0);
  double et0 = et0_it == dailyClimate.end() ? -1.0 : et0_it->second;

  double vw_WindSpeedHeight = envPs.p_WindSpeedHeight;

  cropmodule::step(currentCropModule.get(),
                 tavg,
                 tmax,
                 tmin,
                 globrad,
                 sunhours,
                 date,
                 (relhumid / 100.0),
                 wind,
                 vw_WindSpeedHeight,
                 vw_AtmosphericCO2Concentration,
                 vw_AtmosphericO3Concentration,
                 precip,
                 et0);
  if (simPs.p_UseAutomaticIrrigation
      && (!simPs.p_AutoIrrigationParams.startDate.isValid() || simPs.p_AutoIrrigationParams.startDate <= date)
      && (!simPs.p_AutoIrrigationParams.endDate.isValid() || date <= simPs.p_AutoIrrigationParams.endDate)) {
    const AutomaticIrrigationParameters& aips = simPs.p_AutoIrrigationParams;
    bool irrigationTriggered = false;
    double irrigationAmount = 0.0;
    tie(irrigationTriggered, irrigationAmount) = soilColumnApplyIrrigationViaTrigger(soilColumn.get(), aips);
    if (irrigationTriggered) {
      soilOrganic->irrigationAmount += irrigationAmount;
      monicamodel::addDailySumIrrigationWater(model, irrigationAmount);
    }
  }

  p_accuNStress += currentCropModule->vc_CropNRedux;
  p_accuWaterStress += currentCropModule->vc_TranspirationDeficit;
  p_accuHeatStress += currentCropModule->vc_CropHeatRedux;
  p_accuOxygenStress += currentCropModule->vc_OxygenDeficit;

  /*
  //prepare VOC calculations
  //TODO: right now assumes that we have daily values and thus xxx24 is the same as xxx
  double globradWm2 = globrad * 1000000.0 / 86400.0; //MJ/m2/d (a sum) -> W/m2
  if(index240 < stepSize240 - 1)
  {
    index240++;
  }
  else
  {
    index240 = 0;
    full240 = true;
  }
  rad240[index240] = globradWm2;
  tfol240[index240] = tavg;

  Voc::MicroClimateData mcd;
  //hourly or time step average global radiation (in case of monica usually 24h)
  mcd.rad = globradWm2;
  mcd.rad24 = mcd.rad; //just daily values, thus average over 24h is the same
  mcd.rad240 = accumulate(rad240.begin(), rad240.end(), 0.0) / (full240 ? rad240.size() : index240 + 1);
  mcd.tFol = tavg;
  mcd.tFol24 = mcd.tFol;
  mcd.tFol240 = accumulate(tfol240.begin(), tfol240.end(), 0.0) / (full240 ? tfol240.size() : index240 + 1);

  double lai = currentCropModule->get_LeafAreaIndex();
  auto sunShadeLaiAtZenith = laiSunShade(sitePs.vs_Latitude, julday, 12, lai);
  mcd.sunlitfoliagefraction = sunShadeLaiAtZenith.first / lai;
  mcd.sunlitfoliagefraction24 = mcd.sunlitfoliagefraction;

  //debug() << "calculating voc emissions at " << date.toIsoDateString() << endl;
  cropmodule::calculateVOCEmissions(currentCropModule.get(), mcd);
  */
}

/**
* @brief Returns atmospheric CO2 concentration for date [ppm]
*
* @param year, julianDay, isLeapYear
* @param rcp
*
* @return
*/
double monica::monicamodel::CO2ForDate(const double year,
                                     const double julianDay,
                                     const bool isLeapYear,
                                     const mas::schema::climate::RCP rcp) {
  using namespace mas::schema::climate;
  const double decimalDate = year + julianDay / (isLeapYear ? 366.0 : 365);

  //old value
  double co2 = 0;
  switch (rcp) {
  case RCP::RCP19: co2 = 309.61 + 110.21 / (1 + exp(-(0.0819 * (decimalDate - 1995.41)))) + (
                           2.5 * sin((decimalDate - 0.5) / 0.1592));
    break;
  case RCP::RCP26: co2 = 306.23 + 158.52 / (1 + exp(-(0.0601 * (decimalDate - 2005.77)))) + (
                           2.5 * sin((decimalDate - 0.5) / 0.1592));
    break;
  case RCP::RCP34: co2 = 302.24 + 185.07 / (1 + exp(-(0.0512 * (decimalDate - 2010.26)))) + (
                           2.5 * sin((decimalDate - 0.5) / 0.1592));
    break;
  case RCP::RCP45: co2 = 292.83 + 348.08 / (1 + exp(-(0.0349 * (decimalDate - 2036.46)))) + (
                           2.5 * sin((decimalDate - 0.5) / 0.1592));
    break;
  case RCP::RCP60: co2 = 287.56 + 474.21 / (1 + exp(-(0.0301 * (decimalDate - 2052.37)))) + (
                           2.5 * sin((decimalDate - 0.5) / 0.1592));
    break;
  case RCP::RCP70: co2 = 277.25 + 1338.47 / (1 + exp(-(0.0237 * (decimalDate - 2110.06)))) + (
                           2.5 * sin((decimalDate - 0.5) / 0.1592));
    break;
  case RCP::RCP85:
  default: co2 = 294.27 + 2361.06 / (1 + exp(-(0.0287 * (decimalDate - 2120.53)))) + (
                   2.5 * sin((decimalDate - 0.5) / 0.1592));
    break;
  }

  return co2;
}

double monica::monicamodel::CO2ForDate(const Date& d, const mas::schema::climate::RCP rcp) {
  return monicamodel::CO2ForDate(d.year(), d.julianDay(), d.isLeapYear(), rcp);
}


/**
* @brief Returns groundwater table for date [m]
*
* @param pm_MaxGroundwaterTable; pm_MinGroundwaterTable; pm_MaxGroundwaterTableMonth
*
* @return
*/
double monica::monicamodel::groundwaterDepthForDate(double maxGroundwaterDepth,
                                                  double minGroundwaterDepth,
                                                  int minGroundwaterDepthMonth,
                                                  double julianDay,
                                                  bool isLeapYear) {
  double days = 365;
  if (isLeapYear) days = 366.0;

  double meanGroundwaterDepth = (maxGroundwaterDepth + minGroundwaterDepth) / 2.0;
  double groundwaterAmplitude = (maxGroundwaterDepth - minGroundwaterDepth) / 2.0;

  double sinus = sin(((julianDay / days * 360.0) - 90.0 -
                      ((double(minGroundwaterDepthMonth) * 30.0) - 15.0)) *
                     3.14159265358979 / 180.0);

  double groundwaterDepth = meanGroundwaterDepth + (sinus * groundwaterAmplitude);

  if (groundwaterDepth < 0.0) groundwaterDepth = 20.0;

  return groundwaterDepth;
}

void monica::monicamodel::clearEvents(MonicaModel* model) {
  model->previousDaysEvents = model->currentEvents;
  model->currentEvents.clear();
}

void monica::monicamodel::setOtherCropHeightAndLAIt(MonicaModel* model, double cropHeight, double lait) {
  if (model->currentCropModule) {
    cropmodule::setOtherCropHeightAndLAIt(model->currentCropModule.get(), cropHeight, lait);
  }
}
