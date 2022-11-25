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

#include "frost-component.h"
#include "soilcolumn.h"
#include "tools/debug.h"
#include "tools/algorithms.h"
#include "tools/helper.h"

using namespace std;
using namespace monica;
using namespace Tools;

FrostComponent::FrostComponent(SoilColumn& sc,
  double pm_HydraulicConductivityRedux,
  double p_timeStep)
  : soilColumn(sc),
  vm_LambdaRedux(sc.vs_NumberOfLayers() + 1, 1.0),
  vm_HydraulicConductivityRedux(pm_HydraulicConductivityRedux),
  pt_TimeStep(p_timeStep),
  pm_HydraulicConductivityRedux(pm_HydraulicConductivityRedux) {}

void FrostComponent::deserialize(mas::schema::model::monica::FrostModuleState::Reader reader) {
  vm_FrostDepth = reader.getFrostDepth();
  vm_accumulatedFrostDepth = reader.getAccumulatedFrostDepth();
  vm_NegativeDegreeDays = reader.getNegativeDegreeDays();
  vm_ThawDepth = reader.getThawDepth();
  vm_FrostDays = reader.getFrostDays();
  setFromCapnpList(vm_LambdaRedux, reader.getLambdaRedux());
  vm_TemperatureUnderSnow = reader.getTemperatureUnderSnow();
  vm_HydraulicConductivityRedux = reader.getHydraulicConductivityRedux();
  pt_TimeStep = reader.getPtTimeStep();
  pm_HydraulicConductivityRedux = reader.getPmHydraulicConductivityRedux();
}

void FrostComponent::serialize(mas::schema::model::monica::FrostModuleState::Builder builder) const {
  builder.setFrostDepth(vm_FrostDepth);
  builder.setAccumulatedFrostDepth(vm_accumulatedFrostDepth);
  builder.setNegativeDegreeDays(vm_NegativeDegreeDays);
  builder.setThawDepth(vm_ThawDepth);
  builder.setFrostDays(vm_FrostDays);
  setCapnpList(vm_LambdaRedux, builder.initLambdaRedux((uint)vm_LambdaRedux.size()));
  builder.setTemperatureUnderSnow(vm_TemperatureUnderSnow);
  builder.setHydraulicConductivityRedux(vm_HydraulicConductivityRedux);
  builder.setPtTimeStep(pt_TimeStep);
  builder.setPmHydraulicConductivityRedux(pm_HydraulicConductivityRedux);
}

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
FrostComponent::calcTemperatureUnderSnow(double mean_air_temperature, double snow_depth) const {
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
