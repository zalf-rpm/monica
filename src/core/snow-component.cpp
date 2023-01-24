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
#include "snow-component.h"

#include <algorithm> //for min, max
#include <iostream>
#include <cmath>

#include "soilcolumn.h"
#include "tools/debug.h"
#include "tools/algorithms.h"
#include "monica-parameters.h"

using namespace std;
using namespace monica;

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

void SnowComponent::deserialize(mas::schema::model::monica::SnowModuleState::Reader reader) {
  vm_SnowDensity = reader.getSnowDensity();
  vm_SnowDepth = reader.getSnowDepth();
  vm_FrozenWaterInSnow = reader.getFrozenWaterInSnow();
  vm_LiquidWaterInSnow = reader.getLiquidWaterInSnow();
  vm_WaterToInfiltrate = reader.getWaterToInfiltrate();
  vm_maxSnowDepth = reader.getMaxSnowDepth();
  vm_AccumulatedSnowDepth = reader.getAccumulatedSnowDepth();
  vm_SnowmeltTemperature = reader.getSnowmeltTemperature();
  vm_SnowAccumulationThresholdTemperature = reader.getSnowAccumulationThresholdTemperature();
  vm_TemperatureLimitForLiquidWater = reader.getTemperatureLimitForLiquidWater();
  vm_CorrectionRain = reader.getCorrectionRain();
  vm_CorrectionSnow = reader.getCorrectionSnow();
  vm_RefreezeTemperature = reader.getRefreezeTemperature();
  vm_RefreezeP1 = reader.getRefreezeP1();
  vm_RefreezeP2 = reader.getRefreezeP2();
  vm_NewSnowDensityMin = reader.getNewSnowDensityMin();
  vm_SnowMaxAdditionalDensity = reader.getSnowMaxAdditionalDensity();
  vm_SnowPacking = reader.getSnowPacking();
  vm_SnowRetentionCapacityMin = reader.getSnowRetentionCapacityMin();
  vm_SnowRetentionCapacityMax = reader.getSnowRetentionCapacityMax();
}

void SnowComponent::serialize(mas::schema::model::monica::SnowModuleState::Builder builder) const {
  builder.setSnowDensity(vm_SnowDensity);
  builder.setSnowDepth(vm_SnowDepth);
  builder.setFrozenWaterInSnow(vm_FrozenWaterInSnow);
  builder.setLiquidWaterInSnow(vm_LiquidWaterInSnow);
  builder.setWaterToInfiltrate(vm_WaterToInfiltrate);
  builder.setMaxSnowDepth(vm_maxSnowDepth);
  builder.setAccumulatedSnowDepth(vm_AccumulatedSnowDepth);
  builder.setSnowmeltTemperature(vm_SnowmeltTemperature);
  builder.setSnowAccumulationThresholdTemperature(vm_SnowAccumulationThresholdTemperature);
  builder.setTemperatureLimitForLiquidWater(vm_TemperatureLimitForLiquidWater);
  builder.setCorrectionRain(vm_CorrectionRain);
  builder.setCorrectionSnow(vm_CorrectionSnow);
  builder.setRefreezeTemperature(vm_RefreezeTemperature);
  builder.setRefreezeP1(vm_RefreezeP1);
  builder.setRefreezeP2(vm_RefreezeP2);
  builder.setNewSnowDensityMin(vm_NewSnowDensityMin);
  builder.setSnowMaxAdditionalDensity(vm_SnowMaxAdditionalDensity);
  builder.setSnowPacking(vm_SnowPacking);
  builder.setSnowRetentionCapacityMin(vm_SnowRetentionCapacityMin);
  builder.setSnowRetentionCapacityMax(vm_SnowRetentionCapacityMax);
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
