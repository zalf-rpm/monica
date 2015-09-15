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

#ifndef MONICA_PARAMETERS_H_
#define MONICA_PARAMETERS_H_

/**
 * @file monica-parameters.h
 */

#include <map>
#include <string>
#include <vector>
#include <iostream>
#include <memory>
#include <functional>

#include "json11/json11.hpp"

#include "climate/climate-common.h"
#include "tools/date.h"
#include "soil/soil.h"
#include "monica-typedefs.h"

namespace Monica
{
#undef min
#undef max

	inline std::string pathSeparator()
	{
		return
#ifdef __unix__
			"/";
#else
			"\\";
#endif
	}

	class CentralParameterProvider;

	enum Eva2_Nutzung
	{
		NUTZUNG_UNDEFINED=0,
		NUTZUNG_GANZPFLANZE=1,
		NUTZUNG_KORN=2,
		NUTZUNG_GRUENDUENGUNG=7,
		NUTZUNG_CCM=8
	};

	const double UNDEFINED = -9999.9;
	const int UNDEFINED_INT = -9999;

	enum Region
	{
		weisseritz, uecker
	};

	enum { MONTH=12 };

	//----------------------------------------------------------------------------

	/**
	* @brief Enumeration for defining automatic harvesting times
	* Definition of different harvest time definition for the automatic
	* yield trigger
	*/
	enum AutomaticHarvestTime
	{
		maturity,	/**< crop is harvested when maturity is reached */
		unknown		/**< default error value */
	};


	//----------------------------------------------------------------------------

	enum ResultId
	{
		//! primary yield for the crop (e.g. the actual fruit)
		primaryYield,

		//! secondary yield for the crop (e.g. leafs and other stuff useable)
		secondaryYield,


		//! above ground biomass of the crop
		aboveGroundBiomass,

		//! Julian day of anthesis of the crop
		anthesisDay,

		//! Julian day of maturity of the crop
		maturityDay,
		
		//! Julian day of harvest
		harvestDay,

		//! sum of applied fertilizer for that crop during growth period
		sumFertiliser,

		//! sum of used irrigation water for the crop during growth period
		sumIrrigation,

		//! sum of N turnover
		sumMineralisation,

		//! the monthly average of the average corg content in the first 10 cm soil
		avg10cmMonthlyAvgCorg,

		//! the monthly average of the average corg content in the first 30 cm soil
		avg30cmMonthlyAvgCorg,

		//! the monthly average of the summed up water content in the first 90 cm soil
		mean90cmMonthlyAvgWaterContent,

		//! at some day in the year the sum of the N content in the first 90cm soil
		sum90cmYearlyNatDay,

		//! the monthly summed up amount of ground water recharge
		monthlySumGroundWaterRecharge,

		//! the monthly sum of N leaching
		monthlySumNLeaching,

		//! height of crop at harvesting date
		cropHeight,

		//! sum of NO3 content in the first 90cm soil at a special, hardcoded date
		sum90cmYearlyNO3AtDay,

		//! sum of NH4 content in the first 90cm soil at a special, hardcoded date
		sum90cmYearlyNH4AtDay,

		//! value of maximal snow depth during simulation duration
		maxSnowDepth,

		//! sum of snow depth for every day
		sumSnowDepth,

		//! sum of frost depth
		sumFrostDepth,

		//! Average soil temperature in the first 30cm soil at special, hardcoded date
		avg30cmSoilTemperature,

		//! Sum of soil temperature in the first 30cm soil at special, hardcoded date
		sum30cmSoilTemperature,

		//! Average soilmoisture content in first 30cm soil at special, hardcoded date
		avg0_30cmSoilMoisture,

		//! Average soilmoisture content in 30-60cm soil at special, hardcoded date
		avg30_60cmSoilMoisture,

		//! Average soilmoisture content in 60-90cm soil at special, hardcoded date
		avg60_90cmSoilMoisture,

		//! Average soilmoisture content in 0-90cm soil at special, hardcoded date
		avg0_90cmSoilMoisture,

		//! water flux at bottom layer of soil at special, hardcoded date
		waterFluxAtLowerBoundary,

		//! capillary rise in first 30 cm soil at special date
		avg0_30cmCapillaryRise,

		//! capillary rise in 30-60 cm soil at special date
		avg30_60cmCapillaryRise,

		//! capillary rise in 60-90cm cm soil at special date
		avg60_90cmCapillaryRise,

		//! percolation ratein first 30 cm soil at special date
		avg0_30cmPercolationRate,

		//! percolation ratein 30-60 cm soil at special date
		avg30_60cmPercolationRate,

		//! percolation rate in 60-90cm cm soil at special date
		avg60_90cmPercolationRate,

		//! sum of surface run off volumes during hole simulation duration
		sumSurfaceRunOff,

		//! Evapotranspiration amount at a special date
		evapotranspiration,

		//! Transpiration amount at a special date
		transpiration,

		//! Evaporation amount at a special date
		evaporation,

		//! N content in biomass after harvest
		biomassNContent,

		//! N content in above ground biomass after harvest
		aboveBiomassNContent,

		//! sum of total N uptake of plant
		sumTotalNUptake,

		//! sum of CO2 evolution rate in first 30 cm soil at special date
		sum30cmSMB_CO2EvolutionRate,

		//! volatilised NH3 at a special date
		NH3Volatilised,

		//! sum of all volatilised NH3
		sumNH3Volatilised,

		//! sum of denitrification rate in first 30cm at a special date
		sum30cmActDenitrificationRate,

		//! leaching N at boundary at special date
		leachingNAtBoundary,

		//! leaching N accumulated for a year
		yearlySumNLeaching,

		//! Groundwater recharge accumulated for a year
		yearlySumGroundWaterRecharge,

		//! Evapotranspiration in time of crop cultivation
		sumETaPerCrop,
		sumTraPerCrop,
		cropname,
		primaryYieldTM,
		secondaryYieldTM,
		monthlySurfaceRunoff,
		monthlyPrecip,
		monthlyETa,
		monthlySoilMoistureL0,
		monthlySoilMoistureL1,
		monthlySoilMoistureL2,
		monthlySoilMoistureL3,
		monthlySoilMoistureL4,
		monthlySoilMoistureL5,
		monthlySoilMoistureL6,
		monthlySoilMoistureL7,
		monthlySoilMoistureL8,
		monthlySoilMoistureL9,
		monthlySoilMoistureL10,
		monthlySoilMoistureL11,
		monthlySoilMoistureL12,
		monthlySoilMoistureL13,
		monthlySoilMoistureL14,
		monthlySoilMoistureL15,
		monthlySoilMoistureL16,
		monthlySoilMoistureL17,
		monthlySoilMoistureL18,
		daysWithCrop,
		NStress,
		WaterStress,
		HeatStress,
		OxygenStress,
		dev_stage
	};

	/*!
	 * @return list of results from a single crop
	 */
	const std::vector<ResultId>& cropResultIds();

  std::pair<std::string, std::string>
  nameAndUnitForResultId(ResultId rid);

	const std::vector<int>& eva2CropResultIds();
	const std::vector<int>& eva2MonthlyResultIds();

	/*!
	 * @return list of the montly results
	 */
	const std::vector<ResultId>& monthlyResultIds();

	/**
	 * @return list if ids used for sensitivity analysis
	 */
	const std::vector<int>& CCGermanyResultIds();

	struct ResultIdInfo {
		ResultIdInfo(const std::string& n, const std::string& u,
								 const std::string& sn = std::string())
			: name(n), unit(u), shortName(sn) {}
		std::string name;
		std::string unit;
		std::string shortName;
	};

	ResultIdInfo resultIdInfo(ResultId rid);

	/**
	 * @brief structure holding the results for a particular crop (in one year usually)
	 */
	struct PVResult
	{
    PVResult() {}

    PVResult(int id) : id(id) {}

    PVResult(json11::Json j);

    json11::Json to_json() const;

		//! id of crop
    CropId id{-1};

		//custom id to enable mapping of monica results to user defined other entities
		//e.g. a crop activity id from Carbiocial
    int customId{-1};
    Tools::Date date;

		//! different results for a particular crop
		std::map<ResultId, double> pvResults;
	};

  typedef std::shared_ptr<PVResult> PVResultPtr;

	//----------------------------------------------------------------------------

	/**
	 * @brief
	 */
	struct YieldComponent
	{
		/**
		 * @brief Constructor
		 * @param oid organ ID
		 * @param yp Yield percentage
		 */
		YieldComponent(int oid, double yp, double ydm) : organId(oid), yieldPercentage(yp), yieldDryMatter(ydm) { }

		int organId; /**<  */
		double yieldPercentage; /**<  */
		double yieldDryMatter;
	};

	//----------------------------------------------------------------------------

	/**
	 * @brief Parameter for crops
	 */
	struct CropParameters
	{
		void resizeStageOrganVectors();
		std::string toString() const;

		//		pc_DevelopmentAccelerationByNitrogenStress(0)

		// members
		std::string pc_CropName; /**< Name */
    bool pc_Perennial{false};
    int pc_NumberOfDevelopmentalStages{0};
    int pc_NumberOfOrgans{0};
    int pc_CarboxylationPathway{0};
    double pc_DefaultRadiationUseEfficiency{0.0};
    double pc_PartBiologicalNFixation{0.0};
    double pc_InitialKcFactor{0.0};
    double pc_LuxuryNCoeff{0.0};
    double pc_MaxAssimilationRate{0.0};
    double pc_MaxCropDiameter{0.0};
    double pc_MaxCropHeight{0.0};
    double pc_CropHeightP1{0.0};
    double pc_CropHeightP2{0.0};
    double pc_StageAtMaxHeight{0.0};
    double pc_StageAtMaxDiameter{0.0};
    double pc_MinimumNConcentration{0.0};
    double pc_MinimumTemperatureForAssimilation{0.0};
    double pc_NConcentrationAbovegroundBiomass{0.0};
    double pc_NConcentrationB0{0.0};
    double pc_NConcentrationPN{0.0};
    double pc_NConcentrationRoot{0.0};
    double pc_ResidueNRatio{0.0};
    int pc_DevelopmentAccelerationByNitrogenStress{0};
    double pc_FieldConditionModifier{1.0};
    double pc_AssimilateReallocation{0.0};
    double pc_LT50cultivar{0.0};
    double pc_FrostHardening{0.0};
    double pc_FrostDehardening{0.0};
    double pc_LowTemperatureExposure{0.0};
    double pc_RespiratoryStress{0.0};
    int pc_LatestHarvestDoy{-1};

    std::vector<std::vector<double>> pc_AssimilatePartitioningCoeff;
    std::vector<std::vector<double>> pc_OrganSenescenceRate;

    std::vector<double> pc_BaseDaylength;
    std::vector<double> pc_BaseTemperature;
    std::vector<double> pc_OptimumTemperature;
    std::vector<double> pc_DaylengthRequirement;
    std::vector<double> pc_DroughtStressThreshold;
    std::vector<double> pc_OrganMaintenanceRespiration;
    std::vector<double> pc_OrganGrowthRespiration;
    std::vector<double> pc_SpecificLeafArea;
    std::vector<double> pc_StageMaxRootNConcentration;
    std::vector<double> pc_StageKcFactor;
    std::vector<double> pc_StageTemperatureSum;
    std::vector<double> pc_VernalisationRequirement;
    std::vector<double> pc_InitialOrganBiomass;
    std::vector<double> pc_CriticalOxygenContent;

    double pc_CropSpecificMaxRootingDepth{0.0};
    std::vector<int> pc_AbovegroundOrgan;
    std::vector<int> pc_StorageOrgan;

    double pc_SamplingDepth{0.0};
    double pc_TargetNSamplingDepth{0.0};
    double pc_TargetN30{0.0};
    double pc_HeatSumIrrigationStart{0.0};
    double pc_HeatSumIrrigationEnd{0.0};
    double pc_MaxNUptakeParam{0.0};
    double pc_RootDistributionParam{0.0};
    double pc_PlantDensity{0.0};
    double pc_RootGrowthLag{0.0};
    double pc_MinimumTemperatureRootGrowth{0.0};
    double pc_InitialRootingDepth{0.0};
    double pc_RootPenetrationRate{0.0};
    double pc_RootFormFactor{0.0};
    double pc_SpecificRootLength{0.0};
    int pc_StageAfterCut{0};
    double pc_CriticalTemperatureHeatStress{0.0};
    double pc_LimitingTemperatureHeatStress{0.0};
    double pc_BeginSensitivePhaseHeatStress{0.0};
    double pc_EndSensitivePhaseHeatStress{0.0};
    int pc_CuttingDelayDays{0};
    double pc_DroughtImpactOnFertilityFactor{0.0};

		std::vector<YieldComponent> pc_OrganIdsForPrimaryYield;
		std::vector<YieldComponent> pc_OrganIdsForSecondaryYield;
		std::vector<YieldComponent> pc_OrganIdsForCutting;
	};

	//----------------------------------------------------------------------------

	/**
	 * @brief Returns crop parameters for a special crop, specified by cropId.
	 * @param cropID
	 * @param @return CropParameters
	 */
	const CropParameters* getCropParametersFromMonicaDB(int cropId);

	//----------------------------------------------------------------------------

  enum FertiliserType { mineral, organic, undefined };

  /**
   * @brief Parameters for mineral fertiliser.
   * Simple data structure that holds information about mineral fertiliser.
   * @author Xenia Holtmann, Dr. Claas Nendel
   */
  class MineralFertiliserParameters
  {
  public:
    MineralFertiliserParameters() {}

    MineralFertiliserParameters(json11::Json j);

    MineralFertiliserParameters(const std::string& name, double carbamid,
                                double no3, double nh4);

    json11::Json to_json() const;

    std::string toString() const;


    /**
     * @brief Returns name of fertiliser.
     * @return Name
     */
    inline std::string getName() const { return name; }

    /**
     * @brief Returns carbamid part in percentage of fertiliser.
     * @return Carbamid in percent
     */
    inline double getCarbamid() const { return vo_Carbamid; }

    /**
     * @brief Returns ammonium part of fertliser.
     * @return Ammonium in percent
     */
    inline double getNH4() const { return vo_NH4; }

    /**
     * @brief Returns nitrat part of fertiliser
     * @return Nitrat in percent
     */
    inline double getNO3() const { return vo_NO3; }

    // Setter ------------------------------------

    /**
     * @brief Sets name of fertiliser
     * @param name
     */
    inline void setName(const std::string& name) { this->name = name; }

    /**
     * Sets carbamid part of fertilisers
     * @param vo_Carbamid percent
     */
    inline void setCarbamid(double vo_Carbamid)
    {
      this->vo_Carbamid = vo_Carbamid;
    }

    /**
     * @brief Sets nitrat part of fertiliser.
     * @param vo_NH4
     */
    inline void setNH4(double vo_NH4) { this->vo_NH4 = vo_NH4; }

    /**
     * @brief Sets nitrat part of fertiliser.
     * @param vo_NO3
     */
    inline void setNO3(double vo_NO3) { this->vo_NO3 = vo_NO3; }



  private:
    std::string name;
    double vo_Carbamid{0.0};
    double vo_NH4{0.0};
    double vo_NO3{0.0};
  };

  //----------------------------------------------------------------------------

  struct NMinUserParameters
  {
    NMinUserParameters() {}

    NMinUserParameters(double min, double max, int delayInDays);

    NMinUserParameters(json11::Json j);

    json11::Json to_json() const;

    std::string toString() const;

    double min{0.0}, max{0.0};
    int delayInDays{0};
  };

  //----------------------------------------------------------------------------

  struct IrrigationParameters
  {
    IrrigationParameters() {}

    IrrigationParameters(double nitrateConcentration,
                         double sulfateConcentration)
      : nitrateConcentration(nitrateConcentration),
        sulfateConcentration(sulfateConcentration)
    {}

    IrrigationParameters(json11::Json j)
      : nitrateConcentration(j["nitrateConcentration"].number_value()),
        sulfateConcentration(j["sulfateConcentration"].number_value())
    {}

    virtual json11::Json to_json() const
    {
      return json11::Json::object {
        {"type", "IrrigationParameters"},
        {"nitrateConcentration", nitrateConcentration},
        {"sulfateConcentration", sulfateConcentration}};
    }

    double nitrateConcentration{0.0};
    double sulfateConcentration{0.0};

    std::string toString() const;
  };

  //----------------------------------------------------------------------------

  struct AutomaticIrrigationParameters : public IrrigationParameters
  {
    AutomaticIrrigationParameters() {}

		AutomaticIrrigationParameters(double a, double t, double nc, double sc)
			: IrrigationParameters(nc, sc), amount(a), treshold(t) {}

    AutomaticIrrigationParameters(json11::Json j)
      : IrrigationParameters(j["irrigationParameters"]),
        amount(j["amount"].number_value()),
        treshold(j["treshold"].number_value())
    {}

    virtual json11::Json to_json() const
    {
      return json11::Json::object {
        {"type", "AutomaticIrrigationParameters"},
        {"irrigationParameters", IrrigationParameters::to_json()},
        {"amount", amount},
        {"treshold", treshold}};
    }

    double amount{17.0};
    double treshold{0.35};

    std::string toString() const;
  };

  //----------------------------------------------------------------------------

  class MeasuredGroundwaterTableInformation
  {
  public:
    MeasuredGroundwaterTableInformation() {}
//    ~MeasuredGroundwaterTableInformation() {}

    MeasuredGroundwaterTableInformation(json11::Json j)
      : groundwaterInformationAvailable(j["groundwaterInformationAvailable"].bool_value())
    {
      for(auto p : j["groundwaterInfo"].object_items())
        groundwaterInfo[Tools::Date::fromIsoDateString(p.first)] = p.second.number_value();
    }

    void readInGroundwaterInformation(std::string path);

    double getGroundwaterInformation(Tools::Date gwDate) const;

    bool isGroundwaterInformationAvailable() const { return groundwaterInformationAvailable; }

    json11::Json to_json() const
    {
      json11::Json::object gi;
      for(auto p : groundwaterInfo)
        gi[p.first.toIsoDateString()] = p.second;

      return json11::Json::object {
        {"type", "MeasuredGroundwaterTableInformation"},
        {"groundwaterInformationAvailable", groundwaterInformationAvailable},
        {"groundwaterInfo", gi}};
    }

  private:
      bool groundwaterInformationAvailable{false};
      std::map<Tools::Date, double> groundwaterInfo;
  };

  //----------------------------------------------------------------------------

	struct GeneralParameters
	{
    GeneralParameters(double layerThickness = 0.1);

    GeneralParameters(json11::Json j);

    json11::Json to_json() const;

    size_t ps_NumberOfLayers() const { return ps_LayerThickness.size(); }

    double ps_ProfileDepth{2.0};
    double ps_MaxMineralisationDepth{0.4};
    bool pc_NitrogenResponseOn{true};
    bool pc_WaterDeficitResponseOn{true};
    bool pc_EmergenceFloodingControlOn{true};
    bool pc_EmergenceMoistureControlOn{true};

    std::vector<double> ps_LayerThickness;

    bool useNMinMineralFertilisingMethod{false};
    MineralFertiliserParameters nMinFertiliserPartition;
    NMinUserParameters nMinUserParams;
    double atmosphericCO2{-1.0};
    bool useAutomaticIrrigation{false};
    AutomaticIrrigationParameters autoIrrigationParams;
    bool useSecondaryYields{true};
    double windSpeedHeight{0};
    double albedo{0};
    MeasuredGroundwaterTableInformation groundwaterInformation;

    std::string pathToOutputDir;
	};

	//----------------------------------------------------------------------------

	/**
	 * @author Claas Nendel, Michael Berg
	 */
	struct SiteParameters
	{
    SiteParameters(){}

    SiteParameters(json11::Json j);

    json11::Json to_json() const;

    std::string toString() const;

    double vs_Latitude{60.0};
    //! slope [m m-1]
    double vs_Slope{0.01};
    //! [m]
    double vs_HeightNN{50.0};
    //! [m]
    double vs_GroundwaterDepth{70.0};
    double vs_Soil_CN_Ratio{10.0};
    double vs_DrainageCoeff{1.0};
    double vq_NDeposition{30.0};
    double vs_MaxEffectiveRootingDepth{2.0};
	};

	//----------------------------------------------------------------------------

	/**
	* @brief Data structure that containts all relevant parameters for the automatic yield trigger.
	*/
	class AutomaticHarvestParameters
	{
		public:
		/** @brief Constructor */
		AutomaticHarvestParameters() : 
			_harvestTime(AutomaticHarvestTime::unknown),
			_latestHarvestDOY(-1) {}
	
		/** @brief Constructor */
		AutomaticHarvestParameters(AutomaticHarvestTime yt) : 
			_harvestTime(yt),
			_latestHarvestDOY(-1) {}

		/**
		 * @brief Setter for automatic harvest time
		 */
		void setHarvestTime(AutomaticHarvestTime time) { _harvestTime = time;  }

		/**
		 * @brief Getter for automatic harvest time
		 */
		AutomaticHarvestTime getHarvestTime() const { return _harvestTime;  }

		/**
		 * @brief Setter for fallback automatic harvest day
		 */
		void setLatestHarvestDOY(int doy) { _latestHarvestDOY = doy;  }

		/**
		* @brief Getter for fallback automatic harvest day
		*/
		int getLatestHarvestDOY() const { return _latestHarvestDOY; }
		
		std::string toString() const;

		private:

		AutomaticHarvestTime _harvestTime;		/**< Harvest time parameter */
		int _latestHarvestDOY;					/**< Fallback day for latest harvest of the crop */

		
	};

	//----------------------------------------------------------------------------

  struct OrganicMatterParameters;
	class AutomaticHarvestParameters;
	
	class Crop;
  typedef std::shared_ptr<Crop> CropPtr;
	
	class Crop
	{
	public:
    Crop(const std::string& name = "fallow");

    Crop(CropId id, const std::string& name,
         const CropParameters* cps = nullptr,
         const OrganicMatterParameters* rps = nullptr,
         double crossCropAdaptionFactor = 1);

    Crop(CropId id, const std::string& name,
         const Tools::Date& seedDate,
         const Tools::Date& harvestDate,
         const CropParameters* cps = nullptr,
         const OrganicMatterParameters* rps = nullptr,
         double crossCropAdaptionFactor = 1);

    Crop(json11::Json j);

    json11::Json to_json() const;

    CropId id() const { return _id; }

    std::string name() const { return _name; }

    bool isValid() const { return _id > -1; }

    const CropParameters* cropParameters() const { return _cropParams; }

    const CropParameters* perennialCropParameters() const { return _perennialCropParams; }

    void setCropParameters(const CropParameters* cps) { _cropParams = cps; }

    void setPerennialCropParameters(const CropParameters* cps) { _perennialCropParams = cps; }

    const OrganicMatterParameters* residueParameters() const { return _residueParams; }

    void setResidueParameters(const OrganicMatterParameters* rps) { _residueParams = rps; }

		Tools::Date seedDate() const { return _seedDate; }

		Tools::Date harvestDate() const { return _harvestDate; }

		std::vector<Tools::Date> getCuttingDates() const { return _cuttingDates; }

    void setSeedAndHarvestDate(const Tools::Date& sd, const Tools::Date& hd)
		{
      _seedDate = sd;
      _harvestDate = hd;
		}

		void addCuttingDate(const Tools::Date cd) { _cuttingDates.push_back(cd); }

		//void applyCutting();

		std::string toString(bool detailed = false) const;

		void setHarvestYields(double primaryYield, double secondaryYield)
		{
			_primaryYield += primaryYield;
			_secondaryYield += secondaryYield;
		}

		void setHarvestYieldsTM(double primaryYieldTM, double secondaryYieldTM)
		{
			_primaryYieldTM += primaryYieldTM;
			_secondaryYieldTM += secondaryYieldTM;
		}

		void setYieldNContent(double primaryYieldN, double secondaryYieldN)
		{
			_primaryYieldN += primaryYieldN;
			_secondaryYieldN += secondaryYieldN;
		}

		void addAppliedIrrigationWater(double amount) { _appliedAmountIrrigation += amount; }
		void setSumTotalNUptake(double sum) { _sumTotalNUptake = sum; }
		void setCropHeight(double height) {_cropHeight = height; }
		void setAccumulatedETa(double eta) { _accumulatedETa = eta; }
		void setAccumulatedTranspiration(double transp) { _accumulatedTranspiration = transp;  }

		double appliedIrrigationWater() const { return _appliedAmountIrrigation; }
		double sumTotalNUptake() const {return _sumTotalNUptake; }
		double primaryYield() const { return _primaryYield * _crossCropAdaptionFactor; }
		double aboveGroundBiomass() const { return _primaryYield * _crossCropAdaptionFactor + _secondaryYield * _crossCropAdaptionFactor; }
		double secondaryYield() const { return _secondaryYield * _crossCropAdaptionFactor; }
		double primaryYieldTM() const {  return _primaryYieldTM * _crossCropAdaptionFactor; }
		double secondaryYieldTM() const { return _secondaryYieldTM * _crossCropAdaptionFactor; }
		double primaryYieldN() const { return _primaryYieldN; }
		double aboveGroundBiomasseN() const { return _primaryYieldN + _secondaryYieldN; }
		double secondaryYieldN() const { return _secondaryYieldN; }
		double cropHeight() const { return _cropHeight; }

		void reset()
		{
			_primaryYield = _secondaryYield = _appliedAmountIrrigation = 0;
			_primaryYieldN = _secondaryYieldN = _accumulatedETa = _accumulatedTranspiration =  0.0;
			_primaryYieldTM = _secondaryYield = 0.0;
			_anthesisDay = _maturityDay = -1;
		}

		void setEva2TypeUsage(int type) { eva2_typeUsage = type; }
		int getEva2TypeUsage() const { return eva2_typeUsage; }

		double get_AccumulatedETa() const {return _accumulatedETa;}
		double get_AccumulatedTranspiration() const { return _accumulatedTranspiration; }

		void writeCropParameters(std::string path);

		void setAnthesisDay(int day) { _anthesisDay = day; }
		int getAnthesisDay() const { return _anthesisDay; }

		void setMaturityDay(int day) { _maturityDay = day; }
		int getMaturityDay() const { return _maturityDay; }
		
		// Automatic yiedl trigger parameters
		bool useAutomaticHarvestTrigger() { return _automaticHarvest; }
		void activateAutomaticHarvestTrigger(AutomaticHarvestParameters params) { _automaticHarvest = true; _automaticHarvestParams = params; }
		AutomaticHarvestParameters getAutomaticHarvestParams() { return _automaticHarvestParams; }

	private:
    CropId _id{-1};
		std::string _name;
		Tools::Date _seedDate;
		Tools::Date _harvestDate;
		std::vector<Tools::Date> _cuttingDates;
    const CropParameters* _cropParams{nullptr};
    const CropParameters* _perennialCropParams{nullptr};
    const OrganicMatterParameters* _residueParams{nullptr};
    double _primaryYield{0.0};
    double _secondaryYield{0.};
    double _primaryYieldTM{0.0};
    double _secondaryYieldTM{0.0};
    double _appliedAmountIrrigation{0.0};
    double _primaryYieldN{0.0};
    double _secondaryYieldN{0.0};
    double _sumTotalNUptake{0.0};

    double _crossCropAdaptionFactor{1.0};
    double _cropHeight{0.0};
    double _accumulatedETa{0.0};
    double _accumulatedTranspiration{0.0};

    int eva2_typeUsage{NUTZUNG_UNDEFINED};

    int _anthesisDay{-1};
    int _maturityDay{-1};
		
    bool _automaticHarvest{false};
		AutomaticHarvestParameters _automaticHarvestParams;
	};

	
	//----------------------------------------------------------------------------



	class MonicaModel;

	class WorkStep
	{
	public:

		WorkStep(const Tools::Date& d) : _date(d) { }

		Tools::Date date() const { return _date; }
		virtual void setDate(Tools::Date date) {_date = date; }

		//! do whatever the workstep has to do
		virtual void apply(MonicaModel* model) = 0;

		virtual std::string toString() const;

		virtual WorkStep* clone() const = 0;

    virtual json11::Json to_json() const = 0;

	protected:
		Tools::Date _date;
	};

  typedef std::shared_ptr<WorkStep> WSPtr;

	//----------------------------------------------------------------------------

	class Seed : public WorkStep
	{
	public:

		Seed(const Tools::Date& at, CropPtr crop)
      : WorkStep(at),
        _crop(crop)
    {}

    Seed(json11::Json j)
      : WorkStep(Tools::Date::fromIsoDateString(j["date"].string_value())),
        _crop(std::make_shared<Crop>(j["crop"]))
    {}

		virtual void apply(MonicaModel* model);

		void setDate(Tools::Date date)
		{
			this->_date = date;
			_crop.get()->setSeedAndHarvestDate(date, _crop.get()->harvestDate());
		}

		virtual std::string toString() const;

		virtual Seed* clone() const {return new Seed(*this); }

    CropPtr crop() const { return _crop; }

    virtual json11::Json to_json() const
    {
      return json11::Json::object {
        {"type", "Seed"},
        {"date", date().toIsoDateString()},
        {"crop", _crop->to_json()}};
    }

	private:
		CropPtr _crop;
	};

	//----------------------------------------------------------------------------

	class Harvest : public WorkStep
	{
	public:
    Harvest(const Tools::Date& at,
            CropPtr crop,
            PVResultPtr cropResult,
            std::string method = "total")
      : WorkStep(at),
        _crop(crop),
        _cropResult(cropResult),
        _method(method)
    {}

    Harvest(json11::Json j,
            CropPtr crop)
      : WorkStep(Tools::Date::fromIsoDateString(j["date"].string_value())),
        _crop(crop),
        _cropResult(new PVResult(crop->id())),
        _method(j["method"].string_value()),
        _percentage(j["percentage"].number_value()),
        _exported(j["exported"].bool_value())
    {}

		virtual void apply(MonicaModel* model);

		void setDate(Tools::Date date)
		{
			this->_date = date;
			_crop.get()->setSeedAndHarvestDate(_crop.get()->seedDate(), date);
		}

		void setPercentage(double percentage)
		{
			_percentage = percentage;
		}

		void setExported(bool exported)
		{
			_exported = exported;
		}
		
		virtual std::string toString() const;

		virtual Harvest* clone() const { return new Harvest(*this); }

    virtual json11::Json to_json() const
    {
      return json11::Json::object {
        {"type", "Harvest"},
        {"date", date().toIsoDateString()},
        {"method", _method},
        {"percentage", _percentage},
        {"exported", _exported}};
    }

    PVResultPtr cropResult() const { return _cropResult; }

	private:
		CropPtr _crop;
		PVResultPtr _cropResult;
    std::string _method;
    double _percentage{0};
    bool _exported{true};
	};

  //----------------------------------------------------------------------------

	class Cutting : public WorkStep
	{
	public:

		Cutting(const Tools::Date& at, CropPtr crop) : WorkStep(at), _crop(crop) { }

    Cutting(json11::Json j, CropPtr crop)
      : WorkStep(Tools::Date::fromIsoDateString(j["date"].string_value())),
        _crop(crop) {}

		virtual void apply(MonicaModel* model);

		virtual std::string toString() const;

		virtual Cutting* clone() const {return new Cutting(*this); }

    virtual json11::Json to_json() const
    {
      return json11::Json::object {
        {"type", "Cutting"},
        {"date", date().toIsoDateString()}};
    }

	private:
		CropPtr _crop;
	};

	//----------------------------------------------------------------------------

	struct NMinCropParameters
	{
    NMinCropParameters() {}

		NMinCropParameters(double samplingDepth, double nTarget, double nTarget30)
      : samplingDepth(samplingDepth),
        nTarget(nTarget),
        nTarget30(nTarget30) {}

    NMinCropParameters(json11::Json j)
      : samplingDepth(j["samplingDepth"].number_value()),
        nTarget(j["nTarget"].number_value()),
        nTarget30(j["nTarget30"].number_value())
    {}

    json11::Json to_json() const
    {
      return json11::Json::object {
        {"type", "NMinCropParameters"},
        {"samplingDepth", samplingDepth},
        {"nTarget", nTarget},
        {"nTarget30", nTarget30}};
    }

		std::string toString() const;

    double samplingDepth{0.0};
    double nTarget{0.0};
    double nTarget30{0.0};
	};

	//----------------------------------------------------------------------------



	class MineralFertiliserApplication : public WorkStep
	{
	public:
		MineralFertiliserApplication(const Tools::Date& at,
																 MineralFertiliserParameters partition,
																 double amount)
			: WorkStep(at), _partition(partition), _amount(amount) { }

    MineralFertiliserApplication(json11::Json j)
      : WorkStep(Tools::Date::fromIsoDateString(j["date"].string_value())),
        _partition(j["parameters"]),
        _amount(j["amount"].number_value())
    {}

		virtual void apply(MonicaModel* model);

		MineralFertiliserParameters partition() const
		{
			return _partition;
		}

		double amount() const { return _amount; }

		virtual std::string toString() const;

		virtual MineralFertiliserApplication* clone() const {return new MineralFertiliserApplication(*this); }

    virtual json11::Json to_json() const
    {
      return json11::Json::object {
        {"type", "MineralFertiliserApplication"},
        {"date", date().toIsoDateString()},
        {"amount", _amount},
        {"parameters", _partition}};
    }

	private:
		MineralFertiliserParameters _partition;
		double _amount;
	};

	//----------------------------------------------------------------------------

	class OrganicFertiliserApplication : public WorkStep
	{
	public:
		OrganicFertiliserApplication(const Tools::Date& at,
																 const OrganicMatterParameters* params,
																 double amount,
                                 bool incorp = true);

    OrganicFertiliserApplication(json11::Json j);

    virtual json11::Json to_json() const;

    virtual void apply(MonicaModel* model);

    //! Returns parameter for organic fertilizer
    const OrganicMatterParameters* parameters() const { return _params; }

		//! Returns fertilization amount
		double amount() const { return _amount; }

		//! Returns TRUE, if fertilizer is applied with incorporation
		bool incorporation() const { return _incorporation; }

		//! Returns a string that contains all parameters. Used for debug outputs.
		virtual std::string toString() const;

    virtual OrganicFertiliserApplication* clone() const {return new OrganicFertiliserApplication(*this); }



  private:
    std::shared_ptr<OrganicMatterParameters> _paramsPtr;
    const OrganicMatterParameters* _params{nullptr};
    double _amount{0.0};
    bool _incorporation{false};

	};

	//----------------------------------------------------------------------------

	class TillageApplication : public WorkStep
	{
	public:
		TillageApplication(const Tools::Date& at, double depth)
			: WorkStep(at), _depth(depth) { }

    TillageApplication(json11::Json j)
      : WorkStep(Tools::Date::fromIsoDateString(j["date"].string_value())),
        _depth(j["depthparameters"].number_value())
    {}

		virtual void apply(MonicaModel* model);

		double depth() const { return _depth; }

		virtual std::string toString() const;

		virtual TillageApplication* clone() const {return new TillageApplication(*this); }

    virtual json11::Json to_json() const
    {
      return json11::Json::object {
        {"type", "TillageApplication"},
        {"date", date().toIsoDateString()},
        {"depth", _depth}};
    }

	private:
    double _depth{0.0};
	};

	//----------------------------------------------------------------------------





	class IrrigationApplication : public WorkStep
	{
	public:
    IrrigationApplication(const Tools::Date& at, double amount,
                          IrrigationParameters params = IrrigationParameters())
      : WorkStep(at),
        _amount(amount),
        _params(params)
    {}

    IrrigationApplication(json11::Json j)
      : WorkStep(Tools::Date::fromIsoDateString(j["date"].string_value())),
        _amount(j["amount"].number_value()),
        _params(j["parameters"])
    {}

		virtual void apply(MonicaModel* model);

		double amount() const { return _amount; }

		double nitrateConcentration() const { return _params.nitrateConcentration; }

		double sulfateConcentration() const { return _params.sulfateConcentration; }

		virtual std::string toString() const;

		virtual IrrigationApplication* clone() const {return new IrrigationApplication(*this); }

    virtual json11::Json to_json() const
    {
      return json11::Json::object {
        {"type", "IrrigationApplication"},
        {"date", date().toIsoDateString()},
        {"amount", _amount},
        {"parameters", _params}};
    }

	private:
    double _amount{0};
		IrrigationParameters _params;
	};

	//----------------------------------------------------------------------------



	/**
	 * @class PV
	 * @param params
	 * @param id
	 * @param start Start date
	 * @param end End date
	 */
	class ProductionProcess
	{
	public:
		ProductionProcess() { }

		ProductionProcess(const std::string& name, CropPtr crop = CropPtr());

		//ProductionProcess(const ProductionProcess&);

		ProductionProcess deepCloneAndClearWorksteps() const;

		template<class Application>
		void addApplication(const Application& a)
		{
			_worksteps.insert(std::make_pair(a.date(), WSPtr(new Application(a))));
    }

		void addApplication(WSPtr a)
		{
			_worksteps.insert(std::make_pair((a.get())->date(), a));
		}

		void apply(const Tools::Date& date, MonicaModel* model) const;

		Tools::Date nextDate(const Tools::Date & date) const;

		std::string name() const { return _name; }

		CropPtr crop() const { return _crop; }

		bool isFallow() const { return !_crop->isValid();  }

		//! when does the PV start
		Tools::Date start() const;

		//! when does the whole PV end
		Tools::Date end() const;

    const std::multimap<Tools::Date, WSPtr>& getWorksteps() { return _worksteps; }

		void clearWorksteps() { _worksteps.clear(); }

		std::string toString() const;

		PVResult cropResult() const { return *(_cropResult.get()); }
		PVResultPtr cropResultPtr() const { return _cropResult; }

		//the custom id is used to keep a potentially usage defined
    //mapping to an entity from another domain,
    //e.g. a Carbiocial CropActivity which the ProductionProcess was based on
		void setCustomId(int cid) { _customId = cid; }
		int customId() const { return _customId; }

    void setIrrigateCrop(bool irr){ _irrigateCrop = irr; }
    bool irrigateCrop() const { return _irrigateCrop; }

	private:
    int _customId{0};
		std::string _name;
		CropPtr _crop;
    bool _irrigateCrop{false};

		//!ordered list of worksteps to be done for this PV
		std::multimap<Tools::Date, WSPtr> _worksteps;

		//store results of the productionprocess
		PVResultPtr _cropResult;
	};

	MineralFertiliserParameters
	getMineralFertiliserParametersFromMonicaDB(int mineralFertiliserId);

	void attachFertiliserApplicationsToCropRotation
	(std::vector<ProductionProcess>& cropRotation,
	 const std::string& pathToFertiliserFile);

	std::vector<ProductionProcess>
	attachFertiliserSA(std::vector<ProductionProcess> cropRotation,
										 const std::string pathToFertiliserFile);

	void attachIrrigationApplicationsToCropRotation
	(std::vector<ProductionProcess>& cropRotation,
	 const std::string& pathToIrrigationFile);

	/*!
	 * - create cropRotation from hermes file
	 * - the returned production processes contain absolute dates
	 */
	std::vector<ProductionProcess>
		cropRotationFromHermesFile(const std::string& pathToFile, bool useAutomaticHarvestTrigger = false, AutomaticHarvestParameters autoHarvestParameters=AutomaticHarvestParameters());

	Climate::DataAccessor climateDataFromHermesFiles(const std::string& pathToFile,
																									 int fromYear, int toYear,
																									 const CentralParameterProvider& cpp,
																									 bool useLeapYears = true, double latitude = 51.2);

	//----------------------------------------------------------------------------

	/**
	 * @brief Parameters for organic fertiliser
	 */
  struct OrganicMatterParameters
	{
//	public:
		OrganicMatterParameters();
    OrganicMatterParameters(json11::Json);
		OrganicMatterParameters(const OrganicMatterParameters&);

		~OrganicMatterParameters() { }

		std::string toString() const;

    json11::Json to_json() const
    {
      return json11::Json::object {
        {"name", name},
        {"AOM_DryMatterContent", vo_AOM_DryMatterContent},
        {"AOM_AOM_NH4Content", vo_AOM_NH4Content},
        {"AOM_AOM_NO3Content", vo_AOM_NO3Content},
        {"AOM_AOM_NO3Content", vo_AOM_NO3Content},
        {"AOM_AOM_SlowDecCoeffStandard", vo_AOM_SlowDecCoeffStandard},
        {"AOM_AOM_FastDecCoeffStandard", vo_AOM_FastDecCoeffStandard},
        {"AOM_PartAOM_to_AOM_Slow", vo_PartAOM_to_AOM_Slow},
        {"AOM_PartAOM_to_AOM_Fast", vo_PartAOM_to_AOM_Fast},
        {"AOM_CN_Ratio_AOM_Slow", vo_CN_Ratio_AOM_Slow},
        {"AOM_CN_Ratio_AOM_Fast", vo_CN_Ratio_AOM_Fast},
        {"AOM_PartAOM_Slow_to_SMB_Slow", vo_PartAOM_Slow_to_SMB_Slow},
        {"AOM_PartAOM_Slow_to_SMB_Fast", vo_PartAOM_Slow_to_SMB_Fast},
        {"AOM_NConcentration", vo_NConcentration}};
    }

		std::string name;

		double vo_AOM_DryMatterContent;   //! Dry matter content of added organic matter [kg DM kg FM-1]
		double vo_AOM_NH4Content;         //! Ammonium content in added organic matter [kg N kg DM-1]
		double vo_AOM_NO3Content;         //! Nitrate content in added organic matter [kg N kg DM-1]
		double vo_AOM_CarbamidContent;    //! Carbamide content in added organic matter [kg N kg DM-1]

		double vo_AOM_SlowDecCoeffStandard; //! Decomposition rate coefficient of slow AOM at standard conditions [d-1]
		double vo_AOM_FastDecCoeffStandard; //! Decomposition rate coefficient of fast AOM at standard conditions [d-1]

		double vo_PartAOM_to_AOM_Slow;    //! Part of AOM that is assigned to the slowly decomposing pool [kg kg-1
		double vo_PartAOM_to_AOM_Fast;    //! Part of AOM that is assigned to the rapidly decomposing pool [kg kg-1]

		double vo_CN_Ratio_AOM_Slow;    //! C to N ratio of the slowly decomposing AOM pool []
		double vo_CN_Ratio_AOM_Fast;    //! C to N ratio of the rapidly decomposing AOM pool []

		double vo_PartAOM_Slow_to_SMB_Slow;   //! Part of AOM slow consumed by slow soil microbial biomass [kg kg-1]
		double vo_PartAOM_Slow_to_SMB_Fast;   //! Part of AOM slow consumed by fast soil microbial biomass [kg kg-1]

		double vo_NConcentration;
	};

	typedef OrganicMatterParameters OMP;
  typedef std::shared_ptr<OMP> OMPPtr;

	OrganicMatterParameters*
	getOrganicFertiliserParametersFromMonicaDB(int organ_fert_id);

	const OrganicMatterParameters* getResidueParametersFromMonicaDB(int crop_id);

	//----------------------------------------------------------------------------

	/**
	 * Class that holds information of crop defined by user.
	 * @author Xenia Specka
	 */
  struct UserCropParameters
	{
//	public:
//		UserCropParameters() {}
//		virtual ~UserCropParameters() {}

    double pc_CanopyReflectionCoefficient{0.0};
    double pc_ReferenceMaxAssimilationRate{0.0};
    double pc_ReferenceLeafAreaIndex{0.0};
    double pc_MaintenanceRespirationParameter1{0.0};
    double pc_MaintenanceRespirationParameter2{0.0};
    double pc_MinimumNConcentrationRoot{0.0};
    double pc_MinimumAvailableN{0.0};
    double pc_ReferenceAlbedo{0.0};
    double pc_StomataConductanceAlpha{0.0};
    double pc_SaturationBeta{0.0};
    double pc_GrowthRespirationRedux{0.0};
    double pc_MaxCropNDemand{0.0};
    double pc_GrowthRespirationParameter1{0.0};
    double pc_GrowthRespirationParameter2{0.0};
    double pc_Tortuosity{0.0};
	};

	//----------------------------------------------------------------------------

	/**
	 * Class that holds information about user defined environment parameters.
	 * @author Xenia Specka
	 */
  struct UserEnvironmentParameters
	{
//	public:
//		UserEnvironmentParameters() :
//			p_MaxGroundwaterDepth(20),
//			p_MinGroundwaterDepth(20)
//		{}

//		virtual ~UserEnvironmentParameters() {}

    bool p_UseAutomaticIrrigation{false};
    bool p_UseNMinMineralFertilisingMethod{false};
    bool p_UseSecondaryYields{true};
    bool p_UseAutomaticHarvestTrigger{false};

    double p_LayerThickness{0.0};
    double p_Albedo{0.0};
    double p_AthmosphericCO2{0.0};
    double p_WindSpeedHeight{0.0};
    double p_LeachingDepth{0.0};
    double p_timeStep{0.0};
    double p_MaxGroundwaterDepth{20.0};
    double p_MinGroundwaterDepth{20.0};

    int p_NumberOfLayers{0};
    int p_StartPVIndex{0};
    int p_JulianDayAutomaticFertilising{0};
    int p_MinGroundwaterDepthMonth{0};
	};

  struct UserInitialValues
	{
//	public:
//		UserInitialValues() :
//			p_initPercentageFC(0.8),
//			p_initSoilNitrate(0.0001),
//			p_initSoilAmmonium(0.0001)
//		{}

//		virtual ~UserInitialValues() {}

    double p_initPercentageFC{0.8};    // Initial soil moisture content in percent field capacity
    double p_initSoilNitrate{0.0001};     // Initial soil nitrate content [kg NO3-N m-3]
    double p_initSoilAmmonium{0.0001};    // Initial soil ammonium content [kg NH4-N m-3]
	};

	//----------------------------------------------------------------------------

	/**
	 * Class that holds information about user defined soil moisture parameters.
	 * @author Xenia Specka
	 */
  struct UserSoilMoistureParameters
	{
//	public:
//		UserSoilMoistureParameters() {}
//		virtual ~UserSoilMoistureParameters() {}

    double pm_CriticalMoistureDepth{0.0};
    double pm_SaturatedHydraulicConductivity{0.0};
    double pm_SurfaceRoughness{0.0};
    double pm_GroundwaterDischarge{0.0};
    double pm_HydraulicConductivityRedux{0.0};
    double pm_SnowAccumulationTresholdTemperature{0.0};
    double pm_KcFactor{0.0};
    double pm_TemperatureLimitForLiquidWater{0.0};
    double pm_CorrectionSnow{0.0};
    double pm_CorrectionRain{0.0};
    double pm_SnowMaxAdditionalDensity{0.0};
    double pm_NewSnowDensityMin{0.0};
    double pm_SnowRetentionCapacityMin{0.0};
    double pm_RefreezeParameter1{0.0};
    double pm_RefreezeParameter2{0.0};
    double pm_RefreezeTemperature{0.0};
    double pm_SnowMeltTemperature{0.0};
    double pm_SnowPacking{0.0};
    double pm_SnowRetentionCapacityMax{0.0};
    double pm_EvaporationZeta{0.0};
    double pm_XSACriticalSoilMoisture{0.0};
    double pm_MaximumEvaporationImpactDepth{0.0};
    double pm_MaxPercolationRate{0.0};
    double pm_MoistureInitValue{0.0};
	};

	//----------------------------------------------------------------------------

	/**
	 * Class that holds information about user defined soil temperature parameters.
	 * @author Xenia Specka
	 */
  struct UserSoilTemperatureParameters
	{
//	public:
//		UserSoilTemperatureParameters() {pt_SoilMoisture = 0.25;}
//		virtual ~UserSoilTemperatureParameters() {}

    double pt_NTau{0.0};
    double pt_InitialSurfaceTemperature{0.0};
    double pt_BaseTemperature{0.0};
    double pt_QuartzRawDensity{0.0};
    double pt_DensityAir{0.0};
    double pt_DensityWater{0.0};
    double pt_DensityHumus{0.0};
    double pt_SpecificHeatCapacityAir{0.0};
    double pt_SpecificHeatCapacityQuartz{0.0};
    double pt_SpecificHeatCapacityWater{0.0};
    double pt_SpecificHeatCapacityHumus{0.0};
    double pt_SoilAlbedo{0.0};
    double pt_SoilMoisture{0.25};
	};

	//----------------------------------------------------------------------------

	/**
	 * Class that holds information about user defined soil transport parameters.
	 * @author Xenia Specka
	 */
  struct UserSoilTransportParameters
	{
//	public:
//		UserSoilTransportParameters() {}
//		~UserSoilTransportParameters() {}

    double pq_DispersionLength{0.0};
    double pq_AD{0.0};
    double pq_DiffusionCoefficientStandard{0.0};
    double pq_NDeposition{0.0};

	};

	//----------------------------------------------------------------------------

	/**
	 * Class that holds information about user-defined soil organic parameters.
	 * @author Claas Nendel
	 */
  struct UserSoilOrganicParameters
	{
//	public:
//		UserSoilOrganicParameters() {}
//		virtual ~UserSoilOrganicParameters() {}

    double po_SOM_SlowDecCoeffStandard{0.0}; //4.30e-5 [d-1], Bruun et al. 2003 4.3e-5
    double po_SOM_FastDecCoeffStandard{0.0}; //1.40e-4 [d-1], from DAISY manual 1.4e-4
    double po_SMB_SlowMaintRateStandard{0.0}; //1.00e-3 [d-1], from DAISY manual original 1.8e-3
    double po_SMB_FastMaintRateStandard{0.0}; //1.00e-2 [d-1], from DAISY manual
    double po_SMB_SlowDeathRateStandard{0.0}; //1.00e-3 [d-1], from DAISY manual
    double po_SMB_FastDeathRateStandard{0.0}; //1.00e-2 [d-1], from DAISY manual
    double po_SMB_UtilizationEfficiency{0.0}; //0.60 [], from DAISY manual 0.6
    double po_SOM_SlowUtilizationEfficiency{0.0}; //0.40 [], from DAISY manual 0.4
    double po_SOM_FastUtilizationEfficiency{0.0}; //0.50 [], from DAISY manual 0.5
    double po_AOM_SlowUtilizationEfficiency{0.0}; //0.40 [], from DAISY manual original 0.13
    double po_AOM_FastUtilizationEfficiency{0.0}; //0.10 [], from DAISY manual original 0.69
    double po_AOM_FastMaxC_to_N{0.0}; // 1000.0
    double po_PartSOM_Fast_to_SOM_Slow{0.0}; //0.30) [], Bruun et al. 2003
    double po_PartSMB_Slow_to_SOM_Fast{0.0}; //0.60) [], from DAISY manual
    double po_PartSMB_Fast_to_SOM_Fast{0.0}; //0.60 [], from DAISY manual
    double po_PartSOM_to_SMB_Slow{0.0}; //0.0150 [], optimised
    double po_PartSOM_to_SMB_Fast{0.0}; //0.0002 [], optimised
    double po_CN_Ratio_SMB{0.0}; //6.70 [], from DAISY manual
    double po_LimitClayEffect{0.0}; //0.25 [kg kg-1], from DAISY manual
    double po_AmmoniaOxidationRateCoeffStandard{0.0}; //1.0e-1[d-1], from DAISY manual
    double po_NitriteOxidationRateCoeffStandard{0.0}; //9.0e-1[d-1], fudged by Florian Stange
    double po_TransportRateCoeff{0.0}; //0.1 [d-1], from DAISY manual
    double po_SpecAnaerobDenitrification{0.0}; //0.1 //[g gas-N g CO2-C-1]
    double po_ImmobilisationRateCoeffNO3{0.0}; //0.5 //[d-1]
    double po_ImmobilisationRateCoeffNH4{0.0}; //0.5 //[d-1]
    double po_Denit1{0.0}; //0.2 Denitrification parameter
    double po_Denit2{0.0}; //0.8 Denitrification parameter
    double po_Denit3{0.0}; //0.9 Denitrification parameter
    double po_HydrolysisKM{0.0}; //0.00334 from Tabatabai 1973
    double po_ActivationEnergy{0.0}; //41000.0 from Gould et al. 1973
    double po_HydrolysisP1{0.0}; //4.259e-12 from Sadeghi et al. 1988
    double po_HydrolysisP2{0.0}; //1.408e-12 from Sadeghi et al. 1988
    double po_AtmosphericResistance{0.0}; //0.0025 [s m-1], from Sadeghi et al. 1988
    double po_N2OProductionRate{0.0}; //0.5 [d-1]
    double po_Inhibitor_NH3{0.0}; //1.0 [kg N m-3] NH3-induced inhibitor for nitrite oxidation
	};

	//----------------------------------------------------------------------------

	class SensitivityAnalysisParameters
	{
	public:
		SensitivityAnalysisParameters()
			: p_MeanFieldCapacity(UNDEFINED),
				p_MeanBulkDensity(UNDEFINED),
				p_HeatConductivityFrozen(UNDEFINED),
				p_HeatConductivityUnfrozen(UNDEFINED),
				p_LatentHeatTransfer(UNDEFINED),
				p_ReducedHydraulicConductivity(UNDEFINED),
				vs_FieldCapacity(UNDEFINED),
				vs_Saturation(UNDEFINED),
				vs_PermanentWiltingPoint(UNDEFINED),
				vs_SoilMoisture(UNDEFINED),
				vs_SoilTemperature(UNDEFINED),
				vc_SoilCoverage(UNDEFINED),
				vc_MaxRootingDepth(UNDEFINED),
				vc_RootDiameter(UNDEFINED),
				sa_crop_id(-1)
		{
			crop_parameters.pc_InitialKcFactor = UNDEFINED;
			crop_parameters.pc_StageAtMaxHeight = UNDEFINED;
			crop_parameters.pc_CropHeightP1 = UNDEFINED;
			crop_parameters.pc_CropHeightP2 = UNDEFINED;
			crop_parameters.pc_LuxuryNCoeff = UNDEFINED;
			crop_parameters.pc_ResidueNRatio = UNDEFINED;
			crop_parameters.pc_CropSpecificMaxRootingDepth = UNDEFINED;
			crop_parameters.pc_RootPenetrationRate = UNDEFINED;
			crop_parameters.pc_RootGrowthLag = UNDEFINED;
			crop_parameters.pc_InitialRootingDepth = UNDEFINED;
			crop_parameters.pc_RootFormFactor = UNDEFINED;
			crop_parameters.pc_MaxNUptakeParam = UNDEFINED;
			crop_parameters.pc_CarboxylationPathway = UNDEFINED_INT;
			crop_parameters.pc_MaxAssimilationRate = UNDEFINED;
			crop_parameters.pc_MaxCropDiameter = UNDEFINED;
			crop_parameters.pc_MinimumNConcentration = UNDEFINED;
			crop_parameters.pc_NConcentrationB0 = UNDEFINED;
			crop_parameters.pc_NConcentrationPN =  UNDEFINED;
			crop_parameters.pc_NConcentrationRoot = UNDEFINED;
			crop_parameters.pc_PlantDensity = UNDEFINED;
			crop_parameters.pc_ResidueNRatio = UNDEFINED;

			organic_matter_parameters.vo_AOM_DryMatterContent = UNDEFINED;
			organic_matter_parameters.vo_AOM_NH4Content = UNDEFINED;
			organic_matter_parameters.vo_AOM_NO3Content = UNDEFINED;
			organic_matter_parameters.vo_AOM_CarbamidContent = UNDEFINED;
			organic_matter_parameters.vo_PartAOM_to_AOM_Slow = UNDEFINED;
			organic_matter_parameters.vo_PartAOM_to_AOM_Fast = UNDEFINED;
			organic_matter_parameters.vo_CN_Ratio_AOM_Slow = UNDEFINED;
			organic_matter_parameters.vo_CN_Ratio_AOM_Fast = UNDEFINED;
		}

//		~SensitivityAnalysisParameters() {}

		// soilmoisture module parameters
    double p_MeanFieldCapacity{UNDEFINED};
    double p_MeanBulkDensity{UNDEFINED};
    double p_HeatConductivityFrozen{UNDEFINED};
    double p_HeatConductivityUnfrozen{UNDEFINED};
    double p_LatentHeatTransfer{UNDEFINED};
    double p_ReducedHydraulicConductivity{UNDEFINED};
    double vs_FieldCapacity{UNDEFINED};
    double vs_Saturation{UNDEFINED};
    double vs_PermanentWiltingPoint{UNDEFINED};
    double vs_SoilMoisture{UNDEFINED};
    double vs_SoilTemperature{UNDEFINED};


		// crop parameters
    double vc_SoilCoverage{UNDEFINED};
    double vc_MaxRootingDepth{UNDEFINED};
    double vc_RootDiameter{UNDEFINED};

		CropParameters crop_parameters;
		OrganicMatterParameters organic_matter_parameters;
    int sa_crop_id{-1};
	};

	//----------------------------------------------------------------------------

	/**
	 * @brief Central data distribution class.
	 *
	 * Class that holds pointers and direct information of user defined parameters.
	 *
	 * @author Xenia Specka
	 */
	class CentralParameterProvider
	{
	public:
		CentralParameterProvider();
		CentralParameterProvider(const CentralParameterProvider&);
		virtual ~CentralParameterProvider() {}

		UserCropParameters userCropParameters;
		UserEnvironmentParameters userEnvironmentParameters;
		UserSoilMoistureParameters userSoilMoistureParameters;
		UserSoilTemperatureParameters userSoilTemperatureParameters;
		UserSoilTransportParameters userSoilTransportParameters;
		UserSoilOrganicParameters userSoilOrganicParameters;
		SensitivityAnalysisParameters sensitivityAnalysisParameters;
		UserInitialValues userInitValues;

    Soil::CapillaryRiseRates capillaryRiseRates;

		double getPrecipCorrectionValue(int month) const;
		void setPrecipCorrectionValue(int month, double value);

		bool writeOutputFiles;

	private:
		double precipCorrectionValues[12];
	};

	//----------------------------------------------------------------------------

	CentralParameterProvider readUserParameterFromDatabase(int type = 0);

	void testClimateData(Climate::DataAccessor &climateData);

	CropPtr hermesCropId2Crop(const std::string& hermesCropId);

  //----------------------------------------------------------------------------

  const std::map<int, std::string>& availableMonicaCrops();
}

#endif
