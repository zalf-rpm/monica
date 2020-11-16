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

#ifndef _SOILMOISTURE_H
#define _SOILMOISTURE_H

/**
 * @file soilmoisture.h
 */

#include <vector>

#include "monica-parameters.h"
#include "crop-growth.h"

namespace Monica 
{

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
      SnowComponent(SoilColumn& sc, const SoilMoistureModuleParameters& smps);
      ~SnowComponent() {}

      SnowComponent(SoilColumn& sc, mas::models::monica::SnowModuleState::Reader reader) : soilColumn(sc) { deserialize(reader); }
      void deserialize(mas::models::monica::SnowModuleState::Reader reader);
      void serialize(mas::models::monica::SnowModuleState::Builder builder) const;

      void calcSnowLayer(double vw_MeanAirTemperature, double vc_NetPrecipitation);

      double getVm_SnowDepth() const { return this->vm_SnowDepth; }
      double getWaterToInfiltrate() const { return this->vm_WaterToInfiltrate; }
      double getMaxSnowDepth() const {return this->vm_maxSnowDepth; }
      double getAccumulatedSnowDepth() const {return this->vm_AccumulatedSnowDepth; }

    private:
      double calcSnowMelt(double vw_MeanAirTemperature);
      double calcNetPrecipitation(double mean_air_temperature, double net_precipitation, double& net_precipitation_water, double& net_precipitation_snow);
      double calcRefreeze(double mean_air_temperature);
      double calcNewSnowDensity(double vw_MeanAirTemperature,double vm_NetPrecipitationSnow);
      double calcAverageSnowDensity(double net_precipitation_snow, double new_snow_density);
      double calcLiquidWaterRetainedInSnow(double frozen_water_in_snow, double snow_water_equivalent);
      double calcPotentialInfiltration(double net_precipitation, double snow_layer_water_release, double snow_depth);
      void calcSnowDepth(double snow_water_equivalent);

			SoilColumn& soilColumn;

      double vm_SnowDensity{ 0.0 }; //!< Snow density [kg dm-3]
      double vm_SnowDepth{ 0.0 }; //!< Snow depth [mm]
      double vm_FrozenWaterInSnow{ 0.0 }; //!< [mm]
      double vm_LiquidWaterInSnow{ 0.0 }; //!< [mm]
      double vm_WaterToInfiltrate{ 0.0 }; //!< [mm]
      double vm_maxSnowDepth{ 0.0 };     //!< [mm]
      double vm_AccumulatedSnowDepth{ 0.0 }; //!< [mm]

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
      FrostComponent(SoilColumn& sc,
                     double pm_HydraulicConductivityRedux,
                     double p_timeStep);

      FrostComponent(SoilColumn& sc, mas::models::monica::FrostModuleState::Reader reader) : soilColumn(sc) { deserialize(reader); }
      void deserialize(mas::models::monica::FrostModuleState::Reader reader);
      void serialize(mas::models::monica::FrostModuleState::Builder builder) const;

      void calcSoilFrost(double mean_air_temperature, double snow_depth);
      double getFrostDepth() const { return vm_FrostDepth; }
      double getThawDepth() const { return vm_ThawDepth; }
      double getLambdaRedux(size_t layer) const { return vm_LambdaRedux[layer]; }
      double getAccumulatedFrostDepth() const { return vm_accumulatedFrostDepth; }
      double getTemperatureUnderSnow() const { return vm_TemperatureUnderSnow; }

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
      double vm_FrostDepth{0.0};
      double vm_accumulatedFrostDepth{0.0};
      double vm_NegativeDegreeDays{0.0}; //!< Counts negative degree-days under snow
      double vm_ThawDepth{0.0};
      int vm_FrostDays{0};
      std::vector<double> vm_LambdaRedux; //!< Reduction factor for Lambda []
      double vm_TemperatureUnderSnow{0.0};


      // user defined or data base parameter
      double vm_HydraulicConductivityRedux{0.0};
      double pt_TimeStep{0.0};

      double pm_HydraulicConductivityRedux{0.0};
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
  class SoilMoisture
  {
  public:
    SoilMoisture(MonicaModel& monica);

    SoilMoisture(MonicaModel& monica, mas::models::monica::SoilMoistureModuleState::Reader reader)
    : monica(monica) { deserialize(reader); }
    void deserialize(mas::models::monica::SoilMoistureModuleState::Reader reader);
    void serialize(mas::models::monica::SoilMoistureModuleState::Builder builder) const;

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
		int vs_JulianDay,
		double vw_ReferenceEvapotranspiration);

    //void fm_SoilMoistureUpdate();
    double get_SnowDepth() const;

    // Getter
    double get_SoilMoisture(int layer) const;
    double get_CapillaryRise(int layer) const;
    double get_PercolationRate(int layer) const;

    //! Returns infiltration value [mm]
    double get_Infiltration() const { return vm_Infiltration; }

    //! Returns surface water storage [mm]
    double get_SurfaceWaterStorage() const { return vm_SurfaceWaterStorage; }

    //! Returns surface overflow [mm]
    double get_SurfaceRunOff() const { return vm_SurfaceRunOff; }

    //! Returns evapotranspiration value [mm]
    double get_ActualEvapotranspiration() const { return vm_ActualEvapotranspiration; }

		double get_PotentialEvapotranspiration() const
		{
			return get_ET0() * get_KcFactor();
		}

    //! Returns evaporation value [mm]
    double get_ActualEvaporation() const { return vm_ActualEvaporation; }

    //! Returns reference evapotranspiration [mm]
    double get_ET0() const { return vm_ReferenceEvapotranspiration; }

    //! Returns percentage soil coverage.
    double get_PercentageSoilCoverage() const { return vc_PercentageSoilCoverage; }

    //! Returns stomata resistance [s-1]
    double get_StomataResistance() const { return vc_StomataResistance; }

    //! Returns frost depth [m]
    double get_FrostDepth() const { return frostComponent.getFrostDepth(); }

    //! Returns thaw depth [m]
    double get_ThawDepth() const { return frostComponent.getThawDepth(); }

    double get_CapillaryRise();

    double getMaxSnowDepth() const;

    double getAccumulatedSnowDepth() const;

    double getAccumulatedFrostDepth() const;

		double getTemperatureUnderSnow() const;

    double get_EReducer_1(int i_Layer,
                          double vm_PercentageSoilCoverage,
                          double vm_PotentialEvapotranspiration);

    void put_Crop(Monica::CropGrowth* crop);
    void remove_Crop();

//    void fm_SoilFrost(double vw_MeanAirTemperature,
//                      double vm_SnowDepth);

    void fm_Infiltration(double vm_WaterToInfiltrate,
                         double vc_PercentageSoilCoverage,
                         size_t vm_GroundwaterTable);

    double get_DeprivationFactor(int layerNo, double deprivationDepth,
                                 double zeta, double vs_LayerThickness);

    void fm_CapillaryRise();

    void fm_PercolationWithGroundwater(double vs_GroundwaterDepth);

    void fm_GroundwaterReplenishment();

    void fm_PercolationWithoutGroundwater();

    void fm_BackwaterReplenishment();

    void fm_Evapotranspiration(double vc_PercentageSoilCoverage,
                               double vc_KcFactor,
                               double vs_HeightNN,
                               double vw_MaxAirTemperature,
                               double vw_MinAirTemperature,
                               double vw_RelativeHumidity,
                               double vw_MeanAirTemperature,
                               double vw_WindSpeed,
                               double vw_WindSpeedHeight,
                               double vw_NetRadiation,
                               int vc_DevelopmentalStage,
                               int vs_JulianDay,
                               double vs_Latitude,
							   double vw_ReferenceEvapotranspiration);

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

    //! Returns percolation rate.
    double get_GroundwaterRecharge() const { return vm_FluxAtLowerBoundary; }

    double get_SumSurfaceRunOff() const { return vm_SumSurfaceRunOff; }

    double get_KcFactor() const;
    double get_TranspirationDeficit() const;

  private:
    SoilColumn& soilColumn;
    const SiteParameters& siteParameters;
    MonicaModel& monica;
    const SoilMoistureModuleParameters& smPs;
    const EnvironmentParameters& envPs;
    const CropModuleParameters& cropPs;
		const size_t vm_NumberOfLayers{0};
		const size_t vs_NumberOfLayers{0};

		double vm_ActualEvaporation{0.0}; //!< Sum of evaporation of all layers [mm]
		double vm_ActualEvapotranspiration{0.0}; //!< Sum of evaporation and transpiration of all layers [mm]
		double vm_ActualTranspiration{0.0}; //!< Sum of transpiration of all layers [mm]
    std::vector<double> vm_AvailableWater; //!< Soil available water in [mm]
		double vm_CapillaryRise{0.0}; //!< Capillary rise [mm]
    std::vector<double> pm_CapillaryRiseRate; //!< Capillary rise rate from database in dependence of groundwater distance and texture [m d-1]
    std::vector<double> vm_CapillaryWater; //!< soil capillary water in [mm]
    std::vector<double> vm_CapillaryWater70; //!< 70% of soil capillary water in [mm]
    std::vector<double> vm_Evaporation; //!< Evaporation of layer [mm]
    std::vector<double> vm_Evapotranspiration; //!< Evapotranspiration of layer [mm]
    std::vector<double> vm_FieldCapacity; //!< Soil water content at Field Capacity
		double vm_FluxAtLowerBoundary{0.0}; //! Water flux out of bottom layer [mm]

    std::vector<double> vm_GravitationalWater; //!< Soil water content drained by gravitation only [mm]
    //double vc_GrossPhotosynthesisRate; //!< Gross photosynthesis of crop to estimate reference evapotranspiration [mol m-2 s-1]
		double vm_GrossPrecipitation{0.0}; //!< Precipitation amount that falls on soil and vegetation [mm]
		double vm_GroundwaterAdded{0.0};
		double vm_GroundwaterDischarge{0.0};
    //std::vector<int> vm_GroundwaterDistance; /**< Distance between groundwater table and eff. rooting depth [m] */
		size_t vm_GroundwaterTable{0}; //!< Layer of groundwater table []
    std::vector<double> vm_HeatConductivity; //!< Heat conductivity of layer [J m-1 d-1]
		double vm_HydraulicConductivityRedux{0.0}; //!< Reduction factor for hydraulic conductivity [0.1 - 9.9]
		double vm_Infiltration{0.0}; //!< Amount of water that infiltrates into top soil layer [mm]
		double vm_Interception{0.0}; //!< [mm], water that is intercepted by the crop and evaporates from it's surface; not accountable for soil water budget
		double vc_KcFactor{0.6};
    std::vector<double> vm_Lambda; //!< Empirical soil water conductivity parameter []
		double vm_LambdaReduced{0.0};
		double vs_Latitude{0.0};
    std::vector<double> vm_LayerThickness;
		double pm_LayerThickness{0.0};
		double pm_LeachingDepth{0.0};
		int pm_LeachingDepthLayer{0};
		double vw_MaxAirTemperature{0.0}; //!< [°C]
		double pm_MaxPercolationRate{0.0}; //!< [mm d-1]
		double vw_MeanAirTemperature{0.0}; //!< [°C]
		double vw_MinAirTemperature{0.0}; //!< [°C]
    double vc_NetPrecipitation{0.0}; //!< Precipitation amount that is not intercepted by vegetation [mm]
		double vw_NetRadiation{0.0}; //!< [MJ m-2]
    std::vector<double> vm_PermanentWiltingPoint; //!< Soil water content at permanent wilting point [m3 m-3]
		double vc_PercentageSoilCoverage{0.0}; //!< [m2 m-2]
    std::vector<double> vm_PercolationRate; //!< Percolation rate per layer [mm d-1]
		double vw_Precipitation{0.0}; //!< Precipition taken from weather data [mm]
		double vm_ReferenceEvapotranspiration{6.0}; //!< Evapotranspiration of a 12mm cut grass crop at sufficient water supply [mm]
		double vw_RelativeHumidity{0.0}; //!< [m3 m-3]
    std::vector<double> vm_ResidualEvapotranspiration; //!< Residual evapotranspiration in [mm]
    std::vector<double> vm_SaturatedHydraulicConductivity; //!< Saturated hydraulic conductivity [mm d-1]

    std::vector<double> vm_SoilMoisture; //!< Result - Soil moisture of layer [m3 m-3]
		double vm_SoilMoisture_crit{0};
		double vm_SoilMoistureDeficit{0}; //!< Soil moisture deficit [m3 m-3]
    std::vector<double> vm_SoilPoreVolume; //!< Total soil pore volume [m3]; same as vs_Saturation
		double vc_StomataResistance{0.0};
		double vm_SurfaceRoughness{0.0}; //!< Average amplitude of surface micro-elevations to hold back water in ponds [m]
		double vm_SurfaceRunOff{0.0}; //!< Amount of water running off on soil surface [mm]
		double vm_SumSurfaceRunOff{0.0}; //!< internal accumulation variable
		double vm_SurfaceWaterStorage{0.0}; //!<  Simulates a virtual layer that contains the surface water [mm]
		double pt_TimeStep{0.0};
		double vm_TotalWaterRemoval{0.0}; //!< Total water removal of layer [m3]
    std::vector<double> vm_Transpiration; //!< Transpiration of layer [mm]
		double vm_TranspirationDeficit{0.0};
    std::vector<double> vm_WaterFlux; //!< Soil water flux at the layer's upper boundary[mm d-1]
		double vw_WindSpeed{0.0}; //!< [m s-1]
		double vw_WindSpeedHeight{0.0}; //!< [m]
		double vm_XSACriticalSoilMoisture{0.0};

    SnowComponent snowComponent;
    FrostComponent frostComponent;
    CropGrowth* crop{nullptr};

  }; // SoilMoisture



}

#endif


