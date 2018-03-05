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


#ifndef  PHOTOSYNTHESIS_FVCB_H_
#define  PHOTOSYNTHESIS_FVCB_H_

#include <map>
#include <vector>
#include <cmath>

namespace FvCB
{
	enum FvCB_Model_Consts { Rd = 0, Vcmax, Vomax, Gamma, Kc, Ko, Jmax };
	extern std::map<FvCB_Model_Consts, double> c_bernacchi;// = {{ Rd, 18.72},{ Vcmax, 26.35 },{ Vomax, 22.98 },{ Gamma, 19.02 },{ Kc, 38.05 },{ Ko, 20.30 }, { Jmax, 17.57 }}; //dimensionless
	extern std::map<FvCB_Model_Consts, double> deltaH_bernacchi;// = {{ Rd, 46.39 },{ Vcmax, 65.33 },{ Vomax, 60.11 },{ Gamma, 37.83 },{ Kc, 79.43 },{ Ko, 36.38 },{ Jmax, 43.54 }}; //kJ mol - 1
		
	struct FvCB_canopy_hourly_params {
		double Vcmax_25;
		double kn = { 0.713 }; //coefficient of leaf nitrogen allocation
		double gb = { 1.5 }; //boundary layer conductance, mol m-2 s-1 bar-1; TODO: function needed
		double g0 = { 0.01 }; //residual stomatal conductance (with irradiance approaching zero), mol m-2 s-1 bar-1
		double gm_25 = { 0.10125 }; //mesophyll conductance (C3) at 25°C, mol m-2 s-1 bar-1
	};

	struct FvCB_canopy_hourly_in {
		double global_rad; //MJ m-2 h-1
		double extra_terr_rad; //MJ m - 2 h - 1
		double solar_el; //radians
		double LAI; //m2 m-2
		double leaf_temp; //°C
		double VPD; //KPa
		double Ca; //ambient CO2 partial pressure, µbar or µmol mol-1
		double fO3 = { 1.0 }; //effect of high ozone fluxes on rubisco-limited photosynthesis
		double fls = { 1.0 }; //effect of senescence on rubisco-limited photosynthesis, modified by cumulative ozone uptake 
	};

	struct FvCB_leaf_fraction {
		double LAI; //m2 m-2
		double gs; //mol m-2 s-1 bar-1
		double kc; //µmol mol-1 mbar-1 ... Michaelis - Menten constant for CO2 reaction of rubisco per canopy layer
		double ko; //µmol mol-1 mbar-1 ... Michaelis - Menten constant for O2 reaction of rubisco per canopy layer
		double oi; //µmol m-2 ... leaf internal O2 concentration per canopy layer
		double ci; //µmol m-2 ... leaf internal O2 concentration per canopy layer
		double comp; //µmol mol-1 ... CCO2 compensation point at 25oC per canopy layer
		double vcMax; //µmol m-2 s-1 ... actual activity state of rubisco  (unit leaf area)
		double jMax; //µmol m-2 s-1 ... actual electron transport capacity (unit leaf area)
		double rad; //W m-2 ... global radiation (unit ground area)
		double jj; //umol m-2 s-1 ... electron provision (unit leaf area)
		double jv; //umol m-2 s-1 ... used electron transport for photosynthesis (unit leaf area)
		double jj1000; //umol m-2 s-1 ... electron provision (unit leaf area) at normalized conditions
	};

	struct FvCB_canopy_hourly_out {
		double canopy_net_photos; //µmol CO2 m-2 h-1
		double canopy_resp; //µmol CO2 m-2 h-1
		double canopy_gross_photos; //µmol CO2 m-2 h-1
		double jmax_c; //umol m-2 s-1
		FvCB_leaf_fraction sunlit;
		FvCB_leaf_fraction shaded;
	};

	FvCB_canopy_hourly_out FvCB_canopy_hourly_C3(FvCB_canopy_hourly_in in, FvCB_canopy_hourly_params par);
	double Jmax_bernacchi_f(double leafT, double Jmax_25);
	double Vcmax_bernacchi_f(double leafT, double Vcmax_25);
	
}

#endif
