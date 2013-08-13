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

namespace Monica
{
  /**
   * @class Env
   */
  struct Env 
  {
    /**
     * @brief
     */
		enum
		{
			MODE_LC_DSS = 0,
      MODE_ACTIVATE_OUTPUT_FILES,
			MODE_HERMES,
			MODE_EVA2,
			MODE_SENSITIVITY_ANALYSIS,
			MODE_CC_GERMANY
		};

    /**
     * @brief default constructor for value object use
     */
		Env()
			: soilParams(NULL),
				noOfLayers(0),
				layerThickness(0),
				useNMinMineralFertilisingMethod(false),
				useAutomaticIrrigation(false),
				useSecondaryYields(true),
				atmosphericCO2(-1),
				customCallerId(-1)
		{ }

    Env(const Env&);

    ~Env();

    /**
     * @brief Constructor
     * @param sps Soil parameters
     * @param nols Number of layers
     * @param lt Layer thickness
     */
    Env(const SoilPMs* sps, const CentralParameterProvider cpp);

    int numberOfPossibleSteps();
    void addOrReplaceClimateData(std::string, const std::vector<double>& data);

    const SoilPMs* soilParams;  //! a vector of soil parameter objects = layers of soil

    unsigned int noOfLayers;              //! number of layers
    double layerThickness;                //! thickness of a single layer

    bool useNMinMineralFertilisingMethod;
    MineralFertiliserParameters nMinFertiliserPartition;
    NMinUserParameters nMinUserParams;

    bool useAutomaticIrrigation;
    AutomaticIrrigationParameters autoIrrigationParams;

    bool useSecondaryYields;              //! tell if farmer uses the secondary yield products

    //static const int depthGroundwaterTable;   /**<  */
    double windSpeedHeight; /**<  */
    double atmosphericCO2; /**< [ppm] */
    double albedo; /**< Albedo [] */

    Climate::DataAccessor da;     //! object holding the climate data
    std::vector<ProductionProcess> cropRotation; //! vector of elements holding the data of the single crops in the rotation

    Tools::GridPoint gridPoint;        //! the gridpoint the model runs, just a convenience for the dss use
		int customCallerId;

    SiteParameters site;        //! site specific parameters
    GeneralParameters general;  //! general parameters to the model
		OrganicConstants organic;  //! constant organic parameters to the model

    CentralParameterProvider centralParameterProvider;

    std::string toString() const;
		std::string pathToOutputDir;

		void setMode(int mode);
		int getMode() {return this->mode; }

		void setCropRotation(std::vector<ProductionProcess> ff)
		{
		  cropRotation = ff;
		}

    private:
      int mode;   //! Variable for differentiate betweend execution modes of MONICA
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
		int customCallerId;

    //! vector of the result of one crop per year
    std::vector<PVResult> pvrs;

    //! results not regarding a particular crop in a rotation
    typedef std::map<ResultId, std::vector<double> > RId2Vector;
    RId2Vector generalResults;

    std::vector<double> getResultsById(int id);

    int sizeGeneralResults() { return generalResults.size(); }

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
    ~MonicaModel() {
      delete _currentCropGrowth;
    }

    void generalStep(unsigned int stepNo);
    void cropStep(unsigned int stepNo);
    double CO2ForDate(double year, double julianDay, bool isLeapYear);
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
    void harvestCurrentCrop();

    void incorporateCurrentCrop();

    //! cutting crop but not harvesting
    //void cutCurrentCrop();

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
    double accumulatedSnowDepth() const;
    double accumulatedFrostDepth() const;
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
  Result runMonica(Env env);

  void initializeFoutHeader(std::ofstream&);
  void initializeGoutHeader(std::ofstream&);
  void writeCropResults(const CropGrowth*, std::ofstream&, std::ofstream&, bool);
	void writeGeneralResults(std::ofstream &fout, std::ofstream &gout, Env &env,
													 MonicaModel &monica, int d);
  void dumpMonicaParametersIntoFile(std::string, CentralParameterProvider &cpp);
}

#endif
