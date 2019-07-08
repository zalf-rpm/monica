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

#include <cmath>
#include <algorithm>

/**
 * @file soilcolumn.cpp
 *
 * @brief This file contains the definition of classes AOM_Properties,  SoilLayer, SoilColumn
 *
 * @see Monica::AOM_Properties
 * @see Monica::SoilLayer
 * @see Monica::SoilColumn
 * @see Monica::FertilizerTriggerThunk
 */

#include "crop-growth.h"
#include "soilcolumn.h"
#include "tools/debug.h"
#include "soil/constants.h"

using namespace Monica;
using namespace std;
using namespace Soil;
using namespace Tools;


/**
 * Constructor
 * @param vs_LayerThickness Vertical expansion
 * @param sps Soil parameters
 */
SoilLayer::SoilLayer(double vs_LayerThickness,
	const SoilParameters& sps)
	: vs_LayerThickness(vs_LayerThickness)
	, vs_SoilNH4(sps.vs_SoilAmmonium)
	, vs_SoilNO3(sps.vs_SoilNitrate)
	, _sps(sps)
	, vs_SoilMoisture_m3(sps.vs_FieldCapacity * sps.vs_SoilMoisturePercentFC / 100.0)
	//, vs_SoilMoistureOld_m3(sps.vs_FieldCapacity * sps.vs_SoilMoisturePercentFC / 100.0)
{
}

/**
 * Soil layer's moisture content, expressed as logarithm of
 * pressure head in cm water column. Algorithm of Van Genuchten is used.
 * Conversion of water saturation into soil-moisture tension.
 *
 * @todo Einheiten prüfen
 */
double SoilLayer::vs_SoilMoisture_pF()
{
	// Derivation of Van Genuchten parameters (Vereecken at al. 1989)
	//TODO Einheiten prüfen
	double vs_ThetaR = vs_PermanentWiltingPoint();
	double vs_ThetaS = vs_Saturation();

	double vs_VanGenuchtenAlpha = exp(-2.486 + (2.5 * vs_SoilSandContent())
		- (35.1 * vs_SoilOrganicCarbon())
		- (2.617 * (vs_SoilBulkDensity() / 1000.0))
		- (2.3 * vs_SoilClayContent()));

	double vs_VanGenuchtenM = 1.0;

	double vs_VanGenuchtenN = exp(0.053
		- (0.9 * vs_SoilSandContent())
		- (1.3 * vs_SoilClayContent())
		+ (1.5 * (pow(vs_SoilSandContent(), 2.0))));

	//Van Genuchten retention curve
	double vs_MatricHead = get_Vs_SoilMoisture_m3() <= vs_ThetaR
		? 5.0E+7
		: vs_MatricHead = (1.0 / vs_VanGenuchtenAlpha)*
		(pow(pow((vs_ThetaS - vs_ThetaR) / (get_Vs_SoilMoisture_m3() - vs_ThetaR),
			1 / vs_VanGenuchtenM) - 1,
			1 / vs_VanGenuchtenN));

	//  debug() << "get_Vs_SoilMoisture_m3: " << get_Vs_SoilMoisture_m3() << std::endl;
	//  debug() << "vs_SoilSandContent: " << vs_SoilSandContent << std::endl;
	//  debug() << "vs_SoilOrganicCarbon: " << vs_SoilOrganicCarbon() << std::endl;
	//  debug() << "vs_SoilBulkDensity: " << vs_SoilBulkDensity() << std::endl;
	//  debug() << "that.vs_SoilClayContent: " << vs_SoilClayContent << std::endl;
	//  debug() << "vs_ThetaR: " << vs_ThetaR << std::endl;
	//  debug() << "vs_ThetaS: " << vs_ThetaS << std::endl;
	//  debug() << "vs_VanGenuchtenAlpha: " << vs_VanGenuchtenAlpha << std::endl;
	//  debug() << "vs_VanGenuchtenM: " << vs_VanGenuchtenM << std::endl;
	//  debug() << "vs_VanGenuchtenN: " << vs_VanGenuchtenN << std::endl;
	//  debug() << "vs_MatricHead: " << vs_MatricHead << std::endl;

	double soilMoisture_pF = log10(vs_MatricHead);

	/* JV! set _vs_SoilMoisture_pF to "small" number in case of vs_Theta "close" to vs_ThetaS (vs_Psi < 1 -> log(vs_Psi) < 0) */
	return soilMoisture_pF < 0.0 ? 5.0E-7 : soilMoisture_pF;
	//  debug() << "vs_SoilMoisture_pF: " << soilMoisture_pF << std::endl;
}


//------------------------------------------------------------------------------

/**
 * @brief Constructor
 *
 * Constructor with parameter initialization. Initializes every layer
 * in vector with the layer-thickness and special soil parameter in this layer.
 *
 * @param gps General Parameters
 * @param soilParams Soil Parameter
 */
SoilColumn::SoilColumn(double ps_LayerThickness,
	double ps_MaxMineralisationDepth,
	const SoilPMsPtr soilParams,
	double pm_CriticalMoistureDepth)
	: ps_MaxMineralisationDepth(ps_MaxMineralisationDepth)
	, pm_CriticalMoistureDepth(pm_CriticalMoistureDepth)
{
	debug() << "Constructor: SoilColumn " << (soilParams ? soilParams->size() : 0) << endl;
	if (soilParams)
		for (auto sp : *soilParams)
			push_back(SoilLayer(ps_LayerThickness, sp));

	_vs_NumberOfOrganicLayers = calculateNumberOfOrganicLayers();
}

/**
 * @brief Calculates number of organic layers.
 *
 * Calculates number of organic layers in dependency on
 * the layer depth and the ps_MaxMineralisationDepth. Result is saved
 * in private member variable _vs_NumberOfOrganicLayers.
 */
int SoilColumn::calculateNumberOfOrganicLayers()
{
	//std::cout << "--------- set_vs_NumberOfOrganicLayers -----------" << std::endl;
	double lsum = 0;
	int count = 0;
	for (int i = 0; i < vs_NumberOfLayers(); i++)
	{
		count++;
		lsum += at(i).vs_LayerThickness;

		if (lsum >= ps_MaxMineralisationDepth)
			break;
	}

	//std::cout << vs_NumberOfLayers() << std::endl;
	//std::cout << generalParams.ps_MaxMineralisationDepth << std::endl;
	//std::cout << _vs_NumberOfOrganicLayers << std::endl;

	return count;
}

double SoilColumn::applyMineralFertiliserViaNDemand(MineralFertiliserParameters fp,
	double demandDepth,
	double NdemandKgHa)
{
	double sumSoilNkgHa = 0.0;
	int depthCm = 0;
	int i = 0;
	for (const auto& layer : *this)
	{
		double layerSize = layer.vs_LayerThickness;
		depthCm += int(layerSize * 100.0);

		//convert [kg N m-3] to [kg N ha-1]
		sumSoilNkgHa += (at(i).vs_SoilNO3 + at(i).vs_SoilNH4) * 10000.0 * layerSize;

		if (depthCm >= int(demandDepth * 100))
			break;

		i++;
	}

	double fertilizerRecommendation = max(0.0, NdemandKgHa - sumSoilNkgHa);
	if (fertilizerRecommendation > 0)
		applyMineralFertiliser(fp, fertilizerRecommendation);

	return fertilizerRecommendation;
}

/**
 * Method for calculating fertilizer demand from crop demand and soil mineral
 * status (Nmin method).
 *
 * @param fp
 * @param vf_SamplingDepth
 * @param vf_CropNTarget N availability required by the crop down to rooting depth
 * @param vf_CropNTarget30 N availability required by the crop down to 30 cm
 * @param vf_FertiliserMaxApplication Maximal value of N that can be applied until the crop will be damaged
 * @param vf_FertiliserMinApplication Threshold value for economically reasonable fertilizer application
 * @param vf_TopDressingDelay Number of days for which the application of surplus fertilizer is delayed
 */
double SoilColumn::
applyMineralFertiliserViaNMinMethod(MineralFertiliserParameters fp,
	double vf_SamplingDepth,
	double vf_CropNTarget,
	double vf_CropNTarget30,
	double vf_FertiliserMinApplication,
	double vf_FertiliserMaxApplication,
	int vf_TopDressingDelay)
{
	if (at(0).get_Vs_SoilMoisture_m3() > at(0).vs_FieldCapacity())
	{
		_delayedNMinApplications.push_back([=]()
		{
			return this->applyMineralFertiliserViaNMinMethod(fp,
				vf_SamplingDepth,
				vf_CropNTarget,
				vf_CropNTarget30,
				vf_FertiliserMinApplication,
				vf_FertiliserMaxApplication,
				vf_TopDressingDelay);
		});

		debug() << "Soil too wet for fertilisation. Fertiliser event adjourned to next day." << endl;
		return 0.0;
	}

	int vf_Layer30cm = getLayerNumberForDepth(0.3);
	int layerSamplingDepth = getLayerNumberForDepth(vf_SamplingDepth);

	double vf_SoilNO3Sum = 0.0;
	double vf_SoilNH4Sum = 0.0;
	for (int i_Layer = 0; i_Layer < layerSamplingDepth /*(ceil(vf_SamplingDepth / at(i_Layer).vs_LayerThickness))*/; i_Layer++)
	{
		//vf_TargetLayer is in cm. We want number of layers
		vf_SoilNO3Sum += at(i_Layer).vs_SoilNO3; //! [kg N m-3]
		vf_SoilNH4Sum += at(i_Layer).vs_SoilNH4; //! [kg N m-3]
	}

	double vf_SoilNO3Sum30 = 0.0;
	double vf_SoilNH4Sum30 = 0.0;
	// Same calculation for a depth of 30 cm
  /** @todo Must be adapted when using variable layer depth. */
	for (int i_Layer = 0; i_Layer < vf_Layer30cm; i_Layer++)
	{
		vf_SoilNO3Sum30 += at(i_Layer).vs_SoilNO3; //! [kg N m-3]
		vf_SoilNH4Sum30 += at(i_Layer).vs_SoilNH4; //! [kg N m-3]
	}

	// Converts [kg N ha-1] to [kg N m-3]
	double vf_CropNTargetValue = vf_CropNTarget / 10000.0 / at(0).vs_LayerThickness;
	double vf_CropNTargetValue30 = vf_CropNTarget30 / 10000.0 / at(0).vs_LayerThickness;

	double vf_FertiliserDemandVol = vf_CropNTargetValue - (vf_SoilNO3Sum + vf_SoilNH4Sum);
	double vf_FertiliserDemandVol30 = vf_CropNTargetValue30 - (vf_SoilNO3Sum30 + vf_SoilNH4Sum30);

	// Converts fertiliser demand back from [kg N m-3] to [kg N ha-1]
	double vf_FertiliserDemand = vf_FertiliserDemandVol * 10000.0 * at(0).vs_LayerThickness;
	double vf_FertiliserDemand30 = vf_FertiliserDemandVol30 * 10000.0 * at(0).vs_LayerThickness;

	double vf_FertiliserRecommendation = max(vf_FertiliserDemand, vf_FertiliserDemand30);

	if (vf_FertiliserRecommendation < vf_FertiliserMinApplication)
	{
		// If the N demand of the crop is smaller than the user defined
		// minimum fertilisation then no need to fertilise
		vf_FertiliserRecommendation = 0.0;
	}

	if (vf_FertiliserRecommendation > vf_FertiliserMaxApplication)
	{
		// If the N demand of the crop is greater than the user defined
		// maximum fertilisation then need to split so surplus fertilizer can
		// be applied after a delay time
		_vf_TopDressing = vf_FertiliserRecommendation - vf_FertiliserMaxApplication;
		_vf_TopDressingPartition = fp;
		_vf_TopDressingDelay = vf_TopDressingDelay;
		vf_FertiliserRecommendation = vf_FertiliserMaxApplication;
	}

	//Apply fertiliser
	applyMineralFertiliser(fp, vf_FertiliserRecommendation);

	debug() << "SoilColumn::applyMineralFertiliserViaNMinMethod:\t" << vf_FertiliserRecommendation << endl;

	//apply the callback to all of the fertiliser, even though some if it
	//(the top-dressing) will only be applied later
	//we simply assume it really will be applied, in the worst case
	//the delay is so long, that the crop is already harvested until
	//the top-dressing will be applied
	return vf_FertiliserRecommendation;
}

// prüft ob top-dressing angewendet werden sollte, ansonsten wird
// zeitspanne nur reduziert

/**
 * Tests for every calculation step if a delayed fertilising should be applied.
 * If not, the delay time will be decremented. Otherwise the surplus fertiliser
 * stored in _vf_TopDressing is applied.
 *
 * @see ApplyFertiliser
 */
double SoilColumn::applyPossibleTopDressing()
{
	double amount = 0;

	if (_vf_TopDressingDelay > 0)
	{
		_vf_TopDressingDelay--;
	}
	else if (_vf_TopDressingDelay == 0
		&& _vf_TopDressing > 0.0)
	{
		amount = _vf_TopDressing;
		applyMineralFertiliser(_vf_TopDressingPartition, amount);
		_vf_TopDressing = 0;
	}
	return amount;
}

/**
 * Calls function for applying delayed fertilizer and
 * then removes the first fertilizer item in list.
 */
double SoilColumn::applyPossibleDelayedFerilizer() {
	list<std::function<double()> > delayedApps = _delayedNMinApplications;
	double n_amount = 0.0;
	while (!delayedApps.empty()) {
		n_amount += delayedApps.front()();
		delayedApps.pop_front();
		_delayedNMinApplications.pop_front();
	}
	return n_amount;
}

/**
 * @brief Applies mineral fertiliser
 *
 * @author: Claas Nendel
 */
void SoilColumn::applyMineralFertiliser(MineralFertiliserParameters fp,
	double amount) {
	debug() << "SoilColumn::applyMineralFertilser: params: " << fp.toString()
		<< " amount: " << amount << endl;
	// [kg N ha-1 -> kg m-3]
	double kgHaTokgm3 = 10000.0 * at(0).vs_LayerThickness;
	at(0).vs_SoilNO3 += amount * fp.getNO3() / kgHaTokgm3;
	at(0).vs_SoilNH4 += amount * fp.getNH4() / kgHaTokgm3;
	at(0).vs_SoilCarbamid += amount * fp.getCarbamid() / kgHaTokgm3;
}


/**
 * @brief Checks and deletes AOM pool
 *
 * This method checks the content of each AOM Pool. In case the sum over all
 * layers of a respective pool is very low the pool will be deleted from the
 * list.
 *
 * @author: Claas Nendel
 */
void SoilColumn::deleteAOMPool() {

	for (unsigned int i_AOMPool = 0; i_AOMPool < at(0).vo_AOM_Pool.size();) {

		double vo_SumAOM_Slow = 0.0;
		double vo_SumAOM_Fast = 0.0;

		for (int i_Layer = 0; i_Layer < _vs_NumberOfOrganicLayers; i_Layer++) {

			vo_SumAOM_Slow += at(i_Layer).vo_AOM_Pool.at(i_AOMPool).vo_AOM_Slow;
			vo_SumAOM_Fast += at(i_Layer).vo_AOM_Pool.at(i_AOMPool).vo_AOM_Fast;

		}

		//cout << "Pool " << i_AOMPool << " -> Slow: " << vo_SumAOM_Slow << "; Fast: " << vo_SumAOM_Fast << endl;

		if ((vo_SumAOM_Slow + vo_SumAOM_Fast) < 0.00001) {
			for (int i_Layer = 0; i_Layer < _vs_NumberOfOrganicLayers; i_Layer++) {
				vector<AOM_Properties>::iterator it_AOMPool = at(i_Layer).vo_AOM_Pool.begin();
				it_AOMPool += i_AOMPool;

				at(i_Layer).vo_AOM_Pool.erase(it_AOMPool);
			}
			//cout << "Habe Pool " << i_AOMPool << " gelöscht" << endl;
		}
		else {
			i_AOMPool++;
		}
	}
}

/**
 * Method for calculating irrigation demand from soil moisture status.
 * The trigger will be activated and deactivated according to crop parameters
 * (temperature sum)
 *
 * @param vi_IrrigationThreshold
 * @return could irrigation be applied
 */
bool SoilColumn::applyIrrigationViaTrigger(double vi_IrrigationThreshold,
	double vi_IrrigationAmount,
	double vi_IrrigationNConcentration)
{
	//is actually only called from cropStep and thus there should always
	//be a crop
	assert(cropGrowth != NULL);

	double s = cropGrowth->get_HeatSumIrrigationStart();
	double e = cropGrowth->get_HeatSumIrrigationEnd();
	double cts = cropGrowth->get_CurrentTemperatureSum();

	if (cts < s || cts > e)
		return false;

	double vi_CriticalMoistureDepth = pm_CriticalMoistureDepth;

	// Initialisation
	double vi_ActualPlantAvailableWater = 0.0;
	double vi_MaxPlantAvailableWater = 0.0;
	double vi_PlantAvailableWaterFraction = 0.0;

	int vi_CriticalMoistureLayer = int(ceil(vi_CriticalMoistureDepth /
		at(0).vs_LayerThickness));
	for (int i_Layer = 0; i_Layer < vi_CriticalMoistureLayer; i_Layer++)
	{
		vi_ActualPlantAvailableWater +=
			(at(i_Layer).get_Vs_SoilMoisture_m3() - at(i_Layer).vs_PermanentWiltingPoint())
			* vs_LayerThickness()
			* 1000.0; // [mm]
		vi_MaxPlantAvailableWater += (at(i_Layer).vs_FieldCapacity()
			- at(i_Layer).vs_PermanentWiltingPoint())
			* vs_LayerThickness() * 1000.0; // [mm]
		vi_PlantAvailableWaterFraction = vi_ActualPlantAvailableWater
			/ vi_MaxPlantAvailableWater; // []
	}
	if (vi_PlantAvailableWaterFraction <= vi_IrrigationThreshold)
	{
		applyIrrigation(vi_IrrigationAmount, vi_IrrigationNConcentration);

		debug() << "applying automatic irrigation treshold: " << vi_IrrigationThreshold
			<< " amount: " << vi_IrrigationAmount
			<< " N concentration: " << vi_IrrigationNConcentration << endl;

		return true;
	}

	return false;
}


/**
 * @brief Applies irrigation
 *
 * @author: Claas Nendel
 */
void SoilColumn::applyIrrigation(double vi_IrrigationAmount,
	double vi_IrrigationNConcentration)
{
	double vi_NAddedViaIrrigation = 0.0; //[kg m-3]

	// Adding irrigation water amount to surface water storage
	vs_SurfaceWaterStorage += vi_IrrigationAmount; // [mm]

	vi_NAddedViaIrrigation = vi_IrrigationNConcentration * // [mg dm-3]
		vi_IrrigationAmount / //[dm3 m-2]
		at(0).vs_LayerThickness / 1000000.0; // [m]
// [-> kg m-3]

// Adding N from irrigation water to top soil nitrate pool
	at(0).vs_SoilNO3 += vi_NAddedViaIrrigation;
}


/**
 * Applies tillage to effected layers. Parameters for effected soil layers
 * are averaged.
 * @param depth Depth of affected soil.
 */
void SoilColumn::applyTillage(double depth)
{
	int layer_index = getLayerNumberForDepth(depth) + 1;

	double soil_organic_carbon = 0.0;
	double soil_organic_matter = 0.0;
	double soil_temperature = 0.0;
	double soil_moisture = 0.0;
	//double soil_moistureOld = 0.0;
	double som_slow = 0.0;
	double som_fast = 0.0;
	double smb_slow = 0.0;
	double smb_fast = 0.0;
	double carbamid = 0.0;
	double nh4 = 0.0;
	double no2 = 0.0;
	double no3 = 0.0;

	// add up all parameters that are affected by tillage
	for (int i = 0; i < layer_index; i++)
	{
		soil_organic_carbon += at(i).vs_SoilOrganicCarbon();
		//soil_organic_matter += at(i).vs_SoilOrganicMatter();
		soil_temperature += at(i).get_Vs_SoilTemperature();
		soil_moisture += at(i).get_Vs_SoilMoisture_m3();
		//soil_moistureOld += at(i).vs_SoilMoistureOld_m3;
		som_slow += at(i).vs_SOM_Slow;
		som_fast += at(i).vs_SOM_Fast;
		smb_slow += at(i).vs_SMB_Slow;
		smb_fast += at(i).vs_SMB_Fast;
		carbamid += at(i).vs_SoilCarbamid;
		nh4 += at(i).vs_SoilNH4;
		no2 += at(i).vs_SoilNO2;
		no3 += at(i).vs_SoilNO3;
	}

	// calculate mean value of accumulated soil paramters
	soil_organic_carbon /= layer_index;
	//soil_organic_matter /= layer_index;
	soil_temperature /= layer_index;
	soil_moisture /= layer_index;
	//soil_moistureOld /= layer_index;
	som_slow /= layer_index;
	som_fast /= layer_index;
	smb_slow /= layer_index;
	smb_fast /= layer_index;
	carbamid /= layer_index;
	nh4 /= layer_index;
	no2 /= layer_index;
	no3 /= layer_index;

	// use calculated mean values for all affected layers
	for (int i = 0; i < layer_index; i++)
	{
		//assert((soil_organic_carbon - (soil_organic_matter * OrganicConstants::po_SOM_to_C)) < 0.00001);
		at(i).set_SoilOrganicCarbon(soil_organic_carbon);
		//at(i).set_SoilOrganicMatter(soil_organic_matter);
		at(i).set_Vs_SoilTemperature(soil_temperature);
		at(i).set_Vs_SoilMoisture_m3(soil_moisture);
		//at(i).vs_SoilMoistureOld_m3 = soil_moistureOld;
		at(i).vs_SOM_Slow = som_slow;
		at(i).vs_SOM_Fast = som_fast;
		at(i).vs_SMB_Slow = smb_slow;
		at(i).vs_SMB_Fast = smb_fast;
		at(i).vs_SoilCarbamid = carbamid;
		at(i).vs_SoilNH4 = nh4;
		at(i).vs_SoilNO2 = no2;
		at(i).vs_SoilNO3 = no3;
	}

	// merge aom pool
	size_t aom_pool_count = at(0).vo_AOM_Pool.size();

	if (aom_pool_count > 0)
	{
		vector<double> aom_slow(aom_pool_count);
		vector<double> aom_fast(aom_pool_count);

		// initialization of aom pool accumulator
		for (unsigned int pool_index = 0; pool_index < aom_pool_count; pool_index++)
		{
			aom_slow[pool_index] = 0.0;
			aom_fast[pool_index] = 0.0;
		}

		layer_index = min(layer_index, vs_NumberOfOrganicLayers());

		//cout << "Soil parameters before applying tillage for the first "<< layer_index+1 << " layers: " << endl;

		// add up pools for affected layer with same index
		for (int j = 0; j < layer_index; j++)
		{
			//cout << "Layer " << j << endl << endl;

			SoilLayer &layer = at(j);
			unsigned int pool_index = 0;
			for (auto aomp : layer.vo_AOM_Pool)
			{
				aom_slow[pool_index] += aomp.vo_AOM_Slow;
				aom_fast[pool_index] += aomp.vo_AOM_Fast;

				//cout << "AOMPool " << pool_index << endl;
				//cout << "vo_AOM_Slow:\t"<< aomp.vo_AOM_Slow << endl;
				//cout << "vo_AOM_Fast:\t"<< aomp.vo_AOM_Fast << endl;

				pool_index++;
			}
		}

		//
		for (unsigned int pool_index = 0; pool_index < aom_pool_count; pool_index++)
		{
			aom_slow[pool_index] = aom_slow[pool_index] / layer_index;
			aom_fast[pool_index] = aom_fast[pool_index] / layer_index;
		}

		//cout << "Soil parameters after applying tillage for the first "<< layer_index+1 << " layers: " << endl;

		// rewrite parameters of aom pool with mean values
		for (int j = 0; j < layer_index; j++)
		{
			SoilLayer& layer = at(j);
			//cout << "Layer " << j << endl << endl;
			unsigned int pool_index = 0;
			for (auto aomp : layer.vo_AOM_Pool)
			{
				aomp.vo_AOM_Slow = aom_slow[pool_index];
				aomp.vo_AOM_Fast = aom_fast[pool_index];

				//cout << "AOMPool " << pool_index << endl;
				//cout << "vo_AOM_Slow:\t"<< aomp.vo_AOM_Slow << endl;
				//cout << "vo_AOM_Fast:\t"<< aomp.vo_AOM_Fast << endl;

				pool_index++;
			}
		}
	}

	//cout << "soil_organic_carbon: " << soil_organic_carbon << endl;
	//cout << "soil_organic_matter: " << soil_organic_matter << endl;
	//cout << "soil_temperature: " << soil_temperature << endl;
	//cout << "soil_moisture: " << soil_moisture << endl;
	//cout << "soil_moistureOld: " << soil_moistureOld << endl;
	//cout << "som_slow: " << som_slow << endl;
	//cout << "som_fast: " << som_fast << endl;
	//cout << "smb_slow: " << smb_slow << endl;
	//cout << "smb_fast: " << smb_fast << endl;
	//cout << "carbamid: " << carbamid << endl;
	//cout << "nh4: " << nh4 << endl;
	//cout << "no3: " << no3 << endl << endl;

}

/**
 * @brief Returns index of layer that lays in the given depth.
 * @param depth Depth in meters
 * @return Index of layer
 */
int SoilColumn::getLayerNumberForDepth(double depth) const
{
	int layer = 0;
	double accu_depth = 0;
	double layer_thickness = at(0).vs_LayerThickness;

	// find number of layer that lay between the given depth
	for (size_t i = 0, _size = size(); i < _size; i++)
	{
		accu_depth += layer_thickness;
		if (depth <= accu_depth)
			break;
		layer++;
	}

	return layer;
}

/**
 * @brief Makes crop information available when needed.
 *
 * @return crop object
 */
void SoilColumn::put_Crop(CropGrowth* c) {
	cropGrowth = c;
}

/**
 * @brief Deletes crop object when not needed anymore.
 *
 * @return crop object is NULL
 */
void SoilColumn::remove_Crop() {
	cropGrowth = NULL;
}

/**
 * Returns sum of soiltemperature for several soil layers.
 * @param layers Number of layers that are of interest
 * @return Temperature sum
 */
double SoilColumn::sumSoilTemperature(int layers) const
{
	double accu = 0.0;
	for (int i = 0; i < layers; i++)
		accu += at(i).get_Vs_SoilTemperature();
	return accu;
}

//------------------------------------------------------------------------------

/**
 * @brief Constructor
 *
 * Parameter initialization.
 */
 /*
 FertilizerTriggerThunk::FertilizerTriggerThunk(
 MineralFertiliserPartition fp,
 SoilColumn* sc,
 double vf_SamplingDepth,
 double vf_CropNTargetValue,
 double vf_CropNTargetValue30,
 double vf_FertiliserMaxApplication,
 double vf_FertiliserMinApplication,
 int vf_TopDressingDelay)
 : _fp(fp), _sc(sc),
 _sd(vf_SamplingDepth),
 _cntv(vf_CropNTargetValue),
 _cntv30(vf_CropNTargetValue30),
 _fmaxa(vf_FertiliserMaxApplication),
 _fmina(vf_FertiliserMinApplication),
 _tdd(vf_TopDressingDelay)
 {

 }
  */

  /**
   * Starts applying of fertilizer in the soil column according
   * to specified type (nmin or manual).
   * @todo Frage an Micha: Warum wurd operator überladen anstatt eine methode zu implementieren?
   * @todo Micha: Das haut so nicht hin. Beide Fertiliser events haben unterschiedliche Parameter-Anforderungen!
   */
   /*
   void FertilizerTriggerThunk::operator()() const {
	 _sc->nMinFertiliserTrigger(_fp, _sd, _cntv, _cntv30, _fmaxa, _fmina, _tdd);
   }
	*/

