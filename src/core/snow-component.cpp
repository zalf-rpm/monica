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

#include "soilcolumn_simple.h"
#include "tools/debug.h"
#include "tools/algorithms.h"
#include "monica-parameters.h"

using namespace std;
using namespace monica;

void monica::snowComponentInitialize(SnowComponent* sc, SoilColumn* soilColumn, const SoilMoistureModuleParameters& smps) {
  sc->soilColumn = soilColumn;
  sc->vm_SnowDensity = 0.0;
  sc->vm_SnowDepth = 0.0;
  sc->vm_FrozenWaterInSnow = 0.0;
  sc->vm_LiquidWaterInSnow = 0.0;
  sc->vm_WaterToInfiltrate = 0.0;
  sc->vm_maxSnowDepth = 0.0;
  sc->vm_AccumulatedSnowDepth = 0.0;
  sc->vm_SnowmeltTemperature = smps.pm_SnowMeltTemperature;
  sc->vm_SnowAccumulationThresholdTemperature = smps.pm_SnowAccumulationTresholdTemperature;
  sc->vm_TemperatureLimitForLiquidWater = smps.pm_TemperatureLimitForLiquidWater;
  sc->vm_CorrectionRain = smps.pm_CorrectionRain;
  sc->vm_CorrectionSnow = smps.pm_CorrectionSnow;
  sc->vm_RefreezeTemperature = smps.pm_RefreezeTemperature;
  sc->vm_RefreezeP1 = smps.pm_RefreezeParameter1;
  sc->vm_RefreezeP2 = smps.pm_RefreezeParameter2;
  sc->vm_NewSnowDensityMin = smps.pm_NewSnowDensityMin;
  sc->vm_SnowMaxAdditionalDensity = smps.pm_SnowMaxAdditionalDensity;
  sc->vm_SnowPacking = smps.pm_SnowPacking;
  sc->vm_SnowRetentionCapacityMin = smps.pm_SnowRetentionCapacityMin;
  sc->vm_SnowRetentionCapacityMax = smps.pm_SnowRetentionCapacityMax;
}

void monica::snowComponentDeserialize(SnowComponent* sc, mas::schema::model::monica::SnowModuleState::Reader reader) {
  sc->vm_SnowDensity = reader.getSnowDensity();
  sc->vm_SnowDepth = reader.getSnowDepth();
  sc->vm_FrozenWaterInSnow = reader.getFrozenWaterInSnow();
  sc->vm_LiquidWaterInSnow = reader.getLiquidWaterInSnow();
  sc->vm_WaterToInfiltrate = reader.getWaterToInfiltrate();
  sc->vm_maxSnowDepth = reader.getMaxSnowDepth();
  sc->vm_AccumulatedSnowDepth = reader.getAccumulatedSnowDepth();
  sc->vm_SnowmeltTemperature = reader.getSnowmeltTemperature();
  sc->vm_SnowAccumulationThresholdTemperature = reader.getSnowAccumulationThresholdTemperature();
  sc->vm_TemperatureLimitForLiquidWater = reader.getTemperatureLimitForLiquidWater();
  sc->vm_CorrectionRain = reader.getCorrectionRain();
  sc->vm_CorrectionSnow = reader.getCorrectionSnow();
  sc->vm_RefreezeTemperature = reader.getRefreezeTemperature();
  sc->vm_RefreezeP1 = reader.getRefreezeP1();
  sc->vm_RefreezeP2 = reader.getRefreezeP2();
  sc->vm_NewSnowDensityMin = reader.getNewSnowDensityMin();
  sc->vm_SnowMaxAdditionalDensity = reader.getSnowMaxAdditionalDensity();
  sc->vm_SnowPacking = reader.getSnowPacking();
  sc->vm_SnowRetentionCapacityMin = reader.getSnowRetentionCapacityMin();
  sc->vm_SnowRetentionCapacityMax = reader.getSnowRetentionCapacityMax();
}

void monica::snowComponentSerialize(const SnowComponent* sc, mas::schema::model::monica::SnowModuleState::Builder builder) {
  builder.setSnowDensity(sc->vm_SnowDensity);
  builder.setSnowDepth(sc->vm_SnowDepth);
  builder.setFrozenWaterInSnow(sc->vm_FrozenWaterInSnow);
  builder.setLiquidWaterInSnow(sc->vm_LiquidWaterInSnow);
  builder.setWaterToInfiltrate(sc->vm_WaterToInfiltrate);
  builder.setMaxSnowDepth(sc->vm_maxSnowDepth);
  builder.setAccumulatedSnowDepth(sc->vm_AccumulatedSnowDepth);
  builder.setSnowmeltTemperature(sc->vm_SnowmeltTemperature);
  builder.setSnowAccumulationThresholdTemperature(sc->vm_SnowAccumulationThresholdTemperature);
  builder.setTemperatureLimitForLiquidWater(sc->vm_TemperatureLimitForLiquidWater);
  builder.setCorrectionRain(sc->vm_CorrectionRain);
  builder.setCorrectionSnow(sc->vm_CorrectionSnow);
  builder.setRefreezeTemperature(sc->vm_RefreezeTemperature);
  builder.setRefreezeP1(sc->vm_RefreezeP1);
  builder.setRefreezeP2(sc->vm_RefreezeP2);
  builder.setNewSnowDensityMin(sc->vm_NewSnowDensityMin);
  builder.setSnowMaxAdditionalDensity(sc->vm_SnowMaxAdditionalDensity);
  builder.setSnowPacking(sc->vm_SnowPacking);
  builder.setSnowRetentionCapacityMin(sc->vm_SnowRetentionCapacityMin);
  builder.setSnowRetentionCapacityMax(sc->vm_SnowRetentionCapacityMax);
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
void monica::snowComponentCalcSnowLayer(SnowComponent* sc, double mean_air_temperature, double net_precipitation) {
  // Calcs netto precipitation
  double net_precipitation_snow = 0.0;
  double net_precipitation_water = 0.0;
  net_precipitation = snowComponentCalcNetPrecipitation(sc,
                                                        mean_air_temperature,
                                                        net_precipitation,
                                                        net_precipitation_water,
                                                        net_precipitation_snow);

  // Calculate snowmelt
  double vm_Snowmelt = snowComponentCalcSnowMelt(sc, mean_air_temperature);

  // Calculate refreeze in snow
  double vm_Refreeze = snowComponentCalcRefreeze(sc, mean_air_temperature);

  // Calculate density of newly fallen snow
  double vm_NewSnowDensity = snowComponentCalcNewSnowDensity(sc, mean_air_temperature, net_precipitation_snow);

  // Calculate average density of whole snowpack
  sc->vm_SnowDensity = snowComponentCalcAverageSnowDensity(sc, net_precipitation_snow, vm_NewSnowDensity);


  // Calculate amounts of water in frozen snow and liquid form
  sc->vm_FrozenWaterInSnow = sc->vm_FrozenWaterInSnow + net_precipitation_snow - vm_Snowmelt + vm_Refreeze;
  sc->vm_LiquidWaterInSnow = sc->vm_LiquidWaterInSnow + net_precipitation_water + vm_Snowmelt - vm_Refreeze;
  double vm_SnowWaterEquivalent = sc->vm_FrozenWaterInSnow + sc->vm_LiquidWaterInSnow; // snow water equivalent [mm]

  // Calculate snow's capacity to retain liquid
  double vm_LiquidWaterRetainedInSnow =
    snowComponentCalcLiquidWaterRetainedInSnow(sc, sc->vm_FrozenWaterInSnow, vm_SnowWaterEquivalent);

  // Calculate water release from snow
  double vm_SnowLayerWaterRelease = 0.0;
  if (vm_Refreeze > 0.0) {
    vm_SnowLayerWaterRelease = 0.0;
  } else if (sc->vm_LiquidWaterInSnow <= vm_LiquidWaterRetainedInSnow) {
    vm_SnowLayerWaterRelease = 0;
  } else {
    vm_SnowLayerWaterRelease = sc->vm_LiquidWaterInSnow - vm_LiquidWaterRetainedInSnow;
    sc->vm_LiquidWaterInSnow -= vm_SnowLayerWaterRelease;
    vm_SnowWaterEquivalent = sc->vm_FrozenWaterInSnow + sc->vm_LiquidWaterInSnow;
  }

  // Calculate snow depth from snow water equivalent
  snowComponentCalcSnowDepth(sc, vm_SnowWaterEquivalent);

  // Calculate potential infiltration to soil
  sc->vm_WaterToInfiltrate =
    snowComponentCalcPotentialInfiltration(sc, net_precipitation, vm_SnowLayerWaterRelease, sc->vm_SnowDepth);
}

/**
 *
 * @param vw_MeanAirTemperature
 * @return
 */
double monica::snowComponentCalcSnowMelt(const SnowComponent* sc, double vw_MeanAirTemperature) {
  double vm_MeltingFactor = 1.4 * (sc->vm_SnowDensity / 0.1);
  double vm_Snowmelt = 0.0;

  if (vm_MeltingFactor > 4.7) {
    vm_MeltingFactor = 4.7;
  }

  if (sc->vm_FrozenWaterInSnow <= 0.0) {
    vm_Snowmelt = 0.0;
  } else if (vw_MeanAirTemperature < sc->vm_SnowmeltTemperature) {
    vm_Snowmelt = 0.0;
  } else {
    vm_Snowmelt = vm_MeltingFactor * (vw_MeanAirTemperature - sc->vm_SnowmeltTemperature);
    if (vm_Snowmelt > sc->vm_FrozenWaterInSnow) {
      vm_Snowmelt = sc->vm_FrozenWaterInSnow;
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
double monica::snowComponentCalcNetPrecipitation(
  const SnowComponent* sc,
  double mean_air_temperature,
  double net_precipitation,
  double& net_precipitation_water, // return values
  double& net_precipitation_snow)  // return values
{
  double liquid_water_precipitation = 0.0;

  // Calculate forms and proportions of precipitation
  if (mean_air_temperature >= sc->vm_SnowAccumulationThresholdTemperature) {
    liquid_water_precipitation = 1.0;
  } else if (mean_air_temperature <= sc->vm_TemperatureLimitForLiquidWater) {
    liquid_water_precipitation = 0.0;
  } else {
    liquid_water_precipitation = (mean_air_temperature - sc->vm_TemperatureLimitForLiquidWater)
      / (sc->vm_SnowAccumulationThresholdTemperature - sc->vm_TemperatureLimitForLiquidWater);
  }

  net_precipitation_water = liquid_water_precipitation * sc->vm_CorrectionRain * net_precipitation;
  net_precipitation_snow = (1.0 - liquid_water_precipitation) * sc->vm_CorrectionSnow * net_precipitation;

  // Total net precipitation corrected for snow
  net_precipitation = net_precipitation_snow + net_precipitation_water;

  return net_precipitation;
}

/**
 *
 * @param vw_MeanAirTemperature
 * @return
 */
double monica::snowComponentCalcRefreeze(const SnowComponent* sc, double mean_air_temperature) {
  double refreeze = 0.0;
  double refreeze_helper = 0.0;

  // no refreeze if it's too warm
  if (mean_air_temperature > 0) {
    refreeze_helper = 0;
  } else {
    refreeze_helper = mean_air_temperature;
  }

  if (refreeze_helper < sc->vm_RefreezeTemperature) {
    if (sc->vm_LiquidWaterInSnow > 0.0) {
      refreeze = sc->vm_RefreezeP1 * pow((sc->vm_RefreezeTemperature - refreeze_helper), sc->vm_RefreezeP2);
    }
    if (refreeze > sc->vm_LiquidWaterInSnow) {
      refreeze = sc->vm_LiquidWaterInSnow;
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
double monica::snowComponentCalcNewSnowDensity(const SnowComponent* sc,
                                               double mean_air_temperature,
                                               double net_precipitation_snow) {
  double new_snow_density = 0.0;
  double snow_density_factor = 0.0;

  if (net_precipitation_snow <= 0.0) {
    // no snow
    new_snow_density = 0.0;
  } else {
    //
    snow_density_factor = (mean_air_temperature - sc->vm_TemperatureLimitForLiquidWater)
      / (sc->vm_SnowAccumulationThresholdTemperature - sc->vm_TemperatureLimitForLiquidWater);
    if (snow_density_factor > 1.0) {
      snow_density_factor = 1.0;
    }
    if (snow_density_factor < 0.0) {
      snow_density_factor = 0.0;
    }
    new_snow_density = sc->vm_NewSnowDensityMin + sc->vm_SnowMaxAdditionalDensity * snow_density_factor;
  }
  return new_snow_density;
}

/**
 *
 * @param vm_NetPrecipitationSnow
 * @return
 */
double monica::snowComponentCalcAverageSnowDensity(const SnowComponent* sc,
                                                   double net_precipitation_snow,
                                                   double new_snow_density) {
  double snow_density = 0.0;
  if ((sc->vm_SnowDepth + net_precipitation_snow) <= 0.0) {
    // no snow
    snow_density = 0.0;
  } else {
    snow_density = (((1.0 + sc->vm_SnowPacking) * sc->vm_SnowDensity * sc->vm_SnowDepth) +
      (new_snow_density * net_precipitation_snow)) / (sc->vm_SnowDepth + net_precipitation_snow);
    if (snow_density > (sc->vm_NewSnowDensityMin + sc->vm_SnowMaxAdditionalDensity)) {
      snow_density = sc->vm_NewSnowDensityMin + sc->vm_SnowMaxAdditionalDensity;
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
double monica::snowComponentCalcLiquidWaterRetainedInSnow(const SnowComponent* sc,
                                                          double frozen_water_in_snow,
                                                          double snow_water_equivalent) {
  double snow_retention_capacity;
  double liquid_water_retained_in_snow;

  if ((frozen_water_in_snow <= 0.0) || (sc->vm_SnowDensity <= 0.0)) {
    snow_retention_capacity = 0.0;
  } else {
    snow_retention_capacity = sc->vm_SnowRetentionCapacityMax / 10.0 / sc->vm_SnowDensity;

    if (snow_retention_capacity < sc->vm_SnowRetentionCapacityMin)
      snow_retention_capacity = sc->vm_SnowRetentionCapacityMin;
    if (snow_retention_capacity > sc->vm_SnowRetentionCapacityMax)
      snow_retention_capacity = sc->vm_SnowRetentionCapacityMax;
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
double monica::snowComponentCalcPotentialInfiltration(SnowComponent* sc,
                                                      double net_precipitation,
                                                      double snow_layer_water_release,
                                                      double snow_depth) {
  double water_to_infiltrate = net_precipitation;
  if (snow_depth >= 0.01) {
    sc->vm_WaterToInfiltrate = snow_layer_water_release;
  }
  return water_to_infiltrate;
}

/**
 * Calculates snow depth. If there is no snow, vm_SnowDensity
 * vm_FrozenWaterInSnow and vm_LiquidWaterInSnow are set to zero.
 *
 * @param snow_water_equivalent
 */
void monica::snowComponentCalcSnowDepth(SnowComponent* sc, double snow_water_equivalent) {
  double pm_WaterDensity = 1.0; // [kg dm-3]
  if (snow_water_equivalent <= 0.0) {
    sc->vm_SnowDepth = 0.0;
  } else {
    sc->vm_SnowDepth = snow_water_equivalent * pm_WaterDensity / sc->vm_SnowDensity; // [mm * kg dm-3 kg-1 dm3]

    // check if new snow depth is higher than maximal snow depth
    if (sc->vm_SnowDepth > sc->vm_maxSnowDepth) {
      sc->vm_maxSnowDepth = sc->vm_SnowDepth;
    }

    if (sc->vm_SnowDepth < 0.01) {
      sc->vm_SnowDepth = 0.0;
    }
  }
  if (sc->vm_SnowDepth == 0.0) {
    sc->vm_SnowDensity = 0.0;
    sc->vm_FrozenWaterInSnow = 0.0;
    sc->vm_LiquidWaterInSnow = 0.0;
  }

  if (sc->soilColumn) sc->soilColumn->vm_SnowDepth = sc->vm_SnowDepth;
  sc->vm_AccumulatedSnowDepth += sc->vm_SnowDepth;
}
