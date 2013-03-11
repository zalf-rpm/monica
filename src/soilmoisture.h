#ifndef _SOILMOISTURE_H
#define _SOILMOISTURE_H

/**
 * @file soilmoisture.h
 */

#include <vector>

#include "monica-parameters.h"
#include "crop.h"

namespace Monica {

  // forward declarations
  class SoilColumn;
  class MonicaModel;
  struct SiteParameters;



  //#########################################################################
  // Snow Component
  //#########################################################################

  /**
   * @brief Class for calculating snow depth and density according
   * to specifications of ECOMAG.
   *
   * @author Specka, Nendel
   */
  class SnowComponent
  {
    public:
      SnowComponent(const CentralParameterProvider& cpp);
      ~SnowComponent() {}

      void calcSnowLayer(double vw_MeanAirTemperature, double vc_NetPrecipitation);

      double getVm_SnowDepth() const { return this->vm_SnowDepth; }
      double getWaterToInfiltrate() const { return this->vm_WaterToInfiltrate; }
      double getMaxSnowDepth() const {return this->vm_maxSnowDepth; }
      double accumulatedSnowDepth() const {return this->vm_AccumulatedSnowDepth; }

    private:
      double calcSnowMelt(double vw_MeanAirTemperature);
      double calcNetPrecipitation(double mean_air_temperature, double net_precipitation, double& net_precipitation_water, double& net_precipitation_snow);
      double calcRefreeze(double mean_air_temperature);
      double calcNewSnowDensity(double vw_MeanAirTemperature,double vm_NetPrecipitationSnow);
      double calcAverageSnowDensity(double net_precipitation_snow, double new_snow_density);
      double calcLiquidWaterRetainedInSnow(double frozen_water_in_snow, double snow_water_equivalent);
      double calcPotentialInfiltration(double net_precipitation, double snow_layer_water_release, double snow_depth);
      void calcSnowDepth(double snow_water_equivalent);


      double vm_SnowDensity; /**< Snow density [kg dm-3] */
      double vm_SnowDepth; /**< Snow depth [mm] */
      double vm_FrozenWaterInSnow; /** [mm] */
      double vm_LiquidWaterInSnow; /** [mm] */
      double vm_WaterToInfiltrate; /** [mm] */
      double vm_maxSnowDepth;     //! [mm]
      double vm_AccumulatedSnowDepth; //! [mm]

      const CentralParameterProvider& centralParameterProvider;

      // extern or user defined snow parameter
      double vm_SnowmeltTemperature;                  //!< Base temperature for snowmelt [°C]
      double vm_SnowAccumulationThresholdTemperature;
      double vm_TemperatureLimitForLiquidWater;       //!< Lower temperature limit of liquid water in snow
      double vm_CorrectionRain;                       //!< Correction factor for rain (no correction used here)
      double vm_CorrectionSnow;                       //!< Correction factor for snow (value used in COUP by Lars Egil H.)
      double vm_RefreezeTemperature;                  //!< Base temperature for refreeze [°C]
      double vm_RefreezeP1;                           //!< Refreeze parameter (Karvonen's value)
      double vm_RefreezeP2;                           //!< Refreeze exponent (Karvonen's value)
      double vm_NewSnowDensityMin;                    //!< Minimum density of new snow
      double vm_SnowMaxAdditionalDensity;             //!< Maximum additional density of snow (max rho = 0.35, Karvonen)
      double vm_SnowPacking;                          //!< Snow packing factor (calibrated by Helge Bonesmo)
      double vm_SnowRetentionCapacityMin;             //!< Minimum liquid water retention capacity in snow [mm]
      double vm_SnowRetentionCapacityMax;             //!< Maximum liquid water retention capacity in snow [mm]

  }; // SnowComponent



  //#########################################################################
  // Frost Component
  //#########################################################################

  class FrostComponent
  {
    public:
      FrostComponent(SoilColumn& sc, const CentralParameterProvider& cpp);
      ~FrostComponent() {}

      void calcSoilFrost(double mean_air_temperature, double snow_depth);
      double getFrostDepth() const { return this->vm_FrostDepth; }
      double getThawDepth() const { return this->vm_ThawDepth; }
      double getLambdaRedux(int layer) { return this->vm_LambdaRedux[layer]; }
      double getAccumulatedFrostDepth() { return this->vm_accumulatedFrostDepth; }

    private:

      double getMeanBulkDensity();
      double getMeanFieldCapacity();
      double calcHeatConductivityFrozen(double mean_bulk_density, double sii);
      double calcHeatConductivityUnfrozen(double mean_bulk_density, double mean_field_capacity);
      double calcSii(double mean_field_capacity);
      double calcThawDepth(double temperature_under_snow, double heat_conductivity_unfrozen, double mean_field_capacity);
      double calcTemperatureUnderSnow(double mean_air_temperature, double snow_depth);
      double calcFrostDepth(double mean_field_capacity, double heat_conductivity_frozen, double temperature_under_snow);
      void updateLambdaRedux();


      SoilColumn& soilColumn;
      const CentralParameterProvider& centralParameterProvider;
      double vm_FrostDepth;
      double vm_accumulatedFrostDepth;
      double vm_NegativeDegreeDays; /** Counts negative degree-days under snow */
      double vm_ThawDepth;
      int vm_FrostDays;
      std::vector<double> vm_LambdaRedux; /**< Reduction factor for Lambda [] */


      // user defined or data base parameter
      double vm_HydraulicConductivityRedux;
      double pt_TimeStep;


  };

  //#########################################################################
  // MOISTURE MODULE
  //#########################################################################

  /*!
   * @brief Calculation of the water model based on THESEUS.
   *
   * @author Claas Nendel, Michael Berg.
   *
   * Calculation of a usual layer in the water model.
   * Soil moisture part of MONICA. The graphic belows
   * illustrates some parameter e.g. field capacity or
   * saturation.
   *
   * <img src="../images/boden_wasser_schema.png">
   */
  class SoilMoisture {
  public:
    SoilMoisture(SoilColumn& soilColumn, const SiteParameters& siteParameters,
                 MonicaModel& monica, const CentralParameterProvider& cpp);

    ~SoilMoisture();

    void step(double vs_DepthGroundwaterTable,
              // Wetter Variablen
              double vw_Precipitation,
              double vw_MaxAirTemperature,
              double vw_MinAirTemperature,
              double vw_RelativeHumidity,
              double vw_MeanAirTemperature,
              double vw_WindSpeed,
              double vw_WindSpeedHeight,
              double vw_NetRadiation,
              int vs_JulianDay);

    //void fm_SoilMoistureUpdate();
    double get_SnowDepth() const;

    // Getter
    double get_SoilMoisture(int layer) const;
    double get_CapillaryRise(int layer) const;
    double get_PercolationRate(int layer) const;

    /*!
     * @brief Returns infiltration value [mm]
     * @return Moisture
     */
    double get_Infiltration() const {
      return vm_Infiltration;
    }

    /*!
     * @brief Returns surface water storage [mm]
     * @return water content
     */
    double get_SurfaceWaterStorage() const {
      return vm_SurfaceWaterStorage;
    }

    /*!
     * @brief Returns surface overflow [mm]
     * @return water amount
     */
    double get_SurfaceRunOff() const {
      return vm_SurfaceRunOff;
    }

    /*!
     * @brief Returns evapotranspiration value [mm]
     * @return Evapotranspiration
     */
    double get_Evapotranspiration() const {
      return vm_ActualEvapotranspiration;
    }

    /*!
     * @brief Returns evaporation value [mm]
     * @return Evaporation
     */
    double get_ActualEvaporation() const {
      return vm_ActualEvaporation;
    }


    /*!
     * @brief Returns reference evapotranspiration [mm]
     * @return Value for reference evapotranspiration
     */
    double get_ET0() const {
      return vm_ReferenceEvapotranspiration;
    }


     /*!
     * @brief Returns percentage soil coverage.
     * @return Value for percentage soil coverage
     */
    double get_PercentageSoilCoverage() const {
      return vc_PercentageSoilCoverage;
    }


    /**
     * @brief Returns stomata resistance [s-1]
     * @return Value for stomata resistance
     */
    double get_StomataResistance() const {
      return vc_StomataResistance;
    }


    /**
     * @brief Returns frost depth [m]
     * @return Value for frost depth
     */
    double get_FrostDepth() const {
      return frostComponent->getFrostDepth();
    }

    /**
     * @brief Returns thaw depth [m]
     * @return Value for thaw depth
     */
    double get_ThawDepth() const {
      return frostComponent->getThawDepth();
    }

    double get_CapillaryRise();

    double getMaxSnowDepth() const;

    double accumulatedSnowDepth() const;

    double getAccumulatedFrostDepth() const;

    double get_EReducer_1(int i_Layer,
		      double vm_PercentageSoilCoverage,
		      double vm_PotentialEvapotranspiration);

    void put_Crop(Monica::CropGrowth* crop);
    void remove_Crop();

//    void fm_SoilFrost(double vw_MeanAirTemperature,
//                      double vm_SnowDepth);

    void fm_Infiltration(double vm_WaterToInfiltrate,
                         double vc_PercentageSoilCoverage,
                         int vm_GroundwaterTable);

    double get_DeprivationFactor(int layerNo, double deprivationDepth,
                                 double zeta, double vs_LayerThickness);

    void fm_CapillaryRise();

    void fm_PercolationWithGroundwater(double vs_GroundwaterDepth);

    void fm_GroundwaterReplenishment();

    void fm_PercolationWithoutGroundwater();

    void fm_BackwaterReplenishment();

    void fm_Evapotranspiration(double vc_PercentageSoilCoverage,
                               double vc_KcFactor,
                               double vs_HeightNN, double vw_MaxAirTemperature,
                               double vw_MinAirTemperature,
                               double vw_RelativeHumidity,
                               double vw_MeanAirTemperature,
                               double vw_WindSpeed, double vw_WindSpeedHeight,
                               double vw_NetRadiation,
			 int vc_DevelopmentalStage,
			 int vs_JulianDay,
			 double vs_Latitude);

    double ReferenceEvapotranspiration(double vs_HeightNN,
                                       double vw_MaxAirTemperature,
                                       double vw_MinAirTemperature,
                                       double vw_RelativeHumidity,
                                       double vw_MeanAirTemperature,
                                       double vw_WindSpeed,
                                       double vw_WindSpeedHeight,
			         double vw_NetRadiation,
			         int vs_JulianDay,
			         double vs_Latitude);

    double meanWaterContent(double depth_m) const;
    double meanWaterContent(int layer, int number_of_layers) const;

    /**
     * Returns percolation rate.
     * @return percolation rate
     */
    double get_GroundwaterRecharge() const {
      return vm_FluxAtLowerBoundary;
    }

    double get_SumSurfaceRunOff() const {
      return vm_SumSurfaceRunOff;
    }

    double get_KcFactor() const;
    double get_TranspirationDeficit() const;
  private:

    SoilColumn& soilColumn;
    const SiteParameters& siteParameters;
    MonicaModel& monica;
    const CentralParameterProvider& centralParameterProvider;
    const int vm_NumberOfLayers;
    const int vs_NumberOfLayers;

    double vm_ActualEvaporation; /**< Sum of evaporation of all layers [mm] */
    double vm_ActualEvapotranspiration; /**< Sum of evaporation and transpiration of all layers [mm] */
    double vm_ActualTranspiration; /**< Sum of transpiration of all layers [mm] */
    std::vector<double> vm_AvailableWater; /**< Soil available water in [mm] */
    double vm_CapillaryRise; /**< Capillary rise [mm] */
    std::vector<double> pm_CapillaryRiseRate; /** Capillary rise rate from database in dependence of groundwater distance and texture [m d-1] */
    std::vector<double> vm_CapillaryWater; /**< soil capillary water in [mm] */
    std::vector<double> vm_CapillaryWater70; /**< 70% of soil capillary water in [mm] */
    std::vector<double> vm_Evaporation; /**< Evaporation of layer [mm] */
    std::vector<double> vm_Evapotranspiration; /**< Evapotranspiration of layer [mm] */
    std::vector<double> vm_FieldCapacity; /**< Soil water content at Field Capacity*/
    double vm_FluxAtLowerBoundary; /** Water flux out of bottom layer [mm] */

    std::vector<double> vm_GravitationalWater; /**< Soil water content drained by gravitation only [mm] */
    double vc_GrossPhotosynthesisRate; /**< Gross photosynthesis of crop to estimate reference evapotranspiration [mol m-2 s-1] */
    double vm_GrossPrecipitation; /**< Precipitation amount that falls on soil and vegetation [mm] */
    double vm_GroundwaterAdded;
    double vm_GroundwaterDischarge;
    //std::vector<int> vm_GroundwaterDistance; /**< Distance between groundwater table and eff. rooting depth [m] */
    int vm_GroundwaterTable; /**< Layer of groundwater table [] */
    std::vector<double> vm_HeatConductivity; /** Heat conductivity of layer [J m-1 d-1] */
    double vm_HydraulicConductivityRedux; /**< Reduction factor for hydraulic conductivity [0.1 - 9.9] */
    double vm_Infiltration; /**< Amount of water that infiltrates into top soil layer [mm] */
    double vm_Interception; /**< [mm], water that is intercepted by the crop and evaporates from it's surface; not accountable for soil water budget*/
    double vc_KcFactor;
    std::vector<double> vm_Lambda; /**< Empirical soil water conductivity parameter [] */
    double vm_LambdaReduced; /**<  */
    double vs_Latitude;
    std::vector<double> vm_LayerThickness;
    double pm_LayerThickness;
    double pm_LeachingDepth;
    int pm_LeachingDepthLayer;
    double vw_MaxAirTemperature; /**< [°C] */
    double pm_MaxPercolationRate; /**< [mm d-1] */
    double vw_MeanAirTemperature; /**< [°C] */
    double vw_MinAirTemperature; /**< [°C] */
    double vc_NetPrecipitation; /**< Precipitation amount that is not intercepted by vegetation [mm] */
    double vw_NetRadiation; /**< [MJ m-2] */
    std::vector<double> vm_PermanentWiltingPoint; /** Soil water content at permanent wilting point [m3 m-3] */
    double vc_PercentageSoilCoverage; /** [m2 m-2] */
    std::vector<double> vm_PercolationRate; /**< Percolation rate per layer [mm d-1] */
    double vw_Precipitation; /**< Precipition taken from weather data [mm] */
    double vm_ReferenceEvapotranspiration; /**< Evapotranspiration of a 12mm cut grass crop at sufficient water supply [mm] */
    double vw_RelativeHumidity; /**< [m3 m-3] */
    std::vector<double> vm_ResidualEvapotranspiration; /**< Residual evapotranspiration in [mm] */
    std::vector<double> vm_SaturatedHydraulicConductivity; /**< Saturated hydraulic conductivity [m s-1] */

    std::vector<double> vm_SoilMoisture; /**< Result - Soil moisture of layer [m3 m-3] */
    double vm_SoilMoisture_crit;
    double vm_SoilMoistureDeficit; /**< Soil moisture deficit [m3 m-3] */
    std::vector<double> vm_SoilPoreVolume; /**< Total soil pore volume [m3]; same as vs_Saturation */
    double vc_StomataResistance;
    double vm_SurfaceRoughness; /**< Average amplitude of surface micro-elevations to hold back water in ponds [m] */
    double vm_SurfaceRunOff; /**< Amount of water running off on soil surface [mm] */
    double vm_SumSurfaceRunOff;
    double vm_SurfaceWaterStorage; /**<  Simulates a virtual layer that contains the surface water [mm]*/
    double pt_TimeStep;
    double vm_TotalWaterRemoval; /**< Total water removal of layer [m3] */
    std::vector<double> vm_Transpiration; /**< Transpiration of layer [mm] */
    double vm_TranspirationDeficit;
    std::vector<double> vm_WaterFlux; /**< Soil water flux at the layer's upper boundary[mm d-1] */
    double vw_WindSpeed; /**< [m s-1] */
    double vw_WindSpeedHeight; /**< [m] */
    double vm_XSACriticalSoilMoisture;

    SnowComponent* snowComponent;
    FrostComponent* frostComponent;
    CropGrowth* crop;

  }; // SoilMoisture



}

#endif


