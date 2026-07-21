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
  
struct SnowComponent {
  SoilColumn* soilColumn{ nullptr };

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

namespace snowcomponent {
void initialize(SnowComponent* sc, SoilColumn* soilColumn, const SoilMoistureModuleParameters& smps);
void deserialize(SnowComponent* sc, mas::schema::model::monica::SnowModuleState::Reader reader);
void serialize(const SnowComponent* sc, mas::schema::model::monica::SnowModuleState::Builder builder);
void calcSnowLayer(SnowComponent* sc, double meanAirTemperature, double netPrecipitation);
double calcSnowMelt(const SnowComponent* sc, double meanAirTemperature);
double calcNetPrecipitation(
  const SnowComponent* sc,
  double meanAirTemperature,
  double netPrecipitation,
  double& netPrecipitationWater,
  double& netPrecipitationSnow);
double calcRefreeze(const SnowComponent* sc, double meanAirTemperature);
double calcNewSnowDensity(const SnowComponent* sc, double meanAirTemperature, double netPrecipitationSnow);
double calcAverageSnowDensity(const SnowComponent* sc, double netPrecipitationSnow, double newSnowDensity);
double calcLiquidWaterRetainedInSnow(const SnowComponent* sc, double frozenWaterInSnow, double snowWaterEquivalent);
double calcPotentialInfiltration(SnowComponent* sc, double netPrecipitation, double snowLayerWaterRelease, double snowDepth);
void calcSnowDepth(SnowComponent* sc, double snowWaterEquivalent);
} // namespace snowcomponent

} // namespace monica
