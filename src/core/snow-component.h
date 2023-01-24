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

namespace monica 
{
class SoilColumn;
struct SoilMoistureModuleParameters;
  
class SnowComponent {
public:
  SnowComponent(SoilColumn& sc, const SoilMoistureModuleParameters& smps);
  ~SnowComponent() {}

  SnowComponent(SoilColumn& sc, mas::schema::model::monica::SnowModuleState::Reader reader) : soilColumn(sc) { deserialize(reader); }
  void deserialize(mas::schema::model::monica::SnowModuleState::Reader reader);
  void serialize(mas::schema::model::monica::SnowModuleState::Builder builder) const;

  void calcSnowLayer(double vw_MeanAirTemperature, double vc_NetPrecipitation);

  double getVm_SnowDepth() const { return this->vm_SnowDepth; }
  double getWaterToInfiltrate() const { return this->vm_WaterToInfiltrate; }
  double getMaxSnowDepth() const { return this->vm_maxSnowDepth; }
  double getAccumulatedSnowDepth() const { return this->vm_AccumulatedSnowDepth; }

private:
  double calcSnowMelt(double vw_MeanAirTemperature);
  double calcNetPrecipitation(double mean_air_temperature, double net_precipitation, double& net_precipitation_water, double& net_precipitation_snow);
  double calcRefreeze(double mean_air_temperature);
  double calcNewSnowDensity(double vw_MeanAirTemperature, double vm_NetPrecipitationSnow);
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
};

} // namespace monica


