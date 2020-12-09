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

#include <algorithm> //for min, max
#include <iostream>
#include <cmath>

/**
 * @file soilmoisture.cpp
 */

#include "soilmoisture.h"
#include "soilcolumn.h"
#include "crop-growth.h"
#include "monica-model.h"
#include "tools/debug.h"
#include "tools/algorithms.h"
#include "soil/conversion.h"

using namespace std;
using namespace Monica;
using namespace Tools;

// ------------------------------------------------------------------------
// SNOW MODULE
// ------------------------------------------------------------------------

/**
 * @brief Constructor
 *
 * Default initialization of snow density,
 * snow depth, frotzen water in snow and
 * liquid water in snow. Snow parameters from
 * user data base are initialized.
 */
SnowComponent::SnowComponent(SoilColumn& sc, const SoilMoistureModuleParameters& smps) 
  : soilColumn(sc)
  , vm_SnowmeltTemperature(smps.pm_SnowMeltTemperature) // Base temperature for snowmelt [°C]
  , vm_SnowAccumulationThresholdTemperature(smps.pm_SnowAccumulationTresholdTemperature)
  , vm_TemperatureLimitForLiquidWater(smps.pm_TemperatureLimitForLiquidWater) // Lower temperature limit of liquid water in snow
  , vm_CorrectionRain(smps.pm_CorrectionRain) // Correction factor for rain (no correction used here)
  , vm_CorrectionSnow(smps.pm_CorrectionSnow) // Correction factor for snow (value used in COUP by Lars Egil H.)
  , vm_RefreezeTemperature(smps.pm_RefreezeTemperature) // Base temperature for refreeze [°C]
  , vm_RefreezeP1(smps.pm_RefreezeParameter1) // Refreeze parameter (Karvonen's value)
  , vm_RefreezeP2(smps.pm_RefreezeParameter2) // Refreeze exponent (Karvonen's value)
  , vm_NewSnowDensityMin(smps.pm_NewSnowDensityMin) // Minimum density of new snow
  , vm_SnowMaxAdditionalDensity(smps.pm_SnowMaxAdditionalDensity) // Maximum additional density of snow (max rho = 0.35, Karvonen)
  , vm_SnowPacking(smps.pm_SnowPacking) // Snow packing factor (calibrated by Helge Bonesmo)
  , vm_SnowRetentionCapacityMin(smps.pm_SnowRetentionCapacityMin) // Minimum liquid water retention capacity in snow [mm]
  , vm_SnowRetentionCapacityMax(smps.pm_SnowRetentionCapacityMax) { // Maximum liquid water retention capacity in snow [mm]
//  cout << "Monica: vm_SnowmeltTemperature " << vm_SnowmeltTemperature << endl;
//  cout << "Monica: vm_CorrectionRain " << vm_CorrectionRain << endl;
//  cout << "Monica: vm_SnowMaxAdditionalDensity " << vm_SnowMaxAdditionalDensity << endl;
}

/*!
 * @brief Calculation of snow layer
 *
 * Calculation of snow layer thickness, density and water content according to
 * ECOMAG
 *
 * @param vw_MeanAirTemperature
 * @param vc_NetPrecipitation
 */
void
SnowComponent::calcSnowLayer(double mean_air_temperature, double net_precipitation) {
  // Calcs netto precipitation
  double net_precipitation_snow = 0.0;
  double net_precipitation_water = 0.0;
  net_precipitation = calcNetPrecipitation(mean_air_temperature, net_precipitation, net_precipitation_water, net_precipitation_snow);

  // Calculate snowmelt
  double vm_Snowmelt = calcSnowMelt(mean_air_temperature);

  // Calculate refreeze in snow
  double vm_Refreeze = calcRefreeze(mean_air_temperature);

  // Calculate density of newly fallen snow
  double vm_NewSnowDensity = calcNewSnowDensity(mean_air_temperature, net_precipitation_snow);

  // Calculate average density of whole snowpack
  vm_SnowDensity = calcAverageSnowDensity(net_precipitation_snow, vm_NewSnowDensity);


  // Calculate amounts of water in frozen snow and liquid form
  vm_FrozenWaterInSnow = vm_FrozenWaterInSnow + net_precipitation_snow - vm_Snowmelt + vm_Refreeze;
  vm_LiquidWaterInSnow = vm_LiquidWaterInSnow + net_precipitation_water + vm_Snowmelt - vm_Refreeze;
  double vm_SnowWaterEquivalent = vm_FrozenWaterInSnow + vm_LiquidWaterInSnow; // snow water equivalent [mm]

  // Calculate snow's capacity to retain liquid
  double vm_LiquidWaterRetainedInSnow = calcLiquidWaterRetainedInSnow(vm_FrozenWaterInSnow, vm_SnowWaterEquivalent);

  // Calculate water release from snow
  double vm_SnowLayerWaterRelease = 0.0;
  if (vm_Refreeze > 0.0) {
    vm_SnowLayerWaterRelease = 0.0;
  } else if (vm_LiquidWaterInSnow <= vm_LiquidWaterRetainedInSnow) {
    vm_SnowLayerWaterRelease = 0;
  } else {
    vm_SnowLayerWaterRelease = vm_LiquidWaterInSnow - vm_LiquidWaterRetainedInSnow;
    vm_LiquidWaterInSnow -= vm_SnowLayerWaterRelease;
    vm_SnowWaterEquivalent = vm_FrozenWaterInSnow + vm_LiquidWaterInSnow;
  }

  // Calculate snow depth from snow water equivalent
  calcSnowDepth(vm_SnowWaterEquivalent);

  // Calculate potential infiltration to soil
  vm_WaterToInfiltrate = calcPotentialInfiltration(net_precipitation, vm_SnowLayerWaterRelease, vm_SnowDepth);
}

/**
 *
 * @param vw_MeanAirTemperature
 * @return
 */
double
SnowComponent::calcSnowMelt(double vw_MeanAirTemperature) {
  double vm_MeltingFactor = 1.4 * (vm_SnowDensity / 0.1);
  double vm_Snowmelt = 0.0;

  if (vm_MeltingFactor > 4.7) {
    vm_MeltingFactor = 4.7;
  }

  if (vm_FrozenWaterInSnow <= 0.0) {
    vm_Snowmelt = 0.0;
  } else if (vw_MeanAirTemperature < vm_SnowmeltTemperature) {
    vm_Snowmelt = 0.0;
  } else {
    vm_Snowmelt = vm_MeltingFactor * (vw_MeanAirTemperature - vm_SnowmeltTemperature);
    if (vm_Snowmelt > vm_FrozenWaterInSnow) {
      vm_Snowmelt = vm_FrozenWaterInSnow;
    }
  }

  return vm_Snowmelt;
}

/**
 *
 * @param mean_air_temperature
 * @param net_precipitation
 * @param vm_NetPrecipitationWater
 * @param vm_NetPrecipitationSnow
 * @return
 */
double
SnowComponent::calcNetPrecipitation(
  double mean_air_temperature,
  double net_precipitation,
  double& net_precipitation_water, // return values
  double& net_precipitation_snow)  // return values
{
  double liquid_water_precipitation = 0.0;

  // Calculate forms and proportions of precipitation
  if (mean_air_temperature >= vm_SnowAccumulationThresholdTemperature) {
    liquid_water_precipitation = 1.0;
  } else if (mean_air_temperature <= vm_TemperatureLimitForLiquidWater) {
    liquid_water_precipitation = 0.0;
  } else {
    liquid_water_precipitation = (mean_air_temperature - vm_TemperatureLimitForLiquidWater)
      / (vm_SnowAccumulationThresholdTemperature - vm_TemperatureLimitForLiquidWater);
  }

  net_precipitation_water = liquid_water_precipitation * vm_CorrectionRain * net_precipitation;
  net_precipitation_snow = (1.0 - liquid_water_precipitation) * vm_CorrectionSnow * net_precipitation;

  // Total net precipitation corrected for snow
  net_precipitation = net_precipitation_snow + net_precipitation_water;

  return net_precipitation;
}

/**
 *
 * @param vw_MeanAirTemperature
 * @return
 */
double
SnowComponent::calcRefreeze(double mean_air_temperature) {
  double refreeze = 0.0;
  double refreeze_helper = 0.0;

  // no refreeze if it's too warm
  if (mean_air_temperature > 0) {
    refreeze_helper = 0;
  } else {
    refreeze_helper = mean_air_temperature;
  }

  if (refreeze_helper < vm_RefreezeTemperature) {
    if (vm_LiquidWaterInSnow > 0.0) {
      refreeze = vm_RefreezeP1 * pow((vm_RefreezeTemperature - refreeze_helper), vm_RefreezeP2);
    }
    if (refreeze > vm_LiquidWaterInSnow) {
      refreeze = vm_LiquidWaterInSnow;
    }
  } else {
    refreeze = 0;
  }
  return refreeze;
}

/**
 *
 * @param mean_air_temperature
 * @param net_precipitation_snow
 * @return
 */
double
SnowComponent::calcNewSnowDensity(double mean_air_temperature, double net_precipitation_snow) {
  double new_snow_density = 0.0;
  double snow_density_factor = 0.0;

  if (net_precipitation_snow <= 0.0) {
    // no snow
    new_snow_density = 0.0;
  } else {
    //
    snow_density_factor = (mean_air_temperature - vm_TemperatureLimitForLiquidWater)
      / (vm_SnowAccumulationThresholdTemperature - vm_TemperatureLimitForLiquidWater);
    if (snow_density_factor > 1.0) {
      snow_density_factor = 1.0;
    }
    if (snow_density_factor < 0.0) {
      snow_density_factor = 0.0;
    }
    new_snow_density = vm_NewSnowDensityMin + vm_SnowMaxAdditionalDensity * snow_density_factor;
  }
  return new_snow_density;
}

/**
 *
 * @param vm_NetPrecipitationSnow
 * @return
 */
double
SnowComponent::calcAverageSnowDensity(double net_precipitation_snow, double new_snow_density) {
  double snow_density = 0.0;
  if ((vm_SnowDepth + net_precipitation_snow) <= 0.0) {
    // no snow
    snow_density = 0.0;
  } else {
    snow_density = (((1.0 + vm_SnowPacking) * vm_SnowDensity * vm_SnowDepth) +
      (new_snow_density * net_precipitation_snow)) / (vm_SnowDepth + net_precipitation_snow);
    if (snow_density > (vm_NewSnowDensityMin + vm_SnowMaxAdditionalDensity)) {
      snow_density = vm_NewSnowDensityMin + vm_SnowMaxAdditionalDensity;
    }
  }
  return snow_density;
}

/**
 *
 * @param frozen_water_in_snow
 * @param snow_water_equivalent
 * @return
 */
double
SnowComponent::calcLiquidWaterRetainedInSnow(double frozen_water_in_snow, double snow_water_equivalent) {
  double snow_retention_capacity;
  double liquid_water_retained_in_snow;

  if ((frozen_water_in_snow <= 0.0) || (vm_SnowDensity <= 0.0)) {
    snow_retention_capacity = 0.0;
  } else {
    snow_retention_capacity = vm_SnowRetentionCapacityMax / 10.0 / vm_SnowDensity;

    if (snow_retention_capacity < vm_SnowRetentionCapacityMin)
      snow_retention_capacity = vm_SnowRetentionCapacityMin;
    if (snow_retention_capacity > vm_SnowRetentionCapacityMax)
      snow_retention_capacity = vm_SnowRetentionCapacityMax;
  }

  liquid_water_retained_in_snow = snow_retention_capacity * snow_water_equivalent;
  return liquid_water_retained_in_snow;
}

/**
 *
 * @param net_precipitation
 * @param snow_layer_water_release
 * @param snow_depth
 * @return
 */
double
SnowComponent::calcPotentialInfiltration(double net_precipitation, double snow_layer_water_release, double snow_depth) {
  double water_to_infiltrate = net_precipitation;
  if (snow_depth >= 0.01) {
    vm_WaterToInfiltrate = snow_layer_water_release;
  }
  return water_to_infiltrate;
}

/**
 * Calculates snow depth. If there is no snow, vm_SnowDensity
 * vm_FrozenWaterInSnow and vm_LiquidWaterInSnow are set to zero.
 *
 * @param snow_water_equivalent
 */
void
SnowComponent::calcSnowDepth(double snow_water_equivalent) {
  double pm_WaterDensity = 1.0; // [kg dm-3]
  if (snow_water_equivalent <= 0.0) {
    vm_SnowDepth = 0.0;
  } else {
    vm_SnowDepth = snow_water_equivalent * pm_WaterDensity / vm_SnowDensity; // [mm * kg dm-3 kg-1 dm3]

    // check if new snow depth is higher than maximal snow depth
    if (vm_SnowDepth > vm_maxSnowDepth) {
      vm_maxSnowDepth = vm_SnowDepth;
    }

    if (vm_SnowDepth < 0.01) {
      vm_SnowDepth = 0.0;
    }
  }
  if (vm_SnowDepth == 0.0) {
    vm_SnowDensity = 0.0;
    vm_FrozenWaterInSnow = 0.0;
    vm_LiquidWaterInSnow = 0.0;
  }

  soilColumn.vm_SnowDepth = vm_SnowDepth;
  vm_AccumulatedSnowDepth += vm_SnowDepth;
}


//#########################################################################
// FROST MODULE
//#########################################################################

FrostComponent::FrostComponent(SoilColumn& sc,
  double pm_HydraulicConductivityRedux,
  double p_timeStep)
  : soilColumn(sc),
  vm_LambdaRedux(sc.vs_NumberOfLayers() + 1, 1.0),
  vm_HydraulicConductivityRedux(pm_HydraulicConductivityRedux),
  pt_TimeStep(p_timeStep),
  pm_HydraulicConductivityRedux(pm_HydraulicConductivityRedux) {}

/*!
 * @brief Calculation of soil frost
 *
 * Calculation of soil frost and thaw boundaries according to
 * ECOMAG
 *
 * @param vw_MeanAirTemperature
 * @param vm_SnowDepth
 */
void FrostComponent::calcSoilFrost(double mean_air_temperature, double snow_depth) {
  // calculation of mean values
  double mean_field_capacity = getMeanFieldCapacity();
  double mean_bulk_density = getMeanBulkDensity();

  // heat conductivity for frozen and unfrozen soil
  const double sii = calcSii(mean_field_capacity);
  double heat_conductivity_frozen = calcHeatConductivityFrozen(mean_bulk_density, sii);
  double heat_conductivity_unfrozen = calcHeatConductivityUnfrozen(mean_bulk_density, mean_field_capacity);

  // temperature under snow
  vm_TemperatureUnderSnow = calcTemperatureUnderSnow(mean_air_temperature, snow_depth);

  // frost depth
  vm_FrostDepth = calcFrostDepth(mean_field_capacity, heat_conductivity_frozen, vm_TemperatureUnderSnow);
  vm_accumulatedFrostDepth += vm_FrostDepth;

  // thaw depth
  vm_ThawDepth = calcThawDepth(vm_TemperatureUnderSnow, heat_conductivity_unfrozen, mean_field_capacity);

  updateLambdaRedux();
}


/**
 * @brief Calculates mean of bulk density
 * @return Mean bulk density
 */
double
FrostComponent::getMeanBulkDensity() {
  auto vs_number_of_layers = soilColumn.vs_NumberOfLayers();
  double bulk_density_accu = 0.0;
  for (int i_Layer = 0; i_Layer < vs_number_of_layers; i_Layer++) {
    bulk_density_accu += soilColumn[i_Layer].vs_SoilBulkDensity();
  }
  return (bulk_density_accu / double(vs_number_of_layers) / 1000.0); // [Mg m-3]
}

/**
 * Calculates current mean of field capacity
 * @return Mean field capacity
 */
double
FrostComponent::getMeanFieldCapacity() {
  auto vs_number_of_layers = soilColumn.vs_NumberOfLayers();
  double mean_field_capacity_accu = 0.0;
  for (int i_Layer = 0; i_Layer < vs_number_of_layers; i_Layer++) {
    mean_field_capacity_accu += soilColumn[i_Layer].vs_FieldCapacity();
  }
  return (mean_field_capacity_accu / double(vs_number_of_layers));
}

/**
 * Approach for frozen soil acroding to Hansson et al. 2004
 * Vadose Zone Journal 3:693-704
 *
 * @param mean_field_capacity
 * @return
 */
double FrostComponent::calcSii(double mean_field_capacity) {
  /** @TODO Parameters to be supplied from outside */
  double pt_F1 = 13.05; // Hansson et al. 2004
  double pt_F2 = 1.06; // Hansson et al. 2004

  const double sii = (mean_field_capacity + (1.0 + (pt_F1 * pow(mean_field_capacity, pt_F2)) *
    mean_field_capacity)) * 100.0;
  return sii;
}

/**
 * Neusypina, T.A. (1979): Rascet teplovo rezima pocvi v modeli formirovanija urozaja.
 * Teoreticeskij osnovy i kolicestvennye metody programmirovanija urozaev. Leningrad,
 * 53 -62.
 * @param mean_bulk_density
 * @param sii
 * @return
 */
double
FrostComponent::calcHeatConductivityFrozen(double mean_bulk_density, double sii) {
  double cond_frozen = ((3.0 * mean_bulk_density - 1.7) * 0.001) / (1.0
    + (11.5 - 5.0 * mean_bulk_density) * exp((-50.0) * pow((sii / mean_bulk_density), 1.5))) * // [cal cm-1 K-1 s-1]
    86400.0 * double(pt_TimeStep) * // [cal cm-1 K-1 d-1]
    4.184 / // [J cm-1 K-1 d-1]
    1000000.0 * 100;//  [MJ m-1 K-1 d-1]

  return cond_frozen;
}

/**
 * Neusypina, T.A. (1979): Rascet teplovo rezima pocvi v modeli formirovanija urozaja.
 * Teoreticeskij osnovy i kolicestvennye metody programmirovanija urozaev. Leningrad,
 * 53 -62.
 * @param mean_bulk_density
 * @param theta
 * @return
 */
double
FrostComponent::calcHeatConductivityUnfrozen(double mean_bulk_density, double mean_field_capacity) {
  double cond_unfrozen = ((3.0 * mean_bulk_density - 1.7) * 0.001) / (1.0 + (11.5 - 5.0
    * mean_bulk_density) * exp((-50.0) * pow(((mean_field_capacity * 100.0) / mean_bulk_density), 1.5)))
    * double(pt_TimeStep) * // [cal cm-1 K-1 s-1]
    4.184 * // [J cm-1 K-1 s-1]
    100.0; // [W m-1 K-1]

  return cond_unfrozen;
}

/**
 *
 * @param vm_TemperatureUnderSnow
 * @param heat_conductivity_unfrozen
 * @param mean_field_capacity
 * @return
 */
double
FrostComponent::calcThawDepth(double temperature_under_snow, double heat_conductivity_unfrozen, double mean_field_capacity) {
  double thaw_helper1 = 0.0;
  double thaw_helper2 = 0.0;
  double thaw_helper3 = 0.0;
  double thaw_helper4 = 0.0;

  double thaw_depth = 0.0;

  if (temperature_under_snow < 0.0) {
    thaw_helper1 = temperature_under_snow * -1.0;
  } else {
    thaw_helper1 = temperature_under_snow;
  }

  if (vm_FrostDepth == 0.0) {
    thaw_helper2 = 0.0;
  } else {
    /** @todo Claas: check that heat conductivity is in correct unit! */
    thaw_helper2 = sqrt(2.0 * heat_conductivity_unfrozen * thaw_helper1 / (1000.0 * 79.0
      * (mean_field_capacity * 100.0) / 100.0));
  }

  if (temperature_under_snow < 0.0) {
    thaw_helper3 = thaw_helper2 * -1.0;
  } else {
    thaw_helper3 = thaw_helper2;
  }

  thaw_helper4 = this->vm_ThawDepth + thaw_helper3;

  if (thaw_helper4 < 0.0) {
    thaw_depth = 0.0;
  } else {
    thaw_depth = thaw_helper4;
  }
  return thaw_depth;
}

/**
 *
 * @param mean_field_capacity
 * @param heat_conductivity_frozen
 * @param temperature_under_snow
 * @return
 */
double
FrostComponent::calcFrostDepth(double mean_field_capacity, double heat_conductivity_frozen, double temperature_under_snow) {
  double frost_depth = 0.0;

  // Heat released/absorbed on freezing/thawing
  double latent_heat = 1000.0 * (mean_field_capacity * 100.0) / 100.0 * 0.335;

  // Summation of number of days with frost
  if (this->vm_FrostDepth > 0.0) {
    vm_FrostDays++;
  }

  // Ratio of energy sum from subsoil to vm_LatentHeat
  double latent_heat_transfer = 0.3 * vm_FrostDays / latent_heat;

  // Calculate temperature under snowpack
  /** @todo Claas: At a later stage temperature under snow to pass on to soil
   * surface temperature calculation in temperature module */
  if (temperature_under_snow < 0.0) {
    this->vm_NegativeDegreeDays -= temperature_under_snow;
  }

  if (this->vm_NegativeDegreeDays < 0.01) {
    frost_depth = 0.0;
  } else {
    frost_depth = sqrt(((latent_heat_transfer / 2.0) * (latent_heat_transfer / 2.0)) + (2.0
      * heat_conductivity_frozen * vm_NegativeDegreeDays / latent_heat)) - (latent_heat_transfer / 2.0);
  }
  return frost_depth;
}

/**
 *
 * @param mean_air_temperature
 * @param snow_depth
 * @return
 */
double
FrostComponent::calcTemperatureUnderSnow(double mean_air_temperature, double snow_depth) {
  double temperature_under_snow = 0.0;
  if (snow_depth / 100.0 < 0.01) {
    temperature_under_snow = mean_air_temperature;
  } else if (vm_FrostDepth < 0.01) {
    temperature_under_snow = mean_air_temperature;
  } else {
    temperature_under_snow = mean_air_temperature / (1.0 + (10.0 * snow_depth / 100.0) / this->vm_FrostDepth);
  }
  return temperature_under_snow;
}

/**
 *
 */
void
FrostComponent::updateLambdaRedux() {
  auto vs_number_of_layers = soilColumn.vs_NumberOfLayers();

  for (int i_Layer = 0; i_Layer < vs_number_of_layers; i_Layer++) {

    if (i_Layer < (std::floor((vm_FrostDepth / soilColumn[i_Layer].vs_LayerThickness) + 0.5))) {

      // soil layer is frozen
      soilColumn[i_Layer].vs_SoilFrozen = true;
      vm_LambdaRedux[i_Layer] = 0.0;

      if (i_Layer == 0) {
        vm_HydraulicConductivityRedux = 0.0;
      }
    }

    if (i_Layer < (std::floor((vm_ThawDepth / soilColumn[i_Layer].vs_LayerThickness) + 0.5))) {
      // soil layer is thawing

      if (vm_ThawDepth < (double(i_Layer + 1) * soilColumn[i_Layer].vs_LayerThickness) && (vm_ThawDepth < vm_FrostDepth)) {
        // soil layer is thawing but there is more frost than thaw
        soilColumn[i_Layer].vs_SoilFrozen = true;
        vm_LambdaRedux[i_Layer] = 0.0;
        if (i_Layer == 0) {
          vm_HydraulicConductivityRedux = 0.0;
        }

      } else {
        // soil is thawing
        soilColumn[i_Layer].vs_SoilFrozen = false;
        vm_LambdaRedux[i_Layer] = 1.0;
        if (i_Layer == 0) {
          vm_HydraulicConductivityRedux = 0.1;
        }
      }
    }

    // no more frost, because all layers are thawing
    if (vm_ThawDepth >= vm_FrostDepth) {
      vm_ThawDepth = 0.0;
      vm_FrostDepth = 0.0;
      vm_NegativeDegreeDays = 0.0;
      vm_FrostDays = 0;

      vm_HydraulicConductivityRedux = pm_HydraulicConductivityRedux;
      for (int i_Layer = 0; i_Layer < vs_number_of_layers; i_Layer++) {
        soilColumn[i_Layer].vs_SoilFrozen = false;
        vm_LambdaRedux[i_Layer] = 1.0;
      }
    }
  }
}

//#########################################################################
// MOISTURE MODULE
//#########################################################################

/*!
 * @brief Constructor
 * @param sc Soil Column the moisture is calculated
 * @param stps Site parameters
 * @param mm Monica model
 */
SoilMoisture::SoilMoisture(MonicaModel& mm)
  : soilColumn(mm.soilColumnNC())
  , siteParameters(mm.siteParameters())
  , monica(mm)
  , smPs(mm.soilmoistureParameters())
  , envPs(mm.environmentParameters())
  , cropPs(mm.cropParameters())
  , vm_NumberOfLayers(soilColumn.vs_NumberOfLayers() + 1)
  , vs_NumberOfLayers(soilColumn.vs_NumberOfLayers()) //extern
  , vm_AvailableWater(vm_NumberOfLayers, 0.0) // Soil available water in [mm]
  , pm_CapillaryRiseRate(vm_NumberOfLayers, 0.0)
  , vm_CapillaryWater(vm_NumberOfLayers, 0.0) // soil capillary water in [mm]
  , vm_CapillaryWater70(vm_NumberOfLayers, 0.0) // 70% of soil capillary water in [mm]
  , vm_Evaporation(vm_NumberOfLayers, 0.0) //intern
  , vm_Evapotranspiration(vm_NumberOfLayers, 0.0) //intern
  , vm_FieldCapacity(vm_NumberOfLayers, 0.0)
  , vm_GravitationalWater(vm_NumberOfLayers, 0.0) // Gravitational water in [mm d-1] //intern
//, vm_GroundwaterDistance(vm_NumberOfLayers, 0), // map (joachim)
, vm_HeatConductivity(vm_NumberOfLayers, 0)
, vm_Lambda(vm_NumberOfLayers, 0.0)
, vs_Latitude(siteParameters.vs_Latitude)
, vm_LayerThickness(vm_NumberOfLayers, 0.01)
, vm_PermanentWiltingPoint(vm_NumberOfLayers, 0.0)
, vm_PercolationRate(vm_NumberOfLayers, 0.0) // Percolation rate in [mm d-1] //intern
, vm_ResidualEvapotranspiration(vm_NumberOfLayers, 0.0)
, vm_SoilMoisture(vm_NumberOfLayers, 0.20) //result
, vm_SoilPoreVolume(vm_NumberOfLayers, 0.0)
, vm_Transpiration(vm_NumberOfLayers, 0.0) //intern
, vm_WaterFlux(vm_NumberOfLayers, 0.0)
, snowComponent(soilColumn, smPs)
, frostComponent(soilColumn, smPs.pm_HydraulicConductivityRedux, envPs.p_timeStep) {
  debug() << "Constructor: SoilMoisture" << endl;

  vm_HydraulicConductivityRedux = smPs.pm_HydraulicConductivityRedux;
  pt_TimeStep = envPs.p_timeStep;
  vm_SurfaceRoughness = smPs.pm_SurfaceRoughness;
  vm_GroundwaterDischarge = smPs.pm_GroundwaterDischarge;
  pm_MaxPercolationRate = smPs.pm_MaxPercolationRate;
  pm_LeachingDepth = envPs.p_LeachingDepth;

  //  cout << "pm_LeachingDepth:\t" << pm_LeachingDepth << endl;
  pm_LayerThickness = mm.simulationParameters().p_LayerThickness;

  pm_LeachingDepthLayer = int(std::floor(0.5 + (pm_LeachingDepth / pm_LayerThickness))) - 1;

  for (int i = 0; i < vm_NumberOfLayers; i++) {
    vm_SaturatedHydraulicConductivity.resize(vm_NumberOfLayers, smPs.pm_SaturatedHydraulicConductivity); // original [8640 mm d-1]
  }

  //  double vm_GroundwaterDepth = 0.0;
  //  for (int i_Layer = 0; i_Layer < vs_NumberOfLayers; i_Layer++) {
  //    vm_GroundwaterDepth += soilColumn[i_Layer].vs_LayerThickness;
  //    if (vm_GroundwaterDepth <= siteParameters.vs_GroundwaterDepth) {
  //      vm_GroundwaterTable = i_Layer;
  //    }
  //  }
  //  if (vm_GroundwaterDepth < siteParameters.vs_GroundwaterDepth) {
  //    vm_GroundwaterTable = vs_NumberOfLayers + 2;
  //  }
  //  soilColumn.vm_GroundwaterTable = vm_GroundwaterTable;
  //
  //  for (int i_Layer = vm_NumberOfLayers - 1; i_Layer >= vm_GroundwaterTable; i_Layer--) {
  //    soilColumn[i_Layer].set_Vs_SoilMoisture_m3(soilColumn[i_Layer].get_Saturation());
  //  }
}

MonicaModel::~MonicaModel() {
  delete _currentCropGrowth;
}

/*!
 * @param vs_GroundwaterDepth Depth of ground water table; Now, this parameter is not considered in the calculations;
 * @param vw_Precipitation Precipitation amount
 * @param vw_MinAirTemperature Minimal air temperature
 * @param vw_MaxAirTemperature Maximal air temperature
 * @param vw_RelativeHumidity Relative Humidity
 * @param vw_MeanAirTemperature Mean air temperature
 * @param vw_WindSpeed Speed of wind
 * @param vw_WindSpeedHeight Height for the measurement of the wind speed
 * @param vw_GlobalRadiation Global radiation
 */
void SoilMoisture::step(double vs_GroundwaterDepth,
  double vw_Precipitation,
  double vw_MaxAirTemperature,
  double vw_MinAirTemperature,
  double vw_RelativeHumidity,
  double vw_MeanAirTemperature,
  double vw_WindSpeed,
  double vw_WindSpeedHeight,
  double vw_GlobalRadiation,
  int vs_JulianDay,
  double vw_ReferenceEvapotranspiration) {
  for (int i = 0; i < vs_NumberOfLayers; i++) {
    // initialization with moisture values stored in the layer
    vm_SoilMoisture[i] = soilColumn[i].get_Vs_SoilMoisture_m3();
    vm_WaterFlux[i] = 0.0;
    vm_FieldCapacity[i] = soilColumn[i].vs_FieldCapacity();
    vm_SoilPoreVolume[i] = soilColumn[i].vs_Saturation();
    vm_PermanentWiltingPoint[i] = soilColumn[i].vs_PermanentWiltingPoint();
    vm_LayerThickness[i] = soilColumn[i].vs_LayerThickness;
    vm_Lambda[i] = soilColumn[i].vs_Lambda();
  }

  vm_SoilMoisture[vm_NumberOfLayers - 1] = soilColumn[vm_NumberOfLayers - 2].get_Vs_SoilMoisture_m3();
  vm_WaterFlux[vm_NumberOfLayers - 1] = 0.0;
  vm_FieldCapacity[vm_NumberOfLayers - 1] = soilColumn[vm_NumberOfLayers - 2].vs_FieldCapacity();
  vm_SoilPoreVolume[vm_NumberOfLayers - 1] = soilColumn[vm_NumberOfLayers - 2].vs_Saturation();
  vm_LayerThickness[vm_NumberOfLayers - 1] = soilColumn[vm_NumberOfLayers - 2].vs_LayerThickness;
  vm_Lambda[vm_NumberOfLayers - 1] = soilColumn[vm_NumberOfLayers - 2].vs_Lambda();

  vm_SurfaceWaterStorage = soilColumn.vs_SurfaceWaterStorage;

  bool vc_CropPlanted = false;
  double vc_CropHeight = 0.0;
  int vc_DevelopmentalStage = 0;

  if (monica.cropGrowth()) {
    vc_CropPlanted = true;
    vc_PercentageSoilCoverage = monica.cropGrowth()->get_SoilCoverage();
    vc_KcFactor = monica.cropGrowth()->get_KcFactor();
    vc_CropHeight = monica.cropGrowth()->get_CropHeight();
    vc_DevelopmentalStage = monica.cropGrowth()->get_DevelopmentalStage();
    if (vc_DevelopmentalStage > 0) {
      vc_NetPrecipitation = monica.cropGrowth()->get_NetPrecipitation();
    } else {
      vc_NetPrecipitation = vw_Precipitation;
    }

  } else {
    vc_CropPlanted = false;
    vc_KcFactor = smPs.pm_KcFactor;
    vc_NetPrecipitation = vw_Precipitation;
    vc_PercentageSoilCoverage = 0.0;
  }

  // Recalculates current depth of groundwater table
  vm_GroundwaterTable = vs_NumberOfLayers + 2;
  auto vm_GroundwaterHelper = vs_NumberOfLayers - 1;
  for (int i = int(vs_NumberOfLayers - 1); i >= 0; i--) {
    if (vm_SoilMoisture[i] == vm_SoilPoreVolume[i] && (vm_GroundwaterHelper == i)) {
      vm_GroundwaterHelper--;
      vm_GroundwaterTable = i;
    }
  }
  if ((vm_GroundwaterTable > (int(vs_GroundwaterDepth / soilColumn[0].vs_LayerThickness)))
    && (vm_GroundwaterTable < (vs_NumberOfLayers + 2))) {

    vm_GroundwaterTable = (int(vs_GroundwaterDepth / soilColumn[0].vs_LayerThickness));

  } else if (vm_GroundwaterTable >= (vs_NumberOfLayers + 2)) {

    vm_GroundwaterTable = (int(vs_GroundwaterDepth / soilColumn[0].vs_LayerThickness));

  }

  soilColumn.vm_GroundwaterTable = vm_GroundwaterTable;

  // calculates snow layer water storage and release
  snowComponent.calcSnowLayer(vw_MeanAirTemperature, vc_NetPrecipitation);
  double vm_WaterToInfiltrate = snowComponent.getWaterToInfiltrate();

  // Calculates frost and thaw depth and switches lambda
  frostComponent.calcSoilFrost(vw_MeanAirTemperature, snowComponent.getVm_SnowDepth());

  // calculates infiltration of water from surface
  fm_Infiltration(vm_WaterToInfiltrate, vc_PercentageSoilCoverage, vm_GroundwaterTable);

  if ((vs_GroundwaterDepth <= 10.0) && (vs_GroundwaterDepth > 0.0)) {

    fm_PercolationWithGroundwater(vs_GroundwaterDepth);
    fm_GroundwaterReplenishment();

  } else {

    fm_PercolationWithoutGroundwater();
    fm_BackwaterReplenishment();

  }

  fm_Evapotranspiration(vc_PercentageSoilCoverage, vc_KcFactor, siteParameters.vs_HeightNN, vw_MaxAirTemperature,
    vw_MinAirTemperature, vw_RelativeHumidity, vw_MeanAirTemperature, vw_WindSpeed, vw_WindSpeedHeight,
    vw_GlobalRadiation, vc_DevelopmentalStage, vs_JulianDay, vs_Latitude, vw_ReferenceEvapotranspiration);

  fm_CapillaryRise();

  for (int i_Layer = 0; i_Layer < vs_NumberOfLayers; i_Layer++) {
    soilColumn[i_Layer].set_Vs_SoilMoisture_m3(vm_SoilMoisture[i_Layer]);
    soilColumn[i_Layer].vs_SoilWaterFlux = vm_WaterFlux[i_Layer];
    //commented out because old calc_vs_SoilMoisture_pF algorithm is calcualted every time vs_SoilMoisture_pF is accessed
//    soilColumn[i_Layer].calc_vs_SoilMoisture_pF();
  }
  soilColumn.vs_SurfaceWaterStorage = vm_SurfaceWaterStorage;
  soilColumn.vs_FluxAtLowerBoundary = vm_FluxAtLowerBoundary;
}

/*!
 * @brief Calculation of infiltration
 *
 * Calculation of infiltration according to:
 * Wegehenkel, M (2002): Estimating of the impact of land use
 * changes using the conceptual hydrological model THESEUS - a case
 * study. Physics and Chemistry of the Earth 27, 631-640
 *
 * @param vm_WaterToInfiltrate
 * @param vc_PercentageSoilCoverage
 * @param vm_GroundwaterTable
 */
void SoilMoisture::fm_Infiltration(double vm_WaterToInfiltrate, double vc_PercentageSoilCoverage,
  size_t vm_GroundwaterTable) {
  // For receiving daily precipitation data all variables have to be reset
  double vm_RunOffFactor = 0;
  double vm_PotentialInfiltration = 0;
  double vm_ReducedHydraulicConductivity = 0;
  double vm_PercolationFactor = 0;
  double vm_LambdaReduced = 0;

  vm_Infiltration = 0.0;
  vm_Interception = 0.0;
  vm_SurfaceRunOff = 0.0;
  vm_CapillaryRise = 0.0;
  vm_GroundwaterAdded = 0.0;
  vm_ActualTranspiration = 0.0;
  vm_PercolationFactor = 0.0;
  vm_LambdaReduced = 0.0;

  double vm_SurfaceWaterStorageOld = vm_SurfaceWaterStorage;

  // add the netto precipitation to the virtual surface water storage
  vm_SurfaceWaterStorage += vm_WaterToInfiltrate;

  // Calculating potential infiltration in [mm d-1]
  vm_SoilMoistureDeficit = (vm_SoilPoreVolume[0] - vm_SoilMoisture[0]) / vm_SoilPoreVolume[0];
  vm_ReducedHydraulicConductivity = vm_SaturatedHydraulicConductivity[0] * vm_HydraulicConductivityRedux;

  if (vm_ReducedHydraulicConductivity > 0.0) {

    vm_PotentialInfiltration
      = (vm_ReducedHydraulicConductivity * 0.2 * vm_SoilMoistureDeficit * vm_SoilMoistureDeficit);

    // minimum of the availabe amount of water and the amount, soil is able to assimilate water
    // überprüft, dass das zu infiltrierende Wasser nicht größer ist
    // als das Volumnen, welches es aufnehmen kann
    vm_Infiltration = min(vm_SurfaceWaterStorage, vm_PotentialInfiltration);

    /** @todo <b>Claas:</b> Mathematischer Sinn ist zu überprüfen */
    vm_Infiltration = min(vm_Infiltration, ((vm_SoilPoreVolume[0] - vm_SoilMoisture[0]) * 1000.0
      * soilColumn[0].vs_LayerThickness));

    // Limitation of airfilled pore space added to prevent water contents
    // above pore space in layers below (Claas Nendel)
    vm_Infiltration = max(0.0, vm_Infiltration);
  } else {
    vm_Infiltration = 0.0;
  }

  // Updating yesterday's surface water storage
  if (vm_Infiltration > 0.0) {

    // Reduce the water storage with the infiltration amount
    vm_SurfaceWaterStorage -= vm_Infiltration;
  }

  // Calculating overflow due to water level exceeding surface roughness [mm]
  if (vm_SurfaceWaterStorage > (10.0 * vm_SurfaceRoughness / (siteParameters.vs_Slope + 0.001))) {

    // Calculating surface run-off driven by slope and altered by surface roughness and soil coverage
    // minimal slope at which water will be run off the surface
    vm_RunOffFactor = 0.02 + (vm_SurfaceRoughness / 4.0) + (vc_PercentageSoilCoverage / 15.0);
    if (siteParameters.vs_Slope < 0.0 || siteParameters.vs_Slope > 1.0) {

      // no valid slope
      cerr << "Slope value out ouf boundary" << endl;

    } else if (siteParameters.vs_Slope == 0.0) {

      // no slope so there will be no loss of water
      vm_SurfaceRunOff = 0.0;

    } else if (siteParameters.vs_Slope > vm_RunOffFactor) {

      // add all water from the surface to the run-off storage
      vm_SurfaceRunOff += vm_SurfaceWaterStorage;

    } else {

      // some water is running off because of a sloped surface
      /** @todo Claas: Ist die Formel korrekt? vm_RunOffFactor wird einmal reduziert? */
      vm_SurfaceRunOff += ((siteParameters.vs_Slope * vm_RunOffFactor) / (vm_RunOffFactor * vm_RunOffFactor)) * vm_SurfaceWaterStorage;
    }

    // Update surface water storage
    vm_SurfaceWaterStorage -= vm_SurfaceRunOff;
  }

  // Adding infiltrating water to top layer soil moisture
  vm_SoilMoisture[0] += (vm_Infiltration / 1000.0 / vm_LayerThickness[0]);

  // [m3 m-3] += ([mm] - [mm]) / [] / [m]; --> Conversion into volumetric water content [m3 m-3]
  vm_WaterFlux[0] = vm_Infiltration; // Fluss in Schicht 0

  // Calculating excess soil moisture (water content exceeding field capacity) for percolation
  if (vm_SoilMoisture[0] > vm_FieldCapacity[0]) {

    vm_GravitationalWater[0] = (vm_SoilMoisture[0] - vm_FieldCapacity[0]) * 1000.0
      * vm_LayerThickness[0];
    vm_LambdaReduced = vm_Lambda[0] * frostComponent.getLambdaRedux(0);
    vm_PercolationFactor = 1 + vm_LambdaReduced * vm_GravitationalWater[0];
    vm_PercolationRate[0] = (vm_GravitationalWater[0] * vm_GravitationalWater[0] * vm_LambdaReduced)
      / vm_PercolationFactor;
    if (vm_PercolationRate[0] > pm_MaxPercolationRate) {
      vm_PercolationRate[0] = pm_MaxPercolationRate;
    }
    vm_GravitationalWater[0] = vm_GravitationalWater[0] - vm_PercolationRate[0];
    vm_GravitationalWater[0] = max(0.0, vm_GravitationalWater[0]);



    // Adding the excess water remaining after the percolation event to soil moisture
    vm_SoilMoisture[0] = vm_FieldCapacity[0] + (vm_GravitationalWater[0] / 1000.0
      / vm_LayerThickness[0]);

    // For groundwater table in first or second top layer no percolation occurs
    if (vm_GroundwaterTable <= 1) {
      vm_PercolationRate[0] = 0.0;
    }

    // For groundwater table at soil surface no percolation occurs
    if (vm_GroundwaterTable == 0) {
      vm_PercolationRate[0] = 0.0;

      // For soil water volume exceeding total pore volume, surface runoff occurs
      if (vm_SoilMoisture[0] > vm_SoilPoreVolume[0]) {

        vm_SurfaceRunOff += (vm_SoilMoisture[0] - vm_SoilPoreVolume[0]) * 1000.0 * vm_LayerThickness[0];
        vm_SoilMoisture[0] = vm_SoilPoreVolume[0];
        return;
      }
    }
  } else if (vm_SoilMoisture[0] <= vm_FieldCapacity[0]) {

    // For soil moisture contents below field capacity no excess water and no fluxes occur
    vm_PercolationRate[0] = 0.0;
    vm_GravitationalWater[0] = 0.0;
  }

  // Check water balance

  if (fabs((vm_SurfaceWaterStorageOld + vm_WaterToInfiltrate) - (vm_SurfaceRunOff + vm_Infiltration
    + vm_SurfaceWaterStorage)) > 0.01) {

    cerr << "water balance wrong!" << endl;
  }

  // water flux of next layer equals percolation rate of layer above
  vm_WaterFlux[1] = vm_PercolationRate[0];
  vm_SumSurfaceRunOff += vm_SurfaceRunOff;
}

/*!
 * Returns moisture of the layer specified by parameter.
 * @return moisture
 *
 * @param layer Index of layer
 */
double SoilMoisture::get_SoilMoisture(int layer) const {
  return soilColumn[layer].get_Vs_SoilMoisture_m3();
}

/**
 * Returns flux of capillary rise in given layer.
 * @param layer
 * @return Capillary rise in [mm]
 */
double SoilMoisture::get_CapillaryRise(int layer) const {
  return vm_CapillaryWater.at(layer);
}

/**
 * Returns percolation rate at given layer.
 * @param layer
 * @return Percolation rate in [mm]
 */
double SoilMoisture::get_PercolationRate(int layer) const {
  return vm_PercolationRate.at(layer);
}


/*!
 * @brief Calculates capillary rise (flux), if no groundwater is within the profil
 *
 * Capillary rise only above groundwater table and within layer with a water content
 * less than 70 \% of the current availible field capacity
 *
 * Kapillarer Aufstieg nach hiesiger Methode bedeutet:
 * Suchen der ersten Rechenschicht mit bf \< nfk70 oberhalb GW-Spiegel, Zuordnen der
 * Aufstiegsrate. Kapillarer Aufstieg erfolgt nur in diese Schicht, falls noch eine
 * weitere Schicht diese Bedingung erfuellt, ist der kapillare Aufstieg = 0. (!!),
 * diese Bedingung muss also nach dem Auffinden der ersten Bodenschicht \< nFK70
 * uebersprungen werden !!
 *
 * @param vm_GroundwaterTable First layer that contains groundwater
 *
 */
void SoilMoisture::fm_CapillaryRise() {
  auto vc_RootingDepth = crop ? crop->get_RootingDepth() : 0;
  auto vm_GroundwaterDistance = vm_GroundwaterTable - vc_RootingDepth;// []

  if (vm_GroundwaterDistance < 1)
    vm_GroundwaterDistance = 1;


  if ((double(vm_GroundwaterDistance) * vm_LayerThickness[0]) <= 2.70) { // [m]
    // Capillary rise rates in table defined only until 2.70 m

    for (int i_Layer = 0; i_Layer < vs_NumberOfLayers; i_Layer++) {
      // Define capillary water and available water

      vm_CapillaryWater[i_Layer] = vm_FieldCapacity[i_Layer] - vm_PermanentWiltingPoint[i_Layer];
      vm_AvailableWater[i_Layer] = vm_SoilMoisture[i_Layer] - vm_PermanentWiltingPoint[i_Layer];

      if (vm_AvailableWater[i_Layer] < 0.0)
        vm_AvailableWater[i_Layer] = 0.0;

      vm_CapillaryWater70[i_Layer] = 0.7 * vm_CapillaryWater[i_Layer];
    }

    double vm_CapillaryRiseRate = 0.01; //[m d-1]
    double pm_CapillaryRiseRate = 0.01; //[m d-1]
    // Find first layer above groundwater with 70% available water
    auto vm_StartLayer = min(vm_GroundwaterTable, (vs_NumberOfLayers - 1));
    for (int i = int(vm_StartLayer); i >= 0; i--) {
      std::string vs_SoilTexture = soilColumn[i].vs_SoilTexture();
      assert(!vs_SoilTexture.empty());
      pm_CapillaryRiseRate = smPs.getCapillaryRiseRate(vs_SoilTexture, vm_GroundwaterDistance);

      if (pm_CapillaryRiseRate < vm_CapillaryRiseRate)
        vm_CapillaryRiseRate = pm_CapillaryRiseRate;

      if (vm_AvailableWater[i] < vm_CapillaryWater70[i]) {
        auto vm_WaterAddedFromCapillaryRise = vm_CapillaryRiseRate; //[m3 m-2 d-1]
        vm_SoilMoisture[i] += vm_WaterAddedFromCapillaryRise;
        for (int j_Layer = int(vm_StartLayer); j_Layer >= i; j_Layer--)
          vm_WaterFlux[j_Layer] -= vm_WaterAddedFromCapillaryRise;
        break;
      }
    }
  }
}

/**
 * @brief Calculation of percolation with groundwater influence
  */
void SoilMoisture::fm_PercolationWithGroundwater(double vs_GroundwaterDepth) {
  vm_GroundwaterAdded = 0.0;

  for (size_t i = 0; i < vm_NumberOfLayers - 1; i++) {

    if (i < vm_GroundwaterTable - 1) {
      // well above groundwater table
      vm_SoilMoisture[i + 1] += vm_PercolationRate[i] / 1000.0 / vm_LayerThickness[i];
      vm_WaterFlux[i + 1] = vm_PercolationRate[i];

      if (vm_SoilMoisture[i + 1] > vm_FieldCapacity[i + 1]) {
        // Soil moisture exceeding field capacity
        vm_GravitationalWater[i + 1] = (vm_SoilMoisture[i + 1] - vm_FieldCapacity[i + 1]) * 1000.0 * vm_LayerThickness[i + 1];

        double vm_LambdaReduced = vm_Lambda[i + 1] * frostComponent.getLambdaRedux(i + 1);
        double vm_PercolationFactor = 1 + vm_LambdaReduced * vm_GravitationalWater[i + 1];
        vm_PercolationRate[i + 1] = ((vm_GravitationalWater[i + 1] * vm_GravitationalWater[i + 1]
          * vm_LambdaReduced) / vm_PercolationFactor);

        vm_GravitationalWater[i + 1] = vm_GravitationalWater[i + 1] - vm_PercolationRate[i + 1];

        if (vm_GravitationalWater[i + 1] < 0)
          vm_GravitationalWater[i + 1] = 0.0;

        vm_SoilMoisture[i + 1] =
          vm_FieldCapacity[i + 1] + (vm_GravitationalWater[i + 1] / 1000.0 / vm_LayerThickness[i + 1]);

        if (vm_SoilMoisture[i + 1] > vm_SoilPoreVolume[i + 1]) {

          // Soil moisture exceeding soil pore volume
          vm_GravitationalWater[i + 1] = (vm_SoilMoisture[i + 1] - vm_SoilPoreVolume[i + 1]) * 1000.0
            * vm_LayerThickness[i + 1];
          vm_SoilMoisture[i + 1] = vm_SoilPoreVolume[i + 1];
          vm_PercolationRate[i + 1] += vm_GravitationalWater[i + 1];
        }
      } else {
        // Soil moisture below field capacity
        vm_PercolationRate[i + 1] = 0.0;
        vm_GravitationalWater[i + 1] = 0.0;
      }
    } // if (i_Layer < vm_GroundwaterTable - 1) {

    // when the layer directly above ground water table is reached
    if (i == vm_GroundwaterTable - 1) {
      // groundwater table shall not undermatch the oscillating groundwater depth
      // which is generated within the outer framework
      if (vm_GroundwaterTable >= int(vs_GroundwaterDepth / vm_LayerThickness[i])) {
        vm_SoilMoisture[i + 1] += (vm_PercolationRate[i]) / 1000.0
          / vm_LayerThickness[i];
        vm_PercolationRate[i + 1] = vm_GroundwaterDischarge;
        vm_WaterFlux[i + 1] = vm_PercolationRate[i];
      } else {
        vm_SoilMoisture[i + 1] += (vm_PercolationRate[i] - vm_GroundwaterDischarge) / 1000.0
          / vm_LayerThickness[i];
        vm_PercolationRate[i + 1] = vm_GroundwaterDischarge;
        vm_WaterFlux[i + 1] = vm_GroundwaterDischarge;
      }

      if (vm_SoilMoisture[i + 1] >= vm_SoilPoreVolume[i + 1]) {

        //vm_GroundwaterTable--; // Rising groundwater table if vm_SoilMoisture > soil pore volume

        // vm_GroundwaterAdded is the volume of water added to the groundwater body.
        // It does not correspond to groundwater replenishment in the technical sense !!!!!
        vm_GroundwaterAdded = (vm_SoilMoisture[i + 1] - vm_SoilPoreVolume[i + 1]) * 1000.0
          * vm_LayerThickness[i + 1];

        vm_SoilMoisture[i + 1] = vm_SoilPoreVolume[i + 1];

        if (vm_GroundwaterAdded <= 0.0) {
          vm_GroundwaterAdded = 0.0;
        }
      }

    } // if (i_Layer == vm_GroundwaterTable - 1)

    // when the groundwater table is reached
    if (i > vm_GroundwaterTable - 1) {
      vm_SoilMoisture[i + 1] = vm_SoilPoreVolume[i + 1];

      if (vm_GroundwaterTable >= int(vs_GroundwaterDepth / vm_LayerThickness[i])) {
        vm_PercolationRate[i + 1] = vm_PercolationRate[i];
        vm_WaterFlux[i] = vm_PercolationRate[i + 1];
      } else {
        vm_PercolationRate[i + 1] = vm_GroundwaterDischarge;
        vm_WaterFlux[i] = vm_GroundwaterDischarge;
      }
    }

  } // for

  vm_FluxAtLowerBoundary = vm_WaterFlux[pm_LeachingDepthLayer];

}

/**
 * @brief Calculation of groundwater replenishment
 *
 */
void SoilMoisture::fm_GroundwaterReplenishment() {
  // Auffuellschleife von GW-Oberflaeche in Richtung Oberflaeche
  auto vm_StartLayer = vm_GroundwaterTable;

  if (vm_StartLayer > vm_NumberOfLayers - 2) {
    vm_StartLayer = vm_NumberOfLayers - 2;
  }

  for (int i_Layer = int(vm_StartLayer); i_Layer >= 0; i_Layer--) {
    vm_SoilMoisture[i_Layer] += vm_GroundwaterAdded
      / 1000.0
      / vm_LayerThickness[i_Layer + 1];

    if (i_Layer == vm_StartLayer) {
      vm_PercolationRate[i_Layer] = vm_GroundwaterDischarge;
    } else {
      vm_PercolationRate[i_Layer] -= vm_GroundwaterAdded; // Fluss_u durch Grundwasser
      vm_WaterFlux[i_Layer + 1] = vm_PercolationRate[i_Layer]; // Fluss_u durch Grundwasser
    }

    if (vm_SoilMoisture[i_Layer] > vm_SoilPoreVolume[i_Layer]) {
      vm_GroundwaterAdded = (vm_SoilMoisture[i_Layer] - vm_SoilPoreVolume[i_Layer])
        * 1000.0
        * vm_LayerThickness[i_Layer + 1];
      vm_SoilMoisture[i_Layer] = vm_SoilPoreVolume[i_Layer];
      vm_GroundwaterTable--; // Groundwater table rises

      if (i_Layer == 0 && vm_GroundwaterTable == 0) {
        // if groundwater reaches surface
        vm_SurfaceWaterStorage += vm_GroundwaterAdded;
        vm_GroundwaterAdded = 0.0;
      }
    } else {
      vm_GroundwaterAdded = 0.0;
    }

  } // for

  if (pm_LeachingDepthLayer > vm_GroundwaterTable - 1) {
    if (vm_GroundwaterTable - 1 < 0) {
      vm_FluxAtLowerBoundary = 0.0;
    } else {
      vm_FluxAtLowerBoundary = vm_WaterFlux[vm_GroundwaterTable - 1];
    }
  } else {
    vm_FluxAtLowerBoundary = vm_WaterFlux[pm_LeachingDepthLayer];
  }
  //cout << "GWN: " << vm_FluxAtLowerBoundary << endl;
}

/**
 * @brief Calculation of percolation without groundwater influence
 */
void SoilMoisture::fm_PercolationWithoutGroundwater() {
  double vm_PercolationFactor;
  double vm_LambdaReduced;

  for (int i_Layer = 0; i_Layer < vm_NumberOfLayers - 1; i_Layer++) {

    vm_SoilMoisture[i_Layer + 1] += vm_PercolationRate[i_Layer] / 1000.0 / vm_LayerThickness[i_Layer];

    if ((vm_SoilMoisture[i_Layer + 1] > vm_FieldCapacity[i_Layer + 1])) {

      // too much water for this layer so some water is released to layers below
      vm_GravitationalWater[i_Layer + 1] = (vm_SoilMoisture[i_Layer + 1] - vm_FieldCapacity[i_Layer + 1]) * 1000.0
        * vm_LayerThickness[0];
      vm_LambdaReduced = vm_Lambda[i_Layer + 1] * frostComponent.getLambdaRedux(i_Layer + 1);
      vm_PercolationFactor = 1.0 + (vm_LambdaReduced * vm_GravitationalWater[i_Layer + 1]);
      vm_PercolationRate[i_Layer + 1] = (vm_GravitationalWater[i_Layer + 1] * vm_GravitationalWater[i_Layer + 1]
        * vm_LambdaReduced) / vm_PercolationFactor;

      if (vm_PercolationRate[i_Layer + 1] > pm_MaxPercolationRate) {
        vm_PercolationRate[i_Layer + 1] = pm_MaxPercolationRate;
      }

      vm_GravitationalWater[i_Layer + 1] = vm_GravitationalWater[i_Layer + 1] - vm_PercolationRate[i_Layer + 1];

      if (vm_GravitationalWater[i_Layer + 1] < 0.0) {
        vm_GravitationalWater[i_Layer + 1] = 0.0;
      }

      vm_SoilMoisture[i_Layer + 1] = vm_FieldCapacity[i_Layer + 1] + (vm_GravitationalWater[i_Layer + 1]
        / 1000.0 / vm_LayerThickness[i_Layer + 1]);
    } else {

      // no water will be released in other layers
      vm_PercolationRate[i_Layer + 1] = 0.0;
      vm_GravitationalWater[i_Layer + 1] = 0.0;
    }

    vm_WaterFlux[i_Layer + 1] = vm_PercolationRate[i_Layer];
    vm_GroundwaterAdded = vm_PercolationRate[i_Layer + 1];

  } // for

  if ((pm_LeachingDepthLayer > 0) && (pm_LeachingDepthLayer < (vm_NumberOfLayers - 1))) {
    vm_FluxAtLowerBoundary = vm_WaterFlux[pm_LeachingDepthLayer];
  } else {
    vm_FluxAtLowerBoundary = vm_WaterFlux[vm_NumberOfLayers - 2];
  }
}

/**
 * @brief Calculation of backwater replenishment
 *
 */
void SoilMoisture::fm_BackwaterReplenishment() {
  auto vm_StartLayer = vm_NumberOfLayers - 1;
  auto vm_BackwaterTable = vm_NumberOfLayers - 1;
  double vm_BackwaterAdded = 0.0;

  // find first layer from top where the water content exceeds pore volume
  for (size_t i = 0; i < vm_NumberOfLayers - 1; i++) {
    if (vm_SoilMoisture[i] > vm_SoilPoreVolume[i]) {
      vm_StartLayer = i;
      vm_BackwaterTable = i;
    }
  }

  // if there is no such thing nothing will happen
  if (vm_BackwaterTable == 0)
    return;

  // Backwater replenishment upwards
  for (int i = int(vm_StartLayer); i >= 0; i--) {

    //!TODO check loop and whether it really should be i_Layer + 1 or the loop should start one layer higher ????!!!!
    vm_SoilMoisture[i] += vm_BackwaterAdded / 1000.0 / vm_LayerThickness[i];// + 1];
    if (i > 0) 
      vm_WaterFlux[i - 1] -= vm_BackwaterAdded;

    if (vm_SoilMoisture[i] > vm_SoilPoreVolume[i]) {

      //!TODO check also i_Layer + 1 here for same reason as above
      vm_BackwaterAdded = (vm_SoilMoisture[i] - vm_SoilPoreVolume[i]) * 1000.0 * vm_LayerThickness[i];// + 1];
      vm_SoilMoisture[i] = vm_SoilPoreVolume[i];
      vm_BackwaterTable--; // Backwater table rises

      if (i == 0 && vm_BackwaterTable == 0) {
        // if backwater reaches surface
        vm_SurfaceWaterStorage += vm_BackwaterAdded;
        vm_BackwaterAdded = 0.0;
      }
    } else {
      vm_BackwaterAdded = 0.0;
    }
  } // for
}

/**
 * @brief Calculation of Evapotranspiration
 * Calculation of transpiration and evaporation.
 *
 * @param vc_PercentageSoilCoverage
 * @param vc_KcFactor Needed for calculation of the Evapo-transpiration
 * @param vs_HeightNN
 * @param vw_MaxAirTemperature Maximal air temperature
 * @param vw_MinAirTemperature Minimal air temperature
 * @param vw_RelativeHumidity Relative Humidity
 * @param vw_MeanAirTemperature Mean air temperature
 * @param vw_WindSpeed Speed of wind
 * @param vw_WindSpeedHeight Height for the measurement of the wind speed
 * @param vw_GlobalRadiation Global radiaton
 * @param vc_DevelopmentalStage
 */
void SoilMoisture::fm_Evapotranspiration(double vc_PercentageSoilCoverage, double vc_KcFactor, double vs_HeightNN,
  double vw_MaxAirTemperature, double vw_MinAirTemperature, double vw_RelativeHumidity, double vw_MeanAirTemperature,
  double vw_WindSpeed, double vw_WindSpeedHeight, double vw_GlobalRadiation, int vc_DevelopmentalStage, int vs_JulianDay,
  double vs_Latitude, double vw_ReferenceEvapotranspiration) {
  double vm_EReducer_1 = 0.0;
  double vm_EReducer_2 = 0.0;
  double vm_EReducer_3 = 0.0;
  double pm_EvaporationZeta = 0.0;
  double pm_MaximumEvaporationImpactDepth = 0.0; // Das ist die Tiefe, bis zu der maximal die Evaporation vordringen kann
  double vm_EReducer = 0.0;
  double vm_PotentialEvapotranspiration = 0.0;
  double vc_EvaporatedFromIntercept = 0.0;
  double vm_EvaporatedFromSurface = 0.0;
  bool vm_EvaporationFromSurface = false;

  double vm_SnowDepth = snowComponent.getVm_SnowDepth();

  // Berechnung der Bodenevaporation bis max. 4dm Tiefe
  pm_EvaporationZeta = smPs.pm_EvaporationZeta; // Parameterdatei

  // Das sind die Steuerungsparameter für die Steigung der Entzugsfunktion
  vm_XSACriticalSoilMoisture = smPs.pm_XSACriticalSoilMoisture;

  /** @todo <b>Claas:</b> pm_MaximumEvaporationImpactDepth ist aber Abhängig von der Bodenart,
   * da muss was dran gemacht werden */
  pm_MaximumEvaporationImpactDepth = smPs.pm_MaximumEvaporationImpactDepth; // Parameterdatei


  // If a crop grows, ETp is taken from crop module
  if (vc_DevelopmentalStage > 0) {
    // Reference evapotranspiration is only grabbed here for consistent
    // output in monica.cpp
    if (vw_ReferenceEvapotranspiration < 0.0) {
      vm_ReferenceEvapotranspiration = monica.cropGrowth()->get_ReferenceEvapotranspiration();
    } else {
      vm_ReferenceEvapotranspiration = vw_ReferenceEvapotranspiration;
    }

    // Remaining ET from crop module already includes Kc factor and evaporation
    // from interception storage
    vm_PotentialEvapotranspiration = monica.cropGrowth()->get_RemainingEvapotranspiration();
    vc_EvaporatedFromIntercept = monica.cropGrowth()->get_EvaporatedFromIntercept();

  } else { // if no crop grows ETp is calculated from ET0 * kc

 // calculate reference evapotranspiration if not provided via climate files
    if (vw_ReferenceEvapotranspiration < 0.0) {
      vm_ReferenceEvapotranspiration = ReferenceEvapotranspiration(vs_HeightNN, vw_MaxAirTemperature,
        vw_MinAirTemperature, vw_RelativeHumidity, vw_MeanAirTemperature, vw_WindSpeed, vw_WindSpeedHeight,
        vw_GlobalRadiation, vs_JulianDay, vs_Latitude);
    } else {
      // use reference evapotranspiration from climate file		
      vm_ReferenceEvapotranspiration = vw_ReferenceEvapotranspiration;
    }

    vm_PotentialEvapotranspiration = vm_ReferenceEvapotranspiration * vc_KcFactor; // - vm_InterceptionReference;
  }

  vm_ActualEvaporation = 0.0;
  vm_ActualTranspiration = 0.0;

  // from HERMES:
  if (vm_PotentialEvapotranspiration > 6.5) vm_PotentialEvapotranspiration = 6.5;

  if (vm_PotentialEvapotranspiration > 0.0) {
    // If surface is water-logged, subsequent evaporation from surface water sources
    if (vm_SurfaceWaterStorage > 0.0) {
      vm_EvaporationFromSurface = true;
      // Water surface evaporates with Kc = 1.1.
      vm_PotentialEvapotranspiration = vm_PotentialEvapotranspiration * (1.1 / vc_KcFactor);

      // If a snow layer is present no water evaporates from surface water sources
      if (vm_SnowDepth > 0.0) {
        vm_EvaporatedFromSurface = 0.0;
      } else {
        if (vm_SurfaceWaterStorage < vm_PotentialEvapotranspiration) {
          vm_PotentialEvapotranspiration -= vm_SurfaceWaterStorage;
          vm_EvaporatedFromSurface = vm_SurfaceWaterStorage;
          vm_SurfaceWaterStorage = 0.0;
        } else {
          vm_SurfaceWaterStorage -= vm_PotentialEvapotranspiration;
          vm_EvaporatedFromSurface = vm_PotentialEvapotranspiration;
          vm_PotentialEvapotranspiration = 0.0;
        }
      }
      vm_PotentialEvapotranspiration = vm_PotentialEvapotranspiration * (vc_KcFactor / 1.1);
    }


    if (vm_PotentialEvapotranspiration > 0) { // Evaporation from soil

      for (int i_Layer = 0; i_Layer < vs_NumberOfLayers; i_Layer++) {

        vm_EReducer_1 = get_EReducer_1(i_Layer, vc_PercentageSoilCoverage,
          vm_PotentialEvapotranspiration);


        if (i_Layer >= pm_MaximumEvaporationImpactDepth) {
          // layer is too deep for evaporation
          vm_EReducer_2 = 0.0;
        } else {
          // 2nd factor to reduce actual evapotranspiration by
          // MaximumEvaporationImpactDepth and EvaporationZeta
          vm_EReducer_2 = get_DeprivationFactor(i_Layer + 1, pm_MaximumEvaporationImpactDepth,
            pm_EvaporationZeta, vm_LayerThickness[i_Layer]);
        }

        if (i_Layer > 0) {
          if (vm_SoilMoisture[i_Layer] < vm_SoilMoisture[i_Layer - 1]) {
            // 3rd factor to consider if above layer contains more water than
            // the adjacent layer below, evaporation will be significantly reduced
            vm_EReducer_3 = 0.1;
          } else {
            vm_EReducer_3 = 1.0;
          }
        } else {
          vm_EReducer_3 = 1.0;
        }
        // EReducer-> factor to reduce evaporation
        vm_EReducer = vm_EReducer_1 * vm_EReducer_2 * vm_EReducer_3;

        if (vc_DevelopmentalStage > 0) {
          // vegetation is present

          //Interpolation between [0,1]
          if (vc_PercentageSoilCoverage >= 0.0 && vc_PercentageSoilCoverage < 1.0) {
            vm_Evaporation[i_Layer] = ((1.0 - vc_PercentageSoilCoverage) * vm_EReducer)
              * vm_PotentialEvapotranspiration;
          } else {
            if (vc_PercentageSoilCoverage >= 1.0) {
              vm_Evaporation[i_Layer] = 0.0;
            }
          }

          if (vm_SnowDepth > 0.0)
            vm_Evaporation[i_Layer] = 0.0;

          // Transpiration is derived from ET0; Soil coverage and Kc factors
          // already considered in crop part!
          vm_Transpiration[i_Layer] = monica.cropGrowth()->get_Transpiration(i_Layer);

          //std::cout << setprecision(11) << "vm_Transpiration[i_Layer]: " << i_Layer << ", " << vm_Transpiration[i_Layer] << std::endl;

          // Transpiration is capped in case potential ET after surface
          // and interception evaporation has occurred on same day
          if (vm_EvaporationFromSurface) {
            vm_Transpiration[i_Layer] = vc_PercentageSoilCoverage * vm_EReducer * vm_PotentialEvapotranspiration;
          }

        } else {
          // no vegetation present
          if (vm_SnowDepth > 0.0) {
            vm_Evaporation[i_Layer] = 0.0;
          } else {
            vm_Evaporation[i_Layer] = vm_PotentialEvapotranspiration * vm_EReducer;
          }
          vm_Transpiration[i_Layer] = 0.0;

        } // if(vc_DevelopmentalStage > 0)

        vm_Evapotranspiration[i_Layer] = vm_Evaporation[i_Layer] + vm_Transpiration[i_Layer];
        vm_SoilMoisture[i_Layer] -= (vm_Evapotranspiration[i_Layer] / 1000.0 / vm_LayerThickness[i_Layer]);

        //  Generelle Begrenzung des Evaporationsentzuges
        if (vm_SoilMoisture[i_Layer] < 0.01) {
          vm_SoilMoisture[i_Layer] = 0.01;
        }

        vm_ActualTranspiration += vm_Transpiration[i_Layer];
        vm_ActualEvaporation += vm_Evaporation[i_Layer];
      } // for
    } // vm_PotentialEvapotranspiration > 0
  } // vm_PotentialEvapotranspiration > 0.0
  vm_ActualEvapotranspiration = vm_ActualTranspiration + vm_ActualEvaporation + vc_EvaporatedFromIntercept
    + vm_EvaporatedFromSurface;


  //std::cout << setprecision(11) << "vm_ActualTranspiration: " << vm_ActualTranspiration << std::endl;
  //std::cout << setprecision(11) << "vm_ActualEvaporation: " << vm_ActualEvaporation << std::endl;
  //std::cout << setprecision(11) << "vc_EvaporatedFromIntercept: " << vc_EvaporatedFromIntercept << std::endl;
  //std::cout << setprecision(11) << "vm_EvaporatedFromSurface: " << vm_EvaporatedFromSurface << std::endl;

  if (crop) {
    crop->accumulateEvapotranspiration(vm_ActualEvapotranspiration);
    crop->accumulateTranspiration(vm_ActualTranspiration);
  }
}



/**
 * @brief Reference evapotranspiration
 *
 * A method following Penman-Monteith as described by the FAO in Allen
 * RG, Pereira LS, Raes D, Smith M. (1998) Crop evapotranspiration.
 * Guidelines for computing crop water requirements. FAO Irrigation and
 * Drainage Paper 56, FAO, Roma
 *
 * @param vs_HeightNN
 * @param vw_MaxAirTemperature
 * @param vw_MinAirTemperature
 * @param vw_RelativeHumidity
 * @param vw_MeanAirTemperature
 * @param vw_WindSpeed
 * @param vw_WindSpeedHeight
 * @param vw_GlobalRadiation
 * @return
 */
double SoilMoisture::ReferenceEvapotranspiration(double vs_HeightNN, double vw_MaxAirTemperature,
  double vw_MinAirTemperature, double vw_RelativeHumidity, double vw_MeanAirTemperature, double vw_WindSpeed,
  double vw_WindSpeedHeight, double vw_GlobalRadiation, int vs_JulianDay, double vs_Latitude) {

  double vc_Declination;
  double vc_DeclinationSinus; // old SINLD
  double vc_DeclinationCosinus; // old COSLD
  double vc_AstronomicDayLenght;
  double vc_EffectiveDayLenght;
  double vc_PhotoperiodicDaylength;
  double vc_PhotActRadiationMean;
  double vc_ClearDayRadiation;
  double vc_OvercastDayRadiation;

  double vm_AtmosphericPressure; //[kPA]
  double vm_PsycrometerConstant; //[kPA °C-1]
  double vm_SaturatedVapourPressureMax; //[kPA]
  double vm_SaturatedVapourPressureMin; //[kPA]
  double vm_SaturatedVapourPressure; //[kPA]
  double vm_VapourPressure; //[kPA]
  double vm_SaturationDeficit; //[kPA]
  double vm_SaturatedVapourPressureSlope; //[kPA °C-1]
  double vm_WindSpeed_2m; //[m s-1]
  double vm_AerodynamicResistance; //[s m-1]
  double vm_SurfaceResistance; //[s m-1]
  double vc_ExtraterrestrialRadiation;
  double vm_ReferenceEvapotranspiration; //[mm]
  double pc_ReferenceAlbedo = cropPs.pc_ReferenceAlbedo; // FAO Green gras reference albedo from Allen et al. (1998)
  double PI = 3.14159265358979323;

  vc_Declination = -23.4 * cos(2.0 * PI * ((vs_JulianDay + 10.0) / 365.0));
  vc_DeclinationSinus = sin(vc_Declination * PI / 180.0) * sin(vs_Latitude * PI / 180.0);
  vc_DeclinationCosinus = cos(vc_Declination * PI / 180.0) * cos(vs_Latitude * PI / 180.0);

  double arg_AstroDayLength = vc_DeclinationSinus / vc_DeclinationCosinus;
  arg_AstroDayLength = bound(-1.0, arg_AstroDayLength, 1.0); //The argument of asin must be in the range of -1 to 1  
  vc_AstronomicDayLenght = 12.0 * (PI + 2.0 * asin(arg_AstroDayLength)) / PI;

  double arg_EffectiveDayLength = (-sin(8.0 * PI / 180.0) + vc_DeclinationSinus) / vc_DeclinationCosinus;
  arg_EffectiveDayLength = bound(-1.0, arg_EffectiveDayLength, 1.0); //The argument of asin must be in the range of -1 to 1
  vc_EffectiveDayLenght = 12.0 * (PI + 2.0 * asin(arg_EffectiveDayLength)) / PI;

  double arg_PhotoDayLength = (-sin(-6.0 * PI / 180.0) + vc_DeclinationSinus) / vc_DeclinationCosinus;
  arg_PhotoDayLength = bound(-1.0, arg_PhotoDayLength, 1.0); //The argument of asin must be in the range of -1 to 1
  vc_PhotoperiodicDaylength = 12.0 * (PI + 2.0 * asin(arg_PhotoDayLength)) / PI;

  double arg_PhotAct = min(1.0, ((vc_DeclinationSinus / vc_DeclinationCosinus) * (vc_DeclinationSinus / vc_DeclinationCosinus))); //The argument of sqrt must be >= 0
  vc_PhotActRadiationMean = 3600.0 * (vc_DeclinationSinus * vc_AstronomicDayLenght + 24.0 / PI * vc_DeclinationCosinus
    * sqrt(1.0 - arg_PhotAct));


  vc_ClearDayRadiation = 0;
  if (vc_PhotActRadiationMean > 0 && vc_AstronomicDayLenght > 0) {
    vc_ClearDayRadiation = 0.5 * 1300.0 * vc_PhotActRadiationMean * exp(-0.14 / (vc_PhotActRadiationMean
      / (vc_AstronomicDayLenght * 3600.0)));
  }

  vc_OvercastDayRadiation = 0.2 * vc_ClearDayRadiation;
  double SC = 24.0 * 60.0 / PI * 8.20 * (1.0 + 0.033 * cos(2.0 * PI * vs_JulianDay / 365.0));
  double arg_SHA = bound(-1.0, -tan(vs_Latitude * PI / 180.0) * tan(vc_Declination * PI / 180.0), 1.0); //The argument of acos must be in the range of -1 to 1
  double SHA = acos(arg_SHA);

  vc_ExtraterrestrialRadiation = SC * (SHA * vc_DeclinationSinus + vc_DeclinationCosinus * sin(SHA)) / 100.0; // [J cm-2] --> [MJ m-2]

  // Calculation of atmospheric pressure
  vm_AtmosphericPressure = 101.3 * pow(((293.0 - (0.0065 * vs_HeightNN)) / 293.0), 5.26);

  // Calculation of psychrometer constant - Luchtfeuchtigkeit
  vm_PsycrometerConstant = 0.000665 * vm_AtmosphericPressure;

  // Calc. of saturated water vapour pressure at daily max temperature
  vm_SaturatedVapourPressureMax = 0.6108 * exp((17.27 * vw_MaxAirTemperature) / (237.3 + vw_MaxAirTemperature));

  // Calc. of saturated water vapour pressure at daily min temperature
  vm_SaturatedVapourPressureMin = 0.6108 * exp((17.27 * vw_MinAirTemperature) / (237.3 + vw_MinAirTemperature));

  // Calculation of the saturated water vapour pressure
  vm_SaturatedVapourPressure = (vm_SaturatedVapourPressureMax + vm_SaturatedVapourPressureMin) / 2.0;

  // Calculation of the water vapour pressure
  if (vw_RelativeHumidity <= 0.0) {
    // Assuming Tdew = Tmin as suggested in FAO56 Allen et al. 1998
    vm_VapourPressure = vm_SaturatedVapourPressureMin;
  } else {
    vm_VapourPressure = vw_RelativeHumidity * vm_SaturatedVapourPressure;
  }

  // Calculation of the air saturation deficit
  vm_SaturationDeficit = vm_SaturatedVapourPressure - vm_VapourPressure;

  // Slope of saturation water vapour pressure-to-temperature relation
  vm_SaturatedVapourPressureSlope = (4098.0 * (0.6108 * exp((17.27 * vw_MeanAirTemperature) / (vw_MeanAirTemperature
    + 237.3)))) / ((vw_MeanAirTemperature + 237.3) * (vw_MeanAirTemperature + 237.3));

  // Calculation of wind speed in 2m height
  vm_WindSpeed_2m = max(0.5, vw_WindSpeed * (4.87 / (log(67.8 * vw_WindSpeedHeight - 5.42))));
  // 0.5 minimum allowed windspeed for Penman-Monteith-Method FAO

  // Calculation of the aerodynamic resistance
  vm_AerodynamicResistance = 208.0 / vm_WindSpeed_2m;

  vc_StomataResistance = 100; // FAO default value [s m-1]

  vm_SurfaceResistance = vc_StomataResistance / 1.44;

  double vc_ClearSkySolarRadiation = (0.75 + 0.00002 * vs_HeightNN) * vc_ExtraterrestrialRadiation;
  double vc_RelativeShortwaveRadiation = vc_ClearSkySolarRadiation > 0 ? min(vw_GlobalRadiation / vc_ClearSkySolarRadiation, 1.0) : 1.0;

  double pc_BolzmannConstant = 0.0000000049;
  double vc_ShortwaveRadiation = (1.0 - pc_ReferenceAlbedo) * vw_GlobalRadiation;
  double vc_LongwaveRadiation = pc_BolzmannConstant
    * ((pow((vw_MinAirTemperature + 273.16), 4.0)
      + pow((vw_MaxAirTemperature + 273.16), 4.0)) / 2.0)
    * (1.35 * vc_RelativeShortwaveRadiation - 0.35)
    * (0.34 - 0.14 * sqrt(vm_VapourPressure));
  vw_NetRadiation = vc_ShortwaveRadiation - vc_LongwaveRadiation;

  // Calculation of the reference evapotranspiration
  // Penman-Monteith-Methode FAO
  vm_ReferenceEvapotranspiration = ((0.408 * vm_SaturatedVapourPressureSlope * vw_NetRadiation)
    + (vm_PsycrometerConstant * (900.0 / (vw_MeanAirTemperature + 273.0))
      * vm_WindSpeed_2m * vm_SaturationDeficit))
    / (vm_SaturatedVapourPressureSlope + vm_PsycrometerConstant
      * (1.0 + (vm_SurfaceResistance / 208.0) * vm_WindSpeed_2m));

  if (vm_ReferenceEvapotranspiration < 0.0) {
    vm_ReferenceEvapotranspiration = 0.0;
  }

  return vm_ReferenceEvapotranspiration;
}

/*!
 * Get capillary rise from KA4
 *
 * In german: berechnet kapilaren Aufstieg ueber Bodenart.
 *
 * @todo Implementierung des kapilaren Aufstiegs fehlt.
 */
double SoilMoisture::get_CapillaryRise() {
  return vm_CapillaryRise; //get_par(vm_SoilTextureClass,vm_GroundwaterDistance);
}

/*!
 * Calculation of evaporation reduction by soil moisture content
 *
 * @param i_Layer
 * @param vm_PercentageSoilCoverage
 * @param vm_ReferenceEvapotranspiration
 *
 * @return Value for evaporation reduction by soil moisture content
 */
double SoilMoisture::get_EReducer_1(int i_Layer,
  double vm_PercentageSoilCoverage,
  double vm_ReferenceEvapotranspiration) {
  double vm_EReductionFactor;
  int vm_EvaporationReductionMethod = 1;
  double vm_SoilMoisture_m3 = soilColumn[i_Layer].get_Vs_SoilMoisture_m3();
  double vm_PWP = soilColumn[i_Layer].vs_PermanentWiltingPoint();
  double vm_FK = soilColumn[i_Layer].vs_FieldCapacity();
  double vm_RelativeEvaporableWater;
  double vm_CriticalSoilMoisture;
  double vm_XSA;
  double vm_Reducer;

  if (vm_SoilMoisture_m3 < (0.33 * vm_PWP)) vm_SoilMoisture_m3 = 0.33 * vm_PWP;

  vm_RelativeEvaporableWater = (vm_SoilMoisture_m3 - (0.33 * vm_PWP))
    / (vm_FK - (0.33 * vm_PWP));

  if (vm_RelativeEvaporableWater > 1.0) vm_RelativeEvaporableWater = 1.0;

  if (vm_EvaporationReductionMethod == 0) {
    // THESEUS
    vm_CriticalSoilMoisture = 0.65 * vm_FK;
    if (vm_PercentageSoilCoverage > 0) {
      if (vm_ReferenceEvapotranspiration > 2.5) {
        vm_XSA = (0.65 * vm_FK - vm_PWP) * (vm_FK - vm_PWP);
        vm_Reducer = vm_XSA + (((1 - vm_XSA) / 17.5)
          * (vm_ReferenceEvapotranspiration - 2.5));
      } else {
        vm_Reducer = vm_XSACriticalSoilMoisture / 2.5 * vm_ReferenceEvapotranspiration;
      }
      vm_CriticalSoilMoisture = soilColumn[i_Layer].vs_FieldCapacity() * vm_Reducer;
    }

    // Calculation of an evaporation-reducing factor in relation to soil water content
    if (vm_SoilMoisture_m3 > vm_CriticalSoilMoisture) {
      // Moisture is higher than critical value so there is a
      // normal evaporation and nothing must be reduced
      vm_EReductionFactor = 1.0;

    } else {
      // critical value is reached, actual evaporation is below potential

      if (vm_SoilMoisture_m3 > (0.33 * vm_PWP)) {
        // moisture is higher than 30% of permanent wilting point
        vm_EReductionFactor = vm_RelativeEvaporableWater;
      } else {
        // if moisture is below 30% of wilting point nothing can be evaporated
        vm_EReductionFactor = 0.0;
      }
    }

  } else if (vm_EvaporationReductionMethod == 1) {
    // HERMES
    vm_EReductionFactor = 0.0;
    if (vm_RelativeEvaporableWater > 0.33) {
      vm_EReductionFactor = 1.0 - (0.1 * (1.0 - vm_RelativeEvaporableWater) / (1.0 - 0.33));
    } else if (vm_RelativeEvaporableWater > 0.22) {
      vm_EReductionFactor = 0.9 - (0.625 * (0.33 - vm_RelativeEvaporableWater) / (0.33 - 0.22));
    } else if (vm_RelativeEvaporableWater > 0.2) {
      vm_EReductionFactor = 0.275 - (0.225 * (0.22 - vm_RelativeEvaporableWater) / (0.22 - 0.2));
    } else {
      vm_EReductionFactor = 0.05 - (0.05 * (0.2 - vm_RelativeEvaporableWater) / 0.2);
    } // end if
  }
  return vm_EReductionFactor;
}

/*!
 * @brief Calculation of deprivation factor
 * @return deprivationFactor
 *
 * PET deprivation distribution (factor as function of depth).
 * The PET is spread over the deprivation depth. This function computes
 * the factor/weight for a given layer/depth[dm] (layerNo).
 *
 * @param layerNo [1..NLAYER]
 * @param deprivationDepth [dm] maximum deprivation depth
 * @param zeta [0..40] shape factor
 * @param vs_LayerThickness
 */
double SoilMoisture::get_DeprivationFactor(int layerNo, double deprivationDepth, double zeta, double vs_LayerThickness) {
  // factor (f(depth)) to distribute the PET along the soil profil/rooting zone

  double deprivationFactor;

  // factor to introduce layer thickness in this algorithm,
  // to allow layer thickness scaling (Claas Nendel)
  double layerThicknessFactor = deprivationDepth / (vs_LayerThickness * 10.0);

  if ((fabs(zeta)) < 0.0003) {

    deprivationFactor = (2.0 / layerThicknessFactor) - (1.0 / (layerThicknessFactor * layerThicknessFactor)) * (2
      * layerNo - 1);
    return deprivationFactor;

  } else {

    double c2 = 0.0;
    double c3 = 0.0;
    c2 = log((layerThicknessFactor + zeta * layerNo) / (layerThicknessFactor + zeta * (layerNo - 1)));
    c3 = zeta / (layerThicknessFactor * (zeta + 1.0));
    deprivationFactor = (c2 - c3) / (log(zeta + 1.0) - zeta / (zeta + 1.0));
    return deprivationFactor;
  }
}

/*!
 * @brief Calculates mean of water content until the given depth.
 * @param depth_m depth
 *
 * Accumulates moisture values of soil layers until the given depth is reached.
 * The mean moisture value is returned.
 */
double SoilMoisture::meanWaterContent(double depth_m) const {
  double lsum = 0.0, sum = 0.0;
  int count = 0;

  for (int i = 0; i < vs_NumberOfLayers; i++) {
    count++;
    double smm3 = soilColumn[i].get_Vs_SoilMoisture_m3();
    double fc = soilColumn[i].vs_FieldCapacity();
    double pwp = soilColumn[i].vs_PermanentWiltingPoint();
    sum += smm3 / (fc - pwp); //[%nFK]
    lsum += soilColumn[i].vs_LayerThickness;
    if (lsum >= depth_m)
      break;
  }

  return sum / double(count);
}


double SoilMoisture::meanWaterContent(int layer, int number_of_layers) const {
  double sum = 0.0;
  int count = 0;

  if (layer + number_of_layers > vs_NumberOfLayers) {
    return -1;
  }

  for (int i = layer; i < layer + number_of_layers; i++) {
    count++;
    double smm3 = soilColumn[i].get_Vs_SoilMoisture_m3();
    double fc = soilColumn[i].vs_FieldCapacity();
    double pwp = soilColumn[i].vs_PermanentWiltingPoint();
    sum += smm3 / (fc - pwp); //[%nFK]
  }

  return sum / double(count);
}


/**
 * @brief Returns Kc factor
 * @return Kc factor
 */
double SoilMoisture::get_KcFactor() const {
  return vc_KcFactor;
}

/**
 * @brief Returns drought stress factor []
 * @return drought stress factor
 */
double SoilMoisture::get_TranspirationDeficit() const {
  return vm_TranspirationDeficit;
}

/**
 * @brief Returns snow depth [mm]
 * @return Value for snow depth
 */
double SoilMoisture::get_SnowDepth() const {
  return snowComponent.getVm_SnowDepth();
}

double SoilMoisture::getMaxSnowDepth() const {
  return snowComponent.getMaxSnowDepth();
}

double SoilMoisture::getAccumulatedSnowDepth() const {
  return snowComponent.getAccumulatedSnowDepth();
}

double SoilMoisture::getAccumulatedFrostDepth() const {
  return frostComponent.getAccumulatedFrostDepth();
}


/**
* @brief Returns snow depth [mm]
* @return Value for snow depth
*/
double SoilMoisture::getTemperatureUnderSnow() const {
  return frostComponent.getTemperatureUnderSnow();
}

void SoilMoisture::put_Crop(CropGrowth* c) {
  crop = c;
}

void SoilMoisture::remove_Crop() {
  crop = NULL;
}

