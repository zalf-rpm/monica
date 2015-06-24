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

#include "zmq.hpp"

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

  /**
   * @class Env
   */
  struct Env 
  {
		enum
		{
			MODE_LC_DSS = 0,
			MODE_ACTIVATE_OUTPUT_FILES,
			MODE_HERMES,
			MODE_EVA2,
			MODE_SENSITIVITY_ANALYSIS,
			MODE_CC_GERMANY,
			MODE_MACSUR_SCALING, 
			MODE_MACSUR_SCALING_CALIBRATION,
			MODE_CARBIOCIAL_CLUSTER
		};

    Env() {}

    Env(const Env&);
		    
    /**
     * @brief Constructor
     * @param sps Soil parameters
     */
    Env(const Soil::SoilPMs* sps, CentralParameterProvider cpp);

    Env(Soil::SoilPMsPtr sps, CentralParameterProvider cpp);

    //Interface method for python wrapping. Simply returns number
    //of possible simulation steps according to avaible climate data.
    int numberOfPossibleSteps() const { return da.noOfStepsPossible(); }

    void addOrReplaceClimateData(std::string, const std::vector<double>& data);

	private:
    Soil::SoilPMsPtr _soilParamsPtr;
	public:
    //! a vector of soil parameter objects = layers of soil
    const Soil::SoilPMs* soilParams{nullptr};

    size_t noOfLayers{0};
    double layerThickness{0.0};

    bool useNMinMineralFertilisingMethod{false};
    MineralFertiliserParameters nMinFertiliserPartition;
    NMinUserParameters nMinUserParams;

    bool useAutomaticIrrigation{false};
    AutomaticIrrigationParameters autoIrrigationParams;
    MeasuredGroundwaterTableInformation groundwaterInformation;

    //! tell if farmer uses the secondary yield products
    bool useSecondaryYields{true};

    //static const int depthGroundwaterTable;   /**<  */
    double windSpeedHeight{0}; /**<  */
    double atmosphericCO2{-1}; /**< [ppm] */
    double albedo{0}; /**< Albedo [] */

    //! object holding the climate data
    Climate::DataAccessor da;

    //! vector of elements holding the data of the single crops in the rotation
    std::vector<ProductionProcess> cropRotation;

//    Tools::GridPoint gridPoint;        //! the gridpoint the model runs, just a convenience for the dss use
    int customId{-1};

    SiteParameters site;        //! site specific parameters
    GeneralParameters general;  //! general parameters to the model
    Soil::OrganicConstants organic;  //! constant organic parameters to the model

    CentralParameterProvider centralParameterProvider;

    std::string toString() const;
		std::string pathToOutputDir;

    //Set execution mode of Monica.
    //Disables debug outputs for some modes.
    void setMode(int m){ mode = m; }

    int getMode() const { return mode; }

    void setCropRotation(std::vector<ProductionProcess> cr) { cropRotation = cr; }

    struct NIAResult
    {
      NIAResult(){}
      double donation{0};
      int cycleDays{0};
    };
    //a callback which can be used to calculate todays (if any) irrigation application "donation"
    //and after how many days (cycleDays) to check again
    //input to the callback is the day of year (doy) and a vector of soil-moisture values
    std::function<NIAResult(int, std::vector<double>)> nextIrrigationApplication;

    //zeromq reply socket for datastream controlled MONICA run
//    std::unique_ptr<zmq::socket_t> datastream;
//    std::string inputDatastreamAddress;
    std::string inputDatastreamProtocol;
    std::string inputDatastreamPort;

    //zeromq publish socket for outputs of MONICA
//    std::unique_ptr<zmq::socket_t> outputstream;
//    std::string outputDatastreamAddress;
    std::string outputDatastreamProtocol;
    std::string outputDatastreamPort;

    zmq::context_t* zmqContext;
  private:
    //! Variable for differentiate betweend execution modes of MONICA
    int mode{MODE_LC_DSS};
  };

  //----------------------------------------------------------------------------
  
  /*!
   * @brief structure holding all results of one monica run
   */
  class Result 
  {
    public:
      Result(){}
      ~Result(){}

    //! just to keep track of the grid point the calculation is being made for
    Tools::GridPoint gp;
		//is as gp before used to track results when multiple parallel
		//unordered invocations of monica will happen
		int customId;

    //! vector of the result of one crop per year
		std::vector<PVResult> pvrs;

    //! results not regarding a particular crop in a rotation
    typedef std::map<ResultId, std::vector<double> > RId2Vector;
    RId2Vector generalResults;

    std::vector<double> getResultsById(int id);

		int sizeGeneralResults() { return int(generalResults.size()); }

    std::vector<std::string> dates;

    std::string toString();
  };

  //----------------------------------------------------------------------------

  /*!
   * @brief Core class of MONICA
   * @author Claas Nendel, Michael Berg
   */
	class MonicaModel
	{
  public:
    MonicaModel(const Env& env, const Climate::DataAccessor& da);

    /**
     * @brief Destructor
     */
		~MonicaModel()
		{
      delete _currentCropGrowth;
    }


    void generalStep(Tools::Date date, const std::map<int, double>& climateData);
    void generalStep(unsigned int stepNo);

    void cropStep(Tools::Date date, const std::map<int, double>& climateData);
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

		bool isCropPlanted() const
		{
      return _currentCrop.get() && _currentCrop->isValid();
    }

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

    void applyOrganicFertiliser(const OrganicMatterParameters*, double amount,
																bool incorporation);

		bool useNMinMineralFertilisingMethod() const
		{
      return _env.useNMinMineralFertilisingMethod;
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

    bool writeOutputFiles() {return centralParameterProvider.writeOutputFiles; }

    double avgCorg(double depth_m) const;
    double mean90cmWaterContent() const;
    double meanWaterContent(int layer, int number_of_layers) const;
    double sumNmin(double depth_m) const;
    double groundWaterRecharge() const;
    double nLeaching() const;
    double sumSoilTemperature(int layers);
    double sumNO3AtDay(double depth);
    double maxSnowDepth() const;
    double getAccumulatedSnowDepth() const;
    double getAccumulatedFrostDepth() const;
    double avg30cmSoilTemperature();
    double avgSoilMoisture(int start_layer, int end_layer);
    double avgCapillaryRise(int start_layer, int end_layer);
    double avgPercolationRate(int start_layer, int end_layer);
    double sumSurfaceRunOff();
    double surfaceRunoff();
    double getEvapotranspiration();
    double getTranspiration();
    double getEvaporation();
    double get_sum30cmSMB_CO2EvolutionRate();
    double getNH3Volatilised();
    double getSumNH3Volatilised();
    double getsum30cmActDenitrificationRate();
    double getETa();

    double vw_AtmosphericCO2Concentration;
    double vs_GroundwaterDepth;


    /**
     * @brief Returns soil temperature
     * @return temperature
     */
		const SoilTemperature& soilTemperature() { return _soilTemperature; }

    /**
     * @brief Returns soil moisture.
     * @return Moisture
     */
		const SoilMoisture& soilMoisture() { return _soilMoisture; }

    /**
     * @brief Returns soil organic mass.
     * @return soil organic
     */
		const SoilOrganic& soilOrganic() { return _soilOrganic; }

    /**
     * @brief Returns soil transport
     * @return soil transport
     */
		const SoilTransport& soilTransport() { return _soilTransport; }

    /**
     * @brief Returns soil column
     * @return soil column
     */
		const SoilColumn& soilColumn() { return _soilColumn; }

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
		double netRadiation(double globrad) { return globrad * (1 - _env.albedo); }

    int daysWithCrop() {return p_daysWithCrop; }
    double getAccumulatedNStress() { return p_accuNStress; }
    double getAccumulatedWaterStress() { return p_accuWaterStress; }
    double getAccumulatedHeatStress() { return p_accuHeatStress; }
    double getAccumulatedOxygenStress() { return p_accuOxygenStress; }

    double getGroundwaterInformation(Tools::Date date) {return _env.groundwaterInformation.getGroundwaterInformation(date); }

  private:
		//! environment describing the conditions under which the model runs
    Env _env; 

		//! main soil data structure
    SoilColumn _soilColumn;
		//! temperature code
    SoilTemperature _soilTemperature;
		//! moisture code
    SoilMoisture _soilMoisture;
		//! organic code
    SoilOrganic _soilOrganic;
		//! transport code
    SoilTransport _soilTransport;
		//! crop code for possibly planted crop
    CropGrowth* _currentCropGrowth;
		//! currently possibly planted crop
    CropPtr _currentCrop;

		//! store applied fertiliser during one production process
    double _sumFertiliser;

		//! stores the daily sum of applied fertiliser
    double _dailySumFertiliser;

    double _dailySumIrrigationWater;

		//! climate data available to the model
    const Climate::DataAccessor& _dataAccessor;

    const CentralParameterProvider& centralParameterProvider;

    int p_daysWithCrop;
    double p_accuNStress;
    double p_accuWaterStress;
    double p_accuHeatStress;
    double p_accuOxygenStress;
  };

  //----------------------------------------------------------------------------

  /*!
   * main function for running monica under a given Env(ironment)
   * @param env the environment completely defining what the model needs and gets
   * @return a structure with all the Monica results
   */
#ifndef	MONICA_GUI
  Result runMonica(Env env);
#else
  Result runMonica(Env env, Configuration* cfg = NULL);
#endif

  void startZeroMQMonica(Env env);

  void initializeFoutHeader(std::ofstream&);
  void initializeGoutHeader(std::ofstream&);
  void writeCropResults(const CropGrowth*, std::ofstream&, std::ofstream&, bool);
	void writeGeneralResults(std::ofstream &fout, std::ofstream &gout, Env &env,
													 MonicaModel &monica, int d);
  void dumpMonicaParametersIntoFile(std::string, CentralParameterProvider &cpp);
}

#endif
