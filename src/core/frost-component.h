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

struct FrostComponent {
    SoilColumn* soilColumn{ nullptr };
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

namespace frostcomponent {
void initialize(FrostComponent* fc,
                SoilColumn* soilColumn,
                double pm_HydraulicConductivityRedux,
                double p_timeStep);
void deserialize(FrostComponent* fc, mas::schema::model::monica::FrostModuleState::Reader reader);
void serialize(const FrostComponent* fc, mas::schema::model::monica::FrostModuleState::Builder builder);
void calcSoilFrost(FrostComponent* fc, double mean_air_temperature, double snow_depth);
double getMeanBulkDensity(const FrostComponent* fc);
double getMeanFieldCapacity(const FrostComponent* fc);
double calcHeatConductivityFrozen(const FrostComponent* fc, double mean_bulk_density, double sii);
double calcHeatConductivityUnfrozen(const FrostComponent* fc, double mean_bulk_density, double mean_field_capacity);
double calcSii(double mean_field_capacity);
double calcThawDepth(const FrostComponent* fc,
                     double temperature_under_snow,
                     double heat_conductivity_unfrozen,
                     double mean_field_capacity);
double calcFrostDepth(FrostComponent* fc,
                      double mean_field_capacity,
                      double heat_conductivity_frozen,
                      double temperature_under_snow);
double calcTemperatureUnderSnow(const FrostComponent* fc, double mean_air_temperature, double snow_depth);
void updateLambdaRedux(FrostComponent* fc);
} // namespace frostcomponent

} // namespace monica
