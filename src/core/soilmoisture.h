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

#pragma once

#include <vector>

#include "model/monica/monica_state.capnp.h"
#include "monica-parameters.h"
#include "frost-component.h"
#include "snow-component.h"

namespace monica 
{
class MonicaModel;
class SoilColumn;
class CropModule;

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
  SoilMoisture(MonicaModel& monica, const SoilMoistureModuleParameters& smPs);

  SoilMoisture(MonicaModel& monica, mas::schema::model::monica::SoilMoistureModuleState::Reader reader, CropModule* cropModule = nullptr);
  void deserialize(mas::schema::model::monica::SoilMoistureModuleState::Reader reader);
  void serialize(mas::schema::model::monica::SoilMoistureModuleState::Builder builder) const;

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
  double vw_ReferenceEvapotranspiration,
  double vaporPressure);

  //void fm_SoilMoistureUpdate();
  double getSnowDepth() const;

  // Getter
  double get_SoilMoisture(int layer) const;
  double get_CapillaryRise(int layer) const;
  double get_PercolationRate(int layer) const;

  double waterFlux(int layer) const { return vm_WaterFlux.at(layer); }
  double percolationRate(int layer) const { return vm_PercolationRate.at(layer); }

  double get_Infiltration() const { return vm_Infiltration; } // [mm]

  double get_SurfaceWaterStorage() const { return vm_SurfaceWaterStorage; } // [mm]

  double get_SurfaceRunOff() const { return vm_SurfaceRunOff; } // [mm]

  double get_ActualEvapotranspiration() const { return vm_ActualEvapotranspiration; } // [mm]

  double get_PotentialEvapotranspiration() const { return get_ET0() * getKcFactor(); }

  double get_ActualEvaporation() const { return vm_ActualEvaporation; } // [mm]

  double get_ET0() const { return vm_ReferenceEvapotranspiration; } // [mm]

  double get_PercentageSoilCoverage() const { return vc_PercentageSoilCoverage; }

  double get_StomataResistance() const { return vc_StomataResistance; } // [s-1]

  double get_FrostDepth() const; // [m]

  double get_ThawDepth() const; // [m]

  double get_CapillaryRise();

  double getMaxSnowDepth() const;

  double getAccumulatedSnowDepth() const;

  double getAccumulatedFrostDepth() const;

  double getTemperatureUnderSnow() const;

  std::pair<double, double> getSnowDepthAndCalcTemperatureUnderSnow(double avgAirTemp) const;

  double eReducer1(int layerIndex,
                        double percentageSoilCoverage,
                        double vm_PotentialEvapotranspiration,
                        int evaporationReductionMethod = 1);

  void putCrop(monica::CropModule* cm) { cropModule = cm; }
  void removeCrop() { cropModule = nullptr; }

  void fm_Infiltration(double vm_WaterToInfiltrate);

  double get_DeprivationFactor(int layerNo, double deprivationDepth,
                                double zeta, double vs_LayerThickness);

  void fm_CapillaryRise();

  void fm_PercolationWithGroundwater(size_t oscillGroundwaterLayer);

  void fm_GroundwaterReplenishment();

  void fm_PercolationWithoutGroundwater();

  void fm_BackwaterReplenishment();

  void fm_Evapotranspiration(double vs_HeightNN,
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
                double referenceEvapotranspiration);

  double referenceEvapotranspiration(double vs_HeightNN,
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

  double getKcFactor() const;

  double vm_EvaporatedFromSurface{0.0}; //!< Amount of water evaporated from surface [mm]

//private:
  SoilColumn& soilColumn;
  const SiteParameters& siteParameters;
  MonicaModel& monica;
  SoilMoistureModuleParameters _params;
  const EnvironmentParameters& envPs;
  const CropModuleParameters& cropPs;
  size_t numberOfMoistureLayers{0};
  size_t numberOfSoilLayers{0};

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
  size_t vm_GroundwaterTableLayer{0}; //!< Layer of groundwater table []
  std::vector<double> vm_HeatConductivity; //!< Heat conductivity of layer [J m-1 d-1]
  double vm_HydraulicConductivityRedux{0.0}; //!< Reduction factor for hydraulic conductivity [0.1 - 9.9]
  double vm_Infiltration{0.0}; //!< Amount of water that infiltrates into top soil layer [mm]
  double vm_Interception{0.0}; //!< [mm], water that is intercepted by the crop and evaporates from it's surface; not accountable for soil water budget
  double vc_KcFactor{0.6};
  std::vector<double> vm_Lambda; //!< Empirical soil water conductivity parameter []
  double vs_Latitude{0.0};
  std::vector<double> vm_LayerThickness;
  double pm_LayerThickness{0.0};
  double pm_LeachingDepth{0.0};
  int pm_LeachingDepthLayer{0};
  double pm_MaxPercolationRate{0.0}; //!< [mm d-1]
  double vc_NetPrecipitation{0.0}; //!< Precipitation amount that is not intercepted by vegetation [mm]
  double vw_NetRadiation{0.0}; //!< [MJ m-2]
  std::vector<double> vm_PermanentWiltingPoint; //!< Soil water content at permanent wilting point [m3 m-3]
  double vc_PercentageSoilCoverage{0.0}; //!< [m2 m-2]
  std::vector<double> vm_PercolationRate; //!< Percolation rate per layer [mm d-1]
  double vm_ReferenceEvapotranspiration{6.0}; //!< Evapotranspiration of a 12mm cut grass crop at sufficient water supply [mm]
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
  std::vector<double> vm_WaterFlux; //!< Soil water flux at the layer's upper boundary[mm d-1]
  double vm_XSACriticalSoilMoisture{0.0};

  kj::Own<SnowComponent> snowComponent;
  kj::Own<FrostComponent> frostComponent;
  CropModule* cropModule{nullptr};

  double _vaporPressure{-1.0}; //[kPA]
}; 

} // namespace monica


