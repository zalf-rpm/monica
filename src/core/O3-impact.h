/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
Authors:
Tommaso Stella <tommaso.stella@zalf.de>
Michael Berg <michael.berg@zalf.de>

Maintainers:
Currently maintained by the authors.

This file is part of the MONICA model.
Copyright (C) Leibniz Centre for Agricultural Landscape Research (ZALF)
*/


#ifndef  O3_IMPACT_H_
#define  O3_IMPACT_H_

#include <map>
#include <vector>
#include <cmath>

namespace O3impact
{		
	struct O3_impact_params {
		double gamma1 = { 0.060 }; //ozone short-term damage coefficient, unitless
		double gamma2 = { 0.0045 }; //ozone short-term damage coefficient, nmol m-2 s-1
		double gamma3 = { 0.5 }; //ozone long-term damage coefficient, (µmol m-2)-1
		double upper_thr_stomatal = { 0.4 };
		double lower_thr_stomatal = { 1 };
		double Fshape_stomatal = { 2.5 };
	};

	struct O3_impact_in {
		double FC; //field capacity, m3 m-3
		double WP; //wilting point, m3 m-3
		double SWC; //soil water content, m3 m-3
		double ET0; //reference ET, mm d-1
		double O3a; //ambient O3 partial pressure, nbar or nmol mol-1
		double gs; //stomatal conductance mol m-2 s-1 bar-1 (unit ground area)
		int h; //hour of the day (0-23)
		double reldev; //relative development
		double GDD_flo; //GDD from emergence to flowering
		double GDD_mat; //GDD from emergence to maturity
		double fO3s_d_prev; //short term ozone induced reduction of Ac of the previous time step
		double sum_O3_up; //cumulated O3 uptake, µmol m-2 (unit ground area)
	};

	struct O3_impact_out {
		double hourly_O3_up{ 0.0 }; //hourly O3 uptake, µmol m-2 h-1 (unit ground area)
		double fO3s_d{ 1.0 }; //short term ozone induced reduction of Ac
		double fO3l{ 1.0 }; //long term ozone induced senescence
		double fLS{ 1.0 }; //leaf senescence reduction of Ac, modified by O3 cumulative uptake
		double WS_st_clos{ 1.0 };//water deficit factor for stomatal closure
	};	
	
	O3_impact_out O3_impact_hourly(O3_impact_in in, O3_impact_params par, bool WaterDeficitResponseStomata);

	//#define TEST_O3_HOURLY_OUTPUT
#ifdef TEST_O3_HOURLY_OUTPUT
	std::ostream& tout(bool closeFile = false);
#endif
}

#endif
