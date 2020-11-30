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

#ifndef MONICA_H_
#define MONICA_H_

#include <string>
#include <vector>
#include <list>
#include <utility>
#include <sstream>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <queue>
#include <set>

#include <kj/memory.h>
#include "monica/monica_state.capnp.h"

#include "climate/climate-common.h"
#include "soilcolumn.h"
#include "soiltemperature.h"
#include "soilmoisture.h"
#include "soilorganic.h"
#include "soiltransport.h"
#include "crop.h"
#include "tools/date.h"
#include "tools/datastructures.h"
#include "monica-parameters.h"
#include "tools/helper.h"
#include "soil/soil.h"
#include "soil/constants.h"
#include "../run/cultivation-method.h"


namespace Monica
{
	/* forward declaration */
	class Configuration;

	//----------------------------------------------------------------------------

	/*!
	 * @brief Core class of MONICA
	 * @author Claas Nendel, Michael Berg
	 */
	class MonicaModel
	{
	public:
		MonicaModel(const CentralParameterProvider& cpp);

		~MonicaModel() {}

		MonicaModel(mas::models::monica::MonicaModelState::Reader reader) { deserialize(reader); }

		void deserialize(mas::models::monica::MonicaModelState::Reader reader);

		void serialize(mas::models::monica::MonicaModelState::Builder builder);

		void step();
		
		void generalStep();
		
		void cropStep();

		double CO2ForDate(double year, double julianDay, bool isLeapYear);
		double CO2ForDate(Tools::Date);
		double GroundwaterDepthForDate(double maxGroundwaterDepth,
		                               double minGroundwaterDepth,
		                               int minGroundwaterDepthMonth,
		                               double julianday,
		                               bool leapYear);


		//! seed given crop
		void seedCrop(Crop* crop);

		//! what crop is currently seeded ?
		Crop* currentCrop() { return _currentCrop; }
		const Crop* currentCrop() const { return _currentCrop; }

		bool isCropPlanted() const { return _currentCrop && _currentCrop->isValid(); }

		//! harvest the currently seeded crop
		void harvestCurrentCrop(bool exported, Harvest::OptCarbonManagementData optCarbMgmtData = Harvest::OptCarbonManagementData());

		//! harvest the fruit of the current crop
		void fruitHarvestCurrentCrop(double percentage, bool exported);

		//! prune the leaves of the current crop
		void leafPruningCurrentCrop(double percentage, bool exported);

		//! prune the tips of the current crop
		void tipPruningCurrentCrop(double percentage, bool exported);

		//! prune the shoots of the current crop
		void shootPruningCurrentCrop(double percentage, bool exported);

		//! prune the shoots of the current crop
		void cuttingCurrentCrop(double percentage, bool exported);

		void incorporateCurrentCrop();

		void applyMineralFertiliser(MineralFertilizerParameters partition,
		                            double amount);

		void applyOrganicFertiliser(const OrganicMatterParametersPtr,
		                            double amountFM,
		                            bool incorporation);

		bool useNMinMineralFertilisingMethod() const
		{
			return _simPs->p_UseNMinMineralFertilisingMethod;
		}

		double applyMineralFertiliserViaNMinMethod
				(MineralFertilizerParameters partition, NMinCropParameters cropParams);

		double dailySumFertiliser() const { return _dailySumFertiliser; }

		void addDailySumFertiliser(double amount)
		{
			_dailySumFertiliser+=amount;
			_sumFertiliser +=amount;
		}

		double dailySumOrgFertiliser() const { return _dailySumOrgFertiliser; }

		void addDailySumOrgFertiliser(double amountFM, OrganicMatterParametersPtr params)
		{
			using namespace Soil;
			double AOM_fast_factor = OrganicConstants::po_AOM_to_C * params->vo_PartAOM_to_AOM_Fast / params->vo_CN_Ratio_AOM_Fast;
			double AOM_slow_factor = OrganicConstants::po_AOM_to_C * params->vo_PartAOM_to_AOM_Slow / params->vo_CN_Ratio_AOM_Slow;
			double SOM_Factor = (1 - (params->vo_PartAOM_to_AOM_Fast + params->vo_PartAOM_to_AOM_Slow)) * OrganicConstants::po_AOM_to_C / soilColumn().at(0).vs_Soil_CN_Ratio(); // TODO ask CN for correctness 

			double conversion = AOM_fast_factor + AOM_slow_factor + SOM_Factor + params->vo_AOM_NH4Content + params->vo_AOM_NO3Content;

			_dailySumOrgFertiliser += amountFM * params->vo_AOM_DryMatterContent * conversion;
			_sumOrgFertiliser += amountFM * params->vo_AOM_DryMatterContent * conversion;
		}

		double dailySumOrganicFertilizerDM() const { return _dailySumOrganicFertilizerDM; }
		double sumOrganicFertilizerDM() const { return _sumOrganicFertilizerDM; }

		void addDailySumOrganicFertilizerDM(double amountDM)
		{
			_dailySumOrganicFertilizerDM += amountDM;
			_sumOrganicFertilizerDM += amountDM;
		}

		double dailySumIrrigationWater() const { return _dailySumIrrigationWater; }

		void addDailySumIrrigationWater(double amount)
		{
			_dailySumIrrigationWater += amount;
		}

		double sumFertiliser() const { return _sumFertiliser; }
		double sumOrgFertiliser() const { return _sumOrgFertiliser; }

		void resetFertiliserCounter()
		{ 
			_sumFertiliser = 0; 
			_sumOrgFertiliser = 0;
			_sumOrganicFertilizerDM = 0;
		}

		void dailyReset();

		void applyIrrigation(double amount, 
												 double nitrateConcentration = 0,
		                     double sulfateConcentration = 0);

		void applyTillage(double depth);

		double get_AtmosphericCO2Concentration() const
		{
			return vw_AtmosphericCO2Concentration;
		}
		double get_AtmosphericO3Concentration() const
		{
			return vw_AtmosphericO3Concentration;
		}

		double get_GroundwaterDepth() const { return vs_GroundwaterDepth; }

		double avgCorg(double depth_m) const;
		double mean90cmWaterContent() const;
		double meanWaterContent(int layer, int number_of_layers) const;
		double sumNmin(double depth_m) const;
		double groundWaterRecharge() const;
		double nLeaching() const;
		double sumSoilTemperature(int layers) const;
		double sumNO3AtDay(double depth) const;
		double maxSnowDepth() const;
		double getAccumulatedSnowDepth() const;
		double getAccumulatedFrostDepth() const;
		double avg30cmSoilTemperature() const;
		double avgSoilMoisture(size_t startLayer, size_t endLayerInclusive) const;
		double avgCapillaryRise(int start_layer, int end_layer) const;
		double avgPercolationRate(int start_layer, int end_layer) const;
		double sumSurfaceRunOff() const;
		double surfaceRunoff() const;
		double getEvapotranspiration() const;
		double getTranspiration() const;
		double getEvaporation() const;
		double get_sum30cmSMB_CO2EvolutionRate() const;
		double getNH3Volatilised() const;
		double getSumNH3Volatilised() const;
		double getsum30cmActDenitrificationRate() const;
		double getETa() const;
		
		const SoilTemperature& soilTemperature() const { return *_soilTemperature; }
		SoilTemperature& soilTemperatureNC() { return *_soilTemperature; }

		const SoilMoisture& soilMoisture() const { return *_soilMoisture; }
		SoilMoisture& soilMoistureNC() { return *_soilMoisture; }

		const SoilOrganic& soilOrganic() const { return *_soilOrganic; }
		SoilOrganic& soilOrganicNC() { return *_soilOrganic; }

		const SoilTransport& soilTransport() const { return *_soilTransport; }
		SoilTransport& soilTransportNC() { return *_soilTransport; }

		const SoilColumn& soilColumn() const { return *_soilColumn; }
		SoilColumn& soilColumnNC() { return *_soilColumn; }

		CropModule* cropGrowth() { return _currentCropModule.get(); }
		const CropModule* cropGrowth() const { return _currentCropModule.get(); }

		double netRadiation(double globrad) { return globrad * (1 - _envPs->p_Albedo); }

		int daysWithCrop() const {return p_daysWithCrop; }
		double getAccumulatedNStress() const { return p_accuNStress; }
		double getAccumulatedWaterStress() const { return p_accuWaterStress; }
		double getAccumulatedHeatStress() const { return p_accuHeatStress; }
		double getAccumulatedOxygenStress() const { return p_accuOxygenStress; }

		const SiteParameters& siteParameters() const { return *_sitePs; }
		const EnvironmentParameters& environmentParameters() const { return *_envPs; }
		const CropModuleParameters& cropParameters() const { return *_cropPs; }
		const SimulationParameters& simulationParameters() const { return *_simPs; }
		SimulationParameters& simulationParametersNC() { return *_simPs; }

		Tools::Date currentStepDate() const { return _currentStepDate; }
		void setCurrentStepDate(Tools::Date d) { _currentStepDate = d; }

		const std::map<Climate::ACD, double>& currentStepClimateData() const
		{
			return _climateData.back();
		}
		void setCurrentStepClimateData(const std::map<Climate::ACD, double>& cd) { _climateData.push_back(cd); }
		
		const std::vector<std::map<Climate::ACD, double>>& climateData() const { return _climateData; }

		void addEvent(std::string e) { _currentEvents.insert(e); }
		void clearEvents();
		const std::set<std::string>& currentEvents() const { return _currentEvents; }
		const std::set<std::string>& previousDaysEvents() const { return _previousDaysEvents; }
		
		int cultivationMethodCount() const { return _cultivationMethodCount; }

		double optCarbonExportedResidues() const { return _optCarbonExportedResidues; }
		double optCarbonReturnedResidues() const { return _optCarbonReturnedResidues; }
		double humusBalanceCarryOver() const { return _humusBalanceCarryOver; }

	private:
		SiteParameters _sitePs;
		EnvironmentParameters _envPs;
		kj::Own<CropModuleParameters> _cropPs;
		SimulationParameters _simPs;
		MeasuredGroundwaterTableInformation _groundwaterInformation;

		SoilColumn _soilColumn; //!< main soil data structure
		SoilTemperature _soilTemperature; //!< temperature code
		SoilMoisture _soilMoisture; //!< moisture code
		SoilOrganic _soilOrganic; //!< organic code
		SoilTransport _soilTransport; //!< transport code
		Crop* _currentCrop{ nullptr }; //! currently possibly planted crop
		kj::Own<CropModule> _currentCropModule; //!< crop code for possibly planted crop

		//! store applied fertiliser during one production process
		double _sumFertiliser{0.0}; //mineral N
		double _sumOrgFertiliser{ 0.0 }; //organic N

		//! stores the daily sum of applied fertiliser
		double _dailySumFertiliser{0.0}; //mineral N
		double _dailySumOrgFertiliser{ 0.0 }; //organic N

		double _dailySumOrganicFertilizerDM{0.0};
		double _sumOrganicFertilizerDM{0.0};

		double _humusBalanceCarryOver{0.0};

		double _dailySumIrrigationWater{0.0};

		double _optCarbonExportedResidues{0.0};
		double _optCarbonReturnedResidues{0.0};

		Tools::Date _currentStepDate;
		std::vector<std::map<Climate::ACD, double>> _climateData;
		std::set<std::string> _currentEvents;
		std::set<std::string> _previousDaysEvents;

		bool _clearCropUponNextDay{false};

		int p_daysWithCrop{0};
		double p_accuNStress{0.0};
		double p_accuWaterStress{0.0};
		double p_accuHeatStress{0.0};
		double p_accuOxygenStress{0.0};

		double vw_AtmosphericCO2Concentration{0.0};
		double vw_AtmosphericO3Concentration{ 0.0 };
		double vs_GroundwaterDepth{0.0};

		int _cultivationMethodCount{0};

    public:
      uint critPos{ 0 };
      uint cmitPos{ 0 };
	};
}

#endif

