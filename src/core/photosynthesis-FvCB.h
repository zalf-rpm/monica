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

#pragma once

#include <map>
#include <vector>
#include <cmath>
#include <iostream>

namespace FvCB {
  
enum FvCB_Model_Consts {
  Rd = 0, // Day respiration (respiratory CO2 release other than by photorespiration) [μmol CO2 m−2 s−1]
  Vcmax,  // maximum rate of carboxylation
  Vomax,
  Gamma,
  Kc,     // Michaelis–Menten constant for CO2
  Ko,     // Michaelis–Menten constant for O2
  Jmax    // actual electron transport capacity (unit leaf area) [J�mol m-2 s-1]
};
extern std::map<FvCB_Model_Consts, double> c_bernacchi;// = {{ Rd, 18.72},{ Vcmax, 26.35 },{ Vomax, 22.98 },{ Gamma, 19.02 },{ Kc, 38.05 },{ Ko, 20.30 }, { Jmax, 17.57 }}; //dimensionless
extern std::map<FvCB_Model_Consts, double> deltaH_bernacchi;// = {{ Rd, 46.39 },{ Vcmax, 65.33 },{ Vomax, 60.11 },{ Gamma, 37.83 },{ Kc, 79.43 },{ Ko, 36.38 },{ Jmax, 43.54 }}; //kJ mol - 1
  
struct FvCB_canopy_hourly_params {
  double Vcmax_25;            // maximum rate of carboxylation at 25°C
  double kn = { 0.713 };      //coefficient of leaf nitrogen allocation
  double gb = { 1.5 };        //boundary layer conductance [mol m-2 s-1 bar-1]; TODO: function needed
  double g0 = { 0.01 };       //residual stomatal conductance (with irradiance approaching zero) [mol m-2 s-1 bar-1]
  double gm_25 = { 0.10125 }; //mesophyll conductance (C3) at 25°C [mol m-2 s-1 bar-1]
};

struct FvCB_canopy_hourly_in {
  double global_rad; // global radiation [MJ m-2 h-1]
  double extra_terr_rad; // extra-terrestrial radiation [MJ m-2 h-1]
  double solar_el; // solar elevation angle [rad]
  double LAI; // leaf area index [m2 m-2]
  double leaf_temp; //leaf temperature [°C]
  double VPD; // [KPa]
  double Ca; //ambient CO2 partial pressure, [�bar or �mol mol-1]
  //double fO3 = { 1.0 }; //effect of high ozone fluxes on rubisco-limited photosynthesis
  //double fls = { 1.0 }; //effect of senescence on rubisco-limited photosynthesis, modified by cumulative ozone uptake 
};

struct FvCB_leaf_fraction {
  double LAI; // leaf area index [m2 m-2]
  double gs; // stomatal conductance (unit ground area) [mol m-2 s-1 bar-1] 
  double kc; //Michaelis - Menten constant for CO2 reaction of rubisco per canopy layer [�mol mol-1 mbar-1] 
  double ko; //Michaelis - Menten constant for O2 reaction of rubisco per canopy layer [�mol mol-1 mbar-1] 
  double oi; //leaf internal O2 concentration per canopy layer [�mol m-2] 
  double ci; //leaf intercellular CO2 concentration per canopy layer [�mol m-2] 
  double cc; //leaf chloroplast CO2 concentration per canopy layer [�mol m-2] 
  double comp; //CCO2 compensation point at 25oC per canopy layer [�mol mol-1] 
  double vcMax; //actual activity state of rubisco  (unit leaf area) [�mol m-2 s-1] 
  double jMax; //actual electron transport capacity (unit leaf area) [�mol m-2 s-1]
  double rad; //global radiation (unit ground area) [W m-2]
  double jj; //electron provision (unit leaf area) [umol m-2 s-1]
  double jv; //used electron transport for photosynthesis (unit leaf area) [umol m-2 s-1]
  double jj1000; //electron provision (unit leaf area) at normalized conditions [umol m-2 s-1]
};

struct FvCB_canopy_hourly_out {
  double canopy_net_photos; //[�mol CO2 m-2 h-1]
  double canopy_resp; //[�mol CO2 m-2 h-1]
  double canopy_gross_photos; //[�mol CO2 m-2 h-1]
  double jmax_c; //[umol m-2 s-1] //FS: [μmol m-2 s-1] ?
  FvCB_leaf_fraction sunlit;
  FvCB_leaf_fraction shaded;
};

FvCB_canopy_hourly_out FvCB_canopy_hourly_C3(FvCB_canopy_hourly_in in, FvCB_canopy_hourly_params par);

/**
 * @brief 
 * 
 * @param leafT   leaf temperature [°C]
 * @param Jmax_25 actual electron transport capacity (unit leaf area) at 25°C [�mol m-2 s-1] 
 * @return double 
 */
double Jmax_bernacchi_f(double leafT, double Jmax_25);

/**
 * @brief 
 * 
 * @param leafT     leaf temperature [°C]
 * @param Vcmax_25  actual activity state of rubisco (unit leaf area) at 25°C [�mol m-2 s-1] 
 * @return double 
 */
double Vcmax_bernacchi_f(double leafT, double Vcmax_25);

//#define TEST_FVCB_HOURLY_OUTPUT
#ifdef TEST_FVCB_HOURLY_OUTPUT
std::ostream& tout(bool closeFile = false);
#endif

} // namespace FvCB

