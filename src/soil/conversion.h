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

This file is part of the util library used by models created at the Institute of 
Landscape Systems Analysis at the ZALF.
Copyright (C) Leibniz Centre for Agricultural Landscape Research (ZALF)
*/
#pragma once

#include <string>

#include "tools/helper.h"

namespace Soil {

Tools::EResult<double> humusClass2corg(int humusClass);
//Tools::EResult<double> humus_st2corg(int humus_st)
//{
//	return humusClass2corg(humus_st);
//}

//! return soil raw density [kg m-3]
Tools::EResult<double> bulkDensityClass2rawDensity(int bulkDensityClass, double clay);

//inline Tools::EResult<double> ld_eff2trd(int ldEff, double clay)
//{ 
//	return bulkDensityClass2rawDensity(ldEff, clay);
//}


double sandAndClay2lambda(double sand, double clay);

//! sand and clay [0 - 1]
std::string sandAndClay2KA5texture(double sand, double clay);

Tools::EResult<double> KA5texture2sand(std::string soilType);

Tools::EResult<double> KA5texture2clay(std::string soilType);

} //namespace Soil
