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

/**
 * @file soilorganic.cpp
 */

#include "soilorganic.h"

#include <algorithm>
#include <cmath>
#include <tuple>

#include "soilcolumn.h"
#include "monica-model.h"
#include "crop-module.h"
#include "tools/debug.h"
#include "soil/constants.h"
#include "tools/algorithms.h"
#include "stics-nit-denit-n2o.h"

using namespace std;
using namespace Monica;
using namespace Tools;
using namespace Soil;

/**
 * @brief Constructor
 * @param sc Soil column
 * @param gps General parameters
 * @param ocs Organic constants
 * @param stps Site parameters
 * @param org_fert Parameter for organic fertiliser
 */
SoilOrganic::SoilOrganic(SoilColumn& sc, const SoilOrganicModuleParameters& userParams)
  : soilColumn(sc),
  _params(userParams),
  vs_NumberOfLayers(sc.vs_NumberOfLayers()),
  vs_NumberOfOrganicLayers(sc.vs_NumberOfOrganicLayers()),
  vo_ActAmmoniaOxidationRate(sc.vs_NumberOfOrganicLayers()),
  vo_ActNitrificationRate(sc.vs_NumberOfOrganicLayers()),
  vo_ActDenitrificationRate(sc.vs_NumberOfOrganicLayers()),
  vo_AOM_FastDeltaSum(sc.vs_NumberOfOrganicLayers()),
  vo_AOM_FastInput(sc.vs_NumberOfOrganicLayers()),
  vo_AOM_FastSum(sc.vs_NumberOfOrganicLayers()),
  vo_AOM_SlowDeltaSum(sc.vs_NumberOfOrganicLayers()),
  vo_AOM_SlowInput(sc.vs_NumberOfOrganicLayers()),
  vo_AOM_SlowSum(sc.vs_NumberOfOrganicLayers()),
  vo_CBalance(sc.vs_NumberOfOrganicLayers()),
  vo_InertSoilOrganicC(sc.vs_NumberOfOrganicLayers()),
  vo_NetNMineralisationRate(sc.vs_NumberOfOrganicLayers()),
  vo_SMB_CO2EvolutionRate(sc.vs_NumberOfOrganicLayers()),
  vo_SMB_FastDelta(sc.vs_NumberOfOrganicLayers()),
  vo_SMB_SlowDelta(sc.vs_NumberOfOrganicLayers()),
  vo_SoilOrganicC(sc.vs_NumberOfOrganicLayers()),
  vo_SOM_FastInput(sc.vs_NumberOfOrganicLayers()),
  vo_SOM_FastDelta(sc.vs_NumberOfOrganicLayers()),
  vo_SOM_SlowDelta(sc.vs_NumberOfOrganicLayers()) {
  // Subroutine Pool initialisation
  double po_SOM_SlowUtilizationEfficiency = _params.po_SOM_SlowUtilizationEfficiency;
  double po_PartSOM_to_SMB_Slow = _params.po_PartSOM_to_SMB_Slow;
  double po_SOM_FastUtilizationEfficiency = _params.po_SOM_FastUtilizationEfficiency;
  double po_PartSOM_to_SMB_Fast = _params.po_PartSOM_to_SMB_Fast;
  double po_SOM_SlowDecCoeffStandard = _params.po_SOM_SlowDecCoeffStandard;
  double po_SOM_FastDecCoeffStandard = _params.po_SOM_FastDecCoeffStandard;
  double po_PartSOM_Fast_to_SOM_Slow = _params.po_PartSOM_Fast_to_SOM_Slow;

  //Conversion of soil organic carbon weight fraction to volume unit
  for (size_t i = 0; i < vs_NumberOfOrganicLayers; i++) {
    SoilLayer& layer = soilColumn[i];

    vo_SoilOrganicC[i] = layer.vs_SoilOrganicCarbon() * layer.vs_SoilBulkDensity(); //[kg C kg-1] * [kg m-3] --> [kg C m-3]

    // Falloon et al. (1998): Estimating the size of the inert organic matter pool
    // from total soil oragnic carbon content for use in the Rothamsted Carbon model.
    // Soil Biol. Biochem. 30 (8/9), 1207-1211. for values in t C ha-1.
    // vo_InertSoilOrganicC is calculated back to [kg C m-3].
    vo_InertSoilOrganicC[i] = (0.049 * pow((vo_SoilOrganicC[i] // [kg C m-3]
                                                  * layer.vs_LayerThickness // [kg C m-2]
                                                  / 1000 * 10000.0), 1.139)) // [t C ha-1]
      / 10000.0 * 1000.0 // [kg C m-2]
      / layer.vs_LayerThickness; // [kg C m-3]

    vo_SoilOrganicC[i] -= vo_InertSoilOrganicC[i]; // [kg C m-3]

    // Initialisation of pool SMB_Slow [kg C m-3]
    layer.vs_SMB_Slow = po_SOM_SlowUtilizationEfficiency
      * po_PartSOM_to_SMB_Slow * vo_SoilOrganicC[i];

    // Initialisation of pool SMB_Fast [kg C m-3]
    layer.vs_SMB_Fast = po_SOM_FastUtilizationEfficiency
      * po_PartSOM_to_SMB_Fast * vo_SoilOrganicC[i];

    // Initialisation of pool SOM_Slow [kg C m-3]
    layer.vs_SOM_Slow = vo_SoilOrganicC[i] / (1.0 + po_SOM_SlowDecCoeffStandard
                                                                  / (po_SOM_FastDecCoeffStandard * po_PartSOM_Fast_to_SOM_Slow));

    // Initialisation of pool SOM_Fast [kg C m-3]
    layer.vs_SOM_Fast = vo_SoilOrganicC[i] - layer.vs_SOM_Slow;

    // Soil Organic Matter pool update [kg C m-3]
    vo_SoilOrganicC[i] -= layer.vs_SMB_Slow + layer.vs_SMB_Fast;

    layer.set_SoilOrganicCarbon
    ((vo_SoilOrganicC[i] + vo_InertSoilOrganicC[i])
     / layer.vs_SoilBulkDensity()); // [kg C m-3] / [kg m-3] --> [kg C kg-1]

  //this is not needed as either one of organic carbon or organic matter is only be used
//layer.set_SoilOrganicMatter
//	((vo_SoilOrganicC[i] + vo_InertSoilOrganicC[i]) 
//	 / OrganicConstants::po_SOM_to_C
//	 / layer.vs_SoilBulkDensity());  // [kg C m-3] / [kg m-3] --> [kg C kg-1]

    vo_ActDenitrificationRate.at(i) = 0.0;
  } // for
}

void SoilOrganic::deserialize(mas::models::monica::SoilOrganicModuleState::Reader reader) {

}

void SoilOrganic::serialize(mas::models::monica::SoilOrganicModuleState::Builder builder) const {
  builder.initModuleParams();
  _params.serialize(builder.getModuleParams());
}

/**
 * @brief Calculation step for one time step.
 * @param vw_MeanAirTemperature
 * @param vw_Precipitation
 * @param vw_WindSpeed
 */
void SoilOrganic::step(double vw_MeanAirTemperature, double vw_Precipitation,
                       double vw_WindSpeed) {

  double vc_NetPrimaryProduction = 0.0;
  vc_NetPrimaryProduction = crop ? crop->get_NetPrimaryProduction() : 0;

  //cout << "get_OrganBiomass(organ) : " << organ << ", " << organ_percentage << std::endl; // JV!
  //cout << "total_biomass : " << total_biomass << std::endl; // JV!

  //fo_OM_Input(vo_AOM_Addition);
  fo_Urea(vw_Precipitation + irrigationAmount);
  // Mineralisation Immobilisitation Turn-Over
  fo_MIT();
  fo_Volatilisation(addedOrganicMatter, vw_MeanAirTemperature, vw_WindSpeed);
  
  if (_params.sticsParams.use_nit) fo_stics_Nitrification();
  else fo_Nitrification();
  
  if (_params.sticsParams.use_denit) fo_stics_Denitrification();
  else fo_Denitrification();
  
  auto N2OProducedNitDenit = _params.sticsParams.use_n2o
    ? fo_stics_N2OProduction()
    : make_pair(fo_N2OProduction(), 0.0);
  vo_N2O_Produced_Nit = N2OProducedNitDenit.first;
  vo_N2O_Produced_Denit = N2OProducedNitDenit.second;
  vo_N2O_Produced = vo_N2O_Produced_Nit + vo_N2O_Produced_Denit;

  fo_PoolUpdate();

  vo_NetEcosystemProduction =
    fo_NetEcosystemProduction(vc_NetPrimaryProduction, vo_DecomposerRespiration);
  vo_NetEcosystemExchange =
    fo_NetEcosystemExchange(vc_NetPrimaryProduction, vo_DecomposerRespiration);

  vo_SumNH3_Volatilised += vo_NH3_Volatilised;

  vo_SumN2O_Produced += vo_N2O_Produced;

  //clear everything for next step
  //thus in order apply irrigation water or fertiliser, this has to be
  //done before the stepping method
  irrigationAmount = 0.0;

  auto nools = soilColumn.vs_NumberOfOrganicLayers();
  for (size_t i = 0; i < nools; i++) {
    vo_AOM_SlowInput[i] = 0.0;
    vo_AOM_FastInput[i] = 0.0;
    vo_SOM_FastInput[i] = 0.0;
  }
  addedOrganicMatter = false;
}

void SoilOrganic::addOrganicMatter(OrganicMatterParametersPtr params,
																	 map<size_t, double> layer2addedOrganicMatterAmount,
																	 double addedOrganicMatterNConcentration)
{
	debug() << "SoilOrganic: addOrganicMatter: " << params->toString() << endl;

	auto nools = soilColumn.vs_NumberOfOrganicLayers();
	double layerThickness = soilColumn.at(0).vs_LayerThickness;

	// check if the added organic matter is from crop residues
	bool areCropResidueParams = int(params->vo_CN_Ratio_AOM_Fast * 10000.0) == 0;
	bool areOrganicFertilizerParams = !areCropResidueParams;

	// calculate the CN AOM fast ratio and added organic carbon amount from the added organic matter and its N concentration
	// used if we're talking about crop residues (where C/N is 0 in configuration) and not organic fertilizer
	auto calc_CN_Ratio_AOM_Fast_and_added_Corg_amount =
		[&params, layerThickness, this](double vo_AddedOrganicMatterAmount,
																		double vo_AddedOrganicMatterNConcentration)
	{
		double CN_ratio_AOM_fast = 0;

		double added_Corg_amount =
			vo_AddedOrganicMatterAmount
			* params->vo_AOM_DryMatterContent
			* OrganicConstants::po_AOM_to_C
			/ 10000.0
			/ layerThickness;

		// Converting AOM N content from kg N kg DM-1 to kg N m-3
		double added_Norg_amount = vo_AddedOrganicMatterNConcentration <= 0.0 
			? 0.01
			: (vo_AddedOrganicMatterAmount
				 * params->vo_AOM_DryMatterContent
				 * vo_AddedOrganicMatterNConcentration
				 / 10000.0
				 / layerThickness);

		double added_CN_ratio = added_Corg_amount / added_Norg_amount;

		debug() << "Added organic matter N amount: " << added_Norg_amount << endl;

		double N_for_AOM_slow = added_Corg_amount * params->vo_PartAOM_to_AOM_Slow / params->vo_CN_Ratio_AOM_Slow;

		// Assigning the dynamic C/N ratio to the AOM_Fast pool
		if(N_for_AOM_slow < added_Norg_amount)
		{
			double N_for_AOM_fast = added_Norg_amount - N_for_AOM_slow;
			CN_ratio_AOM_fast = added_Corg_amount * params->vo_PartAOM_to_AOM_Fast / N_for_AOM_fast;
		}
		else
			CN_ratio_AOM_fast = _params.po_AOM_FastMaxC_to_N;

		CN_ratio_AOM_fast = min(CN_ratio_AOM_fast, _params.po_AOM_FastMaxC_to_N);

		return make_tuple(CN_ratio_AOM_fast, added_Corg_amount, added_Norg_amount);
	};

	int rounded_AOM_SlowDecCoeffStandard = roundShiftedInt(params->vo_AOM_SlowDecCoeffStandard, 4);
	int rounded_AOM_FastDecCoeffStandard = roundShiftedInt(params->vo_AOM_FastDecCoeffStandard, 4);
	int rounded_PartAOM_Slow_to_SMB_Slow = roundShiftedInt(params->vo_PartAOM_Slow_to_SMB_Slow, 4);
	int rounded_PartAOM_Slow_to_SMB_Fast = roundShiftedInt(params->vo_PartAOM_Slow_to_SMB_Fast, 4);
	int rounded_CN_Ratio_AOM_Slow = roundShiftedInt(params->vo_CN_Ratio_AOM_Slow, 4);

	// compare the organic parameters against the give pool, to see if the parameters match
	auto isSamePoolAsParams = [=](const AOM_Properties& pool)//, double addedOrganicMatterAmount)
	{
		//double CN_ratio_AOM_fast = params->CN_ratio_AOM_fast;
		//if(areCropResidueParams)
		//	CN_ratio_AOM_fast = calc_CN_Ratio_AOM_Fast_and_added_Corg_amount(
		//		addedOrganicMatterAmount, addedOrganicMatterNConcentration).first;

		return
			roundShiftedInt(pool.vo_AOM_SlowDecCoeffStandard, 4) == rounded_AOM_SlowDecCoeffStandard &&
			roundShiftedInt(pool.vo_AOM_FastDecCoeffStandard, 4) == rounded_AOM_FastDecCoeffStandard &&
			roundShiftedInt(pool.vo_PartAOM_Slow_to_SMB_Slow, 4) == rounded_PartAOM_Slow_to_SMB_Slow &&
			roundShiftedInt(pool.vo_PartAOM_Slow_to_SMB_Fast, 4) == rounded_PartAOM_Slow_to_SMB_Fast &&
			roundShiftedInt(pool.vo_CN_Ratio_AOM_Slow, 4) == rounded_CN_Ratio_AOM_Slow; //&&
			//roundShiftedInt(pool.CN_ratio_AOM_fast, 0) == roundShiftedInt(CN_ratio_AOM_fast, 0);
	};

	//urea
	if(nools > 0)
	{
		for(const auto& p : layer2addedOrganicMatterAmount)
		{
			if(p.first < nools)
			{
				// kg N m-3 soil
				soilColumn.at(p.first).vs_SoilCarbamid +=
					p.second
					* params->vo_AOM_DryMatterContent
					* params->vo_AOM_CarbamidContent
					/ 10000.0
					/ layerThickness;
			}
		}
	}

	int poolSetIndex = -1;
	if(areCropResidueParams)
	{
		int i = 0;
		// find the index of an existing matching set of pools
		for(const auto& pool : soilColumn.at(0).vo_AOM_Pool)
		{
			if(isSamePoolAsParams(pool))
			{
				poolSetIndex = i;
				break;
			}
			i++;
		}
	}
	
	for(const auto& p : layer2addedOrganicMatterAmount)
	{
		auto intoLayerIndex = p.first;
		double addedOrganicMatterAmount = p.second;
    auto& intoLayer = soilColumn.at(intoLayerIndex);

		// calculate the CN ratio for AOM fast, if we're talking about crop residues and the
	  // equivalent added organic carbon amount for the given added organic matter amount
		double calced_CN_Ratio_AOM_Fast = 0, added_Corg_amount = 0, added_Norg_amount = 0;
		std::tie(calced_CN_Ratio_AOM_Fast, added_Corg_amount, added_Norg_amount) =
			calc_CN_Ratio_AOM_Fast_and_added_Corg_amount(addedOrganicMatterAmount,
																									 addedOrganicMatterNConcentration);

		double AOM_slow_input = 0, AOM_fast_input = 0;

		//if haven't found an existing pool with the same properties as params (or the params are from organic fertilizer)
		//create a new one, appended to the existing lists
		if(poolSetIndex < 0)
		{
			AOM_Properties pool;
			pool.vo_AOM_SlowDecCoeffStandard = params->vo_AOM_SlowDecCoeffStandard;
			pool.vo_AOM_FastDecCoeffStandard = params->vo_AOM_FastDecCoeffStandard;
			pool.vo_CN_Ratio_AOM_Slow = params->vo_CN_Ratio_AOM_Slow;
			pool.vo_CN_Ratio_AOM_Fast = areCropResidueParams ? calced_CN_Ratio_AOM_Fast : params->vo_CN_Ratio_AOM_Fast;
			pool.vo_PartAOM_Slow_to_SMB_Slow = params->vo_PartAOM_Slow_to_SMB_Slow;
			pool.vo_PartAOM_Slow_to_SMB_Fast = params->vo_PartAOM_Slow_to_SMB_Fast;
			pool.incorporation = this->incorporation;
			pool.noVolatilization = areCropResidueParams;

			// append this pool (template) to each layers pool list
			for(size_t i = 0; i < nools; i++)
			{
				soilColumn.at(i).vo_AOM_Pool.push_back(pool);

				// update the pool where the organic matter will go into
				if(i == intoLayerIndex)
				{
					auto& cpool = intoLayer.vo_AOM_Pool.back();
					cpool.vo_DaysAfterApplication = 1; //start daily volatilization process
					cpool.vo_AOM_DryMatterContent = params->vo_AOM_DryMatterContent;;
					cpool.vo_AOM_NH4Content = params->vo_AOM_NH4Content;
					cpool.vo_AOM_Slow = AOM_slow_input = params->vo_PartAOM_to_AOM_Slow * added_Corg_amount;
					cpool.vo_AOM_Fast = AOM_fast_input = params->vo_PartAOM_to_AOM_Fast * added_Corg_amount;
				}
			}

			// pools are now created, so can be used in the other layers
			poolSetIndex = int(soilColumn.at(0).vo_AOM_Pool.size() - 1);
		}
		else
		{
			auto& cpool = intoLayer.vo_AOM_Pool[poolSetIndex];
			cpool.vo_AOM_Slow += AOM_slow_input = params->vo_PartAOM_to_AOM_Slow * added_Corg_amount;

			double added_CN_ratio_AOM_fast = areCropResidueParams ? calced_CN_Ratio_AOM_Fast : params->vo_CN_Ratio_AOM_Fast;
			double pool_fast_N = cpool.vo_AOM_Fast / cpool.vo_CN_Ratio_AOM_Fast;
			double added_fast_N = params->vo_PartAOM_to_AOM_Fast * added_Corg_amount / added_CN_ratio_AOM_fast;
			cpool.vo_AOM_Fast += AOM_fast_input = params->vo_PartAOM_to_AOM_Fast * added_Corg_amount;
			double new_CN_ratio_AOM_fast = cpool.vo_AOM_Fast / (pool_fast_N + added_fast_N);

			//if(new_CN_ratio_AOM_fast > 300)
			//	cout << "bla" << endl;

			cpool.vo_CN_Ratio_AOM_Fast = new_CN_ratio_AOM_fast;
		}

		double soil_NH4_input =
			params->vo_AOM_NH4Content
			* addedOrganicMatterAmount
			* params->vo_AOM_DryMatterContent
			/ 10000.0
			/ layerThickness;

		double soil_NO3_input =
			params->vo_AOM_NO3Content
			* addedOrganicMatterAmount
			* params->vo_AOM_DryMatterContent
			/ 10000.0
			/ layerThickness;

		double SOM_FastInput =
			max(0.0, (1.0 - (params->vo_PartAOM_to_AOM_Slow + params->vo_PartAOM_to_AOM_Fast)))
			* added_Corg_amount;

		// immediate top layer pool update
    intoLayer.vs_SoilNH4 += soil_NH4_input;
    intoLayer.vs_SoilNO3 += soil_NO3_input;
    intoLayer.vs_SOM_Fast += SOM_FastInput;

		// store for further use
		vo_AOM_SlowInput[intoLayerIndex] += AOM_slow_input;
		vo_AOM_FastInput[intoLayerIndex] += AOM_fast_input;
		vo_SOM_FastInput[intoLayerIndex] += SOM_FastInput;
	}

	addedOrganicMatter = true;
}

void SoilOrganic::addIrrigationWater(double amount) {
  irrigationAmount += amount;
}

/**
 * @brief Supplying parameters of today's added organic matter (AOM)
 * @param residueParams Ernterückstände
 * @param vo_AOM_Addition
 */
 /*
 void SoilOrganic::fo_OM_Input(bool vo_AOM_Addition) {

	 //Allocation of AOM parameters
	 if (vo_AOM_Addition) {
		 assert(addedOrganicMatterParams != NULL);

		 vo_AOM_DryMatterContent = addedOrganicMatterParams->getVo_AOM_DryMatterContent();
		 vo_AOM_NH4Content = addedOrganicMatterParams->getVo_AOM_NH4Content();
		 vo_AOM_NO3Content = addedOrganicMatterParams->getVo_AOM_NO3Content();
		 vo_AOM_CarbamidContent = addedOrganicMatterParams->getVo_AOM_CarbamidContent();
		 vo_AOM_SlowDecCoeffStandard = addedOrganicMatterParams->getVo_AOM_SlowDecCoeffStandard();
		 vo_AOM_FastDecCoeffStandard = addedOrganicMatterParams->getVo_AOM_FastDecCoeffStandard();
		 vo_PartAOM_to_AOM_Slow = addedOrganicMatterParams->getVo_PartAOM_to_AOM_Slow();
		 vo_PartAOM_to_AOM_Fast = addedOrganicMatterParams->getVo_PartAOM_to_AOM_Fast();
		 vo_CN_Ratio_AOM_Slow = addedOrganicMatterParams->getVo_CN_Ratio_AOM_Slow();
		 CN_ratio_AOM_fast = addedOrganicMatterParams->getVo_CN_Ratio_AOM_Fast();
		 vo_PartAOM_Slow_to_SMB_Slow = addedOrganicMatterParams->getVo_PartAOM_Slow_to_SMB_Slow();
		 vo_PartAOM_Slow_to_SMB_Fast = addedOrganicMatterParams->getVo_PartAOM_Slow_to_SMB_Fast();

	 } else {

		 vo_AOM_DryMatterContent = 0.0;
		 vo_AOM_NH4Content = 0.0;
		 vo_AOM_NO3Content = 0.0;
		 vo_AOM_CarbamidContent = 0.0;
		 vo_AOM_SlowDecCoeffStandard = 0.0;
		 vo_AOM_FastDecCoeffStandard = 0.0;
		 vo_PartAOM_to_AOM_Slow = 0.0;
		 vo_PartAOM_to_AOM_Fast = 0.0;
		 vo_CN_Ratio_AOM_Slow = 1.0;
		 CN_ratio_AOM_fast = 1.0;
		 vo_PartAOM_Slow_to_SMB_Slow = 0.0;
		 vo_PartAOM_Slow_to_SMB_Fast = 0.0;
	 }
 }
  */

  /**
   * Calculation of urea solution and hydrolysis as well as ammonia
   * volatilisation from top layer based on Sadeghi et al. 1988
   *
   * <img src="../images/urea_solution.png">
   *
   *
   * @param vo_AddedOrganicMatterAmount
   * @param vo_RainIrrigation
   */
void SoilOrganic::fo_Urea(double vo_RainIrrigation) {
  auto nools = soilColumn.vs_NumberOfOrganicLayers();
  std::vector<double> vo_SoilCarbamid_solid(nools, 0.0); // Solid carbamide concentration in soil solution [kmol urea m-3]
  std::vector<double> vo_SoilCarbamid_aq(nools, 0.0); // Dissolved carbamide concetzration in soil solution [kmol urea m-3]
  std::vector<double> vo_HydrolysisRate1(nools, 0.0); // [kg N d-1]
  std::vector<double> vo_HydrolysisRate2(nools, 0.0); // [kg N d-1]
  std::vector<double> vo_HydrolysisRateMax(nools, 0.0); // [kg N d-1]
  std::vector<double> vo_Hydrolysis_pH_Effect(nools, 0.0);// []
  std::vector<double> vo_HydrolysisRate(nools, 0.0); // [kg N d-1]
  double vo_H3OIonConcentration = 0.0; // Oxonium ion concentration in soil solution [kmol m-3]
  double vo_NH3aq_EquilibriumConst = 0.0; // []
  double vo_NH3_EquilibriumConst = 0.0; // []
  double vs_SoilNH4aq = 0.0; // ammonium ion concentration in soil solution [kmol m-3}
  double vo_NH3aq = 0.0;
  double vo_NH3gas = 0.0;
  double vo_NH3_Volatilising = 0.0;

  double po_HydrolysisKM = _params.po_HydrolysisKM;
  double po_HydrolysisP1 = _params.po_HydrolysisP1;
  double po_HydrolysisP2 = _params.po_HydrolysisP2;
  double po_ActivationEnergy = _params.po_ActivationEnergy;

  vo_NH3_Volatilised = 0.0;

  for (int i = 0; i < soilColumn.vs_NumberOfOrganicLayers(); i++) {
    auto& layer = soilColumn.at(i);

    // kmol urea m-3 soil
    vo_SoilCarbamid_solid[i] = layer.vs_SoilCarbamid /
      OrganicConstants::po_UreaMolecularWeight /
      OrganicConstants::po_Urea_to_N / 1000.0;

    // mol urea kg Solution-1
    vo_SoilCarbamid_aq[i] = (-1258.9 + 13.2843 * (layer.get_Vs_SoilTemperature() + 273.15) -
                                   0.047381 * ((layer.get_Vs_SoilTemperature() + 273.15) *
                                   (layer.get_Vs_SoilTemperature() + 273.15)) +
                                   5.77264e-5 * (pow((layer.get_Vs_SoilTemperature() + 273.15), 3.0)));

    // kmol urea m-3 soil
    vo_SoilCarbamid_aq[i] = (vo_SoilCarbamid_aq[i] / (1.0 +
      (vo_SoilCarbamid_aq[i] * 0.0453))) *
      layer.get_Vs_SoilMoisture_m3();

    if (vo_SoilCarbamid_aq[i] >= vo_SoilCarbamid_solid[i]) {

      vo_SoilCarbamid_aq[i] = vo_SoilCarbamid_solid[i];
      vo_SoilCarbamid_solid[i] = 0.0;

    } else {
      vo_SoilCarbamid_solid[i] -= vo_SoilCarbamid_aq[i];
    }

    // Calculate urea hydrolysis

    vo_HydrolysisRate1[i] = (po_HydrolysisP1 *
      (layer.vs_SoilOrganicMatter() * 100.0) *
                                   OrganicConstants::po_SOM_to_C + po_HydrolysisP2) /
      OrganicConstants::po_UreaMolecularWeight;

    vo_HydrolysisRate2[i] = vo_HydrolysisRate1[i] /
      (exp(-po_ActivationEnergy /
      (8.314 * 310.0)));

    vo_HydrolysisRateMax[i] = vo_HydrolysisRate2[i] * exp(-po_ActivationEnergy /
      (8.314 * (layer.get_Vs_SoilTemperature() + 273.15)));

    vo_Hydrolysis_pH_Effect[i] = exp(-0.064 *
      ((layer.vs_SoilpH() - 6.5) * (layer.vs_SoilpH() - 6.5)));

    // kmol urea kg soil-1 s-1
    vo_HydrolysisRate[i] = vo_HydrolysisRateMax[i] *
      fo_MoistOnHydrolysis(layer.vs_SoilMoisture_pF()) *
      vo_Hydrolysis_pH_Effect[i] * vo_SoilCarbamid_aq[i] /
      (po_HydrolysisKM + vo_SoilCarbamid_aq[i]);

    // kmol urea m soil-3 d-1
    vo_HydrolysisRate[i] = vo_HydrolysisRate[i] * 86400.0 *
      layer.vs_SoilBulkDensity();

    if (vo_HydrolysisRate[i] >= vo_SoilCarbamid_aq[i]) {

      layer.vs_SoilNH4 += layer.vs_SoilCarbamid;
      layer.vs_SoilCarbamid = 0.0;

    } else {

      // kg N m soil-3
      layer.vs_SoilCarbamid -= vo_HydrolysisRate[i] *
        OrganicConstants::po_UreaMolecularWeight *
        OrganicConstants::po_Urea_to_N * 1000.0;

      // kg N m soil-3
      layer.vs_SoilNH4 += vo_HydrolysisRate[i] *
        OrganicConstants::po_UreaMolecularWeight *
        OrganicConstants::po_Urea_to_N * 1000.0;
    }

    // Calculate general volatilisation from NH4-Pool in top layer

    if (i == 0) {
      auto layer0 = soilColumn.at(0);

      vo_H3OIonConcentration = pow(10.0, (-layer0.vs_SoilpH())); // kmol m-3
      vo_NH3aq_EquilibriumConst = pow(10.0, ((-2728.3 /
        (layer0.get_Vs_SoilTemperature() + 273.15)) - 0.094219)); // K2 in Sadeghi's program

      vo_NH3_EquilibriumConst = pow(10.0, ((1630.5 /
        (layer0.get_Vs_SoilTemperature() + 273.15)) - 2.301));  // K1 in Sadeghi's program

      // kmol m-3, assuming that all NH4 is solved
      vs_SoilNH4aq = layer0.vs_SoilNH4 / (OrganicConstants::po_NH4MolecularWeight * 1000.0);

      // kmol m-3
      vo_NH3aq = vs_SoilNH4aq / (1.0 + (vo_H3OIonConcentration / vo_NH3aq_EquilibriumConst));

      vo_NH3gas = vo_NH3aq;
      //  vo_NH3gas = vo_NH3aq / vo_NH3_EquilibriumConst;

      // kg N m-3 d-1
      vo_NH3_Volatilising = vo_NH3gas * OrganicConstants::po_NH3MolecularWeight * 1000.0;

      if (vo_NH3_Volatilising >= layer0.vs_SoilNH4) {
        vo_NH3_Volatilising = layer0.vs_SoilNH4;
        layer0.vs_SoilNH4 = 0.0;
      } else {
        layer0.vs_SoilNH4 -= vo_NH3_Volatilising;
      }

      // kg N m-2 d-1
      vo_NH3_Volatilised = vo_NH3_Volatilising * layer0.vs_LayerThickness;

    } // if (i == 0) {
  } // for

  // set incorporation to false, if carbamid part is falling below a treshold
  // only, if organic matter was not recently added
  if (vo_SoilCarbamid_aq[0] < 0.001 && !addedOrganicMatter) {
    setIncorporation(false);
  }

}

/**
 * @brief Internal Subroutine MIT - Mineralisation Immobilisitation Turn-Over
 *
 * @param vo_AddedOrganicMatterAmount
 * @param vo_AOM_Addition
 * @param vo_AddedOrganicMatterNConcentration
 *
 */
void SoilOrganic::fo_MIT() {

  auto nools = soilColumn.vs_NumberOfOrganicLayers();
  double po_SOM_SlowDecCoeffStandard = _params.po_SOM_SlowDecCoeffStandard;
  double po_SOM_FastDecCoeffStandard = _params.po_SOM_FastDecCoeffStandard;
  double po_SMB_SlowDeathRateStandard = _params.po_SMB_SlowDeathRateStandard;
  double po_SMB_SlowMaintRateStandard = _params.po_SMB_SlowMaintRateStandard;
  double po_SMB_FastDeathRateStandard = _params.po_SMB_FastDeathRateStandard;
  double po_SMB_FastMaintRateStandard = _params.po_SMB_FastMaintRateStandard;
  double po_LimitClayEffect = _params.po_LimitClayEffect;
  double po_SOM_SlowUtilizationEfficiency = _params.po_SOM_SlowUtilizationEfficiency;
  double po_SOM_FastUtilizationEfficiency = _params.po_SOM_FastUtilizationEfficiency;
  double po_PartSOM_Fast_to_SOM_Slow = _params.po_PartSOM_Fast_to_SOM_Slow;
  double po_PartSMB_Slow_to_SOM_Fast = _params.po_PartSMB_Slow_to_SOM_Fast;
  double po_PartSMB_Fast_to_SOM_Fast = _params.po_PartSMB_Fast_to_SOM_Fast;
  double po_SMB_UtilizationEfficiency = _params.po_SMB_UtilizationEfficiency;
  double po_CN_Ratio_SMB = _params.po_CN_Ratio_SMB;
  double po_AOM_SlowUtilizationEfficiency = _params.po_AOM_SlowUtilizationEfficiency;
  double po_AOM_FastUtilizationEfficiency = _params.po_AOM_FastUtilizationEfficiency;
  double po_ImmobilisationRateCoeffNH4 = _params.po_ImmobilisationRateCoeffNH4;
  double po_ImmobilisationRateCoeffNO3 = _params.po_ImmobilisationRateCoeffNO3;

  std::vector<double> AOMslow_to_SMBfast(nools, 0.0);
  std::vector<double> AOMslow_to_SMBslow(nools, 0.0);
  std::vector<double> AOMfast_to_SMBfast(nools, 0.0);

  // Sum of decomposition rates for fast added organic matter pools
  std::vector<double> vo_AOM_FastDecRateSum(nools, 0.0);

  //Added organic matter fast pool change by decomposition [kg C m-3]
  //std::vector<double> vo_AOM_FastDelta(nools, 0.0);

  //Sum of all changes to added organic matter fast pool [kg C m-3]
  std::vector<double> vo_AOM_FastDeltaSum(nools, 0.0);

  //Added organic matter fast pool change by input [kg C m-3]
  //double vo_AOM_FastInput = 0.0;

  // Sum of decomposition rates for slow added organic matter pools
  std::vector<double> vo_AOM_SlowDecRateSum(nools, 0.0);

  // Added organic matter slow pool change by decomposition [kg C m-3]
  //std::vector<double> vo_AOM_SlowDelta(nools, 0.0);

  // Sum of all changes to added organic matter slow pool [kg C m-3]
  std::vector<double> vo_AOM_SlowDeltaSum(nools, 0.0);

  // [kg m-3]
  fill(vo_CBalance.begin(), vo_CBalance.end(), 0.0);

  // N balance of each layer [kg N m-3]
  std::vector<double> vo_NBalance(nools, 0.0);

  // CO2 preduced from fast fraction of soil microbial biomass [kg C m-3 d-1]
  std::vector<double> vo_SMB_FastCO2EvolutionRate(nools, 0.0);

  // Fast fraction of soil microbial biomass death rate [d-1]
  std::vector<double> vo_SMB_FastDeathRate(nools, 0.0);

  // Fast fraction of soil microbial biomass death rate coefficient [d-1]
  std::vector<double> vo_SMB_FastDeathRateCoeff(nools, 0.0);

  // Fast fraction of soil microbial biomass decomposition rate [d-1]
  std::vector<double> vo_SMB_FastDecRate(nools, 0.0);

  // Fast fraction of soil microbial biomass maintenance rate coefficient [d-1]
  std::vector<double> vo_SMB_FastMaintRateCoeff(nools, 0.0);

  // Fast fraction of soil microbial biomass maintenance rate [d-1]
  std::vector<double> vo_SMB_FastMaintRate(nools, 0.0);

  // Soil microbial biomass fast pool change [kg C m-3]
  fill(vo_SMB_FastDelta.begin(), vo_SMB_FastDelta.end(), 0.0);

  // CO2 preduced from slow fraction of soil microbial biomass [kg C m-3 d-1]
  std::vector<double> vo_SMB_SlowCO2EvolutionRate(nools, 0.0);

  // Slow fraction of soil microbial biomass death rate [d-1]
  std::vector<double> vo_SMB_SlowDeathRate(nools, 0.0);

  // Slow fraction of soil microbial biomass death rate coefficient [d-1]
  std::vector<double> vo_SMB_SlowDeathRateCoeff(nools, 0.0);

  // Slow fraction of soil microbial biomass decomposition rate [d-1]
  std::vector<double> vo_SMB_SlowDecRate(nools, 0.0);

  // Slow fraction of soil microbial biomass maintenance rate coefficient [d-1]
  std::vector<double> vo_SMB_SlowMaintRateCoeff(nools, 0.0);

  // Slow fraction of soil microbial biomass maintenance rate [d-1]
  std::vector<double> vo_SMB_SlowMaintRate(nools, 0.0);

  // Soil microbial biomass slow pool change [kg C m-3]
  fill(vo_SMB_SlowDelta.begin(), vo_SMB_SlowDelta.end(), 0.0);

  // Decomposition coefficient for rapidly decomposing soil organic matter [d-1]
  std::vector<double> vo_SOM_FastDecCoeff(nools, 0.0);

  // Decomposition rate for rapidly decomposing soil organic matter [d-1]
  std::vector<double> vo_SOM_FastDecRate(nools, 0.0);

  // Soil organic matter fast pool change [kg C m-3]
  fill(vo_SOM_FastDelta.begin(), vo_SOM_FastDelta.end(), 0.0);

  // Sum of all changes to soil organic matter fast pool [kg C m-3]
  //std::vector<double> vo_SOM_FastDeltaSum(nools, 0.0);

  // Decomposition coefficient for slowly decomposing soil organic matter [d-1]
  std::vector<double> vo_SOM_SlowDecCoeff(nools, 0.0);

  // Decomposition rate for slowly decomposing soil organic matter [d-1]
  std::vector<double> vo_SOM_SlowDecRate(nools, 0.0);

  // Soil organic matter slow pool change, unit [kg C m-3]
  fill(vo_SOM_SlowDelta.begin(), vo_SOM_SlowDelta.end(), 0.0);

  // Sum of all changes to soil organic matter slow pool [kg C m-3]
  //std::vector<double> vo_SOM_SlowDeltaSum(nools, 0.0);

  // Calculation of decay rate coefficients

  for (int i = 0; i < nools; i++) {
    auto& layi = soilColumn.at(i);
    double tod = fo_TempOnDecompostion(layi.get_Vs_SoilTemperature());
    double mod = fo_MoistOnDecompostion(layi.vs_SoilMoisture_pF());

    vo_SOM_SlowDecCoeff[i] = po_SOM_SlowDecCoeffStandard * tod * mod;
    vo_SOM_FastDecCoeff[i] = po_SOM_FastDecCoeffStandard * tod * mod;
    vo_SOM_SlowDecRate[i] = vo_SOM_SlowDecCoeff[i] * layi.vs_SOM_Slow;
    vo_SOM_FastDecRate[i] = vo_SOM_FastDecCoeff[i] * layi.vs_SOM_Fast;

    vo_SMB_SlowMaintRateCoeff[i] = po_SMB_SlowMaintRateStandard
      * fo_ClayOnDecompostion(layi.vs_SoilClayContent(),
                              po_LimitClayEffect) * tod * mod;

    vo_SMB_FastMaintRateCoeff[i] = po_SMB_FastMaintRateStandard * tod * mod;

    vo_SMB_SlowMaintRate[i] = vo_SMB_SlowMaintRateCoeff[i] * layi.vs_SMB_Slow;
    vo_SMB_FastMaintRate[i] = vo_SMB_FastMaintRateCoeff[i] * layi.vs_SMB_Fast;
    vo_SMB_SlowDeathRateCoeff[i] = po_SMB_SlowDeathRateStandard * tod * mod;
    vo_SMB_FastDeathRateCoeff[i] = po_SMB_FastDeathRateStandard * tod * mod;
    vo_SMB_SlowDeathRate[i] = vo_SMB_SlowDeathRateCoeff[i] * layi.vs_SMB_Slow;
    vo_SMB_FastDeathRate[i] = vo_SMB_FastDeathRateCoeff[i] * layi.vs_SMB_Fast;

    vo_SMB_SlowDecRate[i] = vo_SMB_SlowDeathRate[i] + vo_SMB_SlowMaintRate[i];
    vo_SMB_FastDecRate[i] = vo_SMB_FastDeathRate[i] + vo_SMB_FastMaintRate[i];

    for (AOM_Properties& AOM_Pool : layi.vo_AOM_Pool) {
      AOM_Pool.vo_AOM_SlowDecCoeff = AOM_Pool.vo_AOM_SlowDecCoeffStandard * tod * mod;
      AOM_Pool.vo_AOM_FastDecCoeff = AOM_Pool.vo_AOM_FastDecCoeffStandard * tod * mod;
    }
  } // for

  // Calculation of pool changes by decomposition
  for (int i = 0; i < nools; i++) {
    auto& layi = soilColumn.at(i);

    for (AOM_Properties& AOM_Pool : layi.vo_AOM_Pool) {
      // Eq.6-5 and 6-6 in the DAISY manual
      AOM_Pool.vo_AOM_SlowDelta = -(AOM_Pool.vo_AOM_SlowDecCoeff * AOM_Pool.vo_AOM_Slow);

      if (-AOM_Pool.vo_AOM_SlowDelta > AOM_Pool.vo_AOM_Slow)
        AOM_Pool.vo_AOM_SlowDelta = (-AOM_Pool.vo_AOM_Slow);

      AOM_Pool.vo_AOM_FastDelta = -(AOM_Pool.vo_AOM_FastDecCoeff * AOM_Pool.vo_AOM_Fast);

      if (-AOM_Pool.vo_AOM_FastDelta > AOM_Pool.vo_AOM_Fast)
        AOM_Pool.vo_AOM_FastDelta = (-AOM_Pool.vo_AOM_Fast);
    }

    // Eq.6-7 in the DAISY manual
    vo_AOM_SlowDecRateSum[i] = 0.0;

    for (AOM_Properties& AOM_Pool : layi.vo_AOM_Pool) {
      AOM_Pool.vo_AOM_SlowDecRate_to_SMB_Slow = AOM_Pool.vo_PartAOM_Slow_to_SMB_Slow
        * AOM_Pool.vo_AOM_SlowDecCoeff * AOM_Pool.vo_AOM_Slow;

      AOM_Pool.vo_AOM_SlowDecRate_to_SMB_Fast = AOM_Pool.vo_PartAOM_Slow_to_SMB_Fast
        * AOM_Pool.vo_AOM_SlowDecCoeff * AOM_Pool.vo_AOM_Slow;

      vo_AOM_SlowDecRateSum[i] += AOM_Pool.vo_AOM_SlowDecRate_to_SMB_Slow
        + AOM_Pool.vo_AOM_SlowDecRate_to_SMB_Fast;

      AOMslow_to_SMBfast[i] += AOM_Pool.vo_AOM_SlowDecRate_to_SMB_Fast;
      AOMslow_to_SMBslow[i] += AOM_Pool.vo_AOM_SlowDecRate_to_SMB_Slow;
    }

    // Eq.6-8 in the DAISY manual
    vo_AOM_FastDecRateSum[i] = 0.0;

    AOMfast_to_SMBfast[i] = 0.0;

    for (AOM_Properties& AOM_Pool : layi.vo_AOM_Pool) {
      //AOM_Pool.vo_AOM_FastDecRate_to_SMB_Slow = AOM_Pool.vo_PartAOM_Slow_to_SMB_Slow
      //	* AOM_Pool.vo_AOM_FastDecCoeff * AOM_Pool.vo_AOM_Fast;

      //AOM_Pool.vo_AOM_FastDecRate_to_SMB_Fast = AOM_Pool.vo_PartAOM_Slow_to_SMB_Fast
      //	* AOM_Pool.vo_AOM_FastDecCoeff * AOM_Pool.vo_AOM_Fast;

      AOM_Pool.vo_AOM_FastDecRate_to_SMB_Fast = AOM_Pool.vo_AOM_FastDecCoeff * AOM_Pool.vo_AOM_Fast;

      //vo_AOM_FastDecRateSum[i] += AOM_Pool.vo_AOM_FastDecRate_to_SMB_Slow
      //	+ AOM_Pool.vo_AOM_FastDecRate_to_SMB_Fast;
      vo_AOM_FastDecRateSum[i] += AOM_Pool.vo_AOM_FastDecRate_to_SMB_Fast;

      AOMfast_to_SMBfast[i] += AOM_Pool.vo_AOM_FastDecRate_to_SMB_Fast;
    }

    vo_SMB_SlowDelta[i] = (po_SOM_SlowUtilizationEfficiency * vo_SOM_SlowDecRate[i])
      + (po_SOM_FastUtilizationEfficiency * (1.0 - po_PartSOM_Fast_to_SOM_Slow) * vo_SOM_FastDecRate[i])
      //+ (po_AOM_SlowUtilizationEfficiency * vo_AOM_SlowDecRateSum[i])
      + (po_AOM_SlowUtilizationEfficiency * AOMslow_to_SMBslow[i])
      //+ (po_AOM_FastUtilizationEfficiency * AOMfast_to_SMBslow)
      - vo_SMB_SlowDecRate[i];


    vo_SMB_FastDelta[i] = (po_SMB_UtilizationEfficiency *
      (1.0 - po_PartSMB_Slow_to_SOM_Fast) * (vo_SMB_SlowDeathRate[i] + vo_SMB_FastDeathRate[i]))
      //+ (po_AOM_FastUtilizationEfficiency * vo_AOM_FastDecRateSum[i])
      + (po_AOM_FastUtilizationEfficiency * AOMfast_to_SMBfast[i])
      + (po_AOM_SlowUtilizationEfficiency * AOMslow_to_SMBfast[i])
      - vo_SMB_FastDecRate[i];

    //!Eq.6-9 in the DAISY manual
    vo_SOM_SlowDelta[i] = po_PartSOM_Fast_to_SOM_Slow * vo_SOM_FastDecRate[i]
      - vo_SOM_SlowDecRate[i];

    if ((layi.vs_SOM_Slow + vo_SOM_SlowDelta[i]) < 0.0)
      vo_SOM_SlowDelta[i] = layi.vs_SOM_Slow;

    // Eq.6-10 in the DAISY manual
    //vo_SOM_FastDelta[i] = po_PartSMB_Slow_to_SOM_Fast
    //	* (vo_SMB_SlowDeathRate[i] + vo_SMB_FastDeathRate[i])
    //	- vo_SOM_FastDecRate[i];

    vo_SOM_FastDelta[i] = po_PartSMB_Slow_to_SOM_Fast * vo_SMB_SlowDeathRate[i]
      + po_PartSMB_Fast_to_SOM_Fast * vo_SMB_FastDeathRate[i]
      - vo_SOM_FastDecRate[i];

    if ((layi.vs_SOM_Fast + vo_SOM_FastDelta[i]) < 0.0)
      vo_SOM_FastDelta[i] = layi.vs_SOM_Fast;

    vo_AOM_SlowDeltaSum[i] = 0.0;
    vo_AOM_FastDeltaSum[i] = 0.0;

    for (AOM_Properties& AOM_Pool : layi.vo_AOM_Pool) {
      vo_AOM_SlowDeltaSum[i] += AOM_Pool.vo_AOM_SlowDelta;
      vo_AOM_FastDeltaSum[i] += AOM_Pool.vo_AOM_FastDelta;
    }

  } // for i

  // Calculation of N balance
  for (int i = 0; i < nools; i++) {
    auto& layi = soilColumn.at(i);

    double vo_CN_Ratio_SOM_Slow = layi.vs_Soil_CN_Ratio();
    double vo_CN_Ratio_SOM_Fast = vo_CN_Ratio_SOM_Slow;

    vo_NBalance[i] = -(vo_SMB_SlowDelta[i] / po_CN_Ratio_SMB)
      - (vo_SMB_FastDelta[i] / po_CN_Ratio_SMB)
      - (vo_SOM_SlowDelta[i] / vo_CN_Ratio_SOM_Slow)
      - (vo_SOM_FastDelta[i] / vo_CN_Ratio_SOM_Fast);

    vector<AOM_Properties>& AOM_Pool = layi.vo_AOM_Pool;

    for (vector<AOM_Properties>::iterator it_AOM_Pool = AOM_Pool.begin(); it_AOM_Pool != AOM_Pool.end(); it_AOM_Pool++) {

      if (fabs(it_AOM_Pool->vo_CN_Ratio_AOM_Fast) >= 1.0E-7) {
        vo_NBalance[i] -= (it_AOM_Pool->vo_AOM_FastDelta / it_AOM_Pool->vo_CN_Ratio_AOM_Fast);
      } // if

      if (fabs(it_AOM_Pool->vo_CN_Ratio_AOM_Slow) >= 1.0E-7) {
        vo_NBalance[i] -= (it_AOM_Pool->vo_AOM_SlowDelta / it_AOM_Pool->vo_CN_Ratio_AOM_Slow);
      } // if
    } // for it_AOM_Pool
  } // for i

  // Check for Nmin availablity in case of immobilisation

  vo_NetNMineralisation = 0.0;


  for (int i = 0; i < nools; i++) {
    auto& layi = soilColumn.at(i);

    double vo_CN_Ratio_SOM_Slow = layi.vs_Soil_CN_Ratio();
    double vo_CN_Ratio_SOM_Fast = vo_CN_Ratio_SOM_Slow;

    if (vo_NBalance[i] < 0.0) {

      if (fabs(vo_NBalance[i]) >= ((layi.vs_SoilNH4 * po_ImmobilisationRateCoeffNH4)
                                         + (layi.vs_SoilNO3 * po_ImmobilisationRateCoeffNO3))) {
        vo_AOM_SlowDeltaSum[i] = 0.0;
        vo_AOM_FastDeltaSum[i] = 0.0;

        vector<AOM_Properties>& AOM_Pool = layi.vo_AOM_Pool;

        for (vector<AOM_Properties>::iterator it_AOM_Pool = AOM_Pool.begin(); it_AOM_Pool != AOM_Pool.end(); it_AOM_Pool++) {

          if (it_AOM_Pool->vo_CN_Ratio_AOM_Slow >= (po_CN_Ratio_SMB
                                                    / po_AOM_SlowUtilizationEfficiency)) {

            it_AOM_Pool->vo_AOM_SlowDelta = 0.0;
            //correction of the fluxes across pools
            AOMslow_to_SMBfast[i] -= it_AOM_Pool->vo_AOM_SlowDecRate_to_SMB_Fast;
            AOMslow_to_SMBslow[i] -= it_AOM_Pool->vo_AOM_SlowDecRate_to_SMB_Slow;
          } // if

          if (it_AOM_Pool->vo_CN_Ratio_AOM_Fast >= (po_CN_Ratio_SMB
                                                    / po_AOM_FastUtilizationEfficiency)) {

            it_AOM_Pool->vo_AOM_FastDelta = 0.0;
            //correction of the fluxes across pools
            AOMfast_to_SMBfast[i] -= it_AOM_Pool->vo_AOM_FastDecRate_to_SMB_Fast;
          } // if

          vo_AOM_SlowDeltaSum[i] += it_AOM_Pool->vo_AOM_SlowDelta;
          vo_AOM_FastDeltaSum[i] += it_AOM_Pool->vo_AOM_FastDelta;

        } // for

        if (vo_CN_Ratio_SOM_Slow >= (po_CN_Ratio_SMB / po_SOM_SlowUtilizationEfficiency)) {

          vo_SOM_SlowDelta[i] = 0.0;
        } // if

        if (vo_CN_Ratio_SOM_Fast >= (po_CN_Ratio_SMB / po_SOM_FastUtilizationEfficiency)) {

          vo_SOM_FastDelta[i] = 0.0;
        } // if

        // Recalculation of SMB pool changes
        //@todo <b>Claas: </b> Folgende Algorithmen prüfen: Was verändert sich?
        vo_SMB_SlowDelta[i] = (po_SOM_SlowUtilizationEfficiency * vo_SOM_SlowDecRate[i])
          + (po_SOM_FastUtilizationEfficiency * (1.0 - po_PartSOM_Fast_to_SOM_Slow) * vo_SOM_FastDecRate[i])
          //+ (po_AOM_SlowUtilizationEfficiency * vo_AOM_SlowDecRateSum[i])
          + (po_AOM_SlowUtilizationEfficiency * AOMslow_to_SMBslow[i])
          //+ (po_AOM_FastUtilizationEfficiency * AOMfast_to_SMBslow)
          - vo_SMB_SlowDecRate[i];

        if ((layi.vs_SMB_Slow + vo_SMB_SlowDelta[i]) < 0.0) {
          vo_SMB_SlowDelta[i] = layi.vs_SMB_Slow;
        }

        vo_SMB_FastDelta[i] = (po_SMB_UtilizationEfficiency *
          (1.0 - po_PartSMB_Slow_to_SOM_Fast) * (vo_SMB_SlowDeathRate[i] + vo_SMB_FastDeathRate[i]))
          //+ (po_AOM_FastUtilizationEfficiency * vo_AOM_FastDecRateSum[i])
          + (po_AOM_FastUtilizationEfficiency * AOMfast_to_SMBfast[i])
          + (po_AOM_SlowUtilizationEfficiency * AOMslow_to_SMBfast[i])
          - vo_SMB_FastDecRate[i];

        if ((layi.vs_SMB_Fast + vo_SMB_FastDelta[i]) < 0.0) {
          vo_SMB_FastDelta[i] = layi.vs_SMB_Fast;
        }

        // Recalculation of N balance under conditions of immobilisation
        vo_NBalance[i] = -(vo_SMB_SlowDelta[i] / po_CN_Ratio_SMB)
          - (vo_SMB_FastDelta[i] / po_CN_Ratio_SMB) - (vo_SOM_SlowDelta[i]
                                                             / vo_CN_Ratio_SOM_Slow) - (vo_SOM_FastDelta[i] / vo_CN_Ratio_SOM_Fast);

        for (vector<AOM_Properties>::iterator it_AOM_Pool =
             AOM_Pool.begin(); it_AOM_Pool != AOM_Pool.end(); it_AOM_Pool++) {

          if (fabs(it_AOM_Pool->vo_CN_Ratio_AOM_Fast) >= 1.0E-7) {

            vo_NBalance[i] -= (it_AOM_Pool->vo_AOM_FastDelta
                                     / it_AOM_Pool->vo_CN_Ratio_AOM_Fast);
          } // if

          if (fabs(it_AOM_Pool->vo_CN_Ratio_AOM_Slow) >= 1.0E-7) {

            vo_NBalance[i] -= (it_AOM_Pool->vo_AOM_SlowDelta
                                     / it_AOM_Pool->vo_CN_Ratio_AOM_Slow);
          } // if
        } // for

        // Update of Soil NH4 after recalculated N balance
        layi.vs_SoilNH4 += fabs(vo_NBalance[i]);


      } else { //if
       // Bedarf kann durch Ammonium-Pool nicht gedeckt werden --> Nitrat wird verwendet
        if (fabs(vo_NBalance[i]) >= (layi.vs_SoilNH4
                                           * po_ImmobilisationRateCoeffNH4)) {

          layi.vs_SoilNO3 -= fabs(vo_NBalance[i])
            - (layi.vs_SoilNH4
               * po_ImmobilisationRateCoeffNH4);

          layi.vs_SoilNH4 -= layi.vs_SoilNH4
            * po_ImmobilisationRateCoeffNH4;

        } else { // if

          layi.vs_SoilNH4 -= fabs(vo_NBalance[i]);
        } //else
      } //else

    } else { //if (N_Balance[i]) < 0.0

      layi.vs_SoilNH4 += fabs(vo_NBalance[i]);
    }

    auto& lay0 = soilColumn.at(0);
    vo_NetNMineralisationRate[i] = fabs(vo_NBalance[i]) * lay0.vs_LayerThickness; // [kg m-3] --> [kg m-2]
    vo_NetNMineralisation += fabs(vo_NBalance[i]) * lay0.vs_LayerThickness; // [kg m-3] --> [kg m-2]
    vo_SumNetNMineralisation += fabs(vo_NBalance[i]) * lay0.vs_LayerThickness; // [kg m-3] --> [kg m-2]

  }

  vo_DecomposerRespiration = 0.0;

  // Calculation of CO2 evolution
  for (int i = 0; i < nools; i++) {
    vo_SMB_SlowCO2EvolutionRate[i] = ((1.0 - po_SOM_SlowUtilizationEfficiency)
                                            * vo_SOM_SlowDecRate[i])
      + ((1.0 - po_SOM_FastUtilizationEfficiency) * (1.0 - po_PartSOM_Fast_to_SOM_Slow)
         * vo_SOM_FastDecRate[i])
      //+ ((1.0 - po_AOM_SlowUtilizationEfficiency) * vo_AOM_SlowDecRateSum[i])
      + ((1.0 - po_AOM_SlowUtilizationEfficiency) * AOMslow_to_SMBslow[i])
      + vo_SMB_SlowMaintRate[i];

    vo_SMB_FastCO2EvolutionRate[i] = (1.0 - po_SMB_UtilizationEfficiency)
      * (((1.0 - po_PartSMB_Slow_to_SOM_Fast) * vo_SMB_SlowDeathRate[i])
         + ((1.0 - po_PartSMB_Fast_to_SOM_Fast) * vo_SMB_FastDeathRate[i]))
      //+ ((1.0 - po_AOM_FastUtilizationEfficiency) * vo_AOM_FastDecRateSum[i])
      + ((1.0 - po_AOM_SlowUtilizationEfficiency) * AOMslow_to_SMBfast[i])
      + ((1.0 - po_AOM_FastUtilizationEfficiency) * AOMfast_to_SMBfast[i])
      + vo_SMB_FastMaintRate[i];

    vo_SMB_CO2EvolutionRate[i] = vo_SMB_SlowCO2EvolutionRate[i] + vo_SMB_FastCO2EvolutionRate[i];

    vo_DecomposerRespiration += vo_SMB_CO2EvolutionRate[i] * soilColumn.at(i).vs_LayerThickness; // [kg C m-3] -> [kg C m-2]

  } // for i
}

/**
 * @brief Volatilisation
 *
 * This routine calculates NH3 loss after manure or slurry application
 * taken from the ALFAM model: Soegaard et al. 2002 (Atm. Environ. 36,
 * 3309-3319); Only cattle slurry broadcast application considered so
 * far.
 *
 * @param vo_AOM_Addition
 * @param vw_MeanAirTemperature
 * @param vw_WindSpeed
 */
void SoilOrganic::fo_Volatilisation(bool vo_AOM_Addition, double vw_MeanAirTemperature, double vw_WindSpeed) {
  double vo_SoilWet;

  double vo_AOM_TAN_Content; // added organic matter total ammonium content [g N kg FM OM-1]
  double vo_MaxVolatilisation; // Maximum volatilisation [kg N ha-1 (kg N ha-1)-1]
  double vo_VolatilisationHalfLife; // [d]
  double vo_VolatilisationRate; // [kg N ha-1 (kg N ha-1)-1 d-1]
  double vo_N_PotVolatilised; // Potential volatilisation [kg N m-2]
  double vo_N_PotVolatilisedSum = 0.0; // Sums up potential volatilisation of all AOM pools [kg N m-2]
  double vo_N_ActVolatilised = 0.0; // Actual volatilisation [kg N m-2]

  int vo_DaysAfterApplicationSum = 0;

  auto lay0 = soilColumn.at(0);

  if (lay0.vs_SoilMoisture_pF() > 2.5) {
    vo_SoilWet = 0.0;
  } else {
    vo_SoilWet = 1.0;
  }

  vector<AOM_Properties>& AOM_Pool = lay0.vo_AOM_Pool;
  for (vector<AOM_Properties>::iterator it_AOM_Pool = AOM_Pool.begin(); it_AOM_Pool != AOM_Pool.end(); it_AOM_Pool++) {

    vo_DaysAfterApplicationSum += it_AOM_Pool->vo_DaysAfterApplication;
  }

  if (vo_DaysAfterApplicationSum > 0 || vo_AOM_Addition) {

    /** @todo <b>Claas: </b> if (vo_AOM_Addition == true)
     vo_DaysAfterApplication[vo_AOM_PoolAllocator]= 1; */

    vo_N_PotVolatilisedSum = 0.0;

    for (vector<AOM_Properties>::iterator it_AOM_Pool = AOM_Pool.begin(); it_AOM_Pool != AOM_Pool.end(); it_AOM_Pool++) {

      vo_AOM_TAN_Content = 0.0;
      vo_MaxVolatilisation = 0.0;
      vo_VolatilisationHalfLife = 0.0;
      vo_VolatilisationRate = 0.0;
      vo_N_PotVolatilised = 0.0;

      vo_AOM_TAN_Content = it_AOM_Pool->vo_AOM_NH4Content * 1000.0 * it_AOM_Pool->vo_AOM_DryMatterContent;

      vo_MaxVolatilisation = 0.0495 *
        pow(1.1020, vo_SoilWet) *
        pow(1.0223, vw_MeanAirTemperature) *
        pow(1.0417, vw_WindSpeed) *
        pow(1.1080, it_AOM_Pool->vo_AOM_DryMatterContent) *
        pow(0.8280, vo_AOM_TAN_Content) *
        pow(11.300, double(it_AOM_Pool->incorporation));

      vo_VolatilisationHalfLife = 1.0380 *
        pow(1.1020, vo_SoilWet) *
        pow(0.9600, vw_MeanAirTemperature) *
        pow(0.9500, vw_WindSpeed) *
        pow(1.1750, it_AOM_Pool->vo_AOM_DryMatterContent) *
        pow(1.1060, vo_AOM_TAN_Content) *
        pow(1.0000, double(it_AOM_Pool->incorporation)) *
        (18869.3 * exp(-lay0.vs_SoilpH() / 0.63321) + 0.70165);

      // ******************************************************************************************
      // *** Based on He et al. (1999): Soil Sci. 164 (10), 750-758. The curves on p. 755 were  ***
      // *** digitised and fit to Michaelis-Menten. The pH - Nhalf relation was normalised (pH  ***
      // *** 7.0 = 1; average soil pH of the ALFAM experiments) and fit to a decay function.    ***
      // *** The resulting factor was added to the Half Life calculation.                       ***
      // ******************************************************************************************

      vo_VolatilisationRate = vo_MaxVolatilisation * (vo_VolatilisationHalfLife / (pow(
        (it_AOM_Pool->vo_DaysAfterApplication + vo_VolatilisationHalfLife), 2.0)));

      vo_N_PotVolatilised = vo_VolatilisationRate * vo_AOM_TAN_Content * (it_AOM_Pool->vo_AOM_Slow
                                                                          + it_AOM_Pool->vo_AOM_Fast) / 10000.0 / 1000.0;

      vo_N_PotVolatilisedSum += vo_N_PotVolatilised;
    }

    if (lay0.vs_SoilNH4 > (vo_N_PotVolatilisedSum)) {
      vo_N_ActVolatilised = vo_N_PotVolatilisedSum;
    } else {
      vo_N_ActVolatilised = lay0.vs_SoilNH4;
    }

    // update NH4 content of top soil layer with volatilisation balance

    lay0.vs_SoilNH4 -= (vo_N_ActVolatilised / lay0.vs_LayerThickness);
  } else {
    vo_N_ActVolatilised = 0.0;
  }

  // NH3 volatilised from top layer NH4 pool. See Urea section
  vo_Total_NH3_Volatilised = (vo_N_ActVolatilised + vo_NH3_Volatilised); // [kg N m-2]
  /** @todo <b>Claas: </b>Zusammenfassung für output. Wohin damit??? */

  for (vector<AOM_Properties>::iterator it_AOM_Pool = AOM_Pool.begin(); it_AOM_Pool != AOM_Pool.end(); it_AOM_Pool++) {

    if (it_AOM_Pool->vo_DaysAfterApplication > 0 && !vo_AOM_Addition) {
      it_AOM_Pool->vo_DaysAfterApplication++;
    }
  }
}

/**
 * @brief Internal Subroutine Nitrification
 */
void SoilOrganic::fo_Nitrification() {
  auto nools = soilColumn.vs_NumberOfOrganicLayers();
  double po_AmmoniaOxidationRateCoeffStandard = _params.po_AmmoniaOxidationRateCoeffStandard;
  double po_NitriteOxidationRateCoeffStandard = _params.po_NitriteOxidationRateCoeffStandard;

  //! Nitrification rate coefficient [d-1]
  std::vector<double> vo_AmmoniaOxidationRateCoeff(nools, 0.0);
  std::vector<double> vo_NitriteOxidationRateCoeff(nools, 0.0);

  //! Nitrification rate [kg NH4-N m-3 d-1]
  //std::vector<double> vo_AmmoniaOxidationRate(nools, 0.0);
  //std::vector<double> vo_NitriteOxidationRate(nools, 0.0);

  for (int i = 0; i < nools; i++) {
    auto& layi = soilColumn.at(i);
    auto NH4i = layi.vs_SoilNH4;

    // Calculate nitrification rate coefficients
    //  cout << "SO-2:\t" << layi.vs_SoilMoisture_pF() << endl;
    vo_AmmoniaOxidationRateCoeff[i] = 
      po_AmmoniaOxidationRateCoeffStandard 
      * fo_TempOnNitrification(layi.get_Vs_SoilTemperature()) 
      * fo_MoistOnNitrification(layi.vs_SoilMoisture_pF());

    vo_ActAmmoniaOxidationRate[i] = vo_AmmoniaOxidationRateCoeff[i] * NH4i;

    vo_NitriteOxidationRateCoeff[i] = 
      po_NitriteOxidationRateCoeffStandard
      * fo_TempOnNitrification(layi.get_Vs_SoilTemperature())
      * fo_MoistOnNitrification(layi.vs_SoilMoisture_pF())
      * fo_NH3onNitriteOxidation(NH4i, layi.vs_SoilpH());

    vo_ActNitrificationRate[i] = vo_NitriteOxidationRateCoeff[i] * layi.vs_SoilNO2;

    // Update NH4, NO2 and NO3 content with nitrification balance
    // Stange, F., C. Nendel (2014): N.N., in preparation
    if (NH4i > vo_ActAmmoniaOxidationRate[i]) {
      layi.vs_SoilNH4 -= vo_ActAmmoniaOxidationRate[i];
      layi.vs_SoilNO2 += vo_ActAmmoniaOxidationRate[i];
    } else {
      layi.vs_SoilNO2 += NH4i;
      layi.vs_SoilNH4 = 0.0;
    }

    if (layi.vs_SoilNO2 > vo_ActNitrificationRate[i]) {
      layi.vs_SoilNO2 -= vo_ActNitrificationRate[i];
      layi.vs_SoilNO3 += vo_ActNitrificationRate[i];
    } else {
      layi.vs_SoilNO3 += layi.vs_SoilNO2;
      layi.vs_SoilNO2 = 0.0;
    }
  }
}

void SoilOrganic::fo_stics_Nitrification() {
  auto nools = soilColumn.vs_NumberOfOrganicLayers();
  auto sticsParams = _params.sticsParams;

  for (int i = 0; i < nools; i++) {
    auto& layi = soilColumn.at(i);
    auto smi = layi.get_Vs_SoilMoisture_m3(); // m3-water/m3-soil
    auto sbdi = layi.vs_SoilBulkDensity(); // kg-soil/m3-soil
    auto NH4i = layi.get_SoilNH4();

    auto kgN_per_m3_to_mgN_per_kg = 1000.0 * 1000.0 / sbdi;
    auto mgN_per_kg_to_kgN_per_m3 = 1 / kgN_per_m3_to_mgN_per_kg;

    vo_ActNitrificationRate[i] =
      stics::vnit(sticsParams,
                  NH4i * kgN_per_m3_to_mgN_per_kg, // kg-NH4-N/m3-soil -> mg-NH4-N/kg-soil)
                  layi.vs_SoilpH(), // []
                  layi.get_Vs_SoilTemperature(), // [°C]
                  smi / layi.vs_Saturation(), // soil water-filled pore space []
                  smi * 1000 / sbdi, // gravimetric soil water content kg-water/kg-soil
                  layi.vs_FieldCapacity(), // [m3-water/m3-soil] = []
                  layi.vs_Saturation())
      * mgN_per_kg_to_kgN_per_m3; // mg-N -> kg-N;

    if (NH4i > vo_ActNitrificationRate[i]) {
      layi.vs_SoilNH4 -= vo_ActNitrificationRate[i];
      layi.vs_SoilNO3 += vo_ActNitrificationRate[i];
    } else {
      layi.vs_SoilNO3 += NH4i;
      layi.vs_SoilNH4 = 0.0;
    }
  }
}

/**
 * @brief Denitrification
 */
void SoilOrganic::fo_Denitrification() {
  auto nools = soilColumn.vs_NumberOfOrganicLayers();
  std::vector<double> vo_PotDenitrificationRate(nools, 0.0);
  double po_SpecAnaerobDenitrification = _params.po_SpecAnaerobDenitrification;
  double po_TransportRateCoeff = _params.po_TransportRateCoeff;
  vo_TotalDenitrification = 0.0;

  for (int i = 0; i < nools; i++) {
    auto& layi = soilColumn.at(i);
    auto NO3i = layi.vs_SoilNO3;

    //Temperature function is the same as in Nitrification subroutine
    vo_PotDenitrificationRate[i] = po_SpecAnaerobDenitrification
      * vo_SMB_CO2EvolutionRate[i]
      * fo_TempOnNitrification(layi.get_Vs_SoilTemperature());

    vo_ActDenitrificationRate[i] = 
      min(vo_PotDenitrificationRate[i] * fo_MoistOnDenitrification(layi.get_Vs_SoilMoisture_m3(),
                                                                   layi.vs_Saturation()),
          po_TransportRateCoeff * NO3i);
  
    // update NO3 content of soil layer with denitrification balance [kg N m-3]
    if (NO3i > vo_ActDenitrificationRate[i]) {
      layi.vs_SoilNO3 -= vo_ActDenitrificationRate[i];
    } else {
      vo_ActDenitrificationRate[i] = NO3i;
      layi.vs_SoilNO3 = 0.0;
    }

    vo_TotalDenitrification += vo_ActDenitrificationRate[i] * layi.vs_LayerThickness; // [kg m-3] --> [kg m-2] ;
  }

  vo_SumDenitrification += vo_TotalDenitrification; // [kg N m-2]
}

void SoilOrganic::fo_stics_Denitrification() {
  auto nools = soilColumn.vs_NumberOfOrganicLayers();
  auto sticsParams = _params.sticsParams;
  vo_TotalDenitrification = 0.0;
  
  for (int i = 0; i < nools; i++) {
    auto& layi = soilColumn.at(i);
    auto smi = layi.get_Vs_SoilMoisture_m3(); // m3-water/m3-soil
    auto sbdi = layi.vs_SoilBulkDensity(); // kg-soil/m3-soil
    auto lti = layi.vs_LayerThickness;
    auto NO3i = layi.get_SoilNO3();

    auto kgN_per_m3_to_mgN_per_kg = 1000.0 * 1000.0 / sbdi;
    auto mgN_per_kg_to_kgN_per_m3 = 1 / kgN_per_m3_to_mgN_per_kg;

    vo_ActDenitrificationRate[i] =
    stics::vdenit(sticsParams,
                layi.vs_SoilOrganicCarbon() * 100.0, // kg-C/kg-soil = % [0-1] -> % [0-100]
                NO3i * kgN_per_m3_to_mgN_per_kg, // kg-NO3-N/m3-soil -> mg-NO3-N/kg-soil
                layi.get_Vs_SoilTemperature(), // [°C]
                smi / layi.vs_Saturation(), // soil water-filled pore space []
                smi * 1000 / sbdi) // gravimetric soil water content kg-water/kg-soil
    * mgN_per_kg_to_kgN_per_m3; // mg-N -> kg-N;

    // update NO3 content of soil layer with denitrification balance [kg N m-3]
    if (NO3i > vo_ActDenitrificationRate[i]) {
      layi.vs_SoilNO3 -= vo_ActDenitrificationRate[i];
    } else {
      vo_ActDenitrificationRate[i] = NO3i;
      layi.vs_SoilNO3 = 0.0;
    }
    vo_TotalDenitrification += vo_ActDenitrificationRate[i] * lti; // [kg m-3] --> [kg m-2] ;

  }
  
  vo_SumDenitrification += vo_TotalDenitrification; // [kg N m-2]
}

/**
 * @brief N2O production
 */
double SoilOrganic::fo_N2OProduction() {
  auto nools = soilColumn.vs_NumberOfOrganicLayers();
  double N2OProductionRate = _params.po_N2OProductionRate;
  double pKaHNO2 = OrganicConstants::po_pKaHNO2;
  double sumN2OProduced = 0.0;

  for (int i = 0; i < nools; i++) {
    auto& layi = soilColumn.at(i);
    auto pHi = layi.vs_SoilpH();
    auto NO2i = layi.vs_SoilNO2;
    auto lti = layi.vs_LayerThickness;
    auto tempi = layi.get_Vs_SoilTemperature();
    
    // pKaHNO2 original concept pow10. We used pow2 to allow reactive HNO2 being available at higer pH values
    double pH_response = 1.0 / (1.0 + pow(2.0, pHi - pKaHNO2));

    double N2OProductionAtLayer =
      NO2i
      * fo_TempOnNitrification(tempi)
      * N2OProductionRate
      * pH_response
      * lti * 10000; //convert from kg N-N2O m-3 to kg N-N2O ha-1 (for each layer)

    sumN2OProduced += N2OProductionAtLayer;
  }

  return sumN2OProduced;
}

SoilOrganic::NitDenitN2O SoilOrganic::fo_stics_N2OProduction() {
  auto nools = soilColumn.vs_NumberOfOrganicLayers();
  double sumN2OProducedNit = 0.0, sumN2OProducedDenit = 0.0;
  auto sticsParams = _params.sticsParams;

  for (int i = 0; i < nools; i++) {
    auto& layi = soilColumn.at(i);
    auto smi = layi.get_Vs_SoilMoisture_m3(); // m3-water/m3-soil
    auto sbdi = layi.vs_SoilBulkDensity(); // kg-soil/m3-soil
    auto lti = layi.vs_LayerThickness;

    auto kgN_per_m3_to_mgN_per_kg = 1000.0 * 1000.0 / sbdi;
    auto mgN_per_kg_to_kgN_per_m3 = 1 / kgN_per_m3_to_mgN_per_kg;
    
    double stics2monicaUnits = 
      mgN_per_kg_to_kgN_per_m3 // /kg-soil -> /m3-soil 
      * lti // /m3-soil -> /m2-soil
      * 10000.0; // /m2-soil -> /ha-soil
    
    auto N2ONitDenitAtLayer =
      stics::N2O(sticsParams,
                 layi.get_SoilNO3() * kgN_per_m3_to_mgN_per_kg, // kg-NO3-N/m3-soil -> mg-NO3-N/kg-soil
                 smi / layi.vs_Saturation(), // soil water-filled pore space []
                 layi.vs_SoilpH(), // []
                 vo_ActNitrificationRate[i] * kgN_per_m3_to_mgN_per_kg, // nitrification rate [mg-N/kg-soil/day] (* sbd = /m3-soil -> /kg-soil ; * 1000 = kg-N -> mg-N) 
                 vo_ActDenitrificationRate[i] * kgN_per_m3_to_mgN_per_kg); // denitrification rate [mg-N/kg-soil/day] (* sbd = /m3-soil -> /kg-soil ; * 1000 = kg-N -> mg-N)

    sumN2OProducedNit += N2ONitDenitAtLayer.first * stics2monicaUnits;
    sumN2OProducedDenit += N2ONitDenitAtLayer.second * stics2monicaUnits;
  }

  return make_pair(sumN2OProducedNit, sumN2OProducedDenit);
}

/**
 * @brief Internal Subroutine Pool update
 */
void SoilOrganic::fo_PoolUpdate()
{
	for(int i = 0; i < soilColumn.vs_NumberOfOrganicLayers(); i++)
	{
    auto& layi = soilColumn.at(i);

		vo_AOM_SlowDeltaSum[i] = 0.0;
		vo_AOM_FastDeltaSum[i] = 0.0;
		vo_AOM_SlowSum[i] = 0.0;
		vo_AOM_FastSum[i] = 0.0;

		for(auto& pool : layi.vo_AOM_Pool)
		{
			pool.vo_AOM_Slow += pool.vo_AOM_SlowDelta;
			pool.vo_AOM_Fast += pool.vo_AOM_FastDelta;

			vo_AOM_SlowDeltaSum[i] += pool.vo_AOM_SlowDelta;
			vo_AOM_FastDeltaSum[i] += pool.vo_AOM_FastDelta;

			vo_AOM_SlowSum[i] += pool.vo_AOM_Slow;
			vo_AOM_FastSum[i] += pool.vo_AOM_Fast;
		}

		layi.vs_SOM_Slow += vo_SOM_SlowDelta[i];
		layi.vs_SOM_Fast += vo_SOM_FastDelta[i];
		layi.vs_SMB_Slow += vo_SMB_SlowDelta[i];
		layi.vs_SMB_Fast += vo_SMB_FastDelta[i];

		vo_CBalance[i] = 
			vo_AOM_SlowInput[i]
			+ vo_AOM_FastInput[i]
			+ vo_AOM_SlowDeltaSum[i]
			+ vo_AOM_FastDeltaSum[i] 
			+ vo_SMB_SlowDelta[i]
			+ vo_SMB_FastDelta[i] 
			+ vo_SOM_SlowDelta[i]
			+ vo_SOM_FastDelta[i] 
			+ vo_SOM_FastInput[i];

		// ([kg C kg-1] * [kg m-3]) - [kg C m-3]
		vo_SoilOrganicC[i] = 
			(layi.vs_SoilOrganicCarbon()
			 * layi.vs_SoilBulkDensity())
			- vo_InertSoilOrganicC[i];
		vo_SoilOrganicC[i] += vo_CBalance[i];

		// [kg C m-3] / [kg m-3] --> [kg C kg-1]
		layi.set_SoilOrganicCarbon((vo_SoilOrganicC[i] + vo_InertSoilOrganicC[i])
																				/ layi.vs_SoilBulkDensity());

		// [kg C m-3] / [kg m-3] --> [kg C kg-1]
		//layi.set_SoilOrganicMatter
		//	((vo_SoilOrganicC[i] + vo_InertSoilOrganicC[i])
		//	 / OrganicConstants::po_SOM_to_C
		//	 / layi.vs_SoilBulkDensity());
	} 
}

/**
 * @brief Internal Function Clay effect on SOM decompostion
 * @param d_SoilClayContent
 * @param d_LimitClayEffect
 * @return
 */
double SoilOrganic::fo_ClayOnDecompostion(double d_SoilClayContent, double d_LimitClayEffect) {
  double fo_ClayOnDecompostion = 0.0;

  if (d_SoilClayContent >= 0.0 && d_SoilClayContent <= d_LimitClayEffect) {
    fo_ClayOnDecompostion = 1.0 - 2.0 * d_SoilClayContent;
  } else if (d_SoilClayContent > d_LimitClayEffect && d_SoilClayContent <= 1.0) {
    fo_ClayOnDecompostion = 1.0 - 2.0 * d_LimitClayEffect;
  } else {
    vo_ErrorMessage = "irregular clay content";
  }
  return fo_ClayOnDecompostion;
}

/**
 * @brief Internal Function Temperature effect on SOM decompostion
 * @param d_SoilTemperature
 * @return
 */
double SoilOrganic::fo_TempOnDecompostion(double d_SoilTemperature) {
  double fo_TempOnDecompostion = 0.0;

  if (d_SoilTemperature <= 0.0 && d_SoilTemperature > -40.0) {

    //
    fo_TempOnDecompostion = 0.0;

  } else if (d_SoilTemperature > 0.0 && d_SoilTemperature <= 20.0) {

    fo_TempOnDecompostion = 0.1 * d_SoilTemperature;

  } else if (d_SoilTemperature > 20.0 && d_SoilTemperature <= 70.0) {

    fo_TempOnDecompostion = exp(0.47 - (0.027 * d_SoilTemperature) + (0.00193 * d_SoilTemperature * d_SoilTemperature));
  } else {
    vo_ErrorMessage = "irregular soil temperature";
  }

  return fo_TempOnDecompostion;
}

/**
 * @brief Internal Function Moisture effect on SOM decompostion
 * @param d_SoilMoisture_pF
 * @return
 */
double SoilOrganic::fo_MoistOnDecompostion(double d_SoilMoisture_pF) {
  double fo_MoistOnDecompostion = 0.0;

  if (fabs(d_SoilMoisture_pF) <= 1.0E-7) {
    //
    fo_MoistOnDecompostion = 0.6;

  } else if (d_SoilMoisture_pF > 0.0 && d_SoilMoisture_pF <= 1.5) {
    //
    fo_MoistOnDecompostion = 0.6 + 0.4 * (d_SoilMoisture_pF / 1.5);

  } else if (d_SoilMoisture_pF > 1.5 && d_SoilMoisture_pF <= 2.5) {
    //
    fo_MoistOnDecompostion = 1.0;

  } else if (d_SoilMoisture_pF > 2.5 && d_SoilMoisture_pF <= 6.5) {
    //
    fo_MoistOnDecompostion = 1.0 - ((d_SoilMoisture_pF - 2.5) / 4.0);

  } else if (d_SoilMoisture_pF > 6.5) {

    fo_MoistOnDecompostion = 0.0;

  } else {
    vo_ErrorMessage = "irregular soil water content";
  }

  return fo_MoistOnDecompostion;
}

/**
 * @brief Internal Function Moisture effect on urea hydrolysis
 * @param d_SoilMoisture_pF
 * @return
 */
double SoilOrganic::fo_MoistOnHydrolysis(double d_SoilMoisture_pF) {
  double fo_MoistOnHydrolysis = 0.0;

  if (d_SoilMoisture_pF > 0.0 && d_SoilMoisture_pF <= 1.1) {
    fo_MoistOnHydrolysis = 0.72;

  } else if (d_SoilMoisture_pF > 1.1 && d_SoilMoisture_pF <= 2.4) {
    fo_MoistOnHydrolysis = 0.2207 * d_SoilMoisture_pF + 0.4672;

  } else if (d_SoilMoisture_pF > 2.4 && d_SoilMoisture_pF <= 3.4) {
    fo_MoistOnHydrolysis = 1.0;

  } else if (d_SoilMoisture_pF > 3.4 && d_SoilMoisture_pF <= 4.6) {
    fo_MoistOnHydrolysis = -0.8659 * d_SoilMoisture_pF + 3.9849;

  } else if (d_SoilMoisture_pF > 4.6) {
    fo_MoistOnHydrolysis = 0.0;

  } else {
    vo_ErrorMessage = "irregular soil water content";
  }

  return fo_MoistOnHydrolysis;
}

/**
 * @brief Internal Function Temperature effect on nitrification
 * @param d_SoilTemperature
 * @return
 */
double SoilOrganic::fo_TempOnNitrification(double soilTemp) {
  double result = 0.0;

  if (soilTemp <= 2.0 && soilTemp > -40.0)
    result = 0.0;
  else if (soilTemp > 2.0 && soilTemp <= 6.0)
    result = 0.15 * (soilTemp - 2.0);
  else if (soilTemp > 6.0 && soilTemp <= 20.0)
    result = 0.1 * soilTemp;
  else if (soilTemp > 20.0 && soilTemp <= 70.0)
    result = exp(0.47 - (0.027 * soilTemp) + (0.00193 * soilTemp * soilTemp));
  else
    vo_ErrorMessage = "irregular soil temperature";

  return result;
}

/**
 * @brief Internal Function Moisture effect on nitrification
 * @param d_SoilMoisture_pF
 * @return
 */
double SoilOrganic::fo_MoistOnNitrification(double d_SoilMoisture_pF) {
  double fo_MoistOnNitrification = 0.0;

  if (fabs(d_SoilMoisture_pF) <= 1.0E-7) {
    fo_MoistOnNitrification = 0.6;

  } else if (d_SoilMoisture_pF > 0.0 && d_SoilMoisture_pF <= 1.5) {
    fo_MoistOnNitrification = 0.6 + 0.4 * (d_SoilMoisture_pF / 1.5);

  } else if (d_SoilMoisture_pF > 1.5 && d_SoilMoisture_pF <= 2.5) {
    fo_MoistOnNitrification = 1.0;

  } else if (d_SoilMoisture_pF > 2.5 && d_SoilMoisture_pF <= 5.0) {
    fo_MoistOnNitrification = 1.0 - ((d_SoilMoisture_pF - 2.5) / 2.5);

  } else if (d_SoilMoisture_pF > 5.0) {
    fo_MoistOnNitrification = 0.0;

  } else {
    vo_ErrorMessage = "irregular soil water content";
  }
  return fo_MoistOnNitrification;
}

/**
 * @brief Internal Function Moisture effect on denitrification
 * @param d_SoilMoisture_m3
 * @param d_Saturation
 * @return
 */
double SoilOrganic::fo_MoistOnDenitrification(double d_SoilMoisture_m3, double d_Saturation) {

  double po_Denit1 = _params.po_Denit1;
  double po_Denit2 = _params.po_Denit2;
  double po_Denit3 = _params.po_Denit3;
  double fo_MoistOnDenitrification = 0.0;

  if ((d_SoilMoisture_m3 / d_Saturation) <= 0.8) {
    fo_MoistOnDenitrification = 0.0;

  } else if ((d_SoilMoisture_m3 / d_Saturation) > 0.8 && (d_SoilMoisture_m3 / d_Saturation) <= 0.9) {

    fo_MoistOnDenitrification = po_Denit1 * ((d_SoilMoisture_m3 / d_Saturation)
                                             - po_Denit2) / (po_Denit3 - po_Denit2);

  } else if ((d_SoilMoisture_m3 / d_Saturation) > 0.9 && (d_SoilMoisture_m3 / d_Saturation) <= 1.0) {

    fo_MoistOnDenitrification = po_Denit1 + (1.0 - po_Denit1)
      * ((d_SoilMoisture_m3 / d_Saturation) - po_Denit3) / (1.0 - po_Denit3);
  } else {
    vo_ErrorMessage = "irregular soil water content";
  }

  return fo_MoistOnDenitrification;
}




/**
 * @brief Internal Function NH3 effect on nitrite oxidation
 * @param d_SoilNH4
 * @param d_SoilpH
 * @return
 */
double SoilOrganic::fo_NH3onNitriteOxidation(double d_SoilNH4, double d_SoilpH) {

  double po_Inhibitor_NH3 = _params.po_Inhibitor_NH3;
  double fo_NH3onNitriteOxidation = 0.0;

  fo_NH3onNitriteOxidation = po_Inhibitor_NH3 / (po_Inhibitor_NH3 + d_SoilNH4 * (1 - 1 / (1.0 + pow(10.0, d_SoilpH - OrganicConstants::po_pKaNH3))));

  return fo_NH3onNitriteOxidation;
}

/**
 * @brief Calculates Net ecosystem production [kg C ha-1 d-1].
 *
 */
double SoilOrganic::fo_NetEcosystemProduction(double d_NetPrimaryProduction, double d_DecomposerRespiration) {

  double vo_NEP = 0.0;

  vo_NEP = d_NetPrimaryProduction - (d_DecomposerRespiration * 10000.0); // [kg C ha-1 d-1]

  return vo_NEP;
}


/**
 * @brief Calculates Net ecosystem production [kg C ha-1 d-1].
 *
 */
double SoilOrganic::fo_NetEcosystemExchange(double d_NetPrimaryProduction, double d_DecomposerRespiration) {

  // NEE = NEP (M.U.F. Kirschbaum and R. Mueller (2001): Net Ecosystem Exchange. Workshop Proceedings CRC for greenhouse accounting.
  // Per definition: NPP is negative and respiration is positive

  double vo_NEE = 0.0;

  vo_NEE = -d_NetPrimaryProduction + (d_DecomposerRespiration * 10000.0); // [kg C ha-1 d-1]

  return vo_NEE;
}


/**
 * @brief Returns soil organic C [kgC kg-1]
 * @return Soil organic C
 */
double SoilOrganic::get_SoilOrganicC(int i_Layer) const {
  return vo_SoilOrganicC[i_Layer] / soilColumn.at(i_Layer).vs_SoilBulkDensity();
}

/**
 * @brief Returns sum of AOM fast [kg C m-3]
 * @return Sum AOM fast
 */
double SoilOrganic::get_AOM_FastSum(int i_Layer) const {
  return vo_AOM_FastSum[i_Layer];
}

/**
 * @brief Returns sum of AOM slow [kg C m-3]
 * @return Sum AOM slow
 */
double SoilOrganic::get_AOM_SlowSum(int i_Layer) const {
  return vo_AOM_SlowSum[i_Layer];
}

/**
 * @brief Returns SMB fast [kg C m-3]
 * @return SMB fast
 */
double SoilOrganic::get_SMB_Fast(int i_Layer) const {
  return soilColumn.at(i_Layer).vs_SMB_Fast;
}

/**
 * @brief Returns SMB slow [kg C m-3]
 * @return SMB slow
 */
double SoilOrganic::get_SMB_Slow(int i_Layer) const {
  return soilColumn.at(i_Layer).vs_SMB_Slow;
}

/**
 * @brief Returns SOM fast [kg C m-3]
 * @return AOM fast
 */
double SoilOrganic::get_SOM_Fast(int i_Layer) const {
  return soilColumn.at(i_Layer).vs_SOM_Fast;
}

/**
 * @brief Returns SOM slow [kg C m-3]
 * @return SOM slow
 */
double SoilOrganic::get_SOM_Slow(int i_Layer) const {
  return soilColumn.at(i_Layer).vs_SOM_Slow;
}

/**
 * @brief Returns C balance [kg C m-3]
 * @return C balance
 */
double SoilOrganic::get_CBalance(int i_Layer) const {
  return vo_CBalance[i_Layer];
}

/**
 * Returns SMB CO2 evolution rate at given layer.
 * @param i_layer
 * @return  SMB CO2 evolution rate
 */
double SoilOrganic::get_SMB_CO2EvolutionRate(int i_Layer) const {
  return vo_SMB_CO2EvolutionRate.at(i_Layer);
}

/**
 * Returns actual denitrification rate in layer
 * @param i
 * @return denitrification rate [kg N m-3 d-1]
 */
double SoilOrganic::get_ActDenitrificationRate(int i_Layer) const {
  return vo_ActDenitrificationRate.at(i_Layer);
}

/**
 * Returns actual N mineralisation rate in layer
 * @param i
 * @return N mineralisation rate [kg N ha-1 d-1]
 */
double SoilOrganic::get_NetNMineralisationRate(int i_Layer) const {
  return vo_NetNMineralisationRate.at(i_Layer) * 10000.0;
}

/**
 * Returns cumulative total N mineralisation
 * @return Total N mineralistaion [kg N ha-1]
 */
double SoilOrganic::get_NetNMineralisation() const {
  return vo_NetNMineralisation * 10000.0;
}

/**
 * Returns cumulative total N mineralisation
 * @return Total N mineralistaion [kg N ha-1]
 */
double SoilOrganic::get_SumNetNMineralisation() const {
  return vo_SumNetNMineralisation * 10000.0;
}

/**
 * Returns cumulative total N denitrification
 * @return Total N denitrification [kg N ha-1]
 */
double SoilOrganic::get_SumDenitrification() const {
  return vo_SumDenitrification * 10000.0;
}


/**
 * Returns N denitrification
 * @return N denitrification [kg N ha-1]
 */
double SoilOrganic::get_Denitrification() const {
  return vo_TotalDenitrification * 10000.0;
}

/**
 * Returns NH3 volatilisation
 * @return NH3 volatilisation [kgN ha-1]
 */
double SoilOrganic::get_NH3_Volatilised() const {
  return vo_Total_NH3_Volatilised * 10000.0;
}

/**
 * Returns cumulative total NH3 volatilisation
 * @return Total NH3 volatilisation [kg N ha-1]
 */
double SoilOrganic::get_SumNH3_Volatilised() const {
  return vo_SumNH3_Volatilised * 10000.0;
}


/**
 * Returns N2O Production
 * @return N2O Production [kg N ha-1]
 */
double SoilOrganic::get_N2O_Produced() const {
  return vo_N2O_Produced;
}


/**
 * Returns cumulative total N2O Production
 * @return Cumulative total N2O Production [kg N ha-1]
 */
double SoilOrganic::get_SumN2O_Produced() const {
  return vo_SumN2O_Produced;
}


/**
 * Returns daily decomposer respiration
 * @return daily decomposer respiration [kg C ha-1 d-1]
 */
double SoilOrganic::get_DecomposerRespiration() const {
  return vo_DecomposerRespiration * 10000.0;
}


/**
 * Returns daily net ecosystem production
 * @return daily net ecosystem production [kg C ha-1 d-1]
 */
double SoilOrganic::get_NetEcosystemProduction() const {
  return vo_NetEcosystemProduction;
}


/**
 * Returns daily net ecosystem exchange
 * @return daily net ecosystem exchange [kg C ha-1 d-1]
 */
double SoilOrganic::get_NetEcosystemExchange() const {
  return vo_NetEcosystemExchange;
}

void SoilOrganic::put_Crop(CropModule* c) {
  crop = c;
}

void SoilOrganic::remove_Crop() {
  crop = nullptr;
}

double SoilOrganic::get_Organic_N(int i) const {
  double orgN = 0;

  orgN += get_SMB_Fast(i) / _params.po_CN_Ratio_SMB;
  orgN += get_SMB_Slow(i) / _params.po_CN_Ratio_SMB;

  double cn = soilColumn.at(i).vs_Soil_CN_Ratio();
  orgN += get_SOM_Fast(i) / cn;
  orgN += get_SOM_Slow(i) / cn;

  for (const auto& aomp : soilColumn.at(i).vo_AOM_Pool) {
    orgN += aomp.vo_AOM_Fast / aomp.vo_CN_Ratio_AOM_Fast;
    orgN += aomp.vo_AOM_Slow / aomp.vo_CN_Ratio_AOM_Slow;
  }

  return orgN;
}
