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

//This code is based on kernels/mobile/modules/physiology/vocjjv/vocguenther.h
/*!
 * @brief
 *    This gas exchange module calculates only
 *    emission of biogenetic volatile organic compounds.
 *
 *    implemented by: Ruediger Grote (RG), IMK-IFU Garmisch-Partenkirchen,
 *                    ruediger.grote@imk.fzk.de
 *
 * @author
 *    ruediger grote
 */

#ifndef  LDNDC_MOD_PHYSIOLOGY_VOCGUENTHER_H_
#define  LDNDC_MOD_PHYSIOLOGY_VOCGUENTHER_H_

#include <map>
#include <vector>
#include <cmath>

#include "voc-common.h"

namespace Voc
{
	Emissions calculateGuentherVOCEmissions(std::vector<SpeciesData> sds,
																					const MicroClimateData& mc,
																					double day_fraction);
	
	LeafEmissions calcLeafEmission(const leaf_emission_t& lemi,
																 double _species_EF_MONOS);
}

#endif

