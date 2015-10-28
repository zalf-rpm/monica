/**
Authors: 
Dr. Claas Nendel <claas.nendel@zalf.de>
Xenia Specka <xenia.specka@zalf.de>
Michael Berg <michael.berg@zalf.de>

Maintainers: 
Currently maintained by the authors.

This file is part of the MONICA model. 
Copyright (C) 2007-2013, Leibniz Centre for Agricultural Landscape Research (ZALF)

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
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

    ~MonicaModel();

    void generalStep(Tools::Date date, std::map<Climate::ACD, double> climateData);
    void generalStep(unsigned int stepNo);

    void cropStep(Tools::Date date, std::map<Climate::ACD, double> climateData);
    void cropStep(unsigned int stepNo);

    double CO2ForDate(double year, double julianDay, bool isLeapYear);
    double CO2ForDate(Tools::Date);
    double GroundwaterDepthForDate(double maxGroundwaterDepth,
                                   double minGroundwaterDepth,
                                   int minGroundwaterDepthMonth,
                                   double julianday,
                                   bool leapYear);


    //! seed given crop
    void seedCrop(CropPtr crop);

    //! what crop is currently seeded ?
		CropPtr currentCrop() const { return _currentCrop; }

    bool isCropPlanted() const { return _currentCrop.get() && _currentCrop->isValid(); }

    //! harvest the currently seeded crop
		void harvestCurrentCrop(bool exported);
		
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

    void applyMineralFertiliser(MineralFertiliserParameters partition,
                                double amount);

    void applyOrganicFertiliser(const OrganicMatterParametersPtr,
                                double amount,
                                bool incorporation);

		bool useNMinMineralFertilisingMethod() const
		{
      return _envPs.p_UseNMinMineralFertilisingMethod;
    }

    double applyMineralFertiliserViaNMinMethod
        (MineralFertiliserParameters partition, NMinCropParameters cropParams);

		double dailySumFertiliser() const { return _dailySumFertiliser; }

		void addDailySumFertiliser(double amount)
		{
      _dailySumFertiliser+=amount;
      _sumFertiliser +=amount;
    }

		double dailySumIrrigationWater() const { return _dailySumIrrigationWater; }

		void addDailySumIrrigationWater(double amount)
		{
      _dailySumIrrigationWater += amount;
    }

		double sumFertiliser() const { return _sumFertiliser; }

		void resetFertiliserCounter(){ _sumFertiliser = 0; }

		void resetDailyCounter()
		{
      _dailySumIrrigationWater = 0.0;
      _dailySumFertiliser = 0.0;
    }

    void applyIrrigation(double amount, double nitrateConcentration = 0,
                         double sulfateConcentration = 0);

    void applyTillage(double depth);

		double get_AtmosphericCO2Concentration() const
		{
      return vw_AtmosphericCO2Concentration;
    }

		double get_GroundwaterDepth() const { return vs_GroundwaterDepth; }

    bool writeOutputFiles() {return _writeOutputFiles; }

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
    double avgSoilMoisture(int start_layer, int end_layer) const;
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


    /**
     * @brief Returns soil temperature
     * @return temperature
     */
    const SoilTemperature& soilTemperature() const { return _soilTemperature; }

    /**
     * @brief Returns soil moisture.
     * @return Moisture
     */
    const SoilMoisture& soilMoisture() const { return _soilMoisture; }

    /**
     * @brief Returns soil organic mass.
     * @return soil organic
     */
    const SoilOrganic& soilOrganic() const { return _soilOrganic; }

    /**
     * @brief Returns soil transport
     * @return soil transport
     */
    const SoilTransport& soilTransport() const { return _soilTransport; }

    /**
     * @brief Returns soil column
     * @return soil column
     */
    const SoilColumn& soilColumn() const { return _soilColumn; }

		SoilColumn& soilColumnNC() { return _soilColumn; }

    /**
     * @brief returns value for current crop.
     * @return crop growth
     */
		CropGrowth* cropGrowth() { return _currentCropGrowth; }

    /**
     * @brief Returns net radiation.
     * @param globrad
     * @return radiation
     */
    double netRadiation(double globrad) { return globrad * (1 - _envPs.p_Albedo); }

    int daysWithCrop() const {return p_daysWithCrop; }
    double getAccumulatedNStress() const { return p_accuNStress; }
    double getAccumulatedWaterStress() const { return p_accuWaterStress; }
    double getAccumulatedHeatStress() const { return p_accuHeatStress; }
    double getAccumulatedOxygenStress() const { return p_accuOxygenStress; }

    const SiteParameters& siteParameters() const { return _sitePs; }

    const UserSoilMoistureParameters& soilmoistureParameters() const { return _smPs; }
    const UserEnvironmentParameters& environmentParameters() const { return _envPs; }
    const UserCropParameters& cropParameters() const { return _cropPs; }
    const UserSoilTemperatureParameters& soilTemperatureParameters() const { return _soilTempPs; }
    const UserSoilTransportParameters& soilTransportParameters() const { return _soilTransPs; }
    const UserSoilOrganicParameters& soilOrganicParameters() const { return _soilOrganicPs; }

  private:
    const SiteParameters _sitePs;
    const UserSoilMoistureParameters _smPs;
    const UserEnvironmentParameters _envPs;
    const UserCropParameters _cropPs;
    const UserSoilTemperatureParameters& _soilTempPs;
    const UserSoilTransportParameters& _soilTransPs;
    const UserSoilOrganicParameters& _soilOrganicPs;
    const UserInitialValues& _initPs;
    bool _writeOutputFiles{false};
    std::string _pathToOutputDir;
    MeasuredGroundwaterTableInformation _groundwaterInformation;

    SoilColumn _soilColumn; //!< main soil data structure
    SoilTemperature _soilTemperature; //!< temperature code
    SoilMoisture _soilMoisture; //!< moisture code
    SoilOrganic _soilOrganic; //!< organic code
    SoilTransport _soilTransport; //!< transport code
    CropPtr _currentCrop; //! currently possibly planted crop
    CropGrowth* _currentCropGrowth{nullptr}; //!< crop code for possibly planted crop

		//! store applied fertiliser during one production process
    double _sumFertiliser{0.0};

		//! stores the daily sum of applied fertiliser
    double _dailySumFertiliser{0.0};

    double _dailySumIrrigationWater{0.0};

		//! climate data available to the model
    Climate::DataAccessor _dataAccessor;

    int p_daysWithCrop{0};
    double p_accuNStress{0.0};
    double p_accuWaterStress{0.0};
    double p_accuHeatStress{0.0};
    double p_accuOxygenStress{0.0};

    double vw_AtmosphericCO2Concentration;
    double vs_GroundwaterDepth{0.0};
  };
  
}

#endif

