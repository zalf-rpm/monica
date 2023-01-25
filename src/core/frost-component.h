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

class FrostComponent {
  public:
    FrostComponent(SoilColumn& sc,
                    double pm_HydraulicConductivityRedux,
                    double p_timeStep);

    FrostComponent(SoilColumn& sc, mas::schema::model::monica::FrostModuleState::Reader reader) : soilColumn(sc) { deserialize(reader); }
    void deserialize(mas::schema::model::monica::FrostModuleState::Reader reader);
    void serialize(mas::schema::model::monica::FrostModuleState::Builder builder) const;

    void calcSoilFrost(double mean_air_temperature, double snow_depth);
    double getFrostDepth() const { return vm_FrostDepth; }
    double getThawDepth() const { return vm_ThawDepth; }
    double getLambdaRedux(size_t layer) const { return vm_LambdaRedux[layer]; }
    double getAccumulatedFrostDepth() const { return vm_accumulatedFrostDepth; }
    double getTemperatureUnderSnow() const { return vm_TemperatureUnderSnow; }
    double calcTemperatureUnderSnow(double mean_air_temperature, double snow_depth) const;

  private:
    double getMeanBulkDensity();
    double getMeanFieldCapacity();
    double calcHeatConductivityFrozen(double mean_bulk_density, double sii);
    double calcHeatConductivityUnfrozen(double mean_bulk_density, double mean_field_capacity);
    double calcSii(double mean_field_capacity);
    double calcThawDepth(double temperature_under_snow, double heat_conductivity_unfrozen, double mean_field_capacity);
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

} // namespace monica


