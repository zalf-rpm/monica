/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
Authors:
Michael Berg <michael.berg@zalf.de>

Maintainers:
Currently maintained by the authors.

This file is part of the MONICA model.
Copyright (C) Leibniz Centre for Agricultural Landscape Research (ZALF)
*/

#pragma once

#include "core/monica-parameters.h"

namespace stics
{		
double vnit(const Monica::SticsParameters& ps,
            double NH4,
            double pH,
            double soilT,
            double wfps,
            double soilwaterContent,
            double fc,
            double sat);

double vdenit(const Monica::SticsParameters& ps,
              double corg,
              double NO3,
              double soilT,
              double wfps,
              double soilwaterContent);

double N20(const Monica::SticsParameters& ps,
           double corg,
           double NO3,
           double soilT,
           double wfps,
           double soilwaterContent,
           double NH4,
           double pH,
           double fc,
           double sat);
} // namespace stics
