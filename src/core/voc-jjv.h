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
	Emissions calculateJJVVOCEmissionsMultipleSpecies(std::vector<SpeciesData> sds,
																										const MicroClimateData& mcd,
																										double dayFraction = 1.0,
																										bool calculateParTempTerm = false);

	inline Emissions calculateJJVVOCEmissions(SpeciesData sd,
																						const MicroClimateData& mcd,
																						double dayFraction = 1.0,
																						bool calculateParTempTerm = false)
	{
		return calculateJJVVOCEmissionsMultipleSpecies({sd}, mcd, dayFraction, calculateParTempTerm);
	}

	//this->get_option< bool >( "CalcParTempDependence", false);
	LeafEmissions calcLeafEmission(const leaf_emission_t& lemi,
																 const leaf_emission_t& leminorm,
																 const SpeciesData& sd,
																 const MicroClimateData& mcd,
																 bool calculateParTempTerm = false);

	double gamma_PH(const leaf_emission_t& lemi,
									SpeciesData sd,
									const MicroClimateData& mcd);

	struct GammaEnRes
	{
		double en_iso{0.0}; //!< activity factor related to enzyme activity (isoprene synthase)
		double en_mono{0.0}; //!< activity factor related to enzyme activity (monoterpene synthase)

		double ennorm_iso{0.0}; //!< normalized activity factor related to enzyme activity (isoprene synthase)
		double ennorm_mono{0.0}; //!< normalized activity factor related to enzyme activity (monoterpene synthase)
	};
	GammaEnRes gamma_EN(const leaf_emission_t& lemi, 
											const leaf_emission_t& leminorm,
											const SpeciesData& sd);
}

#endif  
