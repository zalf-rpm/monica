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

//This code is based on kernels/mobile/modules/physiology/vocjjv/vocjjv.h
//original header
/*!
 * @brief
 *    This gas exchange module calculates only
 *    emission of biogenic volatile organic compounds.
 *
 *    implemented by: Felix Wiß (fw), IMK-IFU Garmisch-Partenkirchen, felix.wiss@kit.edu
 *                    edited from the original mobile2d vocjjv model implemented by
 *                    Ruediger Grote (rg), IMK-IFU Garmisch-Partenkirchen, ruediger.grote@kit.edu
 *
 * @date
 *    20.02.2015
 *
 * @author
 *    ruediger grote (rg)
 *    felix wiß (fw)
 */

#ifndef  VOC_JJV_H_
#define  VOC_JJV_H_

#include <vector>

#include "voc-common.h"

namespace Voc
{
	Emissions calculateJJVVOCEmissionsMultipleSpecies(std::vector<std::pair<SpeciesData, CPData>> speciesData,
																										const MicroClimateData& mcd,
																										double dayFraction = 1.0,
																										bool calculateParTempTerm = false);

	inline Emissions calculateJJVVOCEmissions(SpeciesData sd,
																						const MicroClimateData& mcd,
																						CPData cpdata,
																						double dayFraction = 1.0,
																						bool calculateParTempTerm = false)
	{
		return calculateJJVVOCEmissionsMultipleSpecies({std::make_pair(sd, cpdata)}, mcd, dayFraction, calculateParTempTerm);
	}
}

#endif  
