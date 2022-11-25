/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
Authors:
Michael Berg <michael.berg-mohnicke@zalf.de>

Maintainers:
Currently maintained by the authors.

This file is part of the MONICA model.
Copyright (C) Leibniz Centre for Agricultural Landscape Research (ZALF)
*/

#pragma once

#include "core/monica-parameters.h"

namespace stics
{		
// NH4 [mg-NH4-N/kg-soil]
// NO3 [mg-NO3-N/kg-soil]
// wfps = water-filled pore space [] = soil-water-content/saturation
// soilWaterContent = gravimetric soil water content [kg-water/kg-soil]
// soilT = soil temperature [�C]
// fc = fieldcapacity [m3-water/m3-soil]
// sat = saturation [m3-water/m3-soil]

// nitrification [mg-N/kg-soil/day]
double vnit(const monica::SticsParameters& ps,
            double NH4, 
            double pH,
            double soilT,
            double wfps, 
            double soilWaterContent, 
            double fc,
            double sat);

// denitrification [mg-N/kg-soil/day]
double vdenit(const monica::SticsParameters& ps,
              double corg,
              double NO3,
              double soilT,
              double wfps,
              double soilWaterContent);

// N2O emissions [mg-N2O-N/kg-soil/day]
// vnit [mg-n/kg-soil/day]
// vdenit [mg-N/kg-soil/day]
typedef std::pair<double, double> NitDenitN2O;
NitDenitN2O N2O(const monica::SticsParameters& ps,
           double NO3,
           double wfps,
           double pH,
           double vnit,
           double vdenit);

// N2O emissions [mg-N2O-N/kg-soil/day]
NitDenitN2O N2O(const monica::SticsParameters& ps,
           double corg,
           double NO3,
           double soilT,
           double wfps,
           double soilWaterContent,
           double NH4,
           double pH,
           double fc,
           double sat);
} // namespace stics
