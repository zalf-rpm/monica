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

#include <algorithm>
#include <cmath>

#include "soilorganic.h"
#include "soilcolumn.h"
#include "monica-model.h"
#include "crop-growth.h"
#include "tools/debug.h"
#include "soil/constants.h"

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
SoilOrganic::SoilOrganic(SoilColumn& sc,
												 const SiteParameters& stps,
												 const UserSoilOrganicParameters& userParams)
	: soilColumn(sc),
	siteParams(stps),
	organicPs(userParams),
	vs_NumberOfLayers(sc.vs_NumberOfLayers()),
	vs_NumberOfOrganicLayers(sc.vs_NumberOfOrganicLayers()),
	vo_ActDenitrificationRate(sc.vs_NumberOfOrganicLayers()),
	vo_AOM_FastDeltaSum(sc.vs_NumberOfOrganicLayers()),
	vo_AOM_FastSum(sc.vs_NumberOfOrganicLayers()),
	vo_AOM_SlowDeltaSum(sc.vs_NumberOfOrganicLayers()),
	vo_AOM_SlowSum(sc.vs_NumberOfOrganicLayers()),
	vo_CBalance(sc.vs_NumberOfOrganicLayers()),
	vo_InertSoilOrganicC(sc.vs_NumberOfOrganicLayers()),
	vo_NetNMineralisationRate(sc.vs_NumberOfOrganicLayers()),
	vo_SMB_CO2EvolutionRate(sc.vs_NumberOfOrganicLayers()),
	vo_SMB_FastDelta(sc.vs_NumberOfOrganicLayers()),
	vo_SMB_SlowDelta(sc.vs_NumberOfOrganicLayers()),
	vo_SoilOrganicC(sc.vs_NumberOfOrganicLayers()),
	vo_SOM_FastDelta(sc.vs_NumberOfOrganicLayers()),
	vo_SOM_SlowDelta(sc.vs_NumberOfOrganicLayers())
{
	// Subroutine Pool initialisation
	double po_SOM_SlowUtilizationEfficiency = organicPs.po_SOM_SlowUtilizationEfficiency;
	double po_PartSOM_to_SMB_Slow = organicPs.po_PartSOM_to_SMB_Slow;
	double po_SOM_FastUtilizationEfficiency = organicPs.po_SOM_FastUtilizationEfficiency;
	double po_PartSOM_to_SMB_Fast = organicPs.po_PartSOM_to_SMB_Fast;
	double po_SOM_SlowDecCoeffStandard = organicPs.po_SOM_SlowDecCoeffStandard;
	double po_SOM_FastDecCoeffStandard = organicPs.po_SOM_FastDecCoeffStandard;
	double po_PartSOM_Fast_to_SOM_Slow = organicPs.po_PartSOM_Fast_to_SOM_Slow;

	//Conversion of soil organic carbon weight fraction to volume unit
	for(size_t i_Layer = 0; i_Layer < vs_NumberOfOrganicLayers; i_Layer++)
	{
		vo_SoilOrganicC[i_Layer] = soilColumn[i_Layer].vs_SoilOrganicCarbon() * soilColumn[i_Layer].vs_SoilBulkDensity(); //[kg C kg-1] * [kg m-3] --> [kg C m-3]

		// Falloon et al. (1998): Estimating the size of the inert organic matter pool
		// from total soil oragnic carbon content for use in the Rothamsted Carbon model.
		// Soil Biol. Biochem. 30 (8/9), 1207-1211. for values in t C ha-1.
		// vo_InertSoilOrganicC is calculated back to [kg C m-3].
		vo_InertSoilOrganicC[i_Layer] = (0.049 * pow((vo_SoilOrganicC[i_Layer] // [kg C m-3]
																									* soilColumn[i_Layer].vs_LayerThickness // [kg C m-2]
																									/ 1000 * 10000.0), 1.139)) // [t C ha-1]
			/ 10000.0 * 1000.0 // [kg C m-2]
			/ soilColumn[i_Layer].vs_LayerThickness; // [kg C m-3]

		vo_SoilOrganicC[i_Layer] -= vo_InertSoilOrganicC[i_Layer]; // [kg C m-3]

		// Initialisation of pool SMB_Slow [kg C m-3]
		soilColumn[i_Layer].vs_SMB_Slow = po_SOM_SlowUtilizationEfficiency
			* po_PartSOM_to_SMB_Slow * vo_SoilOrganicC[i_Layer];

		// Initialisation of pool SMB_Fast [kg C m-3]
		soilColumn[i_Layer].vs_SMB_Fast = po_SOM_FastUtilizationEfficiency
			* po_PartSOM_to_SMB_Fast * vo_SoilOrganicC[i_Layer];

		// Initialisation of pool SOM_Slow [kg C m-3]
		soilColumn[i_Layer].vs_SOM_Slow = vo_SoilOrganicC[i_Layer] / (1.0 + po_SOM_SlowDecCoeffStandard
																																	/ (po_SOM_FastDecCoeffStandard * po_PartSOM_Fast_to_SOM_Slow));

		// Initialisation of pool SOM_Fast [kg C m-3]
		soilColumn[i_Layer].vs_SOM_Fast = vo_SoilOrganicC[i_Layer] - soilColumn[i_Layer].vs_SOM_Slow;

		// Soil Organic Matter pool update [kg C m-3]
		vo_SoilOrganicC[i_Layer] -= soilColumn[i_Layer].vs_SMB_Slow + soilColumn[i_Layer].vs_SMB_Fast;

		soilColumn[i_Layer].set_SoilOrganicCarbon
			((vo_SoilOrganicC[i_Layer] + vo_InertSoilOrganicC[i_Layer])
			 / soilColumn[i_Layer].vs_SoilBulkDensity()); // [kg C m-3] / [kg m-3] --> [kg C kg-1]

		//this is not needed as either one of organic carbon or organic matter is only be used
	//soilColumn[i_Layer].set_SoilOrganicMatter
	//	((vo_SoilOrganicC[i_Layer] + vo_InertSoilOrganicC[i_Layer]) 
	//	 / OrganicConstants::po_SOM_to_C
	//	 / soilColumn[i_Layer].vs_SoilBulkDensity());  // [kg C m-3] / [kg m-3] --> [kg C kg-1]

		vo_ActDenitrificationRate.at(i_Layer) = 0.0;
	} // for
}

/**
 * @brief Calculation step for one time step.
 * @param vw_MeanAirTemperature
 * @param vw_Precipitation
 * @param vw_WindSpeed
 */
void SoilOrganic::step(double vw_MeanAirTemperature, double vw_Precipitation,
											 double vw_WindSpeed)
{

	double vc_NetPrimaryProduction = 0.0;
	vc_NetPrimaryProduction = crop ? crop->get_NetPrimaryProduction() : 0;

	//cout << "get_OrganBiomass(organ) : " << organ << ", " << organ_percentage << std::endl; // JV!
	//cout << "total_biomass : " << total_biomass << std::endl; // JV!

	//fo_OM_Input(vo_AOM_Addition);
	// Mineralisation Immobilisitation Turn-Over
	fo_MIT();
	//fo_Volatilisation(addedOrganicMatter, vw_MeanAirTemperature, vw_WindSpeed);
	fo_Nitrification();
	//fo_Denitrification();
	//fo_N2OProduction();
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
	vo_AOM_SlowInput = 0.0;
	vo_AOM_FastInput = 0.0;
	vo_SOM_FastInput = 0.0;
	addedOrganicMatter = false;
}

void SoilOrganic::addOrganicMatter(OrganicMatterParametersPtr params,
																	 double amount, double nConcentration)
{
	debug() << "SoilOrganic: addOrganicMatter: " << params->toString() << endl;
	double vo_AddedOrganicMatterAmount = amount;
	double vo_AddedOrganicMatterNConcentration = nConcentration;
	double vo_AOM_DryMatterContent = params->vo_AOM_DryMatterContent;
	double vo_AOM_NH4Content = params->vo_AOM_NH4Content;
	double vo_AOM_NO3Content = params->vo_AOM_NO3Content;
	double vo_AOM_CarbamidContent = params->vo_AOM_CarbamidContent;
	double vo_PartAOM_to_AOM_Slow = params->vo_PartAOM_to_AOM_Slow;
	double vo_PartAOM_to_AOM_Fast = params->vo_PartAOM_to_AOM_Fast;
	double vo_CN_Ratio_AOM_Slow = params->vo_CN_Ratio_AOM_Slow;
	double vo_CN_Ratio_AOM_Fast = params->vo_CN_Ratio_AOM_Fast;

	double po_AOM_FastMaxC_to_N = organicPs.po_AOM_FastMaxC_to_N;

	//urea
	if(soilColumn.vs_NumberOfOrganicLayers() > 0)
	{
		// kg N m-3 soil
		soilColumn[0].vs_SoilCarbamid += vo_AddedOrganicMatterAmount
			* vo_AOM_DryMatterContent * vo_AOM_CarbamidContent
			/ 10000.0 / soilColumn[0].vs_LayerThickness;
	}

	double vo_AddedOrganicCarbonAmount = 0.0;
	double vo_AddedOrganicNitrogenAmount = 0.0;

	//MIT
	auto nools = soilColumn.vs_NumberOfOrganicLayers();
	for(int i_Layer = 0; i_Layer < nools; i_Layer++)
	{
		//New AOM pool
		if(i_Layer == 0)
		{
			AOM_Properties aom_pool;

			aom_pool.vo_DaysAfterApplication = 0;
			aom_pool.vo_AOM_DryMatterContent = vo_AOM_DryMatterContent;
			aom_pool.vo_AOM_NH4Content = vo_AOM_NH4Content;
			aom_pool.vo_AOM_Slow = 0.0;
			aom_pool.vo_AOM_Fast = 0.0;
			aom_pool.vo_AOM_SlowDecCoeffStandard = params->vo_AOM_SlowDecCoeffStandard;
			aom_pool.vo_AOM_FastDecCoeffStandard = params->vo_AOM_FastDecCoeffStandard;
			aom_pool.vo_CN_Ratio_AOM_Slow = vo_CN_Ratio_AOM_Slow;
			aom_pool.incorporation = this->incorporation;

			// Converting AOM from kg FM OM ha-1 to kg C m-3
			vo_AddedOrganicCarbonAmount = vo_AddedOrganicMatterAmount
				* vo_AOM_DryMatterContent
				* OrganicConstants::po_AOM_to_C
				/ 10000.0
				/ soilColumn[0].vs_LayerThickness;

			if(vo_CN_Ratio_AOM_Fast <= 1.0E-7)
			{
				// Wenn in der Datenbank hier Null steht, handelt es sich um einen
				// Pflanzenrückstand. Dann erfolgt eine dynamische Berechnung des
				// C/N-Verhältnisses. Für Wirtschafstdünger ist dieser Wert
				// parametrisiert.

				// Converting AOM N content from kg N kg DM-1 to kg N m-3
				vo_AddedOrganicNitrogenAmount = vo_AddedOrganicMatterAmount
					* vo_AOM_DryMatterContent
					* vo_AddedOrganicMatterNConcentration
					/ 10000.0
					/ soilColumn[0].vs_LayerThickness;

				debug() << "Added organic matter N amount: " << vo_AddedOrganicNitrogenAmount << endl;
				if(vo_AddedOrganicMatterNConcentration <= 0.0)
					vo_AddedOrganicNitrogenAmount = 0.01;

				// Assigning the dynamic C/N ratio to the AOM_Fast pool
				if((vo_AddedOrganicCarbonAmount * vo_PartAOM_to_AOM_Slow / vo_CN_Ratio_AOM_Slow)
					 < vo_AddedOrganicNitrogenAmount)
					vo_CN_Ratio_AOM_Fast = (vo_AddedOrganicCarbonAmount * vo_PartAOM_to_AOM_Fast)
					/ (vo_AddedOrganicNitrogenAmount - (vo_AddedOrganicCarbonAmount
																							* vo_PartAOM_to_AOM_Slow
																							/ vo_CN_Ratio_AOM_Slow));
				else
					vo_CN_Ratio_AOM_Fast = po_AOM_FastMaxC_to_N;

				if(vo_CN_Ratio_AOM_Fast > po_AOM_FastMaxC_to_N)
					vo_CN_Ratio_AOM_Fast = po_AOM_FastMaxC_to_N;

				aom_pool.vo_CN_Ratio_AOM_Fast = vo_CN_Ratio_AOM_Fast;
			}
			else
				aom_pool.vo_CN_Ratio_AOM_Fast = params->vo_CN_Ratio_AOM_Fast;


			aom_pool.vo_PartAOM_Slow_to_SMB_Slow = params->vo_PartAOM_Slow_to_SMB_Slow;
			aom_pool.vo_PartAOM_Slow_to_SMB_Fast = params->vo_PartAOM_Slow_to_SMB_Fast;

			soilColumn[0].vo_AOM_Pool.push_back(aom_pool);
			//cout << "poolsize: " << soilColumn[0].vo_AOM_Pool.size() << endl;

		}
		else //if (i_Layer == 0)
		{

			AOM_Properties aom_pool;

			aom_pool.vo_DaysAfterApplication = 0;
			aom_pool.vo_AOM_DryMatterContent = 0.0;
			aom_pool.vo_AOM_NH4Content = 0.0;
			aom_pool.vo_AOM_Slow = 0.0;
			aom_pool.vo_AOM_Fast = 0.0;
			aom_pool.vo_AOM_SlowDecCoeffStandard = params->vo_AOM_SlowDecCoeffStandard;
			aom_pool.vo_AOM_FastDecCoeffStandard = params->vo_AOM_FastDecCoeffStandard;
			aom_pool.vo_CN_Ratio_AOM_Slow = vo_CN_Ratio_AOM_Slow;

			if(!soilColumn[0].vo_AOM_Pool.empty())
				aom_pool.vo_CN_Ratio_AOM_Fast = soilColumn[0].vo_AOM_Pool.back().vo_CN_Ratio_AOM_Fast;
			else
				aom_pool.vo_CN_Ratio_AOM_Fast = vo_CN_Ratio_AOM_Fast;

			aom_pool.vo_PartAOM_Slow_to_SMB_Slow = params->vo_PartAOM_Slow_to_SMB_Slow;
			aom_pool.vo_PartAOM_Slow_to_SMB_Fast = params->vo_PartAOM_Slow_to_SMB_Fast;
			aom_pool.incorporation = this->incorporation;

			soilColumn[i_Layer].vo_AOM_Pool.push_back(aom_pool);

		} //else
	} // for i_Layer

	/*
	AddedOMParams aomps;
	aomps.vo_AddedOrganicCarbonAmount = vo_AddedOrganicCarbonAmount;
	aomps.vo_AddedOrganicMatterAmount = vo_AddedOrganicMatterAmount;
	aomps.vo_AOM_DryMatterContent = vo_AOM_DryMatterContent;
	aomps.vo_AOM_NH4Content = vo_AOM_NH4Content;
	aomps.vo_AOM_NO3Content = vo_AOM_NO3Content;
	aomps.vo_PartAOM_to_AOM_Slow = vo_PartAOM_to_AOM_Slow;
	aomps.vo_PartAOM_to_AOM_Fast = vo_PartAOM_to_AOM_Fast;

	newOM.push_back(aomps);
*/

	double AOM_SlowInput = vo_PartAOM_to_AOM_Slow * vo_AddedOrganicCarbonAmount;
	double AOM_FastInput = vo_PartAOM_to_AOM_Fast * vo_AddedOrganicCarbonAmount;

	double vo_SoilNH4Input = vo_AOM_NH4Content
		* vo_AddedOrganicMatterAmount
		* vo_AOM_DryMatterContent
		/ 10000.0
		/ soilColumn[0].vs_LayerThickness;

	double vo_SoilNO3Input = vo_AOM_NO3Content
		* vo_AddedOrganicMatterAmount
		* vo_AOM_DryMatterContent
		/ 10000.0
		/ soilColumn[0].vs_LayerThickness;

	double SOM_FastInput = (1.0 - (vo_PartAOM_to_AOM_Slow + vo_PartAOM_to_AOM_Fast))
		* vo_AddedOrganicCarbonAmount;
	// Immediate top layer pool update
	soilColumn[0].vo_AOM_Pool.back().vo_AOM_Slow += AOM_SlowInput;
	soilColumn[0].vo_AOM_Pool.back().vo_AOM_Fast += AOM_FastInput;
	soilColumn[0].vs_SoilNH4 += vo_SoilNH4Input;
	soilColumn[0].vs_SoilNO3 += vo_SoilNO3Input;
	soilColumn[0].vs_SOM_Fast += SOM_FastInput;

	//store for further use
	vo_AOM_SlowInput += AOM_SlowInput;
	vo_AOM_FastInput += AOM_FastInput;
	vo_SOM_FastInput += SOM_FastInput;

	addedOrganicMatter = true;
}

void SoilOrganic::addIrrigationWater(double amount)
{
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
		 vo_CN_Ratio_AOM_Fast = addedOrganicMatterParams->getVo_CN_Ratio_AOM_Fast();
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
		 vo_CN_Ratio_AOM_Fast = 1.0;
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
void SoilOrganic::fo_Urea(double vo_RainIrrigation)
{
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

	double po_HydrolysisKM = organicPs.po_HydrolysisKM;
	double po_HydrolysisP1 = organicPs.po_HydrolysisP1;
	double po_HydrolysisP2 = organicPs.po_HydrolysisP2;
	double po_ActivationEnergy = organicPs.po_ActivationEnergy;

	vo_NH3_Volatilised = 0.0;

	for(int i_Layer = 0; i_Layer < soilColumn.vs_NumberOfOrganicLayers(); i_Layer++)
	{

		// kmol urea m-3 soil
		vo_SoilCarbamid_solid[i_Layer] = soilColumn[i_Layer].vs_SoilCarbamid /
			OrganicConstants::po_UreaMolecularWeight /
			OrganicConstants::po_Urea_to_N / 1000.0;

		// mol urea kg Solution-1
		vo_SoilCarbamid_aq[i_Layer] = (-1258.9 + 13.2843 * (soilColumn[i_Layer].get_Vs_SoilTemperature() + 273.15) -
																	 0.047381 * ((soilColumn[i_Layer].get_Vs_SoilTemperature() + 273.15) *
																							 (soilColumn[i_Layer].get_Vs_SoilTemperature() + 273.15)) +
																	 5.77264e-5 * (pow((soilColumn[i_Layer].get_Vs_SoilTemperature() + 273.15), 3.0)));

		// kmol urea m-3 soil
		vo_SoilCarbamid_aq[i_Layer] = (vo_SoilCarbamid_aq[i_Layer] / (1.0 +
																																	(vo_SoilCarbamid_aq[i_Layer] * 0.0453))) *
			soilColumn[i_Layer].get_Vs_SoilMoisture_m3();

		if(vo_SoilCarbamid_aq[i_Layer] >= vo_SoilCarbamid_solid[i_Layer])
		{

			vo_SoilCarbamid_aq[i_Layer] = vo_SoilCarbamid_solid[i_Layer];
			vo_SoilCarbamid_solid[i_Layer] = 0.0;

		}
		else
		{
			vo_SoilCarbamid_solid[i_Layer] -= vo_SoilCarbamid_aq[i_Layer];
		}

		// Calculate urea hydrolysis

		vo_HydrolysisRate1[i_Layer] = (po_HydrolysisP1 *
																	 (soilColumn[i_Layer].vs_SoilOrganicMatter() * 100.0) *
																	 OrganicConstants::po_SOM_to_C + po_HydrolysisP2) /
			OrganicConstants::po_UreaMolecularWeight;

		vo_HydrolysisRate2[i_Layer] = vo_HydrolysisRate1[i_Layer] /
			(exp(-po_ActivationEnergy /
					 (8.314 * 310.0)));

		vo_HydrolysisRateMax[i_Layer] = vo_HydrolysisRate2[i_Layer] * exp(-po_ActivationEnergy /
																																			(8.314 * (soilColumn[i_Layer].get_Vs_SoilTemperature() + 273.15)));

		vo_Hydrolysis_pH_Effect[i_Layer] = exp(-0.064 *
																					 ((soilColumn[i_Layer].vs_SoilpH() - 6.5) *
																						(soilColumn[i_Layer].vs_SoilpH() - 6.5)));

		// kmol urea kg soil-1 s-1
		vo_HydrolysisRate[i_Layer] = vo_HydrolysisRateMax[i_Layer] *
			fo_MoistOnHydrolysis(soilColumn[i_Layer].vs_SoilMoisture_pF()) *
			vo_Hydrolysis_pH_Effect[i_Layer] * vo_SoilCarbamid_aq[i_Layer] /
			(po_HydrolysisKM + vo_SoilCarbamid_aq[i_Layer]);

		// kmol urea m soil-3 d-1
		vo_HydrolysisRate[i_Layer] = vo_HydrolysisRate[i_Layer] * 86400.0 *
			soilColumn[i_Layer].vs_SoilBulkDensity();

		if(vo_HydrolysisRate[i_Layer] >= vo_SoilCarbamid_aq[i_Layer])
		{

			soilColumn[i_Layer].vs_SoilNH4 += soilColumn[i_Layer].vs_SoilCarbamid;
			soilColumn[i_Layer].vs_SoilCarbamid = 0.0;

		}
		else
		{

			// kg N m soil-3
			soilColumn[i_Layer].vs_SoilCarbamid -= vo_HydrolysisRate[i_Layer] *
				OrganicConstants::po_UreaMolecularWeight *
				OrganicConstants::po_Urea_to_N * 1000.0;

			// kg N m soil-3
			soilColumn[i_Layer].vs_SoilNH4 += vo_HydrolysisRate[i_Layer] *
				OrganicConstants::po_UreaMolecularWeight *
				OrganicConstants::po_Urea_to_N * 1000.0;
		}

		// Calculate general volatilisation from NH4-Pool in top layer

		if(i_Layer == 0)
		{

			vo_H3OIonConcentration = pow(10.0, (-soilColumn[0].vs_SoilpH())); // kmol m-3
			vo_NH3aq_EquilibriumConst = pow(10.0, ((-2728.3 /
																							(soilColumn[0].get_Vs_SoilTemperature() + 273.15)) - 0.094219)); // K2 in Sadeghi's program

			vo_NH3_EquilibriumConst = pow(10.0, ((1630.5 /
																						(soilColumn[0].get_Vs_SoilTemperature() + 273.15)) - 2.301));  // K1 in Sadeghi's program

									// kmol m-3, assuming that all NH4 is solved
			vs_SoilNH4aq = soilColumn[0].vs_SoilNH4 / (OrganicConstants::po_NH4MolecularWeight * 1000.0);


			// kmol m-3
			vo_NH3aq = vs_SoilNH4aq / (1.0 + (vo_H3OIonConcentration / vo_NH3aq_EquilibriumConst));


			vo_NH3gas = vo_NH3aq;
			//  vo_NH3gas = vo_NH3aq / vo_NH3_EquilibriumConst;





			// kg N m-3 d-1
			vo_NH3_Volatilising = vo_NH3gas * OrganicConstants::po_NH3MolecularWeight * 1000.0;


			if(vo_NH3_Volatilising >= soilColumn[0].vs_SoilNH4)
			{

				vo_NH3_Volatilising = soilColumn[0].vs_SoilNH4;
				soilColumn[0].vs_SoilNH4 = 0.0;

			}
			else
			{
				soilColumn[0].vs_SoilNH4 -= vo_NH3_Volatilising;
			}

			// kg N m-2 d-1
			vo_NH3_Volatilised = vo_NH3_Volatilising * soilColumn[0].vs_LayerThickness;

		} // if (i_Layer == 0) {
	} // for

	// set incorporation to false, if carbamid part is falling below a treshold
	// only, if organic matter was not recently added
	if(vo_SoilCarbamid_aq[0] < 0.001 && !addedOrganicMatter)
	{
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
void SoilOrganic::fo_MIT()
{

	auto nools = soilColumn.vs_NumberOfOrganicLayers();
	double po_SOM_SlowDecCoeffStandard = organicPs.po_SOM_SlowDecCoeffStandard;
	double po_SOM_FastDecCoeffStandard = organicPs.po_SOM_FastDecCoeffStandard;
	double po_SMB_SlowDeathRateStandard = organicPs.po_SMB_SlowDeathRateStandard;
	double po_SMB_SlowMaintRateStandard = organicPs.po_SMB_SlowMaintRateStandard;
	double po_SMB_FastDeathRateStandard = organicPs.po_SMB_FastDeathRateStandard;
	double po_SMB_FastMaintRateStandard = organicPs.po_SMB_FastMaintRateStandard;
	double po_LimitClayEffect = organicPs.po_LimitClayEffect;
	double po_SOM_SlowUtilizationEfficiency = organicPs.po_SOM_SlowUtilizationEfficiency;
	double po_SOM_FastUtilizationEfficiency = organicPs.po_SOM_FastUtilizationEfficiency;
	double po_PartSOM_Fast_to_SOM_Slow = organicPs.po_PartSOM_Fast_to_SOM_Slow;
	double po_PartSMB_Slow_to_SOM_Fast = organicPs.po_PartSMB_Slow_to_SOM_Fast;
	double po_PartSMB_Fast_to_SOM_Fast = organicPs.po_PartSMB_Fast_to_SOM_Fast;
	double po_SMB_UtilizationEfficiency = organicPs.po_SMB_UtilizationEfficiency;
	double po_CN_Ratio_SMB = organicPs.po_CN_Ratio_SMB;
	double po_AOM_SlowUtilizationEfficiency = organicPs.po_AOM_SlowUtilizationEfficiency;
	double po_AOM_FastUtilizationEfficiency = organicPs.po_AOM_FastUtilizationEfficiency;
	double po_ImmobilisationRateCoeffNH4 = organicPs.po_ImmobilisationRateCoeffNH4;
	double po_ImmobilisationRateCoeffNO3 = organicPs.po_ImmobilisationRateCoeffNO3;

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

	for(int i_Layer = 0; i_Layer < nools; i_Layer++)
	{
		double tod = fo_TempOnDecompostion(soilColumn[i_Layer].get_Vs_SoilTemperature());
		double mod = fo_MoistOnDecompostion(soilColumn[i_Layer].vs_SoilMoisture_pF());

		vo_SOM_SlowDecCoeff[i_Layer] = po_SOM_SlowDecCoeffStandard * tod * mod;
		vo_SOM_FastDecCoeff[i_Layer] = po_SOM_FastDecCoeffStandard * tod * mod;
		vo_SOM_SlowDecRate[i_Layer] = vo_SOM_SlowDecCoeff[i_Layer] * soilColumn[i_Layer].vs_SOM_Slow;
		vo_SOM_FastDecRate[i_Layer] = vo_SOM_FastDecCoeff[i_Layer] * soilColumn[i_Layer].vs_SOM_Fast;

		vo_SMB_SlowMaintRateCoeff[i_Layer] = po_SMB_SlowMaintRateStandard
			* fo_ClayOnDecompostion(soilColumn[i_Layer].vs_SoilClayContent(),
															po_LimitClayEffect) * tod * mod;

		vo_SMB_FastMaintRateCoeff[i_Layer] = po_SMB_FastMaintRateStandard * tod * mod;

		vo_SMB_SlowMaintRate[i_Layer] = vo_SMB_SlowMaintRateCoeff[i_Layer] * soilColumn[i_Layer].vs_SMB_Slow;
		vo_SMB_FastMaintRate[i_Layer] = vo_SMB_FastMaintRateCoeff[i_Layer] * soilColumn[i_Layer].vs_SMB_Fast;
		vo_SMB_SlowDeathRateCoeff[i_Layer] = po_SMB_SlowDeathRateStandard * tod * mod;
		vo_SMB_FastDeathRateCoeff[i_Layer] = po_SMB_FastDeathRateStandard * tod * mod;
		vo_SMB_SlowDeathRate[i_Layer] = vo_SMB_SlowDeathRateCoeff[i_Layer] * soilColumn[i_Layer].vs_SMB_Slow;
		vo_SMB_FastDeathRate[i_Layer] = vo_SMB_FastDeathRateCoeff[i_Layer] * soilColumn[i_Layer].vs_SMB_Fast;

		vo_SMB_SlowDecRate[i_Layer] = vo_SMB_SlowDeathRate[i_Layer] + vo_SMB_SlowMaintRate[i_Layer];
		vo_SMB_FastDecRate[i_Layer] = vo_SMB_FastDeathRate[i_Layer] + vo_SMB_FastMaintRate[i_Layer];

		for(AOM_Properties& AOM_Pool : soilColumn[i_Layer].vo_AOM_Pool)
		{
			AOM_Pool.vo_AOM_SlowDecCoeff = AOM_Pool.vo_AOM_SlowDecCoeffStandard * tod * mod;
			AOM_Pool.vo_AOM_FastDecCoeff = AOM_Pool.vo_AOM_FastDecCoeffStandard * tod * mod;
		}
	} // for

	// Calculation of pool changes by decomposition
	for(int i_Layer = 0; i_Layer < nools; i_Layer++)
	{
		for(AOM_Properties& AOM_Pool : soilColumn[i_Layer].vo_AOM_Pool)
		{
			// Eq.6-5 and 6-6 in the DAISY manual
			AOM_Pool.vo_AOM_SlowDelta = -(AOM_Pool.vo_AOM_SlowDecCoeff * AOM_Pool.vo_AOM_Slow);

			if(-AOM_Pool.vo_AOM_SlowDelta > AOM_Pool.vo_AOM_Slow)
				AOM_Pool.vo_AOM_SlowDelta = (-AOM_Pool.vo_AOM_Slow);

			AOM_Pool.vo_AOM_FastDelta = -(AOM_Pool.vo_AOM_FastDecCoeff * AOM_Pool.vo_AOM_Fast);

			if(-AOM_Pool.vo_AOM_FastDelta > AOM_Pool.vo_AOM_Fast)
				AOM_Pool.vo_AOM_FastDelta = (-AOM_Pool.vo_AOM_Fast);
		}

		// Eq.6-7 in the DAISY manual
		vo_AOM_SlowDecRateSum[i_Layer] = 0.0;
				
		for(AOM_Properties& AOM_Pool : soilColumn[i_Layer].vo_AOM_Pool)
		{
			AOM_Pool.vo_AOM_SlowDecRate_to_SMB_Slow = AOM_Pool.vo_PartAOM_Slow_to_SMB_Slow
				* AOM_Pool.vo_AOM_SlowDecCoeff * AOM_Pool.vo_AOM_Slow;

			AOM_Pool.vo_AOM_SlowDecRate_to_SMB_Fast = AOM_Pool.vo_PartAOM_Slow_to_SMB_Fast
				* AOM_Pool.vo_AOM_SlowDecCoeff * AOM_Pool.vo_AOM_Slow;

			vo_AOM_SlowDecRateSum[i_Layer] += AOM_Pool.vo_AOM_SlowDecRate_to_SMB_Slow
				+ AOM_Pool.vo_AOM_SlowDecRate_to_SMB_Fast;
			
			AOMslow_to_SMBfast[i_Layer] += AOM_Pool.vo_AOM_SlowDecRate_to_SMB_Fast;
			AOMslow_to_SMBslow[i_Layer] += AOM_Pool.vo_AOM_SlowDecRate_to_SMB_Slow;
		}

		// Eq.6-8 in the DAISY manual
		vo_AOM_FastDecRateSum[i_Layer] = 0.0;

		AOMfast_to_SMBfast[i_Layer] = 0.0;

		for(AOM_Properties& AOM_Pool : soilColumn[i_Layer].vo_AOM_Pool)
		{
			//AOM_Pool.vo_AOM_FastDecRate_to_SMB_Slow = AOM_Pool.vo_PartAOM_Slow_to_SMB_Slow
			//	* AOM_Pool.vo_AOM_FastDecCoeff * AOM_Pool.vo_AOM_Fast;

			//AOM_Pool.vo_AOM_FastDecRate_to_SMB_Fast = AOM_Pool.vo_PartAOM_Slow_to_SMB_Fast
			//	* AOM_Pool.vo_AOM_FastDecCoeff * AOM_Pool.vo_AOM_Fast;

			AOM_Pool.vo_AOM_FastDecRate_to_SMB_Fast = AOM_Pool.vo_AOM_FastDecCoeff * AOM_Pool.vo_AOM_Fast;

			//vo_AOM_FastDecRateSum[i_Layer] += AOM_Pool.vo_AOM_FastDecRate_to_SMB_Slow
			//	+ AOM_Pool.vo_AOM_FastDecRate_to_SMB_Fast;
			vo_AOM_FastDecRateSum[i_Layer] += AOM_Pool.vo_AOM_FastDecRate_to_SMB_Fast;

			AOMfast_to_SMBfast[i_Layer] += AOM_Pool.vo_AOM_FastDecRate_to_SMB_Fast;
		}		
		
		vo_SMB_SlowDelta[i_Layer] = (po_SOM_SlowUtilizationEfficiency * vo_SOM_SlowDecRate[i_Layer])
			+ (po_SOM_FastUtilizationEfficiency * (1.0 - po_PartSOM_Fast_to_SOM_Slow) * vo_SOM_FastDecRate[i_Layer])
			//+ (po_AOM_SlowUtilizationEfficiency * vo_AOM_SlowDecRateSum[i_Layer])
			+ (po_AOM_SlowUtilizationEfficiency * AOMslow_to_SMBslow[i_Layer])
			//+ (po_AOM_FastUtilizationEfficiency * AOMfast_to_SMBslow)
			- vo_SMB_SlowDecRate[i_Layer];


		vo_SMB_FastDelta[i_Layer] = (po_SMB_UtilizationEfficiency *
																 (1.0 - po_PartSMB_Slow_to_SOM_Fast) * (vo_SMB_SlowDeathRate[i_Layer] + vo_SMB_FastDeathRate[i_Layer]))
			//+ (po_AOM_FastUtilizationEfficiency * vo_AOM_FastDecRateSum[i_Layer])
			+ (po_AOM_FastUtilizationEfficiency * AOMfast_to_SMBfast[i_Layer])
			+ (po_AOM_SlowUtilizationEfficiency * AOMslow_to_SMBfast[i_Layer])
			- vo_SMB_FastDecRate[i_Layer];

		//!Eq.6-9 in the DAISY manual
		vo_SOM_SlowDelta[i_Layer] = po_PartSOM_Fast_to_SOM_Slow * vo_SOM_FastDecRate[i_Layer]
			- vo_SOM_SlowDecRate[i_Layer];

		if((soilColumn[i_Layer].vs_SOM_Slow + vo_SOM_SlowDelta[i_Layer]) < 0.0)
			vo_SOM_SlowDelta[i_Layer] = soilColumn[i_Layer].vs_SOM_Slow;

		// Eq.6-10 in the DAISY manual
		//vo_SOM_FastDelta[i_Layer] = po_PartSMB_Slow_to_SOM_Fast
		//	* (vo_SMB_SlowDeathRate[i_Layer] + vo_SMB_FastDeathRate[i_Layer])
		//	- vo_SOM_FastDecRate[i_Layer];

		vo_SOM_FastDelta[i_Layer] = po_PartSMB_Slow_to_SOM_Fast * vo_SMB_SlowDeathRate[i_Layer]
			+ po_PartSMB_Fast_to_SOM_Fast * vo_SMB_FastDeathRate[i_Layer]
			- vo_SOM_FastDecRate[i_Layer];

		if((soilColumn[i_Layer].vs_SOM_Fast + vo_SOM_FastDelta[i_Layer]) < 0.0)
			vo_SOM_FastDelta[i_Layer] = soilColumn[i_Layer].vs_SOM_Fast;

		vo_AOM_SlowDeltaSum[i_Layer] = 0.0;
		vo_AOM_FastDeltaSum[i_Layer] = 0.0;

		for(AOM_Properties& AOM_Pool : soilColumn[i_Layer].vo_AOM_Pool)
		{
			vo_AOM_SlowDeltaSum[i_Layer] += AOM_Pool.vo_AOM_SlowDelta;
			vo_AOM_FastDeltaSum[i_Layer] += AOM_Pool.vo_AOM_FastDelta;
		}

	} // for i_Layer
	

	// Calculation of N balance
  //vo_CN_Ratio_SOM_Slow = siteParams.vs_Soil_CN_Ratio;
  //vo_CN_Ratio_SOM_Fast = siteParams.vs_Soil_CN_Ratio;

	for(int i_Layer = 0; i_Layer < nools; i_Layer++)
	{
    double vo_CN_Ratio_SOM_Slow = soilColumn.at(i_Layer).vs_Soil_CN_Ratio();
    double vo_CN_Ratio_SOM_Fast = vo_CN_Ratio_SOM_Slow;

		vo_NBalance[i_Layer] = -(vo_SMB_SlowDelta[i_Layer] / po_CN_Ratio_SMB)
			- (vo_SMB_FastDelta[i_Layer] / po_CN_Ratio_SMB)
			- (vo_SOM_SlowDelta[i_Layer] / vo_CN_Ratio_SOM_Slow)
			- (vo_SOM_FastDelta[i_Layer] / vo_CN_Ratio_SOM_Fast);

		vector<AOM_Properties>& AOM_Pool = soilColumn[i_Layer].vo_AOM_Pool;

		for(vector<AOM_Properties>::iterator it_AOM_Pool = AOM_Pool.begin(); it_AOM_Pool != AOM_Pool.end(); it_AOM_Pool++)
		{

			if(fabs(it_AOM_Pool->vo_CN_Ratio_AOM_Fast) >= 1.0E-7)
			{
				vo_NBalance[i_Layer] -= (it_AOM_Pool->vo_AOM_FastDelta / it_AOM_Pool->vo_CN_Ratio_AOM_Fast);
			} // if

			if(fabs(it_AOM_Pool->vo_CN_Ratio_AOM_Slow) >= 1.0E-7)
			{
				vo_NBalance[i_Layer] -= (it_AOM_Pool->vo_AOM_SlowDelta / it_AOM_Pool->vo_CN_Ratio_AOM_Slow);
			} // if
		} // for it_AOM_Pool
	} // for i_Layer

	// Check for Nmin availablity in case of immobilisation

	vo_NetNMineralisation = 0.0;

	
	for(int i_Layer = 0; i_Layer < nools; i_Layer++)
	{
    double vo_CN_Ratio_SOM_Slow = soilColumn.at(i_Layer).vs_Soil_CN_Ratio();
    double vo_CN_Ratio_SOM_Fast = vo_CN_Ratio_SOM_Slow;

		if(vo_NBalance[i_Layer] < 0.0)
		{			

			if(fabs(vo_NBalance[i_Layer]) >= ((soilColumn[i_Layer].vs_SoilNH4 * po_ImmobilisationRateCoeffNH4)
																				+ (soilColumn[i_Layer].vs_SoilNO3 * po_ImmobilisationRateCoeffNO3)))
			{
				vo_AOM_SlowDeltaSum[i_Layer] = 0.0;
				vo_AOM_FastDeltaSum[i_Layer] = 0.0;

				vector<AOM_Properties>& AOM_Pool = soilColumn[i_Layer].vo_AOM_Pool;

				for(vector<AOM_Properties>::iterator it_AOM_Pool = AOM_Pool.begin(); it_AOM_Pool != AOM_Pool.end(); it_AOM_Pool++)
				{

					if(it_AOM_Pool->vo_CN_Ratio_AOM_Slow >= (po_CN_Ratio_SMB
																									 / po_AOM_SlowUtilizationEfficiency))
					{

						it_AOM_Pool->vo_AOM_SlowDelta = 0.0;
						//correction of the fluxes across pools
						AOMslow_to_SMBfast[i_Layer] -= it_AOM_Pool->vo_AOM_SlowDecRate_to_SMB_Fast;
						AOMslow_to_SMBslow[i_Layer] -= it_AOM_Pool->vo_AOM_SlowDecRate_to_SMB_Slow;
					} // if

					if(it_AOM_Pool->vo_CN_Ratio_AOM_Fast >= (po_CN_Ratio_SMB
																									 / po_AOM_FastUtilizationEfficiency))
					{

						it_AOM_Pool->vo_AOM_FastDelta = 0.0;
						//correction of the fluxes across pools
						AOMfast_to_SMBfast[i_Layer] -= it_AOM_Pool->vo_AOM_FastDecRate_to_SMB_Fast;
					} // if

					vo_AOM_SlowDeltaSum[i_Layer] += it_AOM_Pool->vo_AOM_SlowDelta;
					vo_AOM_FastDeltaSum[i_Layer] += it_AOM_Pool->vo_AOM_FastDelta;

				} // for

				if(vo_CN_Ratio_SOM_Slow >= (po_CN_Ratio_SMB / po_SOM_SlowUtilizationEfficiency))
				{

					vo_SOM_SlowDelta[i_Layer] = 0.0;
				} // if

				if(vo_CN_Ratio_SOM_Fast >= (po_CN_Ratio_SMB / po_SOM_FastUtilizationEfficiency))
				{

					vo_SOM_FastDelta[i_Layer] = 0.0;
				} // if

				// Recalculation of SMB pool changes
				//@todo <b>Claas: </b> Folgende Algorithmen prüfen: Was verändert sich?
				vo_SMB_SlowDelta[i_Layer] = (po_SOM_SlowUtilizationEfficiency * vo_SOM_SlowDecRate[i_Layer])
					+ (po_SOM_FastUtilizationEfficiency * (1.0 - po_PartSOM_Fast_to_SOM_Slow) * vo_SOM_FastDecRate[i_Layer])
					//+ (po_AOM_SlowUtilizationEfficiency * vo_AOM_SlowDecRateSum[i_Layer])
					+ (po_AOM_SlowUtilizationEfficiency * AOMslow_to_SMBslow[i_Layer])
					//+ (po_AOM_FastUtilizationEfficiency * AOMfast_to_SMBslow)
					- vo_SMB_SlowDecRate[i_Layer];
				
				if((soilColumn[i_Layer].vs_SMB_Slow + vo_SMB_SlowDelta[i_Layer]) < 0.0)
				{
					vo_SMB_SlowDelta[i_Layer] = soilColumn[i_Layer].vs_SMB_Slow;
				}

				vo_SMB_FastDelta[i_Layer] = (po_SMB_UtilizationEfficiency *
																		 (1.0 - po_PartSMB_Slow_to_SOM_Fast) * (vo_SMB_SlowDeathRate[i_Layer] + vo_SMB_FastDeathRate[i_Layer]))
					//+ (po_AOM_FastUtilizationEfficiency * vo_AOM_FastDecRateSum[i_Layer])
					+ (po_AOM_FastUtilizationEfficiency * AOMfast_to_SMBfast[i_Layer])
					+ (po_AOM_SlowUtilizationEfficiency * AOMslow_to_SMBfast[i_Layer])
					- vo_SMB_FastDecRate[i_Layer];

				if((soilColumn[i_Layer].vs_SMB_Fast + vo_SMB_FastDelta[i_Layer]) < 0.0)
				{
					vo_SMB_FastDelta[i_Layer] = soilColumn[i_Layer].vs_SMB_Fast;
				}
				
				// Recalculation of N balance under conditions of immobilisation
				vo_NBalance[i_Layer] = -(vo_SMB_SlowDelta[i_Layer] / po_CN_Ratio_SMB)
					- (vo_SMB_FastDelta[i_Layer] / po_CN_Ratio_SMB) - (vo_SOM_SlowDelta[i_Layer]
																														 / vo_CN_Ratio_SOM_Slow) - (vo_SOM_FastDelta[i_Layer] / vo_CN_Ratio_SOM_Fast);

				for(vector<AOM_Properties>::iterator it_AOM_Pool =
						AOM_Pool.begin(); it_AOM_Pool != AOM_Pool.end(); it_AOM_Pool++)
				{

					if(fabs(it_AOM_Pool->vo_CN_Ratio_AOM_Fast) >= 1.0E-7)
					{

						vo_NBalance[i_Layer] -= (it_AOM_Pool->vo_AOM_FastDelta
																		 / it_AOM_Pool->vo_CN_Ratio_AOM_Fast);
					} // if

					if(fabs(it_AOM_Pool->vo_CN_Ratio_AOM_Slow) >= 1.0E-7)
					{

						vo_NBalance[i_Layer] -= (it_AOM_Pool->vo_AOM_SlowDelta
																		 / it_AOM_Pool->vo_CN_Ratio_AOM_Slow);
					} // if
				} // for

				// Update of Soil NH4 after recalculated N balance
				soilColumn[i_Layer].vs_SoilNH4 += fabs(vo_NBalance[i_Layer]);


			}
			else
			{ //if
			 // Bedarf kann durch Ammonium-Pool nicht gedeckt werden --> Nitrat wird verwendet
				if(fabs(vo_NBalance[i_Layer]) >= (soilColumn[i_Layer].vs_SoilNH4
																					* po_ImmobilisationRateCoeffNH4))
				{

					soilColumn[i_Layer].vs_SoilNO3 -= fabs(vo_NBalance[i_Layer])
						- (soilColumn[i_Layer].vs_SoilNH4
							 * po_ImmobilisationRateCoeffNH4);

					soilColumn[i_Layer].vs_SoilNH4 -= soilColumn[i_Layer].vs_SoilNH4
						* po_ImmobilisationRateCoeffNH4;

				}
				else
				{ // if

					soilColumn[i_Layer].vs_SoilNH4 -= fabs(vo_NBalance[i_Layer]);
				} //else
			} //else

		}
		else
		{ //if (N_Balance[i_Layer]) < 0.0

			soilColumn[i_Layer].vs_SoilNH4 += fabs(vo_NBalance[i_Layer]);
		}

		vo_NetNMineralisationRate[i_Layer] = fabs(vo_NBalance[i_Layer])
			* soilColumn[0].vs_LayerThickness; // [kg m-3] --> [kg m-2]
		vo_NetNMineralisation += fabs(vo_NBalance[i_Layer])
			* soilColumn[0].vs_LayerThickness; // [kg m-3] --> [kg m-2]
		vo_SumNetNMineralisation += fabs(vo_NBalance[i_Layer])
			* soilColumn[0].vs_LayerThickness; // [kg m-3] --> [kg m-2]

	}

	vo_DecomposerRespiration = 0.0;

	// Calculation of CO2 evolution
	for (int i_Layer = 0; i_Layer < nools; i_Layer++)
	{
		vo_SMB_SlowCO2EvolutionRate[i_Layer] = ((1.0 - po_SOM_SlowUtilizationEfficiency)
			* vo_SOM_SlowDecRate[i_Layer])
			+ ((1.0 - po_SOM_FastUtilizationEfficiency) * (1.0 - po_PartSOM_Fast_to_SOM_Slow)
				* vo_SOM_FastDecRate[i_Layer])
			//+ ((1.0 - po_AOM_SlowUtilizationEfficiency) * vo_AOM_SlowDecRateSum[i_Layer])
			+ ((1.0 - po_AOM_SlowUtilizationEfficiency) * AOMslow_to_SMBslow[i_Layer])
			+ vo_SMB_SlowMaintRate[i_Layer];

		vo_SMB_FastCO2EvolutionRate[i_Layer] = (1.0 - po_SMB_UtilizationEfficiency)
			* (((1.0 - po_PartSMB_Slow_to_SOM_Fast) * vo_SMB_SlowDeathRate[i_Layer])
				+ ((1.0 - po_PartSMB_Fast_to_SOM_Fast) * vo_SMB_FastDeathRate[i_Layer]))
			//+ ((1.0 - po_AOM_FastUtilizationEfficiency) * vo_AOM_FastDecRateSum[i_Layer])
			+ ((1.0 - po_AOM_SlowUtilizationEfficiency) * AOMslow_to_SMBfast[i_Layer])
			+ ((1.0 - po_AOM_FastUtilizationEfficiency) * AOMfast_to_SMBfast[i_Layer])
			+ vo_SMB_FastMaintRate[i_Layer];

		vo_SMB_CO2EvolutionRate[i_Layer] = vo_SMB_SlowCO2EvolutionRate[i_Layer] + vo_SMB_FastCO2EvolutionRate[i_Layer];

		vo_DecomposerRespiration += vo_SMB_CO2EvolutionRate[i_Layer] * soilColumn[i_Layer].vs_LayerThickness; // [kg C m-3] -> [kg C m-2]

	} // for i_Layer
}

/**
 * @brief Volatilisation
 *
 * This routine calculates NH3 loss after manure or slurry application
 * taken from the ALFAM model: Soegaard et al. 2002 (Atm. Environ. 36,
 * 3309-3319); Only cattle slurry broadcast application considered so
 * far.
 *
 * @param siteParams
 * @param vb_AOM_Incorporation
 * @param vo_AOM_Addition
 * @param vw_MeanAirTemperature
 * @param vw_WindSpeed
 */
void SoilOrganic::fo_Volatilisation(bool vo_AOM_Addition, double vw_MeanAirTemperature, double vw_WindSpeed)
{
	double vo_SoilWet;

	double vo_AOM_TAN_Content; // added organic matter total ammonium content [g N kg FM OM-1]
	double vo_MaxVolatilisation; // Maximum volatilisation [kg N ha-1 (kg N ha-1)-1]
	double vo_VolatilisationHalfLife; // [d]
	double vo_VolatilisationRate; // [kg N ha-1 (kg N ha-1)-1 d-1]
	double vo_N_PotVolatilised; // Potential volatilisation [kg N m-2]
	double vo_N_PotVolatilisedSum = 0.0; // Sums up potential volatilisation of all AOM pools [kg N m-2]
	double vo_N_ActVolatilised = 0.0; // Actual volatilisation [kg N m-2]

	int vo_DaysAfterApplicationSum = 0;

	if(soilColumn[0].vs_SoilMoisture_pF() > 2.5)
	{
		vo_SoilWet = 0.0;
	}
	else
	{
		vo_SoilWet = 1.0;
	}

	vector<AOM_Properties>& AOM_Pool = soilColumn[0].vo_AOM_Pool;
	for(vector<AOM_Properties>::iterator it_AOM_Pool = AOM_Pool.begin(); it_AOM_Pool != AOM_Pool.end(); it_AOM_Pool++)
	{

		vo_DaysAfterApplicationSum += it_AOM_Pool->vo_DaysAfterApplication;
	}

	if(vo_DaysAfterApplicationSum > 0 || vo_AOM_Addition)
	{

		/** @todo <b>Claas: </b> if (vo_AOM_Addition == true)
		 vo_DaysAfterApplication[vo_AOM_PoolAllocator]= 1; */

		vo_N_PotVolatilisedSum = 0.0;

		for(vector<AOM_Properties>::iterator it_AOM_Pool = AOM_Pool.begin(); it_AOM_Pool != AOM_Pool.end(); it_AOM_Pool++)
		{

			vo_AOM_TAN_Content = 0.0;
			vo_MaxVolatilisation = 0.0;
			vo_VolatilisationHalfLife = 0.0;
			vo_VolatilisationRate = 0.0;
			vo_N_PotVolatilised = 0.0;

			vo_AOM_TAN_Content = it_AOM_Pool->vo_AOM_NH4Content * 1000.0 * it_AOM_Pool->vo_AOM_DryMatterContent;

			vo_MaxVolatilisation = 0.0495*
				pow(1.1020, vo_SoilWet)*
				pow(1.0223, vw_MeanAirTemperature)*
				pow(1.0417, vw_WindSpeed)*
				pow(1.1080, it_AOM_Pool->vo_AOM_DryMatterContent)*
				pow(0.8280, vo_AOM_TAN_Content)*
				pow(11.300, double(it_AOM_Pool->incorporation));

			vo_VolatilisationHalfLife = 1.0380*
				pow(1.1020, vo_SoilWet)*
				pow(0.9600, vw_MeanAirTemperature)*
				pow(0.9500, vw_WindSpeed)*
				pow(1.1750, it_AOM_Pool->vo_AOM_DryMatterContent)*
				pow(1.1060, vo_AOM_TAN_Content)*
				pow(1.0000, double(it_AOM_Pool->incorporation))*
				(18869.3*exp(-soilColumn[0].vs_SoilpH() / 0.63321) + 0.70165);

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

		if(soilColumn[0].vs_SoilNH4 > (vo_N_PotVolatilisedSum))
		{
			vo_N_ActVolatilised = vo_N_PotVolatilisedSum;
		}
		else
		{
			vo_N_ActVolatilised = soilColumn[0].vs_SoilNH4;
		}

		// update NH4 content of top soil layer with volatilisation balance

		soilColumn[0].vs_SoilNH4 -= (vo_N_ActVolatilised / soilColumn[0].vs_LayerThickness);
	}
	else
	{
		vo_N_ActVolatilised = 0.0;
	}

	// NH3 volatilised from top layer NH4 pool. See Urea section
	vo_Total_NH3_Volatilised = (vo_N_ActVolatilised + vo_NH3_Volatilised); // [kg N m-2]
	/** @todo <b>Claas: </b>Zusammenfassung für output. Wohin damit??? */

	for(vector<AOM_Properties>::iterator it_AOM_Pool = AOM_Pool.begin(); it_AOM_Pool != AOM_Pool.end(); it_AOM_Pool++)
	{

		if(it_AOM_Pool->vo_DaysAfterApplication > 0 && !vo_AOM_Addition)
		{
			it_AOM_Pool->vo_DaysAfterApplication++;
		}
	}
}

/**
 * @brief Internal Subroutine Nitrification
 */
void SoilOrganic::fo_Nitrification()
{
	auto nools = soilColumn.vs_NumberOfOrganicLayers();
	double po_AmmoniaOxidationRateCoeffStandard = organicPs.po_AmmoniaOxidationRateCoeffStandard;
	double po_NitriteOxidationRateCoeffStandard = organicPs.po_NitriteOxidationRateCoeffStandard;

	//! Nitrification rate coefficient [d-1]
	std::vector<double> vo_AmmoniaOxidationRateCoeff(nools, 0.0);
	std::vector<double> vo_NitriteOxidationRateCoeff(nools, 0.0);

	//! Nitrification rate [kg NH4-N m-3 d-1]
	std::vector<double> vo_AmmoniaOxidationRate(nools, 0.0);
	std::vector<double> vo_NitriteOxidationRate(nools, 0.0);

	for(int i_Layer = 0; i_Layer < nools; i_Layer++)
	{

		// Calculate nitrification rate coefficients
//    cout << "SO-2:\t" << soilColumn[i_Layer].vs_SoilMoisture_pF() << endl;
		vo_AmmoniaOxidationRateCoeff[i_Layer] = po_AmmoniaOxidationRateCoeffStandard * fo_TempOnNitrification(
			soilColumn[i_Layer].get_Vs_SoilTemperature()) * fo_MoistOnNitrification(soilColumn[i_Layer].vs_SoilMoisture_pF());

		vo_AmmoniaOxidationRate[i_Layer] = vo_AmmoniaOxidationRateCoeff[i_Layer] * soilColumn[i_Layer].vs_SoilNH4;

		vo_NitriteOxidationRateCoeff[i_Layer] = po_NitriteOxidationRateCoeffStandard
			*fo_TempOnNitrification(soilColumn[i_Layer].get_Vs_SoilTemperature())
			*fo_MoistOnNitrification(soilColumn[i_Layer].vs_SoilMoisture_pF())
			*fo_NH3onNitriteOxidation(soilColumn[i_Layer].vs_SoilNH4, soilColumn[i_Layer].vs_SoilpH());

		vo_NitriteOxidationRate[i_Layer] = vo_NitriteOxidationRateCoeff[i_Layer] * soilColumn[i_Layer].vs_SoilNO2;

	}

	// Update NH4, NO2 and NO3 content with nitrification balance
	// Stange, F., C. Nendel (2014): N.N., in preparation


	for(int i_Layer = 0; i_Layer < nools; i_Layer++)
	{

		if(soilColumn[i_Layer].vs_SoilNH4 > vo_AmmoniaOxidationRate[i_Layer])
		{

			soilColumn[i_Layer].vs_SoilNH4 -= vo_AmmoniaOxidationRate[i_Layer];
			soilColumn[i_Layer].vs_SoilNO2 += vo_AmmoniaOxidationRate[i_Layer];


		}
		else
		{

			soilColumn[i_Layer].vs_SoilNO2 += soilColumn[i_Layer].vs_SoilNH4;
			soilColumn[i_Layer].vs_SoilNH4 = 0.0;
		}

		if(soilColumn[i_Layer].vs_SoilNO2 > vo_NitriteOxidationRate[i_Layer])
		{

			soilColumn[i_Layer].vs_SoilNO2 -= vo_NitriteOxidationRate[i_Layer];
			soilColumn[i_Layer].vs_SoilNO3 += vo_NitriteOxidationRate[i_Layer];


		}
		else
		{

			soilColumn[i_Layer].vs_SoilNO3 += soilColumn[i_Layer].vs_SoilNO2;
			soilColumn[i_Layer].vs_SoilNO2 = 0.0;
		}
	}
}

/**
 * @brief Denitrification
 */
void SoilOrganic::fo_Denitrification()
{
	auto nools = soilColumn.vs_NumberOfOrganicLayers();
	std::vector<double> vo_PotDenitrificationRate(nools, 0.0);
	double po_SpecAnaerobDenitrification = organicPs.po_SpecAnaerobDenitrification;
	double po_TransportRateCoeff = organicPs.po_TransportRateCoeff;
	vo_TotalDenitrification = 0.0;

	for(int i_Layer = 0; i_Layer < nools; i_Layer++)
	{

		//Temperature function is the same as in Nitrification subroutine
		vo_PotDenitrificationRate[i_Layer] = po_SpecAnaerobDenitrification
			* vo_SMB_CO2EvolutionRate[i_Layer]
			* fo_TempOnNitrification(soilColumn[i_Layer].get_Vs_SoilTemperature());

		vo_ActDenitrificationRate[i_Layer] = min(vo_PotDenitrificationRate[i_Layer]
																						 * fo_MoistOnDenitrification(soilColumn[i_Layer].get_Vs_SoilMoisture_m3(),
																																				 soilColumn[i_Layer].vs_Saturation()), po_TransportRateCoeff
																						 * soilColumn[i_Layer].vs_SoilNO3);
	}

	// update NO3 content of soil layer with denitrification balance [kg N m-3]

	for(int i_Layer = 0; i_Layer < nools; i_Layer++)
	{

		if(soilColumn[i_Layer].vs_SoilNO3 > vo_ActDenitrificationRate[i_Layer])
		{

			soilColumn[i_Layer].vs_SoilNO3 -= vo_ActDenitrificationRate[i_Layer];

		}
		else
		{

			vo_ActDenitrificationRate[i_Layer] = soilColumn[i_Layer].vs_SoilNO3;
			soilColumn[i_Layer].vs_SoilNO3 = 0.0;

		}

		vo_TotalDenitrification += vo_ActDenitrificationRate[i_Layer] * soilColumn[0].vs_LayerThickness; // [kg m-3] --> [kg m-2] ;
	}

	vo_SumDenitrification += vo_TotalDenitrification; // [kg N m-2]

}


/**
 * @brief N2O production
 */
void SoilOrganic::fo_N2OProduction()
{
	auto nools = soilColumn.vs_NumberOfOrganicLayers();
	std::vector<double> vo_N2OProduction(nools, 0.0);
	double po_N2OProductionRate = organicPs.po_N2OProductionRate;
	vo_N2O_Produced = 0.0;

	for(int i_Layer = 0; i_Layer < nools; i_Layer++)
	{

		// pKaHNO2 original concept pow10. We used pow2 to allow reactive HNO2 being available at higer pH values
		double pH_response = 1.0 / (1.0 + pow(2.0, soilColumn[i_Layer].vs_SoilpH() - OrganicConstants::po_pKaHNO2));

		vo_N2OProduction[i_Layer] = soilColumn[i_Layer].vs_SoilNO2
			* fo_TempOnNitrification(soilColumn[i_Layer].get_Vs_SoilTemperature())
			* po_N2OProductionRate
			* pH_response;

		vo_N2OProduction[i_Layer] *= soilColumn[i_Layer].vs_LayerThickness * 10000; //convert from kg N-N2O m-3 to kg N-N2O ha-1 (for each layer)

		vo_N2O_Produced += vo_N2OProduction[i_Layer];
	}

}


/**
 * @brief Internal Subroutine Pool update
 */
void SoilOrganic::fo_PoolUpdate()
{
	for(int i_Layer = 0; i_Layer < soilColumn.vs_NumberOfOrganicLayers(); i_Layer++)
	{
		vector<AOM_Properties>& AOM_Pool = soilColumn[i_Layer].vo_AOM_Pool;

		vo_AOM_SlowDeltaSum[i_Layer] = 0.0;
		vo_AOM_FastDeltaSum[i_Layer] = 0.0;
		vo_AOM_SlowSum[i_Layer] = 0.0;
		vo_AOM_FastSum[i_Layer] = 0.0;

		//for(vector<AOM_Properties>::iterator it_AOM_Pool = AOM_Pool.begin(); it_AOM_Pool != AOM_Pool.end(); it_AOM_Pool++)
		for(auto& AOM_props : AOM_Pool)
		{
			AOM_props.vo_AOM_Slow += AOM_props.vo_AOM_SlowDelta;
			AOM_props.vo_AOM_Fast += AOM_props.vo_AOM_FastDelta;

			vo_AOM_SlowDeltaSum[i_Layer] += AOM_props.vo_AOM_SlowDelta;
			vo_AOM_FastDeltaSum[i_Layer] += AOM_props.vo_AOM_FastDelta;

			vo_AOM_SlowSum[i_Layer] += AOM_props.vo_AOM_Slow;
			vo_AOM_FastSum[i_Layer] += AOM_props.vo_AOM_Fast;
		}

		soilColumn[i_Layer].vs_SOM_Slow += vo_SOM_SlowDelta[i_Layer];
		soilColumn[i_Layer].vs_SOM_Fast += vo_SOM_FastDelta[i_Layer];
		soilColumn[i_Layer].vs_SMB_Slow += vo_SMB_SlowDelta[i_Layer];
		soilColumn[i_Layer].vs_SMB_Fast += vo_SMB_FastDelta[i_Layer];

		if(i_Layer == 0)
		{
			vo_CBalance[i_Layer] = 
				vo_AOM_SlowInput 
				+ vo_AOM_FastInput 
				+ vo_AOM_SlowDeltaSum[i_Layer]
				+ vo_AOM_FastDeltaSum[i_Layer] 
				+ vo_SMB_SlowDelta[i_Layer]
				+ vo_SMB_FastDelta[i_Layer] 
				+ vo_SOM_SlowDelta[i_Layer]
				+ vo_SOM_FastDelta[i_Layer] 
				+ vo_SOM_FastInput;
		}
		else
		{
			vo_CBalance[i_Layer] = 
				vo_AOM_SlowDeltaSum[i_Layer]
				+ vo_AOM_FastDeltaSum[i_Layer] 
				+ vo_SMB_SlowDelta[i_Layer]
				+ vo_SMB_FastDelta[i_Layer] 
				+ vo_SOM_SlowDelta[i_Layer]
				+ vo_SOM_FastDelta[i_Layer];
		}

		// ([kg C kg-1] * [kg m-3]) - [kg C m-3]
		vo_SoilOrganicC[i_Layer] = 
			(soilColumn[i_Layer].vs_SoilOrganicCarbon()
			 * soilColumn[i_Layer].vs_SoilBulkDensity()) 
			- vo_InertSoilOrganicC[i_Layer]; 
		vo_SoilOrganicC[i_Layer] += vo_CBalance[i_Layer];

		// [kg C m-3] / [kg m-3] --> [kg C kg-1]
		soilColumn[i_Layer].set_SoilOrganicCarbon
			((vo_SoilOrganicC[i_Layer] + vo_InertSoilOrganicC[i_Layer]) 
			 / soilColumn[i_Layer].vs_SoilBulkDensity()); 

		// [kg C m-3] / [kg m-3] --> [kg C kg-1]
		//soilColumn[i_Layer].set_SoilOrganicMatter
		//	((vo_SoilOrganicC[i_Layer] + vo_InertSoilOrganicC[i_Layer])
		//	 / OrganicConstants::po_SOM_to_C
		//	 / soilColumn[i_Layer].vs_SoilBulkDensity());
	} 
}

/**
 * @brief Internal Function Clay effect on SOM decompostion
 * @param d_SoilClayContent
 * @param d_LimitClayEffect
 * @return
 */
double SoilOrganic::fo_ClayOnDecompostion(double d_SoilClayContent, double d_LimitClayEffect)
{
	double fo_ClayOnDecompostion = 0.0;

	if(d_SoilClayContent >= 0.0 && d_SoilClayContent <= d_LimitClayEffect)
	{
		fo_ClayOnDecompostion = 1.0 - 2.0 * d_SoilClayContent;
	}
	else if(d_SoilClayContent > d_LimitClayEffect && d_SoilClayContent <= 1.0)
	{
		fo_ClayOnDecompostion = 1.0 - 2.0 * d_LimitClayEffect;
	}
	else
	{
		vo_ErrorMessage = "irregular clay content";
	}
	return fo_ClayOnDecompostion;
}

/**
 * @brief Internal Function Temperature effect on SOM decompostion
 * @param d_SoilTemperature
 * @return
 */
double SoilOrganic::fo_TempOnDecompostion(double d_SoilTemperature)
{
	double fo_TempOnDecompostion = 0.0;

	if(d_SoilTemperature <= 0.0 && d_SoilTemperature > -40.0)
	{

		//
		fo_TempOnDecompostion = 0.0;

	}
	else if(d_SoilTemperature > 0.0 && d_SoilTemperature <= 20.0)
	{

		fo_TempOnDecompostion = 0.1 * d_SoilTemperature;

	}
	else if(d_SoilTemperature > 20.0 && d_SoilTemperature <= 70.0)
	{

		fo_TempOnDecompostion = exp(0.47 - (0.027 * d_SoilTemperature) + (0.00193 * d_SoilTemperature * d_SoilTemperature));
	}
	else
	{
		vo_ErrorMessage = "irregular soil temperature";
	}

	return fo_TempOnDecompostion;
}

/**
 * @brief Internal Function Moisture effect on SOM decompostion
 * @param d_SoilMoisture_pF
 * @return
 */
double SoilOrganic::fo_MoistOnDecompostion(double d_SoilMoisture_pF)
{
	double fo_MoistOnDecompostion = 0.0;

	if(fabs(d_SoilMoisture_pF) <= 1.0E-7)
	{
		//
		fo_MoistOnDecompostion = 0.6;

	}
	else if(d_SoilMoisture_pF > 0.0 && d_SoilMoisture_pF <= 1.5)
	{
		//
		fo_MoistOnDecompostion = 0.6 + 0.4 * (d_SoilMoisture_pF / 1.5);

	}
	else if(d_SoilMoisture_pF > 1.5 && d_SoilMoisture_pF <= 2.5)
	{
		//
		fo_MoistOnDecompostion = 1.0;

	}
	else if(d_SoilMoisture_pF > 2.5 && d_SoilMoisture_pF <= 6.5)
	{
		//
		fo_MoistOnDecompostion = 1.0 - ((d_SoilMoisture_pF - 2.5) / 4.0);

	}
	else if(d_SoilMoisture_pF > 6.5)
	{

		fo_MoistOnDecompostion = 0.0;

	}
	else
	{
		vo_ErrorMessage = "irregular soil water content";
	}

	return fo_MoistOnDecompostion;
}

/**
 * @brief Internal Function Moisture effect on urea hydrolysis
 * @param d_SoilMoisture_pF
 * @return
 */
double SoilOrganic::fo_MoistOnHydrolysis(double d_SoilMoisture_pF)
{
	double fo_MoistOnHydrolysis = 0.0;

	if(d_SoilMoisture_pF > 0.0 && d_SoilMoisture_pF <= 1.1)
	{
		fo_MoistOnHydrolysis = 0.72;

	}
	else if(d_SoilMoisture_pF > 1.1 && d_SoilMoisture_pF <= 2.4)
	{
		fo_MoistOnHydrolysis = 0.2207 * d_SoilMoisture_pF + 0.4672;

	}
	else if(d_SoilMoisture_pF > 2.4 && d_SoilMoisture_pF <= 3.4)
	{
		fo_MoistOnHydrolysis = 1.0;

	}
	else if(d_SoilMoisture_pF > 3.4 && d_SoilMoisture_pF <= 4.6)
	{
		fo_MoistOnHydrolysis = -0.8659 * d_SoilMoisture_pF + 3.9849;

	}
	else if(d_SoilMoisture_pF > 4.6)
	{
		fo_MoistOnHydrolysis = 0.0;

	}
	else
	{
		vo_ErrorMessage = "irregular soil water content";
	}

	return fo_MoistOnHydrolysis;
}

/**
 * @brief Internal Function Temperature effect on nitrification
 * @param d_SoilTemperature
 * @return
 */
double SoilOrganic::fo_TempOnNitrification(double d_SoilTemperature)
{
	double fo_TempOnNitrification = 0.0;

	if(d_SoilTemperature <= 2.0 && d_SoilTemperature > -40.0)
	{
		fo_TempOnNitrification = 0.0;

	}
	else if(d_SoilTemperature > 2.0 && d_SoilTemperature <= 6.0)
	{
		fo_TempOnNitrification = 0.15 * (d_SoilTemperature - 2.0);

	}
	else if(d_SoilTemperature > 6.0 && d_SoilTemperature <= 20.0)
	{
		fo_TempOnNitrification = 0.1 * d_SoilTemperature;

	}
	else if(d_SoilTemperature > 20.0 && d_SoilTemperature <= 70.0)
	{
		fo_TempOnNitrification
			= exp(0.47 - (0.027 * d_SoilTemperature) + (0.00193 * d_SoilTemperature * d_SoilTemperature));
	}
	else
	{
		vo_ErrorMessage = "irregular soil temperature";
	}

	return fo_TempOnNitrification;
}

/**
 * @brief Internal Function Moisture effect on nitrification
 * @param d_SoilMoisture_pF
 * @return
 */
double SoilOrganic::fo_MoistOnNitrification(double d_SoilMoisture_pF)
{
	double fo_MoistOnNitrification = 0.0;

	if(fabs(d_SoilMoisture_pF) <= 1.0E-7)
	{
		fo_MoistOnNitrification = 0.6;

	}
	else if(d_SoilMoisture_pF > 0.0 && d_SoilMoisture_pF <= 1.5)
	{
		fo_MoistOnNitrification = 0.6 + 0.4 * (d_SoilMoisture_pF / 1.5);

	}
	else if(d_SoilMoisture_pF > 1.5 && d_SoilMoisture_pF <= 2.5)
	{
		fo_MoistOnNitrification = 1.0;

	}
	else if(d_SoilMoisture_pF > 2.5 && d_SoilMoisture_pF <= 5.0)
	{
		fo_MoistOnNitrification = 1.0 - ((d_SoilMoisture_pF - 2.5) / 2.5);

	}
	else if(d_SoilMoisture_pF > 5.0)
	{
		fo_MoistOnNitrification = 0.0;

	}
	else
	{
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
double SoilOrganic::fo_MoistOnDenitrification(double d_SoilMoisture_m3, double d_Saturation)
{

	double po_Denit1 = organicPs.po_Denit1;
	double po_Denit2 = organicPs.po_Denit2;
	double po_Denit3 = organicPs.po_Denit3;
	double fo_MoistOnDenitrification = 0.0;

	if((d_SoilMoisture_m3 / d_Saturation) <= 0.8)
	{
		fo_MoistOnDenitrification = 0.0;

	}
	else if((d_SoilMoisture_m3 / d_Saturation) > 0.8 && (d_SoilMoisture_m3 / d_Saturation) <= 0.9)
	{

		fo_MoistOnDenitrification = po_Denit1 * ((d_SoilMoisture_m3 / d_Saturation)
																						 - po_Denit2) / (po_Denit3 - po_Denit2);

	}
	else if((d_SoilMoisture_m3 / d_Saturation) > 0.9 && (d_SoilMoisture_m3 / d_Saturation) <= 1.0)
	{

		fo_MoistOnDenitrification = po_Denit1 + (1.0 - po_Denit1)
			* ((d_SoilMoisture_m3 / d_Saturation) - po_Denit3) / (1.0 - po_Denit3);
	}
	else
	{
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
double SoilOrganic::fo_NH3onNitriteOxidation(double d_SoilNH4, double d_SoilpH)
{

	double po_Inhibitor_NH3 = organicPs.po_Inhibitor_NH3;
	double fo_NH3onNitriteOxidation = 0.0;

	fo_NH3onNitriteOxidation = po_Inhibitor_NH3 / (po_Inhibitor_NH3 + d_SoilNH4 * (1 - 1 / (1.0 + pow(10.0, d_SoilpH - OrganicConstants::po_pKaNH3))));

	return fo_NH3onNitriteOxidation;
}

/**
 * @brief Calculates Net ecosystem production [kg C ha-1 d-1].
 *
 */
double SoilOrganic::fo_NetEcosystemProduction(double d_NetPrimaryProduction, double d_DecomposerRespiration)
{

	double vo_NEP = 0.0;

	vo_NEP = d_NetPrimaryProduction - (d_DecomposerRespiration * 10000.0); // [kg C ha-1 d-1]

	return vo_NEP;
}


/**
 * @brief Calculates Net ecosystem production [kg C ha-1 d-1].
 *
 */
double SoilOrganic::fo_NetEcosystemExchange(double d_NetPrimaryProduction, double d_DecomposerRespiration)
{

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
double SoilOrganic::get_SoilOrganicC(int i_Layer) const
{
	return vo_SoilOrganicC[i_Layer] / soilColumn[i_Layer].vs_SoilBulkDensity();
}

/**
 * @brief Returns sum of AOM fast [kg C m-3]
 * @return Sum AOM fast
 */
double SoilOrganic::get_AOM_FastSum(int i_Layer) const
{
	return vo_AOM_FastSum[i_Layer];
}

/**
 * @brief Returns sum of AOM slow [kg C m-3]
 * @return Sum AOM slow
 */
double SoilOrganic::get_AOM_SlowSum(int i_Layer) const
{
	return vo_AOM_SlowSum[i_Layer];
}

/**
 * @brief Returns SMB fast [kg C m-3]
 * @return SMB fast
 */
double SoilOrganic::get_SMB_Fast(int i_Layer) const
{
	return soilColumn[i_Layer].vs_SMB_Fast;
}

/**
 * @brief Returns SMB slow [kg C m-3]
 * @return SMB slow
 */
double SoilOrganic::get_SMB_Slow(int i_Layer) const
{
	return soilColumn[i_Layer].vs_SMB_Slow;
}

/**
 * @brief Returns SOM fast [kg C m-3]
 * @return AOM fast
 */
double SoilOrganic::get_SOM_Fast(int i_Layer) const
{
	return soilColumn[i_Layer].vs_SOM_Fast;
}

/**
 * @brief Returns SOM slow [kg C m-3]
 * @return SOM slow
 */
double SoilOrganic::get_SOM_Slow(int i_Layer) const
{
	return soilColumn[i_Layer].vs_SOM_Slow;
}

/**
 * @brief Returns C balance [kg C m-3]
 * @return C balance
 */
double SoilOrganic::get_CBalance(int i_Layer) const
{
	return vo_CBalance[i_Layer];
}

/**
 * Returns SMB CO2 evolution rate at given layer.
 * @param i_layer
 * @return  SMB CO2 evolution rate
 */
double SoilOrganic::get_SMB_CO2EvolutionRate(int i_Layer) const
{
	return vo_SMB_CO2EvolutionRate.at(i_Layer);
}

/**
 * Returns actual denitrification rate in layer
 * @param i_Layer
 * @return denitrification rate [kg N m-3 d-1]
 */
double SoilOrganic::get_ActDenitrificationRate(int i_Layer) const
{
	return vo_ActDenitrificationRate.at(i_Layer);
}

/**
 * Returns actual N mineralisation rate in layer
 * @param i_Layer
 * @return N mineralisation rate [kg N ha-1 d-1]
 */
double SoilOrganic::get_NetNMineralisationRate(int i_Layer) const
{
	return vo_NetNMineralisationRate.at(i_Layer) * 10000.0;
}

/**
 * Returns cumulative total N mineralisation
 * @return Total N mineralistaion [kg N ha-1]
 */
double SoilOrganic::get_NetNMineralisation() const
{
	return vo_NetNMineralisation * 10000.0;
}

/**
 * Returns cumulative total N mineralisation
 * @return Total N mineralistaion [kg N ha-1]
 */
double SoilOrganic::get_SumNetNMineralisation() const
{
	return vo_SumNetNMineralisation * 10000.0;
}

/**
 * Returns cumulative total N denitrification
 * @return Total N denitrification [kg N ha-1]
 */
double SoilOrganic::get_SumDenitrification() const
{
	return vo_SumDenitrification * 10000.0;
}


/**
 * Returns N denitrification
 * @return N denitrification [kg N ha-1]
 */
double SoilOrganic::get_Denitrification() const
{
	return vo_TotalDenitrification * 10000.0;
}

/**
 * Returns NH3 volatilisation
 * @return NH3 volatilisation [kgN ha-1]
 */
double SoilOrganic::get_NH3_Volatilised() const
{
	return vo_Total_NH3_Volatilised * 10000.0;
}

/**
 * Returns cumulative total NH3 volatilisation
 * @return Total NH3 volatilisation [kg N ha-1]
 */
double SoilOrganic::get_SumNH3_Volatilised() const
{
	return vo_SumNH3_Volatilised * 10000.0;
}


/**
 * Returns N2O Production
 * @return N2O Production [kg N ha-1]
 */
double SoilOrganic::get_N2O_Produced() const
{
	return vo_N2O_Produced;
}


/**
 * Returns cumulative total N2O Production
 * @return Cumulative total N2O Production [kg N ha-1]
 */
double SoilOrganic::get_SumN2O_Produced() const
{
	return vo_SumN2O_Produced;
}


/**
 * Returns daily decomposer respiration
 * @return daily decomposer respiration [kg C ha-1 d-1]
 */
double SoilOrganic::get_DecomposerRespiration() const
{
	return vo_DecomposerRespiration * 10000.0;
}


/**
 * Returns daily net ecosystem production
 * @return daily net ecosystem production [kg C ha-1 d-1]
 */
double SoilOrganic::get_NetEcosystemProduction() const
{
	return vo_NetEcosystemProduction;
}


/**
 * Returns daily net ecosystem exchange
 * @return daily net ecosystem exchange [kg C ha-1 d-1]
 */
double SoilOrganic::get_NetEcosystemExchange() const
{
	return vo_NetEcosystemExchange;
}

void SoilOrganic::put_Crop(CropGrowth* c)
{
	crop = c;
}

void SoilOrganic::remove_Crop()
{
	crop = NULL;
}

double SoilOrganic::get_Organic_N(int i) const
{
	double orgN = 0;

	orgN += get_SMB_Fast(i) / organicPs.po_CN_Ratio_SMB;
	orgN += get_SMB_Slow(i) / organicPs.po_CN_Ratio_SMB;

	double cn = soilColumn.at(i).vs_Soil_CN_Ratio();
	orgN += get_SOM_Fast(i) / cn;
	orgN += get_SOM_Slow(i) / cn;

	for(const auto& aomp : soilColumn.at(i).vo_AOM_Pool)
	{
		orgN += aomp.vo_AOM_Fast / aomp.vo_CN_Ratio_AOM_Fast;
		orgN += aomp.vo_AOM_Slow / aomp.vo_CN_Ratio_AOM_Slow;
	}

	return orgN;
}
