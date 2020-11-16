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
#include "monica-model.h"
#include "climate/climate-common.h"
#include "db/abstract-db-connections.h"
#include "voc-common.h"
#include "tools/algorithms.h"

using namespace Monica;
using namespace std;
using namespace Climate;
using namespace Tools;
using namespace Soil;

namespace
{
	//! simple functor for use in fertiliser trigger
  struct AddFertiliserAmountsCallback
  {
		double& _sf;
		double& _dsf;
		AddFertiliserAmountsCallback(double& sf, double& dsf) : _sf(sf), _dsf(dsf) { }
    void operator()(double v)
    {
            // cout << "!!!AddFertiliserAmountsCallback: daily_sum_fert += " << v << endl;
			_sf += v;
			_dsf += v;
		}
	};

}

//------------------------------------------------------------------------------

MonicaModel::MonicaModel(const CentralParameterProvider& cpp)
  : _sitePs(cpp.siteParameters)
  , _smPs(cpp.userSoilMoistureParameters)
  , _envPs(cpp.userEnvironmentParameters)
  , _cropPs(cpp.userCropParameters)
  , _soilTempPs(cpp.userSoilTemperatureParameters)
  , _soilTransPs(cpp.userSoilTransportParameters)
  , _soilOrganicPs(cpp.userSoilOrganicParameters)
  , _simPs(cpp.simulationParameters)
  //, _writeOutputFiles(cpp.writeOutputFiles())
  //, _pathToOutputDir(cpp.pathToOutputDir())
  , _groundwaterInformation(cpp.groundwaterInformation)
  , _soilColumn(kj::heap<SoilColumn>(_simPs.p_LayerThickness,
    _soilOrganicPs.ps_MaxMineralisationDepth,
    _sitePs.vs_SoilParameters,
    _smPs.pm_CriticalMoistureDepth))
  , _soilTemperature(kj::heap<SoilTemperature>(*this))
  , _soilMoisture(kj::heap<SoilMoisture>(*this))
  , _soilOrganic(kj::heap<SoilOrganic>(_soilColumn.get(),
                 _soilOrganicPs))
  , _soilTransport(kj::heap<SoilTransport>(_soilColumn.get(),
                   _sitePs,
                   _soilTransPs,
                   _envPs.p_LeachingDepth,
                   _envPs.p_timeStep,
                   _cropPs.pc_MinimumAvailableN))
	//, _rad24(_stepSize24)
	//, _rad240(_stepSize240)
	//, _tfol24(_stepSize24)
	//, _tfol240(_stepSize240)
  , vw_AtmosphericCO2Concentration(_envPs.p_AtmosphericCO2)
{}

void SoilColumn::deserialize(mas::models::monica::SoilColumnState::Reader reader) {

}

void MonicaModel::serialize(mas::models::monica::MonicaModelState::Builder builder) {
	builder.initSitePs();
	_sitePs.serialize(builder.getSitePs());

	builder.initSmPs();
	_smPs.serialize(builder.getSmPs());

	builder.initEnvPs();
	_envPs.serialize(builder.getEnvPs());

	builder.initCropPs();
	_cropPs.serialize(builder.getCropPs());

	builder.initSoilTempPs();
	_soilTempPs.serialize(builder.getSoilTempPs());

	builder.initSoilTransPs();
	_soilTransPs.serialize(builder.getSoilTransPs());

	builder.initSoilOrganicPs();
	_soilOrganicPs.serialize(builder.getSoilOrganicPs());

	builder.initSimPs();
	_simPs.serialize(builder.getSimPs());

	builder.initGroundwaterInformation();
	_groundwaterInformation.serialize(builder.getGroundwaterInformation());

	builder.initSoilColumn();
	/*
	_soilColumn                      @9  :SoilColumnState; # main soil data structure
		soilTemperature                 @10 :SoilTemperatureModuleState; # temperature code
		soilMoisture                    @11 :SoilMoistureModuleState; # moisture code
		soilOrganic                     @12 :SoilOrganicModuleState; # organic code
		soilTransport                   @13 :SoilTransportModuleState; # transport code
		currentCrop                     @14 :Crop; # currently possibly planted crop
		currentCropGrowth               @15 :CropModuleState; # crop code for possibly planted crop

		sumFertiliser                   @16 :Float64;
	sumOrgFertiliser                @17 :Float64;

	dailySumFertiliser              @18 :Float64;
	dailySumOrgFertiliser           @19 :Float64;

	dailySumOrganicFertilizerDM     @20 :Float64;
	sumOrganicFertilizerDM          @21 :Float64;

	humusBalanceCarryOver           @22 :Float64;

	dailySumIrrigationWater         @23 :Float64;

	optCarbonExportedResidues       @24 :Float64;
	optCarbonReturnedResidues       @25 :Float64;

	currentStepDate                 @26 :Date;

	struct ACDToValue {
		acd     @0 :UInt16;
		value   @1 :Float64;
	}
	climateData                     @27 :List(List(ACDToValue));
	currentEvents                   @28 :List(Text);
	previousDaysEvents              @29 :List(Text);

	clearCropUponNextDay            @30 :Bool;

	daysWithCrop                    @31 :UInt16;
	accuNStress                     @32 :Float64;
	accuWaterStress                 @33 :Float64;
	accuHeatStress                  @34 :Float64;
	accuOxygenStress                @35 :Float64;

	vwAtmosphericCO2Concentration   @36 :Float64;
	vwAtmosphericO3Concentration    @37 :Float64;
	vsGroundwaterDepth              @38 :Float64;

	cultivationMethodCount          @39 :UInt16;
	*/

}



/**
 * @brief Simulation of crop seed.
 * @param crop to be planted
 */
void MonicaModel::seedCrop(CropPtr crop)
{
  debug() << "seedCrop" << endl;
  delete _currentCropGrowth;
	_currentCropGrowth = NULL;
	
	p_daysWithCrop = 0;
  p_accuNStress = 0.0;
  p_accuWaterStress = 0.0;
  p_accuHeatStress = 0.0;
  p_accuOxygenStress = 0.0;

  if(crop->isValid())
  {
		_currentCrop = crop;
		_cultivationMethodCount++;

		auto addOMFunc = [this](std::map<size_t, double> layer2amount, double nconc)
		{
			this->_soilOrganic->addOrganicMatter(this->_currentCrop->residueParameters(), layer2amount, nconc); 
		};
    auto cps = _currentCrop->cropParameters();
    _currentCropGrowth = kj::heap<CropGrowth>(
      _soilColumn.get(),
      *cps,
      _sitePs,
      _cropPs,
      _simPs,
      [this](string event) { this->addEvent(event); },
      addOMFunc,
      crop->getEva2TypeUsage());

    if (_currentCrop->perennialCropParameters())
      _currentCropGrowth->setPerennialCropParameters(_currentCrop->perennialCropParameters());

    _soilTransport->put_Crop(_currentCropGrowth);
    _soilColumn->put_Crop(_currentCropGrowth);
    _soilMoisture->put_Crop(_currentCropGrowth);
    _soilOrganic->put_Crop(_currentCropGrowth);

//    debug() << "seedDate: "<< _currentCrop->seedDate().toString()
//            << " harvestDate: " << _currentCrop->harvestDate().toString() << endl;

		if(_simPs.p_UseNMinMineralFertilisingMethod
			 && !currentCrop()->isWinterCrop())
    {
			_soilColumn->clearTopDressingParams();
      debug() << "nMin fertilising summer crop" << endl;
      double fert_amount = applyMineralFertiliserViaNMinMethod
                           (_simPs.p_NMinFertiliserPartition,
                            NMinCropParameters(cps->speciesParams.pc_SamplingDepth,
                                               cps->speciesParams.pc_TargetNSamplingDepth,
                                               cps->speciesParams.pc_TargetN30));
      addDailySumFertiliser(fert_amount);
    }
  }
}

/**
 * @brief Simulating harvest of crop.
 *
 * Deletes the current crop.
 */
void MonicaModel::harvestCurrentCrop(bool exported, 
																		 Harvest::OptCarbonManagementData optCarbMgmtData)
{
	//could be just a fallow, so there might be no CropGrowth object
	if (_currentCrop && _currentCrop->isValid())
  {
    if(exported)
    {
			//prepare to add root and crop residues to soilorganic (AOMs)
			//dead root biomass has already been added daily, so just living root biomass is left
			double rootBiomass = _currentCropGrowth->get_OrganGreenBiomass(0); 
			double rootNConcentration = _currentCropGrowth->get_RootNConcentration();
			debug() << "adding organic matter from root to soilOrganic" << endl;
			debug() << "root biomass: " << rootBiomass
				<< " Root N concentration: " << rootNConcentration << endl;

			_currentCropGrowth->addAndDistributeRootBiomassInSoil(rootBiomass);
			//_soilOrganic.addOrganicMatter(_currentCrop->residueParameters(),
			//															rootBiomass,
			//															rootNConcentration);

			if(optCarbMgmtData.optCarbonConservation)
			{
				double residueBiomass = _currentCropGrowth->get_ResidueBiomass(false); //kg ha-1, secondary yield is ignored with this approach
				double cropContribToHumus = optCarbMgmtData.cropImpactOnHumusBalance;
				double appliedOrganicFertilizerDryMatter = sumOrganicFertilizerDM(); //kg ha-1
				double intermediateHumusBalance = _humusBalanceCarryOver + cropContribToHumus + appliedOrganicFertilizerDryMatter / 1000.0 * optCarbMgmtData.organicFertilizerHeq - _sitePs.vs_SoilSpecificHumusBalanceCorrection;
				double potentialHumusFromResidues = residueBiomass / 1000.0 * optCarbMgmtData.residueHeq;
				
				double fractionToBeLeftOnField = 0.0;
				if (potentialHumusFromResidues > 0)
				{
					fractionToBeLeftOnField = -intermediateHumusBalance / potentialHumusFromResidues;
					//0 <= fractionToBeLeftOnField <= 1
					if (fractionToBeLeftOnField > 1)
					{
						fractionToBeLeftOnField = 1.0;
					}
					else if (fractionToBeLeftOnField < 0)
					{
						fractionToBeLeftOnField = 0.0;
					}
				}
				
				if(optCarbMgmtData.cropUsage == Harvest::greenManure)
					//if the crop is used as green manure, all the residues are incorporated regardless the humus balance
					fractionToBeLeftOnField = 1.0;

				//calculate theoretical residue removal
				_optCarbonReturnedResidues = residueBiomass * fractionToBeLeftOnField;
				_optCarbonExportedResidues = residueBiomass - _optCarbonReturnedResidues;
				
				//adjust it if technically unfeasible
				double maxExportedResidues = residueBiomass * optCarbMgmtData.maxResidueRecoverFraction;
				if (_optCarbonExportedResidues > maxExportedResidues)
				{
					_optCarbonExportedResidues = maxExportedResidues;
					_optCarbonReturnedResidues = residueBiomass - _optCarbonExportedResidues;
				}

				_soilOrganic->addOrganicMatter(_currentCrop->residueParameters(),
																			_optCarbonReturnedResidues,
																			_currentCropGrowth->get_ResiduesNConcentration());
				
				
				_humusBalanceCarryOver = intermediateHumusBalance + _optCarbonReturnedResidues / 1000.0 * optCarbMgmtData.residueHeq;
			}
			else //normal case
			{

				double residueBiomass = _currentCropGrowth->get_ResidueBiomass(_simPs.p_UseSecondaryYields);

				//!@todo Claas: das hier noch berechnen
				double residueNConcentration = _currentCropGrowth->get_ResiduesNConcentration();
				debug() << "adding organic matter from residues to soilOrganic" << endl;
				debug() << "residue biomass: " << residueBiomass
					<< " Residue N concentration: " << residueNConcentration << endl;
				debug() << "primary yield biomass: " << _currentCropGrowth->get_PrimaryCropYield()
					<< " Primary yield N concentration: " << _currentCropGrowth->get_PrimaryYieldNConcentration() << endl;
				debug() << "secondary yield biomass: " << _currentCropGrowth->get_SecondaryCropYield()
					<< " Secondary yield N concentration: " << _currentCropGrowth->get_PrimaryYieldNConcentration() << endl;
				debug() << "Residues N content: " << _currentCropGrowth->get_ResiduesNContent()
					<< " Primary yield N content: " << _currentCropGrowth->get_PrimaryYieldNContent()
					<< " Secondary yield N content: " << _currentCropGrowth->get_SecondaryYieldNContent() << endl;

				_soilOrganic->addOrganicMatter(_currentCrop->residueParameters(),
																			residueBiomass,
																			residueNConcentration);
			}
		}
		else 
		{
			//prepare to add the total plant to soilorganic (AOMs)
			double abovegroundBiomass = _currentCropGrowth->get_AbovegroundBiomass();
			double abovegroundBiomassNConcentration =
				_currentCropGrowth->get_AbovegroundBiomassNConcentration();
			debug() << "adding organic matter from aboveground biomass to soilOrganic" << endl;
			debug() << "aboveground biomass: " << abovegroundBiomass
				<< " Aboveground biomass N concentration: " << abovegroundBiomassNConcentration << endl;
			double rootBiomass = _currentCropGrowth->get_OrganBiomass(0);
			double rootNConcentration = _currentCropGrowth->get_RootNConcentration();
			debug() << "adding organic matter from root to soilOrganic" << endl;
			debug() << "root biomass: " << rootBiomass
				<< " Root N concentration: " << rootNConcentration << endl;

			_soilOrganic->addOrganicMatter(_currentCrop->residueParameters(),
																		abovegroundBiomass,
																		abovegroundBiomassNConcentration);

			_currentCropGrowth->addAndDistributeRootBiomassInSoil(rootBiomass);
			//_soilOrganic.addOrganicMatter(_currentCrop->residueParameters(),
			//															rootBiomass,
			//															rootNConcentration);
		}
	}

	_clearCropUponNextDay = true;
}

void MonicaModel::fruitHarvestCurrentCrop(double percentage, bool exported)
{
	//could be just a fallow, so there might be no CropGrowth object
	if (_currentCrop.get() && _currentCrop->isValid())
	{
		//prepare to remove fruit
		double totalBiomassNContent = _currentCropGrowth->get_TotalBiomassNContent();
		double currentFruitBiomass = _currentCropGrowth->get_OrganBiomass(3);
		double currentFruitNContent = _currentCropGrowth->get_FruitBiomassNContent();
//		double fruitToRemove = percentage * currentFruitBiomass;
		double fruitNToRemove = percentage * currentFruitNContent;
		double fruitToRemain = (1.0 - percentage) * currentFruitBiomass;
		double totalBiomassNToRemain = totalBiomassNContent - fruitNToRemove;
		
		_currentCropGrowth->accumulatePrimaryCropYield(_currentCropGrowth->get_PrimaryCropYield());
		_currentCropGrowth->set_OrganBiomass(3, fruitToRemain);
		_currentCropGrowth->set_TotalBiomassNContent(totalBiomassNToRemain);

    if(exported)
		{
			//no crop residues are added to soilorganic (AOMs)
			debug() << "adding no organic matter from fruit residues to soilOrganic" << endl;
		}
	}
}

void MonicaModel::leafPruningCurrentCrop(double percentage, bool exported)
{
	//could be just a fallow, so there might be no CropGrowth object
  if(_currentCrop.get() && _currentCrop->isValid())
	{
		//prepare to remove leaves
		double currentLeafBiomass = _currentCropGrowth->get_OrganBiomass(1);
		double leavesToRemove = percentage * currentLeafBiomass;
		double leavesToRemain = (1.0 - percentage) * currentLeafBiomass;
		_currentCropGrowth->set_OrganBiomass(1, leavesToRemain);

    if(!exported)
    {
			//prepare to add crop residues to soilorganic (AOMs)
			double leafResidueNConcentration = _currentCropGrowth->get_ResiduesNConcentration();
			debug() << "adding organic matter from leaf residues to soilOrganic" << endl;
			debug() << "leaf residue biomass: " << leavesToRemove
				<< " Leaf residue N concentration: " << leafResidueNConcentration << endl;
			
			_soilOrganic->addOrganicMatter(_currentCrop->residueParameters(),
                                    leavesToRemove,
                                    leafResidueNConcentration);
		}
	}
}

void MonicaModel::tipPruningCurrentCrop(double percentage, bool exported)
{
	//could be just a fallow, so there might be no CropGrowth object
  if(_currentCrop.get() && _currentCrop->isValid())
	{
		//prepare to remove tips
		double currentLeafBiomass = _currentCropGrowth->get_OrganBiomass(1);
		double currentShootBiomass = _currentCropGrowth->get_OrganBiomass(2);
		double leavesToRemove = percentage * currentLeafBiomass;
		double shootsToRemove = percentage * currentShootBiomass;
		double leavesToRemain = (1.0 - percentage) * currentLeafBiomass;
		double shootsToRemain = (1.0 - percentage) * currentShootBiomass;
		_currentCropGrowth->set_OrganBiomass(1, leavesToRemain);
		_currentCropGrowth->set_OrganBiomass(2, shootsToRemain);

    if(!exported)
    {
			//prepare to add crop residues to soilorganic (AOMs)
			double tipResidues = leavesToRemove + shootsToRemove;
			double tipResidueNConcentration = _currentCropGrowth->get_ResiduesNConcentration();
			debug() << "adding organic matter from tip residues to soilOrganic" << endl;
			debug() << "Tip residue biomass: " << tipResidues
				<< " Tip residue N concentration: " << tipResidueNConcentration << endl;

			_soilOrganic->addOrganicMatter(_currentCrop->residueParameters(),
                                    tipResidues,
                                    tipResidueNConcentration);
		}
	}
}

	void MonicaModel::shootPruningCurrentCrop(double percentage, bool exported)
{
	//could be just a fallow, so there might be no CropGrowth object
	if (_currentCrop.get() && _currentCrop->isValid())
	{
		//prepare to remove tips
		double currentLeafBiomass = _currentCropGrowth->get_OrganBiomass(1);
		double currentShootBiomass = _currentCropGrowth->get_OrganBiomass(2);
		double leavesToRemove = percentage * currentLeafBiomass;
		double shootsToRemove = percentage * currentShootBiomass;
		double leavesToRemain = (1.0 - percentage) * currentLeafBiomass;
		double shootsToRemain = (1.0 - percentage) * currentShootBiomass;
		_currentCropGrowth->set_OrganBiomass(1, leavesToRemain);
		_currentCropGrowth->set_OrganBiomass(2, shootsToRemain);

    if (!exported)
    {
			//prepare to add crop residues to soilorganic (AOMs)
			double tipResidues = leavesToRemove + shootsToRemove;
			double tipResidueNConcentration = _currentCropGrowth->get_ResiduesNConcentration();
      debug() << "adding organic matter from shoot and leaf residues to soilOrganic" << endl;
      debug() << "Shoot and leaf residue biomass: " << tipResidues
				<< " Tip residue N concentration: " << tipResidueNConcentration << endl;

      _soilOrganic->addOrganicMatter(_currentCrop->residueParameters(),
                                    tipResidues,
                                    tipResidueNConcentration);
		}
	}
}
/**
 * @brief Simulating plowing or incorporating of total crop.
 *
 * Deletes the current crop.
 */
void MonicaModel::incorporateCurrentCrop()
{
  //could be just a fallow, so there might be no CropGrowth object
  if(_currentCrop && _currentCrop->isValid())
  {
    //prepare to add root and crop residues to soilorganic (AOMs)
	double total_biomass = _currentCropGrowth->totalBiomass();
    double totalNContent = _currentCropGrowth->get_AbovegroundBiomassNContent() + _currentCropGrowth->get_RootNConcentration() * _currentCropGrowth->get_OrganBiomass(0);
	double totalNConcentration = totalNContent / total_biomass;

    debug() << "Adding organic matter from total biomass of crop to soilOrganic" << endl;
    debug() << "Total biomass: " << total_biomass << endl
        << " Total N concentration: " << totalNConcentration << endl;

    _soilOrganic->addOrganicMatter(_currentCrop->residueParameters(),
                                  total_biomass,
                                  totalNConcentration);
  }

	_clearCropUponNextDay = true;
}

/**
 * @brief Simulating cutting of crop.
 *
 * Only removes some biomass of crop but not harvesting the crop.
 */

void MonicaModel::cuttingCurrentCrop(double percentage, bool exported)
{
	//could be just a fallow, so there might be no CropGrowth object
	if (_currentCrop.get() && _currentCrop->isValid())
	{
		//prepare to remove tips
		double currentLeafBiomass = _currentCropGrowth->get_OrganBiomass(1);
		double currentShootBiomass = _currentCropGrowth->get_OrganBiomass(2);
		double currentFruitBiomass = _currentCropGrowth->get_OrganBiomass(3);
		double leavesToRemove = percentage * currentLeafBiomass;
		double shootsToRemove = percentage * currentShootBiomass;
		double fruitsToRemove = currentFruitBiomass;
		double leavesToRemain = (1.0 - percentage) * currentLeafBiomass;
		double shootsToRemain = (1.0 - percentage) * currentShootBiomass;
		int stageAfterCut = _currentCropGrowth->get_StageAfterCut();
		_currentCropGrowth->accumulatePrimaryCropYield(_currentCropGrowth->get_CropYieldAfterCutting());
		_currentCropGrowth->set_OrganBiomass(1, leavesToRemain);
		_currentCropGrowth->set_OrganBiomass(2, shootsToRemain);
		_currentCropGrowth->set_OrganBiomass(3, 0.0); // fruit is not present after cutting
		_currentCropGrowth->set_OrganBiomass(5, 0.0); // sugar is not present after cutting
		_currentCropGrowth->setLeafAreaIndex(_currentCropGrowth->get_OrganBiomass(1) * _currentCropGrowth->getSpecificLeafArea(_currentCropGrowth->get_DevelopmentalStage()));
		_currentCropGrowth->set_DevelopmentalStage(stageAfterCut); // sets developmentag stage according to crop database
		_currentCropGrowth->set_CuttingDelayDays(); // sets delay after cutting according to crop database
		_currentCropGrowth->set_MaxAssimilationRate(0.9); // Reduces maximum assimilation rate by 10%

    if (!exported)
    {
			//prepare to add crop residues to soilorganic (AOMs)
			double residues = leavesToRemove + shootsToRemove + fruitsToRemove;
			double residueNConcentration = _currentCropGrowth->get_AbovegroundBiomassNConcentration();
			debug() << "adding organic matter from cut residues to soilOrganic" << endl;
			debug() << "Residue biomass: " << residues
				<< " Residue N concentration: " << residueNConcentration << endl;

			_soilOrganic->addOrganicMatter(_currentCrop->residueParameters(),
                                    residues,
                                    residueNConcentration);
		}
	}
}

/**
 * @brief Applying of fertilizer.
 *
 * @todo Nothing implemented yet.
 */
void MonicaModel::applyMineralFertiliser(MineralFertiliserParameters partition,
                                         double amount)
{
  if(!_simPs.p_UseNMinMineralFertilisingMethod)
  {
		_soilColumn->applyMineralFertiliser(partition, amount);
		addDailySumFertiliser(amount);
	}
}

void MonicaModel::applyOrganicFertiliser(const OrganicMatterParametersPtr params,
                                         double amountFM,
                                         bool incorporation)
{
	if(params)
	{
		debug() << "MONICA model: applyOrganicFertiliser:\t" << amountFM << "\t" << params->vo_NConcentration << endl;
		_soilOrganic->setIncorporation(incorporation);
		_soilOrganic->addOrganicMatter(params, amountFM, params->vo_NConcentration);
		addDailySumOrgFertiliser(amountFM, params);
		addDailySumOrganicFertilizerDM(amountFM * params->vo_AOM_DryMatterContent);
	}
}

double MonicaModel::applyMineralFertiliserViaNMinMethod(MineralFertiliserParameters partition,
                                                        NMinCropParameters cps)
{
  const NMinUserParameters& ups = _simPs.p_NMinUserParams;
  return _soilColumn->applyMineralFertiliserViaNMinMethod(partition,
                                                         cps.samplingDepth,
                                                         cps.nTarget,
                                                         cps.nTarget30,
                                                         ups.min,
                                                         ups.max,
                                                         ups.delayInDays);
}

void MonicaModel::dailyReset()
{
	_dailySumIrrigationWater = 0.0;
	_dailySumFertiliser = 0.0;
	_dailySumOrgFertiliser = 0.0;
	_dailySumOrganicFertilizerDM = 0.0;
	_optCarbonExportedResidues = 0.0;
	_optCarbonReturnedResidues = 0.0;
	clearEvents();

	if(_clearCropUponNextDay)
	{
		delete _currentCropGrowth;
		_currentCropGrowth = nullptr;
		_currentCrop.reset();
		_soilTransport->remove_Crop();
		_soilColumn->remove_Crop();
		_soilMoisture->remove_Crop();
		_soilOrganic->remove_Crop();

		_clearCropUponNextDay = false;
	}
}

void MonicaModel::applyIrrigation(double amount, double nitrateConcentration,
                                  double /*sulfateConcentration*/)
{
  //if the production process has still some defined manual irrigation dates
  if(!_simPs.p_UseAutomaticIrrigation)
  {
    _soilOrganic->addIrrigationWater(amount);
    _soilColumn->applyIrrigation(amount, nitrateConcentration);
		addDailySumIrrigationWater(amount);
  }
}

/**
 * Applies tillage for a given soil depth. Tillage means in MONICA,
 * that for all effected soil layer the parameters are averaged.
 * @param depth Depth in meters
 */
void MonicaModel::applyTillage(double depth)
{
	_soilColumn->applyTillage(depth);
}

void MonicaModel::step()
{
	if(isCropPlanted() && !_clearCropUponNextDay)
		cropStep();

	generalStep();
}

/**
 * @brief Simulating the soil processes for one time step.
 * @param stepNo Number of current processed step
 */
void MonicaModel::generalStep()
{
	auto date = _currentStepDate;
	unsigned int julday = date.julianDay();
	bool leapYear = date.isLeapYear();

	auto climateData = currentStepClimateData();
	double tmin = climateData[Climate::tmin];
	double tavg = climateData[Climate::tavg];
	double tmax = climateData[Climate::tmax];
	double precip = climateData[Climate::precip];
	double wind = climateData[Climate::wind];
	double globrad = climateData[Climate::globrad];	
	
	// test if data for relhumid are available; if not, value is set to -1.0
	double relhumid = climateData.find(Climate::relhumid) == climateData.end()
		? -1.0
		: climateData[Climate::relhumid];


  // test if simulated gw or measured values should be used
  double gw_value = _groundwaterInformation.getGroundwaterInformation(date);
  //  cout << "vs_GroundwaterDepth:\t" << _envPs.p_MinGroundwaterDepth << "\t" << _envPs.p_MaxGroundwaterDepth << endl;
  vs_GroundwaterDepth = gw_value < 0
                        ? GroundwaterDepthForDate(_envPs.p_MaxGroundwaterDepth,
                                                  _envPs.p_MinGroundwaterDepth,
                                                  _envPs.p_MinGroundwaterDepthMonth,
                                                  julday,
                                                  leapYear)
                        : gw_value / 100.0; // [cm] --> [m]

	// first try to get CO2 concentration from climate data
	auto co2it = climateData.find(Climate::co2);
	if(co2it != climateData.end())
	{
		vw_AtmosphericCO2Concentration = co2it->second;
	}
	else 
	{
		// try to get yearly values from UserEnvironmentParameters
		auto co2sit = _envPs.p_AtmosphericCO2s.find(date.year());
		if(co2sit != _envPs.p_AtmosphericCO2s.end())
			vw_AtmosphericCO2Concentration = co2sit->second;
		// potentially use MONICA algorithm to calculate CO2 concentration
		else if(int(_envPs.p_AtmosphericCO2) <= 0)
			vw_AtmosphericCO2Concentration = CO2ForDate(date);
		// if everything fails value in UserEnvironmentParameters for the whole simulation
		else 
			vw_AtmosphericCO2Concentration = _envPs.p_AtmosphericCO2;
	}

  //  debug << "step: " << stepNo << " p: " << precip << " gr: " << globrad << endl;

  _soilColumn->deleteAOMPool();

	auto possibleDelayedFertilizerAmount = _soilColumn->applyPossibleDelayedFerilizer();
	addDailySumFertiliser(possibleDelayedFertilizerAmount);
  double possibleTopDressingAmount = _soilColumn->applyPossibleTopDressing();
  addDailySumFertiliser(possibleTopDressingAmount);

  if(_currentCrop 
		 && _currentCrop->isValid() 
		 && _simPs.p_UseNMinMineralFertilisingMethod 
		 && _currentCrop->isWinterCrop()
		 && julday == _simPs.p_JulianDayAutomaticFertilising)
  {
		_soilColumn->clearTopDressingParams();
    debug() << "nMin fertilising winter crop" << endl;
    auto cps = _currentCrop->cropParameters();
		double fertilizerAmount = applyMineralFertiliserViaNMinMethod
		(_simPs.p_NMinFertiliserPartition,
		 NMinCropParameters(cps->speciesParams.pc_SamplingDepth,
												cps->speciesParams.pc_TargetNSamplingDepth,
												cps->speciesParams.pc_TargetN30));
    addDailySumFertiliser(fertilizerAmount);
	}

  _soilTemperature->step(tmin, tmax, globrad);

  // first try to get ReferenceEvapotranspiration from climate data
  auto et0_it = climateData.find(Climate::et0);
  double et0 = et0_it == climateData.end() ? -1.0 : et0_it->second;  

  _soilMoisture->step(vs_GroundwaterDepth, precip, tmax, tmin,
	  (relhumid / 100.0), tavg, wind, _envPs.p_WindSpeedHeight, globrad,
	  julday, et0);
  
	_soilOrganic->step(tavg, precip, wind);
	_soilTransport->step();
}

pair<double, double> laiSunShade(double latitude, int doy, int hour, double lai)
{
	//taken from Astronomy.cs Astronomy::SolarDeclination and Astronomy::SolarElevation
	double PI = 3.1415926536;
	double solarDeclination = -0.4093*cos(2*PI*(doy + 10)/365);
	double dA = sin(solarDeclination)*sin(latitude);
	double dB = cos(solarDeclination)*cos(latitude);
	double dHa = PI*(hour - 12)/12;
	double phi = asin(dA + dB*cos(dHa));

	//taken from Norman, J.M., 1982. Simulation of microclimates. In:
	//Hatfield, J.L., Thomason, I.J. (Eds.), Biometeorology in
	//Integrated Pest Management.Academic Press, New York,
	//pp. 65–99. Equation (14)
	double laiSun = 2*cos(phi)*(1 - exp(-0.5*lai/cos(phi)));
	
	return make_pair(laiSun, lai - laiSun);
}

void MonicaModel::cropStep()
{
	auto date = _currentStepDate;
	auto climateData = currentStepClimateData();
  // do nothing if there is no crop
  if(!_currentCropGrowth)
    return;

  p_daysWithCrop++;

  unsigned int julday = date.julianDay();

  double tavg = climateData[Climate::tavg];
  double tmax = climateData[Climate::tmax];
  double tmin = climateData[Climate::tmin];
  double globrad = climateData[Climate::globrad];

	// first try to get CO2 concentration from climate data
	auto o3it = climateData.find(Climate::o3);
	if(o3it != climateData.end())
	{
		vw_AtmosphericO3Concentration = o3it->second;
	}
	else
	{
		// try to get yearly values from UserEnvironmentParameters
		auto o3sit = _envPs.p_AtmosphericO3s.find(date.year());
		if(o3sit != _envPs.p_AtmosphericO3s.end())
			vw_AtmosphericO3Concentration = o3sit->second;
		// if everything fails value in UserEnvironmentParameters for the whole simulation
		else
			vw_AtmosphericO3Concentration = _envPs.p_AtmosphericO3;
	}

  // test if data for sunhours are available; if not, value is set to -1.0
	auto sunhit = climateData.find(Climate::sunhours);
	double sunhours = sunhit == climateData.end() ? -1.0 : sunhit->second;

  // test if data for relhumid are available; if not, value is set to -1.0
	auto rhit = climateData.find(Climate::relhumid);
	double relhumid = rhit == climateData.end() ? -1.0 : rhit->second;

	auto wind_it = climateData.find(Climate::wind);
	double wind = wind_it == climateData.end() ? -1.0 : wind_it->second;

	double precip =  climateData[Climate::precip];
	
	// check if reference evapotranspiration was provided via climate files
	auto et0_it = climateData.find(Climate::et0);
	double et0 = et0_it == climateData.end() ? -1.0 : et0_it->second;

  double vw_WindSpeedHeight = _envPs.p_WindSpeedHeight;

  _currentCropGrowth->step(tavg, tmax, tmin, globrad, sunhours, date,
                           (relhumid / 100.0), wind, vw_WindSpeedHeight,
                           vw_AtmosphericCO2Concentration, vw_AtmosphericO3Concentration, precip, et0);
  if(_simPs.p_UseAutomaticIrrigation)
  {
    const AutomaticIrrigationParameters& aips = _simPs.p_AutoIrrigationParams;
    if(_soilColumn->applyIrrigationViaTrigger(aips.threshold, aips.amount,
                                             aips.nitrateConcentration))
    {
      _soilOrganic->addIrrigationWater(aips.amount);
      addDailySumIrrigationWater(aips.amount);
    }
  }

  p_accuNStress += _currentCropGrowth->get_CropNRedux();
  p_accuWaterStress += _currentCropGrowth->get_TranspirationDeficit();
  p_accuHeatStress += _currentCropGrowth->get_HeatStressRedux();
  p_accuOxygenStress += _currentCropGrowth->get_OxygenDeficit();

	/*
	//prepare VOC calculations
	//TODO: right now assumes that we have daily values and thus xxx24 is the same as xxx
	double globradWm2 = globrad * 1000000.0 / 86400.0; //MJ/m2/d (a sum) -> W/m2
	if(_index240 < _stepSize240 - 1)
	{
		_index240++;
	}
	else
	{
		_index240 = 0;
		_full240 = true;
	}
	_rad240[_index240] = globradWm2;
	_tfol240[_index240] = tavg;
	
	Voc::MicroClimateData mcd;
	//hourly or time step average global radiation (in case of monica usually 24h)
	mcd.rad = globradWm2;
	mcd.rad24 = mcd.rad; //just daily values, thus average over 24h is the same
	mcd.rad240 = accumulate(_rad240.begin(), _rad240.end(), 0.0) / (_full240 ? _rad240.size() : _index240 + 1);
	mcd.tFol = tavg;
	mcd.tFol24 = mcd.tFol;
	mcd.tFol240 = accumulate(_tfol240.begin(), _tfol240.end(), 0.0) / (_full240 ? _tfol240.size() : _index240 + 1);

	double lai = _currentCropGrowth->get_LeafAreaIndex();
	auto sunShadeLaiAtZenith = laiSunShade(_sitePs.vs_Latitude, julday, 12, lai);
	mcd.sunlitfoliagefraction = sunShadeLaiAtZenith.first / lai;
	mcd.sunlitfoliagefraction24 = mcd.sunlitfoliagefraction;
	
	//debug() << "calculating voc emissions at " << date.toIsoDateString() << endl;
	_currentCropGrowth->calculateVOCEmissions(mcd);
	*/
}

/**
* @brief Returns atmospheric CO2 concentration for date [ppm]
*
* @param year, julianDay, leapYear
*
* @return
*/
double MonicaModel::CO2ForDate(double year, double julianDay, bool leapYear)
{
  double decimalDate = year + julianDay/(leapYear ? 366.0 : 365);

  //Atmospheric CO2 concentration according to RCP 8.5
  return 222.0 + exp(0.01467*(decimalDate - 1650.0)) + 2.5*sin((decimalDate - 0.5)/0.1592);
}

double MonicaModel::CO2ForDate(Date d)
{
	return CO2ForDate(d.year(), d.julianDay(), d.isLeapYear());
}


/**
* @brief Returns groundwater table for date [m]
*
* @param pm_MaxGroundwaterTable; pm_MinGroundwaterTable; pm_MaxGroundwaterTableMonth
*
* @return
*/
double MonicaModel::GroundwaterDepthForDate(double maxGroundwaterDepth,
                                            double minGroundwaterDepth,
                                            int minGroundwaterDepthMonth,
                                            double julianday,
                                            bool leapYear)
{
  double days = 365;
  if(leapYear)
    days = 366.0;

  double meanGroundwaterDepth = (maxGroundwaterDepth + minGroundwaterDepth) / 2.0;
  double groundwaterAmplitude = (maxGroundwaterDepth - minGroundwaterDepth) / 2.0;

  double sinus = sin(((julianday/days*360.0) - 90.0 -
                      ((double(minGroundwaterDepthMonth)*30.0) - 15.0)) *
                     3.14159265358979/180.0);

  double groundwaterDepth = meanGroundwaterDepth + (sinus*groundwaterAmplitude);

  if (groundwaterDepth < 0.0)
    groundwaterDepth = 20.0;

  return groundwaterDepth;
}

//----------------------------------------------------------------------------

/**
 * @brief Returns mean soil organic C.
 * @param depth_m
 */
//Kohlenstoffgehalt 0-depth [% kg C / kg Boden]
double MonicaModel::avgCorg(double depth_m) const
{
  double lsum = 0, sum = 0;
  int count = 0;

  for(int i = 0, nols = _simPs.p_NumberOfLayers; i < nols; i++)
  {
    count++;
    sum +=_soilColumn->at(i).vs_SoilOrganicCarbon(); //[kg C / kg Boden]
    lsum += _soilColumn->at(i).vs_LayerThickness;
    if(lsum >= depth_m)
      break;
  }

  return sum / double(count) * 100.0;
}



/**
 * @brief Returns the soil moisture up to 90 cm depth
 * @return water content
 */
//Bodenwassergehalt 0-90cm [%nFK]
double MonicaModel::mean90cmWaterContent() const
{
  return _soilMoisture->meanWaterContent(0.9);
}

double MonicaModel::meanWaterContent(int layer, int number_of_layers) const
{
  return _soilMoisture->meanWaterContent(layer, number_of_layers);
}

/**
 * @brief Returns the N content up to given depth.
 *
 *@return N content
 *
 */
//Boden-Nmin-Gehalt 0-90cm am 31.03. [kg N/ha]
double MonicaModel::sumNmin(double depth_m) const
{
  double lsum = 0, sum = 0;
  int count = 0;

  for(int i = 0, nols = _simPs.p_NumberOfLayers; i < nols; i++)
  {
    count++;
    sum += _soilColumn->at(i).get_SoilNmin(); //[kg N m-3]
    lsum += _soilColumn->at(i).vs_LayerThickness;
    if(lsum >= depth_m)
      break;
  }

  return sum / double(count) * lsum * 10000;
}

/**
 * Returns accumulation of soil nitrate for 90cm soil at 31.03.
 * @param depth Depth of soil
 * @return Accumulated nitrate
 */
double
MonicaModel::sumNO3AtDay(double depth_m) const
{
  double lsum = 0, sum = 0;
  int count = 0;

  for(int i = 0, nols = _simPs.p_NumberOfLayers; i < nols; i++)
  {
    count++;
    sum += _soilColumn->at(i).get_SoilNO3(); //[kg m-3]
    lsum += _soilColumn->at(i).vs_LayerThickness;
    if(lsum >= depth_m)
      break;
  }

  return sum;
}

//Grundwasserneubildung[mm Wasser]
double MonicaModel::groundWaterRecharge() const
{
  return _soilMoisture->get_GroundwaterRecharge();
}

//N-Auswaschung[kg N/ha]
double MonicaModel::nLeaching() const {
  return _soilTransport->get_NLeaching();//[kg N ha-1]
}

/**
 * Returns sum of soiltemperature in given number of soil layers
 * @param layers Number of layers that should be added.
 * @return Soil temperature sum [°C]
 */
double MonicaModel::sumSoilTemperature(int layers) const
{
  return _soilColumn->sumSoilTemperature(layers);
}

/**
 * Returns maximal snow depth during simulation
 * @return
 */
double
MonicaModel::maxSnowDepth() const
{
  return _soilMoisture->getMaxSnowDepth();
}

/**
 * Returns sum of all snowdepth during whole simulation
 * @return
 */
double MonicaModel::getAccumulatedSnowDepth() const
{
  return _soilMoisture->getAccumulatedSnowDepth();
}

/**
 * Returns sum of frost depth during whole simulation.
 * @return Accumulated frost depth
 */
double MonicaModel::getAccumulatedFrostDepth() const
{
  return _soilMoisture->getAccumulatedFrostDepth();
}

/**
 * Returns average soil temperature of first 30cm soil.
 * @return Average soil temperature of organic layers.
 */
double MonicaModel::avg30cmSoilTemperature() const
{
  double nols = 3;
  double acc = 0.0;
  for (int l = 0; l < nols; l++)
    acc += _soilColumn->at(l).get_Vs_SoilTemperature();

  return acc / nols;
}

/**
 * Returns average soil moisture concentration in soil in a defined layer.
 * Layer is specified by start end end of soil layer.
 *
 * @param start_layer
 * @param end_layer
 * @return Average soil moisture concentation
 */
double MonicaModel::avgSoilMoisture(size_t startLayer, 
																		size_t endLayerInclusive) const
{
	size_t nols = max<size_t>(0, min<size_t>(endLayerInclusive - startLayer + 1, _soilColumn->size()));
	return accumulate(_soilColumn->begin(), _soilColumn->begin() + nols, 0.0,
										[](double acc, const SoilLayer& sl)
	{
		return sl.get_Vs_SoilMoisture_m3();
	}) / nols;
}

/**
 * Returns mean of capillary rise in a set of layers.
 * @param start_layer First layer to be included
 * @param end_layer Last layer, is not included;
 * @return Average capillary rise [mm]
 */
double MonicaModel::avgCapillaryRise(int start_layer, int end_layer) const
{
  int num=0;
  double accu = 0.0;
  for (int i=start_layer; i<end_layer; i++)
  {
    accu+=_soilMoisture->get_CapillaryRise(i);
    num++;
  }
  return accu/num;
}

/**
 * @brief Returns mean percolation rate
 * @param start_layer
 * @param end_layer
 * @return Mean percolation rate [mm]
 */
double MonicaModel::avgPercolationRate(int start_layer, int end_layer) const
{
  int num=0;
  double accu = 0.0;
  for (int i=start_layer; i<end_layer; i++)
  {
    accu+=_soilMoisture->get_PercolationRate(i);
    num++;
  }
  return accu/num;
}

/**
 * Returns sum of all surface run offs at this point in simulation time.
 * @return Sum of surface run off in [mm]
 */
double MonicaModel::sumSurfaceRunOff() const
{
  return _soilMoisture->get_SumSurfaceRunOff();
}

/**
 * Returns surface runoff of current day [mm].
 */
double MonicaModel::surfaceRunoff() const
{
  return _soilMoisture->get_SurfaceRunOff();
}

/**
 * Returns remaining evapotranspiration [mm]
 * @return
 */
double MonicaModel::getEvapotranspiration() const
{
  if (_currentCropGrowth)
    return _currentCropGrowth->get_RemainingEvapotranspiration();

  return 0.0;
}

/**
 * Returns actual transpiration
 * @return
 */
double MonicaModel::getTranspiration() const
{
	if(_currentCropGrowth)
		return _currentCropGrowth->get_ActualTranspiration();

  return 0.0;
}

/**
 * Returns actual evaporation
 * @return
 */
double MonicaModel::getEvaporation() const
{
  if (_currentCropGrowth)
    return _currentCropGrowth->get_EvaporatedFromIntercept();

  return 0.0;
}

double MonicaModel::getETa() const
{
  return _soilMoisture->get_ActualEvapotranspiration();
}

/**
 * Returns sum of evolution rate in first three layers.
 * @return
 */
double MonicaModel::get_sum30cmSMB_CO2EvolutionRate() const
{
  double sum = 0.0;
  for (int layer=0; layer<3; layer++) {
    sum+=_soilOrganic->get_SMB_CO2EvolutionRate(layer);
  }

  return sum;
}

/**
 * Returns volatilised NH3
 * @return
 */
double MonicaModel::getNH3Volatilised() const
{
  return _soilOrganic->get_NH3_Volatilised();
}

/**
 * Returns accumulated sum of all volatilised NH3 in simulation time.
 * @return
 */
double MonicaModel::getSumNH3Volatilised() const
{
//  cout << "NH4Vol: " << _soilOrganic.get_sumNH3_Volatilised() << endl;
  return _soilOrganic->get_SumNH3_Volatilised();
}

/**
 * Returns sum of denitrification rate in first 30cm soil.
 * @return Denitrification rate [kg N m-3 d-1]
 */
double MonicaModel::getsum30cmActDenitrificationRate() const
{
  double sum=0.0;
  for (int layer=0; layer<3; layer++) {
//    cout << "DENIT: " << _soilOrganic.get_ActDenitrificationRate(layer) << endl;
    sum+=_soilOrganic->get_ActDenitrificationRate(layer);
  }

  return sum;
}

void MonicaModel::clearEvents() 
{ 
	_previousDaysEvents = _currentEvents;
	_currentEvents.clear(); 
}